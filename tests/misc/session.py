#!/usr/bin/env python3
"""
tests/misc - the session plugin: mc outlives its terminal and a new one takes it

The built mcommander runs in pseudo-terminals with the session plugin on.  The
first terminal is closed under it; the second one takes the session, the only
detached one, without the list, is resized, checks that a program run by mc has
/dev/tty, the agent and DISPLAY of the new terminal, and detaches.  The third
one takes it again and stops reading its terminal while mc writes a lot: mc
must not wait for it, and after a while the session looks lost and another
terminal takes it from the list without a question.  With every session in
use, a new one starts without the list.  The dialog of the sessions starts,
switches to and ends sessions.  k in the list ends a session the way F10 does,
and does not when a dialog is open in it; when it has ended the last one, a
new one starts without the list.

MC_BIN, SESSION_PLUGIN, MC_SESSION_HELPER and MC_DATADIR come from
tests/misc/Makefile.am.

Exit status: 0 passed, 77 skipped, anything else failed.
"""

import errno
import fcntl
import os
import pty
import select
import shutil
import signal
import socket
import struct
import sys
import tempfile
import termios
import time

SKIP = 77
TIMEOUT = 30

F9 = b"\x1b[20~"
F10 = b"\x1b[21~"
END = b"\x1bOF"  # the last entry of a menu, whichever was chosen before
CTRL_O = b"\x0f"
DIALOG = b"Directory"  # the header of the list; slang turns runs of spaces into moves

terminals = []


class Failed(Exception):
    pass


class Terminal:
    """A program in a pseudo-terminal, of which this object holds the master side."""

    def __init__(self, argv, env, cwd, rows=24, cols=80):
        self.buf = b""
        self.status = None
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            try:
                os.chdir(cwd)
                fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
                os.execve(argv[0], argv, env)
            finally:
                os._exit(127)
        terminals.append(self)

    def pump(self, timeout):
        if self.fd is None:
            return False
        ready, _, _ = select.select([self.fd], [], [], timeout)
        if not ready:
            return True
        try:
            data = os.read(self.fd, 65536)
        except OSError as e:
            if e.errno != errno.EIO:
                raise
            data = b""
        if not data:
            return False
        self.buf += data
        return True

    def expect(self, needle, start, timeout=TIMEOUT):
        end = time.time() + timeout
        while needle not in self.buf[start:]:
            left = end - time.time()
            if left <= 0 or not self.pump(min(left, 0.5)):
                if needle in self.buf[start:]:
                    break
                raise Failed("%r did not come" % needle)
        return self.buf.index(needle, start) + len(needle)

    def mark(self):
        return len(self.buf)

    def send(self, data):
        os.write(self.fd, data)

    def settle(self, seconds):
        end = time.time() + seconds
        while time.time() < end and self.pump(0.1):
            pass

    def resize(self, rows, cols):
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))

    def hang_up(self):
        os.close(self.fd)
        self.fd = None

    def exit_code(self, timeout=TIMEOUT):
        end = time.time() + timeout
        while time.time() < end:
            self.pump(0.2)
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid == self.pid:
                self.status = status
                # os.waitstatus_to_exitcode() is Python 3.9
                if os.WIFSIGNALED(status):
                    return -os.WTERMSIG(status)
                return os.WEXITSTATUS(status)
        raise Failed("the client did not exit")

    def kill(self):
        if self.status is None:
            try:
                os.kill(self.pid, 9)
                os.waitpid(self.pid, 0)
            except (OSError, ChildProcessError):
                pass
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None


def sockets(run):
    d = os.path.join(run, "mc6", "sessions")
    return sorted(f for f in os.listdir(d) if f.endswith(".sock")) if os.path.isdir(d) else []


def sessions_dialog(t):
    """Command, Sessions...: the last entry of the menu"""
    m = t.mark()
    t.send(F9)
    t.settle(0.5)
    t.send(b"c")
    t.settle(0.5)
    t.send(END)
    t.settle(0.5)
    t.send(b"\r")
    t.expect(DIALOG, m)
    t.settle(0.5)


def step(text):
    print("ok - " + text)
    sys.stdout.flush()


def run_test(mc, env, work, run, settings):
    # off, as it is by default: mc as without the plugin
    p = Terminal([mc], env, work)
    p.expect(b"Name", 0)
    p.settle(1)
    if sockets(run):
        raise Failed("a session is started with the plugin off")
    p.send(F10)
    if p.exit_code() != 0:
        raise Failed("mc without a session does not exit with 0")
    step("the plugin off: no session")
    # what Manage Plugins writes when it is turned on
    with open(settings, "w") as f:
        f.write("[EnabledPlugins]\nruntime/session=true\n")

    # A: a new session; then the terminal is gone
    a = Terminal([mc], env, work)
    a.expect(b"Name", 0)
    a.settle(1)
    if len(sockets(run)) != 1:
        raise Failed("no socket of the session: %s" % sockets(run))
    step("a session is started")
    a.hang_up()
    time.sleep(1)
    if len(sockets(run)) != 1:
        raise Failed("the session did not outlive its terminal")
    step("the session outlives its terminal")

    # B: the only session is detached: B takes it without the list, and brings an agent of its own
    b = Terminal([mc], dict(env, SSH_AUTH_SOCK="/agent/of/b", DISPLAY=":42"), work)
    b.expect(b"Name", 0)
    if b"M-Commander sessions" in b.buf:
        raise Failed("the list is shown for the only session, detached")
    b.settle(1)
    if len(sockets(run)) != 1:
        raise Failed("a new session instead of the detached one: %s" % sockets(run))
    step("a new terminal takes the only detached session without the list")

    m = b.mark()
    b.resize(30, 120)
    b.expect(b"\x1b[29;", m)  # the command line moves from row 23 to row 29
    b.settle(1)
    step("the window size goes to mc")

    m = b.mark()
    b.send(b"sh -c 'echo x > /dev/tty && echo T\"\"TY-OK'\r")
    b.settle(2)
    b.send(CTRL_O)
    b.expect(b"TTY-OK", m)
    b.send(CTRL_O)
    b.settle(1)
    step("a program run by mc has /dev/tty")

    # a program that prints what looks like a mark does not let the terminal go
    m = b.mark()
    b.send(b"printf '\\033_mc-session;detach\\033\\\\'; echo S\"\"TILL-HERE\r")
    b.settle(2)
    b.send(CTRL_O)
    b.expect(b"STILL-HERE", m)
    b.send(CTRL_O)
    b.settle(1)
    if b"[detached" in b.buf[m:]:
        raise Failed("a mark printed by a program let the terminal go")
    step("output that looks like a mark is output")

    # a paste larger than the input buffer of a pty reaches mc whole
    pasted = os.path.join(work, "pasted")
    b.send(("cat > %s\r" % pasted).encode())
    b.settle(1)
    b.send((b"x" * 1023 + b"\r") * 64)  # lines: a line of a tty in canonical mode is 4 KB at most
    b.settle(2)
    b.send(b"\x04")
    end = time.time() + TIMEOUT
    while time.time() < end and (not os.path.exists(pasted) or os.path.getsize(pasted) < 65536):
        b.pump(0.2)
    b.settle(1)
    size = os.path.getsize(pasted) if os.path.exists(pasted) else -1
    if size != 65536:
        raise Failed("a paste of 65536 bytes arrived as %d" % size)
    os.unlink(pasted)
    step("a large paste reaches mc whole")

    # the shell of the command line was started with mc: its SSH_AUTH_SOCK is a link
    m = b.mark()
    b.send(b"sh -c 'echo A\"\"GENT=`readlink \"$SSH_AUTH_SOCK\"`'\r")
    b.settle(2)
    b.send(CTRL_O)
    b.expect(b"AGENT=/agent/of/b", m)
    b.send(CTRL_O)
    b.settle(1)
    step("the shell of mc reaches the agent of the new terminal")

    # the handler of x.agent is run by mc itself, with what the new terminal brought
    b.send(b"\x1bOB\r")  # Down to x.agent, Enter
    agent = os.path.join(work, "agent.out")
    end = time.time() + TIMEOUT
    while time.time() < end and not os.path.exists(agent):
        b.pump(0.2)
    b.settle(1)
    with open(agent) as f:
        if f.read().strip() != ":42":
            raise Failed("a program run by mc does not have the DISPLAY of the new terminal")
    step("a program run by mc has the environment of the new terminal")

    m = b.mark()
    sessions_dialog(b)
    b.send(b"\x1bd")  # Detach
    b.expect(b"[detached", m)
    code = b.exit_code()
    if code != 0:
        raise Failed("the detached client exits with %d" % code)
    if len(sockets(run)) != 1:
        raise Failed("the session is gone after Detach")
    step("Detach gives the terminal back and keeps the session")

    # C: takes it again; a terminal that is not read must not stop mc
    c = Terminal([mc], env, work)
    c.expect(b"Name", 0)
    c.settle(1)
    done = os.path.join(work, "done")
    c.send(("sh -c 'yes 0123456789012345678901234567890123456789 | head -c 5000000; : > %s'\r"
            % done).encode())
    end = time.time() + TIMEOUT
    while time.time() < end and not os.path.exists(done):
        time.sleep(0.2)  # the terminal is not read meanwhile
    if not os.path.exists(done):
        raise Failed("mc waited for a client that did not read")
    step("mc does not wait for a client that does not read")

    # D: the only session is in use: no list, a new session at once
    d = Terminal([mc], env, work)
    d.expect(b"Name", 0)
    if b"M-Commander sessions" in d.buf:
        raise Failed("the list is shown with no session free to take")
    d.settle(1)
    if len(sockets(run)) != 2:
        raise Failed("no new session with the only one in use: %s" % sockets(run))
    d.send(F10)
    if d.exit_code() != 0:
        raise Failed("the new session does not end with 0 after F10")
    time.sleep(0.5)
    if len(sockets(run)) != 1:
        raise Failed("F10 left the socket of the new session")
    step("with every session in use, a new one starts without the list; F10 ends it")

    # C still is not read: its client looks lost, and E takes the session without a question
    time.sleep(12)
    e = Terminal([mc], env, work, rows=30)  # in 24 rows the Command menu does not fit
    e.expect(b"looks lost", 0)
    m = e.mark()
    e.send(b"1\r")
    e.expect(b"Name", m)
    e.settle(1)
    m = c.mark()
    c.expect(b"taken by another terminal", m)
    for mode in (b"\x1b[?1049l", b"\x1b[?9001l", b"\x1b[<u"):
        if mode not in c.buf[m:]:
            raise Failed("the client that was kicked out leaves %r on" % mode)
    if c.exit_code() != 0:
        raise Failed("the client that was kicked out does not exit with 0")
    step("a client that does not read looks lost, another terminal takes its session, and the "
         "modes of mc in the old terminal are turned off")

    # New from the dialog of the sessions: the first one stays, detached
    first = sockets(run)
    m = e.mark()
    sessions_dialog(e)
    e.send(b"\x1bn")  # New
    e.expect(b"Name", m)
    e.settle(3)  # a new mc asks the terminal what it can do; keys sent meanwhile get lost
    if len(sockets(run)) != 2:
        raise Failed("New in the dialog did not start a session: %s" % sockets(run))
    step("New in the dialog of the sessions")

    # a dialog is open in the second session: k does not close it unseen
    m = e.mark()
    e.send(b"\x1b[18~")  # F7, Mkdir
    e.expect(b"Create a new Directory", m)
    e.hang_up()
    time.sleep(1)
    f = Terminal([mc], env, work, rows=30)
    f.expect(b"r or q: ", 0)  # two sessions are free: the list
    m = f.mark()
    f.send(b"k 2\r")
    f.expect(b"a dialog open", m)
    m = f.mark()
    f.send(b"2\r")
    f.expect(b"Create a new Directory", m)
    f.send(F10)  # closes the dialog
    f.settle(1)
    step("k does not end a session with a dialog open")

    # the dialog of the sessions: a third one, back to the first, the second ended from there
    two = sockets(run)
    m = f.mark()
    sessions_dialog(f)
    f.send(b"\x1bn")  # New
    f.expect(b"Name", m)
    f.settle(3)
    third = [x for x in sockets(run) if x not in two]
    if len(third) != 1:
        raise Failed("New in the dialog did not start a session: %s" % sockets(run))
    m = f.mark()
    sessions_dialog(f)
    f.send(b"\r")  # Switch: the cursor is on the first other one, the oldest
    f.expect(b"Name", m)
    f.settle(1)
    m = f.mark()
    sessions_dialog(f)
    f.send(b"\x1be")  # End: the first other one is the second now
    f.expect(DIALOG, m + len(DIALOG))
    f.settle(1)
    now = sockets(run)
    if len(now) != 2 or third[0] not in now or not set(first) & set(now):
        raise Failed("Switch and End left %s; the first was %s, the third %s"
                     % (now, first, third))
    m = f.mark()
    f.send(b"\x1bd")  # Detach
    f.expect(b"[detached", m)
    f.exit_code()
    step("the dialog of the sessions: New, Switch, End and Detach")

    # q leaves the list and the sessions
    h = Terminal([mc], env, work)
    h.expect(b"r or q: ", 0)
    h.send(b"q\r")
    if h.exit_code() != 0:
        raise Failed("q in the list does not exit with 0")
    if len(sockets(run)) != 2:
        raise Failed("q in the list did not leave the sessions alone: %s" % sockets(run))
    step("q leaves the list, the sessions stay")

    # nothing is open: k ends a session, as F10 does; after the last one a new one starts
    old = sockets(run)
    g = Terminal([mc], env, work)
    g.expect(b"r or q: ", 0)
    m = g.mark()
    g.send(b"k 1\r")
    m = g.expect(b"is ended", m)
    g.expect(b"r or q: ", m)
    if g.buf[m:].count(b"detached") != 1:
        raise Failed("the list after k does not have just the other session")
    m = g.mark()
    g.send(b"k 1\r")
    m = g.expect(b"is ended", m)
    g.expect(b"Name", m)
    if b"r or q: " in g.buf[m:]:
        raise Failed("the list is shown again after k ended the last session")
    g.settle(3)
    now = sockets(run)
    if len(now) != 1 or now[0] in old:
        raise Failed("k did not end %s and start a new session: %s" % (old, now))
    g.send(F10)
    if g.exit_code() != 0:
        raise Failed("the session started after k does not end with 0 after F10")
    time.sleep(1)
    left = os.listdir(os.path.join(run, "mc6", "sessions"))
    if left:
        raise Failed("files of the ended sessions are left: %s" % left)
    step("k ends a session with only the panels open; after the last one a new one starts")

    # mc is killed: the client turns its modes off, since mc could not
    k = Terminal([mc], env, work)
    k.expect(b"Name", 0)
    k.settle(3)
    victim = None
    for pid in os.listdir("/proc"):
        try:
            environ = open("/proc/%s/environ" % pid, "rb").read().split(b"\0")
            argv0 = open("/proc/%s/cmdline" % pid, "rb").read().split(b"\0")[0]
        except OSError:
            continue
        # the client and the holder have no MC_SESSION, the shell of mc is not mcommander
        if (("HOME=" + env["HOME"]).encode() in environ
                and any(e.startswith(b"MC_SESSION=") for e in environ)
                and b"mcommander" in os.path.basename(argv0)):
            victim = int(pid)
    if victim is None:
        raise Failed("the mc of the session is not found")
    m = k.mark()
    os.kill(victim, signal.SIGSEGV)
    code = k.exit_code()
    if code != 128 + signal.SIGSEGV:
        raise Failed("the client exits with %d after mc was killed" % code)
    if b"\x1b[?9001l" not in k.buf[m:]:
        raise Failed("the client leaves the modes of a killed mc on")
    step("mc killed by a signal: the exit code goes on, the modes are turned off")

    # the client is killed: the terminal is left as the shell wants it, the session stays
    t = Terminal([mc], env, work)
    t.expect(b"Name", 0)
    t.settle(3)
    m = t.mark()
    os.kill(t.pid, signal.SIGTERM)
    t.exit_code()
    if b"\x1b[?9001l" not in t.buf[m:]:
        raise Failed("a client killed by SIGTERM leaves the modes of mc on")
    time.sleep(1)
    if len(sockets(run)) != 1:
        raise Failed("the session did not outlive its killed client: %s" % sockets(run))
    step("a client killed by SIGTERM turns the modes off, the session stays")

    # a holder that is stopped is alive: its socket stays, and a new mc does not wait for it
    holder = None
    for pid in os.listdir("/proc"):
        try:
            environ = open("/proc/%s/environ" % pid, "rb").read().split(b"\0")
            argv = open("/proc/%s/cmdline" % pid, "rb").read().split(b"\0")
        except OSError:
            continue
        if (("HOME=" + env["HOME"]).encode() in environ and b"session_helper" in argv[0]
                and len(argv) > 1 and argv[1] == b"client" and os.getppid() != int(pid)
                and int(pid) not in [x.pid for x in terminals]):
            holder = int(pid)
    if holder is None:
        raise Failed("the holder of the session is not found")
    stopped = sockets(run)
    os.kill(holder, signal.SIGSTOP)
    path = os.path.join(run, "mc6", "sessions", stopped[0])
    backlog = []
    for _ in range(32):  # more than its backlog
        c = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        c.setblocking(False)
        try:
            c.connect(path)
        except (BlockingIOError, ConnectionRefusedError):
            c.close()
            break
        backlog.append(c)
    n = Terminal([mc], env, work)
    n.expect(b"Name", 0)
    n.settle(1)
    if stopped[0] not in sockets(run):
        raise Failed("the socket of a stopped holder is removed")
    n.send(F10)
    n.exit_code()
    step("a stopped holder keeps its socket, and a new mc does not wait for it")

    # killed, it is dead: the next list removes its files
    os.kill(holder, signal.SIGKILL)
    for c in backlog:
        c.close()
    time.sleep(0.5)
    w = Terminal([mc], env, work)
    w.expect(b"Name", 0)
    w.settle(1)
    base = stopped[0][:-len(".sock")]
    left = [f for f in os.listdir(os.path.join(run, "mc6", "sessions")) if f.startswith(base)]
    if left:
        raise Failed("the files of a killed holder are left: %s" % left)
    w.send(F10)
    w.exit_code()
    step("the files of a killed holder are removed")


def main():
    mc = os.environ.get("MC_BIN")
    plugin = os.environ.get("SESSION_PLUGIN")
    helper = os.environ.get("MC_SESSION_HELPER")
    if not sys.platform.startswith(("linux", "android")):
        print("SKIP: needs Linux or Android")
        return SKIP
    for path in (mc, plugin, helper):
        if not path or not os.access(path, os.R_OK):
            print("SKIP: the built mcommander, session plugin and helper are needed")
            return SKIP

    top = tempfile.mkdtemp(prefix="mcs")
    try:
        home = os.path.join(top, "h")
        run = os.path.join(top, "r")
        work = os.path.join(top, "w")
        os.makedirs(os.path.join(home, ".config", "mc6"))
        os.makedirs(os.path.join(home, ".local", "lib", "mc6", "runtime-plugins"))
        os.makedirs(run, mode=0o700)
        os.makedirs(work)
        settings = os.path.join(home, ".config", "mc6", "plugins.ini")
        with open(os.path.join(home, ".config", "mc6", "extensions.ini"), "w") as f:
            f.write("[mc.ext.ini]\nVersion=4.0\n\n[agent]\nShell=.agent\n"
                    "Open=echo \"$DISPLAY\" > %s\n" % os.path.join(work, "agent.out"))
        open(os.path.join(work, "x.agent"), "w").close()
        shutil.copy(plugin, os.path.join(home, ".local", "lib", "mc6", "runtime-plugins"))

        env = {
            "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
            "HOME": home,
            "TERM": "xterm-256color",
            "LANG": "C.UTF-8",
            "LC_ALL": "C.UTF-8",
            "SHELL": "/bin/sh",
            "XDG_RUNTIME_DIR": run,
            "MC_NO_LUA": "1",
            "MC_SESSION_HELPER": helper,
        }
        if "MC_DATADIR" in os.environ:
            env["MC_DATADIR"] = os.environ["MC_DATADIR"]

        try:
            run_test(mc, env, work, run, settings)
        except Failed as e:
            print("FAIL: %s" % e)
            for t in terminals:
                print("--- output: %r" % t.buf[-2000:])
            return 1
        return 0
    finally:
        for t in terminals:
            t.kill()
        # whatever of the session is left
        for pid in os.listdir("/proc"):
            if pid.isdigit():
                try:
                    if ("HOME=" + top).encode() in open("/proc/%s/environ" % pid, "rb").read():
                        os.kill(int(pid), 9)
                except OSError:
                    pass
        shutil.rmtree(top, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
