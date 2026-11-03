#!/usr/bin/env python3
# A task across a restart and an update: a task with its own session goes back to that session (never the first idle session of another project), a task added while an update
# waits runs once (not its queued copy and its held copy), a task waiting for a session of the previous run does not fail while the restore is not answered.
# Usage: tests/task_restart.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A task across a restart."""
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
HERE = os.path.dirname(os.path.abspath(__file__))
FAKE = os.path.join(HERE, "fake_claude_cwd.py")
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

class Core:
    def __init__(self, prefix: str, **env: str) -> None:
        self.T = tempfile.mkdtemp(prefix=f"cc-{prefix}-")
        T = self.T
        for folder in ("cfg", "data", "run", "cache", "home", "work", "work2"):
            os.makedirs(f"{T}/{folder}")
        open(f"{T}/fake.log", "w", encoding="utf-8").close()
        self.env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
        self.env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
                        XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", **env)
        with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
            config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                         f'[commands]\nclaude = "{FAKE}"\n')
        self.process = None

    def start(self, *args: str) -> None:
        self.process = subprocess.Popen([BIN, "serve", "--no-global", *args], env=self.env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        end = time.time() + 30
        while time.time() < end:
            try:
                self.ask({"cmd": "hello"})
                return
            except OSError:
                time.sleep(0.2)

    def stop(self) -> None:
        if self.process is None:
            return
        self.process.terminate()
        try:
            self.process.wait(25)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.process = None

    def ask(self, request: dict) -> dict:
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(30)
        s.connect(f"{self.T}/sock")
        s.sendall((json.dumps(request) + "\n").encode())
        data = b""
        while not data.endswith(b"\n"):
            chunk = s.recv(1 << 20)
            if not chunk:
                break
            data += chunk
        s.close()
        return json.loads(data)

    def spawn(self, folder: str, name: str) -> str:
        return (self.ask({"cmd": "spawn", "cwd": f"{self.T}/{folder}", "name": name}).get("data") or {}).get("id", "")

    def task(self, tid: str) -> dict:
        return next((t for t in (self.ask({"cmd": "task_list"}).get("data") or []) if t.get("id") == tid), {})

    def prompts(self, word: str) -> list:
        with open(f"{self.T}/fake.log", encoding="utf-8") as log:
            return [line for line in log.read().split("\n") if line.startswith("PROMPT ") and word in line]

    def wait_task(self, tid: str, *states: str, seconds: float = 40) -> dict:
        end = time.time() + seconds
        while time.time() < end:
            one = self.task(tid)
            if one.get("status") in states:
                return one
            time.sleep(0.5)
        return self.task(tid)

    def clean(self) -> None:
        self.stop()
        subprocess.run(["pkill", "-f", self.T], check=False)
        shutil.rmtree(self.T, ignore_errors=True)

# ---- a task with its own session, stopped by a restart
box = Core("taskown", FAKE_CLAUDE_DELAY="120")
try:
    box.start()
    box.spawn("work", "other-project")
    time.sleep(2)
    tid = (box.ask({"cmd": "task_add", "prompt": "slow TASK-OWN", "spawn": {"cwd": f"{box.T}/work2", "name": "own"}}).get("data") or {}).get("id", "")
    one = box.wait_task(tid, "running")
    for _ in range(40):
        if box.prompts("TASK-OWN"):
            break
        time.sleep(0.5)
    own = box.task(tid).get("session", "")
    check("its own session runs it, in its folder", bool(box.prompts("TASK-OWN")) and f"{box.T}/work2 " in box.prompts("TASK-OWN")[0], str(box.prompts("TASK-OWN")))
    box.stop()
    open(f"{box.T}/fake.log", "w", encoding="utf-8").close()
    box.env["FAKE_CLAUDE_DELAY"] = "0"
    box.start("--restore")
    one = box.wait_task(tid, "done", "failed")
    got = box.prompts("TASK-OWN")
    check("restarted: it runs again and ends", one.get("status") == "done", json.dumps(one)[:300])
    check("in its own session again", one.get("session") == own, f"{one.get('session')} instead of {own}")
    check("in the folder of its project, never the first idle session of another one", len(got) == 1 and f"{box.T}/work2 " in got[0], str(got))
    names = [s.get("name") for s in box.ask({"cmd": "list"}).get("data", [])]
    check("no session started again for it", names.count("own") == 1, str(names))
finally:
    box.clean()

# ---- a task added while an update waits: the new version runs it, once
box = Core("taskhold", CLAUDE_CLUSTER_TEST="1", FAKE_CLAUDE_DELAY="0")
try:
    box.start()
    sid = box.spawn("work", "worker")
    time.sleep(2)
    box.ask({"cmd": "upgrade", "action": "hold"})
    tid = (box.ask({"cmd": "task_add", "prompt": "TASK-DOUBLE", "target": sid}).get("data") or {}).get("id", "")
    time.sleep(2)
    check("an update waiting: it stays queued", box.task(tid).get("status") == "queued", json.dumps(box.task(tid))[:300])
    held = []
    if os.path.exists(f"{box.T}/data/held.json"):
        with open(f"{box.T}/data/held.json", encoding="utf-8") as f:
            held = json.load(f)
    check("its prompt not held on top of it", "TASK-DOUBLE" not in json.dumps(held), json.dumps(held)[:300])
    check("nothing sent yet", not box.prompts("TASK-DOUBLE"), str(box.prompts("TASK-DOUBLE")))
    box.stop()
    box.start("--restore")
    one = box.wait_task(tid, "done", "failed")
    time.sleep(5)
    check("the new version runs it", one.get("status") == "done", json.dumps(one)[:300])
    check("once", len(box.prompts("TASK-DOUBLE")) == 1, str(box.prompts("TASK-DOUBLE")))
    # an update that does not switch: it starts when the wait is over
    box.ask({"cmd": "upgrade", "action": "hold"})
    tid = (box.ask({"cmd": "task_add", "prompt": "TASK-RELEASED", "target": sid}).get("data") or {}).get("id", "")
    time.sleep(2)
    box.ask({"cmd": "upgrade", "action": "release"})
    one = box.wait_task(tid, "done", "failed")
    check("the update called off: it runs, once", one.get("status") == "done" and len(box.prompts("TASK-RELEASED")) == 1, json.dumps(one)[:200])
finally:
    box.clean()

# ---- the restore not answered yet (the core of the interfaces, --linger): the session of a task is not there yet
box = Core("taskask", FAKE_CLAUDE_DELAY="0")
try:
    box.start()
    sid = box.spawn("work", "worker")
    time.sleep(2)
    box.stop()
    with open(f"{box.T}/data/state.json", encoding="utf-8") as f:
        state = json.load(f)
    state["tasks"] = [{"id": "t7", "prompt": "TASK-AFTER-THE-QUESTION", "target": sid, "status": "queued", "created": int(time.time())}]
    state["next_task"] = 8
    with open(f"{box.T}/data/state.json", "w", encoding="utf-8") as f:
        json.dump(state, f)
    box.start("--linger")
    time.sleep(4)
    one = box.task("t7")
    check("the restore not answered: its task waits, never failed", one.get("status") == "queued", json.dumps(one)[:300])
    answer = box.ask({"cmd": "restore_answer", "restore": True})
    check("the answer taken", answer.get("ok") is True, json.dumps(answer)[:200])
    one = box.wait_task("t7", "done", "failed")
    check("answered: its session back, it runs", one.get("status") == "done", json.dumps(one)[:300])
finally:
    box.clean()
sys.exit(1 if failed else 0)
