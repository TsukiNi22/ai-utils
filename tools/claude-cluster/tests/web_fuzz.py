#!/usr/bin/env python3
# What an outsider or a broken client sends to the web access: requests cut anywhere, oversized, of made up lengths and methods, paths that leave their folder, frames of a WebSocket
# of any shape, connections dropped in the middle, a device sending its API junk - the core answers or closes, keeps running, serves the next request, and gives no file of outside.
# Usage: tests/web_fuzz.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The web access under malformed input."""
import base64
import http.cookiejar
import json
import os
import random
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request

T = tempfile.mkdtemp(prefix="cc-webfuzz-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 26300 + os.getpid() % 2000
TTY = PORT + 2
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
           HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1), CLAUDE_CLUSTER_NO_SUPERVISOR="1")
RANDOM = random.Random(20261102)
failed = False
jar = http.cookiejar.CookieJar()
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def raw(data: bytes, port: int = PORT, wait: float = 0.3) -> bytes:
    # bytes sent as they are: what came back (nothing is fine)
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=3)
    except OSError:
        return b""
    answer = b""
    try:
        s.sendall(data)
        s.settimeout(wait)
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            answer += chunk
    except OSError:
        pass
    s.close()
    return answer

def alive(port: int = PORT) -> bool:
    # the next request is served
    for _ in range(20):
        answer = raw(f"GET /api/state HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\n\r\n".encode(), port, 1)
        if answer.startswith(b"HTTP/1.1"):
            return True
        time.sleep(0.5)  # busy (503) or a slot being freed
    return False

def post(path: str, body: object) -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
    try:
        with web.open(request, timeout=30) as answer:
            return json.loads(answer.read() or b"{}")
    except urllib.error.HTTPError as e:
        return {"status": e.code}

def get(path: str) -> tuple[int, bytes]:
    try:
        with web.open(f"http://127.0.0.1:{PORT}{path}", timeout=30) as answer:
            return answer.status, answer.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
SECRET = "the secret of the fuzz test"
with open(f"{T}/home/secret.txt", "w", encoding="utf-8") as out:
    out.write(SECRET)
os.makedirs(f"{T}/home/.ssh")
with open(f"{T}/home/.ssh/id_rsa", "w", encoding="utf-8") as out:
    out.write(SECRET)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\ntty_port = {TTY}\nhttps = false\napprove = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)

    # ---- no device: the HTTP of a stranger ----
    host = f"Host: 127.0.0.1:{PORT}\r\n".encode()
    cases = [b"", b"\r\n\r\n", b"GET\r\n\r\n", b"GET /\r\n\r\n", b"GET / HTTP/9.9\r\n\r\n", b"\x00\x01\x02\xff\xfe\r\n\r\n", b"A" * 20000, b"GET /" + b"a" * 20000 + b" HTTP/1.1\r\n\r\n",
             b"GET / HTTP/1.1\r\n" + b"X-Long: " + b"b" * 15000 + b"\r\n\r\n", b"GET / HTTP/1.1\r\n" + b"X: y\r\n" * 4000 + b"\r\n", b"POST /api/login HTTP/1.1\r\nContent-Length: -1\r\n" + host + b"\r\n",
             b"POST /api/login HTTP/1.1\r\nContent-Length: abc\r\n" + host + b"\r\n", b"POST /api/login HTTP/1.1\r\nContent-Length: 99999999999999999999999\r\n" + host + b"\r\n",
             b"POST /api/login HTTP/1.1\r\nContent-Length: 18446744073709551615\r\n" + host + b"\r\n", b"POST /api/login HTTP/1.1\r\nContent-Length: 5\r\n" + host + b"\r\nnot{", b"POST /api/login HTTP/1.1\r\nContent-Length: 4\r\n" + host + b"\r\nnull",
             b"POST /api/login HTTP/1.1\r\nContent-Length: 2\r\n" + host + b"\r\n[]", b"POST /api/login HTTP/1.1\r\nContent-Length: 11\r\n" + host + b"\r\n{\"pin\": 12}", b"POST /api/cmd HTTP/1.1\r\nContent-Length: 5000\r\n" + host + b"\r\n",
             b"GET /api/state?v=abc HTTP/1.1\r\n" + host + b"\r\n", b"GET /api/state?v=-1 HTTP/1.1\r\n" + host + b"\r\n", b"GET /api/state?v=99999999999999999999999 HTTP/1.1\r\n" + host + b"\r\n",
             b"GET /%zz%%%00 HTTP/1.1\r\n" + host + b"\r\n", b"GET /?a=%&b&=&&&=%ff HTTP/1.1\r\n" + host + b"\r\n", b"GET / HTTP/1.1\r\nCookie: ;;;=;cc_web;cc_web=\r\n" + host + b"\r\n", b"GET / HTTP/1.1\r\nHost:\r\n\r\n",
             b"GET / HTTP/1.1\r\nHost: \xff\xfe\r\n\r\n", b"OPTIONS * HTTP/1.1\r\n" + host + b"\r\n", b"DELETE / HTTP/1.1\r\n" + host + b"\r\n", b"PUT /api/cmd HTTP/1.1\r\nContent-Length: 2\r\n" + host + b"\r\n{}", b"GET /../../../../etc/passwd HTTP/1.1\r\n" + host + b"\r\n",
             b"GET /docs/../../../../etc/passwd HTTP/1.1\r\n" + host + b"\r\n", b"GET /docs/%2e%2e/%2e%2e/%2e%2e/etc/passwd HTTP/1.1\r\n" + host + b"\r\n", b"\x16\x03\x01\x02\x00\x01\x00\x01\xfc\x03\x03" + b"\x00" * 64]
    exposed = 0
    for data in cases:
        answer = raw(data, PORT, 0.15)
        exposed += 1 if b"root:x:" in answer else 0
    check(f"{len(cases)} malformed requests: served after", alive(), str(core.poll()))
    check("no file of the system given", exposed == 0)
    # the same bytes cut anywhere, random bytes
    for _ in range(150):
        data = RANDOM.choice(cases)
        cut = data[:RANDOM.randrange(len(data) + 1)] if data else b""
        flip = bytearray(cut)
        for _ in range(min(3, len(flip))):
            flip[RANDOM.randrange(len(flip))] = RANDOM.randrange(256)
        raw(bytes(flip), PORT, 0.01)
    check("150 of them cut and broken: served after", alive(), str(core.poll()))
    # dropped in the middle of a body, at once, many
    threads = []

    def dropped() -> None:
        try:
            s = socket.create_connection(("127.0.0.1", PORT), timeout=3)
            s.sendall(b"POST /api/login HTTP/1.1\r\nContent-Length: 4000\r\n" + host + b"\r\nabc")
            s.close()
        except OSError:
            pass

    for _ in range(40):
        threads.append(threading.Thread(target=dropped))
        threads[-1].start()
    for thread in threads:
        thread.join()
    check("40 clients gone in the middle: served after", alive(), str(core.poll()))

    # ---- a device ----
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    answer = post("/api/login", {"pin": pin, "name": "phone"})
    check("a device connected", any(c.name == "cc_web" for c in jar), json.dumps(answer)[:200])
    session = post("/api/cmd", {"cmd": "spawn", "cwd": f"{T}/work", "name": "s"}).get("data", {}).get("id", "s1")
    time.sleep(1)
    # its API: everything of the wrong kind, on the commands it may use
    junk = [None, 1, -1, 1.5e308, True, "", "x" * 5000, [], {}, [1, [2, [3]]], {"a": {"b": None}}, "../../../etc/passwd", f"{T}/home/.ssh/id_rsa", "\x00\x01\x1b[2J", "é😀"]
    fields = ["session", "text", "prompt", "id", "name", "cwd", "path", "action", "value", "key", "section", "request_id", "behavior", "answers", "index", "item", "display", "kind", "query", "images", "force", "mode", "model",
              "task", "when", "every", "cron", "command", "source", "profile", "version", "target", "limit", "service"]
    commands = ["list", "read", "status", "send", "allow", "interrupt", "unqueue", "rewind", "close", "rename", "describe", "resume", "apply", "mode", "model", "notices", "usage", "task_add", "task_list", "trash", "restore",
                "external", "import", "fork", "transcript", "older", "keep_warm", "supervisor", "ask", "display_act", "display_get", "pin", "unpin", "settings_get", "simple_words", "settings_suggest", "ask_preview", "settings_search",
                "settings_set", "ui_state", "delete", "projects", "agents", "agent_stop", "send_next", "failure", "services", "profiles", "profile", "showcase", "task_act", "orch_state", "job_status", "job_kill", "job_stop", "job_resume",
                "job_prio", "schedule_act", "schedule_add", "schedule_list", "todos", "task_cancel", "schedule_cancel", "schedule_edit", "plugins"]
    bad = 0
    leaked = 0
    for command in commands:
        for _ in range(6):
            body = {"cmd": command}
            for field in RANDOM.sample(fields, RANDOM.randrange(1, 8)):
                body[field] = RANDOM.choice(junk)
            if command in ("close", "delete", "trash"):  # the session it plays on stays
                body["session"] = "nothing"
                body["id"] = "nothing"
            try:
                got = post("/api/cmd", body)
            except (urllib.error.URLError, OSError, ValueError):
                bad += 1
                continue
            leaked += 1 if SECRET in json.dumps(got) else 0
    check(f"{len(commands)} commands with junk: every one answered", bad == 0, f"{bad} not answered")
    check("the core alive", core.poll() is None, str(core.poll()))
    check("served after", alive())
    check("no secret of a hidden file in any answer", leaked == 0)
    # the paths of its files
    outside = [f"{T}/home/.ssh/id_rsa", f"{T}/home/secret.txt", "../../../../etc/passwd", "/etc/passwd", "/proc/self/environ", f"{T}/cfg/config.toml", f"{T}/data/state.json", "%2e%2e/%2e%2e/etc/passwd", ".", "/", ""]
    given = 0
    for path in outside:
        for route in (f"/api/file?session={session}&path={urllib.request.quote(path)}", f"/api/attach/file?session={session}&path={urllib.request.quote(path)}"):
            code, data = get(route)
            given += 1 if code == 200 and (SECRET.encode() in data or b"root:x:" in data or b"[web]" in data) else 0
    check("no file of outside by its paths", given == 0, str(given))
    # names of a file sent: stays in its folder, never hidden
    names = ["../../../../evil.txt", "..", ".", "/etc/passwd", ".hidden", "a/b/c.txt", "x" * 500, "é ü .txt", "..\\..\\evil", "\x00.txt", ""]
    for name in names:
        request = urllib.request.Request(f"http://127.0.0.1:{PORT}/api/attach?session={session}&name={urllib.request.quote(name)}", data=b"hello", headers={"Origin": f"http://127.0.0.1:{PORT}"})
        try:
            web.open(request, timeout=30).read()
        except urllib.error.HTTPError:
            pass
    strays = [path for path in ("evil.txt", "data/evil.txt", "data/attachments/evil.txt", "home/evil.txt", "evil") if os.path.exists(f"{T}/{path}")]
    folder = f"{T}/data/attachments/{session}"
    inside = os.listdir(folder) if os.path.isdir(folder) else []
    check("the names of its files stay in their folder", not strays and all("/" not in name and not name.startswith(".") for name in inside) and len(inside) > 0, str(strays) + str(inside)[:300])

    # ---- the WebSocket of its terminal ----
    cookie = "; ".join(f"{c.name}={c.value}" for c in jar)

    def ws(frames: bytes, mark: bytes = b"") -> bytes:
        s = socket.create_connection(("127.0.0.1", TTY), timeout=5)
        key = base64.b64encode(os.urandom(16)).decode()
        s.sendall((f"GET /api/tty/ws?cols=80&rows=24 HTTP/1.1\r\nHost: 127.0.0.1:{TTY}\r\nOrigin: http://127.0.0.1:{TTY}\r\nCookie: {cookie}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                   f"Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\n\r\n").encode())
        answer = b""
        try:
            while b"\r\n\r\n" not in answer: # its answer first (what comes with the request is not read as frames)
                chunk = s.recv(65536)
                if not chunk:
                    break
                answer += chunk
            s.settimeout(10 if mark else 0.4)
            s.sendall(frames)
            while len(answer) < 200000 and not (mark and mark in answer):
                chunk = s.recv(65536)
                if not chunk:
                    break
                answer += chunk
        except OSError:
            pass
        s.close()
        return answer

    def frame(first: int, payload: bytes, masked: bool = True, size: int = -1) -> bytes:
        length = len(payload) if size < 0 else size
        mask = b"\x11\x22\x33\x44"
        head = bytes([first])
        bit = 0x80 if masked else 0
        if length < 126:
            head += bytes([bit | length])
        elif length < 65536:
            head += bytes([bit | 126]) + struct.pack(">H", length)
        else:
            head += bytes([bit | 127]) + struct.pack(">Q", length)
        return head + (mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)) if masked else payload)

    shapes = [frame(0x81, b"not json"), frame(0x81, b"{}"), frame(0x81, b'{"t": 5}'), frame(0x81, b'{"t": "resize", "cols": -5, "rows": 99999999}'), frame(0x81, b'{"t": "resize", "cols": "x", "rows": []}'),
              frame(0x81, b'{"t": "ping", "n": "x"}'), frame(0x81, b'{"t": "full"}'), frame(0x82, b"\x1b[2J\x1b]0;x\x07"), frame(0x81, b"x", masked=False), frame(0xF1, b"x"), frame(0x83, b"x"), frame(0x88, b""), frame(0x88, b"\x03"),
              frame(0x89, b"p" * 125), frame(0x89, b"p" * 300), frame(0x8A, b""), frame(0x01, b"a") + frame(0x00, b"b") + frame(0x80, b"c"), frame(0x00, b"lone"), frame(0x80, b"lone"), frame(0x01, b"a" * 900000) + frame(0x00, b"b" * 900000),
              bytes([0x82, 0xFF]) + struct.pack(">Q", 2 ** 63) + b"\x00\x00\x00\x00", bytes([0x82, 0xFF]) + struct.pack(">Q", 2 ** 64 - 1), bytes([0x82, 0xFE, 0xFF, 0xFF]) + b"\x00\x00\x00\x00", bytes([0x82, 0xFF]) + struct.pack(">Q", 1000001),
              bytes(RANDOM.randrange(256) for _ in range(4096))]
    for shape in shapes:
        ws(shape)
        if core.poll() is not None:
            break
    check(f"{len(shapes)} frames of any shape: the core alive", core.poll() is None, str(core.poll()))
    check("served after", alive())
    answer = ws(frame(0x81, json.dumps({"t": "ping", "n": 3}).encode()), b"pong")
    check("its terminal still answers a ping", b"pong" in answer, answer[:200].decode("latin-1"))
    post("/api/tty/close", {})
finally:
    code = core.poll()
    if code is None:
        core.terminate()
        try:
            code = core.wait(30)
        except subprocess.TimeoutExpired:
            core.kill()
            code = "killed"
    print("     the end of the core:", code, flush=True)
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
