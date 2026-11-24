"""The process that runs the code written by the model: a namespace kept between runs, call() / pmap() relayed.

It only talks to the engine through its stdin / stdout (one JSON message per line): it can be killed, started again or
closed in an OS sandbox without network, while the MCP connections and their secrets stay in the engine.
"""
from __future__ import annotations

import io
import json
import os
import sys
import threading
import time
import traceback
import types

HELPERS = ("call", "pmap", "ok", "failed", "ToolError", "Skipped", "json", "args")
FLUSH_EVERY = 0.4  # seconds between two flushes of what the code printed

class ToolError(Exception):
    """A tool call that gave no result: kind, server, tool, message, retry_after (seconds, a usage limit), attempts."""

    def __init__(self, info: dict) -> None:
        super().__init__(str(info.get("message") or ""))
        self.kind = str(info.get("kind") or "tool_error")
        self.server = str(info.get("server") or "")
        self.tool = str(info.get("tool") or "")
        self.message = str(info.get("message") or "")
        self.retry_after = info.get("retry_after")
        self.attempts = int(info.get("attempts") or 0)

    def __str__(self) -> str:
        wait = f" (retry after {float(self.retry_after):.0f}s)" if self.retry_after else ""
        return f"{self.server}.{self.tool}: {self.kind}: {self.message}{wait}"

    def __repr__(self) -> str:
        return f"{type(self).__name__}({str(self)!r})"

class Skipped(ToolError):
    """An item of a pmap that was not sent, because the server said it is limited or down before its turn."""

class Channel:
    """The JSON lines exchanged with the engine."""

    def __init__(self) -> None:
        self._in = sys.stdin.buffer
        self._out = sys.stdout.buffer
        self._lock = threading.Lock()

    def send(self, message: dict) -> None:
        data = json.dumps(message, ensure_ascii=False, default=str).encode("utf-8") + b"\n"
        with self._lock:
            self._out.write(data)
            self._out.flush()

    def receive(self) -> dict | None:
        line = self._in.readline()
        if not line:
            return None
        try:
            return json.loads(line.decode("utf-8", "replace"))
        except ValueError:
            return {}

class Output(io.TextIOBase):
    """What the code prints, sent to the engine in small batches (so a long run shows its progress)."""

    def __init__(self, channel: Channel) -> None:
        super().__init__()
        self._channel = channel
        self._parts: list[str] = []
        self._lock = threading.Lock()
        threading.Thread(target=self._tick, daemon=True).start()

    def writable(self) -> bool:
        return True

    def write(self, text: str) -> int:
        text = text if isinstance(text, str) else str(text)
        with self._lock:
            self._parts.append(text)
        return len(text)

    def flush(self) -> None:
        with self._lock:
            text = "".join(self._parts)
            self._parts = []
        if text:
            self._channel.send({"op": "out", "text": text})

    def _tick(self) -> None:
        while True:
            time.sleep(FLUSH_EVERY)
            self.flush()

class Runner:
    """Runs each piece of code in the same namespace and relays its tool calls."""

    def __init__(self) -> None:
        self.channel = Channel()
        self.output = Output(self.channel)
        self.progress: dict[str, dict[str, tuple]] = {}
        self.namespace: dict = {"__name__": "__codemode__", "__builtins__": __builtins__, "call": self.call, "pmap": self.pmap,
                                "ok": ok, "failed": failed, "ToolError": ToolError, "Skipped": Skipped, "json": json, "args": {}}

    def rpc(self, fn: str, payload: dict) -> object:
        """Ask the engine to do something with the servers, and wait for its answer."""
        self.output.flush()
        self.channel.send({"op": "rpc", "fn": fn, **payload})
        while True:
            reply = self.channel.receive()
            if reply is None:
                raise SystemExit(0)  # the engine is gone: nothing left to do here
            if reply.get("op") == "reply":
                break
        if reply.get("error"):
            raise ToolError(reply["error"])
        return reply.get("result")

    def call(self, server: str, tool: str, **args: object) -> object:
        """Call one tool and return its result as data (dict / list / str); ToolError when it fails."""
        return self.rpc("call", {"server": server, "tool": tool, "args": args})

    def pmap(self, server: str, tool: str, items: list, limit: int | None = None, key: str | None = None,
             stop_on_error: bool = False) -> list:
        """The same tool on many argument sets in parallel; a failed item is a ToolError at its place.

        key="name": the successful items are remembered under that name, so running the same pmap again only sends the
        ones that failed or were not sent (resume after a usage limit, nothing done twice).
        """
        items = list(items)
        done = self.progress.setdefault(key, {}) if key else {}
        marks = [json.dumps(item, sort_keys=True, default=str) for item in items]
        todo = [position for position, mark in enumerate(marks) if mark not in done]
        results: list = [done[mark][0] if mark in done else None for mark in marks]
        if todo:
            payload = {"server": server, "tool": tool, "items": [items[position] for position in todo], "limit": limit,
                       "stop_on_error": stop_on_error}
            for position, value in zip(todo, self.rpc("pmap", payload)):
                if isinstance(value, dict) and "__failure__" in value:
                    info = value["__failure__"]
                    value = Skipped(info) if info.get("kind") == "skipped" else ToolError(info)
                elif key:
                    done[marks[position]] = (value,)
                results[position] = value
        return results

    def run(self, message: dict) -> None:
        """Execute one piece of code, then report its error (if any) and the variables it leaves."""
        self.namespace["args"] = message.get("args") or {}
        error = None
        try:
            if message.get("cwd"):
                os.chdir(str(message["cwd"]))
            exec(compile(str(message.get("code") or ""), "<code>", "exec"), self.namespace)  # the purpose of this process
        except SystemExit as stop:
            error = f"exit({stop.code})" if stop.code not in (None, 0) else None
        except KeyboardInterrupt:
            error = "interrupted"
        except Exception as failure:  # whatever the code raised is reported to the model, never lost
            error = describe(failure, str(message.get("code") or ""))
        self.output.flush()
        kept = [name for name, value in self.namespace.items() if not name.startswith("_") and name not in HELPERS
                and not isinstance(value, (types.ModuleType, types.FunctionType, type))]
        self.channel.send({"op": "done", "id": message.get("id"), "error": error, "vars": sorted(kept)[:20]})

    def serve(self) -> int:
        self.channel.send({"op": "ready", "pid": os.getpid()})
        while True:
            message = self.channel.receive()
            if message is None or message.get("op") == "exit":
                return 0
            if message.get("op") == "exec":
                self.run(message)

def ok(results: list) -> list:
    """The successful results of a pmap."""
    return [value for value in results if not isinstance(value, ToolError)]

def failed(results: list) -> list[tuple[int, ToolError]]:
    """The (position, error) pairs of a pmap."""
    return [(position, value) for position, value in enumerate(results) if isinstance(value, ToolError)]

def describe(failure: BaseException, code: str) -> str:
    """The line of the code that failed and the error, without the frames of codemode itself."""
    lines = code.splitlines()
    where = ""
    if isinstance(failure, SyntaxError) and failure.filename == "<code>":
        where = f"line {failure.lineno}: {(failure.text or '').strip()}\n"
    else:
        frames = [frame for frame in traceback.extract_tb(failure.__traceback__) if frame.filename == "<code>"]
        if frames:
            number = frames[-1].lineno or 0
            where = f"line {number}: {lines[number - 1].strip() if 0 < number <= len(lines) else ''}\n"
    return f"{where}{type(failure).__name__}: {failure}"

def main() -> int:
    """Entry of `codemode.py worker`: the protocol keeps the real stdout, the code prints into the relay."""
    runner = Runner()
    sys.stdin = io.StringIO("")
    sys.stdout = runner.output
    sys.stderr = runner.output
    sys.argv = ["codemode"]
    return runner.serve()
