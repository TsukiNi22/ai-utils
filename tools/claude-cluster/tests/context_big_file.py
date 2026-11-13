#!/usr/bin/env python3
# A file given as a context (session_context): its last 60000 bytes, read alone. A big one (a disk image, a sparse log) never whole in the memory of the core, in the time of
# one read; a small one whole, nothing said of a cut.
# Usage: tests/context_big_file.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A big file as a context: only its end read."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-context-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
KEPT = 60000
SIZE = 1 << 30  # sparse: nothing on the disk; a regression reads 1 GB, never the memory of the computer
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict, timeout: int = 120) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(timeout)
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

def peak() -> int:
    # the highest resident memory of the core (MB)
    pids = [core.pid] + [int(p) for p in subprocess.run(["pgrep", "-P", str(core.pid)], capture_output=True, text=True, check=False).stdout.split()]
    highest = 0
    for pid in pids:
        try:
            with open(f"/proc/{pid}/status", encoding="utf-8") as status:
                for line in status:
                    if line.startswith("VmHWM"):
                        highest = max(highest, int(line.split()[1]) // 1024)
        except OSError:
            pass
    return highest

def told(text: str) -> str:
    # what the fake agent received last for a context
    time.sleep(0.5)
    for _ in range(40):
        with open(f"{T}/fake.log", encoding="utf-8") as log:
            prompts = [json.loads(line[7:]) for line in log if line.startswith("PROMPT") and "[context given" in line and text in line]
        if prompts:
            return prompts[-1] if isinstance(prompts[-1], str) else json.dumps(prompts[-1])
        time.sleep(0.25)
    return ""

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
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
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
    time.sleep(1)

    # a big sparse file, its end written
    big = f"{T}/home/big.bin"
    with open(big, "wb") as out:
        out.truncate(SIZE)
        out.seek(SIZE - 12)
        out.write(b"the big end\n")
    before = peak()
    started = time.time()
    answer = ask({"cmd": "context", "origin": "mcp", "session": sid, "source": big})
    took = time.time() - started
    check("1 GB given: answered", answer.get("ok") is True, json.dumps(answer)[:200])
    check("     in the time of one read", took < 5, f"{took:.1f} s")
    check("     the memory of the core did not follow", peak() - before < 200, f"{before} MB -> {peak()} MB")
    prompt = told("the big end")
    check("     the agent reads its end", "the big end" in prompt and "[... its start cut]" in prompt, prompt[-120:])
    check("     its last 60000 bytes at most", len(prompt) < KEPT + 300, str(len(prompt)))

    # around the bound: a line of its own at each end, what is cut told only when something is
    def sized(name: str, size: int, last: str) -> str:
        path = f"{T}/home/{name}"
        with open(path, "w", encoding="utf-8") as out:
            out.write("START" + "x" * (size - 5 - len(last)) + last)
        reply = ask({"cmd": "context", "session": sid, "source": path})
        check(f"{name}: answered", reply.get("ok") is True, json.dumps(reply)[:200])
        return told(last)

    whole = sized("small.txt", 1000, "the small end")
    check("a small file: whole, no cut told", "START" in whole and "the small end" in whole and "its start cut" not in whole, whole[:120])
    exact = sized("exact.txt", KEPT, "the exact end")
    check("60000 bytes: whole, no cut told", "START" in exact and "the exact end" in exact and "its start cut" not in exact, exact[:120])
    over = sized("over.txt", KEPT + 1, "the over end")
    check("one byte more: its start cut", "START" not in over and "the over end" in over and "its start cut" in over, over[:120])

    # refused as before
    open(f"{T}/home/empty.txt", "w", encoding="utf-8").close()
    empty = ask({"cmd": "context", "session": sid, "source": f"{T}/home/empty.txt"})
    check("an empty file: told", empty.get("ok") is False and "empty" in empty.get("error", ""), json.dumps(empty)[:200])
    gone = ask({"cmd": "context", "session": sid, "source": f"{T}/home/none.txt"})
    check("no such file: told", gone.get("ok") is False and "not a file" in gone.get("error", ""), json.dumps(gone)[:200])
    check("the core alive", core.poll() is None and ask({"cmd": "list"}).get("ok") is True)
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
    try:
        os.unlink(f"{T}/home/big.bin")
    except OSError:
        pass
sys.exit(1 if failed else 0)
