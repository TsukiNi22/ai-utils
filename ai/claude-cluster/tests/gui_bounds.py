#!/usr/bin/env python3
# The window (Qt, offscreen): a heatmap or a timeline of a few KB draws a bounded picture: its size follows what can be shown, never the numbers of its JSON (17 KB asked 800 MB); the
# window keeps running, a normal heatmap still drawn.
# Usage: tests/gui_bounds.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster; built without its window: nothing run, ok)
"""The pictures of the displays in the window stay bounded."""
import json
import os
import resource
import signal
import socket
import subprocess
import sys
import tempfile
import time

ROOT = tempfile.mkdtemp(prefix="cc-guibounds-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
GROWTH_MB = 400     # the peak of the window over what it held before (the sanitizers of a test build included)
N = 1800
SHAPES = {
    "a heatmap of 1800 x 1800 (empty rows)": {"type": "heatmap", "rows": [[None] * N] + [[] for _ in range(N)]},
    "a heatmap of one column, 20000 rows": {"type": "heatmap", "rows": [[1] for _ in range(20000)]},
    "a heatmap of one row, 20000 columns": {"type": "heatmap", "rows": [[1] * 20000]},
    "a timeline of 12000 tasks": {"type": "timeline", "tasks": [{"name": "a", "start": 1, "end": 2} for _ in range(12000)]},
}
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def peak_mb(pid: int) -> int:
    try:
        with open(f"/proc/{pid}/status", encoding="utf-8") as f:
            return int(f.read().split("VmHWM:")[1].split()[0]) // 1024
    except (OSError, IndexError):
        return -1

def run(name: str, display: dict, index: int) -> bool:
    T = f"{ROOT}/{index}"
    env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
    env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home",
               FAKE_CLAUDE_LOG=f"{T}/fake.log", QT_QPA_PLATFORM="offscreen", CLAUDE_CLUSTER_NO_SUPERVISOR="1")
    for folder in ("cfg", "data", "run", "cache", "home", "work"):
        os.makedirs(f"{T}/{folder}", mode=0o700)
    open(f"{T}/fake.log", "w", encoding="utf-8").close()
    with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
        config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\nstart_menu = false\n[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\neditor = "vi"\nlanguage = "en"\n[usage]\naccount = false\n'
                     f'[commands]\nclaude = "{FAKE}"\n')

    def ask(request: dict) -> dict:
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(60)
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

    core = subprocess.Popen([BIN, "serve", "--no-global"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    window = None
    try:
        for _ in range(100):
            if os.path.exists(f"{T}/sock"):
                break
            time.sleep(0.1)
        time.sleep(0.5)
        sid = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "bounds"}).get("data", {}).get("id", "s1")
        time.sleep(0.5)
        window = subprocess.Popen([BIN, "--gui"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        time.sleep(8)
        if window.poll() is not None:
            out = window.stderr.read().decode("utf-8", "replace")
            if "not built" in out or "no window" in out.lower() or "display" in out.lower():
                print("ok   no window here: not run (" + out[:120] + ")", flush=True)
                return False
        check(f"{name}: the window runs", window.poll() is None, str(window.poll()))
        before = peak_mb(window.pid)
        ask({"cmd": "display", "session": sid, "display": display})
        ask({"cmd": "ui_focus", "session": sid})
        high = before
        for _ in range(12):
            time.sleep(1)
            if window.poll() is not None:
                break
            high = max(high, peak_mb(window.pid))
        check(f"{name}: the window alive", window.poll() is None, str(window.poll()))
        check(f"{name}: its picture bounded", high - before < GROWTH_MB, f"{before} -> {high} MB")
        # a heatmap of a normal size, after it: drawn, the window goes on
        ask({"cmd": "display", "session": sid, "display": {"type": "heatmap", "rows": [[1, 2, 3], [4, 5, 6]], "xlabels": ["a", "b", "c"], "ylabels": ["x", "y"]}})
        time.sleep(1.5)
        check(f"{name}: a normal one after it", window.poll() is None, str(window.poll()))
        window.send_signal(signal.SIGTERM)
        try:
            code = window.wait(20)
        except subprocess.TimeoutExpired:
            window.kill()
            code = "not ended in 20 s"
        check(f"{name}: a normal end", code == 0 or code == -15, str(code))
        return True
    finally:
        if window is not None and window.poll() is None:
            window.kill()
        core.terminate()
        try:
            core.wait(15)
        except subprocess.TimeoutExpired:
            core.kill()
        os.system(f"pkill -f {T} > /dev/null 2>&1")

for at, (title, shape) in enumerate(SHAPES.items()):
    if not run(title, shape, at):
        break
sys.exit(1 if failed else 0)
