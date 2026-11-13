#!/usr/bin/env python3
# Finished tasks do not pile up for ever: archived ones leave past the days of the trash and past a count, the oldest first, never one a waiting task comes after; an answer stays
# in the core only and is kept to what a chain or a group reads of it: state.json and each sync stay small.
# Usage: tests/task_archive.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The archive of the tasks."""
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-archive-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_cwd.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_REPLY="A" * 100000)
failed = False
core = None

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict, timeout: float = 60) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(timeout)
    s.connect(f"{T}/sock")
    s.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(1 << 22)
        if not chunk:
            break
        data += chunk
    s.close()
    return json.loads(data)

def start(*args: str) -> None:
    global core
    core = subprocess.Popen([BIN, "serve", "--no-global", *args], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
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

def state() -> dict:
    with open(f"{T}/data/state.json", encoding="utf-8") as f:
        return json.load(f)

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[sessions]\ntrash_days = 14\n[commands]\nclaude = "{FAKE}"\n')
try:
    start()
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
    time.sleep(2)
    # ---- a long answer
    first = (ask({"cmd": "task_add", "prompt": "a long answer", "target": sid}).get("data") or {}).get("id", "")
    second = (ask({"cmd": "task_add", "prompt": "after it", "target": sid, "after": [first]}).get("data") or {}).get("id", "")
    third = (ask({"cmd": "task_add", "prompt": "chained: {{" + first + ".result}}", "target": sid}).get("data") or {}).get("id", "")
    end = time.time() + 120
    done = {}
    while time.time() < end:
        done = {t.get("id"): t for t in ask({"cmd": "task_list"}).get("data", [])}
        if all(done.get(i, {}).get("status") == "done" for i in (first, second, third)):
            break
        time.sleep(0.5)
    check("the three tasks done", all(done.get(i, {}).get("status") == "done" for i in (first, second, third)), json.dumps(done)[:300])
    with open(f"{T}/fake.log", encoding="utf-8") as f:
        log = f.read()
    check("the answer given to the task after it", "the answer of" in log and "AAAA" in log, log[-300:])
    hello = ask({"cmd": "hello"}).get("data") or {}
    sync = ask({"cmd": "sync", "build": hello.get("build", "")})
    tasks = (sync.get("data") or {}).get("tasks", [])
    check("a sync holds its tasks", any(t.get("id") == first for t in tasks), json.dumps(sync)[:300])
    check("never its answer", all(not t.get("answer") for t in tasks), str(max((len(t.get("answer", "")) for t in tasks), default=0)))
    size = len(json.dumps(tasks))
    check("a sync is kept small", size < 60000, str(size))
    stop()
    saved = {t.get("id"): t for t in state().get("tasks", [])}
    check("the answer saved, kept to what is read of it", 0 < len(saved.get(first, {}).get("answer", "")) < 20000, str(len(saved.get(first, {}).get("answer", ""))))

    # ---- the old ones leave
    now = int(time.time())
    day = 86400
    old = [{"id": f"t{1000 + n}", "prompt": "old", "status": "done", "archived": True, "created": now - 40 * day, "ended": now - 30 * day, "result": "x"} for n in range(50)]
    old.append({"id": "t2000", "prompt": "old, a waiting task comes after it", "status": "done", "archived": True, "created": now - 40 * day, "ended": now - 30 * day, "result": "kept"})
    recent = [{"id": f"t{3000 + n}", "prompt": "recent", "status": "done", "archived": True, "created": now - 2 * day, "ended": now - day - n, "result": "x"} for n in range(1200)]
    waiting = {"id": "t4000", "prompt": "waits", "status": "queued", "after": ["t2000"], "target": "no-such-yet", "created": now, "not_before": now + 3600, "notBefore": now + 3600}
    finished = {"id": "t4001", "prompt": "just finished", "status": "done", "created": now, "ended": now, "result": "x"}
    state_now = state()
    state_now["tasks"] = old + recent + [waiting, finished]
    state_now["next_task"] = 5000
    with open(f"{T}/data/state.json", "w", encoding="utf-8") as f:
        json.dump(state_now, f)
    start("--restore")
    listed = set()
    end = time.time() + 40
    while time.time() < end:
        listed = {t.get("id") for t in ask({"cmd": "task_list", "archived": True}).get("data", [])}
        if len(listed) < 1100:
            break
        time.sleep(1)
    check("past the days of the trash: gone", not any(f"t{1000 + n}" in listed for n in range(50)), str(sorted(listed)[:10]))
    check("one a waiting task comes after: kept", "t2000" in listed, str(sorted(listed)[:10]))
    check("the waiting task itself: there", "t4000" in listed, str(sorted(listed)[:10]))
    check("a task just finished: there", "t4001" in listed, "")
    kept = [n for n in range(1200) if f"t{3000 + n}" in listed]
    check("past 1000 archived: the oldest leave", 995 <= len(kept) <= 1005, str(len(kept)))
    check("the newest stay", kept[:1] == [0], str(kept[:3]))
    check("the oldest of them gone", 1199 not in kept, str(kept[-3:]))
    time.sleep(1)
    stop()
    saved = {t.get("id") for t in state().get("tasks", [])}
    check("the state keeps what is left only", "t1000" not in saved and "t3000" in saved and "t3199" in saved and "t4199" not in saved, str(len(saved)))
finally:
    stop()
    subprocess.run(["pkill", "-f", T], check=False)
    shutil.rmtree(T, ignore_errors=True)
sys.exit(1 if failed else 0)
