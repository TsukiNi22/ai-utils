#!/usr/bin/env python3
# The jobs of the orchestrator: a queued job that cannot start (a folder gone) is told to its owner, who ended its turn on the promise of being woken up; a job stopped by hand
# is never killed by its timeout (it counts the time it ran, not the time it stayed stopped), it goes on counting when it is resumed.
# Usage: tests/job_end_told.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The end of a job told as it is."""
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-jobend-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_cwd.py")
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

def job(jid: str) -> dict:
    return ask({"cmd": "job_status", "id": jid}).get("data") or {}

def log() -> str:
    with open(f"{T}/fake.log", encoding="utf-8") as f:
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
                 f'[orchestrator]\nprofile = "low"\nmax_jobs = 1\nreserved_ram_mb = 1000\nproc_dir = "{T}/proc"\n[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    end = time.time() + 30
    while time.time() < end:
        try:
            ask({"cmd": "hello"})
            break
        except OSError:
            time.sleep(0.2)
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
    time.sleep(1.5)

    # ---- a queued job that cannot start
    first = ask({"cmd": "job_add", "command": "sleep 3", "class": "custom"}).get("data") or {}
    check("the slot taken", first.get("state") == "running", json.dumps(first)[:300])
    queued = ask({"cmd": "job_add", "command": "echo hi", "session": sid, "callback": True, "class": "custom", "cwd": f"{T}/no/such/folder"}).get("data") or {}
    check("the next one waits", queued.get("state") == "queued", json.dumps(queued)[:300])
    told = False
    end = time.time() + 15
    while time.time() < end and not told:
        told = "[job " + queued.get("id", "?") + " failed]" in log()
        time.sleep(0.5)
    status = job(queued.get("id", ""))
    check("the slot free, it could not start: failed, said why", status.get("state") == "failed" and "not started" in status.get("message", ""), json.dumps(status)[:300])
    check("its owner is told, as promised", told, log()[-400:])
    check("what it reads says why", "not started" in log(), log()[-400:])
    check("told once", log().count("[job " + queued.get("id", "?") + " failed]") == 1, log()[-400:])

    # ---- a job that fails normally is told too (the same path)
    plain = ask({"cmd": "job_add", "command": "exit 3", "session": sid, "callback": True, "class": "custom"}).get("data") or {}
    end = time.time() + 15
    while time.time() < end and "[job " + plain.get("id", "?") + " failed]" not in log():
        time.sleep(0.5)
    check("a job failing normally: told", "[job " + plain.get("id", "?") + " failed]" in log(), log()[-400:])

    # ---- stopped by hand, its timeout does not run
    stopped = ask({"cmd": "job_add", "command": "sleep 300", "class": "custom", "timeout_minutes": 1}).get("data") or {}
    jid = stopped.get("id", "")
    end = time.time() + 15
    while time.time() < end and job(jid).get("state") != "running":
        time.sleep(0.5)
    time.sleep(2)
    check("it runs", job(jid).get("state") == "running", json.dumps(job(jid))[:300])
    ask({"cmd": "job_stop", "id": jid})
    check("stopped", job(jid).get("state") == "stopped", json.dumps(job(jid))[:300])
    time.sleep(62)
    check("past its timeout, stopped all along: never killed", job(jid).get("state") == "stopped", json.dumps(job(jid))[:300])
    ask({"cmd": "job_resume", "id": jid})
    time.sleep(3)
    check("resumed: it goes on", job(jid).get("state") == "running", json.dumps(job(jid))[:300])
    end = time.time() + 75
    while time.time() < end and job(jid).get("state") == "running":
        time.sleep(1)
    check("the rest of its timeout: killed by it", job(jid).get("state") == "killed" and "timeout" in job(jid).get("message", ""), json.dumps(job(jid))[:300])
finally:
    core.terminate()
    try:
        core.wait(25)
    except subprocess.TimeoutExpired:
        core.kill()
    subprocess.run(["pkill", "-f", T], check=False)
    shutil.rmtree(T, ignore_errors=True)
sys.exit(1 if failed else 0)
