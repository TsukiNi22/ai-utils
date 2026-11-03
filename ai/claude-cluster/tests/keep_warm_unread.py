#!/usr/bin/env python3
# A wake-up its agent never read (written while it loads, then a quit, or the agent dead and started again): its transcript does not have it, the leaf is still on the answer before it. The take-back
# finds a real prompt there: never cut (the next wake-up goes after); the conversation of the agent keeps its last real prompt, its conversation is never cleared.
# Usage: tests/keep_warm_unread.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The take-back of a wake-up the agent never read."""
import json
import os
import signal
import time

import fake_harness
from fake_harness import Core, check

CONFIG = '[ui]\nnotify = false\nmachine_days = 0\nrestore = "always"\n[sessions]\nkeep_warm = "chosen"\nkeep_warm_ttl = 1\nkeep_warm_margin = 30\n[commands]\nclaude = "{AGENT}"\n'

# ---- a quit while a wake-up was written to a claude still loading
c = Core("warm-quit", CONFIG, "fake_claude_mon.py", {"FAKE_SLOW_RESUME": "6"})
try:
    c.start("--no-global")
    sid = c.spawn("q", "REAL-PROMPT-1")
    c.wait(lambda: any("REAL-PROMPT-1" in p for p in c.prompts()), 15)
    time.sleep(1)
    c.ask({"cmd": "send", "session": sid, "text": "REAL-PROMPT-2"})
    c.wait(lambda: any("REAL-PROMPT-2" in p for p in c.prompts()), 15)
    time.sleep(1)
    c.ask({"cmd": "keep_warm", "session": sid, "value": "on"})
    check("woken up", c.wait(lambda: c.wakes() >= 1, 80, 0.5), str(c.wakes()))
    # the second one: the first taken back, claude started again (loading 6 s), the prompt written in its stdin, never read
    check("taken back, its agent loading", c.wait(lambda: c.log().count("--resume-session-at") >= 1, 80, 0.5), c.log()[-300:])
    c.wait(lambda: "keep-warm" in c.session_log(sid).split("REAL-PROMPT-2")[-1], 5, 0.1)
    time.sleep(1)
    check("written, not read yet", c.wakes() == 1, str(c.wakes()))
    c.stop()
    c.env["FAKE_SLOW_RESUME"] = ""
    c.start("--no-global", "--restore")
    before = c.wakes()
    check("woken up after the restart", c.wait(lambda: c.wakes() > before, 80, 0.5), str(c.wakes()))
    time.sleep(2)
    conversation = c.conversation()
    check("its last real prompt still in the conversation of the agent", any("REAL-PROMPT-2" in m for m in conversation), str(conversation))
    check("... and in its transcript", "REAL-PROMPT-2" in c.session_log(sid), c.session_log(sid)[-300:])
finally:
    c.clean()

# ---- a wake-up its agent never read, the agent dead, started again by itself (the global session): the next take-back finds a real prompt, never cuts it
GLOBAL = CONFIG.replace('keep_warm = "chosen"', 'keep_warm = "all"')
c = Core("warm-died", GLOBAL, "fake_claude_mon.py", {"FAKE_SLOW_RESUME": "20"}, enabled=True)
try:
    c.start()
    c.wait(lambda: c.listed("g").get("state") == "idle", 15)
    c.ask({"cmd": "send", "session": "g", "text": "REAL-PROMPT-1"})
    c.wait(lambda: any("REAL-PROMPT-1" in p for p in c.prompts()), 15)
    time.sleep(1)
    c.ask({"cmd": "send", "session": "g", "text": "REAL-PROMPT-2"})
    c.wait(lambda: any("REAL-PROMPT-2" in p for p in c.prompts()), 15)
    check("woken up", c.wait(lambda: c.wakes() >= 1, 80, 0.5), str(c.wakes()))
    check("taken back, its agent loading", c.wait(lambda: c.log().count("--resume-session-at") >= 1, 80, 0.5), c.log()[-300:])
    c.wait(lambda: "keep-warm" in c.session_log("g").split("REAL-PROMPT-2")[-1], 8, 0.1)
    pid = c.listed("g").get("where", {}).get("pid")
    check("written, not read", c.wakes() == 1 and bool(pid), f"{c.wakes()} {pid}")
    os.kill(pid, signal.SIGKILL)
    check("its agent started again", c.wait(lambda: c.listed("g").get("where", {}).get("pid") not in (None, pid) and c.listed("g").get("state") == "idle", 15), str(c.listed("g"))[:200])
    before = c.wakes()
    check("woken up after it", c.wait(lambda: c.wakes() > before, 80, 0.5), str(c.wakes()))
    time.sleep(2)
    conversation = c.conversation()
    check("its last real prompt still in the conversation of the agent", any("REAL-PROMPT-2" in m for m in conversation), str(conversation))
    check("... and in its transcript", "REAL-PROMPT-2" in c.session_log("g"), c.session_log("g")[-300:])
    check("the core alive", c.core.poll() is None)
    c.stop()
    with open(f"{c.T}/data/state.json", encoding="utf-8") as state:
        spec = next(s for s in json.load(state)["sessions"] if s.get("global"))
    check("its conversation kept", spec.get("claude_id") == "00000000-0000-0000-0000-000000000001", json.dumps(spec)[:200])
finally:
    c.clean()
fake_harness.finish()
