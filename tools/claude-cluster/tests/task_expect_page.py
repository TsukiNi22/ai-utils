#!/usr/bin/env python3
# expect reads a file into the result of a task: from the page, the gate of :r ([web] shell). A device with the shell off gets refused (task_add, schedule_add and schedule_edit with
# action task, its options too), expect compact stays open, a schedule the user made stays editable; the shell on: the page may as with :r; the computer itself is not concerned.
# Usage: tests/task_expect_page.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""What expect reads, from the page."""
import http.cookiejar
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

T = tempfile.mkdtemp(prefix="cc-expect-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
failed = False
probe = socket.socket()
probe.bind(("127.0.0.1", 0))
PORT = probe.getsockname()[1]
probe.close()
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1))
JAR = http.cookiejar.CookieJar()
WEB = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(JAR))
SECRET = f"{T}/home/notes.txt"

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
    with WEB.open(request, timeout=20) as answer:
        return json.loads(answer.read() or b"{}")

def page(**request: object) -> dict:
    return post("/api/cmd", request)

def config(shell: bool) -> None:
    with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as out:
        out.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                  f'[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\nshell = {"true" if shell else "false"}\n[commands]\nclaude = "{FAKE}"\n')

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(SECRET, "w", encoding="utf-8") as f:
    f.write("THE-CONTENT-OF-A-FILE-OF-THE-USER\n")
config(False)
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
    time.sleep(2)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True, timeout=30).stdout or "{}").get("pin", "").replace(" ", "")
    check("a device of the page", post("/api/login", {"pin": pin, "name": "phone"}).get("ok") is not False, pin)

    # ---- the shell from the page off
    answer = page(cmd="send", session=sid, text=f":r {SECRET}")
    check(":r refused (what this gate follows)", answer.get("ok") is False, json.dumps(answer)[:200])
    answer = page(cmd="task_add", prompt="x", target=sid, expect=SECRET)
    check("task_add with a file: refused", answer.get("ok") is False and "shell" in answer.get("error", ""), json.dumps(answer)[:300])
    answer = page(cmd="task_add", prompt="x", target=sid, expect="~/notes.txt")
    check("~/ too", answer.get("ok") is False, json.dumps(answer)[:300])
    answer = page(cmd="schedule_add", when="+1s", action="task", text="x", target=sid, expect=SECRET)
    check("schedule_add, action task: refused", answer.get("ok") is False and "shell" in answer.get("error", ""), json.dumps(answer)[:300])
    answer = page(cmd="schedule_add", when="+1s", action="task", text="x", options={"target": sid, "expect": SECRET})
    check("in its options: refused", answer.get("ok") is False, json.dumps(answer)[:300])
    time.sleep(3)
    results = ask({"cmd": "task_list"}).get("data", [])
    check("nothing of it read", "THE-CONTENT" not in json.dumps(results), json.dumps(results)[:300])
    answer = page(cmd="task_add", prompt="y", target=sid, expect="compact")
    check("expect compact stays open", answer.get("ok") is True, json.dumps(answer)[:300])
    compact = (answer.get("data") or {}).get("id", "")
    for _ in range(40):
        if next((t for t in ask({"cmd": "task_list"}).get("data", []) if t.get("id") == compact), {}).get("status") == "done":
            break
        time.sleep(0.5)
    # a schedule of the user, a file in its options: a device may still act on it
    made = ask({"cmd": "schedule_add", "when": "+1h", "action": "task", "text": "z", "options": {"target": sid, "expect": SECRET}}).get("data") or {}
    check("the computer itself makes it", bool(made.get("id")), json.dumps(made)[:200])
    answer = page(cmd="schedule_edit", id=made.get("id"), text="edited")
    check("the page edits another of its fields", answer.get("ok") is True, json.dumps(answer)[:300])
    answer = page(cmd="schedule_edit", id=made.get("id"), expect=SECRET)
    check("not by bringing a file itself", answer.get("ok") is False, json.dumps(answer)[:300])

    # ---- the computer: as before
    mine = (ask({"cmd": "task_add", "prompt": "x", "target": sid, "expect": SECRET}).get("data") or {}).get("id", "")
    one = {}
    for _ in range(40):
        one = next((t for t in ask({"cmd": "task_list"}).get("data", []) if t.get("id") == mine), {})
        if one.get("status") == "done":
            break
        time.sleep(0.5)
    check("from the computer: the file is its result", "THE-CONTENT" in one.get("result", ""), json.dumps(one)[:300])
    hidden = (ask({"cmd": "task_add", "prompt": "x", "target": sid, "expect": f"{T}/home/.hidden"}).get("data") or {}).get("id", "")
    with open(f"{T}/home/.hidden", "w", encoding="utf-8") as f:
        f.write("HIDDEN-CONTENT\n")
    for _ in range(40):
        one = next((t for t in ask({"cmd": "task_list"}).get("data", []) if t.get("id") == hidden), {})
        if one.get("status") == "done":
            break
        time.sleep(0.5)
    check("a hidden file: never", "HIDDEN-CONTENT" not in one.get("result", ""), json.dumps(one)[:300])

    # ---- the shell from the page on: the page may, as with :r
    config(True)
    time.sleep(3)
    answer = page(cmd="task_add", prompt="x", target=sid, expect=SECRET)
    check("the shell on: taken", answer.get("ok") is True, json.dumps(answer)[:300])
    allowed = (answer.get("data") or {}).get("id", "")
    for _ in range(40):
        one = next((t for t in page(cmd="task_list").get("data", []) if t.get("id") == allowed), {})
        if one.get("status") == "done":
            break
        time.sleep(0.5)
    check("and read", "THE-CONTENT" in one.get("result", ""), json.dumps(one)[:300])
finally:
    core.terminate()
    try:
        core.wait(25)
    except subprocess.TimeoutExpired:
        core.kill()
    subprocess.run(["pkill", "-f", T], check=False)
    shutil.rmtree(T, ignore_errors=True)
sys.exit(1 if failed else 0)
