#!/usr/bin/env python3
# a stand-in for `claude` (stream-json): logs each user message it receives, answers "ok" to it
import json, os, sys, time
log = open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8")
log.write("ARGV " + json.dumps(sys.argv[1:]) + "\n"); log.flush()
print(json.dumps({"type": "system", "subtype": "init", "session_id": "00000000-0000-0000-0000-000000000001", "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd(),
    "skills": [x for x in os.environ.get("FAKE_CLAUDE_SKILLS", "").split(",") if x], "slash_commands": [x for x in os.environ.get("FAKE_CLAUDE_COMMANDS", "").split(",") if x]}), flush=True)
for line in sys.stdin:
    try: msg = json.loads(line)
    except ValueError: continue
    content = msg.get("message", {}).get("content", "")
    log.write("PROMPT " + json.dumps(content) + "\n"); log.flush()
    if "mute" in json.dumps(content):  # nothing answered for a while (sending): the session works, no message yet
        time.sleep(float(os.environ.get("FAKE_CLAUDE_DELAY", "0")))
    elif "slow" in json.dumps(content):  # a long turn: a first message at once (the session is working), the end later
        print(json.dumps({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": "thinking"}], "usage": {"input_tokens": 1, "output_tokens": 1}}}), flush=True)
        time.sleep(float(os.environ.get("FAKE_CLAUDE_DELAY", "0")))
    print(json.dumps({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": os.environ.get("FAKE_CLAUDE_REPLY", "ok")}], "usage": {"input_tokens": 1, "output_tokens": 1}}}), flush=True)
    print(json.dumps({"type": "result", "subtype": "success", "is_error": False, "result": "ok", "session_id": "00000000-0000-0000-0000-000000000001", "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1, "usage": {"input_tokens": 1, "output_tokens": 1}}), flush=True)
