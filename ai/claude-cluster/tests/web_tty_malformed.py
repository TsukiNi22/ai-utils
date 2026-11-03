#!/usr/bin/env python3
# A message of the WebSocket of the terminal page with a field of the wrong type ({"t": 1}, {"t": "resize", "cols": "x"}...): ignored, the connection goes on and speaks WebSocket only, its eventfd given
# back when it ends (it left its loop by an exception, an HTTP 500 written in the middle of its frames: one descriptor and one reference to the terminal kept for ever per message, the core out of
# descriptors after a thousand). The same fields of the routes of the polling mode: their default.
# Usage: tests/web_tty_malformed.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The eventfds of the WebSockets of the terminal page."""
import base64
import http.cookiejar
import json
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.request

T = tempfile.mkdtemp(prefix="cc-ttymalformed-")
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
TTY = PORT + 2
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

def post(port: int, path: str, body: object) -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{port}"})
    with web.open(request, timeout=30) as answer:
        return json.loads(answer.read() or b"{}")

class Ws:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", TTY), timeout=10)
        key = base64.b64encode(os.urandom(16)).decode()
        cookie = "; ".join(f"{c.name}={c.value}" for c in jar)
        self.s.sendall((f"GET /api/tty/ws?cols=100&rows=30 HTTP/1.1\r\nHost: 127.0.0.1:{TTY}\r\nOrigin: http://127.0.0.1:{TTY}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                        f"Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\nCookie: {cookie}\r\n\r\n").encode())
        data = b""
        while b"\r\n\r\n" not in data:
            chunk = self.s.recv(4096)
            if not chunk:
                break
            data += chunk
        self.head, self.rest = data.split(b"\r\n\r\n", 1) if b"\r\n\r\n" in data else (data, b"")
        self.raw = b""

    def send(self, opcode: int, payload: bytes) -> None:
        mask = os.urandom(4)
        head = bytes([0x80 | opcode])
        if len(payload) < 126:
            head += bytes([0x80 | len(payload)])
        else:
            head += bytes([0x80 | 126]) + struct.pack(">H", len(payload))
        try:
            self.s.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))
        except OSError:
            pass  # closed by the core: frames() tells it

    def frames(self, seconds: float, until: bytes = b"") -> tuple:
        # the frames read for a time (or up to the one holding `until`), its connection closed or not; what is not a frame stays in self.raw
        self.s.settimeout(0.1)
        end = time.time() + seconds
        out = []
        closed = False
        buf = self.rest
        self.rest = b""
        while time.time() < end and not closed:
            try:
                chunk = self.s.recv(1 << 20)
                if not chunk:
                    closed = True
                buf += chunk
                self.raw += chunk
            except socket.timeout:
                pass
            except OSError:
                closed = True
            while len(buf) >= 2:
                size = buf[1] & 0x7F
                at = 2
                if size == 126:
                    if len(buf) < 4:
                        break
                    size = struct.unpack(">H", buf[2:4])[0]
                    at = 4
                elif size == 127:
                    if len(buf) < 10:
                        break
                    size = struct.unpack(">Q", buf[2:10])[0]
                    at = 10
                if len(buf) < at + size:
                    break
                out.append((buf[0] & 0x0F, buf[at:at + size]))
                buf = buf[at + size:]
            if until and any(until in data for _, data in out):
                break
        self.rest = buf
        return out, closed

    def close(self) -> None:
        self.s.close()

def eventfds() -> int:
    count = 0
    for fd in os.listdir(f"/proc/{core.pid}/fd"):
        try:
            count += 1 if "eventfd" in os.readlink(f"/proc/{core.pid}/fd/{fd}") else 0
        except OSError:
            pass
    return count

def descriptors() -> int:
    return len(os.listdir(f"/proc/{core.pid}/fd"))

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\ntty_port = {TTY}\nhttps = false\napprove = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post(PORT, "/api/login", {"pin": pin, "name": "phone"})
    page = Ws()
    if b" 501 " in page.head.split(b"\r\n")[0]:
        print("ok   no WebSocket in this build: not run", flush=True)
        raise SystemExit
    check("the WebSocket of the terminal", b" 101 " in page.head.split(b"\r\n")[0], page.head[:200].decode("latin-1"))
    page.frames(1.5)
    check("one eventfd while it is open", eventfds() >= 1, str(eventfds()))
    page.close()
    time.sleep(1.5)
    base = eventfds()
    total = descriptors()

    junk = [b'{"t":1}', b'{"t":null}', b'{"t":{}}', b'{"t":[]}', b'{"t":"resize","cols":"x"}', b'{"t":"resize","rows":[]}', b'{"t":"resize","cols":{},"rows":null}', b'{"t":"resize","cols":1e300,"rows":-1e300}', b'{"t":"ping","n":"x"}',
            b'{"t":"ping","n":{}}', b'{"t":"ping","n":null}', b"[]", b"1", b'"x"', b"null", b"not json", b"{}"]
    # one connection, every message: it goes on and answers
    page = Ws()
    page.frames(0.3)
    for payload in junk:
        page.send(0x1, payload)
    page.send(0x1, json.dumps({"t": "ping", "n": 3}).encode())
    frames, closed = page.frames(5, b'"n":3')
    pong = [json.loads(data) for op, data in frames if op == 1 and b'"pong"' in data]
    numbered = [p for p in pong if p.get("n") == 3]
    check(f"{len(junk)} messages of the wrong shape: ignored, its ping still answered", bool(numbered) and not closed, str(frames[-2:])[:300])
    check("     nothing but frames on its connection", b"HTTP/1.1" not in page.raw and not any(op == 8 for op, _ in frames), page.raw[:200].decode("latin-1"))
    page.send(0x1, json.dumps({"t": "resize", "cols": 90, "rows": 25}).encode())
    frames, closed = page.frames(5, b'"cols":90')
    check("     a real resize still done", any(b'"cols":90' in data and b'"rows":25' in data for op, data in frames if op == 1), str(frames[-1:])[:300])
    page.close()
    time.sleep(0.5)

    # one connection per message, again and again: no descriptor left (each one a descriptor kept before)
    tries = 0
    refused = 0
    for _ in range(3):
        for payload in junk[:11]:
            page = Ws()
            refused += 0 if b" 101 " in page.head.split(b"\r\n")[0] else 1
            page.send(0x1, payload)
            page.frames(0.05)
            page.close()
            tries += 1
            time.sleep(0.02)
    time.sleep(2)
    check(f"{tries} connections, each a message of the wrong type: all open", refused == 0, str(refused))
    check("     no eventfd left", eventfds() <= base + 2, f"{eventfds()} after them, {base} before")
    check("     no descriptor left", descriptors() <= total + 5, f"{descriptors()} after them, {total} before")

    # its round trip told by the page: a number of any size, or none (a sanitizer build tells a number no int holds)
    for rtt in (1e300, -1e300, 2 ** 70, "x", None, [], 5):
        check(f"/api/ping with {json.dumps(rtt)}: answered", post(PORT, "/api/ping", {"rtt": rtt}).get("ok") is True)
    # the routes of the polling mode: the same fields
    for body in ([], {"cols": "x", "rows": []}, {"cols": 1e300, "rows": -1e300}, {"cols": None}):
        check(f"/api/tty/resize {json.dumps(body)}: answered", post(TTY, "/api/tty/resize", body).get("ok") is True)
    opened = post(TTY, "/api/tty/open", {"cols": "x"})
    check("/api/tty/open with a size of the wrong type: its default", opened.get("cols") == 120, json.dumps(opened)[:200])
    post(TTY, "/api/tty/close", {})
    check("the core alive at the end", core.poll() is None, str(core.poll()))
except SystemExit:
    pass
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
