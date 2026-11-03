#!/usr/bin/env python3
# Keep warm on a session of a per-turn agent (codex, opencode, qwen: one process a turn): its wake-up is silent too (no notice, so no notification, sound or face; warming listed), the real turn after
# it told as usual.
# Usage: tests/keep_warm_per_turn.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A wake-up of a per-turn agent."""
import time

import fake_harness
from fake_harness import Core, check

CONFIG = '[ui]\nnotify = false\nmachine_days = 0\n[sessions]\nkeep_warm = "all"\nkeep_warm_ttl = 1\nkeep_warm_margin = 16\n[commands]\ncodex = "{AGENT}"\nclaude = "{AGENT}"\n'
c = Core("warm-per-turn", CONFIG, "fake_codex.py")
try:
    c.start("--no-global")
    sid = c.spawn("cx", "hello", backend="codex")
    check("a session of codex", bool(sid) and c.wait(lambda: len(c.prompts()) >= 1, 15), str(c.prompts()))
    time.sleep(1.5)
    told = c.told(sid)
    check("the real turn told", any("done" in t for t in told), str(told))
    check("woken up", c.wait(lambda: c.wakes() >= 1, 80, 0.5), str(c.wakes()))
    time.sleep(2)
    listed = c.listed(sid)
    check("its turn over, warming", listed.get("state") == "idle" and listed.get("warming") is True, str(listed)[:200])
    check("told nothing", c.told(sid) == told, str(c.told(sid)))
    check("woken up again", c.wait(lambda: c.wakes() >= 2, 80, 0.5), str(c.wakes()))
    time.sleep(2)
    check("told nothing again", c.told(sid) == told, str(c.told(sid)))
    c.ask({"cmd": "send", "session": sid, "text": "a real prompt"})
    c.wait(lambda: any("a real prompt" in p for p in c.prompts()), 15)
    time.sleep(1.5)
    listed = c.listed(sid)
    check("a real prompt: told, no longer warming", len(c.told(sid)) > len(told) and listed.get("warming") is False, str(c.told(sid)) + str(listed)[:200])
    check("the core alive", c.core.poll() is None)
finally:
    c.clean()
fake_harness.finish()
