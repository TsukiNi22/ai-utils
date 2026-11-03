#!/usr/bin/env python3
# ~/.claude/.credentials.json (a file of another tool) or the shared copy of the account of another shape: the thread reading the account ended holding its lock (the other cores
# of the user waiting too) and its busy flag, the account never read again until a restart. A core per shape, its own cache folder, a fake usage endpoint: the lock free, no thread
# ended, and the account read as soon as the file is right again.
# Usage: tests/account_shape.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The credentials and the copy of the account of another shape."""
import fcntl
import http.server
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time

T = tempfile.mkdtemp(prefix="cc-account-shape-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
GOOD = {"accessToken": "test-token", "expiresAt": int(time.time() + 3600) * 1000}
# name -> what claudeAiOauth holds at first (a token of another kind: no token; a date of another kind: not known, like a date missing)
SHAPES = {"date_text": {"accessToken": "abc", "expiresAt": "soon"}, "token_number": {"accessToken": 5, "expiresAt": 0}, "token_null": {"accessToken": None},
          "both_lists": {"accessToken": ["x"], "expiresAt": {"a": 1}}, "not_an_object": ["x"]}
NO_TOKEN = ("token_number", "token_null", "both_lists", "not_an_object")
hits: dict = {}
failed = False

class Usage(http.server.BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        name = self.path.rsplit("/", 1)[-1]
        hits[name] = hits.get(name, 0) + 1
        body = json.dumps({"five_hour": {"utilization": 42, "resets_at": "2030-01-01T00:00:00Z"}, "seven_day": {"utilization": 7, "resets_at": "2030-01-05T00:00:00Z"}}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args: object) -> None:
        pass

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(sock: str, request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(30)
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

def core(name: str, port: int, oauth: object, copy: object = None) -> subprocess.Popen:
    base = f"{T}/{name}"
    for folder in ("cfg", "data", "run", "cache/claude-cluster", "home/.claude"):
        os.makedirs(f"{base}/{folder}")
    write_credentials(name, oauth)
    if copy is not None:
        with open(f"{base}/cache/claude-cluster/account-usage-test.json", "w", encoding="utf-8") as out:
            json.dump(copy, out)
    with open(f"{base}/cfg/config.toml", "w", encoding="utf-8") as config:
        config.write('[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = true\naccount_refresh = 30\n')
    env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
    env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{base}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{base}/data", CLAUDE_CLUSTER_SOCKET=f"{base}/sock", XDG_RUNTIME_DIR=f"{base}/run",
               XDG_CACHE_HOME=f"{base}/cache", HOME=f"{base}/home", CLAUDE_CLUSTER_ACCOUNT_URL=f"http://127.0.0.1:{port}/usage/{name}")
    with open(f"{base}/core.err", "w", encoding="utf-8") as err:
        process = subprocess.Popen([BIN, "serve", "--no-global"], env=env, stdout=subprocess.DEVNULL, stderr=err, cwd="/")
    for _ in range(100):
        if os.path.exists(f"{base}/sock"):
            try:
                ask(f"{base}/sock", {"cmd": "hello"})
                break
            except OSError:
                pass
        time.sleep(0.1)
    return process

def write_credentials(name: str, oauth: object) -> None:
    with open(f"{T}/{name}/home/.claude/.credentials.json", "w", encoding="utf-8") as cred:
        json.dump({"claudeAiOauth": oauth}, cred)

def windows(name: str) -> dict:
    for item in ask(f"{T}/{name}/sock", {"cmd": "usage", "vendor": "anthropic"}).get("data") or []:
        return {w.get("name"): w.get("used_percent") for w in item.get("windows", [])}
    return {}

def tried(name: str) -> bool:
    # its try written in the shared copy (a thread that ended never wrote it)
    try:
        with open(f"{T}/{name}/cache/claude-cluster/account-usage-test.json", encoding="utf-8") as copy:
            return isinstance(json.load(copy).get("tried"), int)
    except (OSError, ValueError):
        return False

def lock_free(name: str) -> bool:
    with open(f"{T}/{name}/cache/claude-cluster/account-usage-test.json.lock", "a", encoding="utf-8") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            return False
        fcntl.flock(lock, fcntl.LOCK_UN)
    return True

def error_text(name: str) -> str:
    with open(f"{T}/{name}/core.err", encoding="utf-8") as err:
        return err.read()

server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Usage)
threading.Thread(target=server.serve_forever, daemon=True).start()
port = server.server_address[1]
cores = []
try:
    for name, oauth in SHAPES.items():
        cores.append(core(name, port, oauth))
    # a copy of the account of another shape, written by nobody we know: read as if it were old, the account asked
    cores.append(core("copy_kinds", port, GOOD, {"at": "yesterday", "tried": [1], "usage": {"five_hour": {"utilization": 1}}}))

    deadline = time.time() + 70
    while time.time() < deadline and not all(tried(name) for name in SHAPES):
        for name in SHAPES:
            windows(name)
        time.sleep(1)
    for name in SHAPES:
        check(f"{name}: its try written", tried(name))
        check(f"{name}: the lock free", lock_free(name))
        check(f"{name}: no thread ended", "a thread ended" not in error_text(name), error_text(name)[-200:])
        if name in NO_TOKEN:
            check(f"{name}: no token, nothing sent to the account", hits.get(name, 0) == 0, str(hits.get(name)))
    deadline = time.time() + 70
    while time.time() < deadline and windows("copy_kinds").get("5h") != 42:
        time.sleep(1)
    check("a copy of another shape: read again from the account", windows("copy_kinds").get("5h") == 42 and hits.get("copy_kinds", 0) >= 1, json.dumps(windows("copy_kinds")))
    check("     no thread ended", "a thread ended" not in error_text("copy_kinds"), error_text("copy_kinds")[-200:])

    # the file right again: the account read, no restart
    for name in SHAPES:
        write_credentials(name, GOOD)
    deadline = time.time() + 100
    while time.time() < deadline and not all(windows(name).get("5h") == 42 for name in SHAPES):
        time.sleep(1)
    for name in SHAPES:
        check(f"{name}: the file right again: the account read", windows(name).get("5h") == 42 and hits.get(name, 0) >= 1, f"{windows(name)} {hits.get(name)}")
        check(f"{name}: the lock free at the end", lock_free(name))
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
