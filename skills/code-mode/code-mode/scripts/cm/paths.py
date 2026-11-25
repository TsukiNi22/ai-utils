"""Paths, configuration and small helpers shared by every part of codemode (standard library only)."""
from __future__ import annotations

import json
import math
import os
import re
import sys
import tempfile
import time
from pathlib import Path

VERSION = "1.0.0"
PROTOCOL = "2025-06-18"  # the MCP revision spoken first; a server or client asking an older one gets it
PROTOCOLS = ("2025-06-18", "2025-03-26", "2024-11-05")
CHARS_PER_TOKEN = 3.3  # measured on the JSON results of the bench (tiktoken o200k); an estimate, said as such
SCRIPT = Path(__file__).resolve().parent.parent / "codemode.py"
PRIORITIES = ("tokens", "cost")
PRESETS: dict = {  # the hook thresholds of each priority (a value set in config.json wins)
    "tokens": {},
    "cost": {"remind_after": 2, "compress_over_tokens": 6000},  # switch to code sooner, cut later (a preview may cost a re-read)
}
RULES: dict = {  # what each priority asks of the model, one line (search, instructions, status)
    "tokens": "tokens first (subscription): keep the context small - one script for 3+ calls, print only what the answer needs; "
              "a probe of one result before the real run is fine.",
    "cost": "$ first (API): fewer API calls, each one re-reads the whole context - probe and work in ONE script (defensive .get()), "
            "independent searches and calls in the same turn, one call with pick for a single big result, a short final answer.",
}
DEFAULTS: dict = {
    "priority": "tokens",  # tokens (subscription: the context) or cost (API: the $, fewer API calls); asked at setup
    "inline_wait_s": 40,  # an execute still running after this goes on as a background job
    "call_timeout_s": 300,  # one tool call
    "connect_timeout_s": 45,  # start + initialize of a server
    "job_timeout_s": 3600,  # a background job is killed after this
    "max_output_chars": 12000,  # a longer output goes to a file, its head is returned
    "big_result_tokens": 1500,  # a tool known to return more is flagged in search
    "compress_over_tokens": 2500,  # hook: a direct MCP result over this is replaced by a preview
    "preview_chars": 2000,  # the size of that preview
    "remind_after": 3,  # hook: direct calls in a row on one server before the reminder
    "retry_attempts": 3,  # tries of one call on a usage limit / transient failure
    "retry_max_wait_s": 30,  # a longer wait is not slept: the failure is reported with its delay
    "pmap_limit": 8,  # parallel calls of one pmap
    "daemon_idle_s": 1800,  # the session daemon of the command line stops after this without a request
    "index_retry_s": 600,  # a server that failed to list its tools is tried again after this
    "sandbox": False,  # run the code inside the OS sandbox (bubblewrap / sandbox-exec); asked at setup
    "sandbox_hide": [],  # more paths hidden from the sandboxed code (every hidden entry of the home already is)
    "sandbox_show": [],  # hidden paths the sandboxed code may read after all
    "sandbox_writable": [],  # more paths the sandboxed code may write
    "recipes": {"ttl_days": 30, "max_bytes": 5000000, "max_count": 200},
    "audit": {"max_bytes": 10000000, "code_chars": 4000},
    "rules": {"deny": [], "read_only": [], "write": []},  # patterns "server.tool" (fnmatch), on top of read only by default
    "sources": {"claude_code": True, "claude_desktop": True},
    "disabled_servers": [],
}

def home() -> Path:
    """The data folder of codemode (config, index, recipes, audit, jobs), made on first use."""
    given = os.environ.get("CODEMODE_HOME")
    if given:
        base = Path(given).expanduser()
    elif sys.platform == "win32":
        base = Path(os.environ.get("LOCALAPPDATA") or Path.home() / "AppData" / "Local") / "codemode"
    elif sys.platform == "darwin":
        base = Path.home() / "Library" / "Application Support" / "codemode"
    else:
        base = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local" / "share") / "codemode"
    base.mkdir(parents=True, exist_ok=True)
    return base

def sub(*parts: str) -> Path:
    """A folder inside the data folder, made on first use (0700: it may hold results and tokens)."""
    path = home().joinpath(*parts)
    path.mkdir(parents=True, exist_ok=True)
    try:
        path.chmod(0o700)
    except OSError:
        pass  # a filesystem without modes (Windows): the user profile already protects it
    return path

def read_json(path: Path, default: object) -> object:
    """The JSON of a file, or the default when it is missing or broken (never a crash on a config)."""
    try:
        with open(path, "r", encoding="utf-8") as file:
            return json.load(file)
    except (OSError, ValueError):
        return default

def write_json(path: Path, data: object, mode: int = 0o600) -> None:
    """Write a JSON file in one step (a reader never sees half of it)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temp = tempfile.mkstemp(dir=str(path.parent), prefix=path.name + ".", suffix=".tmp")
    try:
        with os.fdopen(handle, "w", encoding="utf-8") as file:
            json.dump(data, file, ensure_ascii=False, indent=1)
        try:
            os.chmod(temp, mode)
        except OSError:
            pass  # no modes on this filesystem
        os.replace(temp, path)
    except OSError:
        try:
            os.unlink(temp)
        except OSError:
            pass  # the temporary file is already gone
        raise

def merge(base: dict, over: dict) -> dict:
    """The defaults with the values of the user over them (dicts merged key by key)."""
    out = dict(base)
    for key, value in over.items():
        out[key] = merge(out[key], value) if isinstance(value, dict) and isinstance(out.get(key), dict) else value
    return out

def session_priority(session: str | None) -> str:
    """The priority set for one session only (`codemode.py priority cost`), or nothing."""
    if not session:
        return ""
    value = read_json(home() / "sessions" / f"{safe_name(session)}.json", {})
    value = value.get("priority") if isinstance(value, dict) else ""
    return value if value in PRIORITIES else ""

def set_session_priority(session: str, priority: str | None) -> None:
    """Set (or clear, with None) the priority of one session."""
    path = sub("sessions") / f"{safe_name(session)}.json"
    if priority is None:
        try:
            path.unlink()
        except OSError:
            pass  # there was none
        return
    write_json(path, {"priority": priority, "set": time.time()})

def priority_of(session: str | None, user: dict | None = None) -> tuple[str, str]:
    """The priority in force and where it comes from: the session, config.json or the default."""
    user = read_json(home() / "config.json", {}) if user is None else user
    own = session_priority(session)
    if own:
        return own, "this session"
    value = user.get("priority") if isinstance(user, dict) else None
    return (value, "config") if value in PRIORITIES else ("tokens", "default")

def load_config(session: str | None = None) -> dict:
    """The defaults, then the thresholds of the priority in force, then config.json (what the user set wins)."""
    user = read_json(home() / "config.json", {})
    user = user if isinstance(user, dict) else {}
    priority, _ = priority_of(session, user)
    merged = merge(merge(DEFAULTS, PRESETS[priority]), user)
    merged["priority"] = priority
    return merged

def save_config(changes: dict) -> None:
    """Keep these values in config.json (only what differs from the defaults is stored)."""
    path = home() / "config.json"
    user = read_json(path, {})
    write_json(path, merge(user if isinstance(user, dict) else {}, changes))

def est_tokens(chars: int) -> int:
    """About how many tokens a text of this many characters costs."""
    return int(math.ceil(chars / CHARS_PER_TOKEN))

def tokens_label(chars: int) -> str:
    """'~850 tok' / '~1.3k tok'."""
    tokens = est_tokens(chars)
    return f"~{tokens / 1000:.1f}k tok" if tokens >= 1000 else f"~{tokens} tok"

def session_key() -> str:
    """What names the current session: given, else the Claude Code session, else one per terminal folder."""
    return os.environ.get("CODEMODE_SESSION") or os.environ.get("CLAUDE_CODE_SESSION_ID") or "default"

def safe_name(text: str, limit: int = 64) -> str:
    """A text usable as a file name."""
    return re.sub(r"[^A-Za-z0-9._-]+", "_", text).strip("._")[:limit] or "x"

def pid_alive(pid: int) -> bool:
    """Whether a process still exists (POSIX signal 0, Windows OpenProcess)."""
    if pid <= 0:
        return False
    if sys.platform == "win32":
        import ctypes  # only needed on Windows
        handle = ctypes.windll.kernel32.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
        if not handle:
            return False
        code = ctypes.c_ulong()
        alive = bool(ctypes.windll.kernel32.GetExitCodeProcess(handle, ctypes.byref(code))) and code.value == 259
        ctypes.windll.kernel32.CloseHandle(handle)
        return alive
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True  # it exists, owned by someone else
    return True

def script_command() -> list[str]:
    """The command that runs codemode.py again with this Python (the installed link when it leads here)."""
    linked = Path.home() / ".claude" / "skills" / "code-mode" / "scripts" / "codemode.py"
    try:
        script = linked if linked.exists() and linked.resolve() == SCRIPT else SCRIPT
    except OSError:
        script = SCRIPT
    return [sys.executable, str(script)]

def now() -> float:
    return time.time()

def stamp(when: float | None = None) -> str:
    """A local date and time, short."""
    return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(when if when is not None else time.time()))

def compact(data: object) -> str:
    """Compact JSON for a model: no spaces, no escaped unicode."""
    return json.dumps(data, ensure_ascii=False, separators=(",", ":"), default=str)

def shape(value: object, depth: int = 0) -> object:
    """The structure of a value without its content: keys, types, list lengths (2 levels)."""
    if isinstance(value, dict):
        if depth >= 2:
            return "{%d keys}" % len(value)
        return {str(key): shape(item, depth + 1) for key, item in list(value.items())[:30]}
    if isinstance(value, list):
        return [shape(value[0], depth + 1), "x%d" % len(value)] if value else []
    return type(value).__name__
