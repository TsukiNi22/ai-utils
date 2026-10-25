#!/usr/bin/env python3
# A file opened by claude-cluster (a PDF of a display, a picture, the log...) stays open: the viewer is left alone - its own session,
# not a child of the core, never killed after a delay (it was: a 30 s capture, its group killed), none of the sockets of the core.
# A stand-in xdg-open (first on the PATH) writes its pid and stays; the core opens its log with it.
# Usage: tests/detach_open.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A viewer opened by claude-cluster is never closed by it."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-detach-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", PATH=f"{T}/bin:" + os.environ.get("PATH", "/usr/bin:/bin"))
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


def alive(pid: int) -> bool:
    try:
        with open(f"/proc/{pid}/stat", encoding="utf-8") as stat:
            return stat.read().split(") ")[-1].split()[0] not in ("Z", "X")
    except OSError:
        return False


for folder in ("cfg", "data", "run", "cache", "home", "bin"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/bin/xdg-open", "w", encoding="utf-8") as viewer:
    viewer.write(f'#!/bin/sh\necho $$ > {T}/viewer.pid\nexec sleep 120\n')
os.chmod(f"{T}/bin/xdg-open", 0o755)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
pid = 0
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    open(f"{T}/data/core.log", "a", encoding="utf-8").close()
    answer = ask({"cmd": "service_act", "service": "core", "action": "log"})
    check("the log opened by the viewer", "opened" in json.dumps(answer), json.dumps(answer))
    for _ in range(50):
        if os.path.exists(f"{T}/viewer.pid"):
            break
        time.sleep(0.1)
    pid = int(open(f"{T}/viewer.pid", encoding="utf-8").read().strip() or 0) if os.path.exists(f"{T}/viewer.pid") else 0
    check("the viewer started", pid > 0 and alive(pid))
    with open(f"/proc/{pid}/stat", encoding="utf-8") as stat:
        fields = stat.read().split(") ")[-1].split()
    check("its own session (never killed with a group of the core)", int(fields[3]) != os.getpgid(core.pid) and int(fields[4]) != os.getsid(core.pid),
          " ".join(fields[:6]))
    check("not a child of the core", int(fields[1]) != core.pid, fields[1])
    sockets = [os.readlink(f"/proc/{pid}/fd/{fd}") for fd in os.listdir(f"/proc/{pid}/fd")]
    check("none of the sockets of the core", not any(s.startswith("socket:") for s in sockets), str(sockets))
    time.sleep(35)  # past the 30 s after which it used to be killed
    check("still open 35 s later", alive(pid))
    core.terminate()
    core.wait(20)
    time.sleep(1)
    check("still open once the core stopped", alive(pid))
finally:
    if pid > 0:
        try:
            os.kill(pid, 9)
        except OSError:
            pass
    if core.poll() is None:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
