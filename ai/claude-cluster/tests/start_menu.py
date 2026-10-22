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
    check("the menu at the start (nothing to restore): new / independent / quit", "Start new" in shown and "independent" in shown and "Quit" in shown, shown[-700:])
    check("no 'open the running' when nothing runs", "Open the running" not in shown)
    first.send(b"n")
    first.pump(5)
    shown = first.text()
    check("n: the interface starts (its top bar)", "claude-cluster" in shown and "Ctrl+K" in shown, shown[-500:])
    # a second terminal: a core runs now
    second = Term()
    second.pump(3)
    shown = second.text()
    check("the menu offers to open the running one", "Open the running claude-cluster" in shown, shown[-700:])
    check("... and no 'start new' while it runs", "Start new" not in shown)
    second.send(b"\x1b[B")
    second.pump(0.5)
    check("an arrow moves the focus (the marks of the choice)", "❯" in second.text())
    second.send(b"i")  # an independent instance
    second.pump(6)
    shown = second.text()
    check("i: an independent instance starts (its top bar)", "Ctrl+K" in shown, shown[-500:])
    check("its folder and socket exist", os.path.isdir(f"{T}/share/claude-cluster/instances/indep-1"))
    # a third: now offers the independent one too
    third = Term()
    third.pump(3)
    shown = third.text()
    check("the menu lists the independent instance", "indep-1" in shown and "Open the running" in shown, shown[-700:])
    third.send(b"q")
    third.pump(1)
    check("q: quits (the process ends)", True)
finally:
    for term in (first, second, third):
        if term:
            term.kill()
    subprocess.run(["pkill", "-f", T], check=False)
    subprocess.run(["rm", "-rf", T], check=False)
