#!/usr/bin/env python3
"""A fake MCP server for the tests of codemode (stdio by default, HTTP with --http PORT, event stream with --sse).

Tools: echo, big (a result of N kB), slow (sleeps), flaky (a usage limit the first N times of a key), limited (works N
times then a lasting usage limit), reset, image (a PNG), write_thing / get_things (state), crash (the process exits),
show_env (an environment variable of the server). Standard library only.
"""
from __future__ import annotations

import json
import os
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PNG = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg=="
STATE: dict = {"things": [], "flaky": {}, "limited": 0}

def tool(name: str, description: str, properties: dict, required: list[str], read_only: bool) -> dict:
    return {"name": name, "description": description, "inputSchema": {"type": "object", "properties": properties, "required": required},
            "annotations": {"readOnlyHint": read_only}}

TOOLS = [
    tool("echo", "Return the text given. Second sentence never shown.", {"text": {"type": "string", "description": "What to return"}}, ["text"], True),
    tool("big", "Return a JSON list of about N kilobytes.", {"kb": {"type": "integer", "default": 20}}, [], True),
    tool("slow", "Sleep then answer.", {"seconds": {"type": "number"}}, ["seconds"], True),
    tool("flaky", "Fail with a usage limit the first N times of a key.", {"key": {"type": "string"}, "fails": {"type": "integer"}}, ["key"], True),
    tool("limited", "Work N times, then answer a lasting usage limit.", {"after": {"type": "integer"}}, ["after"], True),
    tool("reset", "Forget the counters of flaky and limited.", {}, [], True),
    tool("image", "Return a tiny PNG and a text.", {}, [], True),
    tool("write_thing", "Remember a name.", {"name": {"type": "string"}}, ["name"], False),
    tool("get_things", "List the remembered names.", {}, [], True),
    tool("crash", "Exit the server process.", {}, [], True),
    tool("show_env", "Return an environment variable of the server.", {"name": {"type": "string"}}, ["name"], True),
    tool("execute_fake_code", "Pretend to run a script inside an application.", {"code": {"type": "string"}}, ["code"], False),
]

def text(value: str, error: bool = False) -> dict:
    return {"content": [{"type": "text", "text": value}], "isError": error}

def call(name: str, args: dict) -> dict:
    if name == "echo":
        return text(str(args.get("text", "")))
    if name == "big":
        size = int(args.get("kb", 20)) * 1000
        return text(json.dumps([{"id": number, "name": f"item-{number}", "payload": "x" * 60} for number in range(size // 100)]))
    if name == "slow":
        time.sleep(float(args.get("seconds", 1)))
        return text(json.dumps({"slept": args.get("seconds")}))
    if name == "flaky":
        seen = STATE["flaky"][args["key"]] = STATE["flaky"].get(args["key"], 0) + 1
        if seen <= int(args.get("fails", 1)):
            return text("rate limit exceeded, retry after 1s", True)
        return text(json.dumps({"key": args["key"], "tries": seen}))
    if name == "limited":
        STATE["limited"] += 1
        if STATE["limited"] > int(args.get("after", 3)):
            return text("429 Too Many Requests: usage limit reached, try again in 120 seconds", True)
        return text(json.dumps({"n": STATE["limited"]}))
    if name == "reset":
        STATE.update({"flaky": {}, "limited": 0})
        return text("reset")
    if name == "image":
        return {"content": [{"type": "text", "text": "a pixel"}, {"type": "image", "data": PNG, "mimeType": "image/png"}], "isError": False}
    if name == "write_thing":
        STATE["things"].append(str(args.get("name")))
        return text(json.dumps({"written": args.get("name"), "count": len(STATE["things"])}))
    if name == "get_things":
        return text(json.dumps(STATE["things"]))
    if name == "crash":
        os._exit(3)
    if name == "show_env":
        return text(json.dumps({"value": os.environ.get(str(args.get("name")), "")}))
    if name == "execute_fake_code":
        return text(f"ran {len(str(args.get('code', '')))} characters")
    return text(f"unknown tool {name}", True)

def handle(message: dict) -> dict | None:
    """The answer to one JSON-RPC message (nothing for a notification)."""
    method, ident = message.get("method"), message.get("id")
    if ident is None:
        return None
    params = message.get("params") or {}
    if method == "initialize":
        result: object = {"protocolVersion": params.get("protocolVersion", "2025-06-18"), "capabilities": {"tools": {}},
                          "serverInfo": {"name": "fake", "version": "1"}, "instructions": "Fake server for the tests."}
    elif method == "ping":
        result = {}
    elif method == "tools/list":
        result = {"tools": TOOLS}
    elif method == "tools/call":
        result = call(str(params.get("name")), params.get("arguments") or {})
    else:
        return {"jsonrpc": "2.0", "id": ident, "error": {"code": -32601, "message": f"no method {method}"}}
    return {"jsonrpc": "2.0", "id": ident, "result": result}

class Handler(BaseHTTPRequestHandler):
    """Streamable HTTP, answering in JSON or as one event when the server runs with --sse."""

    stream = False

    def do_POST(self) -> None:
        message = json.loads(self.rfile.read(int(self.headers.get("Content-Length") or 0)) or b"{}")
        answer = handle(message)
        if answer is None:
            self.send_response(202)
            self.end_headers()
            return
        body = json.dumps(answer).encode("utf-8")
        if self.stream:
            body = b"event: message\ndata: " + body + b"\n\n"
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream" if self.stream else "application/json")
        self.send_header("Mcp-Session-Id", "fake-session")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_DELETE(self) -> None:
        self.send_response(200)
        self.end_headers()

    def log_message(self, format: str, *args: object) -> None:
        return

def main(argv: list[str]) -> int:
    if "--http" in argv:
        Handler.stream = "--sse" in argv
        server = ThreadingHTTPServer(("127.0.0.1", int(argv[argv.index("--http") + 1])), Handler)
        server.serve_forever()
        return 0
    for line in sys.stdin.buffer:
        try:
            message = json.loads(line)
        except ValueError:
            continue
        answer = handle(message) if isinstance(message, dict) else None
        if answer is not None:
            sys.stdout.buffer.write(json.dumps(answer).encode("utf-8") + b"\n")
            sys.stdout.buffer.flush()
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
