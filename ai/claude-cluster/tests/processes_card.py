#!/usr/bin/env python3
# Settings > Services > Processes and version: the versions (this interface, the core), its uptime, the changes of the last versions, the
# memory of the core and of each session with what runs under it, the tokens of this launch; the shells of a session stopped from it.
# Usage: tests/subagents.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The Processes card of the Services tab."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-procs-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_agents.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_OUT=T, FAKE_SHELL_SECONDS="30")
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


def agents(session):
    return {a.get("task_id") or a.get("taskId"): a for a in (ask({"cmd": "agents", "session": session}).get("data") or [])}


for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(50):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.2)
    spawned = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker", "prompt": "review with two agents"})
    session = (spawned.get("data") or {}).get("id", "s1")
    time.sleep(4)
    services = ask({"cmd": "services"}).get("data") or {}
    part = services.get("processes", {})
    card = (services.get("cards") or {}).get("processes", {})
    text = "\n".join(line.get("text", "") for line in card.get("lines", []))
    check("its version and the core", part.get("version") and f"the core: {part.get('version')}" in text, text[:300])
    check("its summary: uptime, memory, tokens", " up " in card.get("summary", "") and "MB in all" in card.get("summary", "") and "tokens this launch" in card.get("summary", ""),
          card.get("summary", ""))
    check("the memory of the core counted", part.get("core_kb", 0) > 0 and part.get("total_kb", 0) >= part.get("core_kb", 0), json.dumps({k: part.get(k) for k in ("core_kb", "total_kb")}))
    row = next((s for s in part.get("sessions", []) if s.get("id") == session), {})
    check("the session: its agent and what runs under it", row.get("pid", 0) > 0 and row.get("rss_kb", 0) > 0, json.dumps(row)[:300])
    check("its shell running counted", row.get("shells") == 1, json.dumps(row)[:300])
    buttons = {b.get("action"): b.get("label") for b in card.get("buttons", [])}
    check("a button to stop its shells", f"shells:{session}" in buttons, json.dumps(buttons))
    done = ask({"cmd": "service_act", "service": "processes", "action": f"shells:{session}"})
    check("its shells stopped", "1 shell(s) stopped" in json.dumps(done), json.dumps(done)[:200])
    time.sleep(1.5)
    shell = next((a for a in ask({"cmd": "agents", "session": session}).get("data") or [] if (a.get("task_id") or a.get("taskId")) == "task-sh"), {})
    check("the shell stopped (the session goes on)", shell.get("status") == "stopped", json.dumps({k: shell.get(k) for k in ("status", "stopping", "taskId")}))
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
