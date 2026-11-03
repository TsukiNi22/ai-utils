#!/usr/bin/env python3
# The scan of the transcripts of Claude Code (the numbers of Usage): a dangling link in a folder no longer ends its walk (the files after it erased from the numbers), one line of
# another shape no longer loses the scan (the numbers frozen for as long as it is there): that line skipped, every other one counted.
# Usage: tests/usage_scan.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The numbers of the transcripts against a link or a line of another shape."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-usagescan-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
PROJECT = f"{T}/home/.claude/projects/-tmp-p"
FILES = 40
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

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
    return json.loads(data)

def line(i: int, j: int, usage: dict | None = None, **top: object) -> str:
    message = {"id": f"m{i}-{j}", "model": "claude-opus-4-1", "usage": usage or {"input_tokens": 100, "output_tokens": 0}}
    return json.dumps(dict({"type": "assistant", "requestId": f"r{i}-{j}", "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S.000Z", time.gmtime()), "message": message}, **top)) + "\n"

def total() -> int:
    data = ask({"cmd": "usage", "vendor": "anthropic"}).get("data") or []
    return (data[0].get("lifetime") or {}).get("tokens", -1) if data else -1

def settled(wanted: int, sec: int = 12) -> int:
    # read in the background (asked again by the command): up to its number, some time left to go past it
    for _ in range(sec):
        if total() == wanted:
            break
        time.sleep(1)
    time.sleep(2)
    return total()

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
os.makedirs(f"{PROJECT}/sub")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
# dangling links among the files, whatever the order of the folder: one of them before most of its files
for i in range(FILES):
    with open(f"{PROJECT}/{i:04d}.jsonl", "w", encoding="utf-8") as out:
        out.write(line(i, 0))
    if i % 8 == 0:
        os.symlink("/nonexistent/x", f"{PROJECT}/link-{i}.jsonl")
os.symlink("/nonexistent/x", f"{PROJECT}/sub/link.jsonl")
os.symlink(f"{PROJECT}/loop.jsonl", f"{PROJECT}/loop.jsonl")  # a link to itself: an error of its own too
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd="/")
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    expected = FILES * 100
    got = settled(expected, 40)
    check("dangling links in the folder: every file counted", got == expected, str(got))

    # lines of another shape: skipped each, the good lines of every file counted at each step
    def add(file: int, text: str, grown: int) -> None:
        global expected
        with open(f"{PROJECT}/{file:04d}.jsonl", "a", encoding="utf-8") as out:
            out.write(text)
        expected += grown
        count = settled(expected)
        check(f"     then {expected}", count == expected, str(count))

    print("     one line of each shape, then good ones", flush=True)
    add(7, line(7, 1, {"input_tokens": "many", "output_tokens": 0}), 0)  # a count of another kind: nothing for it (a number, or not at all)
    add(30, line(30, 1), 100)
    add(2, line(2, 1), 100)
    add(7, line(7, 2), 100)
    add(8, line(8, 1, {"input_tokens": 100, "output_tokens": [], "cache_read_input_tokens": {}, "cache_creation": 5, "output_tokens_details": "x"}), 100)
    add(9, json.dumps({"type": 1, "x": "assistant", "usage": 1}) + "\n", 0)
    add(10, line(10, 1, requestId=5), 0)
    add(11, json.dumps({"type": "assistant", "requestId": "r", "message": {"id": 1, "model": "claude-opus-4-1", "usage": {"input_tokens": 100}}}) + "\n", 0)
    add(12, json.dumps({"type": "assistant", "requestId": "r2", "message": {"id": "m2", "model": ["x"], "usage": {"input_tokens": 100}}}) + "\n", 0)
    add(13, json.dumps({"type": "assistant", "requestId": "r3", "timestamp": 12, "message": {"id": "m3", "model": "claude-opus-4-1", "usage": {"input_tokens": 100}}}) + "\n", 0)
    add(14, "\"usage\" \"assistant\" not json\n", 0)
    add(15, json.dumps(["usage", "assistant"]) + "\n", 0)
    add(16, line(16, 1), 100)

    # a number out of any real count: nothing for it, the sums never past the end of their type
    add(17, line(17, 1, {"input_tokens": 1e308, "output_tokens": -5}), 0)
    add(18, line(18, 1, {"input_tokens": 9223372036854775807, "output_tokens": 9223372036854775807}), 0)
    add(19, line(19, 1), 100)

    # a transcript gone: its numbers too (a real end of the walk)
    os.unlink(f"{PROJECT}/0020.jsonl")
    expected -= 100
    got = settled(expected)
    check("a transcript removed: its numbers gone", got == expected, f"{got}, {expected} expected")

    # its numbers as saved: read back at the next start, nothing counted twice
    core.terminate()
    core.wait(20)
    if os.path.exists(f"{T}/sock"):
        os.unlink(f"{T}/sock")
    core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd="/")
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    got = settled(expected, 40)
    check("the next start: the same numbers", got == expected, f"{got}, {expected} expected")
    check("the core alive", core.poll() is None and ask({"cmd": "list"}).get("ok") is True)
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
