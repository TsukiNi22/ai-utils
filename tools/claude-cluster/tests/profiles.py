#!/usr/bin/env python3
# Settings synced and settings profiles: two cores (the main one, an independent instance) share config.toml - a setting changed through
# one is read by the other; a profile saved (any name), applied (both follow it), undone, renamed, noted, archived / unarchived, deleted to
# the trash (purge in 7 days), restored, purged when its 7 days are over; the cards of Settings > Profiles (an ask: button for the name).
# Usage: tests/profiles.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""Settings synced between the claude-clusters of a computer, and the settings profiles."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-prof-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
OTHER = dict(ENV, CLAUDE_CLUSTER_DATA_DIR=f"{T}/data/instances/b", CLAUDE_CLUSTER_SOCKET=f"{T}/data/instances/b/sock", CLAUDE_CLUSTER_INSTANCE="b")
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(sock: str, request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(15)
    s.connect(sock)
    s.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        data += chunk
    s.close()
    return json.loads(data)

A = f"{T}/sock"
B = f"{T}/data/instances/b/sock"

def ttl(sock: str) -> str:
    for s in ask(sock, {"cmd": "settings_get", "section": "sessions"}).get("data") or []:
        if s.get("key") == "keep_warm_ttl":
            return str(s.get("value"))
    return "?"

def until(sock: str, value: str, seconds: float = 8) -> bool:
    end = time.time() + seconds
    while time.time() < end:
        if ttl(sock) == value:
            return True
        time.sleep(0.3)
    return False

def profiles() -> dict:
    return {p["name"]: p for p in (ask(A, {"cmd": "profiles"}).get("data") or {}).get("profiles") or []}

def profile(action: str, name: str = "", arg: str = "") -> dict:
    return ask(A, {"cmd": "profile", "action": action, "name": name, "arg": arg})

for folder in ("cfg", "data/instances/b", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[sessions]\nkeep_warm_ttl = 5\n[commands]\nclaude = "{FAKE}"\n')
cores = [subprocess.Popen([BIN, "serve", "--no-global"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) for env in (ENV, OTHER)]
try:
    for sock in (A, B):
        for _ in range(100):
            if os.path.exists(sock):
                break
            time.sleep(0.1)
    time.sleep(0.8)
    # sync: a setting changed through one claude-cluster, read by the other
    ask(A, {"cmd": "settings_set", "section": "sessions", "key": "keep_warm_ttl", "value": "7"})
    check("a setting changed through the main one: the instance reads it", until(B, "7"), ttl(B))
    ask(B, {"cmd": "settings_set", "section": "sessions", "key": "keep_warm_ttl", "value": "8"})
    check("... and the other way", until(A, "8"), ttl(A))
    # profiles
    got = profile("save", "Work setup", "the long sessions")
    check("save: a profile of the settings of now", "saved" in json.dumps(got) and "Work setup" in profiles(), json.dumps(got)[:300])
    check("... its note, its settings counted", profiles().get("Work setup", {}).get("note") == "the long sessions" and profiles()["Work setup"].get("settings", 0) > 5,
          json.dumps(profiles())[:300])
    modes = [os.stat(f"{T}/cfg/profiles/{f}").st_mode & 0o777 for f in os.listdir(f"{T}/cfg/profiles") if f.endswith(".toml")]
    check("its file private (0600: keys of the accounts may be in it)", modes and all(m == 0o600 for m in modes), str([oct(m) for m in modes]))
    ask(A, {"cmd": "settings_set", "section": "sessions", "key": "keep_warm_ttl", "value": "11"})
    until(B, "11")
    got = profile("apply", "work SETUP")  # the name matched without its case
    check("apply: told", "applied" in json.dumps(got), json.dumps(got)[:300])
    check("... both claude-clusters follow it", until(A, "8") and until(B, "8"), f"{ttl(A)} {ttl(B)}")
    got = profile("undo")
    check("undo: the settings before it back on both", "back" in json.dumps(got) and until(A, "11") and until(B, "11"), f"{json.dumps(got)[:200]} {ttl(A)} {ttl(B)}")
    got = profile("rename", "Work setup", "Travail 🌙 du soir")
    check("rename: any name", "Travail 🌙 du soir" in profiles() and "Work setup" not in profiles(), json.dumps(got)[:300])
    check("a name taken twice: refused", profile("save", "x").get("ok") is not False and profile("rename", "x", "travail 🌙 du soir").get("ok") is False)
    profile("note", "Travail 🌙 du soir", "evening")
    check("note: written", profiles()["Travail 🌙 du soir"].get("note") == "evening")
    profile("archive", "Travail 🌙 du soir")
    check("archive: kept apart", profiles()["Travail 🌙 du soir"].get("archived", 0) > 0)
    profile("unarchive", "Travail 🌙 du soir")
    check("unarchive: back in the list", profiles()["Travail 🌙 du soir"].get("archived", 1) == 0)
    got = profile("delete", "x")
    p = profiles().get("x", {})
    check("delete: in the trash, gone for good in 7 days", p.get("deleted", 0) > 0 and abs(p.get("purge_at", 0) - p.get("deleted", 0) - 7 * 86400) < 5, json.dumps(p))
    check("... a profile of the trash cannot be applied", profile("apply", "x").get("ok") is False)
    profile("restore", "x")
    check("restore: back", profiles().get("x", {}).get("deleted", 1) == 0)
    # the 7 days over: removed (its file too)
    profile("delete", "x")
    with open(f"{T}/cfg/profiles/index.json", encoding="utf-8") as f:
        index = json.load(f)
    for entry in index["profiles"]:
        if entry["name"] == "x":
            entry["deleted"] -= 8 * 86400
            gone = entry["file"]
    with open(f"{T}/cfg/profiles/index.json", "w", encoding="utf-8") as f:
        json.dump(index, f)
    check("its 7 days over: deleted for good, its file too", "x" not in profiles() and not os.path.exists(f"{T}/cfg/profiles/{gone}.toml"), json.dumps(list(profiles())))
    # the cards (Settings > Profiles, the same as Services)
    cards = (ask(A, {"cmd": "services"}).get("data") or {}).get("cards") or {}
    add = cards.get("profiles", {})
    check("the card of the profiles: a button that asks the name", any(b.get("action", "").startswith("ask:save:") for b in add.get("buttons", [])), json.dumps(add)[:300])
    one = cards.get("profile:Travail 🌙 du soir", {})
    check("a card per profile: Apply (asked first), Rename (asks), Delete", any(b.get("action") == "apply" and b.get("risky") for b in one.get("buttons", []))
          and any(b.get("action", "").startswith("ask:rename") for b in one.get("buttons", [])) and any(b.get("action") == "delete" for b in one.get("buttons", [])),
          json.dumps(one)[:400])
    got = ask(A, {"cmd": "service_act", "service": "profiles", "action": "save:From the card"})
    check("the asked name sent by the card: saved", "From the card" in profiles(), json.dumps(got)[:300])
    pages = {s.get("page") for s in ask(A, {"cmd": "settings_get"}).get("data") or []}
    check("Settings: a Profiles page", "Profiles" in pages, str(pages))
    check("the global session may not change them", ask(A, {"cmd": "profile", "action": "save", "name": "g", "origin": "mcp"}).get("ok") is False)
finally:
    for core in cores:
        core.terminate()
    for core in cores:
        try:
            core.wait(10)
        except subprocess.TimeoutExpired:
            core.kill()
sys.exit(1 if failed else 0)
