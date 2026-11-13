#!/usr/bin/env python3
# The question of the previous run (serve --linger): while it waits nothing starts (a queued task never failed on a session not back yet, a one-time schedule never consumed); its answer
# runs once whoever answers (two interfaces at once, its global session resumed once on its conversation, never replaced by a new one, the core never frozen by the session dropped under
# its lock), and the tick starts no second global session while the answer restores; "start new": a new conversation, its sessions in the trash.
# Usage: tests/restore_answer.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The answer to the previous-run question."""
import json
import threading
import time

import fake_harness
from fake_harness import Core, check

CONFIG = '[ui]\nnotify = false\nmachine_days = 0\n[commands]\nclaude = "{AGENT}"\n'

def global_starts(c: Core) -> list:
    return [line for line in c.log().splitlines() if line.startswith("START") and "global=True" in line]

def saved_id(c: Core) -> str:
    with open(f"{c.T}/data/state.json", encoding="utf-8") as state:
        return next(s for s in json.load(state)["sessions"] if s.get("global")).get("claude_id", "")

# the agents listed slowly (startGlobal waits for `claude agents --json`), a last line when stopped: the reader of a session dropped would wait for the lock of the manager
c = Core("restore", CONFIG, "fake_claude_slow.py", {"FAKE_AGENTS_S": "2", "FAKE_LINE_ON_TERM": "1"}, enabled=True)
try:
    c.start()
    sid = c.spawn("worker")
    c.wait(lambda: c.listed("g").get("state") == "idle", 10)
    c.ask({"cmd": "send", "session": "g", "text": "remember this"})
    c.wait(lambda: any("remember this" in p for p in c.prompts()), 10)
    now = int(time.time())
    c.ask({"cmd": "task_add", "prompt": "TASK-P", "target": sid, "not_before": now + 4})
    c.ask({"cmd": "schedule_add", "when": "+5s", "text": "SCHED-P", "session": sid})
    code = c.stop()
    conversation = saved_id(c)
    check("the first run ends, its global conversation saved", code == 0 and bool(conversation), f"exit {code}, {conversation!r}")
    time.sleep(6)  # both due
    open(f"{c.T}/fake.log", "w", encoding="utf-8").close()
    c.start("--linger")
    c.interface()
    asked = c.sync()
    check("the previous run asked", asked.get("restore_ask") is True, json.dumps(asked)[:200])
    time.sleep(4)
    tasks = {t["id"]: t for t in c.ask({"cmd": "task_list"}).get("data") or []}
    schedules = {w["id"]: w for w in c.ask({"cmd": "schedule_list"}).get("data") or []}
    check("while it waits a queued task does not fail", all(t.get("status") == "queued" for t in tasks.values()) and len(tasks) == 1, json.dumps(tasks)[:300])
    check("while it waits a one-time schedule is not consumed", len(schedules) == 1 and all(w.get("status") not in ("done", "failed") and not w.get("runs") for w in schedules.values()),
          json.dumps(schedules)[:300])
    check("while it waits no global session started", global_starts(c) == [], str(global_starts(c)))

    replies = []
    threads = [threading.Thread(target=lambda: replies.append(c.ask({"cmd": "restore_answer", "restore": True}, 40))) for _ in range(2)]
    for thread in threads:
        thread.start()
    time.sleep(0.5)
    during = c.sync()
    check("its answer running: the question closed for the other interfaces", during.get("restore_ask") is False, json.dumps(during.get("restore_ask")))
    for thread in threads:
        thread.join(45)
    check("two interfaces answer: both answered", len(replies) == 2 and all(r.get("ok") for r in replies), json.dumps(replies)[:300])
    check("the core not frozen", c.wait(lambda: c.listed("g").get("state") == "idle", 15), json.dumps(c.listed("g"))[:200])
    time.sleep(3)
    starts = global_starts(c)
    check("its global session started once, resumed on its conversation", len(starts) == 1 and f"START {conversation} resumed=True" in starts[0], str(starts))
    told = [text for text in c.told("g") if "was missing" in text]
    check("the tick did not start it a second time", not told, str(told))
    check("the session is back", c.listed(sid).get("state") == "idle", json.dumps(c.listed(sid))[:200])
    check("the task runs once it is back", c.wait(lambda: any("TASK-P" in p for p in c.prompts()), 15), str(c.prompts()))
    check("the schedule too", c.wait(lambda: any("SCHED-P" in p for p in c.prompts()), 15), str(c.prompts()))
    second = c.ask({"cmd": "restore_answer", "restore": False}, 20)
    time.sleep(1)
    check("an answer after it changes nothing", second.get("ok") is True and len(global_starts(c)) == 1 and c.listed(sid).get("state") == "idle", json.dumps(second)[:200])
    begun = time.time()
    code = c.stop(20)
    check("it ends on its signal", code == 0 and time.time() - begun < 20, f"exit {code}")
    check("the same conversation saved", saved_id(c) == conversation, f"{saved_id(c)} != {conversation}")

    # start new: the sessions in the trash, a new conversation
    open(f"{c.T}/fake.log", "w", encoding="utf-8").close()
    c.start("--linger")
    got = c.ask({"cmd": "restore_answer", "restore": False}, 40)
    check("start new: answered", got.get("ok") is True, json.dumps(got)[:200])
    c.wait(lambda: bool(global_starts(c)), 15)
    starts = global_starts(c)
    check("start new: a new conversation", len(starts) == 1 and "resumed=False" in starts[0], str(starts))
    check("start new: its sessions in the trash", any(s.get("id") == sid for s in c.ask({"cmd": "trash"}).get("data") or []), json.dumps(c.ask({"cmd": "trash"}))[:200])
    check("start new: the core alive", c.core is not None and c.core.poll() is None)
finally:
    c.clean()
fake_harness.finish()
