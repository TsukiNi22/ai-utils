#!/usr/bin/env python3
# A local command in a task or a schedule, whatever its shape: from a device with [web] shell off (a command that is no text: its JSON went to /bin/sh; action / type in
# another case; nested in options) and from the MCP of a sub-session (the same, and what its schedules wake: never another session, never the global session, whatever it leaves out).
# Usage: tests/command_shapes.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The gates of the page and of a sub-session on what a task or a schedule will run."""
import http.cookiejar
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

T = tempfile.mkdtemp(prefix="cc-shapes-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 34000 + os.getpid() % 1500
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1))
failed = False
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))

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

def post(path: str, body: dict) -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
    with web.open(request, timeout=30) as answer:
        return json.loads(answer.read() or b"{}")

def device(**body: object) -> dict:
    return post("/api/cmd", body)

def marks() -> list:
    return [name for name in sorted(os.listdir(T)) if name.startswith("mark-")]

for folder in ("cfg", "data", "run", "cache", "home", "work", "work2"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n[agent]\nsub_schedules = true\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=T)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post("/api/login", {"pin": pin, "name": "phone"})
    sid = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"})["data"]["id"]
    other = ask({"cmd": "spawn", "cwd": f"{T}/work2", "name": "victim"})["data"]["id"]
    time.sleep(1)

    # --- a device, the shell off: every shape refused, nothing runs
    run = f"$(touch {T}/mark-device)"
    shapes = {
        "if.command an object": {"cmd": "schedule_add", "when": "+1s", "text": "x", "session": sid, "if": {"command": {"a": run}}},
        "if.command a list": {"cmd": "schedule_add", "when": "+1s", "text": "x", "session": sid, "if": {"command": [run]}},
        "if.command a number": {"cmd": "schedule_add", "when": "+1s", "text": "x", "session": sid, "if": {"command": 1}},
        "action Command, a list": {"cmd": "schedule_add", "when": "+1s", "action": "Command", "command": [run]},
        "action COMMAND, a text": {"cmd": "schedule_add", "when": "+1s", "action": "COMMAND", "session": sid, "command": f"touch {T}/mark-device"},
        "a task on_event Command, a list": {"cmd": "task_add", "prompt": "x", "target": sid, "on_event": {"type": "Command", "command": [run], "every": 2}},
        "a task on_event COMMAND, a text": {"cmd": "task_add", "prompt": "x", "target": sid, "on_event": {"type": "COMMAND", "command": f"touch {T}/mark-device", "every": 2}},
        "a schedule of a task, on_event in its options": {"cmd": "schedule_add", "when": "+1s", "action": "task", "text": "x", "options": {"on_event": {"type": "command", "command": [run], "every": 2}}},
        "schedule_edit: if.command a list": {"cmd": "schedule_edit", "id": "w1", "if": {"command": [run]}},
        "schedule_edit: action Command": {"cmd": "schedule_edit", "id": "w1", "action": "Command", "command": [run]},
    }
    for name, request in shapes.items():
        answer = device(**request)
        check(f"device: {name} refused", answer.get("ok") is False and "shell" in answer.get("error", ""), json.dumps(answer)[:200])
    fine = device(cmd="schedule_add", when="+1h", text="a reminder", session=sid)
    check("device: a schedule without a command still made", fine.get("ok") is True, json.dumps(fine)[:200])
    fine = device(cmd="task_add", prompt="after the file", target=sid, on_event={"type": "file", "path": f"{T}/never"})
    check("device: a task on a file still made", fine.get("ok") is True, json.dumps(fine)[:200])

    # --- whoever asks: a command that is no text never runs (its JSON would); a text still does
    for name, request in {"the user": {}, "the global session": {"origin": "mcp", "tool": "schedule_add"}}.items():
        answer = ask(dict(request, cmd="schedule_add", when="+1s", action="command", command=[f"touch {T}/mark-json"]))
        check(f"{name}: a command that is no text refused", answer.get("ok") is False and "command" in answer.get("error", ""), json.dumps(answer)[:200])
    answer = ask({"cmd": "schedule_add", "when": "+1s", "action": "command", "session": sid, "command": f"touch {T}/mark-text"})
    check("the user: a schedule running a command still made", answer.get("ok") is True, json.dumps(answer)[:200])

    # --- a sub-session (as its MCP sends them): a local command in any shape
    def sub(tool: str, **body: object) -> dict:
        return ask(dict(body, cmd=tool, origin="sub", owner=sid, tool=tool))

    subs = {
        "task_add on_event Command": ("task_add", {"prompt": "x", "on_event": {"type": "Command", "command": f"touch {T}/mark-sub", "every": 2}}),
        "task_add on_event command, a list": ("task_add", {"prompt": "x", "on_event": {"type": "command", "command": [f"touch {T}/mark-sub"], "every": 2}}),
        "schedule_add action Command, itself": ("schedule_add", {"when": "+1s", "action": "Command", "session": sid, "command": f"touch {T}/mark-sub"}),
        "schedule_add action task, options.on_event": ("schedule_add", {"when": "+1s", "action": "task", "text": "y", "options": {"on_event": {"type": "command", "command": f"touch {T}/mark-sub", "every": 2}}}),
        "schedule_add if.command": ("schedule_add", {"when": "+1s", "text": "y", "if": {"command": f"touch {T}/mark-sub"}}),
    }
    for name, (tool, body) in subs.items():
        answer = sub(tool, **body)
        check(f"sub-session: {name} refused", answer.get("ok") is False and "not allowed" in answer.get("error", ""), json.dumps(answer)[:200])

    # --- a sub-session: what its schedules wake is itself
    answer = sub("schedule_add", when="+1h", action="task", text="TASK-OTHER", options={"target": other})
    check("sub-session: a task in another session (options.target) refused", answer.get("ok") is False, json.dumps(answer)[:200])
    answer = sub("schedule_add", when="+1h", action="task", text="TASK-OTHER", target=other)
    check("sub-session: a task in another session (target) refused", answer.get("ok") is False, json.dumps(answer)[:200])
    answer = sub("schedule_add", when="+1h", text="x", session=other)
    check("sub-session: a prompt in another session refused", answer.get("ok") is False, json.dumps(answer)[:200])

    def schedule(wid: str) -> dict:
        return next(w for w in ask({"cmd": "schedule_list"})["data"] if w["id"] == wid)

    w = sub("schedule_add", when="+1h", action="Prompt", text="TO-GLOBAL")["data"]["id"]
    check("sub-session: action Prompt without a session wakes itself", schedule(w).get("session") == sid, json.dumps(schedule(w))[:200])
    answer = sub("schedule_edit", id=w, session="", when="+2h")
    check("sub-session: schedule_edit session empty stays on itself", answer.get("ok") is True and schedule(w).get("session") == sid, json.dumps(answer)[:200])
    sub("schedule_edit", id=w, action="notify")
    sub("schedule_edit", id=w, action="prompt")
    check("sub-session: notify then prompt: back on itself", schedule(w).get("session") == sid, json.dumps(schedule(w))[:200])
    w = sub("schedule_add", when="+1h", action="task", text="TASK-ANY")["data"]["id"]
    check("sub-session: a schedule of a task without a target: for itself", schedule(w).get("options", {}).get("target") == sid, json.dumps(schedule(w))[:200])
    sub("schedule_edit", id=w, options={})
    check("sub-session: its options emptied: still for itself", schedule(w).get("options", {}).get("target") == sid, json.dumps(schedule(w))[:200])
    w = sub("schedule_add", when="+1h", text="back to me")["data"]["id"]
    sub("schedule_edit", id=w, action="TASK")
    check("sub-session: edited into a task: for itself", schedule(w).get("options", {}).get("target") == sid, json.dumps(schedule(w))[:200])
    answer = sub("task_add", prompt="SPAWN-EMPTY", spawn={})
    task = next((t for t in ask({"cmd": "task_list"})["data"] if t["id"] == answer.get("data", {}).get("id")), {})
    check("sub-session: spawn without a folder: a task for itself, not for any idle session", task.get("target") == sid, json.dumps(answer)[:200])
    w = sub("schedule_add", when="+1s", text="WAKE-MINE")
    check("sub-session: its own wake-up still made", w.get("ok") is True, json.dumps(w)[:200])

    # --- nothing of the refused ones ran; what was allowed did
    deadline = time.time() + 15
    while time.time() < deadline and f"mark-text" not in marks():
        time.sleep(0.5)
    time.sleep(6)
    check("nothing refused ran", marks() == ["mark-text"], str(marks()))
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        prompts = log.read()
    check("its own wake-up arrived", "WAKE-MINE" in prompts)
    check("no prompt of a sub-session in another session", "TASK-OTHER" not in prompts and "TO-GLOBAL" not in prompts)
    check("the core alive at the end", core.poll() is None)
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
