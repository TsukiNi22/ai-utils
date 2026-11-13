#!/usr/bin/env python3
# a stand-in for `claude` (stream-json) that answers the user's messages only, never a control line, and logs the folder it runs in with each prompt: which session got it
import json, os, sys, time
log = open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8")
sid = "00000000-0000-0000-0000-000000000001"
print(json.dumps({"type": "system", "subtype": "init", "session_id": sid, "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd(), "skills": [], "slash_commands": []}), flush=True)
for line in sys.stdin:
    try:
        msg = json.loads(line)
    except ValueError:
        continue
    if msg.get("type") == "control_request":
        print(json.dumps({"type": "control_response", "response": {"subtype": "success", "request_id": msg.get("request_id"), "response": {}}}), flush=True)
        continue
    if msg.get("type") != "user":
        continue
    text = json.dumps(msg.get("message", {}).get("content", ""))
    log.write("PROMPT " + os.getcwd() + " " + text + "\n")
    log.flush()
    if "slow" in text:  # a long turn: a first message at once (the session is working), the end later
        print(json.dumps({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": "thinking"}], "usage": {"input_tokens": 1, "output_tokens": 1}}}), flush=True)
        time.sleep(float(os.environ.get("FAKE_CLAUDE_DELAY", "0")))
    reply = os.environ.get("FAKE_CLAUDE_REPLY", "ok")
    print(json.dumps({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": reply}], "usage": {"input_tokens": 1, "output_tokens": 1}}}), flush=True)
    print(json.dumps({"type": "result", "subtype": "success", "is_error": False, "result": reply, "session_id": sid, "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1, "usage": {"input_tokens": 1, "output_tokens": 1}}), flush=True)
