#!/usr/bin/env python3
# A display of any shape (an agent writes them, they stay in its log): every type, every field of the wrong kind or missing, a question of the agent with odd options -
# the core answers, a terminal showing them keeps running, the page reads its state. A wrong shape fails that display, never an interface or the core.
# Usage: tests/display_shapes.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""Displays of odd shapes: the core and a terminal go on."""
import fcntl
import json
import os
import pty
import resource
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

T = tempfile.mkdtemp(prefix="cc-shapes-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color")
failed = False
TYPES = ["markdown", "table", "chart", "image", "diagram", "svg", "callout", "progress", "code", "diff", "json", "tree", "terminal", "checklist", "steps", "timeline", "kanban",
         "actions", "form", "details", "compare", "metrics", "heatmap", "audio", "math", "file"]
FIELDS = ["text", "title", "rows", "columns", "series", "labels", "kind", "value", "max", "code", "language", "diff", "before", "after", "data", "root", "output", "items", "steps",
          "events", "lanes", "cards", "buttons", "fields", "summary", "left", "right", "values", "matrix", "path", "paths", "files", "svg", "dot", "mime", "alt", "tone", "level", "formula", "tex", "file"]
BAD = [None, 1, -1, 1.5, True, "", "x", [], {}, [1, "a", None, [], {}], {"a": 1, "b": [1], "c": None}, [[1, None], ["a", {}]], [{"x": 1}, {"label": 3, "name": [], "text": {}}]]
# a display of each type accepted as it is: its parts then replaced by what they should not be
BASE = {"markdown": {"text": "x"}, "table": {"columns": ["a"], "rows": [[1]]}, "chart": {"labels": ["a"], "series": [{"name": "a", "values": [1]}]}, "svg": {"svg": "<svg></svg>"},
        "callout": {"level": "info", "text": "x"}, "progress": {"value": 1, "max": 2, "label": "x"}, "code": {"code": "x", "language": "c"}, "diff": {"diff": "x"}, "json": {"data": {"a": 1}},
        "tree": {"root": {"name": "x", "dir": True, "children": [{"name": "y"}]}}, "terminal": {"output": "x", "command": "y"}, "checklist": {"items": [{"text": "a", "done": False}]},
        "steps": {"steps": [{"text": "a", "state": "done"}]}, "timeline": {"tasks": [{"name": "a", "start": "2026-01-01", "end": "2026-01-02"}]},
        "kanban": {"columns": [{"name": "a", "cards": ["b"]}]}, "actions": {"buttons": [{"label": "a", "prompt": "b"}]}, "form": {"fields": [{"name": "a", "label": "b", "type": "text", "value": "c"}], "submit": "s"},
        "details": {"summary": "x", "text": "y"}, "compare": {"left": {"title": "a", "text": "b"}, "right": {"title": "c", "text": "d"}},
        "metrics": {"items": [{"label": "a", "value": 1, "delta": "+1", "trend": [1, 2]}]}, "heatmap": {"rows": [[1, 2]], "xlabels": ["a"], "ylabels": ["b"]}, "math": {"latex": "x"},
        "image": {"data": "aGk=", "mime": "image/png"}}
# numbers of every size: an index out of its table (inf / inf), a bar without end (the memory of the computer gone)
NUMBERS = [{"type": "chart", "kind": kind, "labels": ["a", "b", "c"], "series": [{"name": "a", "values": values}, {"name": "b", "values": values[::-1]}]}
           for kind in ("pie", "bar", "line") for values in ([1e300, -1e300, 1], [1e308, -1e308], [1e308, 1e308], [-1e308], [0, 0], [1e-308, -1e-308], [2 ** 63, -2 ** 63], [1, -1])]
NUMBERS += [{"type": "metrics", "items": [{"label": "a", "value": 1e308, "delta": -1e308, "trend": trend}]} for trend in ([1e308, -1e308], [1e300, -1e300, 1], [-1e308, 0, 1e308])]
NUMBERS += [{"type": "heatmap", "rows": [[1e308, -1e308], [-1e308, 1e-308]]}, {"type": "heatmap", "rows": [[-1e308, 0]]}, {"type": "progress", "value": 1e308, "max": 1e-308}, {"type": "progress", "value": -1e308, "max": -1e308},
            {"type": "progress", "value": 1e308, "max": 1e308}, {"type": "timeline", "tasks": [{"name": "a", "start": "2026-01-01", "end": "2026-01-02", "from": -1e308, "to": 1e308}, {"name": "b", "from": 1e308, "to": -1e308}]},
            {"type": "form", "fields": [{"name": "a", "label": "b", "type": "number", "value": 1e308}]}, {"type": "tree", "depth": 1e308, "root": {"name": "x"}}, {"type": "table", "columns": [1e308], "rows": [[-1e308, 1e-308]]}]

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(30)
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
        self.ended = False
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            if "ASAN_OPTIONS" not in os.environ and "TSAN_OPTIONS" not in os.environ: # a regression here never takes the memory of the computer with it
                resource.setrlimit(resource.RLIMIT_AS, (4 << 30, 4 << 30))
            os.execve(BIN, [BIN, "--tty"], ENV)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 160, 0, 0))
        self.screen = pyte.Screen(160, 50)
        self.stream = pyte.ByteStream(self.screen)

    def pump(self, sec: float) -> None:
        end = time.time() + sec
        while time.time() < end:
            ready, _, _ = select.select([self.fd], [], [], 0.1)
            if ready:
                try:
                    self.stream.feed(os.read(self.fd, 1 << 16))
                except OSError:
                    return

    def alive(self) -> bool:
        if self.ended:
            return False
        self.ended = os.waitpid(self.pid, os.WNOHANG)[0] != 0
        return not self.ended

    def tail(self) -> str:
        return " | ".join(line.strip() for line in self.screen.display if line.strip())[-1500:]

    def keys(self, data: bytes) -> None:
        os.write(self.fd, data)

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\nstart_menu = false\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
term = None
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "shapes", "prompt": "hello"}).get("data") or {}).get("id", "s1")
    time.sleep(1.5)
    term = Term()
    term.pump(3)
    term.keys(b"\x1b[B")  # the session in the list
    term.pump(1)
    made = 0
    refused = 0
    last = None
    for kind in TYPES:
        if term.ended:
            break
        for field in FIELDS:
            if term.ended:
                break
            for bad in BAD:
                last = {"type": kind, field: bad}
                answer = ask({"cmd": "display", "session": sid, "display": last})
                made += 1 if answer.get("ok") else 0
                refused += 0 if answer.get("ok") else 1
                if answer.get("ok"):
                    term.pump(0.05)
                    if not term.alive():
                        print("     the terminal ended on " + json.dumps(last) + ": " + term.tail(), flush=True)
                        break
        # the fields it needs of the wrong kind together
        for bad in BAD:
            if term.ended:
                break
            last = dict({"type": kind}, **{field: bad for field in FIELDS})
            answer = ask({"cmd": "display", "session": sid, "display": last})
            made += 1 if answer.get("ok") else 0
            refused += 0 if answer.get("ok") else 1
            if answer.get("ok"):
                term.pump(0.05)
                if not term.alive():
                    print("     the terminal ended on " + json.dumps(last)[:400] + ": " + term.tail(), flush=True)
    # accepted displays, their inside of the wrong kind: drawn by the terminal
    def one(display: object) -> None:
        global made, refused
        if term.ended:
            return
        answer = ask({"cmd": "display", "session": sid, "display": display})
        made += 1 if answer.get("ok") else 0
        refused += 0 if answer.get("ok") else 1
        if answer.get("ok"):
            term.pump(0.02)
            if not term.alive():
                print("     the terminal ended on " + json.dumps(display)[:400] + ": " + term.tail(), flush=True)

    for display in NUMBERS:
        one(display)
        term.pump(0.2)
    check("numbers of every size: the terminal alive", term.alive(), term.tail())
    for kind, base in BASE.items():
        for field, value in base.items():
            for bad in BAD:
                one(dict({"type": kind}, **dict(base, **{field: bad})))
            if isinstance(value, list) and value and isinstance(value[0], dict):
                for inner in value[0]:
                    for bad in BAD:
                        one(dict({"type": kind}, **dict(base, **{field: [dict(value[0], **{inner: bad})]})))
            if isinstance(value, dict):
                for inner in value:
                    for bad in BAD:
                        one(dict({"type": kind}, **dict(base, **{field: dict(value, **{inner: bad})})))
    print(f"     {made} displays kept, {refused} refused", flush=True)
    check("the core alive after them", core.poll() is None and ask({"cmd": "list"}).get("ok") is True)
    term.pump(5)
    check("a terminal showing them alive", term.alive(), "the terminal ended")
    state = ask({"cmd": "read", "session": sid})
    check("its transcript read", state.get("ok") is True, json.dumps(state)[:200])
    # a new terminal on them (they stay in the log: a crash at each start)
    term2 = Term()
    term2.pump(4)
    check("a new terminal on them alive", term2.alive(), term2.tail())
    if term2.alive():
        os.kill(term2.pid, signal.SIGKILL)
    # displays an older version let in: in the log, read at the next start
    if term.alive():
        os.kill(term.pid, signal.SIGKILL)
    time.sleep(1)
    core.terminate()
    core.wait(20)
    with open(f"{T}/data/logs/{sid}.jsonl", encoding="utf-8") as log:
        kinds = [json.loads(line) for line in log]
    kind = next((k["kind"] for k in kinds if isinstance(k.get("text"), str) and k["text"].startswith("{\"") and "\"type\"" in k["text"]), 7)
    with open(f"{T}/data/logs/{sid}.jsonl", "a", encoding="utf-8") as log:
        for display in ({"type": "markdown", "text": 1}, {"type": "table", "rows": 3, "columns": {}}, {"type": "chart", "series": "x"}, {"type": "progress", "value": "x", "max": []},
                        {"type": "image", "data": 1}, {"type": "file", "files": 1}, {"type": "compare", "left": 1, "right": []}, {"type": "tree", "root": "x"}, {"type": "details", "text": []},
                        {"type": "kanban", "columns": 1}, {"type": "metrics", "items": 1}, {"type": "form", "fields": 1}, {"type": "actions", "buttons": 1}, {"type": "unknown"}, {"id": 1}):
            log.write(json.dumps({"time": int(time.time()), "kind": kind, "text": json.dumps(display), "tool": "", "origin": ""}) + "\n")
    if os.path.exists(f"{T}/sock"):
        os.remove(f"{T}/sock")
    core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(1)
    term = Term()
    term.pump(4)
    term.keys(b"\x1b[B")
    term.pump(4)
    check("a terminal on the displays of an older version alive", term.alive(), term.tail())
    check("the core reads them (search, export)", ask({"cmd": "search", "text": "x"}).get("ok") is True and ask({"cmd": "export", "session": sid}).get("ok") is True)
    check("the core alive at the end", core.poll() is None)
finally:
    if term is not None and not term.ended:
        try:
            os.kill(term.pid, signal.SIGKILL)
        except OSError:
            pass
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
