#!/usr/bin/env python3
# A model / mode / effort for the next new sub-sessions: written, answered as done (what settings_set answers says "at the next start", never a failure), read at once by the new sessions.
# Usage: tests/apply_new.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""`apply` scope new."""
import json
import re

import fake_harness
from fake_harness import Core, check

c = Core("apply-new", '[ui]\nnotify = false\nmachine_days = 0\n[commands]\nclaude = "{AGENT}"\n', "fake_claude.py")
try:
    c.start("--no-global")
    sid = c.spawn("first")

    def config() -> str:
        with open(f"{c.T}/cfg/config.toml", encoding="utf-8") as out:
            return out.read()

    got = c.ask({"cmd": "apply", "kind": "mode", "value": "plan", "scope": "new", "session": sid})
    check("a mode: answered done", got.get("ok") is True and "next new" in json.dumps(got), json.dumps(got)[:200])
    check("... written", re.search(r'^permission_mode\s*=\s*"plan"', config(), re.M) is not None, config()[-200:])
    got = c.ask({"cmd": "apply", "kind": "effort", "value": "low", "scope": "new", "session": sid})
    check("an effort: answered done", got.get("ok") is True and "next new" in json.dumps(got), json.dumps(got)[:200])
    got = c.ask({"cmd": "apply", "kind": "model", "value": "sonnet", "scope": "new", "session": sid, "confirmed": True})
    check("a model (its setting restarts nothing but answers so): answered done", got.get("ok") is True and "next new" in json.dumps(got), json.dumps(got)[:300])
    check("... written", re.search(r'^provider\s*=\s*"[^"]*/sonnet"', config(), re.M) is not None, config()[-200:])
    second = c.spawn("second")
    listed = c.listed(second)
    check("the next session has them", listed.get("mode") == "plan" and "sonnet" in listed.get("backend", "") and listed.get("effort") == "low", json.dumps(listed)[:300])
    first = c.listed(sid)
    check("the session already there keeps its own", first.get("mode") != "plan" and "sonnet" not in first.get("backend", ""), json.dumps(first)[:300])
finally:
    c.clean()
fake_harness.finish()
