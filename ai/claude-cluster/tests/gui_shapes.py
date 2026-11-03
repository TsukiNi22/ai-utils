#!/usr/bin/env python3
# The window (Qt, offscreen) on the same displays as the terminal: wrong shapes, numbers of every size, a chart of each kind: it keeps running; a quit by a signal is a normal end.
# Usage: tests/gui_shapes.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster; built without its window: nothing run, ok)
"""The window keeps running on displays of odd shapes."""
import json
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-guishapes-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home",
           FAKE_CLAUDE_LOG=f"{T}/fake.log", QT_QPA_PLATFORM="offscreen", CLAUDE_CLUSTER_NO_SUPERVISOR="1")
failed = False

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
    return json.loads(data or b"{}")

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\nstart_menu = false\n[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\neditor = "vi"\nlanguage = "en"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
window = None
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    sid = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "shapes", "prompt": "hello"}).get("data", {}).get("id", "s1")
    time.sleep(1)
    window = subprocess.Popen([BIN, "--gui"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    time.sleep(5)
    if window.poll() is not None:
        out = window.stderr.read().decode("utf-8", "replace")
        if "built without" in out or "no window" in out.lower() or "display" in out.lower():
            print("ok   no window here: not run (" + out[:120] + ")", flush=True)
            sys.exit(0)
    check("the window runs (offscreen)", window.poll() is None, str(window.poll()))
    displays = [{"type": "chart", "kind": kind, "labels": ["a", "b", "c"], "series": [{"name": "a", "values": values}, {"name": "b", "values": values[::-1]}]}
                for kind in ("pie", "bar", "line") for values in ([1e300, -1e300, 1], [1e308, -1e308], [1e308, 1e308], [-1e308], [0, 0], [1, -1], [])]
    displays += [{"type": "metrics", "items": [{"label": "a", "value": 1e308, "delta": -1e308, "trend": [1e308, -1e308]}]}, {"type": "heatmap", "rows": [[1e308, -1e308], [-1e308, 1e-308]]}, {"type": "heatmap", "rows": []},
                 {"type": "progress", "value": 1e308, "max": 1e-308}, {"type": "progress", "value": -1e308, "max": -1e308}, {"type": "timeline", "tasks": [{"name": "a", "start": "2026-01-01", "end": "2026-01-02"}, {"name": "b"}]},
                 {"type": "markdown", "text": "x" * 100000}, {"type": "table", "columns": [1e308, None, []], "rows": [[-1e308, {}, [1]], "x", 1]}, {"type": "kanban", "columns": [{"name": 1, "cards": [1, None, {}]}]},
                 {"type": "checklist", "items": [{"text": 1, "done": "x"}, 2]}, {"type": "actions", "buttons": [{"label": 1, "prompt": []}, 3]}, {"type": "form", "fields": [{"name": 1, "type": "number", "value": 1e308}, 2]},
                 {"type": "compare", "left": 1, "right": []}, {"type": "tree", "root": "x"}, {"type": "details", "text": []}, {"type": "json", "data": {"a": [1, {"b": None}]}}, {"type": "unknown"}, {"id": 1}]
    for display in displays:
        ask({"cmd": "display", "session": sid, "display": display})
        time.sleep(0.15)
        if window.poll() is not None:
            print("     the window ended on " + json.dumps(display)[:300], flush=True)
            break
    time.sleep(2)
    check("every display drawn: the window alive", window.poll() is None, str(window.poll()))
    window.send_signal(signal.SIGTERM)
    try:
        code = window.wait(20)
    except subprocess.TimeoutExpired:
        window.kill()
        code = "not ended in 20 s"
    check("a normal end", code == 0 or code == -15, str(code))
    check("the core alive", core.poll() is None, str(core.poll()))
finally:
    if window is not None and window.poll() is None:
        window.kill()
    core.terminate()
    try:
        core.wait(15)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
