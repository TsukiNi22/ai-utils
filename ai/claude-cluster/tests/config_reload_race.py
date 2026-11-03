#!/usr/bin/env python3
# The settings written again and again (two writers: the reload of each, the tick) while every thread reads them (the lists, the state of the interfaces, the settings page, the jobs copying their
# settings, the styles, the providers): never a read of what a reload freed, every change applied, none lost. A config.toml broken by hand: told, the previous values kept, the next good one read.
# Usage: tests/config_reload_race.py [path/to/claude-cluster] [seconds]   (default: /tmp/cc-build/claude-cluster, 30 s)
"""Config::reload under the readers of every thread."""
from collections.abc import Callable
import json
import os
import re
import socket
import sys
import threading
import time

import fake_harness
from fake_harness import Core, check

SECONDS = float(sys.argv[2]) if len(sys.argv) > 2 else 30
PROVIDERS = "".join(f'[providers.own{i}]\ndriver = "claude"\nbase_url = "http://127.0.0.1:{9000 + i}"\nmodel = "m{i}"\n' for i in range(5))
STYLES = "".join(f'[styles.own{i}]\nbg = "#00000{i}"\ntext = "#ffffff"\n' for i in range(5))
CONFIG = ('[ui]\nnotify = false\nmachine_days = 0\n[commands]\nclaude = "{AGENT}"\n'
          '[orchestrator]\nenabled = true\npatterns = ["a-long-pattern-of-a-heavy-command-held-on-the-heap = build", "another-long-pattern-of-a-heavy-command-on-the-heap = tests"]\n'
          + PROVIDERS + STYLES)

def setting(c: Core, section: str, key: str) -> str:
    rows = c.ask({"cmd": "settings_get", "section": section}).get("data") or []
    return next((row.get("value") for row in rows if row.get("key") == key), "?")

c = Core("reload", CONFIG, "fake_claude.py")
stop = threading.Event()
counts = {"reads": 0, "writes": 0, "lost": 0}
try:
    c.start("--no-global")
    sid = c.spawn("worker", "hello")
    c.wait(lambda: c.listed(sid).get("state") == "idle", 15)

    def call(s: socket.socket, request: dict) -> dict:
        s.sendall((json.dumps(request) + "\n").encode())
        data = b""
        while not data.endswith(b"\n"):
            chunk = s.recv(1 << 20)
            if not chunk:
                raise OSError("closed")
            data += chunk
        return json.loads(data)

    def reader(request: dict) -> None:
        while not stop.is_set():
            try:
                s = socket.socket(socket.AF_UNIX)
                s.settimeout(30)
                s.connect(f"{c.T}/sock")
                while not stop.is_set():
                    call(s, request)
                    counts["reads"] += 1
                s.close()
            except OSError:
                if c.core.poll() is not None:
                    return
                time.sleep(0.05)

    def writer(section: str, key: str, value: Callable[[int], str]) -> None:
        n = 0
        while not stop.is_set():
            n += 1
            try:
                got = c.ask({"cmd": "settings_set", "section": section, "key": key, "value": value(n), "confirmed": True}, 30)
                counts["writes"] += 1 if got.get("ok") else 0
            except OSError:
                if c.core.poll() is not None:
                    return
                time.sleep(0.05)

    readers = [{"cmd": "list"}, {"cmd": "list"}, {"cmd": "ui_state"}, {"cmd": "ui_state"}, {"cmd": "settings_get", "section": "orchestrator"}, {"cmd": "job_status", "id": "nothing"},
               {"cmd": "job_status", "id": "nothing"}, {"cmd": "orch_state"}]
    writers = [("orchestrator", "patterns", lambda n: f"a-long-pattern-of-a-heavy-command-held-on-the-heap-{n} = build,another-long-pattern-of-a-heavy-command-on-the-heap-{n} = tests"),
               ("sessions", "keep_warm_margin", lambda n: str(20 + n % 30)), ("sessions", "trash_days", lambda n: str(10 + n % 50)), ("ui", "style", lambda n: f"own{n % 5}")]
    threads = [threading.Thread(target=reader, args=(request,), daemon=True) for request in readers] + [threading.Thread(target=writer, args=one, daemon=True) for one in writers]
    for thread in threads:
        thread.start()
    end = time.time() + SECONDS
    while time.time() < end and c.core.poll() is None:
        time.sleep(0.2)
    stop.set()
    for thread in threads:
        thread.join(40)
    check("the core alive after the changes under its readers", c.core.poll() is None, f"exit {c.core.poll()}, {counts}")
    check("it went on reading and writing", counts["reads"] > 100 and counts["writes"] > 20, str(counts))

    # none lost: what is written last is what is read, at once
    for section, key, value in (("sessions", "trash_days", "33"), ("sessions", "keep_warm_margin", "47"), ("ui", "style", "own3"), ("orchestrator", "max_jobs", "7")):
        c.ask({"cmd": "settings_set", "section": section, "key": key, "value": value, "confirmed": True})
        check(f"{section}.{key} read as written", setting(c, section, key) == value, setting(c, section, key))
    # what still reads the settings without the versions (a task queue): follows them
    c.ask({"cmd": "settings_set", "section": "sessions", "key": "task_queue", "value": "1"})
    c.ask({"cmd": "task_add", "prompt": "one", "target": sid, "not_before": int(time.time()) + 3600})
    refused = c.ask({"cmd": "task_add", "prompt": "two", "target": sid, "not_before": int(time.time()) + 3600})
    check("a queue of one task: the second one refused at once", refused.get("ok") is False, json.dumps(refused)[:200])
    # the list of the sessions follows them too; a reader of the first object sees the values of the start for ever, no error tells it: nothing reads it but through current()
    c.ask({"cmd": "settings_set", "section": "agent", "key": "effort", "value": "high", "confirmed": True})
    label = str(c.listed(sid).get("effort_label"))
    check("the list of the sessions: the setting followed", "high" in label, label)
    stale = []
    for folder, _, names in os.walk(os.path.join(fake_harness.HERE, "..", "src")):
        for name in names:
            with open(os.path.join(folder, name), encoding="utf-8", errors="replace") as source:
                stale += [f"{name}:{number}" for number, line in enumerate(source, 1) if re.search(r"this->_config\b(?!\.current\(\))", line)]
    check("no reader of the first object in the sources", stale == [], str(stale[:8]))

    # changed by hand: read by the tick; broken: told, the values before kept
    with open(f"{c.T}/cfg/config.toml", encoding="utf-8") as config:
        text = config.read()
    time.sleep(0.05)
    with open(f"{c.T}/cfg/config.toml", "w", encoding="utf-8") as out:
        out.write(text + "\n[broken\n")
    check("a broken file: told", c.wait(lambda: any("config error" in n for n in c.told("")), 5), str(c.told("")))
    check("... the values before kept", setting(c, "sessions", "trash_days") == "33", setting(c, "sessions", "trash_days"))
    time.sleep(0.05)
    with open(f"{c.T}/cfg/config.toml", "w", encoding="utf-8") as out:
        out.write(text.replace("trash_days = 33", "trash_days = 44"))
    check("the next good one read", c.wait(lambda: setting(c, "sessions", "trash_days") == "44", 5), setting(c, "sessions", "trash_days"))
    check("the core alive at the end", c.core.poll() is None)
finally:
    stop.set()
    c.clean()
fake_harness.finish()
