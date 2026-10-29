#!/usr/bin/env python3
# Context at most: [sessions] context given to claude (CLAUDE_CODE_AUTO_COMPACT_WINDOW) at every start; a spawn asking its own
# (context: 150 = 150k) has it, kept over a restart; auto: nothing set (the window of the model); a value out of 100k..1M refused.
# Usage: tests/context_window.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The context of the sessions limited."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-context-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_") and k != "CLAUDE_CODE_AUTO_COMPACT_WINDOW"}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
failed = False


def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(15)
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


def windows() -> list:
    # the sessions only (stream-json): not the one-shot calls of claude-cluster (the description of a session, haiku)
    out, session = [], False
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        for line in log:
            if line.startswith("ARGV "):
                session = "stream-json" in line
            elif line.startswith("WINDOW ") and session:
                out.append(line.split()[1])
    return out


def started(count: int) -> list:
    deadline = time.time() + 15
    while time.time() < deadline and len(windows()) < count:
        time.sleep(0.2)
    return windows()


for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[sessions]\ncontext = "200k"\n[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "plain", "prompt": "hello"})
    check("[sessions] context = 200k: given to claude", started(1)[-1:] == ["200k"], json.dumps(windows()))
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "small", "prompt": "hello", "context": "150"})
    check("a spawn asking its own (150 = 150k): that one", started(2)[-1:] == ["150k"], json.dumps(windows()))
    got = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "huge", "context": "2M"})
    check("out of 100k..1M: refused with the range", "100k to 1M" in json.dumps(got) and not (got.get("data") or {}).get("id"), json.dumps(got)[:300])
    with open(f"{T}/data/state.json", encoding="utf-8") as state:
        kept = json.dumps(json.load(state))
    check("kept with the session (a restart starts it with it again)", '"context": "150k"' in kept, kept[:300])
    ask({"cmd": "settings_set", "section": "sessions", "key": "context", "value": "auto"})
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "auto", "prompt": "hello"})
    check("auto: nothing set (the window of the model)", started(3)[-1:] == ["auto"], json.dumps(windows()))
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
