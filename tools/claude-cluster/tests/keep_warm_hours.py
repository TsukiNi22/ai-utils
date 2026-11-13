#!/usr/bin/env python3
# keep_warm_hours: a session marked, its last real prompt older than it: not kept warm any more and not shown so (its ♨ gone, told once), never a mark that seems on;
# /keepwarm counts as a use: on again at once, woken up before its cache expires (it was marked again and stopped at the next window: its ♨ stayed on, its cache
# expired). Its core restarted with an old state (cache life 1 min, margin 16 s here).
# Usage: tests/keep_warm_hours.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""keep_warm_hours: the mark, the ♨, a use by /keepwarm."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-warmhours-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_wake.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_NO_SUPERVISOR="1")
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
        return log.read().count("keep-warm")

def listed(sid: str) -> dict:
    return next((s for s in ask({"cmd": "list"}).get("data") or [] if s.get("id") == sid), {})

def serve() -> subprocess.Popen:
    process = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(1)
    return process

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[sessions]\nkeep_warm = "chosen"\nkeep_warm_ttl = 1\nkeep_warm_margin = 16\nkeep_warm_hours = 2\n[commands]\nclaude = "{FAKE}"\n')
core = serve()
try:
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "warm", "prompt": "hello"}).get("data") or {}).get("id", "s1")
    time.sleep(2)
    ask({"cmd": "keep_warm", "session": sid, "value": "on"})
    check("marked: kept warm (♨)", listed(sid).get("keep_warm") is True, json.dumps(listed(sid))[:200])
    time.sleep(1.5)
    core.terminate()
    core.wait(20)
    # its last real prompt three hours ago, its cache still alive: in its state and in its log
    three = 3 * 3600
    with open(f"{T}/data/state.json", encoding="utf-8") as saved:
        state = json.load(saved)
    for spec in state["sessions"]:
        if spec["id"] == sid:
            spec["used_at"] = int(time.time()) - three
            spec["cached_at"] = int(time.time()) - 5
    with open(f"{T}/data/state.json", "w", encoding="utf-8") as saved:
        json.dump(state, saved)
    with open(f"{T}/data/logs/{sid}.jsonl", encoding="utf-8") as log:
        lines = [json.loads(line) for line in log]
    with open(f"{T}/data/logs/{sid}.jsonl", "w", encoding="utf-8") as log:
        for line in lines:
            line["time"] -= three
            log.write(json.dumps(line) + "\n")
    if os.path.exists(f"{T}/sock"):
        os.remove(f"{T}/sock")
    core = serve()
    deadline = time.time() + 20
    while time.time() < deadline and listed(sid).get("state") != "idle":
        time.sleep(0.5)
    check("past keep_warm_hours: no more ♨", listed(sid).get("keep_warm") is False, json.dumps(listed(sid))[:300])
    deadline = time.time() + 25
    told = []
    while time.time() < deadline and not told:
        told = [n.get("text", "") for n in ask({"cmd": "notices"}).get("data") or [] if "keep warm: stopped" in n.get("text", "")]
        time.sleep(1)
    check("told once why", len(told) == 1, json.dumps(told)[:300])
    check("not woken up", wakes() == 0, str(wakes()))
    done = ask({"cmd": "keep_warm", "session": sid, "value": "toggle"})
    check("/keepwarm (toggled): on again, not off", listed(sid).get("keep_warm") is True and "kept warm" in json.dumps(done), json.dumps(done)[:300])
    deadline = time.time() + 70
    while time.time() < deadline and wakes() == 0:
        time.sleep(1)
    check("... and woken up before its cache expires (a use, not a mark that stops at once)", wakes() >= 1, str(wakes()))
    check("the core alive at the end", core.poll() is None)
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
