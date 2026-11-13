#!/usr/bin/env python3
# A recording being recognized for the page holds nothing else: a schedule due meanwhile runs on time (the tick of the whole core waited for the end of the recognition, up to 2 minutes per recording:
# schedules, tasks, keep warm, the quit), and the quit ends it at once, its voice-listen gone with it.
# Usage: tests/web_listen_tick.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A recognition of the page never stops the tick of the core."""
import http.cookiejar
import json
import os
import signal
import socket
import stat
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request

T = tempfile.mkdtemp(prefix="cc-listentick-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")

def free_ports() -> int:
    # 3 ports in a row free here, under the range a client of this computer takes its port from (one taken: that port of the core not opened)
    for base in range(10000 + os.getpid() % 7000, 17100, 3):
        try:
            with socket.socket() as first, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as second, socket.socket() as third:
                first.bind(("", base))
                second.bind(("", base + 1))
                third.bind(("", base + 2))
            return base
        except OSError:
            continue
    return 17100

PORT = free_ports()
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
           HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1), CLAUDE_CLUSTER_NO_SUPERVISOR="1")
failed = False
jar = http.cookiejar.CookieJar()
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def post(path: str, data: bytes, kind: str = "application/json", timeout: int = 90) -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=data, headers={"Content-Type": kind, "Origin": f"http://127.0.0.1:{PORT}"})
    with web.open(request, timeout=timeout) as answer:
        return json.loads(answer.read() or b"{}")

def cmd(**body: object) -> dict:
    return post("/api/cmd", json.dumps(body).encode())

for folder in ("cfg", "data", "run", "cache", "home", "work", "bin"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
listen = f"{T}/bin/voice-listen"
with open(f"{T}/delay", "w", encoding="utf-8") as delay:
    delay.write("15")
with open(listen, "w", encoding="utf-8") as script:
    script.write(f'#!/usr/bin/env python3\nimport json, sys, time\nfor line in sys.stdin:\n    time.sleep(float(open("{T}/delay").read()))\n    print(json.dumps({{"text": "the sentence heard"}}), flush=True)\n')
os.chmod(listen, os.stat(listen).st_mode | stat.S_IEXEC)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\n[voice]\nlisten_command = "{listen}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
answers = {}

def recording(key: str) -> None:
    try:
        answers[key] = post("/api/listen", b"RIFF....WAVEfmt ", "audio/wav")
    except (urllib.error.URLError, OSError, ValueError) as error:
        answers[key] = {"error": str(error)}

try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post("/api/login", json.dumps({"pin": pin, "name": "phone"}).encode())
    # a reminder in 3 s, run by the tick of the core; a recording of 15 s sent first
    start = time.time()
    worker = threading.Thread(target=recording, args=("first",))
    worker.start()
    time.sleep(0.5)
    made = cmd(cmd="schedule_add", when="+3s", action="notify", text="tick ran")
    check("a schedule made while it is recognized", made.get("ok") is True, json.dumps(made)[:200])
    ran = None
    while time.time() - start < 20 and ran is None:
        planned = cmd(cmd="schedule_list").get("data") or []
        if planned and planned[0].get("status") != "waiting":
            ran = time.time() - start
        time.sleep(0.25)
    busy = worker.is_alive()
    check("it runs on time, the recognition still running (the tick does not wait for it)", ran is not None and ran < 13 and busy, f"after {ran} s, still recognized: {busy}")
    worker.join(30)
    check("the recording answered", answers.get("first", {}).get("text") == "the sentence heard", json.dumps(answers.get("first"))[:200])
    check("after the 15 s of its recognition", time.time() - start >= 14.5, str(time.time() - start))

    # the quit during a recognition of a minute: a normal end at once, no voice-listen left
    with open(f"{T}/delay", "w", encoding="utf-8") as delay:
        delay.write("60")
    worker = threading.Thread(target=recording, args=("last",), daemon=True)
    worker.start()
    time.sleep(1.5)
    start = time.time()
    core.send_signal(signal.SIGTERM)
    try:
        code = core.wait(15)
    except subprocess.TimeoutExpired:
        code = "still running"
    check("the quit does not wait for it", code == 0, f"{code} after {time.time() - start:.1f} s")
    time.sleep(1)
    left = subprocess.run(["pgrep", "-f", listen], capture_output=True, text=True).stdout.split()
    check("its voice-listen gone with it", not left, str(left))
finally:
    if core.poll() is None:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
