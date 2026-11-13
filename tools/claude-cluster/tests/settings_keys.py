#!/usr/bin/env python3
# Regression test: the settings of the terminal - a key given to two actions marked ! (the Keys tab, its line, the actions named under it),
# a fixed one no longer marked; the settings reopened on the page they were left on.
# Usage: tests/settings_keys.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The Keys page of the terminal settings: a key used twice is marked; the settings reopen where they were left."""
import fcntl
import os
import pty
import re
import select
import signal
import struct
import sys
import tempfile
import termios
import time

import pyte

T = tempfile.mkdtemp(prefix="cc-keys-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", TERM="xterm-256color")
failed = False

def check(name, ok, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

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

    def send(self, data, wait: float = 0.4) -> None:
        os.write(self.fd, data)
        self.pump(wait)

    def kill(self) -> None:
        try:
            os.kill(self.pid, signal.SIGKILL)
        except OSError:
            pass

for folder in ("cfg", "data", "run", "cache"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[keys]\nsearch = "Ctrl+K"\n')
term = None
try:
    term = Term()
    term.pump(5)
    if "asked again at the next start" in term.text():  # the popup of the start (restore / map this computer): no
        term.send(b"n", 1.5)
    term.send(b"\x1b[20~", 1.5)  # F9: the settings
    shown = term.text()
    check("F9: the settings", "Keys" in shown, shown[-600:])
    check("the Keys tab marked ! (search and palette share Ctrl+K)", "Keys !" in shown, shown[:400])
    on_keys = lambda: re.search(r"palette\s*!?\s*\.{3,}", term.text()) is not None  # the Keys page shown: its line "palette ....."
    for _ in range(30):  # Tab: the next page, up to Keys
        if on_keys():
            break
        term.send(b"\t", 0.3)
    shown = term.text()
    lines = [line for line in shown.splitlines() if "palette" in line or "search" in line]
    check("the Keys page: palette and search marked !", any("!" in line and "palette" in line for line in lines) and any("!" in line and "search" in line for line in lines),
          "\n".join(lines))
    # the line of search selected: the actions sharing its key named under it
    for _ in range(80):
        rows = term.text().splitlines()
        if any("Ctrl+K is also" in row for row in rows):
            break
        term.send(b"\x1b[B", 0.15)
    check("a key used twice: the other action named under it", "Ctrl+K is also" in term.text(), term.text()[-900:])
    # closed then opened again: the same page
    term.send(b"\x1b", 0.8)
    if "save" in term.text().lower() and "discard" in term.text().lower():
        term.send(b"d", 0.8)
    term.send(b"\x1b[20~", 1.5)
    check("opened again: on the Keys page it was left on", on_keys() and "Ctrl+K is also" in term.text(), term.text()[-700:])
finally:
    if term:
        term.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
