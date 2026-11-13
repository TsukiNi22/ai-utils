#!/usr/bin/env python3
# A wake-up never takes the output of a !command meant for the next prompt (taken back, the agent would never see it). Esc takes back the prompt of the user, never the wake-up after it (the editor
# gets that prompt, the wake-up goes with it).
# Usage: tests/keep_warm_takeback.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The take-back of a keep-warm wake-up, and of the prompt before it."""
import time

import fake_harness
from fake_harness import Core, check

CONFIG = '[ui]\nnotify = false\nmachine_days = 0\nrestore = "always"\nshell_context = "always"\n[sessions]\nkeep_warm = "chosen"\nkeep_warm_ttl = 1\nkeep_warm_margin = 16\n[commands]\nclaude = "{AGENT}"\n'

# ---- a !command, the wake-ups, then Esc
c = Core("warm-context", CONFIG, "fake_claude_mon.py")
try:
    c.start("--no-global")
    sid = c.spawn("ctx", "FIRST-PROMPT")
    c.wait(lambda: len(c.prompts()) >= 1, 15)
    time.sleep(1)
    c.ask({"cmd": "keep_warm", "session": sid, "value": "on"})
    got = c.ask({"cmd": "shell", "session": sid, "command": "echo MARKER_CTX_OUTPUT"})
    check("a !command run", "MARKER_CTX_OUTPUT" in str(got), str(got)[:200])
    check("woken up", c.wait(lambda: c.wakes() >= 1, 80, 0.5), str(c.wakes()))
    check("the wake-up without its output", not any("MARKER_CTX_OUTPUT" in p for p in c.prompts()), str(c.prompts()))
    check("woken up again", c.wait(lambda: c.wakes() >= 2, 80, 0.5), str(c.wakes()))
    time.sleep(1)
    check("the one before taken back", c.log().count("--resume-session-at") >= 1, c.log()[-300:])
    c.ask({"cmd": "send", "session": sid, "text": "THE-REAL-NEXT-PROMPT"})
    c.wait(lambda: any("THE-REAL-NEXT-PROMPT" in p for p in c.prompts()), 15)
    real = next(p for p in c.prompts() if "THE-REAL-NEXT-PROMPT" in p)
    check("given with the next real prompt", "MARKER_CTX_OUTPUT" in real, real[:300])
    check("... and in the conversation of the agent", any("MARKER_CTX_OUTPUT" in m for m in c.conversation()), str(c.conversation()))

    # the user takes back their prompt just after a wake-up
    c.wait(lambda: c.listed(sid).get("state") == "idle", 15)
    used = c.wakes()
    check("woken up after it", c.wait(lambda: c.wakes() > used, 80, 0.5), str(c.wakes()))
    c.wait(lambda: c.listed(sid).get("state") == "idle", 15, 0.1)
    time.sleep(0.5)
    got = c.ask({"cmd": "rewind", "session": sid, "back": 1})
    check("Esc: the prompt of the user back, never the wake-up", got.get("ok") is True and "THE-REAL-NEXT-PROMPT" in str((got.get("data") or {}).get("text")), str(got)[:300])
    kept = c.session_log(sid)
    # (the wake-up before that prompt stays: it was already the conversation when the prompt came)
    check("that prompt and the wake-up after it out of its transcript", "THE-REAL-NEXT-PROMPT" not in kept and kept.count("keep-warm") <= 1 and "FIRST-PROMPT" in kept, kept[-400:])
    got = c.ask({"cmd": "send", "session": sid, "text": "ANOTHER-PROMPT"})
    check("its next prompt goes on from there", got.get("ok") is True and c.wait(lambda: any("ANOTHER-PROMPT" in p for p in c.prompts()), 15), str(c.prompts()[-3:]))
    time.sleep(1)
    conversation = c.conversation()
    check("... out of the conversation of the agent", not any("THE-REAL-NEXT-PROMPT" in m for m in conversation) and any("FIRST-PROMPT" in m for m in conversation) and any("ANOTHER-PROMPT" in m for m in conversation), str(conversation))
    check("the core alive", c.core.poll() is None)
finally:
    c.clean()
fake_harness.finish()
