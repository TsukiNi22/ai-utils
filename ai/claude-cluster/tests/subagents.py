#!/usr/bin/env python3
# Sub-agents of a session (its Agent tool): listed by the core from the task events of claude and their own messages (model,
# activity, tokens), never mixed in the conversation of the session, its shells run in the background beside them (their output),
# one stopped alone (stop_task), and the session woken when a task ends while it is idle (the callback, like Claude Code).
# Usage: tests/subagents.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The agents a session makes: listed, their activity kept apart, one stopped."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-agents-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_agents.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_OUT=T)
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
    listed = {}
    for _ in range(50):
        listed = agents(session)
        if "task-fg" in listed and listed["task-fg"].get("status") == "done":
            break
        time.sleep(0.2)
    check("two agents and a shell listed", set(listed) == {"task-fg", "task-bg", "task-sh"}, json.dumps(list(listed)))
    check("the shell: its kind, in the background", listed.get("task-sh", {}).get("kind") == "shell" and listed.get("task-sh", {}).get("background") is True,
          json.dumps(listed.get("task-sh"))[:200])
    fg, bg = listed.get("task-fg", {}), listed.get("task-bg", {})
    check("the reviewer: done, its type, its model, its tokens", fg.get("status") == "done" and fg.get("type") == "code-reviewer"
          and fg.get("model") == "claude-haiku-5-5" and fg.get("tokens") == 2000, json.dumps({k: fg.get(k) for k in ("status", "type", "model", "tokens")}))
    check("the reviewer: its activity (its words, its Read, its result) and its summary", any("SUBAGENT-SECRET-TEXT" in e.get("text", "") for e in fg.get("entries", []))
          and any(e.get("tool") == "Read" for e in fg.get("entries", [])) and fg.get("summary") == "no issue found", json.dumps(fg.get("entries"))[:400])
    check("the watcher: still running in the background, its own model", bg.get("status") == "running" and bg.get("background") is True and bg.get("model") == "claude-opus-5-5",
          json.dumps({k: bg.get(k) for k in ("status", "background", "model")}))
    check("its start and its end known (uptime)", fg.get("started", 0) > 0 and fg.get("ended", 0) >= fg.get("started", 0) and bg.get("ended", 0) == 0)
    read = ask({"cmd": "read", "session": session, "limit": 50})
    check("the words of an agent never in the conversation of the session", "SUBAGENT-SECRET-TEXT" not in json.dumps(read) and "the watcher goes on" in json.dumps(read),
          json.dumps(read)[:300])
    every = ask({"cmd": "agents"}).get("data") or []
    check("every session listed at once (without the activity)", len(every) == 3 and all("entries" not in a for a in every))
    woken = ""
    for _ in range(40):
        with open(f"{T}/fake.log", encoding="utf-8") as log:
            woken = log.read()
        if "task-notification" in woken:
            break
        time.sleep(0.2)
    check("the shell ended while idle: the session woken with its notification", "<task-notification>" in woken and "task-sh" in woken and "task-sh.output" in woken,
          woken[-400:])
    shell = agents(session).get("task-sh", {})
    check("the shell: done, the end of its output kept", shell.get("status") == "done" and "SHELL-OUTPUT-END" in shell.get("output", ""), json.dumps(shell)[:300])
    time.sleep(1)
    check("its answer in the conversation", "the tests of the shell passed" in json.dumps(ask({"cmd": "read", "session": session, "limit": 50})))
    stop = ask({"cmd": "agent_stop", "session": session, "task": "task-bg"})
    check("the watcher stopped (stop_task sent)", stop.get("ok") is not False, json.dumps(stop))
    for _ in range(30):
        if agents(session).get("task-bg", {}).get("status") == "stopped":
            break
        time.sleep(0.2)
    after = agents(session).get("task-bg", {})
    check("its status: stopped, its end known", after.get("status") == "stopped" and after.get("ended", 0) > 0, json.dumps(after)[:200])
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        check("claude got stop_task with its task id", '"subtype": "stop_task", "task_id": "task-bg"' in log.read())
    again = ask({"cmd": "agent_stop", "session": session, "task": "task-fg"})
    check("an ended agent: not stopped again (told why)", again.get("ok") is False and "ended" in again.get("error", ""), json.dumps(again))
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
