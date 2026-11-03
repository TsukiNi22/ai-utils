#!/usr/bin/env python3
# The events a queued task waits for: a command every "constantly" is looked at every 2 s as promised, not 5; git_push on a branch not pushed yet starts at its first push (no
# branch at the first look is not "not looked yet"), a remote not reached tells nothing; a cycle in the after of session_fanout is refused, never a group that never ends.
# Usage: tests/task_events.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The events of the tasks, a group waiting for itself."""
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-events-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", GIT_CONFIG_GLOBAL="/dev/null", GIT_CONFIG_SYSTEM="/dev/null",
           GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t.t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t.t")
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

def task(tid: str) -> dict:
    return next((t for t in (ask({"cmd": "task_list"}).get("data") or []) if t.get("id") == tid), {})

def git(*args: str) -> None:
    subprocess.run(["git", *args], env=ENV, check=True, capture_output=True, timeout=60)

for folder in ("cfg", "data", "run", "cache", "home", "work", "work2"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
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
    other = (ask({"cmd": "spawn", "cwd": f"{T}/work2", "name": "second"}).get("data") or {}).get("id", "")
    time.sleep(2)

    # ---- a cycle in the after of a group
    before = len(ask({"cmd": "task_list"}).get("data", []))
    answer = ask({"cmd": "fanout", "origin": "mcp", "label": "cycle", "tasks": [{"session": sid, "prompt": "A", "after": [1]}, {"session": other, "prompt": "B", "after": [0]}]})
    check("two tasks waiting for each other: refused", answer.get("ok") is False and "cycle" in answer.get("error", ""), json.dumps(answer)[:300])
    answer = ask({"cmd": "fanout", "origin": "mcp", "label": "cycle3", "tasks": [{"session": sid, "prompt": "A", "after": [2]}, {"session": other, "prompt": "B", "after": [0]},
                                                                                 {"session": sid, "prompt": "C", "after": [1]}]})
    check("three in a ring: refused", answer.get("ok") is False and "cycle" in answer.get("error", ""), json.dumps(answer)[:300])
    check("no task left of them", len(ask({"cmd": "task_list"}).get("data", [])) == before, json.dumps(ask({"cmd": "task_list"}))[:300])
    answer = ask({"cmd": "fanout", "origin": "mcp", "label": "chain", "tasks": [{"session": sid, "prompt": "CHAIN-ONE"}, {"session": other, "prompt": "CHAIN-TWO", "after": [0]},
                                                                                {"session": sid, "prompt": "CHAIN-THREE", "after": [0, 1, 1]}]})
    check("a chain (a dependency given twice): taken", answer.get("ok") is True, json.dumps(answer)[:300])
    group = (answer.get("data") or {}).get("group", "")
    status = {}
    end = time.time() + 40
    while time.time() < end:
        status = ask({"cmd": "group_status", "group": group}).get("data") or {}
        if status.get("finished"):
            break
        time.sleep(0.5)
    check("it ends", status.get("finished") is True, json.dumps(status)[:300])

    # ---- every: constantly, promised 2 s
    ticks = f"{T}/ticks"
    constant = (ask({"cmd": "task_add", "prompt": "x", "target": sid, "on_event": {"type": "command", "command": f"date +%s >> {ticks}; exit 1", "every": "constantly", "code": 0}}).get("data") or {}).get("id", "")
    time.sleep(16)
    ask({"cmd": "task_act", "id": constant, "action": "cancel"})
    with open(ticks, encoding="utf-8") as f:
        seen = [int(x) for x in f.read().split()]
    gaps = [b - a for a, b in zip(seen, seen[1:])]
    check("every: constantly is looked at every 2 s", len(seen) >= 6 and sum(gaps) / max(1, len(gaps)) <= 3.0, f"{len(seen)} looks, gaps {gaps}")

    # ---- git_push on a branch not pushed yet
    os.makedirs(f"{T}/remote.git")
    git("-C", f"{T}/remote.git", "init", "--bare", "-q", "-b", "main")
    git("clone", "-q", f"{T}/remote.git", f"{T}/repo")
    with open(f"{T}/repo/file", "w", encoding="utf-8") as f:
        f.write("one\n")
    git("-C", f"{T}/repo", "add", "file")
    git("-C", f"{T}/repo", "commit", "-q", "-m", "one")
    git("-C", f"{T}/repo", "push", "-q", "origin", "HEAD:refs/heads/main")
    git("-C", f"{T}/repo", "checkout", "-q", "-b", "new-branch")
    new = (ask({"cmd": "task_add", "prompt": "AFTER-THE-PUSH", "target": sid, "on_event": {"type": "git_push", "repo": f"{T}/repo", "branch": "new-branch"}}).get("data") or {}).get("id", "")
    # the branch there all along, not pushed again: no event
    still = (ask({"cmd": "task_add", "prompt": "NOT-A-PUSH", "target": sid, "on_event": {"type": "git_push", "repo": f"{T}/repo", "branch": "main"}}).get("data") or {}).get("id", "")
    # a remote not reached at the first look tells nothing: its branch, there all along, is not a push when the remote is back
    os.makedirs(f"{T}/remote2.git")
    git("-C", f"{T}/remote2.git", "init", "--bare", "-q", "-b", "main")
    git("clone", "-q", f"{T}/remote2.git", f"{T}/repo2")
    with open(f"{T}/repo2/file", "w", encoding="utf-8") as f:
        f.write("one\n")
    git("-C", f"{T}/repo2", "add", "file")
    git("-C", f"{T}/repo2", "commit", "-q", "-m", "one")
    git("-C", f"{T}/repo2", "push", "-q", "origin", "HEAD:refs/heads/main")
    os.rename(f"{T}/remote2.git", f"{T}/remote2.away")
    flaky = (ask({"cmd": "task_add", "prompt": "NOT-A-PUSH-EITHER", "target": sid, "on_event": {"type": "git_push", "repo": f"{T}/repo2", "branch": "main"}}).get("data") or {}).get("id", "")
    time.sleep(5)
    os.rename(f"{T}/remote2.away", f"{T}/remote2.git")
    check("no branch yet: it waits", task(new).get("status") == "queued" and not task(new).get("on_event", {}).get("met"), json.dumps(task(new))[:300])
    git("-C", f"{T}/repo", "push", "-q", "origin", "new-branch")
    # looked at every 60 s: the look after the first one starts it, never a second push
    end = time.time() + 80
    while time.time() < end and task(new).get("status") != "done":
        time.sleep(1)
    check("its first push starts it", task(new).get("status") == "done", json.dumps(task(new))[:300])
    time.sleep(3)
    check("the branch there all along, not pushed again: still waiting", task(still).get("status") == "queued", json.dumps(task(still))[:300])
    check("the remote back, its branch there all along: still waiting", task(flaky).get("status") == "queued", json.dumps(task(flaky))[:300])
finally:
    core.terminate()
    try:
        core.wait(25)
    except subprocess.TimeoutExpired:
        core.kill()
    subprocess.run(["pkill", "-f", T], check=False)
    shutil.rmtree(T, ignore_errors=True)
sys.exit(1 if failed else 0)
