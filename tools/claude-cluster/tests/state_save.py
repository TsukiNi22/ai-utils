#!/usr/bin/env python3
# Many saves at once (every request of every thread saves the state): state.json always a whole JSON, never the mix of two writers in one temporary file (at the next
# start: every session gone), this user only, no temporary file left. A session edited with keep: a line of its conversation that is no message (an attachment) read
# as one was a read past the end of a map in the core.
# Usage: tests/state_save.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The state saved under concurrent requests; session_edit on a chain with other lines."""
import glob
import json
import os
import socket
import stat
import subprocess
import sys
import tempfile
import threading
import time

T = tempfile.mkdtemp(prefix="cc-save-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_wake.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
SID = "00000000-0000-0000-0000-000000000001"
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

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "saved", "prompt": "hello"}).get("data") or {}).get("id", "s1")
    time.sleep(2)
    # many labels: a state big enough to be written in several steps
    ask({"cmd": "describe", "session": sid, "text": "x" * 200000})
    stop = threading.Event()
    bad = []

    def saves(n: int) -> None:
        count = 0
        while not stop.is_set():
            count += 1
            try:
                ask({"cmd": "rename", "session": sid, "name": f"saved-{n}-{count}"})
                ask({"cmd": "ui_set", "key": f"k{n}", "value": str(count)})
            except (OSError, ValueError) as e:
                bad.append(str(e))
                return

    def reads() -> None:
        while not stop.is_set():
            try:
                with open(f"{T}/data/state.json", encoding="utf-8") as saved:
                    json.load(saved)
            except ValueError as e:
                bad.append("state.json not a JSON: " + str(e))
            except OSError:
                pass

    threads = [threading.Thread(target=saves, args=(i,)) for i in range(8)] + [threading.Thread(target=reads)]
    for thread in threads:
        thread.start()
    time.sleep(10)
    stop.set()
    for thread in threads:
        thread.join()
    check("state.json always whole, under 8 clients saving at once", not bad, "; ".join(bad[:3]))
    time.sleep(0.5)
    check("no temporary file left", not glob.glob(f"{T}/data/state.json.tmp*"), str(glob.glob(f"{T}/data/state.json.tmp*")))
    check("this user only", stat.S_IMODE(os.stat(f"{T}/data/state.json").st_mode) == 0o600, oct(stat.S_IMODE(os.stat(f"{T}/data/state.json").st_mode)))

    # session_edit with keep: its chain holds a line that is no message
    folder = f"{T}/home/.claude/projects/-work"
    os.makedirs(folder)
    lines = [
        {"uuid": "a", "parentUuid": None, "type": "user", "sessionId": SID, "message": {"role": "user", "content": "first"}},
        {"uuid": "b", "parentUuid": "a", "type": "assistant", "sessionId": SID, "message": {"role": "assistant", "content": [{"type": "text", "text": "one"}]}},
        {"uuid": "c", "parentUuid": "b", "type": "attachment", "sessionId": SID, "attachment": {"type": "x"}},
        {"uuid": "d", "parentUuid": "c", "type": "user", "sessionId": SID, "message": {"role": "user", "content": "second"}},
        {"uuid": "e", "parentUuid": "d", "type": "assistant", "sessionId": SID, "message": {"role": "assistant", "content": [{"type": "text", "text": "two"}]}},
        {"uuid": "f", "parentUuid": "e", "type": "attachment", "sessionId": SID, "attachment": {"type": "y"}},
    ]
    with open(f"{folder}/{SID}.jsonl", "w", encoding="utf-8") as transcript:
        for line in lines:
            transcript.write(json.dumps(line) + "\n")
    done = ask({"cmd": "edit", "session": sid, "keep": 2})
    check("session_edit keep: done", done.get("ok") is True and "change" in json.dumps(done), json.dumps(done)[:300])
    time.sleep(1)
    check("the core alive at the end", core.poll() is None and ask({"cmd": "list"}).get("ok") is True)
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
