#!/usr/bin/env python3
# a stand-in for `claude` (stream-json) whose turn makes two sub-agents: one ends with the turn, one goes on in the background
# until a control_request stop_task stops it (its task events, its own messages with parent_tool_use_id, its model)
import json, os, sys, threading, time
log = open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8")
SID = "00000000-0000-0000-0000-000000000002"
BG_DESC = os.environ.get("FAKE_BG_DESC", "Watch the tests")  # the background agent (a dangerous one: deploy, push...)


def out(event):
    print(json.dumps(event), flush=True)


def assistant(content, parent=None, model="claude-sonnet-5-5"):
    out({"type": "assistant", "parent_tool_use_id": parent, "message": {"role": "assistant", "model": model, "content": content, "usage": {"input_tokens": 1, "output_tokens": 1}}})


def result_of(call, text, parent=None):
    out({"type": "user", "parent_tool_use_id": parent, "message": {"role": "user", "content": [{"type": "tool_result", "tool_use_id": call, "content": text}]}})


out({"type": "system", "subtype": "init", "session_id": SID, "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd()})
for line in sys.stdin:
    try:
        msg = json.loads(line)
    except ValueError:
        continue
    if msg.get("type") == "control_request":
        request = msg.get("request", {})
        log.write("CONTROL " + json.dumps(request) + "\n")
        log.flush()
        if request.get("subtype") == "stop_task":
            out({"type": "control_response", "response": {"subtype": "success", "request_id": msg.get("request_id"), "response": {}}})
            out({"type": "system", "subtype": "task_notification", "task_id": request.get("task_id"), "tool_use_id": "call-bg", "status": "killed", "summary": "stopped by the user"})
        continue
    content = msg.get("message", {}).get("content", "")
    log.write("PROMPT " + json.dumps(content) + "\n")
    log.flush()
    if "task-notification" in json.dumps(content):  # woken by the end of its shell: one short answer
        assistant([{"type": "text", "text": "the tests of the shell passed"}])
        out({"type": "result", "subtype": "success", "is_error": False, "result": "ok", "session_id": SID, "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1,
             "usage": {"input_tokens": 1, "output_tokens": 1}})
        continue
    # the session asks two agents: a reviewer (in the foreground) and a watcher (in the background)
    assistant([{"type": "text", "text": "I ask two agents."},
               {"type": "tool_use", "id": "call-fg", "name": "Agent", "input": {"description": "Review the diff", "subagent_type": "code-reviewer", "prompt": "Review it"}},
               {"type": "tool_use", "id": "call-bg", "name": "Agent", "input": {"description": BG_DESC, "subagent_type": "general-purpose", "prompt": "Watch",
                                                                                   "run_in_background": True}}])
    out({"type": "system", "subtype": "task_started", "task_id": "task-fg", "tool_use_id": "call-fg", "description": "Review the diff", "subagent_type": "code-reviewer",
         "task_type": "local_agent", "is_backgrounded": False, "prompt": "Review it"})
    out({"type": "system", "subtype": "task_started", "task_id": "task-bg", "tool_use_id": "call-bg", "description": BG_DESC, "subagent_type": "general-purpose",
         "task_type": "local_agent", "is_backgrounded": True, "prompt": "Watch"})
    out({"type": "system", "subtype": "task_started", "task_id": "task-sh", "tool_use_id": "call-sh", "description": "npm test", "task_type": "local_bash"})  # a shell in the background
    out({"type": "user", "parent_tool_use_id": "call-fg", "message": {"role": "user", "content": "Review it"}})
    assistant([{"type": "text", "text": "SUBAGENT-SECRET-TEXT reading the diff"}, {"type": "tool_use", "id": "sub-1", "name": "Read", "input": {"file_path": "/tmp/a.cpp"}}],
              parent="call-fg", model="claude-haiku-5-5")
    result_of("sub-1", "int main() {}", parent="call-fg")
    assistant([{"type": "tool_use", "id": "sub-2", "name": "Bash", "input": {"command": "ctest"}}], parent="call-bg", model="claude-opus-5-5")
    out({"type": "system", "subtype": "task_progress", "task_id": "task-fg", "tool_use_id": "call-fg", "description": "Review the diff", "subagent_type": "code-reviewer",
         "usage": {"total_tokens": 1234, "tool_uses": 1, "duration_ms": 900}, "last_tool_name": "Read", "summary": "reading the diff"})
    out({"type": "system", "subtype": "task_notification", "task_id": "task-fg", "tool_use_id": "call-fg", "status": "completed", "summary": "no issue found",
         "usage": {"total_tokens": 2000, "tool_uses": 1, "duration_ms": 1500}})
    result_of("call-fg", "no issue found")
    result_of("call-bg", "Async agent launched: it runs in the background")
    assistant([{"type": "text", "text": os.environ.get("FAKE_CLAUDE_REPLY", "the review is done, the watcher goes on")}])
    out({"type": "result", "subtype": "success", "is_error": False, "result": "ok", "session_id": SID, "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1,
         "usage": {"input_tokens": 1, "output_tokens": 1}})

    def shell_ends():  # the shell ends after the turn: its output written (line by line, FAKE_SHELL_SECONDS), its notification (the session idle: woken by claude-cluster)
        output = os.path.join(os.environ.get("FAKE_CLAUDE_OUT", "/tmp"), "task-sh.output")
        seconds = float(os.environ.get("FAKE_SHELL_SECONDS", "1.0"))
        with open(output, "w", encoding="utf-8") as file:
            for i in range(int(seconds * 2)):
                time.sleep(0.5)
                file.write(f"SHELL-LINE-{i}\n")
                file.flush()
            file.write("PASS 12 tests\nSHELL-OUTPUT-END\n")
        out({"type": "system", "subtype": "task_notification", "task_id": "task-sh", "tool_use_id": "call-sh", "status": "completed", "summary": "exit code 0",
             "output_file": output})
    threading.Thread(target=shell_ends, daemon=True).start()
