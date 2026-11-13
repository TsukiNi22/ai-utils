#!/usr/bin/env python3
# The prompts waiting in a queue survive a quit: a long turn runs, two prompts are queued behind it, the core stops (as at a quit), it is
# started again: the queue is in its saved state, then sent to the session once it runs, in its order, once.
# Usage: tests/queue_persist.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The queues kept over a quit of the core."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-qpersist-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_wake.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", HOME=f"{T}/home")
failed = False


def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def ask(request: dict) -> dict:
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


def serve(*extra: str) -> subprocess.Popen:
    core = subprocess.Popen([BIN, "serve", "--no-global", *extra], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(80):
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


def log() -> str:
    with open(f"{T}/fake.log", encoding="utf-8") as f:
        return f.read()


for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = serve()
try:
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "q"}).get("data") or {}).get("id", "s1")
    time.sleep(3)
    ask({"cmd": "send", "session": sid, "text": "a slow job"})
    time.sleep(1.5)
    ask({"cmd": "send", "session": sid, "text": "queued one"})
    ask({"cmd": "send", "session": sid, "text": "queued two"})
    time.sleep(1.5)
    stop(core)
    with open(f"{T}/data/state.json", encoding="utf-8") as f:
        saved = json.load(f).get("queued", {}).get(sid, [])
    check("the queue saved at the quit", [q.get("text") for q in saved] == ["queued one", "queued two"], json.dumps(saved))
    check("not sent before the quit", "queued one" not in log(), log()[-300:])
    core = serve("--restore")
    deadline = time.time() + 30
    while time.time() < deadline and "queued two" not in log():
        time.sleep(1)
    got = log()
    check("started again: the queue sent to its session", "queued one" in got and "queued two" in got, got[-400:])
    check("in its order", 0 <= got.find("queued one") < got.find("queued two"), got[-400:])
    time.sleep(3)
    check("each prompt once", log().count("queued one") == 1 and log().count("queued two") == 1, log()[-400:])
finally:
    stop(core)
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
