#!/usr/bin/env python3
# Regression test: a paste into the question popup of the terminal (binary bytes of an image pasted as text, accents, a long text) - the
# answer typed gets valid UTF-8 at once, nothing freezes or crashes (the interface and the core still answer after it).
# Usage: tests/paste_question.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""A paste in the Other answer of a question: one valid piece, no freeze, no crash."""
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

T = tempfile.mkdtemp(prefix="cc-paste-")
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
    if "Other" not in term.text():
        open("/home/tsukini/.claude/jobs/6eed2a92/tmp/paste-screen.txt", "w").write(term.text())
    check("the question popup is shown", "Other" in term.text() or "Type something" in term.text(), term.text()[-800:])
    term.send(b"\x1b[200~" + "été ✓ ok".encode() + b"\x1b[201~", 1.5)
    check("a short paste: in the answer, the popup kept", "été ✓ ok" in term.text(), term.text()[-900:])
    term.send(b"\x1b[200~" + b"x" * 3000 + b"\x1b[201~", 2)
    check("a long paste (3000 characters): the popup kept", "Other" in term.text() or "xxxx" in term.text(), term.text()[-900:])
    binary = (bytes(range(0x20, 0x7f)) + bytes(range(0x80, 0x100))) * 40   # the bytes of an image pasted as text (a terminal never pastes C0 controls)
    started = time.time()
    term.send(b"\x1b[200~" + binary + "été ✓ ".encode() + b"\x1b[201~", 3)
    took = time.time() - started
    check("the terminal interface still runs after the paste", term.alive())
    other = (ask({"cmd": "ask"}).get("data") or {}).get("other", "")
    check("the paste was taken at once (no freeze)", took < 6, f"{took:.1f} s")
    check("the question popup is still shown", "Other" in term.text() or "Type something" in term.text())
    check("the answer in the core: valid text, no control character, 8000 bytes at most",
          0 < len(other.encode("utf-8")) <= 8010 and not any(ord(c) < 0x20 or 0x80 <= ord(c) < 0xa0 for c in other), f"{len(other)} chars")
    state = ask({"cmd": "list"})
    check("the core still answers", state.get("ok") is not False, str(state)[:200])
    term.send(b"\x1b", 1)  # Esc on the popup: no crash either
    check("Esc after it: still running", term.alive())
finally:
    if term:
        term.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
