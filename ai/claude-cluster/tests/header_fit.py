#!/usr/bin/env python3
# The header of a session fits its width: a narrow terminal drops the (default) marks, then shortens the mode and the folder, never a
# field cut in the middle ("(defaul", "bypassPermission"); the folder never glued to what is before it.
# Usage: tests/queue_back.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The session header fitted to the width."""
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

T = tempfile.mkdtemp(prefix="cc-header-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_DELAY="12", TERM="xterm-256color")
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
    def __init__(self, cols):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], ENV)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 50, cols, 0, 0))
        self.screen = pyte.Screen(cols, 50)
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

for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
term = None
for cols in (230, 150, 110, 90):
    try:
        term = Term(cols)
        term.pump(4)
        if "asked again at the next start" in term.text():
            term.send(b"n", 1.5)
        if cols == 230:
            ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker", "mode": "bypassPermissions"})
        term.pump(2)
        for _ in range(3):
            if "worker" in term.text().split("\n")[1]:
                break
            term.send(b"\x1bn", 1)
        line = next((l for l in term.text().split("\n") if "worker" in l and ("think" in l or "mode" in l)), "")
        cut = [w for w in ("(defaul ", "(defaul\n", "bypassPermission ", "bypassPermissio") if w in line + "\n" and "bypassPermissions" not in line]
        cut += ["(default" for _ in [0] if line.count("(default") != line.count("(default)")]
        check(f"{cols} columns: no field cut", line != "" and not cut, line)
        check(f"{cols} columns: the folder apart", "shell~" not in line and "s~/" not in line, line)
    finally:
        if term:
            term.kill()
        os.system(f"pkill -f '{T}' > /dev/null 2>&1") if cols == 90 else None
sys.exit(1 if failed else 0)
