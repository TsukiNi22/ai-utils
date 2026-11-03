#!/usr/bin/env python3
# A cycle of parentUuid in a transcript of Claude Code (a file of another tool, not trusted): each line of its chain read once, whatever the number of lines
# around it (a small file was made into gigabytes of the same text: the memory of the core gone). An import, a context, its messages and an edit on it.
# Usage: tests/transcript_cycle.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A cycle of parents in a transcript: its text once, never a copy per step of the walk."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-cycle-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
PROJECT = f"{T}/home/.claude/projects/-tmp-p"
IMPORTED = "11111111-1111-4111-8111-111111111111"
RUNNING = "00000000-0000-0000-0000-000000000001"  # the conversation of the fake agent
TEXT = "THE-ANSWER-" + "A" * 5000  # 5 KB: 100 000 times in the old walk, 500 MB at most if this ever comes back
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(120)
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

def cycle(path: str, filler: int) -> None:
    # the last line starts the walk: a -> b -> a; lines that are in no chain before it (the walk bounded by the number of lines would still go round that many times)
    with open(path, "w", encoding="utf-8") as out:
        for i in range(filler):
            out.write(json.dumps({"type": "attachment", "uuid": f"x{i}", "parentUuid": None}) + "\n")
        out.write(json.dumps({"type": "user", "uuid": "a", "parentUuid": "b", "cwd": f"{T}/work", "sessionId": "x", "message": {"role": "user", "content": "the question"}}) + "\n")
        out.write(json.dumps({"type": "assistant", "uuid": "b", "parentUuid": "a", "cwd": f"{T}/work", "sessionId": "x", "message": {"role": "assistant", "content": TEXT}}) + "\n")

def idle(sid: str) -> None:
    for _ in range(40):
        if (ask({"cmd": "read", "session": sid}).get("data") or {}).get("state") == "idle":
            return
        time.sleep(0.25)

for folder in ("cfg", "data", "run", "cache", "home", "work", "home/.claude/projects/-tmp-p"):
    os.makedirs(f"{T}/{folder}", exist_ok=True)
open(f"{T}/fake.log", "w", encoding="utf-8").close()
cycle(f"{PROJECT}/{IMPORTED}.jsonl", 2000)
cycle(f"{PROJECT}/{RUNNING}.jsonl", 2000)
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
    listed = ask({"cmd": "external", "agent": "claude"})
    check("listed", listed.get("ok") is True and any(c.get("id") == IMPORTED for c in listed.get("data") or []), json.dumps(listed)[:200])

    # an import: its transcript shown from the walk
    started = time.time()
    made = ask({"cmd": "import", "agent": "claude", "id": IMPORTED[:8], "name": "imported"})
    sid = (made.get("data") or {}).get("id", "")
    check("imported", made.get("ok") is True and sid != "", json.dumps(made)[:200])
    entries = (ask({"cmd": "transcript", "session": sid, "from": 0, "count": 500}).get("data") or {}).get("entries", [])
    shown = sum(1 for e in entries if e.get("text", "").startswith("THE-ANSWER-"))
    check("its text once, not once per step of the walk", shown == 1, f"{shown} times in {len(entries)} entries")
    check("in seconds", time.time() - started < 60, f"{time.time() - started:.0f} s")

    # the same as a context given to a session: what the agent reads
    worker = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
    time.sleep(1)
    given = ask({"cmd": "context", "session": worker, "source": f"claude:{IMPORTED[:8]}"})
    check("given as a context", given.get("ok") is True, json.dumps(given)[:200])
    idle(worker)
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        prompts = [line for line in log if line.startswith("PROMPT") and "[context given" in line]
    told = sum(line.count("THE-ANSWER-") for line in prompts)
    check("the agent reads it once", told == 1, f"{told} times")

    # a session on such a conversation: its messages, an edit
    ask({"cmd": "send", "session": worker, "text": "go"})
    time.sleep(1)
    idle(worker)
    messages = ask({"cmd": "messages", "session": worker})
    listed = (messages.get("data") or {}).get("messages", [])
    check("messages: each line once", messages.get("ok") is True and len(listed) == 2, f"{len(listed)}: {json.dumps(messages)[:200]}")
    edited = ask({"cmd": "edit", "session": worker, "replace": [{"index": 0, "text": "another question"}]})
    check("edited", edited.get("ok") is True, json.dumps(edited)[:200])
    names = [n for n in os.listdir(PROJECT) if n not in (f"{IMPORTED}.jsonl", f"{RUNNING}.jsonl")]
    check("one new conversation", len(names) == 1, str(names))
    with open(f"{PROJECT}/{names[0]}", encoding="utf-8") as new:
        lines = [json.loads(line) for line in new]
    check("written with each line once, chained again", [x.get("uuid") for x in lines] == ["a", "b"] and lines[0].get("parentUuid") is None and lines[1].get("parentUuid") == "a",
          json.dumps([[x.get("uuid"), x.get("parentUuid")] for x in lines])[:200])
    check("the core alive", core.poll() is None and ask({"cmd": "list"}).get("ok") is True)
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
