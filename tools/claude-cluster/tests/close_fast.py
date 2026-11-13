#!/usr/bin/env python3
# A session closed leaves the lists at once, even while its agent takes its time to stop (SIGTERM ignored: killed after 3 s); the version
# the interfaces wait on only grows - a new conversation (/clear: the session made again, its own version from 1) then a close never
# bring it back to a number already seen (an interface then never drew the list again: the closed session still shown).
# Usage: tests/close_fast.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A closed session out of the lists at once; the sync version never goes back."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time

T = tempfile.mkdtemp(prefix="cc-closefast-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_STUBBORN="1")
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict) -> dict:
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

def names() -> list:
    return [s.get("name") for s in ask({"cmd": "list"}).get("data") or []]

def version(build: str) -> int:
    return (ask({"cmd": "sync", "client": "test", "v": 0, "known": {}, "build": build}).get("data") or {}).get("version", 0)

for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(50):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    build = (ask({"cmd": "hello"}).get("data") or {}).get("build", "")
    session = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "stubborn", "prompt": "hello"}).get("data") or {}).get("id", "s1")
    time.sleep(3)
    seen = [version(build)]
    ask({"cmd": "slash", "session": session, "text": "/clear"})  # a new conversation: the session made again (its agent killed after 3 s)
    time.sleep(1)
    seen.append(version(build))
    closing = threading.Thread(target=lambda: ask({"cmd": "close", "session": session}))
    start = time.time()
    closing.start()
    time.sleep(0.6)
    check("closed: out of the list at once, its agent still stopping", "stubborn" not in names() and closing.is_alive(), str(names()))
    seen.append(version(build))
    closing.join(15)
    took = time.time() - start
    check("its agent stopped after (killed: about 3 s)", not closing.is_alive() and took >= 2, f"{took:.1f} s")
    seen.append(version(build))
    check("the sync version only grows (clear, close)", all(b > a for a, b in zip(seen, seen[1:])), str(seen))
    trash = json.dumps(ask({"cmd": "trash"}).get("data"))
    check("in the trash (restorable)", "stubborn" in trash, trash[:300])
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
