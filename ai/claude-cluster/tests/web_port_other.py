#!/usr/bin/env python3
# The web access on a port another program uses: its failure popup offers a third choice next to OK / Retry - "Use port N", the next
# free one; chosen, the port is saved in the settings and the web access opens on it.
# Usage: tests/web_port_other.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A free port offered when the one of the web access is taken."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

T = tempfile.mkdtemp(prefix="cc-webport-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", CLAUDE_CLUSTER_DISCOVERY_PORT="18887")
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


taken = socket.socket()  # another program on the port of the web access
taken.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
taken.bind(("0.0.0.0", 0))
taken.listen(1)
PORT = taken.getsockname()[1]
for folder in ("cfg", "data", "run", "cache", "home"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[web]\nenabled = true\nport = {PORT}\nhttps = false\ntty_port = 0\ndiscovery = false\nupnp = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    failure = {}
    deadline = time.time() + 20
    while time.time() < deadline and not failure:
        try:
            failure = next((f for f in ask({"cmd": "failures"}).get("data") or [] if "web access" in f.get("action", "")), {})
        except OSError:
            pass
        time.sleep(0.5)
    check("the port taken: a failure popup", bool(failure), json.dumps(failure))
    check("OK and Retry, and a free port offered", failure.get("retry") is True and failure.get("other", "").startswith("Use port "), json.dumps(failure))
    free = int(failure.get("other", "Use port 0").split()[-1])
    check("the next free port, not the taken one", free > PORT, str(free))
    ask({"cmd": "failure", "id": failure.get("id", ""), "answer": "other"})
    answered = False
    deadline = time.time() + 20
    while time.time() < deadline and not answered:
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{free}/", timeout=2) as page:
                answered = page.status == 200
        except Exception:
            time.sleep(0.5)
    check("chosen: the web access answers on it", answered)
    with open(f"{T}/cfg/config.toml", encoding="utf-8") as config:
        saved = config.read()
    check("saved in the settings", f"port = {free}" in saved, saved[-300:])
    check("the popup gone", not any("web access" in f.get("action", "") for f in ask({"cmd": "failures"}).get("data") or []))
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    taken.close()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
