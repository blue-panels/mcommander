#!/usr/bin/env python3
"""
Start M-Commander in a pseudo-terminal, wait for the menu bar of the panels
and quit with F10.

MC_BIN is the program to run (default: mcommander from PATH).

Exit status: 0 passed, anything else failed.
"""

import os
import pty
import re
import select
import shutil
import struct
import sys
import tempfile
import termios
import fcntl
import time

START_TIMEOUT = 40
QUIT_TIMEOUT = 20
F10 = b"\x1b[21~"
MENU = re.compile(r"Left\s+File\b.*\bCommand\s+Options\s+Right")
ESCAPES = re.compile(rb"\x1b(\[[0-?]*[ -/]*[@-~]|\][^\x07\x1b]*(\x07|\x1b\\)|[()][0-9A-Za-z]|.)")


def text(buf):
    return ESCAPES.sub(b" ", buf).decode("utf-8", "replace")


def read(fd, buf, deadline, done):
    while time.monotonic() < deadline:
        ready, _, _ = select.select([fd], [], [], 0.5)
        if not ready:
            continue
        try:
            chunk = os.read(fd, 65536)
        except OSError:
            return buf, True
        if not chunk:
            return buf, True
        buf += chunk
        if done(buf):
            return buf, False
    return buf, False


def fail(message, buf):
    print("FAIL: " + message, file=sys.stderr)
    print(text(buf[-4000:]), file=sys.stderr)
    sys.exit(1)


def main():
    program = shutil.which(os.environ.get("MC_BIN", "mcommander"))
    if program is None:
        print("FAIL: no mcommander in PATH", file=sys.stderr)
        sys.exit(1)

    home = tempfile.mkdtemp(prefix="mc-screen-")
    env = dict(os.environ, HOME=home, TERM="xterm-256color", LANG="C.UTF-8", LC_ALL="C.UTF-8")
    for name in ("XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "LANGUAGE"):
        env.pop(name, None)

    pid, fd = pty.fork()
    if pid == 0:
        try:
            os.chdir(home)
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
            os.execve(program, [program], env)
        finally:
            os._exit(127)

    buf, eof = read(fd, b"", time.monotonic() + START_TIMEOUT, lambda b: MENU.search(text(b)))
    if not MENU.search(text(buf)):
        fail("no menu bar after %d s%s" % (START_TIMEOUT, ", the program ended" if eof else ""),
             buf)

    os.write(fd, F10)
    deadline = time.monotonic() + QUIT_TIMEOUT
    while not eof and time.monotonic() < deadline:
        buf, eof = read(fd, buf, deadline, lambda b: False)

    _, status = os.waitpid(pid, os.WNOHANG) if not eof else os.waitpid(pid, 0)
    if not eof:
        os.kill(pid, 9)
        fail("still running %d s after F10" % QUIT_TIMEOUT, buf)
    if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
        fail("ended with status %d" % status, buf)

    shutil.rmtree(home, ignore_errors=True)
    print("ok: %s drew its panels and quit" % program)


if __name__ == "__main__":
    main()
