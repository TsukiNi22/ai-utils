#!/usr/bin/env python3
# What a frame of the terminal draws of a display stays bounded whatever its JSON holds (the transcript is rebuilt at every frame): a few KB never cost rows x columns of memory and
# time: tables, charts, kanbans, a markdown table, buttons, a line of brackets, megabytes in one cell, line breaks by the million. The screen and its keys answer, what is cut is told;
# a table of a normal size is whole, Ctrl+O shows more. A WAV ending in the header of its fmt chunk: its length read inside the file.
# Usage: tests/display_bounds.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""Displays of a few KB: a frame of the terminal stays small."""
import fcntl
import json
import os
import pty
import select
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import termios
import time

import pyte

ROOT = tempfile.mkdtemp(prefix="cc-dispbounds-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
N = 12000
GROWTH_MB = 700     # the peak of the terminal over what it held before the display (the sanitizers of a test build included): gigabytes without a bound
KEY_SECONDS = 15    # a key drawn: at each frame the whole display was built again, minutes without a bound
KILL_MB = 3000      # a terminal past this: killed here, never the memory of the computer running this test
failed = False
used = 0

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

class Term:
    def __init__(self, env: dict) -> None:
        self.ended = False
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], env)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 160, 0, 0))
        self.screen = pyte.Screen(160, 50)
        self.stream = pyte.ByteStream(self.screen)

    def pump(self, sec: float) -> None:
        end = time.time() + sec
        while time.time() < end:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    chunk = os.read(self.fd, 1 << 18)
                except OSError:
                    return
                try:
                    self.stream.feed(chunk)
                except TypeError:
                    pass

    def first_frame(self) -> None:
        # the interface drawn (a build with sanitizers on a busy machine: seconds), the map of this computer asked at its start answered
        end = time.time() + 40
        while time.time() < end and "claude-cluster" not in self.text():
            self.pump(0.2)
        self.pump(1.5)
        if "asked again at the next start" in self.text():
            self.send(b"n", 1.5)

    def text(self) -> str:
        return "\n".join(line.rstrip() for line in self.screen.display)

    def send(self, data: bytes, wait: float = 0.6) -> None:
        os.write(self.fd, data)
        self.pump(wait)

    def watch(self, before: int, sec: float) -> None:
        end = time.time() + sec
        while time.time() < end and self.alive():
            self.pump(0.2)
            if self.peak_mb() - before > KILL_MB:
                self.kill()

    def peak_mb(self) -> int:
        try:
            with open(f"/proc/{self.pid}/status", encoding="utf-8") as f:
                return int(f.read().split("VmHWM:")[1].split()[0]) // 1024
        except (OSError, IndexError):
            return -1

    def alive(self) -> bool:
        if self.ended:
            return False
        self.ended = os.waitpid(self.pid, os.WNOHANG)[0] != 0
        return not self.ended

    def kill(self) -> None:
        if self.alive():
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
            self.ended = True

class Box:
    def __init__(self) -> None:
        global used
        used += 1
        self.T = f"{ROOT}/{used}"
        T = self.T
        self.env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
        self.env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
                        HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color", CLAUDE_CLUSTER_NO_SUPERVISOR="1")
        for folder in ("cfg", "data", "run", "cache", "home", "work"):
            os.makedirs(f"{T}/{folder}", mode=0o700)
        open(f"{T}/fake.log", "w", encoding="utf-8").close()
        with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
            config.write('[global]\nenabled = false\n[ui]\nnotify = false\nstart_menu = false\nmachine_days = 0\n[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\neditor = "vi"\nlanguage = "en"\n'
                         f'[usage]\naccount = false\n[commands]\nclaude = "{FAKE}"\n')
        self.core = subprocess.Popen([BIN, "serve", "--no-global"], env=self.env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if os.path.exists(f"{T}/sock"):
                break
            time.sleep(0.1)
        time.sleep(0.5)
        self.sid = self.ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "bounds"}).get("data", {}).get("id", "s1")
        self.term = Term(self.env)
        self.term.first_frame()

    def ask(self, request: dict) -> dict:
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(120)
        s.connect(f"{self.T}/sock")
        s.sendall((json.dumps(request) + "\n").encode())
        data = b""
        while not data.endswith(b"\n"):
            chunk = s.recv(1 << 20)
            if not chunk:
                break
            data += chunk
        s.close()
        return json.loads(data or b"{}")

    def close(self) -> None:
        self.term.kill()
        if self.core.poll() is None:
            self.core.terminate()
            try:
                self.core.wait(15)
            except subprocess.TimeoutExpired:
                self.core.kill()
        os.system(f"pkill -f {self.T} > /dev/null 2>&1")

SHAPES = {
    "table of empty rows under a wide one": ({"type": "table", "rows": [["x"] * N] + [[] for _ in range(N)]}, "more rows"),
    "table of one cell of 2 MB": ({"type": "table", "rows": [["y" * 2000000]]}, ""),
    "kanban": ({"type": "kanban", "columns": [{"name": "a", "cards": ["x"] * N}] + [{"name": "b", "cards": []} for _ in range(N // 3)]}, "more columns"),
    "chart, count x series": ({"type": "chart", "series": [{"name": "a", "values": [1] * N}] + [{"values": []} for _ in range(N)]}, "more bars"),
    "chart of lines": ({"type": "chart", "kind": "line", "series": [{"values": []} for _ in range(N)]}, "more series"),
    "markdown table": ({"type": "markdown", "text": "|" + "x|" * N + "\n|---|\n" + "|\n" * N}, "more rows"),
    "buttons": ({"type": "actions", "buttons": [{"label": "x"} for _ in range(30000)]}, "more buttons"),
    "brackets on a line": ({"type": "markdown", "text": "[" * 800000}, ""),
    "brackets and one close": ({"type": "markdown", "text": "[" * 400000 + "]"}, ""),
    "links never closed": ({"type": "markdown", "text": "[a](" * 150000}, ""),
    "a checklist of empty items": ({"type": "checklist", "items": [{"text": "i"} for _ in range(60000)]}, "more items"),
    "a timeline of tasks": ({"type": "timeline", "tasks": [{"name": "a", "start": 1, "end": 2} for _ in range(30000)]}, "more tasks"),
    "a heatmap": ({"type": "heatmap", "rows": [[1] * N] + [[] for _ in range(N)]}, "more rows"),
    "a code of a million lines": ({"type": "code", "code": "x\n" * 1000000}, "more lines"),
    "a markdown of a million lines": ({"type": "markdown", "text": "x\n\n" * 1000000}, "more lines"),
    "a code of nothing but line breaks": ({"type": "code", "code": "\n" * 2000000}, ""),
}
boxes = []
try:
    only = sys.argv[2] if len(sys.argv) > 2 else None
    for name, (display, note) in SHAPES.items():
        if only and only != name:
            continue
        box = Box()
        boxes.append(box)
        term = box.term
        before = term.peak_mb()
        answer = box.ask({"cmd": "display", "session": box.sid, "display": display})
        check(f"{name} ({len(json.dumps(display)) // 1024} KB): taken", answer.get("ok") is True, json.dumps(answer)[:200])
        term.watch(before, 2)
        if term.alive():
            os.write(term.fd, b"qk")
        began = time.time()
        drawn = False
        while time.time() - began < KEY_SECONDS and term.alive():
            term.watch(before, 0.3)
            if "› qk" in term.text():
                drawn = True
                break
        check(f"{name}: a key drawn at once", drawn, f"after {time.time() - began:.1f} s; {term.text()[-300:]}")
        check(f"{name}: drawn, not an error", term.alive() and "could not be drawn" not in term.text() and "unexpected shape" not in term.text(), term.text()[:300])
        if note:
            term.send(b"\x7f\x7f", 1.0)
            for _ in range(60):  # a build with sanitizers: 20000 rows at each frame
                if note in term.text():
                    break
                term.watch(before, 0.5)
            check(f"{name}: what is cut is told", note in term.text(), term.text()[-500:])
        check(f"{name}: its memory bounded", term.alive() and term.peak_mb() - before < GROWTH_MB, f"{before} -> {term.peak_mb()} MB")
        box.close()

    if not only:
        # a table of a normal size: whole; past the first rows: its end told, Ctrl+O shows more
        box = Box()
        boxes.append(box)
        term = box.term
        box.ask({"cmd": "display", "session": box.sid, "display": {"type": "table", "columns": ["id", "name"], "rows": [[i, f"row{i}"] for i in range(300)]}})
        term.pump(2.5)
        check("a table of 300 rows: whole", "row299" in term.text() and "more rows" not in term.text(), term.text()[-400:])
        box.ask({"cmd": "display", "session": box.sid, "display": {"type": "table", "columns": ["id", "name"], "rows": [[i, f"big{i}"] for i in range(450)]}})
        term.pump(2.5)
        check("a table of 450 rows: its first ones, 50 told", "big399" in term.text() and "big400" not in term.text() and "50 more rows" in term.text(), term.text()[-400:])
        term.send(b"\x0f", 2.0)  # Ctrl+O
        check("     Ctrl+O: more", "big449" in term.text() and "more rows" not in term.text(), term.text()[-400:])
        term.send(b"\x0f", 1.0)
        # a WAV that ends in the header of its fmt chunk: its rate past the end of the file was read
        for size in range(20, 40):
            wav = (b"RIFF" + (size - 8).to_bytes(4, "little") + b"WAVEfmt " + (16).to_bytes(4, "little") + b"\x01" * 40)[:size]
            with open(f"{box.T}/work/short{size}.wav", "wb") as out:
                out.write(wav)
            answer = box.ask({"cmd": "display", "session": box.sid, "display": {"type": "audio", "path": f"{box.T}/work/short{size}.wav", "alt": f"short{size}"}})
            if answer.get("ok") is not True:
                check(f"an audio of {size} bytes taken", False, json.dumps(answer)[:200])
            term.pump(0.3)
        term.pump(1.5)
        check("audio files cut in their header: drawn, the terminal alive", term.alive() and "short39" in term.text() and "could not be drawn" not in term.text(), term.text()[-400:])
        box.close()
finally:
    for box in boxes:
        box.close()
    os.system(f"pkill -f {ROOT} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
