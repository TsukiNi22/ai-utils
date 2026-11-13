#!/usr/bin/env python3
# A prompt sent while a session is being replaced (its model, its effort, its folder, /clear: its old object stopping, 3 s for a slow agent) is never answered ok and lost: it goes to the new one, in
# its order. Two changes at once: both applied, none lost. The change failing: the prompt goes to the session as it was.
# Usage: tests/relaunch_prompt.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A prompt during the replacement of a session."""
import threading
import time

import fake_harness
from fake_harness import Core, check

c = Core("relaunch", '[ui]\nnotify = false\nmachine_days = 0\n[commands]\nclaude = "{AGENT}"\n', "fake_claude.py", {"FAKE_CLAUDE_STUBBORN": "1"})
try:
    c.start("--no-global")
    sid = c.spawn("m", "hello")
    c.wait(lambda: any("hello" in p for p in c.prompts()) and c.listed(sid).get("state") == "idle", 15)

    def during(change: dict, text: str, what: str) -> None:
        result = {}

        def run() -> None:
            result["reply"] = c.ask(change, 40)

        thread = threading.Thread(target=run)
        thread.start()
        time.sleep(1)  # its old agent stopping (3 s for a slow one)
        got = c.ask({"cmd": "send", "session": sid, "text": text})
        check(f"{what}: the prompt during it answered", got.get("ok") is True, str(got)[:200])
        thread.join(45)
        check(f"{what}: done", result.get("reply", {}).get("ok") is True, str(result)[:200])
        check(f"{what}: the new agent got it", c.wait(lambda: any(text in p for p in c.prompts()), 15), str(c.prompts()[-4:]))
        check(f"{what}: in the session", c.wait(lambda: text in c.session_log(sid), 5), c.session_log(sid)[-300:])
        check(f"{what}: nothing left waiting", c.wait(lambda: c.listed(sid).get("state") == "idle" and c.listed(sid).get("queued") == 0, 15), str(c.listed(sid))[:200])

    during({"cmd": "model", "session": sid, "model": "haiku"}, "PROMPT-DURING-THE-MODEL", "a model")
    during({"cmd": "model", "session": sid, "effort": "low"}, "PROMPT-DURING-THE-EFFORT", "an effort")
    during({"cmd": "slash", "session": sid, "text": "/clear"}, "PROMPT-DURING-THE-CLEAR", "/clear")
    during({"cmd": "cd", "session": sid, "cwd": f"{c.T}/home"}, "PROMPT-DURING-THE-FOLDER", "a folder")

    # two changes at once, a prompt behind them
    replies = []
    threads = [threading.Thread(target=lambda r=request: replies.append(c.ask(r, 40))) for request in ({"cmd": "model", "session": sid, "model": "sonnet"}, {"cmd": "model", "session": sid, "effort": "high"})]
    for thread in threads:
        thread.start()
    time.sleep(1)
    c.ask({"cmd": "send", "session": sid, "text": "PROMPT-DURING-BOTH"})
    for thread in threads:
        thread.join(45)
    check("two changes at once: both done", len(replies) == 2 and all(r.get("ok") for r in replies), str(replies)[:300])
    listed = c.listed(sid)
    check("... both applied", "sonnet" in listed.get("backend", "") and listed.get("effort") == "high", str(listed)[:300])
    check("... the prompt given", c.wait(lambda: any("PROMPT-DURING-BOTH" in p for p in c.prompts()), 15), str(c.prompts()[-4:]))

    # the change failing: its agent not installed any more
    with open(f"{c.T}/cfg/config.toml", encoding="utf-8") as config:
        text = config.read()
    good = next(line for line in text.splitlines() if line.startswith("claude = "))
    with open(f"{c.T}/cfg/config.toml", "w", encoding="utf-8") as config:
        config.write(text.replace(good, 'claude = "/nonexistent/claude"'))
    c.wait(lambda: any("config reloaded" in n for n in c.told("")), 5)
    result = {}
    thread = threading.Thread(target=lambda: result.update(c.ask({"cmd": "model", "session": sid, "model": "opus"}, 40)))
    thread.start()
    time.sleep(1)
    with_prompt = c.ask({"cmd": "send", "session": sid, "text": "PROMPT-DURING-THE-FAILURE"})
    thread.join(45)
    check("the change refused, told", result.get("ok") is False, str(result)[:200])
    check("the prompt during it answered", with_prompt.get("ok") is True, str(with_prompt)[:200])
    check("... and given to the session as it was", c.wait(lambda: any("PROMPT-DURING-THE-FAILURE" in p for p in c.prompts()), 15), str(c.prompts()[-4:]))
    check("the session still there, on its model", "sonnet" in c.listed(sid).get("backend", ""), str(c.listed(sid))[:200])
    check("the core alive", c.core.poll() is None)
finally:
    c.clean()
fake_harness.finish()
