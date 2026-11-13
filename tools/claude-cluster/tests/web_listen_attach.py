#!/usr/bin/env python3
# The microphone of the page and its attachments, two routes: /api/listen answers the text of its recording (voice-listen, a stand-in here); /api/attach stores
# the file and answers once, its bytes never given to the speech model (the voice code stood under /api/attach: /api/listen answered nothing, every upload went
# to the speech engine and a second HTTP answer followed the first).
# Usage: tests/web_listen_attach.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The routes of the page: its recording, its attachments."""
import http.cookiejar
import json
import os
import socket
import stat
import subprocess
import sys
import tempfile
import time
import urllib.request

T = tempfile.mkdtemp(prefix="cc-listen-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 20200 + os.getpid() % 2000
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1))
failed = False
jar = http.cookiejar.CookieJar()
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def post(path: str, data: bytes, kind: str = "application/json") -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=data, headers={"Content-Type": kind, "Origin": f"http://127.0.0.1:{PORT}"})
    with web.open(request, timeout=30) as answer:
        return json.loads(answer.read() or b"{}")

def heard() -> int:
    if not os.path.exists(f"{T}/listen.log"):
        return 0
    with open(f"{T}/listen.log", encoding="utf-8") as log:
        return len(log.readlines())

for folder in ("cfg", "data", "run", "cache", "home", "work", "bin"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
listen = f"{T}/bin/voice-listen"
with open(listen, "w", encoding="utf-8") as script:
    script.write(f'#!/usr/bin/env python3\nimport json, sys\nfor line in sys.stdin:\n    open("{T}/listen.log", "a").write(line)\n    print(json.dumps({{"text": "the sentence heard"}}), flush=True)\n')
os.chmod(listen, os.stat(listen).st_mode | stat.S_IEXEC)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\n[voice]\nlisten_command = "{listen}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post("/api/login", json.dumps({"pin": pin, "name": "phone"}).encode())
    sid = (post("/api/cmd", json.dumps({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).encode()).get("data") or {}).get("id", "s1")
    time.sleep(1)
    answer = post(f"/api/attach?session={sid}&name=notes.txt", b"my private notes\n", "application/octet-stream")
    check("an upload stored", answer.get("ok") is True and answer.get("attachment", {}).get("name") == "notes.txt", json.dumps(answer)[:200])
    listed = json.loads(web.open(f"http://127.0.0.1:{PORT}/api/attach/list?session={sid}", timeout=30).read())
    check("listed", len(listed.get("attachments", [])) == 1, json.dumps(listed)[:200])
    check("its bytes never given to the speech model", heard() == 0, str(heard()))
    # one answer on its connection, nothing after it
    cookies = "; ".join(f"{c.name}={c.value}" for c in jar)
    raw = socket.create_connection(("127.0.0.1", PORT), timeout=10)
    raw.sendall(f"GET /api/attach/list?session={sid} HTTP/1.1\r\nHost: 127.0.0.1:{PORT}\r\nCookie: {cookies}\r\n\r\n".encode())
    reply = b""
    while True:
        chunk = raw.recv(65536)
        if not chunk:
            break
        reply += chunk
    raw.close()
    check("one HTTP answer on its connection", reply.count(b"HTTP/1.1 ") == 1, reply[:200].decode(errors="replace"))
    answer = post("/api/listen", b"RIFF....WAVEfmt ", "audio/wav")
    check("/api/listen answers the text of its recording", answer.get("text") == "the sentence heard", json.dumps(answer)[:200])
    check("given to the speech model once", heard() == 1, str(heard()))
    check("the core alive at the end", core.poll() is None)
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
