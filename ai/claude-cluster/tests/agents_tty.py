#!/usr/bin/env python3
# A session whose turn is over but whose shell / agent still runs in the background: purple, ~_~ background (idle only once it ended);
# Alt+B opens its agents and shells, the time of a running one goes on live, a running shell shows its last lines as they come (its output
# file found where Claude Code writes it, even when claude does not tell it).
# Usage: tests/queue_back.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""Agents and shells: the background state, the popup live."""
import fcntl
import json
import os
import pty
import select
import signal
import socket
import struct
import sys
import tempfile
import termios
import time

import pyte

T = tempfile.mkdtemp(prefix="cc-agtty-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_agents.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_SHELL_SECONDS="20", TMPDIR=f"{T}/tmp", TERM="xterm-256color")
failed = False

def check(name, ok, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(10)
    s.connect(f"{T}/sock")
    s.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        data += chunk
    s.close()
    return json.loads(data)

class Term:
    def __init__(self):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], ENV)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 160, 0, 0))
        self.screen = pyte.Screen(160, 50)
        self.stream = pyte.ByteStream(self.screen)

    def pump(self, sec) -> None:
        end = time.time() + sec
        while time.time() < end:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    self.stream.feed(os.read(self.fd, 65536))
                except OSError:
                    return

    def text(self) -> str:
        return "\n".join(line.rstrip() for line in self.screen.display)

    def send(self, data, wait: float = 0.5) -> None:
        os.write(self.fd, data)
        self.pump(wait)

    def alive(self):
        return os.waitpid(self.pid, os.WNOHANG) == (0, 0)

    def kill(self) -> None:
        try:
            os.kill(self.pid, signal.SIGKILL)
        except OSError:
            pass

import re

TASKS = f"{T}/tmp/claude-{os.getuid()}/-work/00000000-0000-0000-0000-000000000002/tasks"
for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
os.makedirs(TASKS)
ENV["FAKE_CLAUDE_OUT"] = TASKS
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
term = None


def typed(text):
    for c in text.encode():
        term.send(bytes([c]), 0.03)


def shell_time(text):
    found = re.search(r"\$ shell in the background · ⏱ (\d+) s", text)
    return int(found.group(1)) if found else -1


try:
    term = Term()
    term.pump(4)
    if "asked again at the next start" in term.text():
        term.send(b"n", 1.5)
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"})
    term.pump(2)
    for _ in range(3):
        if "worker" in term.text().split("\n")[1]:
            break
        term.send(b"\x1bn", 1)
    typed("run the checks")
    term.send(b"\r", 4)
    shown = term.text()
    check("its turn over, its background running: ~_~ background", "~_~ background" in shown, shown[:600])
    term.send(b"\x1bb", 1.5)  # Alt+B
    first = term.text()
    check("Alt+B: its agents and shells", "Agents of worker" in first, first[-800:])
    before = shell_time(first)
    term.pump(3)
    later = term.text()
    after = shell_time(later)
    check("the time of the running shell goes on live", before >= 0 and after > before, f"{before} -> {after}")
    check("its last lines shown as they come", "SHELL-LINE-" in later, later[-900:])
    check("the interface runs", term.alive())
finally:
    if term:
        term.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
