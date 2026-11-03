#!/usr/bin/env python3
# A paste in a text field of the terminal: put there at once, whole, its filter run once (never replayed key by key: no frame until its end, a search of the transcript per
# character); never a key of what is over or behind it (the core lost, a failure popup); in the chat of a question it goes to the chat, keeping its lines, never to the answer; on
# the review of the questions nothing changes; a path dropped on the sources of the plugins opens the field with it. A click after a popup closed is not eaten by its old field.
# Usage: tests/paste_fields.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""A paste in a text field: at once, in that field, nothing else."""
import fcntl
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

import pyte

ROOT = tempfile.mkdtemp(prefix="cc-pastefields-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
failed = False
used = 0

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

class Term:
    def __init__(self, env: dict) -> None:
        self.ended = False
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], env)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 160, 0, 0))
        self.screen = pyte.Screen(160, 50)
        self.stream = pyte.ByteStream(self.screen)

    def pump(self, sec: float) -> None:
        end = time.time() + sec
        while time.time() < end:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    chunk = os.read(self.fd, 1 << 16)
                except OSError:
                    return
                try:
                    self.stream.feed(chunk)
                except TypeError:
                    pass

    def first_frame(self) -> None:
        # the interface drawn (a build with sanitizers on a busy machine: seconds), the map of this computer asked at its start answered
        end = time.time() + 40
        while time.time() < end and "claude-cluster" not in self.text():
            self.pump(0.2)
        self.pump(1.5)
        if "asked again at the next start" in self.text():
            self.send(b"n", 1.5)

    def text(self) -> str:
        return "\n".join(line.rstrip() for line in self.screen.display)

    def send(self, data: bytes, wait: float = 0.6) -> None:
        os.write(self.fd, data)
        self.pump(wait)

    def paste(self, text: str, wait: float = 1.0) -> None:
        self.send(b"\x1b[200~" + text.encode() + b"\x1b[201~", wait)

    def alive(self) -> bool:
        if self.ended:
            return False
        self.ended = os.waitpid(self.pid, os.WNOHANG)[0] != 0
        return not self.ended

    def wait_for(self, word: str, sec: float) -> float:
        began = time.time()
        while time.time() - began < sec:
            self.pump(0.1)
            if word in self.text():
                return time.time() - began
        return -1.0

    def kill(self) -> None:
        if self.alive():
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
            self.ended = True

class Box:
    def __init__(self, extra: str = "") -> None:
        global used
        used += 1
        self.T = f"{ROOT}/{used}"
        T = self.T
        self.env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
        self.env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
                        HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color", CLAUDE_CLUSTER_NO_SUPERVISOR="1")
        for folder in ("cfg", "data", "run", "cache", "home", "work"):
            os.makedirs(f"{T}/{folder}", mode=0o700)
        open(f"{T}/fake.log", "w", encoding="utf-8").close()
        with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
            config.write('[global]\nenabled = false\n[ui]\nnotify = false\nstart_menu = false\nmachine_days = 0\n[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\neditor = "vi"\nlanguage = "en"\n'
                         f'[usage]\naccount = false\n[commands]\nclaude = "{FAKE}"\n' + extra)
        self.core = subprocess.Popen([BIN, "serve", "--no-global"], env=self.env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if os.path.exists(f"{T}/sock"):
                break
            time.sleep(0.1)
        time.sleep(0.5)
        self.term = None

    def ask(self, request: dict) -> dict:
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(60)
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

    def terminal(self, wait: float = 4) -> Term:
        self.term = Term(self.env)
        self.term.pump(wait)
        self.term.first_frame()
        return self.term

    def close(self) -> None:
        if self.term is not None:
            self.term.kill()
        if self.core.poll() is None:
            self.core.terminate()
            try:
                self.core.wait(15)
            except subprocess.TimeoutExpired:
                self.core.kill()
        os.system(f"pkill -f {self.T} > /dev/null 2>&1")

boxes = []
try:
    # the time of a paste in a field: measured on a transcript of 2 MB, its search lower-casing it for each character when replayed
    box = Box()
    boxes.append(box)
    sid = box.ask({"cmd": "spawn", "cwd": f"{box.T}/work", "name": "fields"}).get("data", {}).get("id", "s1")
    big = "lorem ipsum dolor sit amet " * 800
    for i in range(60):
        box.ask({"cmd": "send", "session": sid, "text": f"{i} {big}"})
    time.sleep(2)
    term = box.terminal(5)
    for name, keys in (("the palette", b"\x0b"), ("the search of the transcript", b"\x06"), ("the history", b"\x12"), ("the search of the settings", b"\x1b[20~/")):
        term.send(keys, 2.0)
        began = time.time()
        os.write(term.fd, b"\x1b[200~" + b"zq" * 1000 + b"\x1b[201~")
        shown = term.wait_for("zqzqzq", 60)
        check(f"2000 characters pasted in {name}: shown at once", 0 <= shown < 12, f"{time.time() - began:.1f} s")
        term.send(b"\x1b", 1.0)
        if keys.startswith(b"\x1b[20"):
            term.send(b"\x1b", 1.0)
    # its filter ran on the whole text; line breaks are spaces, control characters not keys
    term.send(b"\x06", 1.0)
    term.paste("lorem\nipsum\x16\x03\x05 dolor")
    for _ in range(100):  # a build with sanitizers, the 20 KB above in the transcript
        if "no match" not in term.text() and "find › lorem ipsum dolor" in term.text():
            break
        term.pump(0.3)
    check("in the search: one line, its filter run (a match)", "no match" not in term.text() and "find › lorem ipsum dolor" in term.text(), term.text()[-300:])
    term.send(b"\x1b", 1.0)
    box.close()

    # a form: its number field takes digits
    box = Box()
    boxes.append(box)
    sid = box.ask({"cmd": "spawn", "cwd": f"{box.T}/work", "name": "form"}).get("data", {}).get("id", "s1")
    term = box.terminal()
    answer = box.ask({"cmd": "display", "session": sid, "display": {"type": "form", "title": "the form", "fields": [{"name": "age", "label": "age", "type": "number"}, {"name": "who", "label": "who", "type": "text"}]}})
    check("a form made", answer.get("ok") is True, json.dumps(answer)[:200])
    term.pump(2)
    x = next((i for i, line in enumerate(term.screen.display) if "Fill the form" in line), -1)
    column = term.screen.display[x].find("Fill the form") if x >= 0 else 0
    check("     drawn", x >= 0, term.text()[-500:])
    term.send(b"\x1b[<0;%d;%dM\x1b[<0;%d;%dm" % (column + 2, x + 1, column + 2, x + 1), 1.5)
    term.paste("12a3b.5")
    check("a number field of a form: its digits only", "123.5" in term.text() and "12a3b" not in term.text(), term.text()[-600:])
    term.send(b"\t", 0.5)
    term.paste("some body")
    check("     a text field: the whole text", "some body" in term.text(), term.text()[-600:])
    term.send(b"\x1b", 1.0)
    box.close()

    # a click where the field of a closed popup was: for what is there now
    box = Box()
    boxes.append(box)
    box.ask({"cmd": "spawn", "cwd": f"{box.T}/work", "name": "click"})
    term = box.terminal()

    def where(word: str) -> tuple:
        for y, line in enumerate(term.screen.display):
            x = line.find(word)
            if x >= 0:
                return x, y
        return -1, -1

    term.send(b"abcdefghij", 0.6)
    term.send(b"\x06", 0.8)  # Ctrl+F: the search field, on the row of the prompt
    term.send(b"abc", 0.6)
    term.send(b"\x1b", 1.0)
    x, y = where("abcdefghij")
    term.send(b"\x1b[<0;%d;%dM\x1b[<0;%d;%dm" % (x + 9, y + 1, x + 9, y + 1), 0.6)
    term.send(b"Y", 0.8)
    check("a click after a popup closed: the cursor of the prompt placed", "abcdefghYij" in term.text(), [line.strip()[:40] for line in term.text().split("\n") if "abc" in line].__str__())
    box.close()

    # the core lost, a field open: its popup keeps its keys, the paste is no key (q quits, r retries)
    box = Box()
    boxes.append(box)
    box.ask({"cmd": "spawn", "cwd": f"{box.T}/work", "name": "lost"})
    term = box.terminal()
    term.send(b"\x06", 1.0)
    box.core.send_signal(signal.SIGKILL)
    box.core.wait(10)
    check("the core lost: its popup", term.wait_for("Reconnecting", 40) >= 0, term.text()[-500:])
    term.paste("unique request", 0.2)
    told = term.wait_for("not taken here", 5) >= 0
    check("a paste with a field open: the terminal alive", term.alive(), term.text()[-300:])
    check("     refused, told", told or "Reconnecting" not in term.text(), term.text()[-400:])  # the core back meanwhile: the popup gone, nothing to refuse
    check("     nothing behind the popup", "unique" not in term.text(), term.text()[-400:])
    box.close()

    # the chat of a question, its answer, its review
    box = Box("[sessions]\nquestions_to_global = false\n")
    boxes.append(box)
    term = box.terminal(4)
    box.ask({"cmd": "showcase", "what": "questions", "platform": "tty"})
    term.pump(3)
    for _ in range(4):
        if "Other" in term.text() or "Type something" in term.text():
            break
        term.send(b"\x1bn", 1.5)  # Alt+N: the session asking
    check("the question popup", "Other" in term.text() or "Type something" in term.text(), term.text()[-800:])
    other = (box.ask({"cmd": "ask"}).get("data") or {}).get("other", "")
    term.send(b"\x14", 0.8)  # Ctrl+T: its chat
    term.paste("pasted-for-the-chat\nsecond-line")
    data = box.ask({"cmd": "ask"}).get("data") or {}
    check("a paste in the chat: not the answer", data.get("other", "") == other, json.dumps(data)[:300])
    check("     in the chat field, its lines kept", "pasted-for-the-chat" in term.text() and "second-line" in term.text(), term.text()[-900:])
    term.send(b"\x1b", 0.8)  # back to the options
    term.paste("my typed answer")
    data = box.ask({"cmd": "ask"}).get("data") or {}
    check("out of the chat: the answer (Other)", data.get("other", "") == "my typed answer", json.dumps(data)[:300])
    # the review: every answer given, its Submit
    for _ in range(12):
        data = box.ask({"cmd": "ask"}).get("data") or {}
        if data.get("review"):
            break
        term.send(b"\t", 0.6)
    data = box.ask({"cmd": "ask"}).get("data") or {}
    if data.get("review"):
        answers = json.dumps(data.get("answers"))
        other = data.get("other")
        term.paste("changed-by-a-paste", 1.2)
        data = box.ask({"cmd": "ask"}).get("data") or {}
        check("a paste on the review: its answers kept, told", json.dumps(data.get("answers")) == answers and data.get("other") == other and "not taken here" in term.text(), json.dumps(data)[:400])
    else:
        print("     no review in this showcase: not checked", flush=True)
    box.close()

    # a folder dropped on the sources of the plugins: the field with it, nothing added before Enter
    box = Box()
    boxes.append(box)
    box.ask({"cmd": "spawn", "cwd": f"{box.T}/work", "name": "plugins"})
    term = box.terminal()
    term.send(b"/plugins", 0.4)
    term.send(b"\x1b", 0.4)
    term.send(b"\r", 1.5)
    check("the plugins page", "Plugins and skills" in term.text(), term.text()[:400])
    term.paste("/tmp/some-skills-folder", 1.2)
    check("a path pasted on its sources: in its field", "source › /tmp/some-skills-folder" in term.text() and "not taken" not in term.text(), term.text()[-500:])
    term.send(b"\x1b", 0.8)
    term.paste("https://example.invalid/skills.git", 1.2)
    check("     a link too", "source › https://example.invalid/skills.git" in term.text(), term.text()[-500:])
    term.send(b"\x1b", 0.8)
    box.close()
finally:
    for box in boxes:
        box.close()
    os.system(f"pkill -f {ROOT} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
