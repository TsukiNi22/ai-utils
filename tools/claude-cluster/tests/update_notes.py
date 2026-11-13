#!/usr/bin/env python3
# The update told and kept visible: a newer binary installed is offered with what it brings (its notes, the size of the step); Update while
# a turn runs: a mark on the top bar (what it waits for); a newer one again meanwhile: offered again, Update moves the waiting one to it
# (never two updates, no crash). The newer binaries are fakes (python: --version, --notes, serve opens its socket): nothing is switched.
# Usage: tests/update_notes.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""Update popup with notes, a mark while it waits, a newer version during the wait."""
import fcntl
import json
import os
import pty
import select
import shutil
import signal
import socket
import stat
import struct
import sys
import tempfile
import termios
import time

import pyte

T = tempfile.mkdtemp(prefix="cc-notes-")
BUILT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
BIN = f"{T}/bin/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_DELAY="60", TERM="xterm-256color")
FAKE_BINARY = '''#!/usr/bin/env python3
import os, socket, sys, time
if "--version" in sys.argv:
    print("claude-cluster VERSION")
elif any(a.startswith("--notes") for a in sys.argv):
    print("feat VERSION NOTE-NEW-FEATURE agents seen and managed")
    print("fix VERSION NOTE-A-FIX the update no longer loops")
elif "serve" in sys.argv:
    s = socket.socket(socket.AF_UNIX)
    s.bind(os.environ["CLAUDE_CLUSTER_SOCKET"])
    s.listen(4)
    time.sleep(30)
'''
failed = False


def check(name, ok, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def ask(request):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(10)
    s.connect(f"{T}/sock")
    s.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        data += chunk
    s.close()
    return json.loads(data)


def install(version):
    # a newer binary put in place of the running one (renamed over it, like an install)
    path = f"{T}/bin/new"
    with open(path, "w", encoding="utf-8") as file:
        file.write(FAKE_BINARY.replace("VERSION", version))
    os.chmod(path, os.stat(path).st_mode | stat.S_IEXEC)
    os.rename(path, BIN)


class Term:
    def __init__(self):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], ENV)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 200, 0, 0))
        self.screen = pyte.Screen(200, 50)
        self.stream = pyte.ByteStream(self.screen)

    def pump(self, sec) -> None:
        end = time.time() + sec
        while time.time() < end:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    self.stream.feed(os.read(self.fd, 65536))
                except OSError:
                    return

    def text(self) -> str:
        return "\n".join(line.rstrip() for line in self.screen.display)

    def send(self, data, wait: float = 0.5) -> None:
        os.write(self.fd, data)
        self.pump(wait)

    def alive(self):
        return os.waitpid(self.pid, os.WNOHANG) == (0, 0)

    def wait_for(self, text, sec):
        end = time.time() + sec
        while time.time() < end and text not in self.text():
            self.pump(0.3)
        return text in self.text()


for folder in ("cfg", "data", "run", "cache", "work", "bin"):
    os.makedirs(f"{T}/{folder}")
shutil.copy(BUILT, BIN)
current = os.popen(f"{BIN} --version").read().split()[-1]
major, minor, patch = (int(x) for x in current.split("."))
first, second = f"{major}.{minor}.{patch + 1}", f"{major}.{minor}.{patch + 2}"
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
term = None
try:
    term = Term()
    term.pump(5)
    if "asked again at the next start" in term.text():
        term.send(b"n", 1.5)
    ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker", "prompt": "slow work"})
    term.pump(2)
    install(first)
    check("a newer binary installed: offered with what it brings", term.wait_for("A newer claude-cluster is installed", 15) and "NOTE-NEW-FEATURE" in term.text()
          and "NOTE-A-FIX" in term.text() and "patch" in term.text() and f"{current}  →  {first}" in term.text(), term.text()[-1500:])
    term.send(b"u", 2)  # Update: its self-test (the fake opens its socket), then it waits for the turn of worker
    check("Update: a mark on the top bar while it waits (what for)", term.wait_for(f"update → {first}", 10) and "waiting for" in term.text(), term.text()[:600])
    install(second)
    check("a newer one while it waits: offered again", term.wait_for("came while the update waits", 15) and f"{current}  →  {second}" in term.text(), term.text()[-1500:])
    term.send(b"u", 2)
    check("Update again: the waiting update moved to the newest (one update only)", term.wait_for(f"update → {second}", 10), term.text()[:600])
    offer = (ask({"cmd": "status", "session": "worker"}).get("ok") is not False)
    check("the core and the interface still run", offer and term.alive())
finally:
    if term:
        try:
            os.kill(term.pid, signal.SIGKILL)
        except OSError:
            pass
    os.system(f"pkill -9 -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
