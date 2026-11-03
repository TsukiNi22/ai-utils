#!/usr/bin/env python3
# What is imported in a prompt of a terminal takes time when it is big: its popup (over the screen, what it does, its size), the screen alive meanwhile, the same result.
# A paste is never keys: a popup of keys open (the map of this computer, a device asking...) takes nothing of it; the core lost: its letters never quit; an end that never
# comes: the keys are keys again. The clipboard (Ctrl+V, a slow wl-paste, a slow magick): read in the background, its steps, then [Image #N] / [Pasted text #N]. The terminal of
# the page: megabytes pasted at once over its WebSocket, the connection kept.
# Usage: tests/paste_import.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""A big paste, a big image: the popup of the import; a paste is never keys."""
import base64
import fcntl
import http.cookiejar
import json
import os
import pty
import re
import select
import signal
import socket
import stat
import struct
import subprocess
import sys
import tempfile
import termios
import time
import urllib.request

import pyte

T = tempfile.mkdtemp(prefix="cc-import-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 24300 + os.getpid() % 2000
TTY = PORT + 2
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
           HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color", PATH=f"{T}/bin:" + os.environ.get("PATH", ""), CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1),
           CLAUDE_CLUSTER_NO_SUPERVISOR="1")
PNG = bytes.fromhex("89504e470d0a1a0a") + b"\0" * 3000000
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def script(name: str, text: str) -> None:
    with open(f"{T}/bin/{name}", "w", encoding="utf-8") as out:
        out.write("#!/usr/bin/env python3\n" + text)
    os.chmod(f"{T}/bin/{name}", 0o755)

class Term:
    def __init__(self):
        self.ended = False
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], ENV)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 160, 0, 0))
        self.screen = pyte.Screen(160, 50)
        self.stream = pyte.ByteStream(self.screen)
        self.raw = b""

    def pump(self, sec: float) -> None:
        end = time.time() + sec
        while time.time() < end:
            ready, _, _ = select.select([self.fd], [], [], 0.05)
            if ready:
                try:
                    chunk = os.read(self.fd, 1 << 16)
                except OSError:
                    return
                self.raw = (self.raw + chunk)[-200000:]
                self.stream.feed(chunk)

    def text(self) -> str:
        return "\n".join(self.screen.display)

    def alive(self) -> bool:
        if self.ended:
            return False
        self.ended = os.waitpid(self.pid, os.WNOHANG)[0] != 0
        return not self.ended

    def keys(self, data: bytes) -> None:
        os.write(self.fd, data)

    def send_all(self, data: bytes, until: float = 300, marks: tuple = ()) -> dict:
        # big input and the screen read side by side; the first time each mark was seen, with what the screen said then
        seen = {}
        sent = 0
        draws = 0
        os.set_blocking(self.fd, False)
        begin = time.time()
        try:
            while time.time() - begin < until:
                ready, can, _ = select.select([self.fd], [self.fd] if sent < len(data) else [], [], 0.05)
                if ready:
                    chunk = os.read(self.fd, 1 << 16)
                    draws += 1
                    self.stream.feed(chunk)
                if can:
                    try:
                        sent += os.write(self.fd, data[sent:sent + (1 << 14)])
                    except BlockingIOError:
                        pass
                text = self.text()
                for mark in marks:
                    if mark in text and mark not in seen:
                        seen[mark] = (time.time() - begin, text)
                if marks and marks[-1] in seen:
                    break
        finally:
            os.set_blocking(self.fd, True)
        seen["sent"] = sent
        seen["took"] = time.time() - begin
        return seen

    def flood(self, data: bytes, mark: bytes, end: bytes, until: float = 180) -> tuple:
        # written by a process of its own as fast as it can, the output only read (a real terminal is far faster than pyte): when the mark and the end came
        writer = os.fork()
        if writer == 0:
            sent = 0
            while sent < len(data):
                sent += os.write(self.fd, data[sent:sent + (1 << 16)])
            os._exit(0)
        begin = time.time()
        marked = 0.0
        ended = 0.0
        tail = b""
        chunks = []
        while time.time() - begin < until and not ended:
            if select.select([self.fd], [], [], 0.05)[0]:
                chunk = os.read(self.fd, 1 << 20)
                chunks.append(chunk)
                tail = (tail + chunk)[-32768:]
                if not marked and mark in tail:
                    marked = time.time() - begin
                if end in tail:
                    ended = time.time() - begin
        os.waitpid(writer, 0)
        for chunk in chunks:
            self.stream.feed(chunk)
        return marked, ended

    def wait_for(self, mark: str, sec: float) -> bool:
        end = time.time() + sec
        while time.time() < end:
            self.pump(0.2)
            if mark in self.text():
                return True
        return False

for folder in ("cfg", "data", "run", "cache", "home", "work", "bin", "clip"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
# the clipboard of the test: a wl-paste giving what clip/ holds, slowly
script("wl-paste", f"""
import os, sys, time
clip = "{T}/clip"
if "--list-types" in sys.argv:
    print(open(clip + "/types").read())
    sys.exit(0)
data = open(clip + ("/image" if "image/png" in sys.argv else "/text"), "rb").read()
size = len(data) // 15 + 1
for at in range(0, len(data), size):
    os.write(1, data[at:at + size])
    time.sleep(0.1)
""")
# and a slow magick: what it was given as it is
script("magick", """
import shutil, sys, time
time.sleep(1.2)
args = [a for a in sys.argv[1:]]
source = next(a for a in args if a.endswith((".png", ".jpg", ".webp")))
shutil.copyfile(source, args[-1])
""")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nstart_menu = false\n[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\neditor = "vi"\nlanguage = "en"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\ntty_port = {TTY}\nhttps = false\napprove = false\nshell = true\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
term = None
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(1)
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(30)
    s.connect(f"{T}/sock")
    s.sendall((json.dumps({"cmd": "spawn", "cwd": f"{T}/work", "name": "paste"}) + "\n").encode())
    s.recv(1 << 16)
    s.close()
    time.sleep(1)
    term = Term()
    term.pump(4)

    # the map of this computer asked (y / n): a popup of keys
    asked = "machine.md" in term.text()
    check("a popup of keys over the screen", asked, term.text()[-600:])
    term.keys(b"\x1b[200~yes no\x1b[201~")
    term.pump(1.5)
    check("a paste is not its answer: still asked, told", "machine.md" in term.text() and "not taken here" in term.text(), term.text()[-700:])
    term.keys(b"n")
    term.pump(1.5)
    check("the key is", "machine.md" not in term.text(), term.text()[-400:])

    # small: as before, nothing over the screen
    term.keys(b"\x1b[200~small one\x1b[201~")
    term.pump(1)
    check("a small paste in the prompt", "small one" in term.text() and "Importing" not in term.text(), term.text()[-500:])
    term.keys(b"\x7f" * 12)
    term.pump(0.5)

    # big: its popup with its size while it comes, the screen alive, one block at its end
    line = b"the quick brown fox jumps over the lazy dog 0123456789 " * 2 + b"\r"
    big = b"\x1b[200~" + line * (6 * 1024 * 1024 // len(line)) + b"\x1b[201~"
    seen = term.send_all(big, 300, ("Importing the pasted text", "Pasted text #1"))
    print(f"     6 MB pasted in {seen['took']:.1f} s", flush=True)
    check("6 MB: its popup while it comes", "Importing the pasted text" in seen, str(seen.get("took")))
    if "Importing the pasted text" in seen:
        check("     its size told", " so far" in seen["Importing the pasted text"][1] or "MB" in seen["Importing the pasted text"][1], seen["Importing the pasted text"][1][-900:])
    check("6 MB: one block in the prompt", "Pasted text #1" in seen, term.text()[-500:])
    check("the terminal alive", term.alive())
    term.pump(2.5)
    check("its popup gone after", "Importing the pasted text" not in term.text(), term.text()[-900:])

    # faster than it reads (no frame drawn by FTXUI meanwhile): its popup all the same
    term.keys(b"\x03")  # the prompt emptied
    term.pump(1)
    marked, ended = term.flood(b"\x1b[200~" + line * (4 * 1024 * 1024 // len(line)) + b"\x1b[201~", b"so far", b"imported (")
    print(f"     4 MB at full speed in {ended:.1f} s, its popup at {marked:.1f} s", flush=True)
    check("a paste faster than it is read: its popup before its end", 0 < marked < ended, f"{marked} {ended}")
    term.pump(2.5)
    check("     the terminal alive, one block", term.alive() and "Pasted text #1" in term.text(), term.text()[-400:])

    # a block never runs: what it starts with is hidden under its [Pasted text]
    term.keys(b"\x03")  # the prompt emptied
    term.pump(1)
    pwned = f"{T}/pwned"
    term.keys(b"\x1b[200~!touch " + pwned.encode() + b"\r" + b"second\rthird\rfourth\rfifth\x1b[201~")
    term.pump(1)
    check("a block starting with ! is a block", "Pasted text" in term.text(), term.text()[-400:])
    term.keys(b"\r")
    term.pump(3)
    check("     Enter never runs it", not os.path.exists(pwned))
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        check("     it goes to the agent as text", "second" in log.read())
    term.keys(b"!")
    term.pump(0.3)
    term.keys(b"\x1b[200~touch " + pwned.encode() + b"\r" + b"\r\r\r\x1b[201~")
    term.pump(1)
    term.keys(b"\r")
    for _ in range(40):
        term.pump(0.25)
        if os.path.exists(pwned):
            break
    check("     ! typed first, then the block: run", os.path.exists(pwned))
    term.pump(1.5)
    term.keys(b"\x1b")  # its output given to the agent? a popup: not this time
    term.pump(1)

    # Ctrl+V: a big text of the clipboard, read in the background: the screen goes on, its popup
    with open(f"{T}/clip/types", "w", encoding="utf-8") as out:
        out.write("text/plain\n")
    with open(f"{T}/clip/text", "wb") as out:
        out.write(b"clip text line\n" * 200000)
    term.keys(b"\x16")
    popup = term.wait_for("Importing from the clipboard", 8)
    check("Ctrl+V a big text: its popup", popup, term.text()[-900:])
    if popup:
        term.pump(0.5)
        check("     what it reads, how much", "Reading the text" in term.text(), term.text()[-900:])
    check("one more block", term.wait_for("Pasted text #", 30), term.text()[-500:])
    term.pump(2.5)

    # Ctrl+V: a big image: read, converted, stored
    with open(f"{T}/clip/types", "w", encoding="utf-8") as out:
        out.write("image/png\ntext/plain\n")
    with open(f"{T}/clip/image", "wb") as out:
        out.write(PNG)
    term.keys(b"\x16")
    popup = term.wait_for("Importing from the clipboard", 8)
    check("Ctrl+V a big image: its popup", popup, term.text()[-900:])
    steps = set()
    end = time.time() + 30
    while time.time() < end and "Image #1" not in term.text():
        term.pump(0.2)
        for step in ("Reading the image", "Preparing the image", "Storing it"):
            if step in term.text():
                steps.add(step)
    check("     its steps told", "Reading the image" in steps and "Preparing the image" in steps, str(steps))
    check("[Image #1] in the prompt", "Image #1" in term.text(), term.text()[-500:])
    stored = []
    for root, _, files in os.walk(f"{T}/data/attachments"):
        stored += [os.path.join(root, name) for name in files if name.endswith(".png") and not name.startswith(".")]
    check("the image stored, private, whole", len(stored) == 1 and stat.S_IMODE(os.stat(stored[0]).st_mode) == 0o600 and os.path.getsize(stored[0]) == len(PNG), str(stored))
    term.pump(2.5)

    # an image over the limit: told, nothing read to the end
    with open(f"{T}/clip/image", "wb") as out:
        out.write(bytes.fromhex("89504e470d0a1a0a") + b"\0" * (21 * 1024 * 1024))
    term.keys(b"\x16")
    check("an image over the limit: told", term.wait_for("too big", 20), term.text()[-500:])
    term.pump(2.5)

    # an end of paste that never comes: the keys are keys again after 10 s
    term.keys(b"\x1b[200~lost end ")
    term.pump(11.5)
    term.keys(b"typed")
    term.pump(1)
    # (its invisible marks between them: as written to the terminal, pyte stops at them)
    check("an end lost: what came kept, the keys typed", b"lost end typed" in re.sub(rb"\xf3\xa0[\x80-\x81][\x80-\xbf]", b"", term.raw), term.text()[-500:])

    # the terminal of the page: 6 MB at once over its WebSocket, the connection kept, then a ping answered
    jar = http.cookiejar.CookieJar()
    web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    login = urllib.request.Request(f"http://127.0.0.1:{PORT}/api/login", data=json.dumps({"pin": pin, "name": "phone"}).encode(),
                                   headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
    web.open(login, timeout=30).read()
    cookie = "; ".join(f"{c.name}={c.value}" for c in jar)
    ws = socket.create_connection(("127.0.0.1", TTY), timeout=30)
    key = base64.b64encode(os.urandom(16)).decode()
    ws.sendall((f"GET /api/tty/ws?cols=80&rows=24 HTTP/1.1\r\nHost: 127.0.0.1:{TTY}\r\nOrigin: http://127.0.0.1:{TTY}\r\nCookie: {cookie}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                f"Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\n\r\n").encode())
    head = b""
    while b"\r\n\r\n" not in head:
        head += ws.recv(4096)
    upgraded = b" 101 " in head.split(b"\r\n")[0]
    if not upgraded and b" 501 " in head.split(b"\r\n")[0]:
        print("     no WebSocket in this build", flush=True)
    else:
        check("the WebSocket of the terminal", upgraded, head[:200].decode("latin-1"))

        def frame(op: int, payload: bytes) -> bytes:
            mask = b"\x01\x02\x03\x04"
            size = len(payload)
            head = bytes([0x80 | op]) + (bytes([0x80 | size]) if size < 126 else bytes([0x80 | 126]) + size.to_bytes(2, "big") if size < 65536 else bytes([0x80 | 127]) + size.to_bytes(8, "big"))
            return head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload))

        part = (b"x" * 99 + b"\r") * 5120
        burst = b"".join(frame(2, (b"\x1b[200~" if i == 0 else b"") + part) for i in range(12)) + frame(2, b"\x1b[201~") + frame(1, json.dumps({"t": "ping", "n": 7}).encode())
        ws.settimeout(0.2)
        sent = 0
        answer = b""
        end = time.time() + 120
        closed = False
        while time.time() < end and b'"pong"' not in answer and not closed:
            if sent < len(burst):
                try:
                    sent += ws.send(burst[sent:sent + (1 << 20)])
                except TimeoutError:
                    pass
            try:
                got = ws.recv(1 << 20)
                closed = not got
                answer = (answer + got)[-200000:]
            except TimeoutError:
                pass
        check("6 MB at once over the WebSocket: the connection kept, a ping answered", b'"pong"' in answer, f"sent {sent} of {len(burst)}, closed {closed}")
    ws.close()

    # the core lost: its popup takes keys (q quits); what is pasted is not keys
    core.send_signal(signal.SIGKILL)
    core.wait(10)
    term.pump(1)
    term.keys(b"\x1b[200~quit q Q\x1b[201~")
    term.pump(1.5)
    check("the core lost: a paste never quits", term.alive(), term.text()[-300:])
finally:
    if term is not None and term.alive():
        os.kill(term.pid, signal.SIGKILL)
    if core.poll() is None:
        core.terminate()
        try:
            core.wait(15)
        except subprocess.TimeoutExpired:
            core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
