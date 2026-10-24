#!/usr/bin/env python3
# A prompt typed and not sent survives the interface: saved a moment after it is typed (drafts.json), the terminal killed (a crash)
# then started again: the prompt is back in it; sent, the draft is gone (the next start has an empty prompt).
# Usage: tests/prompt_draft.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The unsent prompt kept over a crash of the terminal."""
import fcntl
import json
import os
import pty
import select
import signal
import struct
import sys
import tempfile
import termios
import time

import pyte

T = tempfile.mkdtemp(prefix="cc-draft-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color")
DRAFT = "a long prompt I did not send yet"
failed = False


def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def drafts() -> dict:
    try:
        with open(f"{T}/data/drafts.json", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


class Term:
    def __init__(self) -> None:
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], ENV)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 140, 0, 0))
        self.screen = pyte.Screen(140, 40)
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

    def kill(self) -> None:
        try:
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
        except OSError:
            pass


def start() -> Term:
    term = Term()
    term.pump(5)
    if "asked again at the next start" in term.text():
        term.send(b"n", 1.5)
    return term


for folder in ("cfg", "data", "run", "cache"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
term = None
try:
    term = start()
    term.send(DRAFT.encode(), 3)  # typed, not sent; the draft keeper saves it a moment later
    check("the prompt typed is saved", drafts().get("tty", {}).get("text") == DRAFT, json.dumps(drafts()))
    term.kill()  # a crash: no quit, nothing flushed
    term = start()
    check("started again: the prompt is back", DRAFT in term.text(), term.text()[-700:])
    check("told so", "unsent prompt is back" in term.text(), term.text()[-700:])
    term.send(b"\x15", 3)  # cleared (Ctrl+U; sent works the same): the draft goes
    check("cleared: no draft left", "tty" not in drafts(), json.dumps(drafts()))
    term.kill()
    term = start()
    check("the next start: nothing given back", "unsent prompt is back" not in term.text(), term.text()[-500:])
finally:
    if term:
        term.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
