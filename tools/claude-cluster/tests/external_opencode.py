#!/usr/bin/env python3
# The list of the conversations of the agents against the output of `opencode session list` (a program of another tool, a fake one in the PATH): a field of another kind in one
# session failed the list of every agent. Its sessions kept with what they say, the ones without an id left out, the conversations of the others listed.
# Usage: tests/external_opencode.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A session of opencode of another shape, against the list of every agent."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-opencode-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
           HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", PATH=f"{T}/bin:{os.environ['PATH']}")
SESSIONS = [{"id": "ses_null", "title": None, "directory": "/tmp", "updated": 5000},
            {"id": "ses_kinds", "title": 7, "directory": ["x"], "updated": "soon"},
            {"id": "ses_float", "title": "a float", "directory": "/tmp", "updated": 1e308},
            {"id": 12, "title": "an id of another kind", "directory": "/tmp", "updated": 1},
            {"title": "no id at all", "directory": "/tmp", "updated": 1},
            "a session that is no object", 3, None, [],
            {"id": "ses_good", "title": "a good one", "directory": "/tmp", "updated": 9000000}]
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(40)
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

def program(output: str) -> None:
    with open(f"{T}/session.json", "w", encoding="utf-8") as out:
        out.write(output)

for folder in ("cfg", "data", "run", "cache", "home", "work", "bin"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/bin/opencode", "w", encoding="utf-8") as fake:
    fake.write(f'#!/bin/sh\ncat "{T}/session.json"\n')
os.chmod(f"{T}/bin/opencode", 0o755)
program(json.dumps(SESSIONS))
os.makedirs(f"{T}/home/.claude/projects/-tmp-p")
with open(f"{T}/home/.claude/projects/-tmp-p/33333333-3333-4333-8333-333333333333.jsonl", "w", encoding="utf-8") as transcript:
    transcript.write(json.dumps({"type": "user", "uuid": "a", "cwd": f"{T}/work", "message": {"role": "user", "content": "a conversation of claude"}}) + "\n")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd="/")
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    every = ask({"cmd": "external"})
    check("the list of every agent answers", every.get("ok") is True, json.dumps(every)[:300])
    ids = {c.get("id"): c for c in every.get("data") or []}
    check("the conversation of claude in it", "33333333-3333-4333-8333-333333333333" in ids, str(list(ids)))
    check("the sessions of opencode in it", all(name in ids for name in ("ses_null", "ses_kinds", "ses_float", "ses_good")), str(list(ids)))
    check("a null title: left empty", ids.get("ses_null", {}).get("title", "x") == "", json.dumps(ids.get("ses_null"))[:200])
    check("other kinds: left empty", ids.get("ses_kinds", {}).get("title", "x") == "" and ids.get("ses_kinds", {}).get("cwd", "x") == "" and ids.get("ses_kinds", {}).get("updated", 1) == 0,
          json.dumps(ids.get("ses_kinds"))[:200])
    check("a good one as it says", ids.get("ses_good", {}).get("title") == "a good one" and ids.get("ses_good", {}).get("cwd") == "/tmp" and ids.get("ses_good", {}).get("updated") == 9000,
          json.dumps(ids.get("ses_good"))[:200])
    check("no id: left out", 12 not in ids and "" not in ids, str(list(ids)))
    check("the first prompt of the others kept", ids.get("33333333-3333-4333-8333-333333333333", {}).get("first_prompt") == "a conversation of claude")
    only = ask({"cmd": "external", "agent": "opencode"})
    check("opencode alone", only.get("ok") is True and {c.get("id") for c in only.get("data", [])} == {"ses_null", "ses_kinds", "ses_float", "ses_good"}, json.dumps(only)[:300])

    # no list at all, not JSON
    for name, output in (("an object", '{"id":"x"}'), ("not JSON", "[{"), ("nothing", "")):
        program(output)
        answer = ask({"cmd": "external"})
        check(f"{name}: the others still listed", answer.get("ok") is True and any(c.get("agent") == "claude" for c in answer.get("data", [])), json.dumps(answer)[:200])
    check("the core alive", core.poll() is None and ask({"cmd": "list"}).get("ok") is True)
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
