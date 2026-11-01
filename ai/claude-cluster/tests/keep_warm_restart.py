#!/usr/bin/env python3
# The sessions kept warm (/keepwarm, mode chosen) are still kept warm after a restart of the core (an update): their choice is saved in
# state.json (it was not before 1.9.2, every restart left them to cool).
# Usage: tests/keep_warm_restart.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""Keep warm chosen, kept over a restart."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-warmbg-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_agents.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV["CLAUDE_CLUSTER_TEST"] = "1"
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_OUT=f"{T}/out")
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

def wakes() -> int:
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        return sum(1 for line in log if line.startswith("PROMPT") and "keep-warm" in line)

def listed(sid: str) -> dict:
    return next((s for s in ask({"cmd": "list"}).get("data") or [] if s.get("id") == sid), {})

def running(sid: str) -> list:
    return [a for a in ask({"cmd": "agents"}).get("data") or [] if a.get("session") == sid and a.get("status") == "running"]

for folder in ("cfg", "data", "run", "cache", "home", "work", "out"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[sessions]\nkeep_warm = "chosen"\nkeep_warm_ttl = 1\nkeep_warm_margin = 16\n[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "build", "prompt": "compile"}).get("data") or {}).get("id", "s1")
    deadline = time.time() + 20
    while time.time() < deadline and not (listed(sid).get("state") == "idle" and running(sid)):
        time.sleep(0.5)
    check("idle", listed(sid).get("state") == "idle", json.dumps(listed(sid))[:200])
    ask({"cmd": "keep_warm", "session": sid, "value": "on"})
    check("kept warm (♨)", listed(sid).get("keep_warm") is True, json.dumps(listed(sid))[:200])
    time.sleep(2)
    core.terminate()
    core.wait(20)
    os.remove(f"{T}/sock") if os.path.exists(f"{T}/sock") else None
    core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(1)
    check("still kept warm after the restart (♨)", listed(sid).get("keep_warm") is True, json.dumps(listed(sid))[:200])
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
sys.exit(1 if failed else 0)
