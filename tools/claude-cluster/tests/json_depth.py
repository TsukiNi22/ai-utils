#!/usr/bin/env python3
# JSON nested very deep at every door of the core: a request of the control socket (an agent through its MCP tools, an interface), a request of a device of the page, a line of its agent,
# a file read at the next start. nlohmann parses any depth but its copies and dumps recurse: the stack gone, the core ended by a SIGSEGV. Now a malformed text like any other.
# Usage: tests/json_depth.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""JSON nested very deep never ends the core."""
import http.cookiejar
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

T = tempfile.mkdtemp(prefix="cc-depth-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 20300 + os.getpid() % 2000
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home",
           FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_NO_SUPERVISOR="1", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1))
DEEP = b"[" * 200000 + b"]" * 200000
failed = False
jar = http.cookiejar.CookieJar()
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(raw: bytes) -> bytes:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(30)
    s.connect(f"{T}/sock")
    s.sendall(raw + b"\n")
    data = b""
    try:
        while not data.endswith(b"\n"):
            chunk = s.recv(1 << 20)
            if not chunk:
                break
            data += chunk
    except OSError:
        pass
    s.close()
    return data

def start() -> subprocess.Popen:
    if os.path.exists(f"{T}/sock"):
        os.remove(f"{T}/sock")
    process = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(1.5)
    return process

for folder in ("cfg", "data", "run", "cache", "home", "work", "bin"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
# an agent writing such a line, then its answer
with open(f"{T}/bin/agent", "w", encoding="utf-8") as out:
    out.write('''#!/usr/bin/env python3
import json, sys, time
for line in sys.stdin:
    deep = "[" * 200000 + "]" * 200000
    print('{"type":"assistant","message":{"content":[{"type":"tool_use","id":"t1","name":"Read","input":' + deep + '}]}}', flush=True)
    print('{"type":"assistant","message":{"content":[{"type":"text","text":"still here"}]}}', flush=True)
    print('{"type":"result","subtype":"success","result":"still here","session_id":"s","usage":{}}', flush=True)
''')
os.chmod(f"{T}/bin/agent", 0o755)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\n')
core = start()
try:
    sid = json.loads(ask(json.dumps({"cmd": "spawn", "cwd": f"{T}/work", "name": "x"}).encode())).get("data", {}).get("id", "s1")
    time.sleep(0.5)
    for name, raw in (("a field of a request", b'{"cmd":"list","x":' + DEEP + b"}"), ("a display", b'{"cmd":"display","session":"' + sid.encode() + b'","display":{"type":"json","data":' + DEEP + b"}}"),
                      ("a prompt", b'{"cmd":"send","session":"' + sid.encode() + b'","text":' + DEEP + b"}"), ("a setting", b'{"cmd":"settings_set","section":"ui","key":"notify","value":' + DEEP + b"}"),
                      ("a task", b'{"cmd":"task_add","prompt":"x","session":"' + sid.encode() + b'","spawn":' + DEEP + b"}"), ("nothing but that", DEEP)):
        answer = ask(raw)
        check(f"the socket, {name}: told, the core alive", core.poll() is None and b'"ok":false' in answer.replace(b" ", b""), f"{core.poll()} {answer[:120]}")
        if core.poll() is not None:
            break
    # what is legal goes on: well nested, a string full of brackets
    fine = {"cmd": "display", "session": sid, "display": {"type": "json", "data": json.loads("[" * 100 + "]" * 100), "title": "[" * 2000 + "{" * 2000}}
    answer = json.loads(ask(json.dumps(fine).encode()))
    check("100 levels and brackets in a string are not refused", answer.get("ok") is True, json.dumps(answer)[:200])

    # a device of the page
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    login = urllib.request.Request(f"http://127.0.0.1:{PORT}/api/login", data=json.dumps({"pin": pin, "name": "phone"}).encode(), headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
    web.open(login, timeout=30).read()
    for cmd in (b"task_add", b"schedule_add", b"display_act", b"ask"):
        request = urllib.request.Request(f"http://127.0.0.1:{PORT}/api/cmd", data=b'{"cmd":"' + cmd + b'","session":"' + sid.encode() + b'","x":' + DEEP + b"}",
                                         headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
        try:
            web.open(request, timeout=30).read()
        except (urllib.error.URLError, OSError, ValueError):
            pass
    check("a device of the page: the core alive", core.poll() is None, str(core.poll()))
    # a session whose agent writes it
    made = json.loads(ask(json.dumps({"cmd": "spawn", "cwd": f"{T}/work", "name": "deep", "prompt": "go"}).encode()))
    deep_id = (made.get("data") or {}).get("id", "")
    with open(f"{T}/cfg/config.toml", "a", encoding="utf-8") as config:
        pass
    # a file read at the next start, an entry of its log
    with open(f"{T}/data/logs/{sid}.jsonl", "a", encoding="utf-8") as log:
        log.write('{"time":1,"kind":7,"text":"' + "[" * 200000 + '","tool":"","origin":""}\n')
        log.write("[" * 200000 + "]" * 200000 + "\n")
    with open(f"{T}/data/drafts.json", "w", encoding="utf-8") as drafts:
        drafts.write("[" * 200000 + "]" * 200000)
    core.terminate()
    check("a normal end", core.wait(30) == 0, str(core.poll()))
    state = f"{T}/data/state.json"
    with open(state, "rb") as file:
        kept = file.read()
    core = start()
    check("started again on them: alive", core.poll() is None, str(core.poll()))
    answer = ask(json.dumps({"cmd": "read", "session": sid}).encode())
    check("     they read", core.poll() is None and answer != b"", str(core.poll()))
    core.terminate()
    core.wait(30)
    with open(state, "wb") as file:
        file.write(b'{"version":1,"next":1,"sessions":' + DEEP + b"}")
    core = start()
    check("a state file of that shape: alive", core.poll() is None, str(core.poll()))
    core.terminate()
    core.wait(30)

    # its agent: a line nested very deep, its next lines read
    with open(state, "wb") as file:
        file.write(kept)
    with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
        config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                     f'[commands]\nclaude = "{T}/bin/agent"\n')
    core = start()
    made = json.loads(ask(json.dumps({"cmd": "spawn", "cwd": f"{T}/work", "name": "agent", "prompt": "go"}).encode()))
    agent = (made.get("data") or {}).get("id", "")
    answer = {}
    for _ in range(60):
        time.sleep(0.5)
        if core.poll() is not None:
            break
        answer = json.loads(ask(json.dumps({"cmd": "read", "session": agent}).encode()) or b"{}")
        if "still here" in json.dumps(answer):
            break
    check("an agent writing it: the core alive", core.poll() is None, str(core.poll()))
    check("     its next lines read", "still here" in json.dumps(answer), json.dumps(answer)[:300])
finally:
    if core.poll() is None:
        core.terminate()
        try:
            core.wait(20)
        except subprocess.TimeoutExpired:
            core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
