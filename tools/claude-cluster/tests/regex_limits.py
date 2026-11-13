#!/usr/bin/env python3
# A regex built from what someone typed or sent (:s from a device, the output wanted from a command): no stack gone in its compiler (deep parentheses, a long pattern: a SIGSEGV of the
# core), no search without an end ((a*)*b), no replacement of gigabytes; :s itself works as before, its groups, flags and back-references too.
# Usage: tests/regex_limits.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""cluster::regex_check / regex_found / regex_changed, through :s and the command triggers."""
import http.cookiejar
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request

T = tempfile.mkdtemp(prefix="cc-regex-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 39000 + os.getpid() % 1500
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1))
failed = False
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict, timeout: int = 30) -> dict:
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

def post(path: str, body: dict, timeout: int = 280) -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
    with web.open(request, timeout=timeout) as answer:
        return json.loads(answer.read() or b"{}")

def start(*args: str) -> subprocess.Popen:
    process = subprocess.Popen([BIN, "serve", "--no-global", *args], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=T)
    for _ in range(100):
        if os.path.exists(f"{T}/sock") and process.poll() is None:
            break
        time.sleep(0.1)
    time.sleep(2)
    return process

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\n')
core = start()
try:
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post("/api/login", {"pin": pin, "name": "phone"})

    def page_open() -> None:
        # what the page does (a device silent for 30 s is gone): a search that takes that long, on a build with sanitizers, never logs it out
        while core.poll() is None:
            try:
                web.open(f"http://127.0.0.1:{PORT}/api/state", timeout=5).read()
            except OSError:
                pass
            time.sleep(8)
    threading.Thread(target=page_open, daemon=True).start()
    sid = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"})["data"]["id"]
    time.sleep(1)

    def prompt(text: str) -> dict:
        answer = post("/api/cmd", {"cmd": "send", "session": sid, "text": text})
        time.sleep(1.5)
        return answer

    def sub(pattern: str, new: str = "x", flags: str = "") -> dict:
        return post("/api/cmd", {"cmd": "send", "session": sid, "text": f":s/{pattern}/{new}/{flags}"})

    def changed(name: str, answer: dict, wanted: str) -> None:
        check(name, answer.get("ok") is True and answer.get("data", {}).get("fill") == wanted, json.dumps(answer)[:220])

    def wrong(name: str, answer: dict) -> None:
        check(name, answer.get("ok") is False and core.poll() is None, json.dumps(answer)[:220])

    # --- :s works as before
    prompt("hello hello World of words")
    changed(":s a word", sub("hello", "bye"), "bye hello World of words")
    changed(":s g: every one", sub("hello", "bye", "g"), "bye bye World of words")
    changed(":s i: any case", sub("WORLD", "Moon", "i"), "hello hello Moon of words")
    changed(":s its groups", sub("(hello) (hello)", "$2-$1"), "hello-hello World of words")
    changed(":s $&", sub("World", "[$&]"), "hello hello [World] of words")
    changed(":s another delimiter", post("/api/cmd", {"cmd": "send", "session": sid, "text": ":s#World#Moon#"}), "hello hello Moon of words")
    changed(":s an alternative", sub("World|words", "x", "g"), "hello hello x of x")
    changed(":s a back-reference", sub(r"(hello) \1", "once"), "once World of words")
    changed(":s lazy", sub("h.*?o", "x"), "x hello World of words")
    changed(":s a lookahead", sub("hello(?= World)", "x"), "hello x World of words")
    changed(":s 64 levels of parentheses", sub("(" * 64 + "World" + ")" * 64, "x"), "hello hello x of words")
    wrong(":s a wrong pattern: told", sub("(["))
    wrong(":s nothing matches: told", sub("nothing here"))

    # --- what used to take the stack of the compiler
    for depth in (65, 1000, 10000, 50000, 200000):
        answer = sub("(" * depth)
        wrong(f":s {depth} parentheses: told", answer)
    wrong(":s 65 levels closed: told", sub("(" * 65 + "a" + ")" * 65))
    wrong(":s a pattern of 100 000 letters: told", sub("a" * 100000))
    wrong(":s a pattern of 100 000 alternatives: told", sub("a|" * 50000))
    deep = "(" * 60000
    for name, request in (("schedule if.regex", {"cmd": "schedule_add", "origin": "mcp", "tool": "schedule_add", "when": "+1h", "text": "x", "session": sid, "if": {"command": "true", "regex": deep}}),
                          ("task on_event regex", {"cmd": "task_add", "origin": "mcp", "tool": "task_add", "prompt": "x", "target": sid, "on_event": {"type": "command", "command": "true", "regex": deep}})):
        answer = ask(request)
        check(f"{name}: 60 000 parentheses refused", answer.get("ok") is False and "regex" in answer.get("error", "") and len(answer.get("error", "")) < 500 and core.poll() is None, json.dumps(answer)[:200])

    # --- what never ended
    prompt("a" * 40)
    begin = time.time()
    answer = sub("(a*)*b")
    took = time.time() - begin
    check(":s (a*)*b over 40 letters: answered at once", answer.get("ok") is False and took < 20, f"{took:.1f} s {json.dumps(answer)[:200]}")
    begin = time.time()
    answer = sub(r"(a*)*\1b")
    took = time.time() - begin
    check(":s the same with a back-reference: stopped", answer.get("ok") is False and took < 270, f"{took:.1f} s {json.dumps(answer)[:200]}")
    prompt("a" * 100000)
    begin = time.time()
    answer = sub("(a|aa)*b")
    took = time.time() - begin
    check(":s over 100 KB: stopped by its budget", answer.get("ok") is False and took < 270, f"{took:.1f} s {json.dumps(answer)[:200]}")
    answer = sub("b", "x")
    check(":s a plain word over 100 KB: answered", answer.get("ok") is False and "nothing matches" in answer.get("error", ""), json.dumps(answer)[:200])
    answer = sub("aaa", "b")
    check(":s a replacement over 100 KB still works", answer.get("ok") is True and len(answer.get("data", {}).get("fill", "")) == 99998, json.dumps(answer)[:200])

    # --- a replacement of gigabytes for a small request
    begin = time.time()
    answer = sub("a", "b" * 500000, "g")
    took = time.time() - begin
    check(":s 500 KB for each of 100 000 letters: refused", answer.get("ok") is False and core.poll() is None and took < 60, f"{took:.1f} s {json.dumps(answer)[:200]}")
    answer = sub(".*", "$&" * 100000)
    check(":s $& 100 000 times: refused", answer.get("ok") is False and core.poll() is None, json.dumps(answer)[:200])
    check("another request after all that", ask({"cmd": "list"}, 10).get("ok") is True)

    # --- a command trigger: its regex still read; a pattern saved before these limits never stops the core
    answer = ask({"cmd": "task_add", "prompt": "REGEX-MET", "target": sid, "on_event": {"type": "command", "command": "echo build 42 ok", "regex": "build [0-9]+ ok", "every": 2}})
    check("a task on a regex made", answer.get("ok") is True, json.dumps(answer)[:200])
    answer = ask({"cmd": "task_add", "prompt": "REGEX-NEVER", "target": sid, "on_event": {"type": "command", "command": "echo aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "regex": "(a*)*b", "every": 2}})
    check("a task on (a*)*b made", answer.get("ok") is True, json.dumps(answer)[:200])
    never = answer.get("data", {}).get("id")
    answer = ask({"cmd": "task_add", "prompt": "SAVED", "target": sid, "not_before": 9999999999, "on_event": {"type": "command", "command": "echo aaa", "regex": "aaa", "every": 2}})
    saved = answer.get("data", {}).get("id")
    time.sleep(12)
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        prompts = log.read()
    check("command trigger: its regex met", "REGEX-MET" in prompts)
    check("command trigger: (a*)*b not met, the core goes on", "REGEX-NEVER" not in prompts and core.poll() is None)
    task = next((t for t in ask({"cmd": "task_list"})["data"] if t["id"] == never), {})
    check("command trigger: (a*)*b looked at", "exited 0" in task.get("on_event", {}).get("last", ""), json.dumps(task)[:300])
    core.terminate()
    core.wait(25)
    with open(f"{T}/data/state.json", encoding="utf-8") as state:
        data = json.load(state)
    found = 0
    for item in data.get("tasks", []):
        if item.get("id") in (never, saved):
            item["on"]["match"]["regex"] = "(" * 60000
            found += 1
    with open(f"{T}/data/state.json", "w", encoding="utf-8") as state:
        json.dump(data, state)
    check("a pattern of 60 000 parentheses put in state.json", found == 2)
    core = start("--restore")
    time.sleep(9)
    check("a pattern saved before these limits: the core stays up", core.poll() is None and ask({"cmd": "list"}, 10).get("ok") is True)
    task = next((t for t in ask({"cmd": "task_list"})["data"] if t["id"] == never), {})
    check("... told why it is not met", "regex" in task.get("on_event", {}).get("last", ""), json.dumps(task)[:300])
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
