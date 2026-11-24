"""The MCP entry: codemode as a stdio MCP server with four tools (search, call, execute, job).

The rules of use travel in the `instructions` of the server and in the tool descriptions, so a client without skills
(Claude Desktop) gets them too. No output schema: results come back as plain text, not wrapped and escaped again.
"""
from __future__ import annotations

import json
import sys
import threading
from concurrent.futures import ThreadPoolExecutor

from .engine import Engine
from .paths import PROTOCOL, PROTOCOLS, VERSION

INSTRUCTIONS = """codemode drives the user's local MCP servers through code: only what your code prints enters the context.
Choose by token cost:
- 1-2 calls with a small result you need as it is: `call` (or the server's own tool when it is loaded here).
- 3 calls or more, a loop over items, a chain where a result feeds the next call, or a result to filter / count / aggregate (search shows `~Nk tok` on tools measured to return a lot): `execute` with one Python script.
- An image you must look at: `call` (execute only gives file paths).
Flow: `search` (one line per tool; detail=true for the arguments of the few tools you will use), then `execute`. In the code: call(server, tool, **args) returns parsed data; pmap(server, tool, [args, ...], key="name") runs in parallel and puts a ToolError in place of each failed item; variables persist between executes, so look at the shape of one result first, then process everything. Print only what the answer needs: counts, a short table, ids, errors.
Failures: a usage limit is waited and tried again by itself; when it lasts, the batch stops, what already succeeded is kept and the footer lists what failed or was not sent. Never redo what succeeded: run the same pmap with the same key, it only sends the missing items.
Writes: a tool without [ro] needs allow_write=true; set it only when the user's request asks for a change.
An execute that outlasts its wait goes on as a job: `job` returns what it printed since."""

TOOLS = [
    {"name": "search",
     "description": "Find tools of the local MCP servers (and saved recipes). Empty query: the servers and what they hold. query: words "
                    "or a regex on `server.tool(args) - summary`; detail=true adds the arguments of up to 5 tools. [ro] = read only, "
                    "`~N tok` = the measured size of its results.",
     "inputSchema": {"type": "object", "properties": {
         "query": {"type": "string", "description": "Words or a regex; empty lists the servers"},
         "server": {"type": "string", "description": "Only this server"},
         "detail": {"type": "boolean", "description": "Arguments and full description (5 tools at most)"}}},
     "annotations": {"readOnlyHint": True, "title": "Search tools"}},
    {"name": "call",
     "description": "Call one tool of a local MCP server and get its result compact. pick keeps only some fields "
                    "(`format.duration,streams[].codec_name`). A long result goes to a file: its head and path come back.",
     "inputSchema": {"type": "object", "required": ["server", "tool"], "properties": {
         "server": {"type": "string"}, "tool": {"type": "string"},
         "args": {"type": "object", "description": "The arguments of the tool"},
         "pick": {"type": "string", "description": "Fields to keep, comma separated paths"},
         "allow_write": {"type": "boolean", "description": "Needed by a tool without [ro]"}}},
     "annotations": {"title": "Call one tool"}},
    {"name": "execute",
     "description": "Run Python against the local MCP servers; only what it prints comes back. Helpers: call(server, tool, **args), "
                    "pmap(server, tool, items, key=None), ToolError, ok(results), failed(results), args. Variables persist between "
                    "calls (fresh=true: a clean separate run). recipe= runs a saved script, save_as= keeps this one once it succeeded.",
     "inputSchema": {"type": "object", "properties": {
         "code": {"type": "string", "description": "Python source"},
         "recipe": {"type": "string", "description": "Name of a saved recipe to run instead of code"},
         "args": {"type": "object", "description": "Values the code reads as args[...]"},
         "allow_write": {"type": "boolean", "description": "Let the code call tools without [ro]"},
         "fresh": {"type": "boolean", "description": "Separate process with an empty namespace"},
         "wait": {"type": "integer", "description": "Seconds to wait before it goes on as a job (default 40)"},
         "save_as": {"type": "string", "description": "Keep the code as a recipe under this name"},
         "description": {"type": "string", "description": "What the recipe does (with save_as)"}}},
     "annotations": {"title": "Run code on the servers"}},
    {"name": "job",
     "description": "Follow a script still running in the background: what it printed since the last look, wait for its end, kill it, "
                    "or list the jobs.",
     "inputSchema": {"type": "object", "properties": {
         "id": {"type": "string", "description": "The job id; empty lists the jobs"},
         "action": {"type": "string", "enum": ["status", "wait", "kill", "list"]},
         "wait": {"type": "integer", "description": "Seconds to wait for its end"}}},
     "annotations": {"title": "Follow a job"}},
]
EMPTY = {"resources/list": {"resources": []}, "resources/templates/list": {"resourceTemplates": []}, "prompts/list": {"prompts": []}}

class Server:
    """JSON-RPC over stdin / stdout: one message per line."""

    def __init__(self, engine: Engine) -> None:
        self.engine = engine
        self._in = sys.stdin.buffer
        self._out = sys.stdout.buffer
        self._lock = threading.Lock()
        self._pool = ThreadPoolExecutor(max_workers=8, thread_name_prefix="tool")
        sys.stdout = sys.stderr  # nothing but the protocol may reach the real stdout

    def send(self, message: dict) -> None:
        data = json.dumps(message, ensure_ascii=False, default=str).encode("utf-8") + b"\n"
        with self._lock:
            try:
                self._out.write(data)
                self._out.flush()
            except (OSError, ValueError):
                pass  # the client left: the loop ends at its next read

    def serve(self) -> int:
        while True:
            line = self._in.readline()
            if not line:
                break
            if not line.strip():
                continue
            try:
                message = json.loads(line.decode("utf-8", "replace"))
            except ValueError:
                self.send({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "not JSON"}})
                continue
            for item in message if isinstance(message, list) else [message]:
                if isinstance(item, dict):
                    self.dispatch(item)
        self._pool.shutdown(wait=False)
        self.engine.close()
        return 0

    def dispatch(self, message: dict) -> None:
        method, ident = message.get("method"), message.get("id")
        if method is None or ident is None:
            return  # an answer to nothing we asked, or a notification: nothing to reply
        params = message.get("params") if isinstance(message.get("params"), dict) else {}
        if method == "initialize":
            asked = str(params.get("protocolVersion") or "")
            result: object = {"protocolVersion": asked if asked in PROTOCOLS else PROTOCOL, "capabilities": {"tools": {"listChanged": False}},
                              "serverInfo": {"name": "codemode", "title": "Code mode", "version": VERSION}, "instructions": INSTRUCTIONS}
        elif method == "ping":
            result = {}
        elif method == "tools/list":
            result = {"tools": TOOLS}
        elif method == "tools/call":
            self._pool.submit(self.run_tool, ident, params)
            return
        elif method in EMPTY:
            result = EMPTY[method]
        else:
            self.send({"jsonrpc": "2.0", "id": ident, "error": {"code": -32601, "message": f"codemode has no method {method}"}})
            return
        self.send({"jsonrpc": "2.0", "id": ident, "result": result})

    def run_tool(self, ident: object, params: dict) -> None:
        """One tools/call, in its own thread: a long execute never blocks the other requests."""
        name = str(params.get("name") or "")
        arguments = params.get("arguments") if isinstance(params.get("arguments"), dict) else {}
        try:
            if name not in ("search", "call", "execute", "job"):
                result = {"text": f"codemode has no tool {name} (search, call, execute, job)", "is_error": True, "parts": []}
            else:
                result = self.engine.handle(name, arguments)
        except Exception as error:  # a bug of codemode must come back as an error, not kill the server
            result = {"text": f"codemode internal error: {type(error).__name__}: {error}", "is_error": True, "parts": []}
        content = [{"type": "text", "text": result["text"]}] + [part for part in result.get("parts") or [] if part.get("type") == "image"]
        self.send({"jsonrpc": "2.0", "id": ident, "result": {"content": content, "isError": bool(result["is_error"])}})
