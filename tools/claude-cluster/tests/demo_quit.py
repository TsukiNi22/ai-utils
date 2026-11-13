#!/usr/bin/env python3
# Regression test: the demo quit from its terminal (its core, its agents and its folder gone, even with sessions working).
# Usage: tests/demo_quit.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The demo quit from its terminal: its core, its agents and its folder gone, even with sessions working; settings not saved."""
import fcntl
import glob
import os
import pty
import select
import struct
import subprocess
import termios
import time

import pyte

import sys
import tempfile

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
CACHE = tempfile.mkdtemp(prefix="cc-demoq-")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(XDG_CACHE_HOME=CACHE, TERM="xterm-256color")


def check(name, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


pid, fd = pty.fork()
if pid == 0:
    os.execve(BIN, [BIN, "--demo", "--tty"], ENV)
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 150, 0, 0))
screen = pyte.Screen(150, 40)
stream = pyte.ByteStream(screen)


def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        if select.select([fd], [], [], 0.05)[0]:
            try:
                stream.feed(os.read(fd, 65536))
            except OSError:
                return


def text():
    return "\n".join(line.rstrip() for line in screen.display)


try:
    pump(22)
    dirs = glob.glob(CACHE + "/claude-cluster-demo/*")
    check("the demo has a folder of its own (per launch)", len(dirs) == 1, str(dirs))
    agents = [line for line in subprocess.run(["ps", "-eo", "args"], capture_output=True, text=True).stdout.splitlines() if "demo-agent" in line]
    check("its agents run", len(agents) >= 8, str(len(agents)))
    os.write(fd, b"\x11")  # Ctrl+Q
    start = time.time()
    gone = None
    while time.time() - start < 12:
        pump(0.2)
        if os.waitpid(pid, os.WNOHANG)[0] == pid:
            gone = time.time() - start
            break
    print("     the terminal ended after", gone)
    core_t = None
    while time.time() - start < 30:
        if not [l for l in subprocess.run(["ps", "-eo", "args"], capture_output=True, text=True).stdout.splitlines() if BIN + " serve" in l]:
            core_t = time.time() - start
            break
        time.sleep(0.3)
    print("     the core ended after", core_t)
    shown = text()
    check("no 'keep in background' question", "background" not in shown.lower() or "Quit" not in shown, shown[-400:])
    pump(6)
    agents = [line for line in subprocess.run(["ps", "-eo", "args"], capture_output=True, text=True).stdout.splitlines() if "demo-agent" in line]
    check("its agents are gone", not agents, str(agents[:3]))
    cores = [line for line in subprocess.run(["ps", "-eo", "args"], capture_output=True, text=True).stdout.splitlines() if BIN + " serve" in line]
    check("its core is gone", not cores, str(cores))
    check("its folder is gone", not glob.glob(CACHE + "/claude-cluster-demo/*"), str(glob.glob(CACHE + "/claude-cluster-demo/*")))
finally:
    try:
        os.kill(pid, 9)
    except OSError:
        pass
    subprocess.run(["rm", "-rf", CACHE], check=False)
