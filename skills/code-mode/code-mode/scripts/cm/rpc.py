"""MCP client side: JSON-RPC 2.0 over stdio, streamable HTTP and the older SSE transport (standard library only)."""
from __future__ import annotations

import json
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import urllib.error
import urllib.parse
import urllib.request
from collections.abc import Callable, Iterator
from pathlib import Path

from .paths import PROTOCOL, VERSION, safe_name

VAR = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)(?::-([^}]*))?\}")  # ${NAME} and ${NAME:-default} in a server config
HTTP_KINDS = ("http", "streamable-http", "streamablehttp", "streamable_http")
LOG_LIMIT = 1000000  # the stderr log of a server starts again over this size
Notify = Callable[[str, str, dict], None]

class RpcError(Exception):
    """A JSON-RPC error answered by the other side."""

    def __init__(self, code: int, message: str, data: object = None) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.data = data

class ConnectionLost(Exception):
    """The server is gone: its process exited, the connection was refused or closed."""

class Waiter:
    """The answer one request waits for."""

    def __init__(self) -> None:
        self.event = threading.Event()
        self.result: object = None
        self.error: Exception | None = None

class Connection:
    """What every transport shares: request ids, answers matched to their request, the requests of the server."""

    def __init__(self, name: str, on_notification: Notify | None = None) -> None:
        self.name = name
        self.on_notification = on_notification
        self.closed = False
        self.reason = ""
        self._pending: dict[int, Waiter] = {}
        self._lock = threading.Lock()
        self._next = 0

    def request(self, method: str, params: dict | None = None, timeout: float | None = None) -> object:
        """Send a request and wait for its result; RpcError, TimeoutError or ConnectionLost otherwise."""
        if self.closed:
            raise ConnectionLost(self.reason or f"{self.name}: connection closed")
        with self._lock:
            self._next += 1
            ident = self._next
            waiter = self._pending[ident] = Waiter()
        message: dict = {"jsonrpc": "2.0", "id": ident, "method": method}
        if params is not None:
            message["params"] = params
        try:
            self._send(message, timeout)
            if not waiter.event.wait(timeout):
                self._cancel(ident)
                raise TimeoutError(f"{self.name}: no answer to {method} after {timeout:.0f}s")
        finally:
            with self._lock:
                self._pending.pop(ident, None)
        if waiter.error is not None:
            raise waiter.error
        return waiter.result

    def notify(self, method: str, params: dict | None = None) -> None:
        message: dict = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            message["params"] = params
        self._send(message, 30)

    def close(self) -> None:
        self.closed = True

    def _send(self, message: dict, timeout: float | None = None) -> None:
        raise NotImplementedError

    def _cancel(self, ident: int) -> None:
        """Tell the server a request is no longer waited for (it may still finish it)."""
        try:
            self.notify("notifications/cancelled", {"requestId": ident, "reason": "timeout"})
        except (ConnectionLost, RpcError, OSError):
            pass  # the server is gone or refuses: nothing more to cancel

    def _incoming(self, message: object) -> None:
        """One message read from the server: an answer, a request of its own or a notification."""
        if isinstance(message, list):
            for item in message:
                self._incoming(item)
            return
        if not isinstance(message, dict):
            return
        if "method" in message:
            if "id" in message:
                self._answer(message)
            elif self.on_notification is not None:
                self.on_notification(self.name, str(message["method"]), message.get("params") or {})
            return
        with self._lock:
            waiter = self._pending.get(message.get("id"))
        if waiter is None:
            return
        if message.get("error") is not None:
            error = message["error"] if isinstance(message["error"], dict) else {"message": str(message["error"])}
            waiter.error = RpcError(int(error.get("code", -32000)), str(error.get("message", "")), error.get("data"))
        else:
            waiter.result = message.get("result")
        waiter.event.set()

    def _answer(self, message: dict) -> None:
        """A request sent by the server: ping and roots are answered, the rest is refused."""
        reply: dict = {"jsonrpc": "2.0", "id": message["id"]}
        if message["method"] == "ping":
            reply["result"] = {}
        elif message["method"] == "roots/list":
            reply["result"] = {"roots": []}
        else:
            reply["error"] = {"code": -32601, "message": f"codemode does not handle {message['method']}"}
        try:
            self._send(reply, 30)
        except (ConnectionLost, RpcError, OSError):
            pass  # the server left before its answer

    def _fail_all(self, reason: str) -> None:
        """The connection is lost: every waiting request fails with the reason."""
        self.closed = True
        self.reason = self.reason or reason
        with self._lock:
            waiting = list(self._pending.values())
        for waiter in waiting:
            if not waiter.event.is_set():
                waiter.error = ConnectionLost(self.reason)
                waiter.event.set()

class StdioConnection(Connection):
    """A server started as a child process: one JSON message per line on its stdin / stdout."""

    def __init__(self, name: str, command: str, args: list[str], env: dict[str, str], cwd: str, log_path: Path,
                 on_notification: Notify | None = None) -> None:
        super().__init__(name, on_notification)
        self._log_path = log_path
        self._write = threading.Lock()
        if log_path.is_file() and log_path.stat().st_size > LOG_LIMIT:
            log_path.unlink()
        self._log = open(log_path, "ab")
        options: dict = {"creationflags": 0x08000200} if sys.platform == "win32" else {"start_new_session": True}
        try:
            self._proc = subprocess.Popen([shutil.which(command, path=env.get("PATH")) or command, *args], stdin=subprocess.PIPE,
                                          stdout=subprocess.PIPE, stderr=self._log, env=env, cwd=cwd or None, **options)
        except OSError as error:
            self._log.close()
            raise ConnectionLost(f"{name}: cannot start {command}: {error}") from error
        threading.Thread(target=self._read, name=f"mcp-{name}", daemon=True).start()

    def _send(self, message: dict, timeout: float | None = None) -> None:
        data = json.dumps(message, ensure_ascii=False).encode("utf-8") + b"\n"
        with self._write:
            try:
                self._proc.stdin.write(data)
                self._proc.stdin.flush()
            except (OSError, ValueError) as error:
                raise ConnectionLost(self._gone()) from error

    def _read(self) -> None:
        """Reader thread: every JSON line of the server (a server that logs on stdout is tolerated)."""
        while True:
            line = self._proc.stdout.readline()
            if not line:
                break
            line = line.strip()
            if line[:1] not in (b"{", b"["):
                continue
            try:
                message = json.loads(line.decode("utf-8", "replace"))
            except ValueError:
                continue
            self._incoming(message)
        self._fail_all(self._gone())

    def _gone(self) -> str:
        """Why the server is no longer there, with the end of what it wrote on stderr."""
        code = self._proc.poll()
        tail = ""
        try:
            self._log.flush()
            with open(self._log_path, "rb") as file:
                file.seek(max(0, self._log_path.stat().st_size - 2000))
                lines = [l.strip() for l in file.read().decode("utf-8", "replace").splitlines() if l.strip()]
            tail = " | ".join(lines[-3:])[-300:]
        except (OSError, ValueError):
            pass  # no log to quote
        state = "server closed its output" if code is None else f"server exited (code {code})"
        return f"{self.name}: {state}" + (f": {tail}" if tail else "")

    def close(self) -> None:
        self.closed = True
        try:
            self._proc.stdin.close()
        except OSError:
            pass  # already closed by the server
        try:
            self._proc.wait(2)
        except subprocess.TimeoutExpired:
            self._kill_tree()
        try:
            self._log.close()
        except OSError:
            pass  # the log is not worth an error at close

    def _kill_tree(self) -> None:
        """Stop the server and what it started (uvx, npx and node keep a child)."""
        if sys.platform == "win32":
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(self._proc.pid)], capture_output=True, check=False)
            return
        for sig in (signal.SIGTERM, signal.SIGKILL):
            try:
                os.killpg(self._proc.pid, sig)
            except (ProcessLookupError, PermissionError):
                return
            try:
                self._proc.wait(2)
                return
            except subprocess.TimeoutExpired:
                continue

def sse_events(stream: object) -> Iterator[tuple[str, str]]:
    """The (event, data) pairs of a text/event-stream body."""
    event, data = "message", []
    for raw in stream:
        line = raw.decode("utf-8", "replace").rstrip("\r\n")
        if not line:
            if data:
                yield event, "\n".join(data)
            event, data = "message", []
            continue
        if line.startswith(":"):
            continue
        name, _, value = line.partition(":")
        value = value[1:] if value.startswith(" ") else value
        if name == "event":
            event = value
        elif name == "data":
            data.append(value)
    if data:
        yield event, "\n".join(data)

def http_failure(name: str, error: urllib.error.HTTPError) -> Exception:
    """An HTTP status as an error that keeps what a retry needs (429, Retry-After)."""
    try:
        detail = error.read(600).decode("utf-8", "replace").strip()
    except OSError:
        detail = ""
    retry = error.headers.get("Retry-After", "") if error.headers else ""
    text = f"HTTP {error.code} {error.reason}" + (f" (Retry-After: {retry})" if retry else "") + (f": {detail}" if detail else "")
    return RpcError(-32000, f"{name}: {text}", {"status": error.code})

class HttpConnection(Connection):
    """Streamable HTTP: each message is a POST, the answer comes back as JSON or as an event stream."""

    def __init__(self, name: str, url: str, headers: dict[str, str], on_notification: Notify | None = None) -> None:
        super().__init__(name, on_notification)
        self.url = url
        self.headers = {str(key): str(value) for key, value in headers.items()}
        self.session_id = ""
        self.protocol = ""

    def _send(self, message: dict, timeout: float | None = None) -> None:
        headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream", **self.headers}
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        if self.protocol:
            headers["MCP-Protocol-Version"] = self.protocol
        request = urllib.request.Request(self.url, data=json.dumps(message, ensure_ascii=False).encode("utf-8"), headers=headers,
                                         method="POST")
        try:
            response = urllib.request.urlopen(request, timeout=timeout or 60)
        except urllib.error.HTTPError as error:
            if error.code == 404 and self.session_id:
                self._fail_all(f"{self.name}: session expired")
                raise ConnectionLost(self.reason) from error
            raise http_failure(self.name, error) from error
        except TimeoutError as error:
            raise TimeoutError(f"{self.name}: no answer from {self.url}") from error
        except (urllib.error.URLError, OSError) as error:
            reason = getattr(error, "reason", error)
            if isinstance(reason, TimeoutError):
                raise TimeoutError(f"{self.name}: no answer from {self.url}") from error
            raise ConnectionLost(f"{self.name}: {reason} ({self.url})") from error
        with response:
            self.session_id = response.headers.get("Mcp-Session-Id") or self.session_id
            if response.status == 202:
                return
            if "text/event-stream" in (response.headers.get("Content-Type") or ""):
                for _, data in sse_events(response):
                    try:
                        parsed = json.loads(data)
                    except ValueError:
                        continue
                    self._incoming(parsed)
                    if "id" in message and isinstance(parsed, dict) and parsed.get("id") == message["id"] and "method" not in parsed:
                        break
                return
            raw = response.read()
            if raw.strip():
                try:
                    self._incoming(json.loads(raw.decode("utf-8", "replace")))
                except ValueError as error:
                    raise RpcError(-32700, f"{self.name}: answer is not JSON") from error

    def close(self) -> None:
        self.closed = True
        if not self.session_id:
            return
        request = urllib.request.Request(self.url, headers={**self.headers, "Mcp-Session-Id": self.session_id}, method="DELETE")
        try:
            urllib.request.urlopen(request, timeout=5).close()
        except (urllib.error.URLError, OSError):
            pass  # a server may refuse DELETE: the session ends by itself

class SseConnection(Connection):
    """The older HTTP+SSE transport: a long GET stream gives the POST address and carries every answer."""

    def __init__(self, name: str, url: str, headers: dict[str, str], timeout: float, on_notification: Notify | None = None) -> None:
        super().__init__(name, on_notification)
        self.url = url
        self.headers = {str(key): str(value) for key, value in headers.items()}
        self._endpoint = ""
        self._ready = threading.Event()
        self._stream: object = None
        threading.Thread(target=self._listen, name=f"mcp-{name}", daemon=True).start()
        if not self._ready.wait(timeout) or not self._endpoint:
            self.closed = True
            raise ConnectionLost(self.reason or f"{name}: no endpoint event from {url}")

    def _listen(self) -> None:
        reason = "stream closed"
        try:
            request = urllib.request.Request(self.url, headers={"Accept": "text/event-stream", **self.headers})
            self._stream = urllib.request.urlopen(request)
            for event, data in sse_events(self._stream):
                if event == "endpoint":
                    self._endpoint = urllib.parse.urljoin(self.url, data.strip())
                    self._ready.set()
                    continue
                try:
                    self._incoming(json.loads(data))
                except ValueError:
                    continue
        except (urllib.error.URLError, OSError, ValueError) as error:
            reason = str(getattr(error, "reason", error))
        self._fail_all(f"{self.name}: {reason} ({self.url})")
        self._ready.set()

    def _send(self, message: dict, timeout: float | None = None) -> None:
        request = urllib.request.Request(self._endpoint, data=json.dumps(message, ensure_ascii=False).encode("utf-8"),
                                         headers={"Content-Type": "application/json", **self.headers}, method="POST")
        try:
            urllib.request.urlopen(request, timeout=timeout or 60).close()
        except urllib.error.HTTPError as error:
            raise http_failure(self.name, error) from error
        except TimeoutError as error:
            raise TimeoutError(f"{self.name}: no answer from {self._endpoint}") from error
        except (urllib.error.URLError, OSError) as error:
            raise ConnectionLost(f"{self.name}: {getattr(error, 'reason', error)}") from error

    def close(self) -> None:
        self.closed = True
        try:
            if self._stream is not None:
                self._stream.close()
        except OSError:
            pass  # the stream is already closed

class McpClient:
    """The MCP calls codemode needs on a connection: initialize, tools/list, tools/call."""

    def __init__(self, connection: Connection) -> None:
        self.connection = connection
        self.info: dict = {}
        self.instructions = ""

    def initialize(self, timeout: float) -> None:
        params = {"protocolVersion": PROTOCOL, "capabilities": {}, "clientInfo": {"name": "codemode", "version": VERSION}}
        result = self.connection.request("initialize", params, timeout)
        result = result if isinstance(result, dict) else {}
        self.info = result.get("serverInfo") or {}
        self.instructions = str(result.get("instructions") or "")
        if isinstance(self.connection, HttpConnection):
            self.connection.protocol = str(result.get("protocolVersion") or PROTOCOL)
        self.connection.notify("notifications/initialized")

    def list_tools(self, timeout: float) -> list[dict]:
        """Every tool definition of the server (all pages)."""
        tools: list[dict] = []
        cursor = None
        for _ in range(500):
            page = self.connection.request("tools/list", {"cursor": cursor} if cursor else {}, timeout)
            page = page if isinstance(page, dict) else {}
            tools += [tool for tool in page.get("tools") or [] if isinstance(tool, dict) and tool.get("name")]
            cursor = page.get("nextCursor")
            if not cursor:
                break
        return tools

    def call_tool(self, name: str, arguments: dict, timeout: float) -> dict:
        result = self.connection.request("tools/call", {"name": name, "arguments": arguments}, timeout)
        return result if isinstance(result, dict) else {}

    def close(self) -> None:
        self.connection.close()

def expand(value: object, env: dict[str, str]) -> object:
    """${NAME} / ${NAME:-default} replaced in every string of a server config, like Claude Code does."""
    if isinstance(value, str):
        return VAR.sub(lambda m: env.get(m.group(1), m.group(2) if m.group(2) is not None else m.group(0)), value)
    if isinstance(value, list):
        return [expand(item, env) for item in value]
    if isinstance(value, dict):
        return {key: expand(item, env) for key, item in value.items()}
    return value

def open_connection(name: str, config: dict, cwd: str, log_dir: Path, timeout: float, on_notification: Notify | None = None) -> Connection:
    """The connection a server config describes (stdio by default, http / sse when it has a url)."""
    env = dict(os.environ)
    config = expand(config, env)
    kind = str(config.get("type") or config.get("transport") or ("http" if config.get("url") else "stdio")).lower()
    if kind in HTTP_KINDS:
        return HttpConnection(name, str(config.get("url") or ""), config.get("headers") or {}, on_notification)
    if kind == "sse":
        return SseConnection(name, str(config.get("url") or ""), config.get("headers") or {}, timeout, on_notification)
    if kind != "stdio":
        raise ConnectionLost(f"{name}: transport {kind!r} is not supported (stdio, http, sse)")
    if not config.get("command"):
        raise ConnectionLost(f"{name}: no command in its config")
    env.update({str(key): str(value) for key, value in (config.get("env") or {}).items()})
    args = [str(arg) for arg in config.get("args") or []]
    return StdioConnection(name, str(config["command"]), args, env, cwd, log_dir / f"{safe_name(name)}.log", on_notification)
