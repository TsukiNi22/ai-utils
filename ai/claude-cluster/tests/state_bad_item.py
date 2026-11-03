#!/usr/bin/env python3
# One bad item of state.json (edited by hand, an infinite number written as null) never stops the core: left out, told, a copy kept; a session whose cost is null is kept, the next ids never one
# already there. An agent telling a huge cost twice (over a restart of its agent): never written as null. `claude agents --json` of another shape (a null sessionId): the core starts, the global
# session resumed; the entry that does hold its conversation still stopped.
# Usage: tests/state_bad_item.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The start of the core on a state or an output of another shape."""
import json
import os
import time

import fake_harness
from fake_harness import Core, check

CONFIG = '[ui]\nnotify = false\nmachine_days = 0\nrestore = "always"\n[commands]\nclaude = "{AGENT}"\n'

def state_of(c: Core) -> dict:
    with open(f"{c.T}/data/state.json", encoding="utf-8") as state:
        return json.load(state)

# ---- bad items
c = Core("bad-item", CONFIG, "fake_claude.py")
try:
    c.start("--no-global")
    keep = c.spawn("keep")
    other = c.spawn("other")
    c.ask({"cmd": "task_add", "prompt": "GOOD-TASK", "target": keep, "not_before": int(time.time()) + 3600})
    c.ask({"cmd": "schedule_add", "when": "+3600s", "text": "GOOD-SCHEDULE", "session": keep})
    c.stop()
    state = state_of(c)
    good_task = state["tasks"][0]["id"]
    for spec in state["sessions"]:
        if spec["id"] == other:
            spec["cost"] = None  # an infinite cost of a version before
    state["sessions"].append({"id": 7, "name": ["not", "a", "name"]})
    state["trash"] = [{"id": "s90", "name": "gone", "created": "not a number", "deleted": 1}]
    state["tasks"].append({"id": "t50", "prompt": "BAD-TASK", "status": "queued", "maxCost": None})
    state["tasks"].append(12)
    state["schedules"].append({"id": "w50", "every": "not a number"})
    state["ui"] = "not a map"
    state["next"] = None
    with open(f"{c.T}/data/state.json", "w", encoding="utf-8") as out:
        json.dump(state, out)
    c.start("--no-global")
    check("the core starts", c.core.poll() is None and c.answers(), open(f"{c.T}/core.err", encoding="utf-8", errors="replace").read()[-300:])
    sessions = [s.get("id") for s in c.ask({"cmd": "list"}).get("data") or []]
    check("its good sessions restored, the one with a null cost too", keep in sessions and other in sessions, str(sessions))
    tasks = [t.get("id") for t in c.ask({"cmd": "task_list"}).get("data") or []]
    check("its good task kept, the bad ones left out", good_task in tasks and "t50" not in tasks, str(tasks))
    schedules = [w.get("id") for w in c.ask({"cmd": "schedule_list"}).get("data") or []]
    check("its good schedule kept, the bad one left out", len(schedules) == 1 and "w50" not in schedules, str(schedules))
    told = [text for text in c.told("") if "state.json" in text]
    check("told once, a copy kept", len(told) == 1 and os.path.exists(f"{c.T}/data/state.json.bad"), str(told))
    fresh = c.spawn("fresh")
    check("a new session: never the id of one kept", fresh not in (keep, other) and bool(fresh), fresh)
    c.stop()
    for bad in ("not json", "[1, 2]"):
        with open(f"{c.T}/data/state.json", "w", encoding="utf-8") as out:
            out.write(bad)
        c.start("--no-global")
        check(f"state.json = {bad}: the core starts", c.answers() and c.core.poll() is None)
        c.stop()
finally:
    c.clean()

# ---- a huge cost, twice
c = Core("bad-cost", CONFIG, "fake_claude_cost.py")
try:
    c.start("--no-global")
    sid = c.spawn("worker")
    time.sleep(1)
    c.ask({"cmd": "send", "session": sid, "text": "one"})
    c.wait(lambda: (c.listed(sid).get("metrics") or {}).get("turns", 1) >= 1 and c.listed(sid).get("state") == "idle", 10)
    time.sleep(1)
    c.ask({"cmd": "model", "session": sid, "model": "other"})  # its agent started again: its cost adds to the one before
    time.sleep(2)
    c.ask({"cmd": "send", "session": sid, "text": "two"})
    time.sleep(2)
    c.stop()
    costs = [s.get("cost") for s in state_of(c)["sessions"]]
    check("a huge cost twice: never null in state.json", costs and all(isinstance(cost, (int, float)) for cost in costs), str(costs))
    c.start("--no-global")
    check("the core starts again", c.answers() and c.core.poll() is None, open(f"{c.T}/core.err", encoding="utf-8", errors="replace").read()[-300:])
    check("its session back", any(s.get("id") == sid for s in c.ask({"cmd": "list"}).get("data") or []))
finally:
    c.clean()

# ---- `claude agents --json`
c = Core("bad-agents", CONFIG, "fake_claude_slow.py", {"FAKE_AGENTS_S": "0"}, enabled=True)
try:
    c.start()
    c.wait(lambda: c.listed("g").get("state") == "idle", 15)
    c.ask({"cmd": "send", "session": "g", "text": "hello"})
    c.wait(lambda: any("hello" in p for p in c.prompts()), 10)
    c.stop()
    conversation = next(s for s in state_of(c)["sessions"] if s.get("global")).get("claude_id")
    open(f"{c.T}/fake.log", "w", encoding="utf-8").close()
    c.env["FAKE_AGENTS_OUT"] = json.dumps([{"id": "bg1", "sessionId": None}, {"id": None, "sessionId": conversation}, {"id": 5, "sessionId": 6}, 7, "text", {"sessionId": conversation}, {}])
    c.start()
    check("a null sessionId: the core starts", c.answers() and c.core.poll() is None, open(f"{c.T}/core.err", encoding="utf-8", errors="replace").read()[-300:])
    check("its global session resumed", c.wait(lambda: f"START {conversation} resumed=True" in c.log(), 15), c.log()[:300])
    check("nothing stopped for those", "TOOL stop" not in c.log(), c.log()[:300])
    c.stop()
    c.env["FAKE_AGENTS_OUT"] = json.dumps([{"id": "bg1", "sessionId": conversation}])
    open(f"{c.T}/fake.log", "w", encoding="utf-8").close()
    c.start()
    check("the background claude of its conversation still stopped", c.wait(lambda: "TOOL stop bg1" in c.log(), 15), c.log()[:300])
finally:
    c.clean()
fake_harness.finish()
