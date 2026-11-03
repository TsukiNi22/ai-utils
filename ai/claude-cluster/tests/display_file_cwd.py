#!/usr/bin/env python3
# A file display with a relative path: judged where it lands, joined to its folder. The gate of the global session and of the page reads the path alone: a cwd in ~/.ssh, a
# .. out of the folder, a link in it, the session in a hidden folder gave a preview of a hidden picture. What is not hidden keeps its card and its preview, and the user of the
# computer (no origin) still shows a hidden file by its whole path.
# Usage: tests/display_file_cwd.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The folder of a file display against the hidden files."""
import base64
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-filecwd-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
HOME = f"{T}/home"
HIDDEN = b"\x89PNG\r\n\x1a\nHIDDEN-PICTURE"
SHOWN = b"\x89PNG\r\n\x1a\nSHOWN-PICTURE"
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

def displays(sid: str) -> list:
    entries = (ask({"cmd": "transcript", "session": sid, "from": 0, "count": 500}).get("data") or {}).get("entries", [])
    return [json.loads(e["text"]) for e in entries if e["kind"] == "display"]

def previews(sid: str) -> list:
    return [base64.b64decode(f.get("preview", "")) for d in displays(sid) for f in d.get("files", [])]

def refused(name: str, sid: str, request: dict) -> None:
    before = len(displays(sid))
    answer = ask(dict(request, session=sid))
    check(name + ": refused", answer.get("ok") is False and "hidden" in answer.get("error", ""), json.dumps(answer)[:200])
    check(name + ": nothing shown", len(displays(sid)) == before and HIDDEN not in previews(sid))

for folder in ("cfg", "data", "run", "cache", "home", "home/.ssh", "home/.config/tool", "work", "work/sub"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
for path, content in ((f"{HOME}/.ssh/shot.png", HIDDEN), (f"{HOME}/.config/tool/shot.png", HIDDEN), (f"{T}/work/shown.png", SHOWN), (f"{T}/work/sub/other.png", SHOWN)):
    with open(path, "wb") as picture:
        picture.write(content)
os.symlink(f"{HOME}/.ssh", f"{T}/work/link")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n[agent]\nglobal_pin = true\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd="/")
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
    inside = (ask({"cmd": "spawn", "cwd": f"{HOME}/.config/tool", "name": "inside"}).get("data") or {}).get("id", "")
    time.sleep(1)

    # the global session (origin mcp), a device (origin web, its pin)
    for origin in ("mcp", "web"):
        block = {"type": "file", "path": "shot.png", "cwd": f"{HOME}/.ssh"}
        refused(f"{origin}: a cwd in a hidden folder", sid, {"cmd": "display", "origin": origin, "display": block})
        refused(f"{origin}: the same by a pin", sid, {"cmd": "pin", "origin": origin, "display": block})
        refused(f"{origin}: .. out of its folder", sid, {"cmd": "display", "origin": origin, "display": {"type": "file", "path": "../home/.ssh/shot.png"}})
        refused(f"{origin}: a link in its folder", sid, {"cmd": "display", "origin": origin, "display": {"type": "file", "path": "link/shot.png"}})
        refused(f"{origin}: a cwd that is a link", sid, {"cmd": "display", "origin": origin, "display": {"type": "file", "path": "shot.png", "cwd": f"{T}/work/link"}})
        refused(f"{origin}: among other files", sid, {"cmd": "display", "origin": origin, "display": {"type": "file", "paths": ["shown.png", "link/shot.png"]}})
        refused(f"{origin}: the session in a hidden folder", inside, {"cmd": "display", "origin": origin, "display": {"type": "file", "path": "shot.png"}})
    # a display replaced
    made = ask({"cmd": "display", "origin": "mcp", "session": sid, "display": {"type": "file", "path": "shown.png"}})
    check("its folder is not hidden: shown", made.get("ok") is True, json.dumps(made)[:200])
    check("     with its preview", SHOWN in previews(sid), str(previews(sid))[:200])
    updated = ask({"cmd": "display_update", "origin": "mcp", "session": sid, "id": (made.get("data") or {}).get("id", ""), "display": {"type": "file", "path": "shot.png", "cwd": f"{HOME}/.ssh"}})
    check("display_update with a cwd in a hidden folder: refused", updated.get("ok") is False and HIDDEN not in previews(sid), json.dumps(updated)[:200])

    # what is not hidden keeps its way
    other = ask({"cmd": "display", "origin": "mcp", "session": sid, "display": {"type": "file", "path": "other.png", "cwd": f"{T}/work/sub"}})
    check("a cwd given, not hidden: shown", other.get("ok") is True, json.dumps(other)[:200])
    other = ask({"cmd": "display", "origin": "web", "session": sid, "display": {"type": "file", "path": "sub/other.png"}})
    check("a device, a folder in the session: shown", other.get("ok") is True, json.dumps(other)[:200])
    missing = ask({"cmd": "display", "origin": "mcp", "session": sid, "display": {"type": "file", "path": "none.png"}})
    check("no such file: told as before", missing.get("ok") is False and "no such file" in missing.get("error", ""), json.dumps(missing)[:200])

    # the gate of the callers still reads a whole path, the user of the computer is not behind it
    plain = ask({"cmd": "display", "origin": "mcp", "session": sid, "display": {"type": "file", "path": f"{HOME}/.ssh/shot.png"}})
    check("the global session, its whole path: refused", plain.get("ok") is False, json.dumps(plain)[:200])
    mine = ask({"cmd": "display", "session": sid, "display": {"type": "file", "path": f"{HOME}/.ssh/shot.png"}})
    check("the user, its whole path: shown", mine.get("ok") is True and HIDDEN in previews(sid), json.dumps(mine)[:200])
    told = ask({"cmd": "display", "session": inside, "display": {"type": "file", "path": "shot.png"}})
    check("the user, by a relative path in a hidden folder: told to give its whole path", told.get("ok") is False and "whole path" in told.get("error", ""), json.dumps(told)[:200])
    check("the core alive", core.poll() is None and ask({"cmd": "list"}).get("ok") is True)
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
