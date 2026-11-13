#!/usr/bin/env python3
# A stand-in for claude whose result tells a huge cost (two of them sum to infinity)
import json
import os
import sys

SESSION = "00000000-0000-0000-0000-000000000001"
print(json.dumps({"type": "system", "subtype": "init", "session_id": SESSION, "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd(), "skills": [], "slash_commands": []}), flush=True)
for line in sys.stdin:
    try:
        msg = json.loads(line)
    except ValueError:
        continue
    if msg.get("type") != "user":
        continue
    print(json.dumps({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": "ok"}], "usage": {"input_tokens": 1, "output_tokens": 1}}}), flush=True)
    print(json.dumps({"type": "result", "subtype": "success", "is_error": False, "result": "ok", "session_id": SESSION, "total_cost_usd": 1.7e308, "duration_ms": 1, "num_turns": 1,
                      "usage": {"input_tokens": 1, "output_tokens": 1}}), flush=True)
