#!/usr/bin/env python3
# A body sent slowly by a connected device is read to its end whatever its size (under 1 MB it had 10 s for everything, over it 60 s and more: a phone at 60 KB/s lost a prompt or a file of 600 KB to 1 MB and
# sent one of 1.1 MB); a stranger still has its 10 s for a few KB, and nothing over them.
# Usage: tests/web_slow_upload.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The time given to a slow upload."""
import http.cookiejar
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request

T = tempfile.mkdtemp(prefix="cc-slowupload-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")

def free_ports() -> int:
    # 3 ports in a row free here, under the range a client of this computer takes its port from (one taken: that port of the core not opened)
    for base in range(10000 + os.getpid() % 7000, 17100, 3):
        try:
            with socket.socket() as first, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as second, socket.socket() as third:
                first.bind(("", base))
                second.bind(("", base + 1))
                third.bind(("", base + 2))
            return base
        except OSError:
            continue
    return 17100

PORT = free_ports()
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
           HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1), CLAUDE_CLUSTER_NO_SUPERVISOR="1")
failed = False
jar = http.cookiejar.CookieJar()
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def post(path: str, body: dict) -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
    with web.open(request, timeout=30) as answer:
        return json.loads(answer.read() or b"{}")

def upload(path: str, body: bytes, rate: int, cookie: str) -> dict:
    # its body by pieces, 10 per second: how far it went, the JSON of its answer
    s = socket.create_connection(("127.0.0.1", PORT), timeout=30)
    s.sendall((f"POST {path} HTTP/1.1\r\nHost: 127.0.0.1:{PORT}\r\nOrigin: http://127.0.0.1:{PORT}\r\nContent-Type: application/json\r\nCookie: {cookie}\r\nContent-Length: {len(body)}\r\n\r\n").encode())
    start = time.time()
    sent = 0
    step = max(1, rate // 10)
    answer = b""
    try:
        while sent < len(body):
            s.sendall(body[sent:sent + step])
            sent += len(body[sent:sent + step])
            time.sleep(0.1)
        while True:
            data = s.recv(65536)
            if not data:
                break
            answer += data
    except OSError as error:
        return {"sent": sent, "after": round(time.time() - start, 1), "error": str(error)}
    finally:
        s.close()
    parts = answer.split(b"\r\n\r\n", 1)
    return {"sent": sent, "after": round(time.time() - start, 1), "status": parts[0].split(b"\r\n")[0].decode("latin-1"), "body": json.loads(parts[1]) if len(parts) > 1 and parts[1].startswith(b"{") else {}}

def stranger(size: int, first: int) -> tuple:
    # a few bytes of its body, then a drop every second (its socket reads never time out, only the 10 s of its request end it); nothing for a body it may not send: what it gets, how soon
    s = socket.create_connection(("127.0.0.1", PORT), timeout=30)
    s.sendall((f"POST /api/login HTTP/1.1\r\nHost: 127.0.0.1:{PORT}\r\nOrigin: http://127.0.0.1:{PORT}\r\nContent-Type: application/json\r\nContent-Length: {size}\r\n\r\n").encode() + b"x" * first)
    start = time.time()
    s.settimeout(1)
    answer = b""
    try:
        while time.time() - start < 30:
            try:
                data = s.recv(65536)
            except socket.timeout:
                s.sendall(b"x")
                continue
            if not data:
                break
            answer += data
    except OSError:
        pass
    s.close()
    return answer, time.time() - start

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post("/api/login", {"pin": pin, "name": "phone"})
    session = (post("/api/cmd", {"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "s1")
    time.sleep(1)
    cookie = "; ".join(f"{c.name}={c.value}" for c in jar)
    # a device connected: a file under 1 MB, one over it, a long prompt: 60 KB/s; 5 KB at 300 B/s: over 10 s each
    prompt = json.dumps({"cmd": "list", "x": "y" * 890000}).encode()
    uploads = {"990 KB": (f"/api/attach?session={session}&name=small.bin", b"x" * 990000, 60000), "1.1 MB": (f"/api/attach?session={session}&name=big.bin", b"x" * 1100000, 60000),
               "5 KB": (f"/api/attach?session={session}&name=tiny.bin", b"x" * 5000, 300), "a prompt of 890 KB": ("/api/cmd", prompt, 60000)}
    results = {}
    threads = [threading.Thread(target=lambda n=name, u=item: results.__setitem__(n, upload(u[0], u[1], u[2], cookie))) for name, item in uploads.items()]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    for name, (path, body, rate) in uploads.items():
        got = results.get(name, {})
        check(f"{name} at {rate / 1000:.1f} KB/s: read to its end, answered", got.get("sent") == len(body) and got.get("body", {}).get("ok") is True, json.dumps(got)[:300])
        if "name=" in path:
            stored = got.get("body", {}).get("attachment", {}).get("path", "")
            check(f"     {name} on its disk, whole", bool(stored) and os.path.exists(stored) and os.path.getsize(stored) == len(body), stored)
    # a stranger: its few KB in its 10 s, however it keeps sending
    answer, took = stranger(4096, 10)
    check("a stranger's 4 KB sent drop by drop: cut after its 10 s", 8 <= took <= 20 and b" 200 " not in answer.split(b"\r\n")[0], f"{took:.1f} s {answer[:100]}")
    # more than that: refused before its body, at once
    answer, took = stranger(5000, 0)
    check("a stranger's 5 KB: refused before its body", took < 3 and b" 401 " in answer.split(b"\r\n")[0], f"{took:.1f} s {answer[:100]}")
    check("the core alive at the end", core.poll() is None, str(core.poll()))
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
