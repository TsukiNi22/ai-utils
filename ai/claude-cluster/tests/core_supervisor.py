#!/usr/bin/env python3
# A core that crashes is started again by its watcher with its sessions restored (an instance too: its own socket); a normal stop
# (SIGTERM: quit, Stop all, the replacement of a version) ends the watcher with it, nothing started again.
# Usage: tests/core_supervisor.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A crashed core started again by its watcher."""
import json
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-super-")
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


def core_pid(wait: float = 15):
    end = time.time() + wait
    while time.time() < end:
        try:
            return (ask({"cmd": "hello"}).get("data") or {}).get("pid", 0)
        except OSError:
            time.sleep(0.3)
    return 0


for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
watcher = subprocess.Popen([BIN, "serve"], env=ENV, stdout=subprocess.DEVNULL, stderr=open(f"{T}/serve.err", "w"))
try:
    first = core_pid()
    check("the core runs under its watcher", first > 0 and first != watcher.pid, f"core {first}, watcher {watcher.pid}")
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "kept"})
    time.sleep(1.5)
    os.kill(first, signal.SIGSEGV)  # a crash
    time.sleep(2)
    second = core_pid()
    check("a crash: the core started again", second > 0 and second != first, f"{first} -> {second}")
    names = [s.get("name") for s in ask({"cmd": "list"}).get("data", [])]
    check("its sessions restored", "kept" in names, json.dumps(names))
    with open(f"{T}/serve.err", encoding="utf-8") as err:
        check("told in its log", "crashed (signal 11): started again" in err.read())
    os.kill(second, signal.SIGTERM)  # a normal stop
    try:
        watcher.wait(15)
        ended = True
    except subprocess.TimeoutExpired:
        ended = False
    check("a normal stop: the watcher ends with it, nothing started again", ended and core_pid(2) == 0)
finally:
    watcher.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
