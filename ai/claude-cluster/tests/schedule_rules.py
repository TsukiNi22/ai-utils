#!/usr/bin/env python3
# The time rules of the schedules: a cron denser than its days / window finds its next time (never refused, never ended by a count of occurrences), weekday 7 is Sunday as in a
# crontab, the rules are bounded and a long one never holds the lock of the manager, a time past the rules of a zone never breaks a list, a number stod cannot read and a schedule
# that throws stop nothing else, a command is a string (never the JSON text of something else, run by /bin/sh).
# Usage: tests/schedule_rules.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The rules of time of the schedules."""
import datetime
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time

T = tempfile.mkdtemp(prefix="cc-schedrules-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
failed = False
core = None

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict, timeout: float = 60) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(timeout)
    s.connect(f"{T}/sock")
    s.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        data += chunk
    s.close()
    return json.loads(data)

def start(*args: str) -> None:
    global core
    core = subprocess.Popen([BIN, "serve", "--no-global", *args], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    end = time.time() + 30
    while time.time() < end:
        try:
            ask({"cmd": "hello"})
            return
        except OSError:
            time.sleep(0.2)

def stop() -> None:
    global core
    if core is None:
        return
    core.terminate()
    try:
        core.wait(25)
    except subprocess.TimeoutExpired:
        core.kill()
        core.wait()
    core = None

def dry(**request: object) -> dict:
    return ask({"cmd": "schedule_add", "text": "x", "session": sid, "dry_run": True, "tz": "UTC", **request})

def weekdays(answer: dict) -> set:
    """The days of the week it fires on (3 letters), from a dry run."""
    return {datetime.datetime.strptime(t, "%Y-%m-%d %H:%M").strftime("%a") for t in (answer.get("data") or {}).get("would_fire", [])}

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n')
try:
    start()
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
    time.sleep(1.5)

    # ---- a cron denser than its days / window
    now = datetime.datetime.now(datetime.timezone.utc)
    opening = (now + datetime.timedelta(hours=9)).strftime("%H:00")
    closing = (now + datetime.timedelta(hours=11)).strftime("%H:00")
    answer = dry(cron="*/5 * * * *", window=f"{opening}-{closing}")
    fires = (answer.get("data") or {}).get("would_fire", [])
    check("cron */5 with a window opening in hours: not refused", answer.get("ok") is True and len(fires) == 5, json.dumps(answer)[:300])
    check("its first time is the opening of the window", bool(fires) and fires[0].endswith(opening), str(fires))
    day = (now + datetime.timedelta(days=3)).strftime("%a").lower()
    answer = dry(cron="0 * * * *", days=day)
    check("cron of every hour with one day of the week: not refused", answer.get("ok") is True and weekdays(answer) == {day.capitalize()}, json.dumps(answer)[:300])
    answer = dry(cron="*/15 * * * *", window="09:00-18:00", days="mon-fri")
    check("*/15, 09:00-18:00, mon-fri: all inside", answer.get("ok") is True and weekdays(answer) <= {"Mon", "Tue", "Wed", "Thu", "Fri"}, json.dumps(answer)[:300])
    check("a date of the year", dry(cron="0 9 29 2 *").get("ok") is True, json.dumps(dry(cron="0 9 29 2 *"))[:200])
    # a rule that can never fire: said, soon
    began = time.time()
    answer = dry(cron="0 9 * * mon", days="tue")
    check("a cron outside its days for ever: refused", answer.get("ok") is False and "never fires" in answer.get("error", ""), json.dumps(answer)[:300])
    answer = dry(cron="0 9 * * *", days="fri", window="10:00-10:01")
    check("outside its window for ever: refused", answer.get("ok") is False and "never fires" in answer.get("error", ""), json.dumps(answer)[:300])
    check("told in a few seconds", time.time() - began < 15, f"{time.time() - began:.1f} s")

    # ---- for real: a window closing soon, the schedule stays alive for the day after
    if datetime.datetime.now().second > 45:
        time.sleep(61 - datetime.datetime.now().second)
    local = datetime.datetime.now()
    window = f"{local.strftime('%H:%M')}-{(local + datetime.timedelta(minutes=2)).strftime('%H:%M')}" # one run in it: the next minute, then closed
    wid = (ask({"cmd": "schedule_add", "text": "IN-THE-WINDOW", "session": sid, "cron": "* * * * *", "window": window}).get("data") or {}).get("id", "")
    w = {}
    end = time.time() + 75
    while time.time() < end:
        w = next((one for one in ask({"cmd": "schedule_list"}).get("data", []) if one.get("id") == wid), {})
        if w.get("runs", 0) >= 1:
            break
        time.sleep(1)
    check("it ran in its window", w.get("runs", 0) >= 1, json.dumps(w)[:300])
    time.sleep(2)
    w = next((one for one in ask({"cmd": "schedule_list"}).get("data", []) if one.get("id") == wid), {})
    check("it stays waiting for its window tomorrow, never ended by the times it skips", w.get("status") == "waiting" and "no next time" not in w.get("reason", ""), json.dumps(w)[:300])
    ask({"cmd": "schedule_act", "id": wid, "action": "delete"})

    # ---- weekday 7 is Sunday
    check("0 9 * * 7: Sundays only", weekdays(dry(cron="0 9 * * 7")) == {"Sun"}, str(weekdays(dry(cron="0 9 * * 7"))))
    check("0 9 * * sun: the same", weekdays(dry(cron="0 9 * * sun")) == {"Sun"}, str(weekdays(dry(cron="0 9 * * sun"))))
    check("0 9 * * 0: the same", weekdays(dry(cron="0 9 * * 0")) == {"Sun"}, str(weekdays(dry(cron="0 9 * * 0"))))
    check("0 9 * * 5-7: Friday to Sunday", weekdays(dry(cron="0 9 * * 5-7")) == {"Fri", "Sat", "Sun"}, str(weekdays(dry(cron="0 9 * * 5-7"))))
    check("0 9 * * fri-sun: the same", weekdays(dry(cron="0 9 * * fri-sun")) == {"Fri", "Sat", "Sun"}, str(weekdays(dry(cron="0 9 * * fri-sun"))))
    check("0 9 * * *: every day", len(weekdays(dry(cron="0 9 * * *"))) == 5, str(weekdays(dry(cron="0 9 * * *"))))
    check("0 9 * * */2: Sunday, Tuesday...", weekdays(dry(cron="0 9 * * */2")) == {"Sun", "Tue", "Thu", "Sat"}, str(weekdays(dry(cron="0 9 * * */2"))))
    check("days 7: Sundays only", weekdays(dry(every=1440, when="+1h", days="7")) == {"Sun"}, str(weekdays(dry(every=1440, when="+1h", days="7"))))
    check("days 6-7: the weekend", weekdays(dry(every=1440, when="+1h", days="6-7")) == {"Sat", "Sun"}, str(weekdays(dry(every=1440, when="+1h", days="6-7"))))
    check("a weekday 8 refused", dry(cron="0 9 * * 8").get("ok") is False, json.dumps(dry(cron="0 9 * * 8"))[:200])

    # ---- bounded
    item = "09:00-18:00"
    check("a window of 16 ranges", dry(cron="0 10 * * *", window=",".join([item] * 16)).get("ok") is True, json.dumps(dry(cron="0 10 * * *", window=",".join([item] * 16)))[:200])
    check("17 ranges: refused", dry(cron="0 10 * * *", window=",".join([item] * 17)).get("ok") is False, "")
    check("quiet too", dry(cron="0 10 * * *", quiet=",".join(["01:00-02:00"] * 17)).get("ok") is False, "")
    check("a window of 300 characters: refused", dry(cron="0 10 * * *", window=" " * 300 + item).get("ok") is False, "")
    check("a cron of 300 characters: refused", dry(cron="0 10 * * " + ",".join(["1"] * 150)).get("ok") is False, "")
    check("days of 300 characters: refused", dry(cron="0 10 * * *", days=",".join(["1"] * 150)).get("ok") is False, "")
    pid = (ask({"cmd": "hello"}).get("data") or {}).get("pid", 0)
    check("a time of 100 000 characters: refused", ask({"cmd": "schedule_add", "text": "x", "session": sid, "when": "1" * 100000}).get("ok") is False, "")
    check("an if.usage of 100 000 characters: the same", ask({"cmd": "schedule_add", "text": "x", "session": sid, "when": "+1h", "if": {"usage": "5h>" + "1" * 100000}}).get("ok") is False, "")
    check("the same core is still there", (ask({"cmd": "hello"}).get("data") or {}).get("pid", 0) == pid, "")

    # ---- a time past the rules of a zone
    before = len(ask({"cmd": "schedule_list"}).get("data", []))
    answer = ask({"cmd": "schedule_add", "when": "+1h", "text": "x", "session": sid, "until": "9999999999999"})
    check("until past the year 9999: refused", answer.get("ok") is False and "9999" in answer.get("error", ""), json.dumps(answer)[:200])
    for when in ("9999999999999", "9223372036854775808", "+99999999999999999999999d", "+3650000d"):
        answer = ask({"cmd": "schedule_add", "when": when, "text": "x", "session": sid, "tz": "Europe/Paris"})
        check(f"when {when}: refused", answer.get("ok") is False, json.dumps(answer)[:200])
    check("none of them kept", len(ask({"cmd": "schedule_list"}).get("data", [])) == before, json.dumps(ask({"cmd": "schedule_list"}))[:300])
    check("a time far away still taken", ask({"cmd": "schedule_add", "when": "99999999999", "text": "x", "session": sid, "dry_run": True, "tz": "Europe/Paris"}).get("ok") is True, "")

    # ---- what an older state.json holds: the lists work, the others go on
    stop()
    with open(f"{T}/data/state.json", encoding="utf-8") as f:
        state = json.load(f)
    moment = int(time.time()) + 3
    local = datetime.datetime.now()
    closing = f"{(local - datetime.timedelta(minutes=2)).strftime('%H:%M')}-{(local + datetime.timedelta(minutes=1)).strftime('%H:%M')}" # closed at its next time: looked for minute by minute
    state["schedules"] = [
        {"id": "w70", "at": 9999999999999, "text": "FAR", "session": sid, "action": "prompt", "status": "waiting", "until": 9999999999999, "every": 1, "tz": "Europe/Paris"},
        {"id": "w71", "at": moment, "text": "POISON-NUMBER", "session": sid, "action": "prompt", "status": "waiting", "cond": {"usage": "5h>1" + "0" * 400}},
        {"id": "w72", "at": moment, "text": "POISON-ZONE", "session": sid, "action": "prompt", "status": "waiting", "every": 1, "tz": "Mars/Olympus", "window": "00:00-23:59"},
        {"id": "w73", "at": moment, "text": "POISON-COMMAND", "session": sid, "action": "command", "status": "waiting", "options": {"command": ["echo", "no"]}},
        {"id": "w74", "at": moment + 1, "text": "VICTIM-AFTER", "session": sid, "action": "prompt", "status": "waiting"},
        {"id": "w75", "at": moment, "text": "LONG-WINDOW", "session": sid, "action": "prompt", "status": "waiting", "every": 2, "window": ",".join([closing] * 2000)},
    ]
    state["next_schedule"] = 80
    with open(f"{T}/data/state.json", "w", encoding="utf-8") as f:
        json.dump(state, f)
    open(f"{T}/fake.log", "w", encoding="utf-8").close()
    start("--restore")
    answer = ask({"cmd": "schedule_list"})
    check("schedule_list with a time past the rules of the zone", answer.get("ok") is True, json.dumps(answer)[:300])
    listed = subprocess.run([BIN, "schedule", "list"], env=ENV, capture_output=True, text=True, timeout=60)
    check("the same from the command line", listed.returncode == 0 and "w70" in listed.stdout, (listed.stdout + listed.stderr)[:300])
    worst = 0.0
    end = time.time() + 12
    while time.time() < end:
        began = time.time()
        ask({"cmd": "task_list"})
        worst = max(worst, time.time() - began)
        time.sleep(0.2)
    with open(f"{T}/fake.log", encoding="utf-8") as f:
        log = f.read()
    check("the schedule after them runs", "VICTIM-AFTER" in log, log[-400:])
    check("a condition it cannot read: not run", "POISON-NUMBER" not in log, log[-400:])
    check("a command that is not a string: never run", "output of" not in log, log[-400:])
    by = {w.get("id"): w for w in ask({"cmd": "schedule_list", "archived": True}).get("data", [])}
    check("the condition said why", "cannot be read" in by.get("w71", {}).get("note", ""), json.dumps(by.get("w71"))[:300])
    check("the zone gone: said in its note, or ended", "failed" in by.get("w72", {}).get("note", "") or by.get("w72", {}).get("status") == "cancelled", json.dumps(by.get("w72"))[:300])
    check("the command said why", "failed" in by.get("w73", {}).get("note", ""), json.dumps(by.get("w73"))[:300])
    check("a long window of an older state read to its bound: nothing held the lock of the manager", worst < 5.0, f"{worst:.1f} s")
    check("that one ran and is waiting for its window", by.get("w75", {}).get("status") == "waiting" and by.get("w75", {}).get("runs", 0) + by.get("w75", {}).get("skipped", 0) >= 1, json.dumps(by.get("w75"))[:300])

    # ---- a command is a string
    stop()
    os.makedirs(f"{T}/m", exist_ok=True)
    start("--restore")
    for name, request in (("if.command an object", {"cmd": "schedule_add", "when": "+1s", "text": "x", "session": sid, "if": {"command": {"a": f"$(touch {T}/m/m1)"}}}),
                          ("action command, a list", {"cmd": "schedule_add", "when": "+1s", "text": "x", "session": sid, "action": "Command", "command": [f"$(touch {T}/m/m2)"]}),
                          ("options.command a number", {"cmd": "schedule_add", "when": "+1s", "text": "x", "session": sid, "action": "command", "options": {"command": 5}}),
                          ("options.on_event a list", {"cmd": "schedule_add", "when": "+1s", "text": "x", "action": "task", "options": {"on_event": {"type": "command", "command": [f"$(touch {T}/m/m3)"]}}}),
                          ("on_event of a task a list", {"cmd": "task_add", "prompt": "x", "target": sid, "on_event": {"type": "Command", "command": [f"$(touch {T}/m/m4)"]}})):
        answer = ask(request)
        check(f"{name}: refused", answer.get("ok") is False, json.dumps(answer)[:200])
    time.sleep(4)
    check("nothing of them ran", os.listdir(f"{T}/m") == [], str(os.listdir(f"{T}/m")))
    check("a string: taken", ask({"cmd": "schedule_add", "when": "+1h", "text": "x", "session": sid, "if": {"command": "true"}}).get("ok") is True, "")
    check("a string in on_event: taken", ask({"cmd": "task_add", "prompt": "x", "target": sid, "on_event": {"type": "command", "command": "false"}}).get("ok") is True, "")
finally:
    stop()
    subprocess.run(["pkill", "-f", T], check=False)
    shutil.rmtree(T, ignore_errors=True)
sys.exit(1 if failed else 0)
