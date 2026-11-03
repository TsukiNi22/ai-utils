#!/usr/bin/env python3
# The soundboard of the Debug page: each sound of the notifications played on the computer, whatever [ui] notify_sound says, all of them in a
# row, an unknown name refused, nothing to play it with told; the rows of the page.
# Usage: tests/sound_board.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The soundboard: what it plays, what it refuses."""
from collections.abc import Callable
import json
import os
import socket
import stat
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-sound-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
PLAYED = f"{T}/played.log"
SOUNDS = ["message", "complete", "message-new-instant", "dialog-warning", "service-login"]
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(10)
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

def played() -> list[str]:
    try:
        with open(PLAYED, encoding="utf-8") as text:
            return [line.strip() for line in text if line.strip()]
    except OSError:
        return []

def wait_for(test: Callable[[], bool], seconds: float) -> bool:
    end = time.time() + seconds
    while time.time() < end:
        if test():
            return True
        time.sleep(0.1)
    return test()

def serve(path: str) -> subprocess.Popen:
    env = {key: value for key, value in os.environ.items() if not key.startswith("CLAUDE_CLUSTER_")}
    env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
               XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", PATH=path)
    process = subprocess.Popen([BIN, "serve"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    end = time.time() + 30
    while time.time() < end:
        try:
            if ask({"cmd": "hello"}):
                return process
        except OSError:
            time.sleep(0.2)
    return process

def stop(process: subprocess.Popen) -> None:
    process.terminate()
    try:
        process.wait(15)
    except subprocess.TimeoutExpired:
        process.kill()
    if os.path.exists(f"{T}/sock"):
        os.remove(f"{T}/sock")

for folder in ("cfg", "data", "run", "cache", "home", "bin", "empty"):
    os.makedirs(f"{T}/{folder}", exist_ok=True)
with open(f"{T}/bin/canberra-gtk-play", "w", encoding="utf-8") as script:
    script.write(f'#!/bin/sh\n[ "$1" = "-i" ] && echo "$2" >> {PLAYED}\n')
os.chmod(f"{T}/bin/canberra-gtk-play", stat.S_IRWXU)
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write("[ui]\nnotify_sound = false\n")

core = serve(f"{T}/bin:/usr/bin:/bin")
try:
    answer = ask({"cmd": "play_sound", "sound": "complete"})
    message = answer.get("data", {}).get("message", "")
    check("one sound played, whatever notify_sound says", wait_for(lambda: "complete" in played(), 5), str(played()))
    check("its message says which one", "complete" in message and "heavy job" in message, json.dumps(answer)[:300])
    refused = ask({"cmd": "play_sound", "sound": "../../etc/passwd"})
    check("an unknown name refused, nothing played", "no sound" in json.dumps(refused) and len(played()) == 1, json.dumps(refused)[:300])
    done = ask({"cmd": "play_sound", "sound": "all"})
    check("all of them: told at once", "every sound" in json.dumps(done), json.dumps(done)[:300])
    check("all of them: each one, in the order of the page", wait_for(lambda: played()[1:] == SOUNDS, 15), str(played()))
    settings = json.dumps(ask({"cmd": "settings_get", "section": "debug"}))
    check("the rows of the Debug page", all(f"sound_{sound}" in settings for sound in SOUNDS) and "sound_all" in settings, settings[:200])
finally:
    stop(core)

# nothing to play it with: told, never a silent success
core = serve(f"{T}/empty")
try:
    answer = ask({"cmd": "play_sound", "sound": "message"})
    text = json.dumps(answer)
    system = os.path.exists("/usr/share/sounds/freedesktop/stereo/message.oga")
    check("no player: told", "nothing here can play it" in text or not system, text[:300])
finally:
    stop(core)

os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
