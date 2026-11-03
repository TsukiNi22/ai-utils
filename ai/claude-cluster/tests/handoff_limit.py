#!/usr/bin/env python3
# The prompts of a queue kept over a restart: sent again even when a spending limit was reached meanwhile (they were accepted when they were given, a queue that went on would have
# sent them: never lost on a refusal, like the prompts held by an update); a burst of them while other threads save the state and schedule_act runs: each one sent once, in order.
# Usage: tests/handoff_limit.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The queues kept over a restart."""
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time

T = tempfile.mkdtemp(prefix="cc-handoff-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude_cwd.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_DELAY="30")
failed = False
core = None

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict, timeout: float = 30) -> dict:
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

def start(*args: str) -> None:
    global core
    core = subprocess.Popen([BIN, "serve", "--no-global", *args], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    end = time.time() + 30
    while time.time() < end:
        try:
            ask({"cmd": "hello"})
            return
        except OSError:
            time.sleep(0.2)

def stop() -> None:
    global core
    if core is None:
        return
    core.terminate()
    try:
        core.wait(25)
    except subprocess.TimeoutExpired:
        core.kill()
        core.wait()
    core = None

def prompts(word: str) -> list:
    with open(f"{T}/fake.log", encoding="utf-8") as f:
        return [line for line in f.read().split("\n") if line.startswith("PROMPT ") and word in line]

def core_pid() -> int:
    """The process of the core now (its watcher starts it again after a crash: another pid), 0 when it does not answer."""
    try:
        return (ask({"cmd": "hello"}, 15).get("data") or {}).get("pid", 0)
    except (OSError, ValueError):
        return 0

def state() -> dict:
    with open(f"{T}/data/state.json", encoding="utf-8") as f:
        return json.load(f)

def config(extra: str = "") -> None:
    with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as out:
        out.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                  f'[sessions]\nqueue_given = "end"\n[commands]\nclaude = "{FAKE}"\n' + extra)

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
config()
try:
    start()
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
    time.sleep(2)
    ask({"cmd": "send", "session": sid, "text": "slow first"})
    time.sleep(1)
    ask({"cmd": "send", "session": sid, "text": "Q-KEPT-ONE"})
    ask({"cmd": "send", "session": sid, "text": "Q-KEPT-TWO"})
    time.sleep(1)
    stop()
    check("the queue saved", [q.get("text") for q in state().get("queued", {}).get(sid, [])] == ["Q-KEPT-ONE", "Q-KEPT-TWO"], json.dumps(state().get("queued"))[:300])
    # the day's limit reached while it was down
    config('[limits]\ndaily_usd = 0.01\nblock = true\nscope = "all"\n')
    with open(f"{T}/data/usage.jsonl", "a", encoding="utf-8") as f:
        f.write(json.dumps({"time": int(time.time()), "vendor": "anthropic", "model": "x", "session": sid, "tokens": 1, "cost": 5.0, "cost_known": True}) + "\n")
    open(f"{T}/fake.log", "w", encoding="utf-8").close()
    ENV["FAKE_CLAUDE_DELAY"] = "0"
    start("--restore")
    end = time.time() + 30
    while time.time() < end and not (prompts("Q-KEPT-ONE") and prompts("Q-KEPT-TWO")):
        time.sleep(0.5)
    check("a limit reached since: the queue is still sent", len(prompts("Q-KEPT-ONE")) == 1 and len(prompts("Q-KEPT-TWO")) == 1, str(prompts("Q-KEPT")))
    refused = ask({"cmd": "send", "session": sid, "text": "NEW-PROMPT"})
    check("a new prompt is refused by that limit", refused.get("ok") is False, json.dumps(refused)[:300])
    end = time.time() + 10
    while time.time() < end and state().get("queued"):
        time.sleep(0.5)
    check("and nothing is left to send", not state().get("queued"), json.dumps(state().get("queued"))[:300])
    stop()
    open(f"{T}/fake.log", "w", encoding="utf-8").close()

    # ---- many prompts given back while other threads save and run a schedule by hand: each one once, in order
    config()
    state_now = state()
    state_now["queued"] = {sid: [{"images": [], "origin": "", "text": f"BURST-{n:03d}."} for n in range(120)] + [{"text": 5}, {"images": 7, "text": "NOT-A-PROMPT"}, 9]}
    with open(f"{T}/data/state.json", "w", encoding="utf-8") as f:
        json.dump(state_now, f)
    start("--restore")
    pid = core_pid()
    wid = (ask({"cmd": "schedule_add", "when": "+1h", "every": 60, "action": "notify", "text": "x"}).get("data") or {}).get("id", "")
    stop_all = threading.Event()

    def hammer(request: dict) -> None:
        while not stop_all.is_set():
            try:
                ask(request, 10)
            except (OSError, ValueError):
                time.sleep(0.02)

    threads = [threading.Thread(target=hammer, args=(request,)) for request in ({"cmd": "rename", "session": sid, "name": "n1"}, {"cmd": "rename", "session": sid, "name": "n2"},
                                                                                {"cmd": "schedule_act", "id": wid, "action": "run"}, {"cmd": "schedule_act", "id": wid, "action": "run"})]
    for t in threads:
        t.start()
    end = time.time() + 60
    while time.time() < end and len(prompts("BURST-")) < 120:
        time.sleep(0.5)
    time.sleep(2)
    stop_all.set()
    for t in threads:
        t.join()
    got = [line.split("BURST-")[1].split(".")[0] for line in prompts("BURST-")]
    check("the same core is still there", core_pid() == pid, "")
    # (the agent is given what waits in the queue together: its prompt holds several, in their order)
    numbers = []
    for line in prompts("BURST-"):
        for piece in line.split("BURST-")[1:]:
            numbers.append(int(piece[:3]))
    check("each one sent once", sorted(numbers) == list(range(120)), f"{len(numbers)} sent: {numbers[:12]}")
    check("in their order", numbers == sorted(numbers), str(numbers[:30]))
    check("what is not a prompt dropped, never sent nor kept", "NOT-A-PROMPT" not in open(f"{T}/fake.log", encoding="utf-8").read(), "")
    time.sleep(2)
    check("no queue left", not state().get("queued"), json.dumps(state().get("queued"))[:200])
finally:
    stop()
    subprocess.run(["pkill", "-f", T], check=False)
    shutil.rmtree(T, ignore_errors=True)
sys.exit(1 if failed else 0)
