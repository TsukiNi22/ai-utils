#!/usr/bin/env python3
# Ctrl+K: the archived sessions and the conversations of the other agents are not listed one by one in the palette any more - two entries
# open their own lists: Archived sessions… (pick:trash: then each one: restore / delete for good) and Import a session from another agent…
# (pick:importer:, then the conversations of one agent); a session closed then restored through them.
# Usage: tests/palette_handlers.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The archive and import handlers of the palette."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-handlers-")
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


for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
# a conversation of Claude Code on this "computer" (its own transcript folder)
project = f"{T}/home/.claude/projects/-work"
os.makedirs(project)
with open(f"{project}/0e6c1c9e-1111-4222-8333-944455556666.jsonl", "w", encoding="utf-8") as transcript:
    transcript.write(json.dumps({"type": "user", "cwd": f"{T}/work", "sessionId": "0e6c1c9e-1111-4222-8333-944455556666",
                                 "message": {"role": "user", "content": "outside hello"}}) + "\n")
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
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "gone"}).get("data") or {}).get("id", "s1")
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "kept"})
    time.sleep(1)
    ask({"cmd": "action", "id": "close_session", "session": sid})
    time.sleep(0.5)
    ids = [a.get("id", "") for a in ask({"cmd": "actions", "session": "g"}).get("data") or []]
    check("no archived session one by one in the palette", not any(i.startswith(("restore:", "purge:")) for i in ids), str([i for i in ids if ":" in i][:8]))
    check("no conversation of another agent one by one", not any(i.startswith("import:") for i in ids), str([i for i in ids if i.startswith("import")][:5]))
    check("the two entries: Archived sessions…, Import a session…", "trash_view" in ids and "importer" in ids, str(ids[:12]))
    got = ask({"cmd": "action", "id": "trash_view", "session": "g"}).get("data") or {}
    check("Archived sessions…: its list", got.get("ui") == "pick:trash:", json.dumps(got))
    got = ask({"cmd": "action", "id": f"trash_item:{sid}", "session": "g"}).get("data") or {}
    check("... one of them: its choices (restore / delete for good)", got.get("ui") == f"pick:trash_item:{sid}", json.dumps(got))
    got = ask({"cmd": "action", "id": f"restore:{sid}", "session": "g"}).get("data") or {}
    listed = [s.get("id") for s in ask({"cmd": "list"}).get("data") or []]
    check("... restored", sid in listed, json.dumps(got)[:200] + str(listed))
    got = ask({"cmd": "action", "id": "importer", "session": "g"}).get("data") or {}
    check("Import a session…: the agents", got.get("ui") == "pick:importer:", json.dumps(got))
    got = ask({"cmd": "action", "id": "importer_agent:claude", "session": "g"}).get("data") or {}
    check("... Claude Code: its conversations", got.get("ui") == "pick:importer_agent:claude", json.dumps(got))
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
