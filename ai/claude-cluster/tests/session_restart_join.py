#!/usr/bin/env python3
# The agent of the global session ends by itself and a prompt comes within the second before its automatic restart: that restart sees it is up again (it never joins a reader with a live agent, under
# the lock every stop needs), the core still ends on its signal.
# Usage: tests/session_restart_join.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""Session::restart with its agent started again meanwhile."""
import os
import signal
import time

import fake_harness
from fake_harness import Core, check

c = Core("restart-join", '[ui]\nnotify = false\nmachine_days = 0\n[commands]\nclaude = "{AGENT}"\n', "fake_claude.py", enabled=True)
try:
    c.start()
    c.wait(lambda: c.listed("g").get("state") == "idle", 15)
    pid = c.listed("g").get("where", {}).get("pid")
    check("its agent runs", bool(pid), str(pid))
    os.kill(pid, signal.SIGKILL)
    c.wait(lambda: c.listed("g").get("state") == "error", 3, 0.02)
    got = c.ask({"cmd": "send", "session": "g", "text": "hello again"})
    check("a prompt at once", got.get("ok") is True, str(got)[:200])
    check("its agent got it", c.wait(lambda: any("hello again" in p for p in c.prompts()), 10), str(c.prompts()))
    time.sleep(2.5)  # past the restart of the second after its end
    now = c.listed("g").get("where", {}).get("pid")
    check("up, the same agent", c.listed("g").get("state") == "idle" and bool(now) and now != pid, str(c.listed("g"))[:200])
    begun = time.time()
    code = c.stop(25)
    check("it ends on its signal", code is not None and time.time() - begun < 25, f"exit {code} in {time.time() - begun:.1f} s")
finally:
    c.clean()
fake_harness.finish()
