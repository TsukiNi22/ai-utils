#!/usr/bin/env python3
# A heavy job active (queued, running) never breaks the state the interfaces sync: the sync and orch_state answer while it runs (they
# failed on a null read of its count: every interface in Reconnecting while a job was active).
# Usage: tests/core_supervisor.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The sync answers while a heavy job runs."""
import json
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-orchsync-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log")
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


def core_pid(wait: float = 15):
    end = time.time() + wait
    while time.time() < end:
        try:
            return (ask({"cmd": "hello"}).get("data") or {}).get("pid", 0)
        except OSError:
            time.sleep(0.3)
    return 0


for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    core_pid()
    build = (ask({"cmd": "hello"}).get("data") or {}).get("build", "")
    added = ask({"cmd": "job_add", "command": "sleep 20", "name": "long-build", "class": "build", "cwd": f"{T}/work", "callback": False})
    check("a heavy job queued", added.get("ok") is True, json.dumps(added)[:300])
    time.sleep(1.5)
    state = ask({"cmd": "orch_state"})
    check("orch_state answers while it is active", state.get("ok") is True, json.dumps(state)[:300])
    jobs = (state.get("data") or {}).get("jobs", [])
    check("the job is active", any(j.get("state") in ("queued", "running") for j in jobs), json.dumps(jobs)[:300])
    sync = ask({"cmd": "sync", "client": "t:1", "v": 0, "known": {}, "build": build})
    check("the sync of the interfaces answers while it is active", sync.get("ok") is True, json.dumps(sync)[:300])
finally:
    core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
