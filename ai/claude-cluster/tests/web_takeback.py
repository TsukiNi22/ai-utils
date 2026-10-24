#!/usr/bin/env python3
# The page / the app takes a prompt back like the terminal (↑ in its empty prompt, Esc): the one queued last out of the queue (unqueue),
# a turn running asked back (rewind; not in the transcript of claude yet: the page stops it, its text back) - both allowed to a device of
# the web access, the session going on.
# Usage: tests/web_takeback.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A prompt taken back from the web page."""
import http.cookiejar
import json
import os
import subprocess
import sys
import tempfile
import time
import urllib.request

T = tempfile.mkdtemp(prefix="cc-webback-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 18797
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_CLAUDE_DELAY="20")
failed = False
jar = http.cookiejar.CookieJar()
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))


def check(name, ok, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)


def post(path, body):
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json",
        "Origin": f"http://127.0.0.1:{PORT}"})
    with web.open(request, timeout=15) as answer:
        return json.loads(answer.read() or b"{}")


for folder in ("cfg", "data", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[sessions]\nqueue_given = "end"\n[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    time.sleep(4)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post("/api/login", {"pin": pin, "name": "phone"})
    spawned = post("/api/cmd", {"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"})
    session = (spawned.get("data") or {}).get("id", "s1")
    time.sleep(1.5)
    post("/api/cmd", {"cmd": "send", "session": session, "text": "slow first"})
    time.sleep(1.5)
    post("/api/cmd", {"cmd": "send", "session": session, "text": "QUEUED-FROM-THE-PHONE"})
    time.sleep(0.8)
    back = post("/api/cmd", {"cmd": "unqueue", "session": session})
    check("unqueue from the page: the prompt queued back", (back.get("data") or {}).get("text") == "QUEUED-FROM-THE-PHONE", json.dumps(back)[:300])
    listed = next((s for s in post("/api/cmd", {"cmd": "list"}).get("data", []) if s.get("id") == session), {})
    check("out of the queue", listed.get("queued") == 0, json.dumps({k: listed.get(k) for k in ("queued", "state")}))
    rewind = post("/api/cmd", {"cmd": "rewind", "session": session, "back": 1})
    error = rewind.get("error", "")
    check("rewind allowed to the page (its own answer, not refused by the web access)", rewind.get("ok") is True or ("rewind" in error and "not allowed" not in error),
          json.dumps(rewind)[:300])
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
