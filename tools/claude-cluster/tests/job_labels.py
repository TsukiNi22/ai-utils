#!/usr/bin/env python3
# The label of a background command: a migration is dangerous (an update never kills and runs it again), a build or a test stays safe. Its command never runs: no such folder.
# Usage: tests/job_labels.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""cluster::command_label through job_add."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-labels-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", CLAUDE_CLUSTER_DISCOVERY_PORT=str(44000 + os.getpid() % 1500))
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

for folder in ("cfg", "data", "run", "cache", "home", "proc/pressure"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/proc/loadavg", "w", encoding="utf-8") as file:
    file.write("0.10 0.00 0.00 1/100 1\n")
with open(f"{T}/proc/meminfo", "w", encoding="utf-8") as file:
    file.write("MemTotal: 16000000 kB\nMemFree: 8000000 kB\nMemAvailable: 12000000 kB\nSwapTotal: 2000000 kB\nSwapFree: 2000000 kB\n")
with open(f"{T}/proc/cpuinfo", "w", encoding="utf-8") as file:
    file.write("".join(f"processor\t: {i}\nmodel name\t: Fake CPU\n\n" for i in range(4)))
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n[commands]\nclaude = "true"\n'
                 f'[orchestrator]\nprofile = "low"\nmax_jobs = 2\nreserved_ram_mb = 1000\nproc_dir = "{T}/proc"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=T)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(1.5)

    def label(command: str) -> str:
        answer = ask({"cmd": "job_add", "command": command, "class": "custom", "name": "x", "prio": -10, "cwd": f"{T}/no/such"})
        return answer.get("data", {}).get("label", json.dumps(answer)[:160])

    for command in ("python manage.py migrate", "python manage.py migrate --fake", "rails db:migrate", "alembic upgrade head", "npx prisma migrate deploy", "npx knex migrate:latest", "npm run db-migrate up",
                    "./run_migrations.py", "flyway migrate", "git push origin main"):
        got = label(command)
        check(f"dangerous: {command}", got == "dangerous", got)
    for command in ("make -j4", "cargo build", "pytest tests/", "npm run build", "grep -r TODO src"):
        got = label(command)
        check(f"safe: {command}", got == "safe", got)
    check("the core alive at the end", core.poll() is None)
finally:
    core.terminate()
    try:
        core.wait(15)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
