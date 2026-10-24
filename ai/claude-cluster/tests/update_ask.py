#!/usr/bin/env python3
# The smooth update and a session waiting for the user (a question): it does not hold the update back - its question held (held.json,
# what it asked), its turn stopped; the new version tells it, so it asks again (its popup back).
# Usage: tests/update_hold.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A question held across an update, asked again after it."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-holdask-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_ask.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_TEST="1")
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


def worker():
    return next((s for s in ask({"cmd": "list"}).get("data", []) if s.get("name") == "worker"), {})


def prompts():
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        return [line for line in log.read().split("\n") if line.startswith("PROMPT ") and "HOLD-" in line]


for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(50):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.2)
    session = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker", "prompt": "please ask me the colour"}).get("data") or {}).get("id", "s1")
    for _ in range(30):
        if worker().get("state") in ("waiting", "permission"):
            break
        time.sleep(0.3)
    check("the session waits for the user (its question)", worker().get("state") in ("waiting", "permission"), json.dumps({k: worker().get(k) for k in ("state",)}))
    held = ask({"cmd": "upgrade", "action": "hold"})
    with open(f"{T}/data/held.json", encoding="utf-8") as file:
        kept = json.load(file)
    text = kept[0].get("text", "") if kept else ""
    check("its question held for the new version (what it asked)", "updated while you waited for the user" in text and "Which colour for the logo?" in text, json.dumps(kept)[:400])
    for _ in range(30):
        if worker().get("state") == "idle":
            break
        time.sleep(0.3)
    check("its turn stopped: it no longer holds the update back", worker().get("state") == "idle", json.dumps({k: worker().get(k) for k in ("state",)}))
    ask({"cmd": "upgrade", "action": "release"})
    for _ in range(30):
        with open(f"{T}/fake.log", encoding="utf-8") as log:
            if "was updated while you waited" in log.read():
                break
        time.sleep(0.3)
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        check("after the switch: told to it, so it asks again", "was updated while you waited" in log.read())
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
