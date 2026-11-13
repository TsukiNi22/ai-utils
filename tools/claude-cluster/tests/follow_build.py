#!/usr/bin/env python3
# An interface whose core comes back as another build of its own binary (installed again: an update it did not follow in time, or a core
# started again by the new binary) runs that binary too, never stuck in Reconnecting; the Reconnecting popup is chosen with the arrows.
# Usage: tests/queue_back.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""An interface follows a core of another build of its binary."""
import fcntl
import json
import os
import pty
import shutil
import select
import signal
import socket
import struct
import sys
import tempfile
import termios
import time

import pyte

T = tempfile.mkdtemp(prefix="cc-follow-")
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
            os.execve(COPY, [COPY, "--tty"], ENV)
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

COPY = f"{T}/bin/claude-cluster"


def core_pid():
    return (ask({"cmd": "hello"}).get("data") or {}).get("pid", 0)


for folder in ("cfg", "data", "run", "cache", "work", "bin"):
    os.makedirs(f"{T}/{folder}")
shutil.copy(BIN, COPY)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
term = None
try:
    term = Term()
    term.pump(4)
    if "asked again at the next start" in term.text():
        term.send(b"n", 1.5)
    term.pump(2)
    first = core_pid()
    check("the interface and its core", first > 0 and term.alive())
    time.sleep(1.2)  # the binary installed again (another build: its date), then the core gone: the interface starts the new one
    os.remove(COPY)
    shutil.copy(BIN, COPY)
    os.kill(first, signal.SIGKILL)
    term.pump(16)
    exe = os.readlink(f"/proc/{term.pid}/exe") if term.alive() else ""
    check("the interface runs the binary installed (followed)", exe == COPY, exe)
    shown = term.text()
    check("not stuck in Reconnecting", "Reconnecting" not in shown, shown[-500:])
    os.kill(core_pid(), signal.SIGKILL)  # the core gone for good: the popup, chosen with the arrows
    os.remove(COPY)
    term.pump(4)
    shown = term.text()
    check("the Reconnecting popup", "Reconnecting" in shown, shown[-500:])
    term.send(b"\x1b[C", 0.8)  # → Quit
    shown = term.text()
    check("→: Quit chosen", "Quit" in shown, shown[-500:])
    term.send(b"\r", 1)
    ended = False
    for _ in range(30):  # the whole process gone at once (no thread left behind it: it lingered ~20 s once)
        if not term.alive():
            ended = True
            break
        time.sleep(0.1)
    check("Enter: Quit (the interface ended, within 3 s)", ended)
finally:
    if term:
        term.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
