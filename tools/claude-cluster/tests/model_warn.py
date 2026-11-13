#!/usr/bin/env python3
# A model change on a context still loaded (its prompt cache alive): asked first, Cancel the default, the size of the context told
# (the cache belongs to a model: the new one reads it all again); Cancel changes nothing; the change anyway restarts it on the new
# model; the page (apply) gets a confirm and changes it with confirmed; a cache expired: changed at once (nothing more to lose).
# Usage: tests/model_warn.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A model change asked first while the context is loaded."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-modelwarn-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_CONTEXT="84000")
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


def models() -> list:
    # the --model of every start of a session (stream-json), in order
    out = []
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        for line in log:
            if line.startswith("ARGV ") and "stream-json" in line:
                argv = json.loads(line[5:])
                out.append(argv[argv.index("--model") + 1] if "--model" in argv else "")
    return out


def wait_starts(count: int) -> list:
    deadline = time.time() + 15
    while time.time() < deadline and len(models()) < count:
        time.sleep(0.2)
    return models()


for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[sessions]\nkeep_warm_ttl = 1\n[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "warn", "prompt": "hello"}).get("data") or {}).get("id", "s1")
    time.sleep(3)  # its turn done: its context loaded, its cache fresh
    starts = len(wait_starts(1))
    got = (ask({"cmd": "slash", "session": sid, "text": "/model haiku"}).get("data") or {})
    check("/model on a loaded context: asked first, the size told", got.get("ui") == f"pick:model_warn:{sid}" and "tokens used by this session" in got.get("message", ""),
          json.dumps(got)[:300])
    got = (ask({"cmd": "action", "id": "cancel", "session": sid}).get("data") or {})
    time.sleep(2)
    check("Cancel: nothing changed (no restart)", len(models()) == starts and "nothing changed" in got.get("message", ""), json.dumps(got)[:200] + f" starts {models()}")
    ask({"cmd": "slash", "session": sid, "text": "/model haiku"})
    got = (ask({"cmd": "action", "id": "model_go", "session": sid}).get("data") or {})
    check("changed anyway: started again on the new model", wait_starts(starts + 1)[-1:] == ["haiku"], json.dumps(got)[:200] + f" {models()}")
    ask({"cmd": "send", "session": sid, "text": "again"})
    time.sleep(3)
    got = (ask({"cmd": "apply", "kind": "model", "value": "sonnet", "scope": "here", "session": sid}).get("data") or {})
    check("the page (apply): a confirm first, nothing changed", "confirm" in got and "done" not in got and models()[-1:] == ["haiku"], json.dumps(got)[:300])
    got = (ask({"cmd": "apply", "kind": "model", "value": "sonnet", "scope": "here", "session": sid, "confirmed": True}).get("data") or {})
    check("... confirmed: changed", "done" in got and wait_starts(starts + 2)[-1:] == ["sonnet"], json.dumps(got)[:300] + f" {models()}")
    time.sleep(65)  # its cache expired (1 min here): everything is read again anyway
    got = (ask({"cmd": "slash", "session": sid, "text": "/model opus"}).get("data") or {})
    check("its cache expired: changed at once (nothing asked)", not got.get("ui") and wait_starts(starts + 3)[-1:] == ["opus"], json.dumps(got)[:300] + f" {models()}")
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
