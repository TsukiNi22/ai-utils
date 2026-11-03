#!/usr/bin/env python3
# Two requests at once must never replace a thread still joinable (std::terminate: the core and its sessions gone): `usage` (each one asks for a scan of the transcripts, its
# thread short with few transcripts) and schedule_act run (its pass over the schedules, on the client thread, against the tick). Both are races: hammered for some seconds.
# Usage: tests/thread_start_race.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""Threads of the core started by several requests at once."""
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time

T = tempfile.mkdtemp(prefix="cc-threads-")
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

def ask(request: dict, timeout: float = 20) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(timeout)
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

def core_pid() -> int:
    """The process of the core now (its watcher starts it again after a crash: another pid), 0 when it does not answer."""
    try:
        return (ask({"cmd": "hello"}, 15).get("data") or {}).get("pid", 0)
    except (OSError, ValueError):
        return 0

def crash_said() -> str:
    with open(f"{T}/core.err", encoding="utf-8", errors="replace") as f:
        text = f.read()
    return text[-300:] if "terminate" in text or "crashed" in text else ""

def hammer(request: dict, seconds: float, workers: int = 12) -> int:
    """The same request from several connections; how many were answered."""
    stop = threading.Event()
    answered = [0]

    def one() -> None:
        while not stop.is_set():
            try:
                ask(request, 10)
                answered[0] += 1
            except (OSError, ValueError):
                time.sleep(0.02)

    threads = [threading.Thread(target=one) for _ in range(workers)]
    for t in threads:
        t.start()
    end = time.time() + seconds
    while time.time() < end and not crash_said():
        time.sleep(0.2)
    stop.set()
    for t in threads:
        t.join()
    return answered[0]

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=open(f"{T}/core.err", "w", encoding="utf-8"))
try:
    end = time.time() + 30
    while time.time() < end:
        try:
            ask({"cmd": "hello"})
            break
        except OSError:
            time.sleep(0.2)
    pid = core_pid()
    done = hammer({"cmd": "usage", "vendor": "anthropic"}, 8)
    check("usage from 12 connections: the same core is still there", core_pid() == pid and not crash_said(), crash_said())
    check("they were answered", done > 0, str(done))
    end = time.time() + 20
    while time.time() < end and core_pid() == 0:
        time.sleep(0.5)
    pid = core_pid()
    wid = (ask({"cmd": "schedule_add", "when": "+1h", "every": 60, "action": "notify", "text": "x"}).get("data") or {}).get("id", "")
    check("a schedule to run by hand", bool(wid), wid)
    done = hammer({"cmd": "schedule_act", "id": wid, "action": "run"}, 8)
    check("schedule_act run from 12 connections: the same core is still there", core_pid() == pid and not crash_said(), crash_said())
    check("they were answered", done > 0, str(done))
    time.sleep(2)
    runs = next((w for w in ask({"cmd": "schedule_list"}).get("data", []) if w.get("id") == wid), {})
    check("it did run", runs.get("runs", 0) > 0, json.dumps(runs)[:300])
    check("its next time kept (a run by hand)", runs.get("status") == "waiting", json.dumps(runs)[:300])
finally:
    if core.poll() is None:
        core.terminate()
        try:
            core.wait(25)
        except subprocess.TimeoutExpired:
            core.kill()
    subprocess.run(["pkill", "-f", T], check=False)
    shutil.rmtree(T, ignore_errors=True)
sys.exit(1 if failed else 0)
