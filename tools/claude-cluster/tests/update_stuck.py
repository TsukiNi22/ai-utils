#!/usr/bin/env python3
# An update never stays stuck: asked while the global session is not there yet (the question of the previous run waits: its placeholder in the lists) it goes on to the switch; something failing on
# its way (a file not written) cancels it: this version goes on, its prompts held sent, nothing frozen, the update can be asked again.
# Usage: tests/update_stuck.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""An exception in the thread of the update."""
import json
import os
import shutil
import stat
import time

import fake_harness
from fake_harness import BIN, Core, check

CONFIG = '[ui]\nnotify = false\nmachine_days = 0\n[commands]\nclaude = "{AGENT}"\n'

def newer(c: Core, copy: str) -> None:
    # a newer version installed: a script answering 99.0.0, the same binary behind it
    real = f"{c.T}/bin/real"
    shutil.copy(BIN, real)
    os.chmod(real, os.stat(real).st_mode | stat.S_IEXEC)
    with open(f"{copy}.new", "w", encoding="utf-8") as script:
        script.write(f'#!/bin/sh\nif [ "$1" = "--version" ]; then echo "claude-cluster 99.0.0"; exit 0; fi\nif [ "$1" = "--notes" ]; then exit 0; fi\nexec {real} "$@"\n')
    os.chmod(f"{copy}.new", 0o755)
    os.rename(f"{copy}.new", copy)

# ---- a question waits, no global session yet: it reaches the switch
c = Core("update", CONFIG, "fake_claude.py", enabled=True)
try:
    os.makedirs(f"{c.T}/bin")
    copy = f"{c.T}/bin/claude-cluster"
    shutil.copy(BIN, copy)
    os.chmod(copy, os.stat(copy).st_mode | stat.S_IEXEC)
    c.start(binary=copy)
    c.wait(lambda: c.listed("g").get("state") == "idle", 15)
    sid = c.spawn("worker")
    time.sleep(1)
    c.stop()
    c.start("--linger", binary=copy)
    c.interface()
    pid = c.ask({"cmd": "hello"}).get("data", {}).get("pid")
    newer(c, copy)
    c.wait(lambda: c.ask({"cmd": "hello"}).get("data", {}).get("offered") == "99.0.0", 20)
    got = c.ask({"cmd": "upgrade", "action": "update"})
    check("offered and asked, the question of the previous run waiting", got.get("ok") is True and "99.0.0" in json.dumps(got), json.dumps(got)[:200])

    def switched() -> bool:
        try:
            hello = c.ask({"cmd": "hello"}, 5).get("data", {})
        except (OSError, ValueError):
            return False
        return hello.get("pid") == pid and hello.get("upgrading") is False and hello.get("build") != ""

    # the same pid, the new binary run in place: no longer upgrading (the old one stays upgrading until its exec)
    check("it reaches the switch", c.wait(lambda: not os.path.exists(f"{c.T}/data/upgrade-keep.json") and switched(), 60, 0.5), json.dumps(c.ask({"cmd": "hello"}, 5))[:200])
    check("the core is there", c.core.poll() is None and c.answers(), f"exit {c.core.poll()}")
    got = c.ask({"cmd": "restore_answer", "restore": True}, 30)
    check("the question answered after it", got.get("ok") is True and c.wait(lambda: c.listed(sid).get("state") == "idle", 15), json.dumps(got)[:200])
    c.ask({"cmd": "send", "session": sid, "text": "PROMPT-AFTER"})
    check("a prompt reaches its agent", c.wait(lambda: any("PROMPT-AFTER" in p for p in c.prompts()), 15), str(c.prompts()[-3:]))
finally:
    c.clean()

# ---- something failing on its way: cancelled, nothing frozen
c = Core("update-fail", CONFIG, "fake_claude.py")
try:
    os.makedirs(f"{c.T}/bin")
    copy = f"{c.T}/bin/claude-cluster"
    shutil.copy(BIN, copy)
    os.chmod(copy, os.stat(copy).st_mode | stat.S_IEXEC)
    c.start("--no-global", binary=copy)
    sid = c.spawn("worker")
    time.sleep(1)
    os.makedirs(f"{c.T}/data/upgrade-keep.json")  # never written: what is on for this run, just before the switch
    newer(c, copy)
    c.wait(lambda: c.ask({"cmd": "hello"}).get("data", {}).get("offered") == "99.0.0", 20)
    got = c.ask({"cmd": "upgrade", "action": "update"})
    check("the update asked", got.get("ok") is True, json.dumps(got)[:200])
    check("cancelled, told", c.wait(lambda: any("update cancelled" in n for n in c.told("")), 40, 0.5), str(c.told("")))
    hello = c.ask({"cmd": "hello"}).get("data", {})
    check("no longer updating", hello.get("upgrading") is False, json.dumps(hello)[:200])
    c.ask({"cmd": "send", "session": sid, "text": "PROMPT-AFTER"})
    check("a prompt reaches its agent, nothing held", c.wait(lambda: any("PROMPT-AFTER" in p for p in c.prompts()), 15) and not os.path.exists(f"{c.T}/data/held.json"), str(c.prompts()[-3:]))
    c.ask({"cmd": "task_add", "prompt": "TASK-AFTER", "target": sid})
    check("a task runs", c.wait(lambda: any("TASK-AFTER" in p for p in c.prompts()), 15), str(c.prompts()[-3:]))
    os.rmdir(f"{c.T}/data/upgrade-keep.json")
    again = c.ask({"cmd": "upgrade", "action": "update"})
    check("the update can be asked again", again.get("ok") is True and "already" not in json.dumps(again), json.dumps(again)[:200])
finally:
    c.clean()
fake_harness.finish()
