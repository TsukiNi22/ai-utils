"""The Claude Code hook (PostToolUse on the MCP tools): sizes learned, the reminder and the compressor.

`codemode.py hook [remind] [compress]` reads the hook event on stdin and answers on stdout:
- always: the size of the direct result is recorded (search shows it next to the tool) and counted for the stats;
- remind: at the Nth direct call in a row on one server, or after one big result, one line tells to use codemode;
- compress: a direct result over the threshold is replaced by a preview, its shape and the path of the full text.
It costs nothing in the context while it has nothing to say.
"""
from __future__ import annotations

import json
import sys
import time

from . import audit, sizes
from .paths import compact, est_tokens, load_config, read_json, safe_name, shape, sub, tokens_label, write_json

OWN = ("codemode", "plugin_code-mode-skills_codemode")  # the servers that are codemode itself

def split(name: str) -> tuple[str, str]:
    """mcp__<server>__<tool> -> (server, tool)."""
    server, _, tool = name[len("mcp__"):].partition("__")
    return server, tool

def response_text(response: object) -> tuple[str, bool]:
    """The text of a tool result as the model reads it, and whether it also holds media (never cut then)."""
    if isinstance(response, str):
        return response, False
    blocks = response.get("content") if isinstance(response, dict) and isinstance(response.get("content"), list) else response
    if isinstance(blocks, list) and all(isinstance(block, dict) for block in blocks):
        media = any(block.get("type") not in ("text", None) for block in blocks)
        return "\n".join(str(block.get("text") or "") for block in blocks if block.get("type") in ("text", None)), media
    if isinstance(response, dict) and list(response) == ["result"] and isinstance(response["result"], str):
        return response["result"], False
    return compact(response), False

def replaced(response: object, preview: str) -> object:
    """The preview in the same shape as the result it replaces."""
    if isinstance(response, list):
        return [{"type": "text", "text": preview}]
    if isinstance(response, dict) and isinstance(response.get("content"), list):
        kept = {key: value for key, value in response.items() if key != "structuredContent"}
        return {**kept, "content": [{"type": "text", "text": preview}]}
    if isinstance(response, dict) and list(response) == ["result"]:
        return {"result": preview}
    return preview

def preview_of(text: str, config: dict, path: str) -> str:
    """The head of a long result, its shape when it is JSON, and where the whole of it is."""
    head = text[:int(config["preview_chars"])]
    form = ""
    try:
        form = f" Shape: {compact(shape(json.loads(text)))[:500]}."
    except ValueError:
        pass  # not JSON: the head is all there is to show
    return (f"{head}\n[codemode: result of {tokens_label(len(text))} cut to this preview.{form} Full text: {path} - read only the part "
            f"you need (Read with offset / limit, or grep), or get it through codemode `execute` / `call` with pick and print what matters.]")

def run(flags: list[str]) -> int:
    """Handle one hook event (JSON on stdin); print the answer of the hook when there is one."""
    try:
        event = json.load(sys.stdin)
    except ValueError:
        return 0
    name = str(event.get("tool_name") or "")
    if event.get("hook_event_name") != "PostToolUse" or not name.startswith("mcp__"):
        return 0
    config = load_config()
    session = str(event.get("session_id") or "default")
    server, tool = split(name)
    state_path = sub("run") / f"hook-{safe_name(session)}.json"
    state = read_json(state_path, {})
    state = state if isinstance(state, dict) else {}
    streak = state.setdefault("streak", {})
    told = state.setdefault("told", {})
    if server in OWN or server.endswith("_codemode"):
        if streak:
            state["streak"] = {}
            write_json(state_path, state)
        return 0
    response = event.get("tool_response")
    text, media = response_text(response)
    failed = isinstance(response, dict) and bool(response.get("isError"))
    sizes.record(f"{server}.{tool}", len(text))
    output: dict = {}
    notes: list[str] = []
    cut = 0
    tokens = est_tokens(len(text))
    if "compress" in flags and not media and not failed and tokens > int(config["compress_over_tokens"]):
        path = sub("files", safe_name(session)) / f"direct-{int(time.time() * 1000)}-{safe_name(tool, 30)}.txt"
        with open(path, "w", encoding="utf-8") as file:
            file.write(text)
        preview = preview_of(text, config, str(path))
        output["updatedToolOutput"] = replaced(response, preview)
        cut = len(text) - len(preview)
    entry = streak.setdefault(server, {"n": 0, "chars": 0})
    entry["n"] += 1
    entry["chars"] += len(text) - cut
    if "remind" in flags:
        after = int(config["remind_after"])
        if entry["n"] in (after, after * 4) and told.get(server, 0) < entry["n"]:
            told[server] = entry["n"]
            notes.append(f"[codemode] {entry['n']} direct calls to {server} in a row ({tokens_label(entry['chars'])} of results in the context). "
                         f"For a loop, a chain of calls or results to filter, run one script instead: codemode `execute` "
                         f"(tool mcp__codemode__execute, or the code-mode skill command line); only what it prints comes back.")
        elif not cut and tokens > int(config["big_result_tokens"]) and not told.get(f"big:{name}"):
            told[f"big:{name}"] = 1
            notes.append(f"[codemode] {server}.{tool} returned {tokens_label(len(text))}. Next time get only what matters: codemode `call` "
                         f"with pick, or `execute` and print the fields you need.")
    write_json(state_path, state)
    audit.log({"op": "direct", "session": session, "server": server, "tool": tool, "chars": len(text), "cut": cut}, config)
    if notes:
        output["additionalContext"] = " ".join(notes)
    if output:
        sys.stdout.write(json.dumps({"hookSpecificOutput": {"hookEventName": "PostToolUse", **output}}, ensure_ascii=False) + "\n")
    return 0
