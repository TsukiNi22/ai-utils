#!/usr/bin/env python3
# The smooth update and the background of the sessions: each background agent / shell is labelled at its start (safe | dangerous, a
# tag in the lists); while an update waits, a young safe one (< 1 min) is stopped and run again after it (its session told by the
# new version, held.json), a dangerous one is never stopped (the update waits for it).
# Usage: tests/update_background.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""Young safe background items stopped and run again after an update, dangerous ones waited for."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_agents.py")
failed = False


def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def ask(t: str, request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(10)
    s.connect(f"{t}/sock")
    s.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        data += chunk
    s.close()
    return json.loads(data)


def run(description: str) -> tuple:
    # a session whose turn leaves an agent in the background described so; the update held: what was stopped, what was held
    t = tempfile.mkdtemp(prefix="cc-upbg-")
    for folder in ("cfg", "data", "run", "cache", "work", "out"):
        os.makedirs(f"{t}/{folder}")
    open(f"{t}/fake.log", "w", encoding="utf-8").close()
    with open(f"{t}/cfg/config.toml", "w", encoding="utf-8") as config:
        config.write('[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                     f'[commands]\nclaude = "{FAKE}"\n')
    env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
    env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{t}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{t}/data", CLAUDE_CLUSTER_SOCKET=f"{t}/sock", XDG_RUNTIME_DIR=f"{t}/run",
               XDG_CACHE_HOME=f"{t}/cache", FAKE_CLAUDE_LOG=f"{t}/fake.log", FAKE_CLAUDE_OUT=f"{t}/out", FAKE_SHELL_SECONDS="30", FAKE_BG_DESC=description,
               CLAUDE_CLUSTER_TEST="1")
    core = subprocess.Popen([BIN, "serve", "--no-global"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            if os.path.exists(f"{t}/sock"):
                break
            time.sleep(0.1)
        time.sleep(0.5)
        sid = (ask(t, {"cmd": "spawn", "cwd": f"{t}/work", "name": "bg", "prompt": "go"}).get("data") or {}).get("id", "s1")
        agents = []
        deadline = time.time() + 20
        while time.time() < deadline and not any(a.get("taskId") == "task-bg" for a in agents):
            agents = ask(t, {"cmd": "agents", "session": sid}).get("data") or []
            time.sleep(0.5)
        label = next((a.get("label") for a in agents if a.get("taskId") == "task-bg"), None)
        held = ask(t, {"cmd": "upgrade", "action": "hold"})
        time.sleep(1.5)
        with open(f"{t}/fake.log", encoding="utf-8") as f:
            log = f.read()
        kept = json.load(open(f"{t}/data/held.json", encoding="utf-8")) if os.path.exists(f"{t}/data/held.json") else []
        return label, held.get("data"), log, kept
    finally:
        core.terminate()
        try:
            core.wait(20)
        except subprocess.TimeoutExpired:
            core.kill()
        os.system(f"pkill -f {t} > /dev/null 2>&1")


label, done, log, kept = run("Watch the tests")
check("a background agent labelled at its start: safe", label == "safe", str(label))
check("the update: the young safe agent stopped", '"task_id": "task-bg"' in log and "stop_task" in log, log[-400:])
check("told to run it again after the update (held for the new version)",
      any("stopped your background agent" in k.get("text", "") and "Watch the tests" in k.get("text", "") for k in kept), json.dumps(kept)[:400])
check("its count given", "restarted" in str(done) and not str(done).endswith(" 0 restarted"), str(done))
label, done, log, kept = run("Deploy the site to production")
check("a dangerous one labelled so", label == "dangerous", str(label))
check("a dangerous one never stopped by the update", '"task_id": "task-bg"' not in log, log[-400:])
check("nothing held for it", not any("Deploy the site" in k.get("text", "") for k in kept), json.dumps(kept)[:300])
check("the young safe shell of the same session stopped all the same", any("background shell" in k.get("text", "") for k in kept), json.dumps(kept)[:300])
sys.exit(1 if failed else 0)
