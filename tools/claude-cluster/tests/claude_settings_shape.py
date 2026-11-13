#!/usr/bin/env python3
# ~/.claude/settings.json is a file of another tool: a model or a thinking level of another type never fails the list of the sessions (socket, page), a text is still read.
# A fake agent that never tells its model: the label of a session is then read from that file.
# Usage: tests/claude_settings_shape.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The labels of a session over a settings.json of an unexpected shape."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-claudeset-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", CLAUDE_CLUSTER_DISCOVERY_PORT=str(42000 + os.getpid() % 1500))
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

for folder in ("cfg", "data", "run", "cache", "home/.claude", "work"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/silent_agent.py", "w", encoding="utf-8") as agent:  # never an init line: no model of its own, the label comes from settings.json
    agent.write("#!/usr/bin/env python3\nimport sys\nfor line in sys.stdin:\n    pass\n")
os.chmod(f"{T}/silent_agent.py", 0o755)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{T}/silent_agent.py"\n')

SHAPES = {
    "a model that is a number": {"model": 1},
    "a model that is a list": {"model": ["sonnet"]},
    "a model that is null": {"model": None},
    "a level that is a number": {"model": "claude-sonnet-5-5", "modelSettings": {"claude-sonnet-5-5": {"effortLevel": 1}}},
    "a level that is an object": {"model": "claude-sonnet-5-5", "modelSettings": {"claude-sonnet-5-5": {"effortLevel": {"a": 1}}}},
    "modelSettings of another shape": {"model": "claude-sonnet-5-5", "modelSettings": {"claude-sonnet-5-5": 1}},
}
try:
    for name, shape in list(SHAPES.items()) + [("a level that is a text", {"model": "claude-sonnet-5-5", "modelSettings": {"claude-sonnet-5-5": {"effortLevel": "high"}}})]:
        with open(f"{T}/home/.claude/settings.json", "w", encoding="utf-8") as file:
            json.dump(shape, file)
        core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=T)
        try:
            for _ in range(100):
                if os.path.exists(f"{T}/sock"):
                    break
                time.sleep(0.1)
            time.sleep(1.5)
            sid = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"})["data"]["id"]
            listed = ask({"cmd": "list"})
            read = ask({"cmd": "read", "session": sid})
            check(f"{name}: list answers", listed.get("ok") is True and len(listed.get("data", [])) >= 1, json.dumps(listed)[:200])
            check(f"{name}: read answers", read.get("ok") is True, json.dumps(read)[:200])
            if name == "a level that is a text":
                check(f"{name}: still read", "high" in json.dumps(read.get("data", {})), json.dumps(read)[:400])
                check(f"{name}: its model still read", "claude-sonnet-5-5" in json.dumps(read.get("data", {})), json.dumps(read)[:400])
            ask({"cmd": "close", "session": sid})
        finally:
            core.terminate()
            try:
                core.wait(15)
            except subprocess.TimeoutExpired:
                core.kill()
                core.wait()
finally:
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
