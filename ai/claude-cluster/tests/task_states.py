#!/usr/bin/env python3
# cancelled != finished != stopped: a task whose turn is interrupted is stopped (not done, not failed), the task after it never runs as if it
# finished, and session_wait tells how the last turn ended (cancelled here, finished for a normal end).
# Usage: tests/core_supervisor.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The end of a task said as it is."""
import json
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-taskstate-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_DELAY="30")
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


def task(tid):
    return next((t for t in (ask({"cmd": "task_list"}).get("data") or []) if t.get("id") == tid), {})


for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    core_pid()
    worker = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "s1")
    time.sleep(1.5)
    first = (ask({"cmd": "task_add", "prompt": "mute first job", "target": "worker"}).get("data") or {}).get("id")
    second = (ask({"cmd": "task_add", "prompt": "after the first", "target": "worker", "after": [first]}).get("data") or {}).get("id")
    for _ in range(20):
        if task(first).get("status") == "running":
            break
        time.sleep(0.5)
    check("the first task runs", task(first).get("status") == "running", json.dumps(task(first))[:200])
    ask({"cmd": "interrupt", "session": worker})
    for _ in range(20):
        if task(first).get("status") not in ("running", "queued"):
            break
        time.sleep(0.5)
    one = task(first)
    check("its turn interrupted: stopped (not done, not failed)", one.get("status") == "stopped", json.dumps({k: one.get(k) for k in ("status", "reason")}))
    time.sleep(2)
    two = task(second)
    check("the task after it never runs as if it finished", two.get("status") == "failed" and "stopped" in two.get("reason", ""),
          json.dumps({k: two.get(k) for k in ("status", "reason")}))
    waited = ask({"cmd": "wait", "session": worker, "timeout": 5}).get("data") or {}
    check("session_wait: its outcome cancelled, not finished", waited.get("outcome") == "cancelled" and waited.get("finished") is False,
          json.dumps({k: waited.get(k) for k in ("outcome", "finished", "done")}))
finally:
    core.terminate() # the watcher stops its core (a SIGKILL would leave the core running)
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
