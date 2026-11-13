#!/usr/bin/env python3
# send_next (F8) with the agent gone, a prompt queued: its agent started again and the prompt given, never taken out of the queue and lost, shown as sent. With nothing queued: refused.
# Usage: tests/send_next_down.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""send_next with the agent down."""
import os
import signal

import fake_harness
from fake_harness import Core, check

c = Core("send-next", '[ui]\nnotify = false\nmachine_days = 0\nqueue_given = "end"\n[commands]\nclaude = "{AGENT}"\n', "fake_claude.py", {"FAKE_CLAUDE_DELAY": "60"})
try:
    c.start("--no-global")
    sid = c.spawn("q", "slow")
    c.wait(lambda: c.listed(sid).get("state") == "working", 15, 0.1)
    c.ask({"cmd": "send", "session": sid, "text": "QUEUED-PROMPT"})
    listed = c.listed(sid)
    check("working, a prompt queued", listed.get("queued") == 1, str(listed)[:200])
    os.kill(listed.get("where", {}).get("pid"), signal.SIGKILL)
    c.wait(lambda: c.listed(sid).get("state") == "error", 10, 0.1)
    check("its agent gone, the prompt still queued", c.listed(sid).get("queued") == 1, str(c.listed(sid))[:200])
    got = c.ask({"cmd": "send_next", "session": sid})
    check("send_next answered", got.get("ok") is True, str(got)[:200])
    check("its agent got the prompt", c.wait(lambda: any("QUEUED-PROMPT" in p for p in c.prompts()), 15), str(c.prompts()))
    check("out of the queue", c.listed(sid).get("queued") == 0, str(c.listed(sid))[:200])
    check("in the transcript once", c.session_log(sid).count("QUEUED-PROMPT") == 1, c.session_log(sid)[-300:])
    c.wait(lambda: c.listed(sid).get("state") == "idle", 15)
    got = c.ask({"cmd": "send_next", "session": sid})
    check("nothing queued: refused", got.get("ok") is False, str(got)[:200])
    check("the core alive", c.core.poll() is None)
finally:
    c.clean()
fake_harness.finish()
