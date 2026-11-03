#!/usr/bin/env python3
# The agent gone (a crash, an OOM kill) while a background shell of it runs: its status no longer "running" for ever (nothing can end it any more): the session no longer busy, the background mode of
# keep warm leaves it to cool, a one-shot session can close, an update does not wait for it.
# Usage: tests/background_dead_agent.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The background of a dead agent."""
import os
import signal
import time

import fake_harness
from fake_harness import Core, check

CONFIG = '[ui]\nnotify = false\nmachine_days = 0\n[sessions]\nkeep_warm = "background"\nkeep_warm_ttl = 1\nkeep_warm_margin = 16\n[commands]\nclaude = "{AGENT}"\n'
c = Core("dead-agent", CONFIG, "fake_claude_wake.py")

def running(sid: str) -> list:
    return [a for a in c.ask({"cmd": "agents"}).get("data") or [] if a.get("session") == sid and a.get("status") == "running"]

try:
    c.start("--no-global")
    sid = c.spawn("bg", "build")
    check("its background running", c.wait(lambda: bool(running(sid)), 15, 0.05), str(c.ask({"cmd": "agents"}))[:200])
    pid = c.listed(sid).get("where", {}).get("pid")
    os.kill(pid, signal.SIGKILL)
    check("its agent gone", c.wait(lambda: c.listed(sid).get("state") == "error", 10, 0.1), str(c.listed(sid))[:200])
    check("its background no longer running", not running(sid), str(running(sid)))
    stopped = [a for a in c.ask({"cmd": "agents"}).get("data") or [] if a.get("session") == sid]
    check("... shown stopped, its end known", len(stopped) == 1 and stopped[0].get("status") == "stopped" and stopped[0].get("ended", 0) > 0, str(stopped)[:300])
    c.ask({"cmd": "send", "session": sid, "text": "hello"})
    c.wait(lambda: any("hello" in p for p in c.prompts()), 15)
    c.wait(lambda: c.listed(sid).get("state") == "idle", 15)
    listed = c.listed(sid)
    check("its new agent: idle, not busy on the dead background", listed.get("background_busy") is False and (listed.get("agents") or {}).get("running", 0) == 0, str(listed)[:300])
    check("not kept warm by the background mode (no ♨)", listed.get("keep_warm") is False, str(listed)[:300])
    time.sleep(60)
    check("never woken up for it", c.wakes() == 0, str(c.wakes()))
    check("the core alive", c.core.poll() is None)
finally:
    c.clean()
fake_harness.finish()
