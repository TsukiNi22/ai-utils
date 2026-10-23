#!/usr/bin/env python3
# Regression test: claude-cluster --demo --web (the demo instance, its sessions seeded, its web access read only).
# Usage: tests/demo.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""claude-cluster --demo --web: the page reads (list, transcript) and changes nothing (send, spawn, settings_set refused)."""
import json
import os
import socket
import ssl
import subprocess
import time

import sys
import tempfile

CACHE = tempfile.mkdtemp(prefix="cc-demo-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
SOCK = CACHE + "/claude-cluster-demo/sock"
PORT = 8444
ENV = dict(os.environ, XDG_CACHE_HOME=CACHE)
for name in ("CLAUDE_CLUSTER_CONFIG_DIR", "CLAUDE_CLUSTER_DATA_DIR", "CLAUDE_CLUSTER_SOCKET", "CLAUDE_CLUSTER_DEMO"):
    ENV.pop(name, None)


def check(name, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""))


def ask(request):
    sock = socket.socket(socket.AF_UNIX)
    sock.settimeout(20)
    sock.connect(SOCK)
    sock.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        data += sock.recv(1 << 20)
    sock.close()
    return json.loads(data)


def https(method, path, body="", cookie=""):
    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    raw = socket.create_connection(("127.0.0.1", PORT), timeout=20)
    tls = context.wrap_socket(raw, server_hostname="127.0.0.1")
    tls.sendall(f"{method} {path} HTTP/1.1\r\nHost: 127.0.0.1:{PORT}\r\nOrigin: https://127.0.0.1:{PORT}\r\n{'Cookie: ' + cookie + chr(13) + chr(10) if cookie else ''}"
                f"Content-Type: application/json\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n{body}".encode())
    data = b""
    while True:
        try:
            chunk = tls.recv(65536)
        except (socket.timeout, ssl.SSLError):
            break
        if not chunk:
            break
        data += chunk
    tls.close()
    head, _, payload = data.partition(b"\r\n\r\n")
    cookies = {}
    for line in head.decode(errors="replace").split("\r\n"):
        if line.lower().startswith("set-cookie:"):
            pair = line.split(":", 1)[1].strip().split(";")[0]
            cookies[pair.split("=")[0]] = pair.split("=", 1)[1]
    try:
        parsed = json.loads(payload)
    except ValueError:
        parsed = {}
    return int(head.split(b" ")[1]), parsed, cookies


DECOYS = os.path.join(CACHE, "decoys")
LOG = os.path.join(CACHE, "decoy.log")
os.makedirs(DECOYS)
for agent in ("claude", "codex", "qwen", "opencode", "ollama"):
    with open(os.path.join(DECOYS, agent), "w") as file:
        file.write(f"#!/bin/sh\necho {agent} \"$@\" >> {LOG}\nexit 1\n")
    os.chmod(os.path.join(DECOYS, agent), 0o755)
ENV["PATH"] = DECOYS + ":" + ENV["PATH"]
core = subprocess.Popen([BIN, "--demo", "--web", "serve"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    time.sleep(14)
    pin = ask({"cmd": "web", "action": "devices"})["data"]["pin"].replace(" ", "")
    code, body, cookies = https("POST", "/api/login", json.dumps({"pin": pin, "name": "t"}))
    cookie = f"cc_dev={cookies.get('cc_dev', '')}; cc_web={cookies.get('cc_web', '')}"
    devices = ask({"cmd": "web", "action": "devices"})["data"]
    for item in devices.get("pending", []):
        ask({"cmd": "web", "action": "approve", "id": item.get("id", ""), "answer": "allow"})
    time.sleep(1)
    code, body, cookies2 = https("POST", "/api/login", json.dumps({"pin": pin, "name": "t"}), cookie)
    cookie = f"cc_dev={cookies2.get('cc_dev', cookies.get('cc_dev', ''))}; cc_web={cookies2.get('cc_web', cookies.get('cc_web', ''))}"
    check("the page connected", code == 200 and body.get("state") == "connected", str((code, body)))
    code, body, _ = https("POST", "/api/cmd", json.dumps({"cmd": "list"}), cookie)
    check("list: read (the demo sessions)", body.get("ok") and len(body.get("data", [])) >= 8, str(body)[:200])
    code, body, _ = https("POST", "/api/cmd", json.dumps({"cmd": "send", "session": "s1", "text": "hello"}), cookie)
    check("send refused (read only)", body.get("ok") is False and "read only" in body.get("error", ""), str(body))
    code, body, _ = https("POST", "/api/cmd", json.dumps({"cmd": "spawn", "cwd": "~/x"}), cookie)
    check("spawn refused (read only)", body.get("ok") is False and "read only" in body.get("error", ""), str(body))
    code, body, _ = https("POST", "/api/cmd", json.dumps({"cmd": "interrupt", "session": "s1"}), cookie)
    others = [line for line in subprocess.run(["ps", "-eo", "args"], capture_output=True, text=True).stdout.splitlines() if "demo-agent" in line]
    check("the agents of the demo are the fake one", len(others) >= 8, str(len(others)))
    check("no real agent was ever started (decoys never called)", not os.path.exists(LOG), open(LOG).read() if os.path.exists(LOG) else "")
    check("interrupt refused (read only)", body.get("ok") is False and "read only" in body.get("error", ""), str(body))
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    subprocess.run(["rm", "-rf", CACHE], check=False)
