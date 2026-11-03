#!/usr/bin/env python3
# A question of an agent of any shape (AskUserQuestion: only a non-empty array is checked): the terminal shows it (never the whole screen lost), Esc declines it whatever its shape, an
# answer typed on Other goes back to the agent; a preview that is not what it says shows as a text: a line of # alone, a chart without series.
# Usage: tests/question_shapes.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""Questions of odd shapes: shown, declined, answered."""
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

ROOT = tempfile.mkdtemp(prefix="cc-askshapes-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
failed = False
AGENT = r'''#!/usr/bin/env python3
# a stand-in for claude: its turn asks the question of questions.json next to its log, as it is
import json, os, select, sys
SID = "00000000-0000-0000-0000-000000000003"
log = open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8")
asked = 0

def out(event):
    print(json.dumps(event), flush=True)

def end(text):
    out({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": text}], "usage": {"input_tokens": 1, "output_tokens": 1}}})
    out({"type": "result", "subtype": "success", "is_error": False, "result": text, "session_id": SID, "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1, "usage": {"input_tokens": 1, "output_tokens": 1}})

out({"type": "system", "subtype": "init", "session_id": SID, "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd()})
for line in sys.stdin:
    try:
        msg = json.loads(line)
    except ValueError:
        continue
    if msg.get("type") == "control_request":
        out({"type": "control_response", "response": {"subtype": "success", "request_id": msg.get("request_id"), "response": {}}})
        continue
    if "ask me" not in json.dumps(msg.get("message", {}).get("content", "")):
        end("ok")
        continue
    with open(os.environ["FAKE_CLAUDE_LOG"].rsplit("/", 1)[0] + "/questions.json", encoding="utf-8") as f:
        questions = json.load(f)
    asked += 1
    out({"type": "control_request", "request_id": f"ask-{asked}", "request": {"subtype": "can_use_tool", "tool_name": "AskUserQuestion", "input": {"questions": questions}}})
    while True:
        if not select.select([sys.stdin], [], [], 120)[0]:
            break
        other = sys.stdin.readline()
        if not other:
            sys.exit(0)
        log.write("REPLY " + other.strip() + "\n")
        log.flush()
        reply = json.loads(other)
        if reply.get("type") == "control_request" and reply.get("request", {}).get("subtype") == "interrupt":
            out({"type": "control_response", "response": {"subtype": "success", "request_id": reply.get("request_id"), "response": {}}})
            end("interrupted")
            break
        if reply.get("type") == "control_response":
            end("answered")
            break
'''
SHAPES = {
    "a number for a question": ([1], "1"),
    "a string for a question": (["pick one?"], "pick one?"),
    "options as strings": ([{"question": "which one?", "options": ["yes", "no"]}], "which one?"),
    "options of no list": ([{"question": "a list?", "options": 3}], "a list?"),
    "options as an object": ([{"question": "an object?", "options": {"a": {"label": "first"}, "b": "second"}}], "an object?"),
    "a number for its text": ([{"question": 3, "header": 4, "options": [{"label": 5, "description": 6}]}], "3"),
    "multiSelect of no bool": ([{"question": "several?", "multiSelect": "yes", "options": [{"label": "a"}]}], "several?"),
    "a preview of # alone": ([{"question": "preview?", "options": [{"label": "a", "preview": "###"}, {"label": "b"}]}], "###"),
    "a preview, a chart without series": ([{"question": "chart?", "options": [{"label": "a", "preview": '{"type":"chart","series":[1]}'}, {"label": "b"}]}], "chart?"),
    "a preview of no text": ([{"question": "object?", "options": [{"label": "a", "preview": {"type": "table", "rows": [["c1"]]}}, {"label": "b"}]}], "c1"),
    "a normal one": ([{"question": "normal?", "header": "H", "options": [{"label": "a", "description": "the first"}, {"label": "b"}]}], "normal?"),
}

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

def run(index: int, name: str, questions: list, mark: str) -> None:
    T = f"{ROOT}/{index}"
    env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
    env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
               HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color", CLAUDE_CLUSTER_NO_SUPERVISOR="1")
    for folder in ("cfg", "data", "run", "cache", "home", "work"):
        os.makedirs(f"{T}/{folder}", mode=0o700)
    open(f"{T}/fake.log", "w", encoding="utf-8").close()
    with open(f"{T}/agent.py", "w", encoding="utf-8") as out:
        out.write(AGENT)
    os.chmod(f"{T}/agent.py", 0o755)
    with open(f"{T}/questions.json", "w", encoding="utf-8") as out:
        json.dump(questions, out)
    with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
        config.write('[global]\nenabled = false\n[ui]\nnotify = false\nstart_menu = false\nmachine_days = 0\n[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\neditor = "vi"\nlanguage = "en"\n'
                     f'[usage]\naccount = false\n[sessions]\nquestions_to_global = false\n[commands]\nclaude = "{T}/agent.py"\n')

    def ask(request: dict) -> dict:
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(30)
        s.connect(f"{T}/sock")
        s.sendall((json.dumps(request) + "\n").encode())
        data = b""
        while not data.endswith(b"\n"):
            chunk = s.recv(1 << 20)
            if not chunk:
                break
            data += chunk
        s.close()
        return json.loads(data or b"{}")

    core = subprocess.Popen([BIN, "serve", "--no-global"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    term = None
    try:
        for _ in range(100):
            if os.path.exists(f"{T}/sock"):
                break
            time.sleep(0.1)
        time.sleep(0.5)
        sid = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "asks"}).get("data", {}).get("id", "s1")
        term = Term(env)
        term.first_frame()
        ask({"cmd": "send", "session": sid, "text": "ask me"})
        term.pump(3)
        text = term.text()
        check(f"{name}: its popup, not the whole screen an error", "could not be drawn" not in text and " asks" in text and mark in text, text[-700:])
        # the keys over it: none of them throws
        term.send(b"\x1b[B", 0.4)
        term.send(b"\x1b[A", 0.4)
        check(f"{name}: the arrows", term.alive() and "could not be drawn" not in term.text(), term.text()[-400:])
        # Esc: declined, the agent told
        term.send(b"\x1b", 2.0)
        data = (ask({"cmd": "ask"}).get("data") or {})
        check(f"{name}: Esc declines it", term.alive() and not data.get("request_id") and "question declined" in term.text(), f"{json.dumps(data)[:200]} {term.text()[-300:]}")
        with open(f"{T}/fake.log", encoding="utf-8") as log:
            check(f"{name}: the agent told", "REPLY" in log.read(), "")
        # again: Other typed, sent, whatever its shape
        ask({"cmd": "send", "session": sid, "text": "ask me"})
        term.pump(3)
        for _ in range(8):
            term.send(b"\x1b[B", 0.2)  # the last row: Other (Next on a multi choice)
        cursor = (ask({"cmd": "ask"}).get("data") or {}).get("cursor", 0)
        options = (questions[0].get("options") if isinstance(questions[0], dict) else None) or []
        count = len(options) if isinstance(options, (list, dict)) else 1
        if cursor != count:
            term.send(b"\x1b[A", 0.3)
        term.send(b"typed answer", 1.0)
        term.send(b"\r", 2.5)
        with open(f"{T}/fake.log", encoding="utf-8") as log:
            logged = log.read()
        check(f"{name}: Other typed goes to the agent", term.alive() and "typed answer" in logged, f"{logged[-300:]} {term.text()[-400:]}")
    finally:
        if term is not None:
            term.kill()
        core.terminate()
        try:
            core.wait(15)
        except subprocess.TimeoutExpired:
            core.kill()
        os.system(f"pkill -f {T} > /dev/null 2>&1")

for at, (title, (given, seen)) in enumerate(SHAPES.items()):
    run(at, title, given, seen)
sys.exit(1 if failed else 0)
