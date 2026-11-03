#!/usr/bin/env python3
# The prompt whatever is typed in it: ": " and ":\n" (no word after the colon) never bring the terminal or the window down; a label or a character of an atom of an earlier prompt
# in a text ("[Pasted text #1 +5 lines]" quoted, U+F0000 pasted) stays a text, never that old paste sent to another session; vim: e and the j / k columns never cut a character in
# two (what is sent stays UTF-8), a count of any size answers at once.
# Usage: tests/prompt_edit_shapes.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The prompt: the colon alone, the atoms of earlier prompts, the vim motions and counts."""
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

ROOT = tempfile.mkdtemp(prefix="cc-promptshapes-")
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
        self.raw = b""
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
                self.raw = (self.raw + chunk)[-300000:]
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

    def alive(self) -> bool:
        if self.ended:
            return False
        self.ended = os.waitpid(self.pid, os.WNOHANG)[0] != 0
        return not self.ended

    def kill(self) -> None:
        if self.alive():
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
            self.ended = True

class Box:
    def __init__(self, ui: str = "") -> None:
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
            config.write(f'[global]\nenabled = false\n[ui]\nnotify = false\nstart_menu = false\nmachine_days = 0\n{ui}[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\neditor = "vi"\nlanguage = "en"\n'
                         f'[usage]\naccount = false\n[commands]\nclaude = "{FAKE}"\n')
        self.core = subprocess.Popen([BIN, "serve", "--no-global"], env=self.env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if os.path.exists(f"{T}/sock"):
                break
            time.sleep(0.1)
        time.sleep(0.5)
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

    def spawn(self, name: str) -> str:
        return self.ask({"cmd": "spawn", "cwd": f"{self.T}/work", "name": name}).get("data", {}).get("id", "s1")

    def terminal(self) -> Term:
        self.term = Term(self.env)
        self.term.first_frame()
        return self.term

    def log(self) -> str:
        with open(f"{self.T}/fake.log", encoding="utf-8", errors="surrogateescape") as f:
            return f.read()

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

def answers(term: Term, key: bytes, mark: str, sec: float = 8.0) -> bool:
    # the interface still draws: a key, its echo on the screen
    term.send(key, 0.2)
    end = time.time() + sec
    while time.time() < end:
        term.pump(0.2)
        if mark in term.text():
            return True
    return False

boxes = []
try:
    # the colon alone: the completion of its command has no word to read
    for label, keys in (("typed ': '", b": "), ("pasted ': note'", b"\x1b[200~: note\x1b[201~"), ("pasted ':\\rfoo'", b"\x1b[200~:\rfoo\x1b[201~"), ("typed ':' then Alt+Enter", b":\x1b\r")):
        box = Box()
        boxes.append(box)
        box.spawn("colon")
        term = box.terminal()
        term.send(keys, 1.5)
        check(f"{label}: the terminal alive", term.alive(), term.text()[-300:])
        check(f"{label}: the next key answered", term.alive() and answers(term, b"zk", "zk"), term.text()[-300:])
        box.close()

    # the window: the same line of the same editor (Qt offscreen; a build without a window: nothing run)
    for typed in (": x", ":\nx"):
        box = Box()
        boxes.append(box)
        box.spawn("colon")
        shot = f"{box.T}/shot.png"
        window = subprocess.Popen([BIN, "--gui"], env={**box.env, "QT_QPA_PLATFORM": "offscreen", "CLAUDE_CLUSTER_SCREENSHOT": shot, "CLAUDE_CLUSTER_SCREENSHOT_TYPE": typed},
                                  stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            code = window.wait(40)
        except subprocess.TimeoutExpired:
            window.kill()
            window.wait()
            code = "still running after 40 s"
        err = window.stderr.read().decode("utf-8", "replace")
        if "not built" in err or "no window" in err.lower():
            print("ok   no window here: not run", flush=True)
        else:
            check(f"the window, {typed!r} in its prompt: no crash", code != -signal.SIGSEGV and (code == 0 or code == "still running after 40 s") and "AddressSanitizer" not in err, f"{code} {err[-300:]}")
        box.close()

    # the atoms of an earlier prompt: a block sent to a session, its label or its character in a text for another one
    box = Box()
    boxes.append(box)
    box.spawn("a")
    other = box.spawn("b")
    term = box.terminal()

    def sent() -> str:
        # what the other session was given, as its transcript has it (the log of the agent also holds the descriptions of the sessions)
        return json.dumps(box.ask({"cmd": "transcript", "session": other, "from": -200, "count": 200}).get("data", {}).get("entries", []))

    term.send(b"\x1b[200~SECRET-LINE-1\rSECRET-LINE-2\rSECRET-LINE-3\rSECRET-LINE-4\rSECRET-LINE-5\x1b[201~", 1.0)
    check("a block of 5 lines: its label", "Pasted text #1" in term.text(), term.text()[-400:])
    term.send(b"\r", 2.5)
    check("sent: the agent has it", "SECRET-LINE-1" in box.log(), box.log()[-300:])
    term.send(b"\x1bn", 1.0)  # the other session
    end = time.time() + 15
    while time.time() < end and "to " + other not in term.text():
        term.pump(0.2)
    check("the other session shown", "to " + other in term.text(), term.text()[-300:])
    with open(f"{box.T}/work/notes.txt", "w", encoding="utf-8") as out:
        out.write("as said in [Pasted text #1 +5 lines] earlier")
    term.send(b":r notes.txt", 0.4)
    term.send(b"\x1b", 0.3)
    term.send(b"\r", 2.0)
    check("a file quoting its label, in the prompt", "as said in" in term.text(), term.text()[-400:])
    term.send(b"\r", 2.5)
    check("     sent: the old block not sent again", "SECRET-LINE-1" not in sent() and "as said in" in sent(), sent()[-300:])
    check("     the label sent as a text", "[Pasted text #1 +5 lines]" in sent(), sent()[-300:])
    atom = chr(0xF0000)
    # its character itself: pasted, typed, in a file read
    term.send(b"\x1b[200~quoted " + atom.encode() + b" char-pasted\x1b[201~", 0.8)
    term.send(b"\r", 2.5)
    check("the character of the old atom pasted: not sent as it", "SECRET-LINE-1" not in sent() and "char-pasted" in sent(), sent()[-300:])
    term.send(b"char-typed ", 0.3)
    term.send(atom.encode(), 0.3)
    term.send(b"\r", 2.5)
    check("     typed: neither", "SECRET-LINE-1" not in sent() and "char-typed" in sent(), sent()[-300:])
    with open(f"{box.T}/work/notes2.txt", "w", encoding="utf-8") as out:
        out.write(f"char-read {atom} end")
    term.send(b":r notes2.txt", 0.4)
    term.send(b"\x1b", 0.3)
    term.send(b"\r", 2.0)
    term.send(b"\r", 2.5)
    check("     from a file read: neither", "SECRET-LINE-1" not in sent() and "char-read" in sent(), sent()[-300:])
    # the block of the prompt being written: as ever
    term.send(b"\x1b[200~CURRENT-1\rCURRENT-2\rCURRENT-3\rCURRENT-4\rCURRENT-5\x1b[201~", 0.8)
    term.send(b"\r", 2.5)
    check("a block of this prompt: sent", "CURRENT-1" in sent(), sent()[-300:])
    box.close()

    # vim: the motion e, the columns of j / k on accents
    box = Box("vim = true\n")
    boxes.append(box)
    box.spawn("vim")
    term = box.terminal()
    term.send("été".encode(), 0.4)
    term.send(b"\x1b", 0.6)  # Normal
    term.send(b"0", 0.3)
    term.send(b"e", 0.3)
    term.send(b"i", 0.3)
    term.send(b"x", 0.5)
    term.send(b"\x1b", 0.5)
    term.send(b"\r", 2.5)
    log = box.log()  # as the agent got it: JSON, the accents in \u
    check("e on accents, then i x: the prompt sent whole", "\\u00e9tx\\u00e9" in log, repr(log[-200:]))
    check("     no character cut in two", "\\ufffd" not in log, repr(log[-200:]))
    # j and k landing inside a character: the é of the line above, at the byte of the column
    term.send(b"i", 0.3)
    term.send("aéb".encode(), 0.3)
    term.send(b"\x1b\r", 0.3)
    term.send("éaé".encode(), 0.3)
    term.send(b"\x1b", 0.5)
    term.send(b"0", 0.2)
    term.send(b"l", 0.2)   # on the a: byte 2 of its line
    term.send(b"k", 0.3)   # byte 2 of the line above: inside the é
    term.send(b"i", 0.3)
    term.send(b"Y", 0.4)
    term.send(b"\x1b", 0.5)
    term.send(b"\r", 2.5)
    log = box.log()
    check("k into a character: its start", "aY\\u00e9b" in log, repr(log[-200:]))
    check("     no character cut in two", "\\ufffd" not in log, repr(log[-200:]))
    # a count of any size: one key, an answer at once; after it the keys are keys
    term.send(b"i", 0.3)
    term.send(b"hello", 0.4)
    term.send(b"\x1b", 0.5)
    began = time.time()
    term.send(b"99999999999w", 0.5)
    term.send(b"x", 0.2)
    answered = False
    while time.time() - began < 15:
        term.pump(0.3)
        if "hell" in term.text() and "hello" not in term.text():
            answered = True
            break
    check("a count of 11 digits: the next key answered at once", answered, term.text()[-300:])
    term.send(b"123456789012345678901234567890", 0.5)
    term.send(b"h", 0.3)   # to the start of the line
    term.send(b"x", 1.5)
    check("a count past what a number holds: no error at each key", term.alive() and "stoul" not in term.text() and "ell" in term.text() and "hell" not in term.text(), term.text()[-300:])
    box.close()
finally:
    for box in boxes:
        box.close()
    os.system(f"pkill -f {ROOT} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
