#!/usr/bin/env python3
# Several /entries of the same name (a command of claude-cluster, the agent's own one, a skill): each one listed by the completion, its
# id hidden after its name (tag characters, never shown); the one picked decides - the agent's own one or the skill goes to the agent,
# the command of claude-cluster runs here, a plain /name stays the command of claude-cluster; the hidden id never reaches the agent.
# Usage: tests/slash_ids.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The hidden ids of the /entries of the same name."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-slashids-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_SKILLS="export", FAKE_CLAUDE_COMMANDS="export,context,compact")
COMMAND = 3 # cluster::CompletionKind::Command
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

def hidden(text: str) -> bool:
    return any(0xE0000 <= ord(c) <= 0xE007F for c in text)

def log() -> str:
    with open(f"{T}/fake.log", encoding="utf-8") as f:
        return f.read()

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
    session = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "ids"}).get("data") or {}).get("id", "s1")
    time.sleep(3) # its agent up: its skills and commands known
    items = ask({"cmd": "complete", "session": session, "kind": COMMAND, "query": ""}).get("data") or []
    by = {}
    for item in items:
        by.setdefault(item["label"], []).append(item)
    context, export, compact = by.get("context", []), by.get("export", []), by.get("compact", [])
    check("both /context listed (claude-cluster, the agent)", sorted(i.get("source") for i in context) == ["built-in", "local"], json.dumps(context))
    check("both /export listed (claude-cluster, the skill)", len(export) == 2, json.dumps(export))
    check("a name given once: no hidden id", len(compact) == 1 and not hidden(compact[0]["value"]), json.dumps(compact))
    check("the same name: each value its hidden id, the labels plain", all(hidden(i["value"]) and not hidden(i["label"]) for i in context + export)
          and len({i["value"] for i in context + export}) == 4, json.dumps(context + export))
    agent = next((i["value"] for i in context if i.get("source") == "built-in"), "/context")
    ours = next((i["value"] for i in context if i.get("source") == "local"), "/context")
    skill = next((i["value"] for i in export if "Markdown" not in i.get("description", "")), "export") # claude-cluster's: "conversation as Markdown"

    check("the agent's /context picked: not run here", ask({"cmd": "slash", "session": session, "text": "/" + agent + " now"}).get("data", {}).get("local") is False)
    check("claude-cluster's /context picked: run here", ask({"cmd": "slash", "session": session, "text": "/" + ours}).get("data", {}).get("local") is True)
    check("a plain /context: claude-cluster's (as before)", ask({"cmd": "slash", "session": session, "text": "/context"}).get("data", {}).get("local") is True)
    check("the skill /export picked: not run here", ask({"cmd": "slash", "session": session, "text": "/" + skill + " the notes"}).get("data", {}).get("local") is False)

    ask({"cmd": "send", "session": session, "text": "/" + agent + " now"})
    time.sleep(2)
    ask({"cmd": "send", "session": session, "text": "/" + skill + " the notes"})
    time.sleep(2)
    got = log()
    check("the agent got /context, without the hidden id", 'PROMPT "/context now"' in got, got[-600:])
    check("the agent got the skill /export, without the hidden id", 'PROMPT "/export the notes"' in got, got[-600:])
    check("no tag character ever reaches the agent", not hidden(got))
    transcript = json.dumps(ask({"cmd": "transcript", "session": session}).get("data"), ensure_ascii=False)
    check("the transcript keeps the plain text", "/context now" in transcript and not hidden(transcript), transcript[-400:])
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
