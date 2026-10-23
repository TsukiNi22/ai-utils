#!/usr/bin/env python3
# Regression test: a restored conversation is drawn at once (the typewriter only types what arrives live) - the global session of the
# previous run, drawn empty while the restore is asked, then restored with a long answer of yesterday: its last words are on the
# screen as soon as its first ones are.
# Usage: tests/reveal_restore.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The known answers of a restored session shown whole, never typed again."""
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

T = tempfile.mkdtemp(prefix="cc-reveal-")
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

for folder in ("cfg", "data", "data/logs", "data/global", "run", "cache"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[ui]\nnotify = false\nmachine_days = 0\nrestore = "ask"\ntypewriter = true\ntypewriter_speed = 20\n'
                 '[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
yesterday = int(time.time()) - 86400
with open(f"{T}/data/state.json", "w", encoding="utf-8") as state:
    json.dump({"version": 1, "next": 1, "sessions": [{"id": "g", "name": "global", "global": True, "cwd": f"{T}/data/global", "backend": "claude",
                                                       "claude_id": "11111111-2222-3333-4444-555555555555", "created": yesterday}]}, state)
answer = "ALPHA-START " + " ".join(f"word{i}" for i in range(60)) + " OMEGA-END"
with open(f"{T}/data/logs/g.jsonl", "w", encoding="utf-8") as log:
    log.write(json.dumps({"kind": 0, "text": "explain it", "time": yesterday}) + "\n")
    log.write(json.dumps({"kind": 1, "text": answer, "time": yesterday + 5}) + "\n")
term = None
try:
    term = Term()
    end = time.time() + 15
    while time.time() < end and "Restore the previous run" not in term.text():
        term.pump(0.2)
    check("the previous run is asked (the global session waits for it)", "Restore the previous run" in term.text(), term.text()[-900:])
    term.pump(1.5)  # its placeholder drawn meanwhile
    term.send(b"r", 0)
    first = ""
    end = time.time() + 15
    while time.time() < end and "ALPHA-START" not in first:
        term.pump(0.03)
        first = term.text()
    check("restored: its answer of yesterday is shown", "ALPHA-START" in first, first[-900:])
    check("drawn at once: its last words with its first ones (not typed again)", "OMEGA-END" in first, first[-900:])
    term.pump(2)
    check("still whole after a while", "OMEGA-END" in term.text())
    check("the interface runs", term.alive())
finally:
    if term:
        term.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
