#!/usr/bin/env python3
# gdbus monitor (the clicks of the notifications) ends while the core lives (the session bus restarted): the next notification starts another one, never a thread assigned over the one that ended
# (std::terminate, the core gone). A core that ends never leaves its gdbus monitor behind (it only writes when a signal comes: it would wait for ever).
# Usage: tests/notify_monitor.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The monitor of the notifications ended."""
import os
import time

import fake_harness
from fake_harness import Core, check

c = Core("notify-monitor", '[ui]\nnotify = true\nnotify_sound = false\nnotify_when = "always"\nmachine_days = 0\n[commands]\nclaude = "{AGENT}"\n', "fake_claude.py")
try:
    os.makedirs(f"{c.T}/bin")
    with open(f"{c.T}/bin/gdbus", "w", encoding="utf-8") as out:
        out.write('#!/bin/sh\necho "$1" >> "$FAKE_T/gdbus.log"\ncase "$1" in\n  monitor) exit 0;;\n  call) echo "(uint32 7,)";;\nesac\n')
    os.chmod(f"{c.T}/bin/gdbus", 0o755)
    c.env["PATH"] = f"{c.T}/bin:" + c.env["PATH"]
    c.start("--no-global")
    sid = c.spawn("notified", "first")
    for text in ("second", "third", "fourth"):
        c.wait(lambda: c.listed(sid).get("state") == "idle", 10)
        time.sleep(1.5)
        c.ask({"cmd": "send", "session": sid, "text": text})
    c.wait(lambda: any("fourth" in p for p in c.prompts()), 10)
    time.sleep(2)
    with open(f"{c.T}/gdbus.log", encoding="utf-8") as log:
        calls = log.read().split()
    check("its notifications shown", calls.count("call") >= 3, str(calls))
    check("gdbus monitor started again after it ended", calls.count("monitor") >= 2, str(calls))
    check("the core alive", c.core.poll() is None, f"exit {c.core.poll()}")
finally:
    c.clean()

def alive(pid: int) -> bool:
    try:
        with open(f"/proc/{pid}/stat", encoding="utf-8") as stat:
            return stat.read().rsplit(")", 1)[1].split()[0] != "Z"
    except OSError:
        return False

for name, stop in (("asked to quit", lambda core: core.terminate()), ("killed", lambda core: core.kill())):
    c = Core("notify-left", '[ui]\nnotify = true\nnotify_sound = false\nnotify_when = "always"\nmachine_days = 0\n[commands]\nclaude = "{AGENT}"\n', "fake_claude.py")
    try:
        os.makedirs(f"{c.T}/bin")
        with open(f"{c.T}/bin/gdbus", "w", encoding="utf-8") as out:
            out.write('#!/bin/sh\ncase "$1" in\n  monitor) echo $$ > "$FAKE_T/monitor.pid"; exec sleep 300;;\n  call) echo "(uint32 7,)";;\nesac\n')
        os.chmod(f"{c.T}/bin/gdbus", 0o755)
        c.env["PATH"] = f"{c.T}/bin:" + c.env["PATH"]
        c.start("--no-global")
        sid = c.spawn("notified", "first")
        c.wait(lambda: os.path.exists(f"{c.T}/monitor.pid"), 20)
        with open(f"{c.T}/monitor.pid", encoding="utf-8") as text:
            pid = int(text.read().strip())
        check(f"{name}: its monitor runs", alive(pid), str(pid))
        stop(c.core)
        c.core.wait(30)
        c.wait(lambda: not alive(pid), 10)
        check(f"{name}: its monitor gone with it", not alive(pid), str(pid))
    finally:
        c.clean()
fake_harness.finish()
