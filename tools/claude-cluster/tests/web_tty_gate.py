#!/usr/bin/env python3
# The terminal of the page (origin web on the socket, its keys in a real terminal run with CLAUDE_CLUSTER_WEB_TTY): the actions a device may run by key, the same as its JSON API, the rest
# refused; the web access only read (the code blanked: a device never lets another one in); keep warm never writes [sessions] past [web] settings. Needs python3-pyte.
# Usage: tests/web_tty_gate.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""What a device does by key on the terminal page, and what stays on the computer."""
import fcntl
import http.cookiejar
import json
import os
import pty
import select
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import termios
import time
import urllib.request

import pyte

T = tempfile.mkdtemp(prefix="cc-ttygate-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 40500 + os.getpid() % 1500
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
           HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color", CLAUDE_CLUSTER_NO_SUPERVISOR="1", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 3))
failed = False

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

def device(**body: object) -> dict:
    return ask(dict(body, origin="web"))  # as the terminal of the page sends them (Manager::call)

def action(ident: str, session: str = "", text: str = "") -> dict:
    return device(cmd="action", id=ident, session=session, input=text)

def config_text() -> str:
    with open(f"{T}/cfg/config.toml", encoding="utf-8") as file:
        return file.read()

class Term:
    def __init__(self, env: dict, cols: int = 160, rows: int = 50) -> None:
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], env)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
        self.screen = pyte.Screen(cols, rows)
        self.stream = pyte.ByteStream(self.screen)

    def pump(self, seconds: float) -> None:
        end = time.time() + seconds
        while time.time() < end:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    self.stream.feed(os.read(self.fd, 1 << 16))
                except OSError:
                    return
                except TypeError:
                    pass

    def text(self) -> str:
        return "\n".join(line.rstrip() for line in self.screen.display)

    def send(self, data: bytes, wait: float = 0.8) -> None:
        os.write(self.fd, data)
        self.pump(wait)

    def kill(self) -> None:
        try:
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
        except OSError:
            pass

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nstart_menu = false\nmachine_days = 0\n[user]\nnickname = "t"\nemail = "t@t.t"\nnotes = "t"\nlanguage = "en"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\ntty_port = {PORT + 2}\nhttps = false\napprove = true\nshell = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=f"{T}/work")
term = None
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    sid = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"})["data"]["id"]
    time.sleep(1)
    state = ask({"cmd": "web", "action": "devices"})["data"]
    code = state["pin"]
    check("the computer sees its code", len(code.replace(" ", "")) == 6, json.dumps(state)[:200])

    # --- the web access: read only, the code blanked
    answer = device(cmd="web", action="devices")
    check("device: its list read", answer.get("ok") is True and "devices" in answer.get("data", {}), json.dumps(answer)[:200])
    check("device: the code blanked", answer.get("data", {}).get("pin") == "", json.dumps(answer)[:200])
    check("device: the state alone too", device(cmd="web").get("data", {}).get("pin") == "")
    for what in ("on", "off", "kick", "pin", "approve", "untrust", "ban", "unban"):
        answer = device(cmd="web", action=what, id="d1", ids="d1", answer="trust")
        check(f"device: web {what} refused", answer.get("ok") is False and "computer" in answer.get("error", ""), json.dumps(answer)[:200])
    check("the code unchanged by them", ask({"cmd": "web", "action": "devices"})["data"]["pin"] == code)
    services = device(cmd="services")
    check("device: services without the code", services.get("ok") is True and code not in json.dumps(services) and code.replace(" ", "") not in json.dumps(services), json.dumps(services.get("data", {}).get("web"))[:200])
    check("the computer: services with the code", code in json.dumps(ask({"cmd": "services"})))

    # --- the global session: its tool, never the approval of a device
    for what in ("approve", "untrust", "ban", "unban"):
        answer = ask({"cmd": "web", "origin": "mcp", "tool": "web_access", "action": what, "id": "d1", "answer": "trust"})
        check(f"global session: web {what} refused", answer.get("ok") is False and "not allowed" in answer.get("error", ""), json.dumps(answer)[:200])
    answer = ask({"cmd": "web", "origin": "mcp", "tool": "web_access"})
    check("global session: its address and its code still told", answer.get("ok") is True and answer["data"]["pin"] == code, json.dumps(answer)[:200])

    # --- the palette of a device: what it may run; never the code
    listed = device(cmd="actions", session=sid)
    ids = [a["id"] for a in listed.get("data", [])]
    check("palette: the code nowhere", code not in json.dumps(listed), json.dumps(listed)[:300])
    check("palette: its actions", all(a in ids for a in ("new_session", "interrupt", "rename", "close_session", "resume", "settings", "quit")), str(ids)[:400])
    hidden = [a for a in ids if a.startswith(("web_", "auth_", "voice_", "where_", "edit_", "docs")) or a in ("upgrade", "start_over", "mute", "machine_sync", "config", "providers")]
    check("palette: nothing of the computer", not hidden, str(hidden))
    check("the palette of the computer whole", "web_show" in [a["id"] for a in ask({"cmd": "actions", "session": sid}).get("data", [])])

    # --- by key: what the API of the page allows
    made = action("new_session", sid, f"{T}/work")
    new = made.get("data", {}).get("focus", "")
    check("key: a new session", made.get("ok") is True and new != "", json.dumps(made)[:200])
    answer = action("rename", new, "renamed")
    check("key: rename", answer.get("ok") is True and any(s.get("name") == "renamed" for s in ask({"cmd": "list"})["data"]), json.dumps(answer)[:200])
    for ident in ("interrupt", "resume", "send_next", "info", "stats", "jobs", "shells", "group", "vim", "focus:" + sid, "tasks_view", "cancel", "trash_view", "panel:tokens", "mode:plan", "model:opus", "effort:high", "category", "tags"):
        answer = action(ident, sid, "x" if ident in ("category", "tags") else "")
        check(f"key: {ident} not refused", answer.get("ok") is True, json.dumps(answer)[:200])
    answer = action("pin_last", sid)
    answer = action("unpin", sid)
    check("key: unpin only unpins (no update behind it)", answer.get("ok") is True and answer.get("data", {}).get("message") == "unpinned", json.dumps(answer)[:200])
    answer = action("close_session", new)
    check("key: close", answer.get("ok") is True and not any(s.get("id") == new for s in ask({"cmd": "list"})["data"]), json.dumps(answer)[:200])
    answer = action("send", sid, "A-PROMPT-BY-KEY")
    time.sleep(1.5)
    with open(f"{T}/fake.log", encoding="utf-8") as log:
        check("key: a prompt sent", answer.get("ok") is True and "A-PROMPT-BY-KEY" in log.read(), json.dumps(answer)[:200])
    answer = action("export", sid, "~/conversation.md")
    check("key: export of a .md still allowed", answer.get("ok") is True and os.path.exists(f"{T}/home/conversation.md"), json.dumps(answer)[:200])

    # --- by key: refused, the rest stays on the computer
    for ident in ("web_on", "web_off", "web_pin", "web_show", "web_kick", "web_kick:d1", "web_approve:d1:trust", "web_untrust:d1", "web_trust_ca", "auth_key:anthropic-api", "auth_logout:anthropic-api",
                  "voice_toggle", "voice_confirm", "voice_cancel", "voice_edit", "mute", "voice_setup", "where_folder", "where_terminal", "remote_on", "remote_off", "upgrade", "start_over", "purge", "purge_all",
                  "taxonomy_rename", "shells_kill", "config", "providers", "docs", "backend:anthropic-api", "jobs_scan", "made_up"):
        answer = action(ident, sid, "yes")
        check(f"key: {ident} refused", answer.get("ok") is False and "user's" in answer.get("error", ""), json.dumps(answer)[:200])
    mark = f"{T}/ran"
    for name, text in (("!command", f"!touch {mark}"), (":r !command", f":r !touch {mark}"), (":r", ":r /etc/hostname"), (":set", ":set web.shell=true"), ("/update", "/update")):
        answer = action("send", sid, text)
        check(f"key send: {name} refused like a prompt of the page", answer.get("ok") is False, json.dumps(answer)[:200])
    time.sleep(1)
    check("key send: nothing ran", not os.path.exists(mark))
    with open(f"{T}/home/.bashrc", "w", encoding="utf-8") as rc:
        rc.write("# the rc\n")
    for path in ("~/.bashrc", "~/x.sh"):
        answer = action("export", sid, path)
        check(f"key export: {path} refused", answer.get("ok") is False, json.dumps(answer)[:200])
    with open(f"{T}/home/.bashrc", encoding="utf-8") as rc:
        check("key export: ~/.bashrc untouched", rc.read() == "# the rc\n")
    check("key: a job refused without [web] settings orchestrator", "read only" in action("job_act:j1:kill", sid).get("error", ""))
    before = config_text()
    ask({"cmd": "ui_set", "key": "apply", "value": "mode:bypassPermissions"})
    answer = action("apply:new", sid)
    check("key: apply to the next sessions refused without [web] settings agent", "read only" in answer.get("error", "") and config_text() == before, json.dumps(answer)[:200])
    answer = device(cmd="apply", kind="mode", value="bypassPermissions", scope="new", session=sid)
    check("apply to the next sessions refused without [web] settings agent", answer.get("ok") is False and "read only" in answer.get("error", "") and config_text() == before, json.dumps(answer)[:200])

    # --- keep warm: the mark of a session; the mode of this computer only with [web] settings
    before = config_text()
    for name, answer in (("keep_warm on", device(cmd="keep_warm", session=sid, value="on")), ("keep_warm toggle", device(cmd="keep_warm", session=sid)), ("/keepwarm", device(cmd="send", session=sid, text="/keepwarm")),
                         ("/keepwarm on", device(cmd="send", session=sid, text="/keepwarm on")), (":keepwarm on", device(cmd="send", session=sid, text=":keepwarm on")), ("key keep_warm", action("keep_warm", sid)),
                         ("key keep_warm_force", action("keep_warm_force:" + sid, sid))):
        check(f"keep warm off: {name} refused from a device", answer.get("ok") is False and "read only" in answer.get("error", ""), json.dumps(answer)[:200])
    check("keep warm off: [sessions] untouched", config_text() == before and "keep_warm" not in before)
    answer = device(cmd="keep_warm", session=sid, value="off")
    check("keep warm off: the mark taken off still allowed", answer.get("ok") is True, json.dumps(answer)[:200])
    answer = device(cmd="send", session=sid, text="/keepwarm off")
    check("keep warm off: /keepwarm off still allowed", answer.get("ok") is True, json.dumps(answer)[:200])
    ask({"cmd": "settings_set", "section": "sessions", "key": "keep_warm", "value": "chosen"})
    answer = device(cmd="keep_warm", session=sid, value="on")
    check("keep warm chosen: the mark of a session allowed", answer.get("ok") is True, json.dumps(answer)[:200])
    ask({"cmd": "settings_set", "section": "sessions", "key": "keep_warm", "value": "off"})
    ask({"cmd": "settings_set", "section": "web", "key": "settings", "value": "sessions", "confirmed": True})
    answer = device(cmd="keep_warm", session=sid, value="on")
    check("[web] settings sessions: its device turns it on", answer.get("ok") is True and "chosen" in config_text(), json.dumps(answer)[:200])
    ask({"cmd": "settings_set", "section": "web", "key": "settings", "value": ""})
    ask({"cmd": "settings_set", "section": "sessions", "key": "keep_warm", "value": "off"})

    # --- the real terminal of the page: its keys, a stranger at the door
    env = dict(ENV, CLAUDE_CLUSTER_WEB_TTY="1")
    term = Term(env)
    term.pump(4)
    screen = term.text()
    check("terminal of the page: no code on its screen", code not in screen and code.replace(" ", "") not in screen, screen[:400])
    count = len(ask({"cmd": "list"})["data"])
    term.send(b"\x14", 1.2)  # Ctrl+T: a new session
    term.send(b"\r", 3)
    screen = term.text()
    check("terminal of the page: Ctrl+T Enter makes a session", "palette is the user's" not in screen and len(ask({"cmd": "list"})["data"]) == count + 1, screen[-400:])
    jar = http.cookiejar.CookieJar()
    stranger = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))
    login = urllib.request.Request(f"http://127.0.0.1:{PORT}/api/login", data=json.dumps({"pin": code.replace(" ", ""), "name": "the-stranger"}).encode(),
                                   headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
    stranger.open(login, timeout=30).read()
    term.pump(3)
    term.send(b"t", 1.5)  # the popup of a device, if it opens: allow and trust
    term.send(b"\x1b", 0.5)
    devices = ask({"cmd": "web", "action": "devices"})["data"]
    check("terminal of the page: a stranger never let in by its keys", not any(d["name"] == "the-stranger" and d["state"] == "connected" for d in devices["devices"]) and not devices["trusted"], json.dumps(devices["devices"])[:300])
    term.kill()
    term = None
    check("the core alive at the end", core.poll() is None)
finally:
    if term is not None:
        term.kill()
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
