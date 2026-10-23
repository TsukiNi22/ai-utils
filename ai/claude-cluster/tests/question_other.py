#!/usr/bin/env python3
# A multiple choice answered by its Other alone: Enter validates it (never unticks it), and back on it its text is still there, ticked;
# its text emptied: Other no longer ticked.
# Usage: tests/question_other.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""Other of a multiple choice: kept when coming back, unticked when emptied."""
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
    term.send(b"4", 0.8)  # its Other
    for c in b"MYOTHER":
        term.send(bytes([c]), 0.05)
    term.pump(0.8)
    check("Other typed: ticked", "[x] 4. Other" in term.text() and "MYOTHER" in term.text(), term.text()[-900:])
    term.send(b"\r", 1.2)  # Enter: validated, the next question
    check("Enter validated it (the next question)", "Anything to add" in term.text(), term.text()[-900:])
    term.send(b"\x1b[D", 1.2)  # left: back
    shown = term.text()
    check("back on it: its Other text kept, ticked", "What should the showcase show next" in shown and "[x] 4. Other" in shown and "MYOTHER" in shown, shown[-900:])
    for _ in range(7):
        term.send(b"\x7f", 0.1)
    term.pump(0.8)
    check("its text emptied: Other no longer ticked", "[ ] 4. Other" in term.text(), term.text()[-900:])
    check("the interface runs", term.alive())
finally:
    if term:
        term.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
