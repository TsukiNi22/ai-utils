#!/usr/bin/env python3
# A fault of the drawing thread ends the terminal: FTXUI takes SIGSEGV & co when its loop starts and its handler only records them (the faulting instruction runs again for ever: no
# exit, the terminal left in raw and mouse mode, a core of the computer busy). Once the screen runs a fatal signal gives the terminal back and kills the process by that signal; again
# after a program in the foreground (FTXUI takes them again).
# Usage: tests/tty_fatal.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""A fatal signal in the terminal: the terminal given back, the process gone by it."""
import fcntl
import os
import pty
import select
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time

ROOT = tempfile.mkdtemp(prefix="cc-ttyfatal-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
RESET = b"\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1006l"
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def run(index: int, name: str, number: int, editor_first: bool) -> None:
    T = f"{ROOT}/{index}"
    env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
    env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
               HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color", CLAUDE_CLUSTER_NO_SUPERVISOR="1")
    for folder in ("cfg", "data", "run", "cache", "home", "work", "bin"):
        os.makedirs(f"{T}/{folder}", mode=0o700)
    open(f"{T}/fake.log", "w", encoding="utf-8").close()
    with open(f"{T}/bin/editor", "w", encoding="utf-8") as out:
        out.write("#!/bin/sh\nexit 0\n")
    os.chmod(f"{T}/bin/editor", 0o755)
    with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
        config.write(f'[global]\nenabled = false\n[ui]\nnotify = false\nstart_menu = false\nmachine_days = 0\neditor = "{T}/bin/editor"\n[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\nlanguage = "en"\n'
                     f'[usage]\naccount = false\n[commands]\nclaude = "{FAKE}"\n')
    core = subprocess.Popen([BIN, "serve", "--no-global"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    pid = 0
    try:
        for _ in range(100):
            if os.path.exists(f"{T}/sock"):
                break
            time.sleep(0.1)
        time.sleep(0.5)
        pid, fd = pty.fork()
        if pid == 0:
            os.execve(BIN, [BIN, "--tty"], env)
        fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 120, 0, 0))
        raw = b""

        def pump(sec: float) -> bytes:
            got = b""
            end = time.time() + sec
            while time.time() < end:
                if select.select([fd], [], [], 0.05)[0]:
                    try:
                        chunk = os.read(fd, 1 << 16)
                    except OSError:
                        return got
                    if not chunk:
                        return got
                    got += chunk
            return got

        raw += pump(4)
        check(f"{name}: the screen runs", b"claude-cluster" in raw, raw[-200:].decode("latin-1"))
        os.write(fd, b"n")  # the map of this computer asked at its start
        raw += pump(1.5)
        if editor_first:
            os.write(fd, b"\x05")  # a program in the foreground: the signals taken by FTXUI again, then ours at the next frame
            raw += pump(3)
        os.kill(pid, number)
        began = time.time()
        code = None
        while time.time() - began < 15:
            raw += pump(0.2)
            done, status = os.waitpid(pid, os.WNOHANG)
            if done != 0:
                code = status
                break
        if code is None:
            os.kill(pid, signal.SIGKILL)
            os.waitpid(pid, 0)
        pid = 0
        check(f"{name}: the process gone, by that signal", code is not None and os.WIFSIGNALED(code) and os.WTERMSIG(code) == number,
              "still there after 15 s" if code is None else f"exit {os.WEXITSTATUS(code)}" if os.WIFEXITED(code) else f"signal {os.WTERMSIG(code)}")
        check(f"{name}: the terminal given back", RESET in raw[-4000:], raw[-120:].decode("latin-1"))
    finally:
        if pid > 0:
            try:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            except OSError:
                pass
        core.terminate()
        try:
            core.wait(15)
        except subprocess.TimeoutExpired:
            core.kill()
        os.system(f"pkill -f {T} > /dev/null 2>&1")

run(0, "SIGSEGV", signal.SIGSEGV, False)
run(1, "SIGFPE", signal.SIGFPE, False)
run(2, "SIGILL", signal.SIGILL, False)
run(3, "SIGBUS", signal.SIGBUS, False)
run(4, "SIGSEGV after an editor", signal.SIGSEGV, True)
sys.exit(1 if failed else 0)
