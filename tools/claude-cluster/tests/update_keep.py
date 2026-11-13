#!/usr/bin/env python3
# What is on for a run survives a smooth update: the web access opened by hand (its config off) is open again in the new version (the
# version before writes upgrade-keep.json just before its switch, the new one reads it at its start, once); a normal start never reads it.
# Usage: tests/update_keep.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The web access opened by hand kept across an update."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-keep-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", CLAUDE_CLUSTER_TEST="1")
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


def core(env):
    proc = subprocess.Popen([BIN, "serve"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(60):
        try:
            ask({"cmd": "list"})
            return proc
        except OSError:
            time.sleep(0.2)
    return proc


def stop(proc):
    proc.terminate()
    try:
        proc.wait(10)
    except subprocess.TimeoutExpired:
        proc.kill()
    for _ in range(30):
        if not os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)


def web_on():
    return bool((ask({"cmd": "web"}).get("data") or {}).get("on"))


for folder in ("cfg", "data", "run", "cache"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 '[web]\nenabled = false\nport = 18943\n')
first = core(ENV)
try:
    check("its config: the web access closed", not web_on())
    ask({"cmd": "web", "action": "on"})
    time.sleep(1.5)
    check("opened by hand for this run", web_on())
    ask({"cmd": "upgrade", "action": "keep"})  # what the update writes just before its switch
    check("what is on written before the switch", os.path.exists(f"{T}/data/upgrade-keep.json"))
finally:
    stop(first)
updated = core(dict(ENV, CLAUDE_CLUSTER_UPGRADED_FROM="1.0.0"))
try:
    time.sleep(1.5)
    check("the new version: the web access open again", web_on())
    check("read once (the file gone)", not os.path.exists(f"{T}/data/upgrade-keep.json"))
finally:
    stop(updated)
with open(f"{T}/data/upgrade-keep.json", "w", encoding="utf-8") as stale:
    json.dump({"at": int(time.time()), "web": True, "background": False}, stale)
normal = core(ENV)
try:
    time.sleep(1.5)
    check("a normal start: its config only (the file of an update never read)", not web_on())
finally:
    stop(normal)
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
