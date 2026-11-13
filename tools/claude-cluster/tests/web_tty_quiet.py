#!/usr/bin/env python3
# The terminal page left open on a screen that does not change: its terminal stays (it was closed after 2 minutes, its WebSocket pinging or not, "the terminal quit": only a frame sent counted as read), and a
# terminal nobody reads any more (its WebSocket closed) is still closed after its 2 minutes. Two minutes and more of real time.
# Usage: tests/web_tty_quiet.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The terminal of a page open on a quiet screen."""
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

T = tempfile.mkdtemp(prefix="cc-ttyquiet-")
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

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

class Device:
    # a browser: its cookies
    def __init__(self):
        self.jar = http.cookiejar.CookieJar()
        self.web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.jar))

    def post(self, port: int, path: str, body: dict) -> dict:
        request = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{port}"})
        with self.web.open(request, timeout=30) as answer:
            return json.loads(answer.read() or b"{}")

    def read(self) -> dict:
        with self.web.open(f"http://127.0.0.1:{TTY}/api/tty/read?from=0", timeout=30) as answer:
            return json.loads(answer.read() or b"{}")

    def cookie(self) -> str:
        return "; ".join(f"{c.name}={c.value}" for c in self.jar)

class Ws:
    def __init__(self, device: Device):
        self.s = socket.create_connection(("127.0.0.1", TTY), timeout=10)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall((f"GET /api/tty/ws?cols=100&rows=30 HTTP/1.1\r\nHost: 127.0.0.1:{TTY}\r\nOrigin: http://127.0.0.1:{TTY}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                        f"Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\nCookie: {device.cookie()}\r\n\r\n").encode())
        data = b""
        while b"\r\n\r\n" not in data:
            chunk = self.s.recv(4096)
            if not chunk:
                break
            data += chunk
        self.head, self.rest = data.split(b"\r\n\r\n", 1) if b"\r\n\r\n" in data else (data, b"")

    def send(self, opcode: int, payload: bytes) -> None:
        mask = os.urandom(4)
        try:
            self.s.sendall(bytes([0x80 | opcode, 0x80 | len(payload)]) + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))
        except OSError:
            pass  # closed by the core: frames() tells it

    def frames(self, seconds: float) -> tuple:
        self.s.settimeout(0.2)
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
        self.rest = buf
        return out, closed

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
    open_page = Device()
    left = Device()
    open_page.post(PORT, "/api/login", {"pin": pin, "name": "open"})
    left.post(PORT, "/api/login", {"pin": pin, "name": "left"})
    page = Ws(open_page)
    if b" 501 " in page.head.split(b"\r\n")[0]:
        print("ok   no WebSocket in this build: not run", flush=True)
        raise SystemExit
    check("the WebSocket of the page open", b" 101 " in page.head.split(b"\r\n")[0], page.head[:200].decode("latin-1"))
    # the other device: its terminal opened, its page closed; connected all along (its pings), nobody reads its terminal
    gone = Ws(left)
    gone.frames(1)
    gone.s.close()
    start = time.time()
    last_frame = start
    pinged = 0.0
    ending = ""
    # the time for its 2 minutes after its last frame, its expiry checked every 10 s, closing a terminal: 2 s
    while not ending and time.time() - max(start, last_frame) < 135 and time.time() - start < 300:
        frames, closed = page.frames(5)
        if any(op == 1 and b'"alive":false' in data for op, data in frames):
            ending = "its terminal told gone"
        elif any(op == 1 and data.startswith(b'{"') and b'"t":"f"' in data for op, data in frames):
            last_frame = time.time()
        if any(op == 8 for op, _ in frames) or closed:
            ending = ending or "its connection closed"
        if not ending:
            page.send(0x1, json.dumps({"t": "ping", "n": 1}).encode())
        if time.time() - pinged >= 10:
            pinged = time.time()
            left.post(PORT, "/api/ping", {"rtt": 1})
    took = time.time() - start
    quiet = time.time() - last_frame
    print(f"     {took:.0f} s, its screen quiet for the last {quiet:.0f} s", flush=True)
    check("a page open on a screen that does not change: its terminal kept", not ending, f"{ending} after {took:.0f} s, {quiet:.0f} s after its last frame")
    if quiet < 125 and not ending:
        print("     its screen was never quiet for 2 minutes: the expiry not really met", flush=True)
    if not ending:
        page.send(0x1, json.dumps({"t": "ping", "n": 2}).encode())
        frames, _ = page.frames(2)
        check("     its WebSocket still answers", any(op == 1 and b'"pong"' in data for op, data in frames), str(frames)[:200])
        state = open_page.read()
        check("     its terminal alive", state.get("alive") is True and not state.get("none"), json.dumps(state)[:200])
    state = left.read()
    check("a terminal nobody reads: still closed after its 2 minutes", state.get("alive") is False and state.get("none") is True, json.dumps(state)[:200])
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
