#!/usr/bin/env python3
# a stand-in for `claude` (stream-json): answers every prompt with text around a fenced code block (indentation, a tab, a blank line, spaces
# at the end of a line, a line longer than the box)
import json, sys
ANSWER = "Before the block.\n\n```python\ndef f(x):\n    y = x + 1   \n\n\treturn y\nlong = \"" + "abcdefghij" * 20 + "\"\n```\n\nAfter the block.\n"
print(json.dumps({"type": "system", "subtype": "init", "session_id": "00000000-0000-0000-0000-000000000002", "model": "claude-sonnet-5-5", "tools": [], "cwd": ".",
    "skills": [], "slash_commands": []}), flush=True)
for line in sys.stdin:
    try: msg = json.loads(line)
    except ValueError: continue
    print(json.dumps({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": ANSWER}], "usage": {"input_tokens": 1, "output_tokens": 1}}}), flush=True)
    print(json.dumps({"type": "result", "subtype": "success", "is_error": False, "result": ANSWER, "session_id": "00000000-0000-0000-0000-000000000002", "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1, "usage": {"input_tokens": 1, "output_tokens": 1}}), flush=True)
