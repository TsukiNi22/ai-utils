#!/usr/bin/env python3
# What keeps a hidden file from a device and from the global session: a NUL in a path, a link followed by .., a display block given with a pin, an attachment of the folder of a
# session, the default file of /export (the name of a session is no path), the files of the config behind a setting, the settings that hold a program.
# Usage: tests/hidden_paths.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""hidden_path and the gates built on it."""
import base64
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

T = tempfile.mkdtemp(prefix="cc-hidden-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
PORT = 37500 + os.getpid() % 1500
HOME = f"{T}/home"
KEY = "THE-PRIVATE-KEY-MARKER\n"
RC = "# the rc of the user\n"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=HOME, FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1))
failed = False
web = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))

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

def post(path: str, body: dict) -> dict:
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
    with web.open(request, timeout=30) as answer:
        return json.loads(answer.read() or b"{}")

def device(**body: object) -> dict:
    return post("/api/cmd", body)

def mcp(tool: str, cmd: str, **body: object) -> dict:
    return ask(dict(body, cmd=cmd, origin="mcp", tool=tool))

def refused(name: str, answer: dict, word: str = "hidden") -> None:
    check(name, answer.get("ok") is False and word in answer.get("error", ""), json.dumps(answer)[:220])

def text_of(path: str) -> str:
    with open(path, encoding="utf-8") as file:
        return file.read()

def last_display(sid: str, wanted: str | None = None) -> tuple:
    entries = ask({"cmd": "transcript", "session": sid, "from": -50, "count": 50}).get("data", {}).get("entries", [])
    for entry in reversed(entries):
        if entry["kind"] == "display":
            shown = json.loads(entry["text"])
            if wanted is None or shown.get("type") == wanted:
                return entry["index"], shown
    return -1, {}

for folder in ("cfg", "data", "run", "cache", "home", "home/.ssh", "home/.ssh/sub", "work", "out", "project"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{HOME}/.ssh/id_rsa", "w", encoding="utf-8") as file:
    file.write(KEY)
with open(f"{HOME}/.bashrc", "w", encoding="utf-8") as file:
    file.write(RC)
with open(f"{HOME}/notes.txt", "w", encoding="utf-8") as file:
    file.write("my notes\n")
with open(f"{T}/project/readme.txt", "w", encoding="utf-8") as file:
    file.write("the readme\n")
with open(f"{T}/project/.env", "w", encoding="utf-8") as file:
    file.write("SECRET=1\n")
with open(f"{T}/out/song.wav", "wb") as file:
    file.write(b"RIFF\0\0\0\0WAVEfmt the song")
os.symlink(f"{HOME}/.ssh/sub", f"{T}/out/link")  # a folder link into a hidden tree, out of the hidden folders (a dotfile manager, one an agent made)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\nhttps = false\napprove = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd="/")
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    post("/api/login", {"pin": pin, "name": "phone"})
    sid = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"})["data"]["id"]
    time.sleep(1)
    device(cmd="send", session=sid, text="a prompt\nnext line")
    time.sleep(1.5)
    up = "/.." * 30
    nul = f"{HOME}/.ssh/id_rsa\u0000{up}/x"

    # --- a NUL in a path: the kernel stops at it, the check read past it
    refused("NUL: /export over ~/.bashrc from a device", device(cmd="send", session=sid, text=f"/export {HOME}/.bashrc\u0000{up}/x.md"), "not allowed")
    refused("NUL: :w over ~/.bashrc from a device", device(cmd="send", session=sid, text=f":w {HOME}/.bashrc\u0000{up}/x.md"), "not allowed")
    refused("NUL: export over ~/.bashrc from the global session", mcp("export", "export", session=sid, path=f"{HOME}/.bashrc\u0000{up}/x.md"), "not allowed")
    check("NUL: ~/.bashrc untouched", text_of(f"{HOME}/.bashrc") == RC)
    refused("NUL: context from the global session", mcp("session_context", "context", session=sid, source=nul))
    refused("NUL: display from the global session", mcp("session_display", "display", session=sid, display={"type": "audio", "path": nul}))
    refused("NUL: pin from a device", device(cmd="pin", session=sid, display={"type": "markdown", "text": "x", "files": [{"path": nul, "name": "x"}]}))
    made = ask({"cmd": "display", "session": sid, "display": {"type": "markdown", "text": "x", "files": [{"path": nul, "name": "x"}]}})
    time.sleep(0.5)
    index, _ = last_display(sid, "markdown")
    try:
        with web.open(f"http://127.0.0.1:{PORT}/api/file?session={sid}&index={index}&i=0", timeout=10) as answer:
            code = answer.status
    except urllib.error.HTTPError as e:
        code = e.code
    check("NUL: /api/file never serves it", made.get("ok") is True and code == 404, str(code))
    device(cmd="task_add", prompt="say hi", target=sid, expect=nul)
    time.sleep(4)
    tasks = json.dumps(device(cmd="task_list"))
    check("NUL: task expect never reads it", "THE-PRIVATE-KEY-MARKER" not in tasks, tasks[:300])
    time.sleep(0.5)
    check("NUL: the agent never got the key", "THE-PRIVATE-KEY-MARKER" not in text_of(f"{T}/fake.log"))

    # --- a link followed by ..: the kernel goes to the target of the link, then to its parent
    through = f"{T}/out/link/../id_rsa"
    check("link: the kernel reads the key by this path", text_of(through) == KEY)
    refused("link: display from the global session", mcp("session_display", "display", session=sid, display={"type": "audio", "path": through}))
    refused("link: context from the global session", mcp("session_context", "context", session=sid, source=through))
    refused("link: pin from a device", device(cmd="pin", session=sid, display={"type": "audio", "path": through}))
    refused("link: /export into the hidden folder from a device", device(cmd="send", session=sid, text=f"/export {T}/out/link/../notes.md"), "not allowed")
    check("link: nothing written in ~/.ssh", not os.path.exists(f"{HOME}/.ssh/notes.md"))

    # --- a display block with a pin: the same gate as a display
    refused("pin: audio ~/.ssh/id_rsa from a device", device(cmd="pin", session=sid, display={"type": "audio", "path": "~/.ssh/id_rsa", "id": "leak"}))
    got = device(cmd="display_get", session=sid, id="leak")
    check("pin: nothing to read back", got.get("ok") is False, json.dumps(got)[:200])
    refused("pin: audio /proc/self/environ from a device", device(cmd="pin", session=sid, display={"type": "audio", "path": "/proc/self/environ", "id": "leak2"}))
    refused("pin: audio ~/.ssh/id_rsa from the global session", mcp("session_pin", "pin", session=sid, display={"type": "audio", "path": "~/.ssh/id_rsa", "id": "leak3"}))
    refused("pin: a file of a block from the global session", mcp("session_pin", "pin", session=sid, display={"type": "file", "paths": ["~/.ssh/id_rsa"], "id": "leak4"}))
    fine = device(cmd="pin", session=sid, display={"type": "audio", "path": f"{T}/out/song.wav", "id": "song"})
    shown = device(cmd="display_get", session=sid, id="song")
    check("pin: a file out of the hidden folders still pinned and read", fine.get("ok") is True and b"the song" in base64.b64decode(shown.get("data", {}).get("data", "")), json.dumps(fine)[:200])
    fine = device(cmd="pin", session=sid, display="song")
    check("pin: a display by its id still pinned", fine.get("ok") is True, json.dumps(fine)[:200])

    # --- what the global session attaches: never a hidden file, whatever the folder of the session
    inhome = ask({"cmd": "spawn", "cwd": HOME, "name": "inhome"})["data"]["id"]
    project = ask({"cmd": "spawn", "cwd": f"{T}/project", "name": "project"})["data"]["id"]
    time.sleep(1)

    def attach(session: str, path: str, text: str) -> dict:
        return mcp("session_send", "send", session=session, text=text, attachments=[path])

    refused("attach: ~/.ssh/id_rsa to a session in the home", attach(inhome, "~/.ssh/id_rsa", "LEAK-1"))
    refused("attach: ~/.bashrc to a session in the home", attach(inhome, "~/.bashrc", "LEAK-2"))
    refused("attach: the .env of its project", attach(project, f"{T}/project/.env", "LEAK-3"))
    fine = attach(inhome, "~/notes.txt", "ATTACH-HOME")
    check("attach: a file of the home still attached", fine.get("ok") is True, json.dumps(fine)[:200])
    fine = attach(project, f"{T}/project/readme.txt", "ATTACH-PROJECT")
    check("attach: a file of its project still attached", fine.get("ok") is True, json.dumps(fine)[:200])
    time.sleep(1.5)
    log = "\n".join(line for line in text_of(f"{T}/fake.log").split("\n") if line.startswith("PROMPT "))
    check("attach: nothing hidden told to the agents", "LEAK-" not in log and "-id_rsa" not in log and "-bashrc" not in log and "-env" not in log, log[-300:])
    check("attach: the allowed ones arrived", "ATTACH-HOME" in log and "ATTACH-PROJECT" in log)

    # --- /export, :w without a path: the name of a session is no path
    for name in (".claude/CLAUDE", "../../outside", ".bashrc", ".."):
        renamed = device(cmd="rename", session=sid, name=name)
        answer = device(cmd="send", session=sid, text="/export")
        check(f"/export of a session named {name}: done", renamed.get("ok") is True and answer.get("ok") is True, json.dumps(answer)[:200])
    check("/export: nothing in a hidden folder", not os.path.exists(f"{HOME}/.claude"))
    check("/export: nothing over ~/.bashrc", text_of(f"{HOME}/.bashrc") == RC)
    check("/export: nothing out of the home", not os.path.exists(f"{T}/outside.md"))
    check("/export: a flat file of the home", os.path.exists(f"{HOME}/claude_CLAUDE.md") and os.path.exists(f"{HOME}/bashrc.md"), str(os.listdir(HOME)))
    device(cmd="rename", session=sid, name=".claude/AGENTS")
    device(cmd="send", session=sid, text=":w")
    mcp("session_send", "send", session=sid, text="/export")
    check(":w, /export of the global session: the same file", os.path.exists(f"{HOME}/claude_AGENTS.md") and not os.path.exists(f"{HOME}/.claude"), str(os.listdir(HOME)))
    device(cmd="rename", session=sid, name="my.project")
    device(cmd="send", session=sid, text="/export")
    check("/export of a plain name: its file as before", os.path.exists(f"{HOME}/my.project.md"), str(os.listdir(HOME)))
    device(cmd="rename", session=sid, name="worker")

    # --- the files of the config behind a setting
    before = text_of(f"{T}/cfg/config.toml")
    for section, key, value in (("voice", "words", "~/.bashrc"), ("voice", "vocabulary", "~/.ssh/id_rsa"), ("global", "prompt", "~/.ssh/id_rsa"), ("sessions", "prompt", "~/.bashrc"),
                                ("ui", "terminal", f"{T}/evil"), ("user", "editor", f"{T}/evil"), ("ui", "editor", f"{T}/evil")):
        refused(f"settings_set {section}.{key} from the global session", mcp("settings_set", "settings_set", section=section, key=key, value=value), "not allowed")
    check("config.toml untouched by them", text_of(f"{T}/cfg/config.toml") == before)
    for table, line in (("user", 'editor = "/x/evil"'), ("ui", 'terminal = "/x/evil"'), ("voice", 'words = "~/.bashrc"'), ("global", 'prompt = "~/.ssh/id_rsa"')):
        changed = before.replace(f"[{table}]\n", f"[{table}]\n{line}\n") if f"[{table}]\n" in before else before + f"\n[{table}]\n{line}\n"
        refused(f"config_write [{table}] {line.split()[0]} from the global session", mcp("config_write", "config_write", name="config", content=changed), "changed by the user only")
    check("config.toml untouched by them (config_write)", text_of(f"{T}/cfg/config.toml") == before)
    answer = mcp("config_write", "config_write", name="config", content=before + "\n# a comment of the global session\n")
    check("config_write of something else still written", answer.get("ok") is True and "a comment of the global session" in text_of(f"{T}/cfg/config.toml"), json.dumps(answer)[:200])
    answer = mcp("config_read", "config_read", name="voice_words")
    check("voice_words in its place: read by the global session", answer.get("ok") is True and "content" in answer.get("data", {}), json.dumps(answer)[:200])
    answer = mcp("config_write", "config_write", name="voice_words", content="[cancel]\nstop\n[send]\ngo\n")
    check("voice_words in its place: written by the global session", answer.get("ok") is True, json.dumps(answer)[:200])
    answer = ask({"cmd": "settings_set", "section": "voice", "key": "words", "value": "~/.bashrc"})
    check("the user still sets it", answer.get("ok") is True, json.dumps(answer)[:200])
    refused("voice_words on ~/.bashrc: config_write refused", mcp("config_write", "config_write", name="voice_words", content="touch /tmp/x\n"), "not allowed")
    refused("voice_words on ~/.bashrc: config_read refused", mcp("config_read", "config_read", name="voice_words"), "not allowed")
    check("~/.bashrc untouched", text_of(f"{HOME}/.bashrc") == RC)
    ask({"cmd": "settings_set", "section": "voice", "key": "vocabulary", "value": "~/.ssh/id_rsa"})
    answer = mcp("config_read", "config_read", name="vocabulary")
    check("vocabulary on the key: config_read refused", answer.get("ok") is False and KEY.strip() not in json.dumps(answer), json.dumps(answer)[:200])
    ask({"cmd": "settings_set", "section": "voice", "key": "words", "value": f"{T}/cfg/config.toml"})
    refused("voice_words on config.toml: config_write refused (its protected settings)", mcp("config_write", "config_write", name="voice_words", content="[web]\nshell = true\n"), "not allowed")
    ask({"cmd": "settings_set", "section": "voice", "key": "words", "value": "~/words.txt"})
    answer = mcp("config_write", "config_write", name="voice_words", content="[cancel]\nstop\n[send]\ngo\n")
    check("a .txt out of the hidden folders: still written", answer.get("ok") is True and os.path.exists(f"{HOME}/words.txt"), json.dumps(answer)[:200])
    ask({"cmd": "settings_set", "section": "voice", "key": "words", "value": ""})

    # --- a program in a setting: confirmed first, wherever it is written
    for section, key in (("ui", "terminal"), ("user", "editor")):
        answer = ask({"cmd": "settings_set", "section": section, "key": key, "value": "/x/evil"})
        check(f"{section}.{key}: confirmed first", answer.get("ok") is False and "confirm" in answer.get("error", ""), json.dumps(answer)[:200])
        answer = ask({"cmd": "settings_set", "section": section, "key": key, "value": "/x/evil", "confirmed": True})
        check(f"{section}.{key}: the user still sets it", answer.get("ok") is True, json.dumps(answer)[:200])
    check("the core alive at the end", core.poll() is None)
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
