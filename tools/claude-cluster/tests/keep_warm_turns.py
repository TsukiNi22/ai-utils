#!/usr/bin/env python3
# A real turn after a wake-up: started by the agent itself (a Monitor, a background task), or a prompt typed on claude.ai. It is told (warming only for the wake-up itself) and the next wake-up never
# takes it back: not cut from the transcript, not from the conversation of the agent (the wake-up goes after it).
# Usage: tests/keep_warm_turns.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""What follows a wake-up without a prompt of ours."""
import os
import time

import fake_harness
from fake_harness import Core, check

CONFIG = '[ui]\nnotify = false\nmachine_days = 0\n[sessions]\nkeep_warm = "chosen"\nkeep_warm_ttl = 1\nkeep_warm_margin = 16\n[commands]\nclaude = "{AGENT}"\n'
c = Core("warm-turns", CONFIG, "fake_claude_mon.py", {"FAKE_MONITOR_AFTER_WARM": "1", "FAKE_REPLAY_AFTER_WARM": "1", "FAKE_MONITOR_DELAY": "4"})
try:
    c.start("--no-global")
    sid = c.spawn("mon", "monitor")
    c.wait(lambda: "CONTROL-MONITOR-RESULT" in c.session_log(sid), 20)
    time.sleep(1)
    check("control, a turn of its own with no wake-up before it: told", any("CONTROL" in t for t in c.told(sid)), str(c.told(sid)))
    c.ask({"cmd": "keep_warm", "session": sid, "value": "on"})
    told = len(c.told(sid))
    check("woken up", c.wait(lambda: c.wakes() >= 1, 80, 0.2), str(c.wakes()))
    c.wait(lambda: c.listed(sid).get("warming") is True and c.listed(sid).get("state") == "idle", 3, 0.1)
    check("the wake-up itself silent", c.listed(sid).get("warming") is True and len(c.told(sid)) == told, str(c.told(sid)))

    # a turn the agent starts by itself, 4 s after the wake-up
    check("its own turn after the wake-up", c.wait(lambda: "MONITOR-RESULT-AFTER-WAKE" in c.session_log(sid), 15), c.session_log(sid)[-200:])
    time.sleep(1)
    listed = c.listed(sid)
    check("no longer warming", listed.get("warming") is False and listed.get("state") == "idle", str(listed)[:200])
    check("told", any("MONITOR-RESULT-AFTER-WAKE" in t for t in c.told(sid)), str(c.told(sid)))

    # the next wake-up: goes after it
    check("woken up again", c.wait(lambda: c.wakes() >= 2, 80, 0.5), str(c.wakes()))
    time.sleep(2)
    check("that turn still in its transcript", "MONITOR-RESULT-AFTER-WAKE" in c.session_log(sid), c.session_log(sid)[-300:])
    read = c.ask({"cmd": "read", "session": sid}).get("data", {})
    check("... in what the interfaces read", "MONITOR-RESULT-AFTER-WAKE" in str(read.get("messages", read)), str(read)[:300])
    check("... in the conversation of the agent", any("MONITOR-RESULT-AFTER-WAKE" in m for m in c.conversation()), str(c.conversation()))
    check("its agent not cut for it", "--resume-session-at" not in c.log(), c.log()[-400:])
    check("no backup needed: nothing was the user's", not os.path.isdir(f"{c.T}/data/rewind") or not os.listdir(f"{c.T}/data/rewind"))

    # a prompt typed on claude.ai after the wake-up: real, told, never taken for it
    check("a prompt from claude.ai after the wake-up", c.wait(lambda: "ANSWER-TO-CLAUDE-AI" in c.session_log(sid), 15), c.session_log(sid)[-200:])
    time.sleep(1)
    listed = c.listed(sid)
    check("its turn: not warming", listed.get("warming") is False, str(listed)[:200])
    check("... told", any("ANSWER-TO-CLAUDE-AI" in t for t in c.told(sid)) and any("claude.ai" in t for t in c.told(sid)), str(c.told(sid)))
    before = c.log().count("--resume-session-at")
    check("woken up again after it", c.wait(lambda: c.wakes() >= 3, 80, 0.5), str(c.wakes()))
    time.sleep(2)
    check("that prompt and its answer still there", "PROMPT-FROM-CLAUDE-AI" in c.session_log(sid) and "ANSWER-TO-CLAUDE-AI" in c.session_log(sid), c.session_log(sid)[-300:])
    check("... for the agent too", any("PROMPT-FROM-CLAUDE-AI" in m for m in c.conversation()) and c.log().count("--resume-session-at") == before, str(c.conversation()))
    told = len(c.told(sid))

    # with nothing real after it, a wake-up is still taken back by the next one
    check("the next one takes the one before back", c.wait(lambda: c.wakes() >= 4, 80, 0.5) and c.wait(lambda: c.log().count("--resume-session-at") > before, 5), c.log()[-300:])
    time.sleep(2)
    since = c.session_log(sid).split("ANSWER-TO-CLAUDE-AI")[-1]
    check("... one wake-up after the real turn", since.count("keep-warm") == 1, since[:400])
    check("... silent", len(c.told(sid)) == told, str(c.told(sid)))
    check("the core alive", c.core.poll() is None)
finally:
    c.clean()
fake_harness.finish()
