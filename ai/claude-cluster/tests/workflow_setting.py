#!/usr/bin/env python3
# [agent] workflow, the multi-agent workflows of the model itself: off by default - the tool taken away from claude (--disallowedTools Workflow), not allowed in its system
# prompt; on - no such flag, asked in its system prompt, for the sessions started from then on. The row is under the orchestration mode; the global session never changes it.
# Usage: tests/workflow_setting.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The setting of the workflows of the model."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-workflow-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
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

def last_args() -> list:
    # the command line of the last session started (not the one-shot calls of claude-cluster: a description of a session has no stream-json)
    found = []
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        for line in log:
            if line.startswith("ARGV "):
                args = json.loads(line[5:])
                if "stream-json" in args:
                    found = args
    return found

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    rows = ask({"cmd": "settings_get", "section": "agent"}).get("data") or []
    keys = [row.get("key") for row in rows]
    check("its row right under the orchestration mode", "workflow" in keys and keys.index("workflow") == keys.index("orchestration") + 1, str(keys)[:300])
    check("off by default", next((row.get("value") for row in rows if row.get("key") == "workflow"), "") == "false")
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "off"})
    time.sleep(1.5)
    args = last_args()
    check("off: the tool taken away", "--disallowedTools" in args and args[args.index("--disallowedTools") + 1] == "Workflow", str(args)[:300])
    check("off: not allowed in its system prompt", "Workflows of the model: not allowed" in " ".join(args))
    refused = ask({"cmd": "settings_set", "origin": "mcp", "section": "agent", "key": "workflow", "value": "true"})
    check("the global session never changes it", refused.get("ok") is False, json.dumps(refused)[:200])
    done = ask({"cmd": "settings_set", "section": "agent", "key": "workflow", "value": "true"})
    check("the user does", done.get("ok") is True, json.dumps(done)[:200])
    time.sleep(1.5)
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "on"})
    time.sleep(1.5)
    args = last_args()
    check("on: the tool given", "--disallowedTools" not in args, str(args)[:300])
    check("on: asked in its system prompt", "Workflows of the model: asked" in " ".join(args))
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
