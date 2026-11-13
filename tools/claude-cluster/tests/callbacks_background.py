#!/usr/bin/env python3
# The callbacks of what runs in the background: a shell or an agent that ends or is killed wakes its session whatever the shape of its end (only a task_updated, a task never seen
# starting), or its end in another turn makes the end of that turn its result; it is a line of the conversation; an agent gone says what stopped with it; the callback owed goes
# with its turn (never to a turn of the user ending while the prompt of the global session waits); a sub-session tells something by report_status.
# Usage: tests/callbacks_background.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The callbacks of the background, between the global session and its sessions."""
import time

import fake_harness
from fake_harness import Core, check

c = Core("callbacks-bg", '[ui]\nnotify = false\nmachine_days = 0\n[commands]\nclaude = "{AGENT}"\n', "fake_claude_bg.py", enabled=True)
try:
    c.start()

    def told_to_global(text: str) -> list:
        return [p for p in c.prompts() if text in p]

    def asked(sid: str, text: str) -> dict:
        return c.ask({"cmd": "send", "session": sid, "text": text, "reply": True, "from": "global", "origin": "mcp"})

    def idle(sid: str) -> bool:
        return c.listed(sid).get("state") == "idle"

    # ---- a shell that ends: its session woken, its answer told, a line of the conversation
    w1 = c.spawn("w1")
    c.wait(lambda: idle(w1), 15)
    asked(w1, "BG-ENDS")
    check("its first answer told", c.wait(lambda: told_to_global("started t-ends") != [] and any("[reply from w1" in p and "started t-ends" in p for p in c.prompts()), 20), str(c.prompts())[-500:])
    check("its end wakes it, told too", c.wait(lambda: any("[reply from w1" in p and "WOKEN" in p for p in c.prompts()), 20), str(c.prompts())[-500:])
    check("its end is a line of the conversation", 'background shell \\"the command t-ends\\" done' in c.session_log(w1), c.session_log(w1)[-500:])

    # ---- killed
    c.wait(lambda: idle(w1), 15)
    asked(w1, "BG-KILLED")
    check("a shell killed: told", c.wait(lambda: any("[reply from w1" in p and "WOKEN" in p and "t-killed" in p for p in c.prompts()), 20), str(c.prompts())[-500:])
    check("     a line of the conversation", 'background shell \\"the command t-killed\\" stopped' in c.session_log(w1), c.session_log(w1)[-500:])

    # ---- the end of its output: a line of the conversation, never given to the model as words of the user (it reads its file)
    c.wait(lambda: idle(w1), 15)
    asked(w1, "BG-TAIL")
    check("a shell saying nothing: woken", c.wait(lambda: any("<task-notification>" in p and "t-tail" in p for p in c.prompts()), 20), str(c.prompts())[-500:])
    check("     the end of its output not in its prompt", not any("<task-notification>" in p and "THE-END-OF-ITS-OUTPUT" in p for p in c.prompts()), str(c.prompts())[-500:])
    check("     but in the line of the conversation", c.wait(lambda: "THE-END-OF-ITS-OUTPUT" in c.session_log(w1), 5), c.session_log(w1)[-500:])

    # ---- an output file that is a fifo or huge: nothing blocked, nothing read whole (64 GB)
    c.wait(lambda: idle(w1), 15)
    asked(w1, "BG-ODD-FILES")
    check("     their tasks started", c.wait(lambda: told_to_global("started both") != [] and any("[reply from w1" in p and "started both" in p for p in c.prompts()), 20), str(c.prompts())[-500:])
    began = time.time()
    shells = c.ask({"cmd": "agents", "session": w1}, 10).get("data") or []
    outputs = {shell.get("taskId", shell.get("task_id")): shell.get("output") for shell in shells}
    check("     read at once", time.time() - began < 5, str(time.time() - began))
    check("     a fifo: nothing", outputs.get("t-fifo") == "", str(outputs)[:300])
    check("     a huge file: its end", (outputs.get("t-huge") or "").endswith("LAST\n") and len(outputs.get("t-huge") or "") <= 6000, str(outputs)[:300])

    # ---- only a task_updated: its end all the same, told once
    c.wait(lambda: idle(w1), 15)
    asked(w1, "BG-UPDATED")
    check("an end told by task_updated alone: woken", c.wait(lambda: any("[reply from w1" in p and "WOKEN" in p and "t-updated" in p for p in c.prompts()), 20), str(c.prompts())[-500:])

    # ---- a task of a kind not known, never seen starting: its end all the same
    c.wait(lambda: idle(w1), 15)
    asked(w1, "BG-UNSEEN")
    check("a task never seen starting: woken", c.wait(lambda: any("[reply from w1" in p and "WOKEN" in p and "a monitor fired" in p for p in c.prompts()), 20), str(c.prompts())[-500:])

    # ---- it ends during a turn of the user: the end of that turn is its result
    c.wait(lambda: idle(w1), 15)
    asked(w1, "BG-DURING")
    c.wait(lambda: any("started t-during" in p for p in c.prompts()), 20)
    c.wait(lambda: idle(w1), 15)
    c.ask({"cmd": "send", "session": w1, "text": "ENDS-IN-TURN"})
    check("its end during a turn of the user: told", c.wait(lambda: any("[reply from w1" in p and "RESULT-IN-THE-USER-TURN" in p for p in c.prompts()), 20), str(c.prompts())[-500:])

    # ---- a wake-up it asked for itself: its result goes to whoever follows it
    c.wait(lambda: idle(w1), 15)
    answer = c.ask({"cmd": "schedule_add", "session": w1, "when": "+1s", "text": "LOOK-AGAIN"})
    check("a schedule made", answer.get("ok") is True, str(answer)[:200])
    check("a schedule of it: its result told", c.wait(lambda: any("[reply from w1" in p and "LOOK-AGAIN" in p for p in c.prompts()), 40), str(c.prompts())[-500:])
    c.wait(lambda: idle(w1), 15)
    answer = c.ask({"cmd": "task_add", "origin": "sub", "owner": w1, "tool": "task_add", "prompt": "SELF-FOLLOW-UP"})
    check("a task for itself made", answer.get("ok") is True, str(answer)[:200])
    check("a task for itself: its result told", c.wait(lambda: any("[reply from w1" in p and "SELF-FOLLOW-UP" in p for p in c.prompts()), 40), str(c.prompts())[-500:])

    # ---- the callback owed goes with its turn: the user's first, the global session's after it
    w2 = c.spawn("w2")
    c.wait(lambda: idle(w2), 15)
    c.ask({"cmd": "send", "session": w2, "text": "SLOW"})
    c.wait(lambda: c.listed(w2).get("state") == "working", 5)
    asked(w2, "THE-QUESTION")
    check("the prompt of the global session: its own answer told", c.wait(lambda: any("[reply from w2" in p and "ANSWER-OF" in p and "THE-QUESTION" in p for p in c.prompts()), 25), str(c.prompts())[-500:])
    check("     never the answer of the turn of the user", not any("[reply from w2" in p and "ANSWER-OF-THE-USER" in p for p in c.prompts()), str(c.prompts())[-500:])

    # ---- the agent gone, its answer given long ago: what stopped with it told
    w3 = c.spawn("w3")
    c.wait(lambda: idle(w3), 15)
    asked(w3, "BG-EXIT")
    check("its agent gone with its background: told", c.wait(lambda: any("[stopped: w3" in p and "the command t-exit" in p for p in c.prompts()), 20), str(c.prompts())[-500:])
    check("     a line of the conversation", "stopped with its agent: the command t-exit" in c.session_log(w3), c.session_log(w3)[-400:])

    # ---- report_status
    w4 = c.spawn("w4")
    c.wait(lambda: idle(w4), 15)
    tool = {"cmd": "report_status", "origin": "sub", "owner": w4, "tool": "report_status"}
    answer = c.ask({**tool, "text": "half of it"})
    check("nobody follows it: told so", "nobody follows" in str(answer), str(answer)[:200])
    asked(w4, "A-QUESTION")
    c.wait(lambda: any("[reply from w4" in p for p in c.prompts()), 20)
    answer = c.ask({**tool, "text": "half of it"})
    check("report_status: told", answer.get("ok") is True and c.wait(lambda: any("[status from w4" in p and "half of it" in p for p in c.prompts()), 15), str(answer)[:200])
    answer = c.ask({**tool, "text": "again"})
    check("     one every 30 s", answer.get("ok") is False and "30 s" in str(answer), str(answer)[:200])
    answer = c.ask({"cmd": "report_status", "text": "from nowhere"})
    check("     a sub-session only", answer.get("ok") is False, str(answer)[:200])
    check("the core alive", c.core.poll() is None, f"exit {c.core.poll()}")
finally:
    c.clean()
fake_harness.finish()
