#!/usr/bin/env python3
# A stand-in for claude (stream-json) writing its transcript like claude's (~/.claude/projects/fake/<id>.jsonl, uuid / parentUuid). A prompt holding "monitor", or the first keep-warm
# prompt when FAKE_MONITOR_AFTER_WARM is set: its turn ends, FAKE_MONITOR_DELAY seconds later a second turn starts WITHOUT any prompt (a Monitor / a background task).
# FAKE_REPLAY_AFTER_WARM: the second keep-warm prompt: FAKE_MONITOR_DELAY seconds later a prompt typed on claude.ai (its replay) and its answer.
# FAKE_SLOW_RESUME: seconds before it reads its input when started with --resume-session-at (a claude loading); FAKE_NEW_IDS: a conversation of its own unless resumed
import json
import os
import sys
import threading
import time
import uuid

T = os.environ["FAKE_T"]
log = open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8")
log.write("ARGV " + json.dumps(sys.argv[1:]) + "\n")
log.flush()
if "--resume-session-at" in sys.argv and os.environ.get("FAKE_SLOW_RESUME"):
    time.sleep(float(os.environ["FAKE_SLOW_RESUME"]))
lock = threading.Lock()
transcript = os.path.expanduser("~/.claude/projects/fake/00000000-0000-0000-0000-000000000001.jsonl")
os.makedirs(os.path.dirname(transcript), exist_ok=True)
leaf = None
if "--resume-session-at" in sys.argv:
    leaf = sys.argv[sys.argv.index("--resume-session-at") + 1]
elif os.path.exists(transcript):
    with open(transcript, encoding="utf-8") as old:
        leaf = ([json.loads(x)["uuid"] for x in old if x.strip()] or [None])[-1]

def record(kind: str, content: str | list) -> str:
    global leaf
    me = str(uuid.uuid4())
    with open(transcript, "a", encoding="utf-8") as out:
        out.write(json.dumps({"type": kind, "uuid": me, "parentUuid": leaf, "message": {"role": kind, "content": content}}) + "\n")
    leaf = me
    return me

def out(event: dict) -> None:
    print(json.dumps(event), flush=True)

def turn(text: str) -> None:
    with lock:
        out({"type": "assistant", "uuid": record("assistant", [{"type": "text", "text": text}]),
             "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": text}], "usage": {"input_tokens": 1, "output_tokens": 1}}})
        out({"type": "result", "subtype": "success", "is_error": False, "result": text, "session_id": session_id, "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1,
             "usage": {"input_tokens": 1, "output_tokens": 1}})

def later(text: str) -> None:
    time.sleep(float(os.environ.get("FAKE_MONITOR_DELAY", "3")))
    turn(text)

def replayed() -> None:
    time.sleep(float(os.environ.get("FAKE_MONITOR_DELAY", "3")))
    with lock:
        out({"type": "user", "isReplay": True, "uuid": record("user", "PROMPT-FROM-CLAUDE-AI"), "message": {"role": "user", "content": "PROMPT-FROM-CLAUDE-AI"}})
    turn("ANSWER-TO-CLAUDE-AI")

session_id = "00000000-0000-0000-0000-000000000001"
if os.environ.get("FAKE_NEW_IDS"):
    session_id = sys.argv[sys.argv.index("--resume") + 1] if "--resume" in sys.argv else str(uuid.uuid4())
out({"type": "system", "subtype": "init", "session_id": session_id, "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd()})
for line in sys.stdin:
    try:
        msg = json.loads(line)
    except ValueError:
        continue
    if msg.get("type") != "user":
        continue
    content = msg.get("message", {}).get("content", "")
    text = json.dumps(content)
    log.write("PROMPT " + text + "\n")
    log.flush()
    with lock:
        record("user", content)
    turn("ok")
    if "monitor" in text:
        threading.Thread(target=later, args=("CONTROL-MONITOR-RESULT",), daemon=True).start()
    if "keep-warm" in text and os.environ.get("FAKE_MONITOR_AFTER_WARM") and not os.path.exists(T + "/armed"):
        open(T + "/armed", "w", encoding="utf-8").close()
        threading.Thread(target=later, args=("MONITOR-RESULT-AFTER-WAKE",), daemon=True).start()
    elif "keep-warm" in text and os.environ.get("FAKE_REPLAY_AFTER_WARM") and not os.path.exists(T + "/replayed"):
        open(T + "/replayed", "w", encoding="utf-8").close()
        threading.Thread(target=replayed, daemon=True).start()
