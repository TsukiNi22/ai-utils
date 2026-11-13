#!/usr/bin/env python3
# ↑ in the prompt while prompts wait in the queue of a running turn: the last one back in the prompt (out of the queue), the text
# being written stashed first; with nothing queued, ↑ is the history again.
# Usage: tests/queue_back.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""Up arrow: a queued prompt taken back, the one written stashed."""
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

T = tempfile.mkdtemp(prefix="cc-queue-")
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

for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 '[sessions]\nqueue_given = "end"\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
term = None


def typed(text):
    for c in text.encode():
        term.send(bytes([c]), 0.03)


try:
    term = Term()
    term.pump(4)
    if "asked again at the next start" in term.text():
        term.send(b"n", 1.5)
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"})
    term.pump(2)
    for _ in range(3):  # to the new session (Alt+N) when another one is shown
        if "worker" in term.text().split("\n")[1]:
            break
        term.send(b"\x1bn", 1)
    typed("slow first")
    term.send(b"\r", 2)
    typed("QUEUED-ONE")
    term.send(b"\r", 1.5)
    state = next((s for s in ask({"cmd": "list"}).get("data", []) if s.get("name") == "worker"), {})
    check("the second prompt waits in the queue", state.get("queued") == 1, json.dumps({k: state.get(k) for k in ("queued", "state")}))
    typed("DRAFT-TEXT")
    term.send(b"\x1b[A", 1.5)  # Up
    shown = term.text()
    check("Up: the queued prompt back in the prompt", "QUEUED-ONE" in shown.split("\n")[-3] + shown.split("\n")[-2] or "QUEUED-ONE" in shown[-600:], shown[-600:])
    check("told so (the one written stashed)", "taken back from the queue" in shown and "stashed" in shown, shown[-400:])
    state = next((s for s in ask({"cmd": "list"}).get("data", []) if s.get("name") == "worker"), {})
    check("out of the queue", state.get("queued") == 0, json.dumps({k: state.get(k) for k in ("queued", "state")}))
    with open(f"{T}/data/stash.json", encoding="utf-8") as stash:
        check("the text written is in the stash", "DRAFT-TEXT" in stash.read())
    check("the interface runs", term.alive())
finally:
    if term:
        term.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
