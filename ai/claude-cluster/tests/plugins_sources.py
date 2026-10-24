#!/usr/bin/env python3
# The plugins page: the sources listed with their counts and dates (a folder: its newest SKILL.md, a repository: its last commit), several
# ticks written at once (install, off), a source turned off (its items off, kept installed) and on again, its auto sync turned off, a
# source synced by hand (its new commit seen), an item removed. Isolated: a HOME of its own (the skills linked in its ~/.claude/skills).
# Usage: tests/plugins_sources.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The sources of the plugins page and its bulk changes."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
from typing import Callable

T = tempfile.mkdtemp(prefix="cc-plugins-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/fake.log", HOME=f"{T}/home", GIT_CONFIG_NOSYSTEM="1")
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

def skill(folder: str, name: str) -> None:
    os.makedirs(f"{folder}/{name}")
    with open(f"{folder}/{name}/SKILL.md", "w", encoding="utf-8") as f:
        f.write(f"---\nname: {name}\ndescription: the skill {name}\n---\n\n# {name}\n")

def run(*args: str) -> None:
    subprocess.run(list(args), env=ENV, cwd=f"{T}/repo", check=True, capture_output=True)

def sources() -> dict:
    return {s.get("slug"): s for s in (ask({"cmd": "plugins"}).get("data") or {}).get("sources", [])}

def wait(test: Callable[[dict], bool], seconds: float = 20) -> dict:
    end = time.time() + seconds
    got = sources()
    while time.time() < end and not test(got):
        time.sleep(0.5)
        got = sources()
    return got

for folder in ("cfg", "data", "run", "cache", "home", "folder", "repo"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
for name in ("alpha", "beta", "gamma"):
    skill(f"{T}/folder", name)
skill(f"{T}/repo", "delta")
run("git", "init", "-q")
run("git", "-c", "user.name=t", "-c", "user.email=t@t.t", "add", ".")
run("git", "-c", "user.name=t", "-c", "user.email=t@t.t", "commit", "-q", "-m", "first skill")
GIT = f"file://{T}/repo"
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[plugins]\nrepos = ["{T}/folder", "{GIT}"]\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(50):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    got = wait(lambda s: len(s) == 2 and all(x.get("offered", 0) > 0 for x in s.values()))
    local = next((s for s in got.values() if s.get("kind") == "folder"), {})
    remote = next((s for s in got.values() if s.get("kind") == "git"), {})
    check("both sources listed with what they offer", local.get("offered") == 3 and remote.get("offered") == 1, json.dumps(got)[:400])
    check("a repository: synced, its last commit and date", remote.get("synced", 0) > 0 and "first skill" in remote.get("commit", "") and remote.get("commit_time", 0) > 0,
          json.dumps({k: remote.get(k) for k in ("synced", "commit", "commit_time")}))
    check("a folder: the date of its newest SKILL.md", local.get("commit_time", 0) > 0, str(local.get("commit_time")))
    slug = local.get("slug", "")
    ids = {i["name"]: i["id"] for i in local.get("items", [])}
    ask({"cmd": "plugins_set", "changes": {"items": [{"table": "select", "key": ids.get("alpha", "x"), "value": True},
                                                     {"table": "select", "key": ids.get("beta", "x"), "value": True}]}})
    got = wait(lambda s: s.get(slug, {}).get("installed") == 2)
    check("two ticked at once: installed (linked in ~/.claude/skills)", got.get(slug, {}).get("installed") == 2 and os.path.islink(f"{T}/home/.claude/skills/alpha")
          and os.path.islink(f"{T}/home/.claude/skills/beta"), json.dumps(got.get(slug, {}))[:300])
    ask({"cmd": "plugins_set", "changes": {"items": [{"table": "off", "key": ids.get("alpha", "x"), "value": True}]}})
    got = wait(lambda s: s.get(slug, {}).get("enabled") == 1)
    check("one turned off: kept installed, not loaded", got.get(slug, {}).get("enabled") == 1 and not os.path.exists(f"{T}/home/.claude/skills/alpha"),
          json.dumps({k: got.get(slug, {}).get(k) for k in ("installed", "enabled")}))
    ask({"cmd": "plugins_set", "changes": {"items": [{"table": "paused", "key": slug, "value": True}]}})
    got = wait(lambda s: s.get(slug, {}).get("enabled") == 0)
    check("the source turned off: every item of it off", got.get(slug, {}).get("paused") is True and got.get(slug, {}).get("enabled") == 0
          and not os.path.exists(f"{T}/home/.claude/skills/beta"), json.dumps({k: got.get(slug, {}).get(k) for k in ("paused", "enabled")}))
    ask({"cmd": "plugins_set", "changes": {"items": [{"table": "paused", "key": slug, "value": False}]}})
    got = wait(lambda s: s.get(slug, {}).get("enabled") == 1)
    check("on again: its items as they were (beta on, alpha still off)", got.get(slug, {}).get("enabled") == 1 and os.path.islink(f"{T}/home/.claude/skills/beta"),
          json.dumps({k: got.get(slug, {}).get(k) for k in ("paused", "enabled")}))
    rslug = remote.get("slug", "")
    ask({"cmd": "plugins_set", "changes": {"items": [{"table": "auto", "key": rslug, "value": False}], "sync_hours": 6}})
    data = ask({"cmd": "plugins"}).get("data") or {}
    check("auto sync off for a source, the period changed", sources().get(rslug, {}).get("auto") is False and data.get("sync_hours") == 6,
          json.dumps({"auto": sources().get(rslug, {}).get("auto"), "hours": data.get("sync_hours")}))
    skill(f"{T}/repo", "epsilon")
    run("git", "-c", "user.name=t", "-c", "user.email=t@t.t", "add", ".")
    run("git", "-c", "user.name=t", "-c", "user.email=t@t.t", "commit", "-q", "-m", "second skill")
    ask({"cmd": "plugins_refresh", "source": rslug})
    got = wait(lambda s: "second skill" in s.get(rslug, {}).get("commit", ""))
    check("synced by hand: its new commit and its new skill", "second skill" in got.get(rslug, {}).get("commit", "") and got.get(rslug, {}).get("offered") == 2,
          json.dumps({k: got.get(rslug, {}).get(k) for k in ("commit", "offered")}))
    ask({"cmd": "plugins_set", "changes": {"items": [{"table": "select", "key": ids.get("beta", "x"), "value": False}, {"table": "off", "key": ids.get("beta", "x"), "value": False}]}})
    got = wait(lambda s: s.get(slug, {}).get("installed") == 1)
    check("one removed", got.get(slug, {}).get("installed") == 1 and not os.path.exists(f"{T}/home/.claude/skills/beta"), json.dumps(got.get(slug, {}))[:200])
    refused = ask({"cmd": "plugins_set", "changes": {"items": [{"table": "commands", "key": "x", "value": True}]}})
    check("a change of another table refused", refused.get("ok") is False, json.dumps(refused)[:200])
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
sys.exit(1 if failed else 0)
