#!/usr/bin/env python3
# An edited conversation (session_edit) is a transcript like the ones of Claude Code: this user only, whatever the umask, in folders of mode 0755; its writing failed
# (a full disk, a folder refused): told, the session left on the old one, never started on half a conversation.
# Usage: tests/conversation_edit_mode.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The mode of an edited conversation, and its writing failed."""
import json
import os
import socket
import stat
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-editmode-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
PROJECT = f"{T}/home/.claude/projects/-tmp-p"
UID = "00000000-0000-0000-0000-000000000001"  # the conversation of the fake agent
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
os.makedirs(PROJECT)
open(f"{T}/fake.log", "w", encoding="utf-8").close()
fd = os.open(f"{PROJECT}/{UID}.jsonl", os.O_WRONLY | os.O_CREAT, 0o600)  # as Claude Code writes it
LINES = [{"type": "user", "uuid": "a", "parentUuid": None, "cwd": f"{T}/work", "sessionId": UID, "message": {"role": "user", "content": "secret prompt"}},
         {"type": "assistant", "uuid": "b", "parentUuid": "a", "cwd": f"{T}/work", "sessionId": UID, "message": {"role": "assistant", "content": [{"type": "text", "text": "secret answer"}]}},
         {"type": "user", "uuid": "c", "parentUuid": "b", "cwd": f"{T}/work", "sessionId": UID, "message": {"role": "user", "content": "second"}},
         {"type": "assistant", "uuid": "d", "parentUuid": "c", "cwd": f"{T}/work", "sessionId": UID, "message": {"role": "assistant", "content": [{"type": "text", "text": "two"}]}}]
os.write(fd, "".join(json.dumps(x) + "\n" for x in LINES).encode())
os.close(fd)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd="/", preexec_fn=lambda: os.umask(0o022))
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
    time.sleep(1)
    ask({"cmd": "send", "session": sid, "text": "go"})
    for _ in range(40):
        time.sleep(0.25)
        if (ask({"cmd": "read", "session": sid}).get("data") or {}).get("state") == "idle":
            break
    check("its messages read", len((ask({"cmd": "messages", "session": sid}).get("data") or {}).get("messages", [])) == 4)

    # its folder refused: told, nothing started on a part of it
    os.chmod(PROJECT, 0o500)
    refused = ask({"cmd": "edit", "session": sid, "keep": 2})
    os.chmod(PROJECT, 0o755)
    check("its writing failed: told", refused.get("ok") is False and ".jsonl" in refused.get("error", ""), json.dumps(refused)[:200])
    check("nothing left of it", os.listdir(PROJECT) == [f"{UID}.jsonl"], str(os.listdir(PROJECT)))
    state = ask({"cmd": "messages", "session": sid}).get("data") or {}
    check("the session still on its conversation, whole", state.get("conversation") == UID and len(state.get("messages", [])) == 4, json.dumps(state)[:200])
    check("     not started again", (ask({"cmd": "read", "session": sid}).get("data") or {}).get("state") == "idle")

    # written: as the transcripts of Claude Code
    edited = ask({"cmd": "edit", "session": sid, "keep": 2})
    check("edited", edited.get("ok") is True, json.dumps(edited)[:200])
    new = [n for n in os.listdir(PROJECT) if n != f"{UID}.jsonl"]
    check("one new conversation, no temporary file left", len(new) == 1 and new[0].endswith(".jsonl") and ".tmp" not in new[0], str(new))
    if new:
        mode = stat.S_IMODE(os.stat(f"{PROJECT}/{new[0]}").st_mode)
        check("this user only under a umask of 022", mode == 0o600, oct(mode))
        with open(f"{PROJECT}/{new[0]}", encoding="utf-8") as out:
            lines = [json.loads(line) for line in out]
        check("whole: its kept messages, chained again, its own id",
              [x.get("uuid") for x in lines] == ["a", "b"] and lines[0].get("parentUuid") is None and lines[1].get("parentUuid") == "a" and all(x.get("sessionId") == new[0][:-6] for x in lines),
              json.dumps(lines)[:300])
    check("the old one untouched, its mode too", stat.S_IMODE(os.stat(f"{PROJECT}/{UID}.jsonl").st_mode) == 0o600 and open(f"{PROJECT}/{UID}.jsonl", encoding="utf-8").read().count("\n") == 4)
finally:
    os.chmod(PROJECT, 0o755)
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
