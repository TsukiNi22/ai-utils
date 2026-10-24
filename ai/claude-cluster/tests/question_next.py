#!/usr/bin/env python3
# A multiple choice has a "Next" row after Other (like Claude Code): ↓ reaches it, Enter on it goes on with the ticks kept (never a tick
# itself); a single choice has none.
# Usage: tests/question_other.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The Next row of a multiple choice."""
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

T = tempfile.mkdtemp(prefix="cc-other-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color")
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict) -> dict:
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

    def pump(self, sec: float) -> None:
        end = time.time() + sec
        while time.time() < end:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    self.stream.feed(os.read(self.fd, 65536))
                except OSError:
                    return

    def text(self) -> str:
        return "\n".join(line.rstrip() for line in self.screen.display)

    def send(self, data: bytes, wait: float = 0.5) -> None:
        os.write(self.fd, data)
        self.pump(wait)

    def alive(self) -> bool:
        return os.waitpid(self.pid, os.WNOHANG) == (0, 0)

    def kill(self) -> None:
        try:
            os.kill(self.pid, signal.SIGKILL)
        except OSError:
            pass

for folder in ("cfg", "data", "run", "cache"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n[sessions]\nquestions_to_global = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
term = None
try:
    term = Term()
    term.pump(5)
    if "asked again at the next start" in term.text():
        term.send(b"n", 1.5)
    ask({"cmd": "showcase", "what": "questions", "platform": "tty"})
    term.pump(3)
    if "Other" not in term.text():
        term.send(b"\x1bn", 1.5)  # Alt+N: the next session (the one asking)
    check("the question popup is shown", "Other" in term.text() or "Type something" in term.text(), term.text()[-800:])
    term.send(b"1", 1.2)  # the single choice answered: the multiple one
    check("on the multiple choice", "What should the showcase show next" in term.text(), term.text()[-900:])
    check("its Next row shown (the review: the last but one question goes on)", "Next →" in term.text(), term.text()[-900:])
    term.send(b"1", 0.8)  # Charts ticked
    for _ in range(4):    # down past Other: the Next row
        term.send(b"\x1b[B", 0.3)
    check("the cursor on Next", "› Next →" in term.text(), term.text()[-900:])
    term.send(b"\r", 1.2)  # Enter on Next: the next question
    check("Enter on Next: the next question", "Anything to add" in term.text(), term.text()[-900:])
    check("a single choice: no Next row", "Next →" not in term.text() and "Review →" not in term.text(), term.text()[-900:])
    term.send(b"\x1b[D", 1.2)  # back: the ticks kept, Next never ticked
    check("back: Charts still ticked", "[x] 1. Charts" in term.text(), term.text()[-900:])
    check("the interface runs", term.alive())
finally:
    if term:
        term.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
