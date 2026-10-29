#!/usr/bin/env python3
# Schedule a prompt by its steps (the palette's "Schedule a prompt…", /schedule): to whom (the global session first: the default, this
# one, another, a new one in a folder), when (once in / at, every N, every day / weekday / week at, a cron), then the prompt; a bad time
# refused at its step (before the prompt is typed); the schedules made as asked.
# Usage: tests/schedule_prompt.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""Scheduled prompts made through the steps of the palette."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-sched-")
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


def act(action: str, session: str = "g", text: str = "") -> dict:
    return ask({"cmd": "action", "id": action, "session": session, "input": text})


def schedule(sid: str) -> dict:
    data = ask({"cmd": "schedule_list"}).get("data")
    items = data.get("schedules", []) if isinstance(data, dict) else data or []
    return next((w for w in items if w.get("id") == sid), {})


for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = true\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    for _ in range(60): # the global session up (the default target)
        if any(s.get("id") == "g" for s in ask({"cmd": "list"}).get("data") or []):
            break
        time.sleep(0.5)
    sid =(ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "alpha"}).get("data") or {}).get("id", "s1")
    titles = [a.get("title", "") for a in ask({"cmd": "actions", "session": sid}).get("data") or []]
    check("the palette offers Schedule a prompt…", any(t.startswith("Schedule a prompt") for t in titles), str(titles[:5]))
    got = (act("sched_new", sid).get("data") or {})
    check("1. to whom: its choices (this session known)", got.get("ui") == f"pick:sched_to:{sid}", json.dumps(got))
    got = ask({"cmd": "slash", "session": sid, "text": "/schedule"}).get("data") or {}
    check("/schedule: the same steps", got.get("ui") == f"pick:sched_to:{sid}", json.dumps(got))
    got = act("sched_when:g", sid).get("data") or {}
    check("2. when (the global session)", got.get("ui") == "pick:sched_how:g", json.dumps(got))
    got = act("sched_text:g|daily", sid, "25:00")
    check("a bad time refused at its step", got.get("ok") is False or "HH:MM" in json.dumps(got), json.dumps(got)[:300])
    got = act("sched_text:g|daily", sid, "09:00").get("data") or {}
    check("3. the time checked, then the prompt", got.get("ui") == "pick:sched_make:g|daily|09:00", json.dumps(got))
    got = act("sched_make:g|daily|09:00", sid, "the morning review").get("data") or {}
    made = got.get("message", "")
    check("4. made: every day at 09:00 to the global session", made.startswith("schedule w"), made)
    w = schedule(made.split()[1]) if made.startswith("schedule w") else {}
    check("... its rule (cron 0 9 * * *), a prompt to g", w.get("cron") == "0 9 * * *" and w.get("action") == "prompt" and w.get("session") == "g"
          and w.get("text") == "the morning review", json.dumps(w)[:300])
    got = act(f"sched_make:{sid}|weekdays|18:30", sid, "the end of the day").get("data") or {}
    w = schedule(got.get("message", "").split()[1]) if got.get("message", "").startswith("schedule w") else {}
    check("this session, every weekday at 18:30", w.get("cron") == "30 18 * * 1-5" and w.get("session") == sid, json.dumps(got)[:200] + json.dumps(w)[:200])
    got = act("sched_make:g|weekly|fri 17:00", sid, "the week").get("data") or {}
    w = schedule(got.get("message", "").split()[1]) if got.get("message", "").startswith("schedule w") else {}
    check("every week on friday at 17:00", w.get("cron") == "0 17 * * 5", json.dumps(w)[:200])
    got = act("sched_make:g|in|2h", sid, "later").get("data") or {}
    w = schedule(got.get("message", "").split()[1]) if got.get("message", "").startswith("schedule w") else {}
    check("once in 2 h", 110 <= (w.get("in_minutes") or 0) <= 120 and not w.get("every_minutes"), json.dumps(w)[:200])
    got = act("sched_when:new", sid, f"{T}/work").get("data") or {}
    check("a new session: its folder checked, then when", got.get("ui") == f"pick:sched_how:new={T}/work", json.dumps(got))
    got = act(f"sched_make:new={T}/work|every|2h", sid, "a fresh look").get("data") or {}
    w = schedule(got.get("message", "").split()[1]) if got.get("message", "").startswith("schedule w") else {}
    check("... every 2 h, a new session started each time", w.get("action") == "spawn" and w.get("every_minutes") == 120
          and (w.get("options") or {}).get("cwd") == f"{T}/work", json.dumps(w)[:300])
    got = act("sched_make:g|daily|09:00", sid, "   ")
    check("an empty prompt refused", "empty" in json.dumps(got), json.dumps(got)[:200])
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
