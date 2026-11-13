#!/usr/bin/env python3
# A fake `claude` whose background ends as the prompt asks: its notification after the turn, only a task_updated, for a task never seen starting, during a turn, the agent gone.
import json
import os
import sys
import threading
import time
from collections.abc import Callable

log = open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8")
write_lock = threading.Lock()
SID = "00000000-0000-0000-0000-00000000000b"

def out(event: dict) -> None:
    with write_lock:
        print(json.dumps(event), flush=True)

def answer(text: str) -> None:
    out({"type": "assistant", "parent_tool_use_id": None, "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": text}], "usage": {"input_tokens": 1, "output_tokens": 1}}})
    out({"type": "result", "subtype": "success", "result": text, "is_error": False, "session_id": SID, "total_cost_usd": 0.0, "usage": {"input_tokens": 1, "output_tokens": 1}})

def shell(task: str) -> None:
    out({"type": "system", "subtype": "task_started", "task_id": task, "tool_use_id": "call-" + task, "description": "the command " + task, "task_type": "local_bash"})

def later(seconds: float, what: Callable[[], None]) -> None:
    def run() -> None:
        time.sleep(seconds)
        what()
    threading.Thread(target=run, daemon=True).start()

def told(task: str, status: str, kind: str = "local_bash", summary: str = "exit 0") -> None:
    out({"type": "system", "subtype": "task_notification", "task_id": task, "tool_use_id": "call-" + task, "status": status, "summary": summary, "task_type": kind})

out({"type": "system", "subtype": "init", "session_id": SID, "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd()})
for line in sys.stdin:
    try:
        msg = json.loads(line)
    except ValueError:
        continue
    if msg.get("type") != "user":
        continue
    content = msg.get("message", {}).get("content", "")
    text = content if isinstance(content, str) else " ".join(block.get("text", "") for block in content if isinstance(block, dict))
    log.write("PROMPT " + json.dumps(text) + "\n")
    log.flush()
    if "<task-notification>" in text:
        answer("WOKEN " + text[text.find("<summary>") + 9:text.find("</summary>")])
    elif "[reply from" in text or "[stopped:" in text or "[status from" in text or "keep-warm" in text:
        answer("ok")
    elif "BG-ENDS" in text:
        shell("t-ends")
        answer("started t-ends")
        later(1.0, lambda: told("t-ends", "completed"))
    elif "BG-KILLED" in text:
        shell("t-killed")
        answer("started t-killed")
        later(1.0, lambda: told("t-killed", "killed", summary=""))
    elif "BG-UPDATED" in text:
        shell("t-updated")
        answer("started t-updated")
        later(1.0, lambda: out({"type": "system", "subtype": "task_updated", "task_id": "t-updated", "patch": {"status": "completed"}}))
    elif "BG-UNSEEN" in text:
        answer("nothing started that it knows of")
        later(1.0, lambda: told("t-unseen", "completed", "monitor", "a monitor fired"))
    elif "BG-DURING" in text:
        shell("t-during")
        answer("started t-during")
    elif "ENDS-IN-TURN" in text:
        told("t-during", "completed")
        time.sleep(1.0)
        answer("RESULT-IN-THE-USER-TURN")
    elif "BG-TAIL" in text:
        output = os.environ["FAKE_T"] + "/tail.output"
        with open(output, "w", encoding="utf-8") as file:
            file.write("a line\nTHE-END-OF-ITS-OUTPUT\n")
        out({"type": "system", "subtype": "task_started", "task_id": "t-tail", "tool_use_id": "call-t-tail", "description": "the command t-tail", "task_type": "local_bash", "output_file": output})
        answer("started t-tail")
        later(1.0, lambda: told("t-tail", "completed", summary=""))
    elif "BG-ODD-FILES" in text:
        fifo = os.environ["FAKE_T"] + "/a.fifo"
        huge = os.environ["FAKE_T"] + "/huge.output"
        os.mkfifo(fifo)
        with open(huge, "wb") as file:
            file.truncate(1 << 36)  # 64 GB, a hole
            file.seek(-5, os.SEEK_END)
            file.write(b"LAST\n")
        out({"type": "system", "subtype": "task_started", "task_id": "t-fifo", "tool_use_id": "call-t-fifo", "description": "the command t-fifo", "task_type": "local_bash", "output_file": fifo})
        out({"type": "system", "subtype": "task_started", "task_id": "t-huge", "tool_use_id": "call-t-huge", "description": "the command t-huge", "task_type": "local_bash", "output_file": huge})
        answer("started both")
    elif "BG-EXIT" in text:
        shell("t-exit")
        answer("started t-exit")
        later(1.0, lambda: os._exit(3))
    elif "SLOW" in text:
        time.sleep(4)
        answer("ANSWER-OF-THE-USER")
    else:
        answer("ANSWER-OF " + text[-30:])
