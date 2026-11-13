#!/usr/bin/env python3
# What a device of the web page may not do with [web] shell off and no [web] settings: a shell of this computer, whatever the way (a prompt !x, its hidden
# characters first, :r, a task or a schedule running a command), the settings that protect it (:set), a file written anywhere (/export, :w), the update;
# with [web] shell on the same !command runs. A header never takes what a display or a Host says (a type with a line break, a Host with a CR).
# Usage: tests/web_gate.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The gate of the web page: its shell, its settings, its files."""
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

T = tempfile.mkdtemp(prefix="cc-webgate-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 18000 + os.getpid() % 2000
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

def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(15)
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

def post(path: str, body: object) -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json",
        "Origin": f"http://127.0.0.1:{PORT}"})
    with web.open(request, timeout=15) as answer:
        return json.loads(answer.read() or b"{}")

def cmd(**body: object) -> dict:
    return post("/api/cmd", body)

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\n')
with open(f"{T}/home/.bashrc", "w", encoding="utf-8") as rc:
    rc.write("# the rc of the user\n")
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post("/api/login", {"pin": pin, "name": "phone"})
    sid = (cmd(cmd="spawn", cwd=f"{T}/work", name="worker").get("data") or {}).get("id", "s1")
    time.sleep(1)
    mark = f"{T}/ran"
    run = f"touch {mark}"
    tag = chr(0xE0041)  # a hidden character: dropped when the prompt is read, before its first letter here

    def refused(name: str, answer: dict) -> None:
        check(name, answer.get("ok") is False and not os.path.exists(mark), json.dumps(answer)[:200])

    refused("!command refused", cmd(cmd="send", session=sid, text=f"!{run}"))
    refused("... its hidden characters first", cmd(cmd="send", session=sid, text=f"{tag}!{run}"))
    refused("... raw (the way of an interface)", cmd(cmd="send", session=sid, text=f"!{run}", raw=True))
    refused("... to the Tasks session", cmd(cmd="send", session="tasks", text=f"!{run}"))
    refused(":r !command refused", cmd(cmd="send", session=sid, text=f":r !{run}"))
    answer = cmd(cmd="send", session=sid, text=":r /etc/hostname")
    check(":r <file> refused (no file of this computer read)", answer.get("ok") is False and "fill" not in json.dumps(answer), json.dumps(answer)[:200])
    refused("a schedule running a command refused", cmd(cmd="schedule_add", when="+1s", action="command", command=run))
    refused("a schedule with if.command refused", cmd(cmd="schedule_add", when="+1s", text="x", session=sid, **{"if": {"command": run}}))
    refused("a task with on_event command refused", cmd(cmd="task_add", prompt="x", target=sid, on_event={"type": "command", "command": run}))
    time.sleep(3)
    check("nothing of them ran", not os.path.exists(mark))
    answer = cmd(cmd="send", session=sid, text=":set web.shell=true")
    check(":set web.shell refused", answer.get("ok") is False and "shell" not in open(f"{T}/cfg/config.toml", encoding="utf-8").read().split("[web]")[1], json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text=":set shell=true")
    check(":set shell (without its section) refused", answer.get("ok") is False, json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text=f"{tag}:set! web.settings=all")
    check(":set! web.settings, hidden characters first, refused", answer.get("ok") is False and "all" not in open(f"{T}/cfg/config.toml", encoding="utf-8").read().split("[web]")[1], json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text="/export ~/.bashrc")
    check("/export over a hidden file refused", answer.get("ok") is False and open(f"{T}/home/.bashrc", encoding="utf-8").read() == "# the rc of the user\n", json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text=":w ~/.bashrc")
    check(":w over a hidden file refused", answer.get("ok") is False and open(f"{T}/home/.bashrc", encoding="utf-8").read() == "# the rc of the user\n", json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text="/update")
    check("/update refused", answer.get("ok") is False, json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text=":update")
    check(":update refused", answer.get("ok") is False, json.dumps(answer)[:200])
    # a space after the colon, the /command of the same name: read as they run
    answer = cmd(cmd="send", session=sid, text=": set web.shell=true")
    check(": set web.shell refused", answer.get("ok") is False and "shell" not in open(f"{T}/cfg/config.toml", encoding="utf-8").read().split("[web]")[1], json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text=": r /etc/hostname")
    check(": r <file> refused", answer.get("ok") is False and "fill" not in json.dumps(answer), json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text=": w ~/.bashrc")
    check(": w over a hidden file refused", answer.get("ok") is False and open(f"{T}/home/.bashrc", encoding="utf-8").read() == "# the rc of the user\n", json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text=":export ~/.bashrc")
    check(":export over a hidden file refused", answer.get("ok") is False and open(f"{T}/home/.bashrc", encoding="utf-8").read() == "# the rc of the user\n", json.dumps(answer)[:200])
    refused(": !command refused", cmd(cmd="send", session=sid, text=f": !{run}"))
    answer = cmd(cmd="send", session=sid, text="/export ~/conversation.md")
    check("/export of a .md out of the hidden folders still allowed", answer.get("ok") is True and os.path.exists(f"{T}/home/conversation.md"), json.dumps(answer)[:200])
    # what is pasted is never run: its marks first (the first line of a text copied from a page), it goes to the agent as text
    pasted = "".join(chr(0xE0000 + ord(c)) for c in "paste{") + chr(0xE007F)
    closed = "".join(chr(0xE0000 + ord(c)) for c in "}paste") + chr(0xE007F)
    answer = cmd(cmd="send", session=sid, text=f"{pasted}!{run}{closed}")
    time.sleep(1.5)
    check("a pasted !command is a prompt, never run", answer.get("ok") is True and not os.path.exists(mark), json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text=f"{pasted}:set web.shell=true{closed}")
    check("a pasted :set is a prompt, never run", answer.get("ok") is True and "shell" not in open(f"{T}/cfg/config.toml", encoding="utf-8").read().split("[web]")[1], json.dumps(answer)[:200])
    answer = cmd(cmd="send", session=sid, text="a plain prompt")
    check("a plain prompt still sent", answer.get("ok") is True, json.dumps(answer)[:200])

    # a display: what the agent writes never goes in a header, never a hidden file served
    shown = f"{T}/home/report.txt"
    with open(shown, "w", encoding="utf-8") as report:
        report.write("the report\n")
    secret = f"{T}/home/.secret"
    with open(secret, "w", encoding="utf-8") as hidden:
        hidden.write("the secret\n")
    made = ask({"cmd": "display", "session": sid, "display": {"type": "markdown", "text": "x", "files": [{"path": shown, "name": "report.txt", "mime": "text/html\r\nX-Evil: 1"}]}})
    check("a display of a file made", made.get("ok") is True, json.dumps(made)[:200])
    time.sleep(0.5)
    entries = ask({"cmd": "transcript", "session": sid, "from": -50, "count": 50}).get("data", {}).get("entries", [])
    index = next((e["index"] for e in reversed(entries) if e["kind"] == "display"), -1)
    with web.open(f"http://127.0.0.1:{PORT}/api/file?session={sid}&index={index}&i=0", timeout=10) as answer:
        headers = str(answer.headers)
        body = answer.read()
    check("its type with a line break: no header injected", "X-Evil" not in headers and "application/octet-stream" in headers and body == b"the report\n", headers[:300])
    made = ask({"cmd": "display", "session": sid, "display": {"type": "markdown", "text": "x", "files": [{"path": secret, "name": "x"}]}})
    time.sleep(0.5)
    entries = ask({"cmd": "transcript", "session": sid, "from": -50, "count": 50}).get("data", {}).get("entries", [])
    index = next((e["index"] for e in reversed(entries) if e["kind"] == "display"), -1)
    try:
        with web.open(f"http://127.0.0.1:{PORT}/api/file?session={sid}&index={index}&i=0", timeout=10) as answer:
            code = answer.status
    except urllib.error.HTTPError as e:
        code = e.code
    check("a hidden file of a display never served", code == 404, str(code))

    # a Host with a carriage return: refused, never given back in a header
    raw = socket.create_connection(("127.0.0.1", PORT), timeout=10)
    raw.sendall(f"GET / HTTP/1.1\r\nHost: 127.0.0.1:{PORT}\rEvil\r\n\r\n".encode())
    reply = raw.recv(4096).decode(errors="replace")
    raw.close()
    check("a Host with a control character refused", " 403 " in reply.split("\r\n")[0], reply[:120])
    # a big body: read for a device really connected only (made up cookies: refused before it, told)
    s = socket.create_connection(("127.0.0.1", PORT), timeout=15)
    s.sendall(f"POST /api/attach?session=s1&name=x HTTP/1.1\r\nHost: 127.0.0.1:{PORT}\r\nOrigin: http://127.0.0.1:{PORT}\r\nCookie: cc_web=made-up; cc_dev=made-up\r\n"
              "Content-Length: 10000000\r\n\r\n".encode())
    reply = s.recv(4096).decode("latin-1")
    s.close()
    check("a big body with made up cookies: refused before it is read", " 401 " in reply.split("\r\n")[0], reply[:120])

    # [web] shell on (the computer allows it): the same prompt runs
    ask({"cmd": "settings_set", "section": "web", "key": "shell", "value": "true", "confirmed": True})
    time.sleep(1.5)
    answer = cmd(cmd="send", session=sid, text=f"!{run}")
    deadline = time.time() + 15
    while time.time() < deadline and not os.path.exists(mark):
        time.sleep(0.3)
    check("[web] shell on: the !command runs", os.path.exists(mark), json.dumps(answer)[:200])
    check("the core alive at the end", core.poll() is None)
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
