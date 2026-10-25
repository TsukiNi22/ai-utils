#!/usr/bin/env python3
# The account usage read once for every core of the user: two cores (two instances, their own data) share the cache folder; a fake
# usage endpoint counts the requests. The first core asks it, the second reads the shared copy (no request of its own) and both show
# the same windows; past [usage] account_refresh the account is asked again (once).
# Usage: tests/account_shared.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The account usage shared by every core (one request, one copy)."""
import http.server
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time

T = tempfile.mkdtemp(prefix="cc-account-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
hits = []
failed = False


class Usage(http.server.BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        hits.append(time.time())
        body = json.dumps({"five_hour": {"utilization": 42, "resets_at": "2030-01-01T00:00:00Z"},
                           "seven_day": {"utilization": 7, "resets_at": "2030-01-05T00:00:00Z"}}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args) -> None:
        pass


def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def ask(sock: str, request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(20)
    s.connect(sock)
    s.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        data += chunk
    s.close()
    return json.loads(data)


def core(name: str, port: int) -> subprocess.Popen:
    base = f"{T}/{name}"
    for folder in ("cfg", "data", "run"):
        os.makedirs(f"{base}/{folder}")
    with open(f"{base}/cfg/config.toml", "w", encoding="utf-8") as config:
        config.write('[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = true\naccount_refresh = 30\n')
    env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
    env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{base}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{base}/data", CLAUDE_CLUSTER_SOCKET=f"{base}/sock",
               XDG_RUNTIME_DIR=f"{base}/run", XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", CLAUDE_CLUSTER_ACCOUNT_URL=f"http://127.0.0.1:{port}/usage")
    process = subprocess.Popen([BIN, "serve", "--no-global"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        if os.path.exists(f"{base}/sock"):
            try:
                ask(f"{base}/sock", {"cmd": "hello"})
                break
            except OSError:
                pass
        time.sleep(0.1)
    return process


def windows(name: str) -> dict:
    for item in ask(f"{T}/{name}/sock", {"cmd": "usage", "vendor": "anthropic"}).get("data") or []:
        return {w.get("name"): w.get("used_percent") for w in item.get("windows", [])}
    return {}


os.makedirs(f"{T}/home/.claude")
with open(f"{T}/home/.claude/.credentials.json", "w", encoding="utf-8") as cred:
    json.dump({"claudeAiOauth": {"accessToken": "test-token", "expiresAt": int(time.time() + 3600) * 1000}}, cred)
server = http.server.HTTPServer(("127.0.0.1", 0), Usage)
threading.Thread(target=server.serve_forever, daemon=True).start()
port = server.server_address[1]
cores = []
try:
    cores.append(core("a", port))
    deadline = time.time() + 40
    while time.time() < deadline and windows("a").get("5h") != 42:
        time.sleep(1)
    check("the first core read the account", windows("a").get("5h") == 42, json.dumps(windows("a")))
    asked = len(hits)
    check("one request", asked == 1, str(asked))
    cores.append(core("b", port))
    deadline = time.time() + 40
    while time.time() < deadline and windows("b").get("5h") != 42:
        time.sleep(1)
    check("the second core shows the same windows", windows("b") == windows("a"), json.dumps(windows("b")))
    check("from the shared copy: no request of its own", len(hits) == asked, str(len(hits)))
    deadline = time.time() + 60
    while time.time() < deadline and len(hits) == asked:
        time.sleep(1)
    time.sleep(5)
    check("past account_refresh: asked again, once for both", len(hits) == asked + 1, str(len(hits)))
finally:
    for process in cores:
        process.terminate()
    for process in cores:
        try:
            process.wait(20)
        except subprocess.TimeoutExpired:
            process.kill()
    server.shutdown()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
