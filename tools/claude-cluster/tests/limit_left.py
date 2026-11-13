#!/usr/bin/env python3
# The limits of the account in the panel of a terminal: each one with what is left before its reset (3d5h, 4h40m, 40m12s), moving; none when its reset is not known or is past.
# Usage: tests/limit_left.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""What is left before the reset of a limit."""
import fcntl
import json
import os
import pty
import re
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

T = tempfile.mkdtemp(prefix="cc-left-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home",
           TERM="xterm-256color", CLAUDE_CLUSTER_NO_SUPERVISOR="1")
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(20)
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

for folder in ("cfg", "data", "run", "cache", "home", "work", "bin"):
    os.makedirs(f"{T}/{folder}")
# an agent telling its limits at each prompt: the resets are given by the file of the test
with open(f"{T}/bin/agent", "w", encoding="utf-8") as out:
    out.write(f'''#!/usr/bin/env python3
import json, sys, time
print(json.dumps({{"type": "system", "subtype": "init", "session_id": "11111111-2222-3333-4444-555555555555", "model": "claude-sonnet-5-5", "tools": []}}), flush=True)
for line in sys.stdin:
    with open("{T}/limits") as limits:
        five, seven = json.load(limits)
    print(json.dumps({{"type": "rate_limit_event", "rate_limit_info": {{"unifiedWindows": {{"five_hour": {{"utilization": 0.13, "resets_at": five}}, "seven_day": {{"utilization": 0.21, "resets_at": seven}}}}}}}}), flush=True)
    print(json.dumps({{"type": "assistant", "message": {{"role": "assistant", "model": "claude-sonnet-5-5", "content": [{{"type": "text", "text": "ok"}}]}}}}), flush=True)
    print(json.dumps({{"type": "result", "subtype": "success", "result": "ok", "session_id": "11111111-2222-3333-4444-555555555555", "usage": {{"input_tokens": 1, "output_tokens": 1}}}}), flush=True)
''')
os.chmod(f"{T}/bin/agent", 0o755)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nstart_menu = false\n[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\neditor = "vi"\nlanguage = "en"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{T}/bin/agent"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
term = None
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(1)

    def limits(five: dict, seven: dict) -> None:
        with open(f"{T}/limits", "w", encoding="utf-8") as out:
            json.dump([five, seven], out)

    limits(int(time.time()) + 4 * 3600 + 40 * 60 + 30, int(time.time()) + 3 * 86400 + 5 * 3600 + 30 * 60)
    sid = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "limits", "prompt": "go"}).get("data", {}).get("id", "s1")
    time.sleep(2)
    pid, fd = pty.fork()
    if pid == 0:
        os.execve(BIN, [BIN, "--tty"], ENV)
    term = pid
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 170, 0, 0))
    screen = pyte.Screen(170, 50)
    stream = pyte.ByteStream(screen)

    def pump(sec: float) -> None:
        end = time.time() + sec
        while time.time() < end:
            if select.select([fd], [], [], 0.05)[0]:
                try:
                    stream.feed(os.read(fd, 1 << 16))
                except OSError:
                    return

    def text() -> str:
        return "\n".join(screen.display)

    pump(4)
    os.write(fd, b"n")
    pump(1.5)
    check("5h: hours and minutes", re.search(r"5h limit \(4h40m\)\s+13%", text()) is not None, text()[-900:])
    check("7d: days and hours", re.search(r"7d limit \(3d5h\)\s+21%", text()) is not None, text()[-900:])
    # under an hour: minutes and seconds, moving
    limits(int(time.time()) + 40 * 60 + 30, 0)
    ask({"cmd": "send", "session": sid, "text": "again"})
    pump(2)
    first = re.search(r"5h limit \((40m\d+s)\)", text())
    check("under an hour: minutes and seconds", first is not None, text()[-900:])
    pump(3)
    second = re.search(r"5h limit \((40m\d+s|39m\d+s)\)", text())
    check("     it moves", first is not None and second is not None and first.group(1) != second.group(1), str(second))
    # past: no timer (a reset already gone)
    limits(int(time.time()) + 2, int(time.time()) + 2)
    ask({"cmd": "send", "session": sid, "text": "again"})
    pump(4)
    check("past: none", re.search(r"5h limit\s+\d+%", text()) is not None and "5h limit (" not in text(), text()[-900:])
finally:
    if term is not None:
        try:
            os.kill(term, signal.SIGKILL)
        except OSError:
            pass
    core.terminate()
    try:
        core.wait(15)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
