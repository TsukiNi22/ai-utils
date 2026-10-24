#!/usr/bin/env python3
# a stand-in for `claude` (stream-json) whose turn asks the user a question (AskUserQuestion: a can_use_tool control request) and waits for
# its answer; an interrupt ends the turn; every message received is logged
import json, os, select, sys
log = open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8")
SID = "00000000-0000-0000-0000-000000000003"


def out(event):
    print(json.dumps(event), flush=True)


def end(text):
    out({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": text}], "usage": {"input_tokens": 1, "output_tokens": 1}}})
    out({"type": "result", "subtype": "success", "is_error": False, "result": text, "session_id": SID, "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1,
         "usage": {"input_tokens": 1, "output_tokens": 1}})


out({"type": "system", "subtype": "init", "session_id": SID, "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd()})
for line in sys.stdin:
    try:
        msg = json.loads(line)
    except ValueError:
        continue
    if msg.get("type") == "control_request":
        log.write("CONTROL " + json.dumps(msg.get("request", {})) + "\n")
        log.flush()
        out({"type": "control_response", "response": {"subtype": "success", "request_id": msg.get("request_id"), "response": {}}})
        continue
    content = json.dumps(msg.get("message", {}).get("content", ""))
    log.write("PROMPT " + content + "\n")
    log.flush()
    if "ask me" not in content:
        end("ok")
        continue
    out({"type": "control_request", "request_id": "ask-1", "request": {"subtype": "can_use_tool", "tool_name": "AskUserQuestion",
         "input": {"questions": [{"question": "Which colour for the logo?", "header": "Colour", "multiSelect": False,
                                  "options": [{"label": "red", "description": "warm"}, {"label": "blue", "description": "calm"}]}]}}})
    while True:  # its answer, or an interrupt
        if not select.select([sys.stdin], [], [], 60)[0]:
            break
        other = sys.stdin.readline()
        if not other:
            sys.exit(0)
        log.write("PROMPT " + other.strip() + "\n")
        log.flush()
        reply = json.loads(other)
        if reply.get("type") == "control_request" and reply.get("request", {}).get("subtype") == "interrupt":
            out({"type": "control_response", "response": {"subtype": "success", "request_id": reply.get("request_id"), "response": {}}})
            end("interrupted")
            break
        if reply.get("type") == "control_response":
            end("answered")
            break
