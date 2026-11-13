#!/usr/bin/env python3
# Settings > Plugins in the terminal: one row per source; Enter on it opens its page in the Plugins browser, Esc goes back to the
# settings (the Plugins page), not to the sessions; the browser opened from the palette still closes to the sessions.
# Usage: tests/plugins_settings_back.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The Plugins browser opened from the settings goes back to them."""
import fcntl
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

T = tempfile.mkdtemp(prefix="cc-plugback-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", TERM="xterm-256color")
failed = False


def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


class Term:
    def __init__(self) -> None:
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

    def send(self, data: bytes, wait: float = 0.4) -> None:
        os.write(self.fd, data)
        self.pump(wait)

    def kill(self) -> None:
        try:
            os.kill(self.pid, signal.SIGKILL)
        except OSError:
            pass


for folder in ("cfg", "data", "run", "cache", "home", "skills/hello"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/skills/hello/SKILL.md", "w", encoding="utf-8") as skill:
    skill.write("---\nname: hello\ndescription: says hello\n---\nSay hello.\n")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[plugins]\nrepos = ["{T}/skills"]\n')
term = None
try:
    term = Term()
    term.pump(5)
    if "asked again at the next start" in term.text():  # the popup of the start (restore / map this computer): no
        term.send(b"n", 1.5)
    term.send(b"\x1b[20~", 1.5)  # F9: the settings
    for _ in range(40):  # Tab: the next page, up to Plugins
        if "+ Add a source" in term.text():
            break
        term.send(b"\t", 0.3)
    shown = term.text()
    check("Settings > Plugins: a row per source and + Add a source", "+ Add a source" in shown and "skills" in shown, shown[-900:])
    term.send(b"\r", 1.5)  # Enter on its first row: the source
    shown = term.text()
    check("Enter: the Plugins browser opened", "+ Add a source" not in shown and "hello" in shown, shown[-900:])
    term.send(b"\x1b", 1.0)
    shown = term.text()
    check("Esc: back to the settings (their Plugins page), not the sessions", "+ Add a source" in shown, shown[-900:])
    term.send(b"\x1b", 0.8)  # the settings closed
    if "discard" in term.text().lower():
        term.send(b"d", 0.8)
    term.send(b"\x0b", 0.8)  # Ctrl+K: the palette, its Plugins entry
    term.send(b"Plugins", 0.8)
    term.send(b"\r", 1.5)
    opened = "hello" in term.text() or "skills" in term.text()
    term.send(b"\x1b", 1.0)
    check("opened from the palette: Esc closes it (no settings)", opened and "+ Add a source" not in term.text(), term.text()[-700:])
finally:
    if term:
        term.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
