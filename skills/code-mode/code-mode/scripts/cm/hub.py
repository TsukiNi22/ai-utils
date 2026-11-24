"""The servers codemode drives: where they are declared, lazy connections, the tool index, the rules, the calls.

A call that fails is classified (usage limit, transient, timeout, server down, refused, tool error): a usage limit is
waited and tried again, a transient failure too when the tool only reads, and a batch stops sending once a server says
it is limited or down, so what already succeeded is kept and the rest is reported as not sent.
"""
from __future__ import annotations

import base64
import difflib
import fnmatch
import hashlib
import json
import mimetypes
import os
import re
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from . import sizes
from .paths import home, read_json, safe_name, sub, write_json
from .rpc import ConnectionLost, McpClient, RpcError, open_connection

LIMIT = re.compile(r"rate.?limit|too many requests|\b429\b|quota|usage limit|limit (?:reached|exceeded)|exceeded.{0,30}limit"
                   r"|try again (?:in|later)|retry.?after|overloaded|throttl|at capacity", re.I)
TRANSIENT = re.compile(r"timed? ?out|temporar|connection (?:reset|refused|aborted|closed)|ECONNRESET|ECONNREFUSED|EAGAIN|EPIPE"
                       r"|unavailable|\b50[234]\b|bad gateway|network (?:error|unreachable)", re.I)
AFTER = re.compile(r"(?:retry[- _]?after|try again in|retry in|wait(?:ing)?(?: for)?|available in|resets? in)[^0-9]{0,12}"
                   r"(\d+(?:\.\d+)?)\s*(ms|milliseconds?|s|secs?|seconds?|m|mins?|minutes?|h|hours?)?", re.I)
READ = re.compile(r"(?:^|[_\-.])(?:get|list|search|find|check|detect|info|status|progress|history|version|count|analy[sz]e|look"
                  r"|read|show|describe|query|fetch|view|inspect|probe|stats?|help|ping|whoami|diff|validate|test)(?:[_\-.]|$)", re.I)
WRITE = re.compile(r"(?:^|[_\-.])(?:create|delete|remove|update|set|write|add|replace|send|post|put|patch|move|rename|kill|stop"
                   r"|start|run|exec(?:ute)?|install|import|save|apply|edit|insert|drop|clear|reset|trim|cut|paste|record|generate"
                   r"|upload|launch|open|close|new|make|convert|export|render|mix|merge|split|burn|compress|resize|crop|rotate"
                   r"|play|pause|toggle|trigger|undo|redo|restart|enable|disable|mute|unmute|select|fill|draw|flip|scale"
                   r"|duplicate|reorder|register|publish|share|reply|forward|label|trash|mark)(?:[_\-.]|$)", re.I)
UNITS = {"ms": 0.001, "mi": 0.001, "s": 1.0, "m": 60.0, "h": 3600.0}
STOPPING = ("rate_limit", "server_down", "unknown_server")  # a batch stops sending after one of these
CODE_NAME = re.compile(r"(?:^|[_\-.])(?:execute|exec|run|eval|evaluate)(?:[_\-.]|$)", re.I)
CODE_ARGS = ("code", "script", "source", "luau", "lua", "python", "command", "expression")
MAX_FILE_BYTES = 50000000  # a media part bigger than this is not saved

class ToolFailure(Exception):
    """A call that did not give a result, with what a script needs to decide what to do next."""

    def __init__(self, kind: str, server: str, tool: str, message: str, retry_after: float | None = None, attempts: int = 1) -> None:
        super().__init__(message)
        self.kind = kind
        self.server = server
        self.tool = tool
        self.message = message
        self.retry_after = retry_after
        self.attempts = attempts

    def as_dict(self) -> dict:
        return {"kind": self.kind, "server": self.server, "tool": self.tool, "message": self.message,
                "retry_after": self.retry_after, "attempts": self.attempts}

class Outcome:
    """The result of one successful call."""

    def __init__(self, data: object, raw_chars: int, files: list[str], parts: list[dict], attempts: int, seconds: float) -> None:
        self.data = data
        self.raw_chars = raw_chars  # what the same call would have put in the context of the model
        self.files = files  # images, audio and blobs of the result, saved to disk
        self.parts = parts  # the raw media parts, to hand an image over as it is
        self.attempts = attempts
        self.seconds = seconds

def claude_json() -> Path:
    """The Claude Code file holding the user and local MCP servers."""
    return Path(os.environ.get("CLAUDE_CONFIG_DIR") or Path.home()) / ".claude.json"

def desktop_config() -> Path:
    """The Claude Desktop file holding its local MCP servers."""
    if sys.platform == "win32":
        base = Path(os.environ.get("APPDATA") or Path.home() / "AppData" / "Roaming")
    elif sys.platform == "darwin":
        base = Path.home() / "Library" / "Application Support"
    else:
        base = Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config")
    return base / "Claude" / "claude_desktop_config.json"

def is_self(name: str, config: dict) -> bool:
    """codemode never drives itself."""
    words = " ".join([str(config.get("command") or "")] + [str(arg) for arg in config.get("args") or []])
    return name == "codemode" or "codemode.py" in words

def project_file(cwd: str) -> Path | None:
    """The .mcp.json of the project: the first one found from the folder up."""
    folder = Path(cwd).resolve()
    for parent in [folder, *folder.parents]:
        if (parent / ".mcp.json").is_file():
            return parent / ".mcp.json"
    return None

def approved_project_servers(project: Path, claude: dict) -> tuple[set[str], bool]:
    """The .mcp.json servers the user approved in Claude Code (names, or all): a cloned repository starts nothing."""
    names: set[str] = set()
    every = False
    refused: set[str] = set()
    entry = (claude.get("projects") or {}).get(str(project)) or {}
    files = [project / ".claude" / "settings.local.json", project / ".claude" / "settings.json",
             Path.home() / ".claude" / "settings.json"]
    for source in [entry] + [read_json(path, {}) for path in files]:
        if not isinstance(source, dict):
            continue
        names |= {str(name) for name in source.get("enabledMcpjsonServers") or []}
        refused |= {str(name) for name in source.get("disabledMcpjsonServers") or []}
        every = every or bool(source.get("enableAllProjectMcpServers"))
    return names - refused, every and not refused

def discover(cwd: str, config: dict) -> tuple[dict[str, dict], list[str]]:
    """Every server codemode may drive: name -> {"config", "source"}; and what was left out, with why."""
    found: dict[str, dict] = {}
    notes: list[str] = []
    claude = read_json(claude_json(), {})
    claude = claude if isinstance(claude, dict) else {}

    def take(servers: object, source: str, only: set[str] | None = None) -> None:
        for name, spec in (servers.items() if isinstance(servers, dict) else []):
            if not isinstance(spec, dict) or name in found or is_self(name, spec) or name in config.get("disabled_servers", []):
                continue
            if only is not None and name not in only:
                notes.append(f"{name}: in {source} but not approved in Claude Code (approve it there first)")
                continue
            found[name] = {"config": spec, "source": source}

    moved = read_json(home() / "servers.json", {})
    take({name: item.get("config") for name, item in moved.items() if isinstance(item, dict)} if isinstance(moved, dict) else {},
         "codemode")
    if config.get("sources", {}).get("claude_code", True):
        folder = Path(cwd).resolve()
        for parent in [folder, *folder.parents]:
            take(((claude.get("projects") or {}).get(str(parent)) or {}).get("mcpServers"), "claude-code local")
        project = project_file(cwd)
        if project is not None:
            names, every = approved_project_servers(project.parent, claude)
            declared = read_json(project, {})
            take(declared.get("mcpServers") if isinstance(declared, dict) else {}, "project .mcp.json", None if every else names)
        take(claude.get("mcpServers"), "claude-code user")
    if config.get("sources", {}).get("claude_desktop", True):
        desktop = read_json(desktop_config(), {})
        take(desktop.get("mcpServers") if isinstance(desktop, dict) else {}, "claude-desktop")
    return found, notes

def source_stamp(cwd: str) -> tuple:
    """What changes when a server is declared, moved or removed (the files read by discover)."""
    paths = [home() / "servers.json", claude_json(), desktop_config(), home() / "config.json"]
    project = project_file(cwd)
    if project is not None:
        paths.append(project)
    stamp = []
    for path in paths:
        try:
            stamp.append((str(path), path.stat().st_mtime_ns))
        except OSError:
            stamp.append((str(path), 0))
    return tuple(stamp)

def classify(text: str) -> str:
    """The kind of a failure from what the server said."""
    if LIMIT.search(text):
        return "rate_limit"
    if TRANSIENT.search(text):
        return "transient"
    return "tool_error"

def retry_delay(text: str) -> float | None:
    """The wait a server asks for ("retry after 30s", "try again in 2 minutes"), in seconds."""
    match = AFTER.search(text)
    if not match:
        return None
    unit = (match.group(2) or "s").lower()
    return float(match.group(1)) * UNITS.get(unit[:2] if unit[:2] in ("ms", "mi") else unit[:1], 1.0)

def code_tool(spec: dict) -> bool:
    """Whether a tool already runs a script inside its application (execute_blender_code, execute_luau, run_code)."""
    properties = (spec.get("inputSchema") or {}).get("properties") or {}
    return bool(CODE_NAME.search(str(spec.get("name") or ""))) and any(name in properties for name in CODE_ARGS)

def matches(patterns: list, server: str, tool: str) -> bool:
    return any(fnmatch.fnmatchcase(f"{server}.{tool}", str(p)) or fnmatch.fnmatchcase(server, str(p)) for p in patterns)

class Hub:
    """Connections opened at first use and kept, the tool index, the rules and the calls with their retries."""

    def __init__(self, cwd: str, config: dict, session: str) -> None:
        self.cwd = cwd
        self.config = config
        self.session = session
        self.notes: list[str] = []
        self._servers: dict[str, dict] | None = None
        self._stamp: tuple = ()
        self._clients: dict[str, McpClient] = {}
        self._tools: dict[str, dict[str, dict]] = {}
        self._stale: set[str] = set()
        self._limited: dict[str, float] = {}
        self._locks: dict[str, threading.RLock] = {}
        self._guard = threading.Lock()
        self._files = 0

    def servers(self) -> dict[str, dict]:
        """The servers declared now (read again when one of the config files changed)."""
        stamp = source_stamp(self.cwd)
        with self._guard:
            if self._servers is None or stamp != self._stamp:
                self._servers, self.notes = discover(self.cwd, self.config)
                self._stamp = stamp
            return self._servers

    def _lock(self, name: str) -> threading.RLock:
        with self._guard:
            return self._locks.setdefault(name, threading.RLock())

    def _spec(self, name: str) -> dict:
        spec = self.servers().get(name)
        if spec is None:
            close = difflib.get_close_matches(name, list(self.servers()), 3, 0.5)
            hint = f" (did you mean {', '.join(close)}?)" if close else f" (known: {', '.join(sorted(self.servers())) or 'none'})"
            raise ToolFailure("unknown_server", name, "", f"no MCP server named {name}{hint}")
        return spec

    def client(self, name: str) -> McpClient:
        """The open connection of a server, started and initialized the first time."""
        spec = self._spec(name)
        with self._lock(name):
            client = self._clients.get(name)
            if client is not None and not client.connection.closed:
                return client
            timeout = float(self.config["connect_timeout_s"])
            connection = open_connection(name, spec["config"], self.cwd, sub("logs"), timeout, self._notified)
            client = McpClient(connection)
            try:
                client.initialize(timeout)
            except (ConnectionLost, RpcError, TimeoutError, OSError):
                connection.close()
                raise
            self._clients[name] = client
            return client

    def _notified(self, server: str, method: str, params: dict) -> None:
        if method == "notifications/tools/list_changed":
            self._stale.add(server)

    def _fingerprint(self, name: str) -> str:
        return hashlib.sha256(json.dumps(self._spec(name)["config"], sort_keys=True, default=str).encode("utf-8")).hexdigest()[:16]

    def tools(self, name: str, refresh: bool = False) -> dict[str, dict]:
        """The tool definitions of a server: memory, then the index on disk, then the server itself."""
        with self._lock(name):
            if not refresh and name in self._tools and name not in self._stale:
                return self._tools[name]
            fingerprint = self._fingerprint(name)
            cached = read_json(home() / "index.json", {})
            entry = cached.get(name) if isinstance(cached, dict) else None
            usable = isinstance(entry, dict) and entry.get("fp") == fingerprint and not refresh and name not in self._stale
            if usable and not entry.get("error"):
                self._tools[name] = {tool["name"]: tool for tool in entry.get("tools") or []}
                return self._tools[name]
            if usable and time.time() - float(entry.get("at") or 0) < float(self.config["index_retry_s"]):
                raise ToolFailure("server_down", name, "", str(entry["error"]))
            error = ""
            notes = ""
            listed: list[dict] = []
            try:
                client = self.client(name)
                listed = client.list_tools(float(self.config["connect_timeout_s"]))
                notes = client.instructions[:3000]
            except (ConnectionLost, RpcError, TimeoutError, OSError) as failure:
                error = str(failure) or type(failure).__name__
            self._store_index(name, {"fp": fingerprint, "at": time.time(), "tools": listed, "error": error, "instructions": notes})
            self._stale.discard(name)
            if error:
                raise ToolFailure("server_down", name, "", error)
            self._tools[name] = {tool["name"]: tool for tool in listed}
            return self._tools[name]

    def _store_index(self, name: str, entry: dict) -> None:
        """Keep the tool list of one server on disk, next to the ones of the other servers and sessions."""
        with self._guard:
            cached = read_json(home() / "index.json", {})
            cached = cached if isinstance(cached, dict) else {}
            cached[name] = entry
            try:
                write_json(home() / "index.json", cached)
            except OSError:
                pass  # the index is a cache: memory still has it

    def index(self, refresh: bool = False) -> tuple[dict[str, dict[str, dict]], dict[str, str]]:
        """The tools of every server (servers never listed are started in parallel) and the servers that failed."""
        names = sorted(self.servers())
        tools: dict[str, dict[str, dict]] = {}
        errors: dict[str, str] = {}
        before = set(self._clients)

        def load(name: str) -> None:
            try:
                tools[name] = self.tools(name, refresh)
            except ToolFailure as failure:
                errors[name] = failure.message

        if names:
            with ThreadPoolExecutor(max_workers=min(8, len(names))) as pool:
                list(pool.map(load, names))
        for name in set(self._clients) - before:
            self._drop(name)  # started only to list its tools: it starts again when a call needs it
        return tools, errors

    def instructions(self, name: str) -> str:
        """What a server says about itself at initialize (kept in the index, so it is there without starting it)."""
        cached = read_json(home() / "index.json", {})
        entry = cached.get(name) if isinstance(cached, dict) else None
        return str(entry.get("instructions") or "") if isinstance(entry, dict) else ""

    def read_only(self, server: str, spec: dict) -> bool:
        """Whether a tool only reads: the rules of the user, then what the server declares, then its name."""
        rules = self.config.get("rules", {})
        tool = str(spec.get("name") or "")
        if matches(rules.get("write", []), server, tool):
            return False
        if matches(rules.get("read_only", []), server, tool):
            return True
        hint = (spec.get("annotations") or {}).get("readOnlyHint")
        if hint is not None:
            return bool(hint)
        return bool(READ.search(tool)) and not WRITE.search(tool)

    def definition(self, server: str, tool: str) -> dict:
        """The definition of a tool, the server asked again once when the name is not known."""
        spec = self.tools(server).get(tool)
        if spec is None:
            spec = self.tools(server, refresh=True).get(tool)
        if spec is None:
            close = difflib.get_close_matches(tool, list(self.tools(server)), 3, 0.5)
            raise ToolFailure("unknown_tool", server, tool, f"{server} has no tool {tool}" + (f" (did you mean {', '.join(close)}?)" if close else ""))
        return spec

    def check(self, server: str, tool: str, spec: dict, allow_write: bool) -> None:
        """Refuse what the rules forbid, and a tool that changes things without the write permission."""
        if matches(self.config.get("rules", {}).get("deny", []), server, tool):
            raise ToolFailure("denied", server, tool, f"{server}.{tool} is refused by the rules of the codemode config")
        if not allow_write and not self.read_only(server, spec):
            raise ToolFailure("denied", server, tool, f"{server}.{tool} changes things: it needs allow_write (only when the request asks for a change)")

    def _gate(self, server: str, tool: str) -> None:
        """Wait while a server said it is limited; fail at once when the wait is too long."""
        remaining = self._limited.get(server, 0.0) - time.time()
        if remaining <= 0:
            return
        if remaining > float(self.config["retry_max_wait_s"]):
            raise ToolFailure("rate_limit", server, tool, f"{server} is limited for {remaining:.0f}s more", retry_after=remaining, attempts=0)
        time.sleep(remaining)

    def call(self, server: str, tool: str, args: dict | None = None, allow_write: bool = False, timeout: float | None = None,
             retry: bool = False, record: list | None = None) -> Outcome:
        """Call one tool; a usage limit is waited and tried again, a transient failure too when the tool only reads."""
        started = time.monotonic()
        attempts = 0
        failure: ToolFailure | None = None
        outcome: Outcome | None = None
        try:
            spec = self.definition(server, tool)
            self.check(server, tool, spec, allow_write)
            safe = self.read_only(server, spec) or retry
            limit = max(1, int(self.config["retry_attempts"]))
            while outcome is None:
                self._gate(server, tool)
                attempts += 1
                try:
                    outcome = self._once(server, tool, args or {}, timeout, attempts, started)
                except ToolFailure as error:
                    error.attempts = attempts
                    wait = error.retry_after if error.retry_after is not None else min(2.0 ** (attempts - 1), 8.0)
                    again = error.kind == "rate_limit" or (error.kind in ("transient", "server_down", "timeout") and safe)
                    if error.kind == "rate_limit":
                        self._limited[server] = time.time() + wait
                    if not again or attempts >= limit or wait > float(self.config["retry_max_wait_s"]):
                        raise
                    if error.kind != "rate_limit":
                        time.sleep(wait)
        except ToolFailure as error:
            failure = error
            raise
        finally:
            if record is not None:
                record.append({"server": server, "tool": tool, "ok": failure is None and outcome is not None,
                               "kind": failure.kind if failure else "", "message": failure.message[:200] if failure else "",
                               "retry_after": failure.retry_after if failure else None, "attempts": attempts,
                               "in": len(json.dumps(args or {}, default=str)), "out": outcome.raw_chars if outcome else 0,
                               "ms": int((time.monotonic() - started) * 1000)})
        sizes.record(f"{server}.{tool}", outcome.raw_chars)
        return outcome

    def _once(self, server: str, tool: str, args: dict, timeout: float | None, attempts: int, started: float) -> Outcome:
        """One try of a call, its failure classified."""
        limit = float(timeout or self.config["call_timeout_s"])
        try:
            raw = self.client(server).call_tool(tool, args, limit)
        except TimeoutError as error:
            raise ToolFailure("timeout", server, tool, f"no answer after {limit:.0f}s (the server may still be working on it)") from error
        except ConnectionLost as error:
            self._drop(server)
            raise ToolFailure("server_down", server, tool, str(error)) from error
        except RpcError as error:
            text = error.message or f"error {error.code}"
            if error.code == -32601:
                self._stale.add(server)
                raise ToolFailure("unknown_tool", server, tool, text) from error
            kind = "tool_error" if error.code == -32602 else classify(text)
            raise ToolFailure(kind, server, tool, text, retry_after=retry_delay(text) if kind == "rate_limit" else None) from error
        except OSError as error:
            self._drop(server)
            raise ToolFailure("server_down", server, tool, str(error)) from error
        if raw.get("isError"):
            text = "\n".join(str(part.get("text") or "") for part in raw.get("content") or [] if isinstance(part, dict)).strip()
            text = text or "the tool reported an error without a message"
            kind = classify(text)
            raise ToolFailure(kind, server, tool, text[:2000], retry_after=retry_delay(text) if kind == "rate_limit" else None)
        data, raw_chars, files, parts = self._decode(raw)
        return Outcome(data, raw_chars, files, parts, attempts, time.monotonic() - started)

    def _drop(self, server: str) -> None:
        """Forget a dead connection: the next call starts the server again."""
        with self._lock(server):
            client = self._clients.pop(server, None)
        if client is not None:
            try:
                client.close()
            except OSError:
                pass  # it is already gone

    def _save(self, data: str, mime: str) -> str:
        """Write a base64 media part of a result to a file, and give its path."""
        try:
            blob = base64.b64decode(data, validate=False)
        except ValueError:
            return ""
        if len(blob) > MAX_FILE_BYTES:
            return ""
        with self._guard:
            self._files += 1
            number = self._files
        extension = mimetypes.guess_extension(mime or "") or ".bin"
        path = sub("files", safe_name(self.session)) / f"result-{os.getpid()}-{number}{extension}"
        with open(path, "wb") as file:
            file.write(blob)
        return str(path)

    def _decode(self, raw: dict) -> tuple[object, int, list[str], list[dict]]:
        """A tool result as plain data: its text parsed as JSON when it is JSON, its media saved to files."""
        texts: list[str] = []
        files: list[str] = []
        media: list[dict] = []
        for part in raw.get("content") or []:
            if not isinstance(part, dict):
                continue
            kind = part.get("type")
            if kind == "text":
                texts.append(str(part.get("text") or ""))
            elif kind in ("image", "audio") and part.get("data"):
                path = self._save(str(part["data"]), str(part.get("mimeType") or ""))
                files += [path] if path else []
                media.append(part)
            elif kind == "resource" and isinstance(part.get("resource"), dict):
                resource = part["resource"]
                if resource.get("text") is not None:
                    texts.append(str(resource["text"]))
                elif resource.get("blob"):
                    path = self._save(str(resource["blob"]), str(resource.get("mimeType") or ""))
                    files += [path] if path else []
            elif kind == "resource_link":
                texts.append(str(part.get("uri") or ""))
        text = "\n".join(texts)
        raw_chars = len(text) + sum(len(str(part.get("data") or "")) for part in media)
        data: object = text
        if text.strip()[:1] in ("{", "[", '"') or text.strip() in ("true", "false", "null") or re.fullmatch(r"\s*-?\d+(\.\d+)?\s*", text):
            try:
                data = json.loads(text)
            except ValueError:
                data = text
        elif not text and raw.get("structuredContent") is not None:
            data = raw["structuredContent"]
            if isinstance(data, dict) and list(data) == ["result"]:
                data = data["result"]
            raw_chars += len(json.dumps(data, ensure_ascii=False, default=str))
        if files:
            data = {"result": data, "files": files} if data not in ("", None) else {"files": files}
        return data, raw_chars, files, media

    def pmap(self, server: str, tool: str, items: list, limit: int | None = None, allow_write: bool = False,
             stop_on_error: bool = False, record: list | None = None) -> list[object]:
        """The same tool on many argument sets in parallel; a failed item is its ToolFailure, in place.

        Once a server says it is limited or down (after its own retries), nothing more is sent: the remaining items
        come back as "skipped" so the script knows exactly what is left to do.
        """
        stop = threading.Event()
        why = [""]

        def one(position: int) -> object:
            if stop.is_set():
                if record is not None:
                    record.append({"server": server, "tool": tool, "ok": False, "kind": "skipped", "message": why[0][:200],
                                   "retry_after": None, "attempts": 0, "in": 0, "out": 0, "ms": 0})
                return ToolFailure("skipped", server, tool, f"not sent: {why[0]}", attempts=0)
            item = items[position]
            if not isinstance(item, dict):
                return ToolFailure("tool_error", server, tool, f"item {position} is not an object of arguments", attempts=0)
            try:
                return self.call(server, tool, item, allow_write, record=record).data
            except ToolFailure as failure:
                if (failure.kind in STOPPING or stop_on_error) and not stop.is_set():
                    why[0] = f"{failure.kind} at item {position} ({failure.message[:120]})"
                    stop.set()
                return failure

        if not items:
            return []
        workers = max(1, min(int(limit or self.config["pmap_limit"]), len(items), 32))
        with ThreadPoolExecutor(max_workers=workers) as pool:
            return list(pool.map(one, range(len(items))))

    def close(self) -> None:
        """Close every connection (the servers started here stop)."""
        with self._guard:
            clients = list(self._clients.values())
            self._clients.clear()
        for client in clients:
            try:
                client.close()
            except OSError:
                pass  # the server already left
