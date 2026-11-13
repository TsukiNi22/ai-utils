#!/usr/bin/env python3
# A stand-in for `codex exec --json <prompt>`: one process a turn
import json
import os
import sys

with open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8") as log:
    log.write("PROMPT " + json.dumps(sys.argv[-1]) + "\n")
print(json.dumps({"type": "thread.started", "thread_id": "thread-1"}), flush=True)
print(json.dumps({"type": "item.completed", "item": {"id": "1", "type": "agent_message", "text": "ok"}}), flush=True)
print(json.dumps({"type": "turn.completed", "usage": {"input_tokens": 1, "output_tokens": 1}}), flush=True)
