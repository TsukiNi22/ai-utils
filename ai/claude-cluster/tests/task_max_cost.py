#!/usr/bin/env python3
# A cost of no number (inf, nan: written null in state.json, which the next start could not read): refused from the page, a sub-session and task_act set; the core starts again.
# Usage: tests/task_max_cost.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""max_cost: a finite number only, so that the state always reads back."""
import http.cookiejar
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

T = tempfile.mkdtemp(prefix="cc-maxcost-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 36000 + os.getpid() % 1500
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1))
failed = False
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))

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

def post(path: str, body: dict) -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
    with web.open(request, timeout=30) as answer:
        return json.loads(answer.read() or b"{}")

def start(*args: str) -> subprocess.Popen:
    process = subprocess.Popen([BIN, "serve", "--no-global", *args], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=T)
    for _ in range(100):
        if os.path.exists(f"{T}/sock") and process.poll() is None:
            break
        time.sleep(0.1)
    time.sleep(2)
    return process

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\n')
core = start()
try:
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post("/api/login", {"pin": pin, "name": "phone"})
    sid = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"})["data"]["id"]
    time.sleep(1)
    later = 9999999999  # never started: it stays in the state
    for value in ("inf", "-inf", "nan", "Infinity", "1e999"):
        answer = post("/api/cmd", {"cmd": "task_add", "prompt": "x", "target": "nobody/here", "max_cost": value, "not_before": later})
        check(f"max_cost {value} refused from the page", answer.get("ok") is False and "max_cost" in answer.get("error", ""), json.dumps(answer)[:200])
    answer = ask({"cmd": "task_add", "origin": "sub", "owner": sid, "tool": "task_add", "prompt": "x", "max_cost": "inf", "not_before": later})
    check("max_cost inf refused from a sub-session", answer.get("ok") is False and "max_cost" in answer.get("error", ""), json.dumps(answer)[:200])
    made = post("/api/cmd", {"cmd": "task_add", "prompt": "kept", "target": "nobody/here", "max_cost": "0.5", "not_before": later})
    check("a real cost still taken (a text)", made.get("ok") is True, json.dumps(made)[:200])
    tid = made.get("data", {}).get("id")
    answer = post("/api/cmd", {"cmd": "task_act", "id": tid, "action": "set", "max_cost": "inf"})
    check("task_act set max_cost inf refused", answer.get("ok") is False and "max_cost" in answer.get("error", ""), json.dumps(answer)[:200])
    answer = post("/api/cmd", {"cmd": "task_act", "id": tid, "action": "set", "max_cost": 1.5})
    check("task_act set a real cost still taken (a number)", answer.get("ok") is True, json.dumps(answer)[:200])
    task = next((t for t in ask({"cmd": "task_list"})["data"] if t["id"] == tid), {})
    check("its cost kept", task.get("max_cost") == 1.5, json.dumps(task)[:200])
    core.terminate()
    core.wait(25)
    with open(f"{T}/data/state.json", encoding="utf-8") as state:
        saved = json.load(state)
    check("no cost without a value in state.json", all(t.get("maxCost") is not None for t in saved.get("tasks", [])), json.dumps(saved.get("tasks"))[:300])
    core = start("--restore")
    check("the core starts again", core.poll() is None)
    listed = ask({"cmd": "task_list"})
    check("its task read back", listed.get("ok") is True and any(t["id"] == tid and t.get("max_cost") == 1.5 for t in listed.get("data", [])), json.dumps(listed)[:300])
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
