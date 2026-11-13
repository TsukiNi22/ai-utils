#!/usr/bin/env python3
# Force the switch: a core of an older build (the installed claude-cluster) runs a long turn, one of its terminals is open; the new
# binary is put in its place and started: asked "another version runs", f - nothing waited for: the turn stopped, the old core updates
# itself to the new binary, its terminal follows it (still alive, now on the new build), the new core continues the turn.
# Usage: tests/force_switch.py [path/to/new/claude-cluster] [path/to/old/claude-cluster]   (default: /tmp/cc-build/claude-cluster, the installed one)
"""The forced switch to another build: nothing waited for, the interfaces kept."""
import json
import os
import pty
import select
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-force-")
NEW = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
OLD = sys.argv[2] if len(sys.argv) > 2 else shutil.which("claude-cluster") or ""
BIN = f"{T}/bin/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_DELAY="120", TERM="xterm-256color",
           CLAUDE_CLUSTER_NO_MENU="1")
failed = False


def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(15)
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


def build() -> str:
    try:
        return (ask({"cmd": "hello"}).get("data") or {}).get("build", "")
    except OSError:
        return ""


def term(args: list) -> tuple:
    pid, fd = pty.fork()
    if pid == 0:
        os.execve(BIN, [BIN, *args], ENV)
    return pid, fd


def read(fd: int, sec: float) -> str:
    out = b""
    end = time.time() + sec
    while time.time() < end:
        if select.select([fd], [], [], 0.1)[0]:
            try:
                out += os.read(fd, 65536)
            except OSError:
                break
    return out.decode(errors="replace")


if not OLD or not os.path.exists(OLD):
    print("skip: no installed claude-cluster to play the older build")
    sys.exit(0)
for folder in ("cfg", "data", "run", "cache", "home", "work", "bin"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\nstart_menu = false\n[user]\nname = "t"\nemail = "t@t.t"\n'
                 f'[usage]\naccount = false\n[commands]\nclaude = "{FAKE}"\n')
shutil.copy(OLD, BIN)
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
old_pid = new_pid = 0
try:
    for _ in range(100):
        if build():
            break
        time.sleep(0.1)
    old_build = build()
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "busy", "prompt": "slow job"}).get("data") or {}).get("id", "s1")
    time.sleep(3)
    state = next((s.get("state") for s in ask({"cmd": "list"}).get("data") or [] if s.get("id") == sid), "")
    check("the old core runs a long turn", state == "working", str(state))
    old_pid, old_fd = term(["--tty"])  # a terminal of the old core
    read(old_fd, 4)
    shutil.copy(NEW, f"{T}/bin/new")
    os.rename(f"{T}/bin/new", BIN)  # the new version installed over it
    time.sleep(4)
    new_pid, new_fd = term(["--tty"])
    asked = read(new_fd, 6)
    check("asked: another version runs, with Force the switch", "[f] force the switch" in asked, asked[-400:])
    os.write(new_fd, b"f\n")
    said = read(new_fd, 40)
    check("forced: nothing waited for (updated by itself, or stopped by hand when a turn ignores its interrupt)",
          "switching" in said and ("done" in said or "replaced" in said), said[-500:])
    check("the core runs the new build", build() not in ("", old_build), build())
    deadline = time.time() + 30  # it retries its core every 1 to 10 s, then runs the new binary in place
    while time.time() < deadline and os.readlink(f"/proc/{old_pid}/exe").endswith("(deleted)"):
        read(old_fd, 1)
    alive = old_pid > 0 and os.waitpid(old_pid, os.WNOHANG) == (0, 0)
    check("the terminal of the old core kept (it follows the new one)", alive)
    check("its binary is the new one (re-executed in place)", alive and not os.readlink(f"/proc/{old_pid}/exe").endswith("(deleted)"),
          os.readlink(f"/proc/{old_pid}/exe") if alive else "gone")
    deadline = time.time() + 60  # its session resumed first (a loaded computer: slower)
    log = ""
    while time.time() < deadline and "replaced by another version" not in log:
        with open(f"{T}/fake.log", encoding="utf-8") as f:
            log = f.read()
        time.sleep(1)
    check("the stopped turn continued by the new core", "replaced by another version" in log, log[-400:])
finally:
    for pid in (old_pid, new_pid):
        if pid > 0:
            try:
                os.kill(pid, signal.SIGKILL)
            except OSError:
                pass
    try:
        ask({"cmd": "quit"})
    except OSError:
        pass
    if core.poll() is None:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
