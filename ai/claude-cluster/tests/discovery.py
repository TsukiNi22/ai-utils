#!/usr/bin/env python3
# The app finds the computer on its network: while the web access is open, the core answers its broadcast ("claude-cluster?", UDP) with
# its name, its port, https and the fingerprint of its authority (the one the app pinned) - never its code; anything else unanswered,
# one answer per address every 0.5 s, [web] discovery = false: silent.
# Usage: tests/discovery.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The core found by the app on the LAN."""
import hashlib
import json
import os
import socket
import ssl
import subprocess
import sys
import tempfile
import time

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
PORT = 18799
FIND = 18789
failed = False


def check(name, ok, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def ask(text, wait: float = 1.5):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(wait)
    s.sendto(text.encode(), ("127.0.0.1", FIND))
    try:
        return json.loads(s.recv(4096))
    except (socket.timeout, ValueError):
        return None
    finally:
        s.close()


def core(folder, discovery):
    for name in ("cfg", "data", "run", "cache"):
        os.makedirs(f"{folder}/{name}")
    with open(f"{folder}/cfg/config.toml", "w", encoding="utf-8") as config:
        config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                     f'[web]\nenabled = true\nport = {PORT}\nhttps = true\ntty_port = 0\ndiscovery = {"true" if discovery else "false"}\n')
    env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
    env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{folder}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{folder}/data", CLAUDE_CLUSTER_SOCKET=f"{folder}/sock",
               XDG_RUNTIME_DIR=f"{folder}/run", XDG_CACHE_HOME=f"{folder}/cache", CLAUDE_CLUSTER_DISCOVERY_PORT=str(FIND))
    process = subprocess.Popen([BIN, "serve", "--no-global"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(60):
        if os.path.exists(f"{folder}/cfg/web/ca.pem") and os.path.exists(f"{folder}/sock"):
            break
        time.sleep(0.25)
    time.sleep(2)
    return process


def stop(process, folder):
    process.terminate()
    try:
        process.wait(10)
    except subprocess.TimeoutExpired:
        process.kill()
    os.system(f"pkill -f {folder} > /dev/null 2>&1")


T = tempfile.mkdtemp(prefix="cc-discovery-")
one = core(f"{T}/on", True)
try:
    answer = ask("claude-cluster?")
    check("the broadcast of the app answered", isinstance(answer, dict) and answer.get("app") == "claude-cluster", json.dumps(answer))
    answer = answer or {}
    check("its port and https told", answer.get("port") == PORT and answer.get("https") is True, json.dumps(answer))
    with open(f"{T}/on/cfg/web/ca.pem", encoding="utf-8") as pem:
        digest = hashlib.sha256(ssl.PEM_cert_to_DER_cert(pem.read())).hexdigest().upper()
    mine = ":".join(digest[i:i + 2] for i in range(0, len(digest), 2))
    check("the fingerprint of its authority (what the app pinned)", answer.get("authority") == mine, f"{answer.get('authority')} != {mine}")
    check("nothing secret in it (its code)", set(answer) <= {"app", "name", "port", "https", "authority", "version"}, json.dumps(answer))
    check("asked again at once: one answer per address every 0.5 s", ask("claude-cluster?", 0.3) is None)
    time.sleep(0.6)
    check("anything else unanswered", ask("hello there") is None)
    time.sleep(0.6)
    check("asked again later: answered", ask("claude-cluster?") is not None)
finally:
    stop(one, f"{T}/on")
two = core(f"{T}/off", False)
try:
    check("[web] discovery = false: silent", ask("claude-cluster?") is None)
finally:
    stop(two, f"{T}/off")
sys.exit(1 if failed else 0)
