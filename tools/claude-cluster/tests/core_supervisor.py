#!/usr/bin/env python3
# A core that crashes is started again by its watcher with its sessions restored (an instance too: its own socket), from the binary on
# disk: another build given, it is installed in place of the one running, then the crash - the core comes back on the build installed
# (before: the watcher forked its own image, an update lost by a crash while stopping for it); a normal stop (SIGTERM: quit, Stop all,
# the replacement of a version) ends the watcher with it, nothing started again.
# Usage: tests/core_supervisor.py [path/to/claude-cluster] [another build]   (default: /tmp/cc-build/claude-cluster)
"""A crashed core started again by its watcher."""
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-super-")
TESTED = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
OTHER = sys.argv[2] if len(sys.argv) > 2 else ""
BIN = f"{T}/bin/claude-cluster" # a copy: installed again by the test
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log")
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

def core_pid(wait: float = 15) -> int:
    end = time.time() + wait
    while time.time() < end:
        try:
            return (ask({"cmd": "hello"}).get("data") or {}).get("pid", 0)
        except OSError:
            time.sleep(0.3)
    return 0

def version(binary: str) -> str:
    return subprocess.run([binary, "--version"], capture_output=True, text=True).stdout.split()[-1]

def install(binary: str) -> None:
    shutil.copy2(binary, f"{T}/bin/claude-cluster.new")
    os.rename(f"{T}/bin/claude-cluster.new", BIN) # like an install: a new file in its place, the running one kept in memory

for folder in ("cfg", "data", "run", "cache", "work", "bin"):
    os.makedirs(f"{T}/{folder}")
install(TESTED)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
watcher = subprocess.Popen([BIN, "serve"], env=ENV, stdout=subprocess.DEVNULL, stderr=open(f"{T}/serve.err", "w", encoding="utf-8"))
try:
    first = core_pid()
    check("the core runs under its watcher", first > 0 and first != watcher.pid, f"core {first}, watcher {watcher.pid}")
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "kept"})
    time.sleep(1.5)
    if OTHER:
        install(OTHER) # another version installed while this one runs
        time.sleep(0.5)
    os.kill(first, signal.SIGSEGV)  # a crash
    time.sleep(2)
    second = core_pid()
    check("a crash: the core started again", second > 0 and second != first, f"{first} -> {second}")
    if OTHER:
        running = (ask({"cmd": "hello"}).get("data") or {}).get("version", "")
        check("another build installed meanwhile: the core back on it (the binary on disk, not the image of the watcher)",
              running == version(OTHER) and running != version(TESTED), f"running {running}, tested {version(TESTED)}, installed {version(OTHER)}")
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
