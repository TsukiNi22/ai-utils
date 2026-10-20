#!/usr/bin/env python3
# a stand-in for `claude` (stream-json) that also wakes up by itself: a prompt holding "monitor" ends its turn, then one second
# later a second turn starts WITHOUT any prompt (a Monitor / background task notification); a prompt holding "fail" ends in an API error
import json, os, sys, threading, time
log = open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8")
lock = threading.Lock()
def out(event: dict) -> None:
    with lock: print(json.dumps(event), flush=True)
def turn(text: str, error: bool = False) -> None:
    out({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": text}], "usage": {"input_tokens": 1, "output_tokens": 1}}})
    out({"type": "result", "subtype": "success", "is_error": error, "result": text, "session_id": "00000000-0000-0000-0000-000000000001", "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1, "usage": {"input_tokens": 1, "output_tokens": 1}})
def wake() -> None:
    time.sleep(1)
    out({"type": "system", "subtype": "task_notification", "task_id": "bg1", "status": "completed"})
    turn("woken by the monitor")
out({"type": "system", "subtype": "init", "session_id": "00000000-0000-0000-0000-000000000001", "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd()})
for line in sys.stdin:
    try: msg = json.loads(line)
    except ValueError: continue
    if msg.get("type") != "user": continue  # control requests of claude-cluster: no turn
    content = json.dumps(msg.get("message", {}).get("content", ""))
    with lock: log.write("PROMPT " + content + "\n"); log.flush()
    if "reply from" in content or "error from" in content or "[tasks]" in content: turn("noted"); continue  # the global session answers its callbacks
    if "fail" in content: turn("You have hit your weekly limit", True); continue
    turn("ok " + ("monitor armed" if "monitor" in content else "done"))
    if "monitor" in content: threading.Thread(target=wake, daemon=True).start()
