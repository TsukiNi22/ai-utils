#!/usr/bin/env python3
# The memory guard of the orchestrator: under the memory kept free it kills the biggest command a session left running (its whole tree),
# never the core nor the agent, tells the session why (it may run it again with less memory), and lists the kill for the Processes page.
# guard_free_mb set past the RAM here: the memory looks short at once.
# Usage: tests/memory_guard.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The memory guard kills the biggest command of a session and tells it."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-memguard-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log")
failed = False


def check(name, ok, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def ask(request):
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


def alive(pid):
    try:
        with open(f"/proc/{pid}/stat", encoding="utf-8") as stat:
            return stat.read().split(")")[-1].split()[0] != "Z"
    except OSError:
        return False


def log():
    with open(f"{T}/fake.log", encoding="utf-8") as f:
        return f.read()


for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[orchestrator]\nmemory_guard = false\nguard_free_mb = 100000000\nguard_min_mb = 50\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(50):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.2)
    session = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker", "prompt": "leave a hog behind"}).get("data") or {}).get("id", "s1")
    hog = 0
    for _ in range(40):
        lines = [line for line in log().split("\n") if line.startswith("HOG ")]
        if lines:
            hog = int(lines[0].split()[1])
            break
        time.sleep(0.25)
    time.sleep(2)
    check("a command of the session holds memory", hog > 0 and alive(hog), str(hog))
    agent = 0  # the fake agent of this test: fake_claude.py with its log here
    for entry in os.listdir("/proc"):
        try:
            with open(f"/proc/{entry}/cmdline", "rb") as f:
                cmd = f.read().decode(errors="replace")
            with open(f"/proc/{entry}/environ", "rb") as f:
                environ = f.read().decode(errors="replace")
        except OSError:
            continue
        if "fake_claude.py" in cmd and f"{T}/fake.log" in environ and int(entry) != hog:
            agent = int(entry)
    time.sleep(4)
    check("the guard off: nothing killed", alive(hog))
    ask({"cmd": "settings_set", "section": "orchestrator", "key": "memory_guard", "value": "true", "confirmed": True})
    for _ in range(40):
        if not alive(hog):
            break
        time.sleep(0.25)
    check("the guard on, the memory short: the biggest command killed", not alive(hog))
    check("never the core", core.poll() is None)
    check("never the agent (small)", agent > 0 and alive(agent), str(agent))
    for _ in range(40):
        if "memory guard" in log():
            break
        time.sleep(0.25)
    told = [line for line in log().split("\n") if "memory guard" in line]
    check("its session told why (with less memory next time)", bool(told) and "less memory" in told[0] and "MB" in told[0], told[0][:300] if told else "")
    guard = (ask({"cmd": "orch_state"}).get("data") or {}).get("guard", {})
    kills = guard.get("kills", [])
    check("the kill listed with its details (Processes page)", bool(kills) and kills[-1].get("pid") == hog and kills[-1].get("mb", 0) >= 50 and kills[-1].get("session") == session,
          json.dumps(guard)[:400])
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
    if hog:
        try:
            os.kill(hog, 9)
        except OSError:
            pass
sys.exit(1 if failed else 0)
