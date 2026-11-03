#!/usr/bin/env python3
# A stand-in for claude with the times of the real one: `claude agents --json` takes FAKE_AGENTS_S seconds (a node start) and prints FAKE_AGENTS_OUT (default: []);
# on SIGTERM its last line (FAKE_LINE_ON_TERM) goes out before it ends. A conversation of its own unless resumed
import json
import os
import signal
import sys
import time
import uuid

log = open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8")
if len(sys.argv) > 1 and sys.argv[1] in ("agents", "auth", "stop") or "--no-session-persistence" in sys.argv:
    log.write("TOOL " + " ".join(sys.argv[1:3]) + "\n")
    log.flush()
    if sys.argv[1] == "agents":
        time.sleep(float(os.environ.get("FAKE_AGENTS_S", "1.5")))
        print(os.environ.get("FAKE_AGENTS_OUT", "[]"))
    sys.exit(0)
session_id = sys.argv[sys.argv.index("--resume") + 1] if "--resume" in sys.argv else str(uuid.uuid4())
is_global = "--mcp-config" in sys.argv and "# claude-cluster - sub-session" not in json.dumps(sys.argv)
log.write(f"START {session_id} resumed={'--resume' in sys.argv} global={is_global}\n")
log.flush()
init = {"type": "system", "subtype": "init", "session_id": session_id, "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd()}
if os.environ.get("FAKE_LINE_ON_TERM"):
    def term(*_: object) -> None:
        print(json.dumps(init), flush=True)
        os._exit(0)
    signal.signal(signal.SIGTERM, term)
print(json.dumps(init), flush=True)
for line in sys.stdin:
    try:
        msg = json.loads(line)
    except ValueError:
        continue
    if msg.get("type") != "user":
        continue
    log.write("PROMPT " + json.dumps(msg.get("message", {}).get("content", "")) + "\n")
    log.flush()
    print(json.dumps({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": "ok"}], "usage": {"input_tokens": 1, "output_tokens": 1}}}), flush=True)
    print(json.dumps({"type": "result", "subtype": "success", "is_error": False, "result": "ok", "session_id": session_id, "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1,
                      "usage": {"input_tokens": 1, "output_tokens": 1}}), flush=True)
