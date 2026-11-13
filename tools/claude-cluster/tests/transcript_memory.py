#!/usr/bin/env python3
# The transcripts in memory kept by size, the older entries read again on demand: a session whose log holds ~4 MB is restored with
# [sessions] memory_mb = 1 - its newest entries only in memory (under 1 MB), the rest counted as older; "older" puts entries back in
# front (the memory grows, older shrinks), up to the very start of the log.
# Usage: tests/transcript_memory.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The memory budget of the transcripts and the older entries loaded again."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-tmem-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_wake.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", HOME=f"{T}/home")
ENTRIES = 4000
failed = False


def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(20)
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


def serve(*extra: str) -> subprocess.Popen:
    core = subprocess.Popen([BIN, "serve", "--no-global", *extra], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            try:
                ask({"cmd": "hello"})
                return core
            except OSError:
                pass
        time.sleep(0.1)
    return core


def stop(core: subprocess.Popen) -> None:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()


def in_memory(sid: str) -> int:
    return (ask({"cmd": "transcript", "session": sid, "from": 0, "count": 1}).get("data") or {}).get("total", -1)


for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[sessions]\nmemory_mb = 1\nmemory_total_mb = 100\ntranscript_limit_mb = 0\n[commands]\nclaude = "{FAKE}"\n')
core = serve()
try:
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "big"}).get("data") or {}).get("id", "s1")
    time.sleep(2)
    stop(core)
    # a long conversation in its log: entry i says "entry <i>" and ~1 KB more
    with open(f"{T}/data/logs/{sid}.jsonl", "a", encoding="utf-8") as log:
        for i in range(ENTRIES):
            log.write(json.dumps({"time": 1700000000 + i, "kind": 1, "text": f"entry {i} " + "x" * 1000, "tool": "", "origin": ""}) + "\n")
    core = serve("--restore")
    time.sleep(2)
    first = in_memory(sid)
    check("restored: its newest entries only in memory (under 1 MB)", 300 < first < 1100, str(first))
    head = (ask({"cmd": "transcript", "session": sid, "from": 0, "count": 1}).get("data") or {}).get("entries", [{}])[0].get("text", "")
    check("the last entry of the log is in memory, the first one not", "entry 0 " not in head, head[:40])
    got = ask({"cmd": "older", "session": sid, "count": 300}).get("data") or {}
    check("older: 300 entries read again", got.get("loaded") == 300, json.dumps(got))
    check("more in memory now", in_memory(sid) == first + 300, str(in_memory(sid)))
    left = got.get("older", -1)
    while left > 0:
        got = ask({"cmd": "older", "session": sid, "count": 5000}).get("data") or {}
        left = got.get("older", 0) if got.get("loaded", 0) > 0 else 0
    head = (ask({"cmd": "transcript", "session": sid, "from": 0, "count": 1}).get("data") or {}).get("entries", [{}])[0].get("text", "")
    check("back to the very start of the log", head.startswith("entry 0 "), head[:40])
    check("nothing older left", got.get("older", -1) == 0, json.dumps(got))
finally:
    stop(core)
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
