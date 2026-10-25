#!/usr/bin/env python3
# The update asked for again: a newer claude-cluster installed (a stand-in binary renamed over the running one) is offered, ignored,
# then the Update card of Settings > Services shows it ignored, 'Offer it again' (and /update check) bring it back; up to date: told so.
# Usage: tests/update_ask.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""/update and the Update card: an ignored version offered again."""
import json
import os
import shutil
import socket
import stat
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-upask-")
BUILT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
BIN = f"{T}/bin/claude-cluster"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home")
FAKE = '#!/bin/sh\n[ "$1" = "--version" ] && echo "claude-cluster 9.9.9"\nexit 0\n'
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


def card() -> dict:
    return (ask({"cmd": "services"}).get("data") or {}).get("update", {})


def act(action: str) -> str:
    answer = ask({"cmd": "service_act", "service": "update", "action": action})
    return json.dumps(answer.get("data") or answer.get("error"))


for folder in ("cfg", "data", "run", "cache", "home", "bin"):
    os.makedirs(f"{T}/{folder}")
shutil.copy(BUILT, BIN)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(4)
    check("nothing newer: the card says up to date", card().get("installed") == card().get("running"), json.dumps(card()))
    check("Check again: up to date", "up to date" in act("check"), act("check"))
    with open(f"{T}/bin/new", "w", encoding="utf-8") as fake:
        fake.write(FAKE)
    os.chmod(f"{T}/bin/new", os.stat(f"{T}/bin/new").st_mode | stat.S_IEXEC)
    os.rename(f"{T}/bin/new", BIN)  # a newer one installed over the running binary
    offered = ""
    deadline = time.time() + 15
    while time.time() < deadline and offered != "9.9.9":
        offered = card().get("offered", "")
        time.sleep(0.5)
    check("the newer version offered", offered == "9.9.9", json.dumps(card()))
    print("  ignore:", ask({"cmd": "upgrade", "action": "ignore"}).get("data"))
    check("ignored: no longer offered, the card shows it ignored", card().get("offered", "") == "" and card().get("ignored") == "9.9.9", json.dumps(card()))
    check("Check again: told it is ignored (not offered by itself)", "ignored" in act("check") and card().get("offered", "") == "", act("check"))
    check("Offer it again: offered", "9.9.9" in act("offer") and card().get("offered") == "9.9.9", json.dumps(card()))
    check("no longer ignored", card().get("ignored", "") == "", json.dumps(card()))
    ask({"cmd": "upgrade", "action": "ignore"})
    got = ask({"cmd": "slash", "session": "g", "text": "/update check"})
    check("/update check: offered again too", "9.9.9" in json.dumps(got) and card().get("offered") == "9.9.9", json.dumps(got)[:300])
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
