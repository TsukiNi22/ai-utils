#!/usr/bin/env python3
# cluster-run waiting in the queue, the core restarted: it asks again for its place, it never runs directly because it waited long (30 s counted from the loss of the core, not
# from its own start: at any restart every job waiting started at once, what the queue exists to prevent); the core gone for good: after 30 s, the command runs directly.
# Usage: tests/cluster_run_requeue.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster; cluster-run next to it)
"""A queued cluster-run across a restart of the core."""
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-requeue-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
RUN = os.path.join(os.path.dirname(BIN), "cluster-run")
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
failed = False
core = None

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

def start() -> None:
    global core
    core = subprocess.Popen([BIN, "serve", "--no-global", "--restore"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    end = time.time() + 30
    while time.time() < end:
        try:
            ask({"cmd": "hello"})
            return
        except OSError:
            time.sleep(0.2)

def stop() -> None:
    global core
    if core is None:
        return
    core.terminate()
    try:
        core.wait(25)
    except subprocess.TimeoutExpired:
        core.kill()
        core.wait()
    core = None

def wrapper(name: str, command: str) -> subprocess.Popen:
    return subprocess.Popen([RUN, "--name", name, "--class", "custom", "--", "sh", "-c", command], env=ENV, stdout=subprocess.DEVNULL, stderr=open(f"{T}/{name}.err", "w", encoding="utf-8"),
                            start_new_session=True)

def said(name: str) -> str:
    with open(f"{T}/{name}.err", encoding="utf-8", errors="replace") as f:
        return f.read()

for folder in ("cfg", "data", "run", "cache", "home", "work", "proc", "proc/pressure"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
# a computer of its own: never the load of this one that holds the queue
with open(f"{T}/proc/loadavg", "w", encoding="utf-8") as f:
    f.write("0.10 0.00 0.00 1/100 1\n")
with open(f"{T}/proc/meminfo", "w", encoding="utf-8") as f:
    f.write("MemTotal: 16000000 kB\nMemFree: 8000000 kB\nMemAvailable: 12000000 kB\nSwapTotal: 2000000 kB\nSwapFree: 2000000 kB\n")
with open(f"{T}/proc/cpuinfo", "w", encoding="utf-8") as f:
    f.write("".join(f"processor\t: {i}\nmodel name\t: Fake CPU\n\n" for i in range(4)))
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[orchestrator]\nenabled = true\nmax_jobs = 1\nreserved_ram_mb = 1000\nproc_dir = "{T}/proc"\n[commands]\nclaude = "{FAKE}"\n')
first = None
second = None
try:
    start()
    first = wrapper("first", "sleep 200")
    time.sleep(2)
    second = wrapper("second", f"date +%s > {T}/second.ran; sleep 300")
    time.sleep(4)
    check("the second one waits in the queue", "queued" in said("second") and not os.path.exists(f"{T}/second.ran"), said("second")[:300])
    time.sleep(30) # longer than the 30 s of the loss, counted from its start
    check("still waiting after more than 30 s", not os.path.exists(f"{T}/second.ran"), said("second")[:300])
    stop()
    time.sleep(3)
    check("the core gone: it tries again", "trying again" in said("second"), said("second")[:300])
    check("it does not run directly because it waited long", "runs directly" not in said("second") and not os.path.exists(f"{T}/second.ran"), said("second")[:300])
    start()
    end = time.time() + 20
    while time.time() < end and not os.path.exists(f"{T}/second.ran"):
        time.sleep(0.5)
    jobs = (ask({"cmd": "job_list", "state": "all"}).get("data") or {}).get("jobs", [])
    mine = [j for j in jobs if j.get("name") == "second" and j.get("state") in ("running", "queued")]
    check("the core back: it has its place in the queue again", bool(mine), json.dumps(jobs)[:400])
    check("given by the core, never directly", "runs directly" not in said("second"), said("second")[:300])
    # the core gone for good: 30 s, then directly (it has its turn there, it was waiting for nothing)
    third = wrapper("third", f"date +%s > {T}/third.ran; sleep 1")
    time.sleep(3)
    gone = time.time()
    stop()
    end = time.time() + 45
    while time.time() < end and not os.path.exists(f"{T}/third.ran"):
        time.sleep(0.5)
    waited = time.time() - gone
    check("never back: the command runs directly", os.path.exists(f"{T}/third.ran") and "not back" in said("third"), said("third")[:300])
    check("after the 30 s of the loss", waited >= 28, f"{waited:.1f} s")
    third.wait(20)
finally:
    stop()
    for one in (first, second):
        if one is not None:
            try:
                os.killpg(one.pid, signal.SIGTERM)
            except OSError:
                pass
    subprocess.run(["pkill", "-f", T], check=False)
    shutil.rmtree(T, ignore_errors=True)
sys.exit(1 if failed else 0)
