#!/usr/bin/env python3
# The terminal view of the supervisor under the sanitizers: F9, a value edited (a row of its list replaced under the reference that named it); an action still running when an
# instance is opened from it (its thread outlives the view: the interface of that instance runs in this process); a file of another tool with fields of another type (a job of
# Claude Code, a session of Claude Code, a transcript): the view opens and stays alive.
# Usage: tests/supervisor_safe.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster): a sanitizer build tells the most
"""The supervisor, memory and foreign files."""
import fcntl
import glob
import json
import os
import pty
import select
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import termios
import time

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

class View:
    """claude-cluster --supervisor --tty in a terminal of its own, its folders apart."""

    def __init__(self, prefix: str, config: str = "") -> None:
        self.T = tempfile.mkdtemp(prefix=f"cc-{prefix}-")
        T = self.T
        for folder in ("cfg", "data", "run", "cache", "home"):
            os.makedirs(f"{T}/{folder}")
        open(f"{T}/fake.log", "w", encoding="utf-8").close()
        self.env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
        self.env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
                        XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color", CLAUDE_CLUSTER_NO_MENU="1",
                        ASAN_OPTIONS=f"detect_leaks=0:detect_container_overflow=0:abort_on_error=0:print_stacktrace=1:detect_stack_use_after_return=1:log_path={T}/asan")
        with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as out:
            out.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                      f'[commands]\nclaude = "{FAKE}"\n' + config)
        self.pid = 0
        self.fd = -1
        self.screen = b""

    def launch(self) -> None:
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.environ.clear()
            os.environ.update(self.env)
            os.execv(BIN, [BIN, "--supervisor", "--tty"])
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 45, 150, 0, 0))

    def pump(self, seconds: float) -> bool:
        """What it writes for a while; False when it ended."""
        end = time.time() + seconds
        while time.time() < end:
            ready, _, _ = select.select([self.fd], [], [], 0.2)
            if ready:
                try:
                    data = os.read(self.fd, 65536)
                except OSError:
                    return False
                if not data:
                    return False
                self.screen += data
        return True

    def key(self, keys: bytes, seconds: float = 0.5) -> bool:
        os.write(self.fd, keys)
        return self.pump(seconds)

    def drawn(self, seconds: float = 60) -> bool:
        end = time.time() + seconds
        while time.time() < end:
            if not self.pump(0.5):
                return False
            if b"supervisor" in self.screen:
                return True
        return False

    def ended(self, seconds: float = 30) -> bool:
        """Waits for its end (its reader thread ends first, what it reads may take a while under a sanitizer)."""
        end = time.time() + seconds
        while time.time() < end:
            self.pump(0.2)
            done, _ = os.waitpid(self.pid, os.WNOHANG)
            if done == self.pid:
                self.pid = 0
                return True
        return False

    def reports(self) -> str:
        text = ""
        for name in glob.glob(f"{self.T}/asan*"):
            with open(name, encoding="utf-8", errors="replace") as f:
                content = f.read()
            at = content.find("ERROR: AddressSanitizer")
            text += content[at:at + 900] if at >= 0 else content[:300]
        return text

    def clean(self) -> None:
        if self.pid:
            try:
                os.kill(self.pid, signal.SIGKILL)
                os.waitpid(self.pid, 0)
            except OSError:
                pass
        subprocess.run(["pkill", "-f", self.T], check=False)
        time.sleep(0.5)
        shutil.rmtree(self.T, ignore_errors=True)

# ---- F9: a value edited
view = View("supf9")
try:
    view.launch()
    check("drawn", view.drawn(), view.screen[-200:].decode(errors="replace"))
    view.key(b"\x1b[20~", 3)    # F9: its web access, its settings
    check("F9 shown", b"Web access of the supervisor" in view.screen)
    view.key(b"\r", 1.5)        # Enter on the first row (the port of the supervisor): editing
    alive = view.key(b"\r", 4)  # Enter: written, webList replaced
    check("a value written: the view goes on", alive, view.reports()[:900])
    check("no memory error", view.reports() == "", view.reports())
    view.key(b"\x1b", 1)        # closed
    view.key(b"\x03", 0.2)      # Ctrl+C: quits
    check("Ctrl+C: it ends", view.ended(), view.screen[-200:].decode(errors="replace"))
finally:
    view.clean()

# ---- an action still running when an instance is opened from it
probe = socket.socket()
probe.bind(("127.0.0.1", 0))
port = probe.getsockname()[1]
probe.close()
view = View("supthread", f"[web]\nsupervisor_port = {port}\nhttps = false\n")
view.env["CLAUDE_CLUSTER_DISCOVERY_PORT"] = str(port + 1)
try:
    view.launch()
    check("drawn (2)", view.drawn(), view.screen[-200:].decode(errors="replace"))
    view.key(b"w", 0.3)         # its web access: the toggle runs in a thread (10 s at most)
    view.key(b"p", 0.3)         # the performance: a thread too
    view.key(b"q", 0.2)         # (closes it)
    view.key(b"\r", 0.3)        # the menu of the main one: Cancel first
    view.key(b"\x1b[B", 0.1)
    view.key(b"\x1b[B", 0.3)    # down, down: Start and open it
    view.key(b"\r", 0.5)
    view.pump(20)               # the view gone, the interface of that instance runs, the actions end
    check("an action running past the view: no memory error", view.reports() == "", view.reports())
finally:
    view.clean()

# ---- files of another tool, fields of another type
def foreign(T: str) -> None:
    os.makedirs(f"{T}/home/.claude/jobs/abc")
    with open(f"{T}/home/.claude/jobs/abc/state.json", "w", encoding="utf-8") as f:
        json.dump({"tempo": 1, "state": ["x"], "cwd": 2, "needs": {"a": 1}, "suggestedReply": 3, "name": 4, "detail": 5}, f)
    os.makedirs(f"{T}/home/.claude/jobs/ok")
    with open(f"{T}/home/.claude/jobs/ok/state.json", "w", encoding="utf-8") as f:
        json.dump({"tempo": "active", "state": "running", "cwd": "/tmp", "name": "A-JOB-THAT-READS-WELL"}, f)

view = View("supforeign")
agent = None
try:
    foreign(view.T)
    # an agent of its own, out of any tree of claude-cluster: a process named claude, its session file, its transcript
    fake = f"{view.T}/claude"
    with open(fake, "w", encoding="utf-8") as f:
        f.write("import os, time\nopen(os.environ['PID_FILE'], 'w').write(str(os.getpid()))\ntime.sleep(120)\n")
    pid_file = f"{view.T}/agent.pid"
    subprocess.run(["sh", "-c", f"PID_FILE={pid_file} setsid {sys.executable} {fake} >/dev/null 2>&1 &"], env=view.env, check=False)
    end = time.time() + 5
    while time.time() < end and not os.path.exists(pid_file):
        time.sleep(0.1)
    time.sleep(0.2)
    agent = int(open(pid_file, encoding="utf-8").read())
    os.makedirs(f"{view.T}/home/.claude/sessions")
    with open(f"{view.T}/home/.claude/sessions/{agent}.json", "w", encoding="utf-8") as f:
        json.dump({"status": 1, "parkedJobId": 2, "sessionId": "the-session", "name": 3}, f)
    os.makedirs(f"{view.T}/home/.claude/projects/p")
    with open(f"{view.T}/home/.claude/projects/p/the-session.jsonl", "w", encoding="utf-8") as f:
        f.write(json.dumps({"type": "summary", "summary": 1}) + "\n")
        f.write(json.dumps({"type": 2, "summary": "x"}) + "\n")
        f.write(json.dumps({"type": "user", "message": {"content": [{"type": "text", "text": 5}, {"type": 7}]}}) + "\n")
    view.launch()
    alive = view.drawn() and view.pump(3)
    check("a job and a session of another tool, fields of another type: it stays alive", alive, view.screen[-300:].decode(errors="replace"))
    check("its view is drawn", b"Other agents" in view.screen, view.screen[-300:].decode(errors="replace"))
    check("what reads well is shown", b"A-JOB-THAT-READS-WELL" in view.screen, view.screen[-300:].decode(errors="replace"))
    check("no error of json on its screen", b"json.exception" not in view.screen, view.screen[-300:].decode(errors="replace"))
    os.makedirs(f"{view.T}/home/.claude/jobs/later")
    with open(f"{view.T}/home/.claude/jobs/later/state.json", "w", encoding="utf-8") as f:
        json.dump({"tempo": "active", "state": "running", "cwd": "/tmp", "name": "A-JOB-SEEN-LATER"}, f)
    alive = view.pump(6)
    check("it still reads them (its reader thread alive)", alive and b"A-JOB-SEEN-LATER" in view.screen, view.screen[-300:].decode(errors="replace"))
    check("no memory error (3)", view.reports() == "", view.reports())
    view.key(b"q", 0.2)
    check("q: it ends", view.ended(), view.screen[-200:].decode(errors="replace"))
finally:
    view.clean()
    if agent:
        try:
            os.kill(agent, signal.SIGKILL)
        except OSError:
            pass
sys.exit(1 if failed else 0)
