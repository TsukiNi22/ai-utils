#!/usr/bin/env python3
# Keep-warm: off by default (nothing woken up); /keepwarm marks a session (the mode becomes chosen, the session listed keep_warm, ♨);
# idle, it is woken up a little before its prompt cache expires (cache life 1 min, margin 10 s here: ~50 s after its last entry), then
# again, the previous wake-up taken back first (one in the conversation, not one more each time); unmarked: left to cool; turned on while its cache already expired: asked first, kept warm anyway: one wake-up at once.
# Usage: tests/keep_warm.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""Sessions kept warm before their prompt cache expires."""
import glob
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-warm-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_TRANSCRIPT="1")
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


def wakes() -> int:
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        return log.read().count("keep-warm")


def listed(sid: str) -> dict:
    return next((s for s in ask({"cmd": "list"}).get("data") or [] if s.get("id") == sid), {})


for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[sessions]\nkeep_warm_ttl = 1\nkeep_warm_margin = 10\n[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "warm", "prompt": "hello"}).get("data") or {}).get("id", "s1")
    time.sleep(70)
    check("off by default: never woken up", wakes() == 0, str(wakes()))
    check("not listed kept warm", listed(sid).get("keep_warm") is False, json.dumps(listed(sid))[:200])
    check("its cache counted from its last turn (expired: negative)", (listed(sid).get("cache_left") or 0) < 0, json.dumps(listed(sid).get("cache_left")))
    got = ask({"cmd": "slash", "session": sid, "text": "/keepwarm"})
    check("/keepwarm, its cache already expired: asked first (Cancel the default)", (got.get("data") or {}).get("ui") == f"pick:warm:{sid}" and wakes() == 0
          and listed(sid).get("keep_warm") is False, json.dumps(got)[:300])
    got = ask({"cmd": "action", "id": f"keep_warm_force:{sid}", "session": sid})
    deadline = time.time() + 20
    while time.time() < deadline and wakes() == 0:
        time.sleep(0.5)
    check("... kept warm anyway: the mode chosen, one wake-up at once", "chosen" in json.dumps(got) and wakes() == 1, json.dumps(got)[:300] + f" wakes {wakes()}")
    check("listed kept warm (♨), its cache fresh again", listed(sid).get("keep_warm") is True and (listed(sid).get("cache_left") or 0) > 30, json.dumps(listed(sid))[:300])
    ask({"cmd": "send", "session": sid, "text": "a real prompt"})
    deadline = time.time() + 90
    while time.time() < deadline and wakes() == 1:
        time.sleep(1)
    check("woken up before its cache expires", wakes() >= 2, str(wakes()))
    first = wakes()
    deadline = time.time() + 90
    while time.time() < deadline and wakes() == first:
        time.sleep(1)
    check("and again (kept warm)", wakes() > first, str(wakes()))
    kept = "".join(open(path, encoding="utf-8").read() for path in glob.glob(f"{T}/data/logs/{sid}*.jsonl"))
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        resumed = log.read().count("--resume-session-at")
    since = kept.split("a real prompt", 1)[-1]  # the wake-up forced before it stays (a real prompt came after it)
    check("the previous wake-up taken back before the next one: one after the real prompt, the prompt kept", "a real prompt" in kept
          and since.count("keep-warm") == 1 and resumed >= 1, f"keep-warm after it {since.count('keep-warm')}, resumed at {resumed}")
    # taken back by the user (nothing sent after): its last entry older now, its cache still counted from the last request
    got = ask({"cmd": "rewind", "session": sid, "back": 2})
    left = listed(sid).get("cache_left") or 0
    check("taken back by the user: its cache still counted from the last request", left > 20, json.dumps(got)[:200] + f" cache_left {left}")
    before = wakes()
    deadline = time.time() + 90
    while time.time() < deadline and wakes() == before:
        time.sleep(1)
    check("... and woken up again before it expires", wakes() > before, str(wakes()))
    got = ask({"cmd": "slash", "session": sid, "text": "/keepwarm off"})
    check("/keepwarm off: no longer kept warm", "no longer" in json.dumps(got) and listed(sid).get("keep_warm") is False, json.dumps(got)[:200])
    after = wakes()
    time.sleep(75)
    check("unmarked: left to cool", wakes() == after, str(wakes()))
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
