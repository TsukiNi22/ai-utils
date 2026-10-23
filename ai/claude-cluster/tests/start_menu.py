#!/usr/bin/env python3
# Regression test: the start menu of the terminal (shown at each start, what it offers follows the state; an independent instance).
# Usage: tests/start_menu.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The start menu of the terminal: shown at each start (without the isolated environment of the tests), what it offers follows the state."""
import fcntl
import os
import pty
import select
import signal
import struct
import subprocess
import termios
import time

import pyte

import sys
import tempfile

T = tempfile.mkdtemp(prefix="cc-menu-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(XDG_CONFIG_HOME=f"{T}/config", XDG_DATA_HOME=f"{T}/share", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/log", TERM="xterm-256color")


def check(name, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


class Term:
    def __init__(self):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], ENV)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 140, 0, 0))
        self.screen = pyte.Screen(140, 40)
        self.stream = pyte.ByteStream(self.screen)

    def pump(self, sec):
        end = time.time() + sec
        while time.time() < end:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    self.stream.feed(os.read(self.fd, 65536))
                except OSError:
                    return

    def text(self):
        return "\n".join(line.rstrip() for line in self.screen.display)

    def send(self, data):
        os.write(self.fd, data)

    def kill(self):
        try:
            os.kill(self.pid, signal.SIGKILL)
        except OSError:
            pass


for folder in ("config/claude-cluster", "share", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/config/claude-cluster/config.toml", "w").write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                                                           f'[commands]\nclaude = "{FAKE}"\n')
first = second = third = None
try:
    first = Term()
    first.pump(3)
    shown = first.text()
    check("the menu at the start (nothing to restore): new / independent / quit", "Start new" in shown and "Independent instances" in shown and "Quit" in shown, shown[-700:])
    check("no 'open the running' when nothing runs", "Connect to the running" not in shown)
    first.send(b"n")
    first.pump(5)
    shown = first.text()
    check("n: the interface starts (its top bar)", "claude-cluster" in shown and "Ctrl+K" in shown, shown[-500:])
    # a second terminal: a core runs now
    second = Term()
    second.pump(3)
    shown = second.text()
    check("the menu offers to open the running one", "Connect to the running claude-cluster" in shown, shown[-700:])
    check("... and no 'start new' while it runs", "Start new" not in shown)
    second.send(b"\x1b[B")
    second.pump(0.5)
    check("an arrow moves the focus (the marks of the choice)", "❯" in second.text())
    second.send(b"i")  # the independent instances
    second.pump(1)
    shown = second.text()
    check("i: the list of the independent instances (a new one at least)", "independent instances" in shown and "New independent instance" in shown, shown[-700:])
    second.send(b"n")
    second.pump(0.5)
    check("n: the name is asked", "Name:" in second.text(), second.text()[-500:])
    for letter in b"a bc":
        second.send(bytes([letter]))
        second.pump(0.1)
    check("a space in the name: told", "space not allowed" in second.text(), second.text()[-500:])
    second.send(b"\x7f")
    second.pump(0.3)
    check("a character deleted, the name still wrong: still told", "space not allowed" in second.text(), second.text()[-500:])
    for _ in range(3):
        second.send(b"\x7f")
        second.pump(0.1)
    check("the name fixed: the error gone", "not allowed" not in second.text(), second.text()[-500:])
    for letter in b"work":
        second.send(bytes([letter]))
        second.pump(0.1)
    second.send(b"\r")
    second.pump(6)
    shown = second.text()
    check("the independent instance 'work' starts (its top bar)", "Ctrl+K" in shown, shown[-500:])
    check("its folder exists", os.path.isdir(f"{T}/share/claude-cluster/instances/work"))
    # a stopped instance with sessions kept, and a stopped empty one (removed by the menu)
    kept = f"{T}/share/claude-cluster/instances/old-one"
    os.makedirs(kept)
    with open(f"{kept}/state.json", "w") as file:
        file.write('{"version": 1, "sessions": [{"id": "s1", "name": "a", "cwd": "/tmp", "backend": "claude"}]}')
    os.makedirs(f"{T}/share/claude-cluster/instances/empty-one")
    third = Term()
    third.pump(3)
    shown = third.text()
    check("the main menu: connect, the instances counted, quit (no list of them)", "Connect to the running" in shown and "Independent instances (2)" in shown and "old-one" not in shown, shown[-700:])
    check("the empty instance was removed", not os.path.isdir(f"{T}/share/claude-cluster/instances/empty-one"))
    third.send(b"i")
    third.pump(1)
    shown = third.text()
    check("the list: running and stopped ones, with their last use", "work" in shown and "old-one" in shown and "last used" in shown and "running" in shown and "stopped" in shown, shown[-900:])
    third.send(b"d")
    third.pump(0.5)
    check("d: asked before the delete", "Delete the independent instance" in third.text(), third.text()[-500:])
    third.send(b"y")
    third.pump(0.8)
    check("y: the stopped instance is deleted", not os.path.isdir(kept))
    third.send(b"\x1b")
    third.pump(0.5)
    third.send(b"q")
    third.pump(1)
    check("q: quits (the process ends)", True)
finally:
    for term in (first, second, third):
        if term:
            term.kill()
    subprocess.run(["pkill", "-f", T], check=False)
    subprocess.run(["rm", "-rf", T], check=False)
