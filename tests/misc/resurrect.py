#!/usr/bin/env python3
"""
An M-Commander that loses its terminal waits for a new one, and a new
M-Commander started in another terminal takes it over (src/resurrect.c).

The real program is run in pseudo-terminals.  The first one is closed on it
(what a dropped SSH connection or a closed window looks like), the second one
offers the lost M-Commander and hands its terminal over.

MC_BIN is the program to run, and MC_DATADIR where its skins are, if it has
not been installed.

Exit status: 0 passed, 77 skipped, anything else failed.
"""

import errno
import fcntl
import glob
import os
import pty
import re
import select
import shutil
import signal
import struct
import sys
import tempfile
import termios
import time

SKIP = 77
START_TIMEOUT = 40
STEP_TIMEOUT = 30

terminals = []


class Terminal:
    """A program running in a pseudo-terminal, which is this object's master."""

    def __init__(self, argv, env, cwd, rows=24, cols=80):
        self.argv = argv
        self.buf = b""
        self.eof = False
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
        """Read what is there, for at most timeout seconds; False at the end of the output."""
        if self.fd is None or self.eof:
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
            self.eof = True
            return False
        self.buf += data
        return True

    def expect(self, needle, timeout=STEP_TIMEOUT, start=0):
        end = time.time() + timeout
        while needle not in self.buf[start:]:
            left = end - time.time()
            if left <= 0 or not self.pump(min(left, 0.5)):
                if needle in self.buf[start:]:
                    break
                if left <= 0 or self.eof:
                    return False
        return True

    def send(self, data):
        os.write(self.fd, data)

    def hang_up(self):
        os.close(self.fd)
        self.fd = None

    def wait(self, timeout):
        """The exit status of the program, or None if it is still there."""
        end = time.time() + timeout
        while self.status is None:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                self.status = status
                break
            if time.time() >= end:
                return None
            if self.fd is not None:
                self.pump(0.1)
            else:
                time.sleep(0.1)
        return self.status

    def alive(self):
        return self.wait(0) is None

    def kill(self):
        if self.status is None:
            try:
                os.kill(self.pid, signal.SIGKILL)
                os.waitpid(self.pid, 0)
            except OSError:
                pass


def fail(message, term=None):
    print("FAIL: " + message)
    if term is not None:
        text = re.sub(rb"\x1b(\[[0-9;?]*[ -/]*[@-~]|[()][A-Z0-9]|[=>])", b"", term.buf)
        print("--- the last of the output of mc %s, without the escape sequences ---"
              % " ".join(term.argv[1:]))
        print(text[-2500:].decode("utf-8", "replace"))
    sys.stdout.flush()
    return 1


def exited_with(term, code, timeout):
    status = term.wait(timeout)
    return status is not None and os.WIFEXITED(status) and os.WEXITSTATUS(status) == code


def waiting_files(run):
    return sorted(glob.glob(os.path.join(run, "mc-resurrect", "srv-*.info")))


def wait_for_files(run, present, timeout):
    end = time.time() + timeout
    while time.time() < end:
        if bool(waiting_files(run)) == present:
            return True
        time.sleep(0.1)
    return False


def quit_keys(term):
    """F10, as xterm sends it; the program asks nothing and exits."""
    term.send(b"\x1b[21~")


def plain_quit(mc, env, work, run):
    """The keys used to end the others work on an M-Commander that has lost nothing."""
    d = Terminal([mc, "--nomouse"], env, work)
    if not d.expect(b"marker_one.txt", START_TIMEOUT):
        return fail("the M-Commander did not show the panel", d)
    quit_keys(d)
    if not exited_with(d, 0, STEP_TIMEOUT):
        return fail("F10 did not end the M-Commander (status %r)" % d.status, d)
    return 0


def immortal(mc, env, work, run):
    a = Terminal([mc, "--nomouse"], env, work)
    if not a.expect(b"marker_one.txt", START_TIMEOUT):
        return fail("the first M-Commander did not show the panel", a)

    # the terminal is gone
    a.hang_up()
    if not wait_for_files(run, True, STEP_TIMEOUT):
        return fail("the M-Commander that lost its terminal did not wait for a new one")
    if not a.alive():
        return fail("the M-Commander exited with its terminal (status %r)" % a.status)

    b = Terminal([mc, "--nomouse"], env, work)
    if not b.expect(b"lost their terminals", START_TIMEOUT):
        return fail("the new M-Commander did not offer the lost one", b)
    mark = len(b.buf)
    b.send(b"1\r")
    if not b.expect(b"marker_one.txt", STEP_TIMEOUT, mark):
        return fail("the lost M-Commander did not draw its panel on the new terminal", b)
    if waiting_files(run):
        return fail("the files of the waiting M-Commander were left after it was taken over")

    # the size of the window is taken over as well: the shim passes SIGWINCH on
    mark = len(b.buf)
    fcntl.ioctl(b.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
    end = time.time() + 10
    while len(b.buf) == mark and time.time() < end:
        b.pump(0.2)
    if len(b.buf) == mark:
        return fail("the new size of the window was not passed to the M-Commander", b)

    # quit in the new terminal: both processes finish, and with the exit code of the old one
    quit_keys(b)
    if not exited_with(b, 0, STEP_TIMEOUT):
        return fail("the new M-Commander did not exit with the code of the old one (status %r)"
                    % b.status, b)
    if not exited_with(a, 0, STEP_TIMEOUT):
        return fail("the old M-Commander did not finish (status %r)" % a.status)
    return 0


def mortal(mc, env, work, run):
    c = Terminal([mc, "--mortal", "--nomouse"], env, work)
    if not c.expect(b"marker_one.txt", START_TIMEOUT):
        return fail("the M-Commander did not show the panel", c)
    c.hang_up()
    if c.wait(STEP_TIMEOUT) is None:
        return fail("an M-Commander started with --mortal did not exit with its terminal")
    if waiting_files(run):
        return fail("an M-Commander started with --mortal left files to be taken over")
    return 0


def main():
    mc = os.environ.get("MC_BIN")
    if not sys.platform.startswith(("linux", "android")) or not mc or not os.access(mc, os.X_OK):
        print("SKIP: needs Linux or Android and the built mcommander (MC_BIN)")
        return SKIP

    tmp = tempfile.mkdtemp(prefix="mcrv")
    try:
        home, run, work, tmpdir = (os.path.join(tmp, d) for d in ("home", "run", "work", "tmp"))
        for d in (home, run, work, tmpdir):
            os.mkdir(d, 0o700)
        with open(os.path.join(work, "marker_one.txt"), "w") as f:
            f.write("one\n")

        env = {
            "HOME": home,
            "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
            "TERM": "xterm",
            "LC_ALL": "C.UTF-8",
            "XDG_RUNTIME_DIR": run,
            "TMPDIR": tmpdir,
            "MC_NO_LUA": "1",
            "MC_SIXEL": "0",
            "MC_KITTY_KEYBOARD": "0",
            "MC_WIN32_INPUT": "0",
            "MC_FAR2L": "0",
        }
        if "MC_DATADIR" in os.environ:
            env["MC_DATADIR"] = os.environ["MC_DATADIR"]

        for scenario in (plain_quit, immortal, mortal):
            result = scenario(mc, env, work, run)
            if result != 0:
                return result
            print("ok: " + scenario.__name__)
        return 0
    finally:
        for t in terminals:
            t.kill()
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
