#!/usr/bin/env python3
# The terminal of the page opened by several requests at once (two tabs of one device): one terminal kept - a second one dropped with its reader thread running ended
# the core (std::terminate). A quit with a long poll of an interface and of the page still running: a normal end (a request using what was destroyed: a crash, the
# watcher then starting a core nobody wants).
# Usage: tests/web_tty_race.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The terminal of the page opened at once; the quit under long polls."""
import http.cookiejar
import json
import os
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request

T = tempfile.mkdtemp(prefix="cc-ttyrace-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 22300 + os.getpid() % 2000
TTY = PORT + 2
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1), CLAUDE_CLUSTER_NO_SUPERVISOR="1")
failed = False
jar = http.cookiejar.CookieJar()
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def post(port: int, path: str, body: object) -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json",
        "Origin": f"http://127.0.0.1:{port}"})
    with web.open(request, timeout=30) as answer:
        return json.loads(answer.read() or b"{}")

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\ntty_port = {TTY}\nhttps = false\napprove = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post(PORT, "/api/login", {"pin": pin, "name": "phone"})
    answers = []

    def opens() -> None:
        try:
            answers.append(post(TTY, "/api/tty/open", {"cols": 80, "rows": 24}))
        except (urllib.error.URLError, OSError, ValueError) as e:
            answers.append({"error": str(e)})

    for _ in range(3):
        threads = [threading.Thread(target=opens) for _ in range(8)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        post(TTY, "/api/tty/close", {})
    check("8 opens at once, 3 times: the core alive", core.poll() is None, str(core.poll()))
    check("each one answered", all("cols" in a for a in answers), json.dumps(answers[:3])[:300])

    # the quit with long polls running: the page, an interface (sync), a wait
    def polls() -> None:
        try:
            web.open(f"http://127.0.0.1:{PORT}/api/state?v=1", timeout=20).read()
            version = json.loads(web.open(f"http://127.0.0.1:{PORT}/api/state", timeout=20).read()).get("version", 0)
            web.open(f"http://127.0.0.1:{PORT}/api/state?v={version}", timeout=20).read()
        except (urllib.error.URLError, OSError, ValueError):
            pass

    def waits() -> None:
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(20)
        try:
            s.connect(f"{T}/sock")
            s.sendall(b'{"cmd": "sync", "v": 1, "build": "x", "client": "t"}\n{"cmd": "wait", "session": "g", "timeout": 100}\n')
            s.recv(1 << 16)
        except OSError:
            pass
        s.close()

    threads = [threading.Thread(target=polls, daemon=True) for _ in range(4)] + [threading.Thread(target=waits, daemon=True) for _ in range(4)]
    for thread in threads:
        thread.start()
    time.sleep(1)
    core.send_signal(signal.SIGTERM)
    code = core.wait(30)
    check("a quit under long polls: a normal end", code == 0, str(code))
finally:
    if core.poll() is None:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
