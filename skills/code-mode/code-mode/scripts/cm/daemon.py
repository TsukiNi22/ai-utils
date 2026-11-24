"""The command line talks to one engine per session over a local socket (127.0.0.1 and a token kept in a 0600 file).

That engine is the MCP server of the session when codemode is registered in the client (same state for both entries),
otherwise a small daemon started at the first command; it stops with the session that owns it, or after a while
without a request. Nothing listens outside the computer.
"""
from __future__ import annotations

import hmac
import json
import os
import secrets
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

from .engine import Engine
from .paths import load_config, pid_alive, read_json, safe_name, script_command, sub, write_json

MAX_REQUEST = 20000000  # bytes of one request line (a script and its arguments)
START_WAIT = 20.0  # seconds to wait for a daemon just started

class NoEngine(Exception):
    """No engine runs for this session (and none was to be started)."""

def run_file(key: str) -> Path:
    return sub("run") / f"{safe_name(key)}.json"

class Control(threading.Thread):
    """The local socket of an engine: one JSON request per connection, one JSON answer."""

    def __init__(self, engine: Engine, key: str, kind: str) -> None:
        super().__init__(name="control", daemon=True)
        self.engine = engine
        self.key = key
        self.stop_requested = threading.Event()
        self._token = secrets.token_hex(24)
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.bind(("127.0.0.1", 0))
        self._socket.listen(16)
        write_json(run_file(key), {"pid": os.getpid(), "port": self._socket.getsockname()[1], "token": self._token, "kind": kind,
                                   "cwd": engine.cwd, "started": time.time()})

    def run(self) -> None:
        while True:
            try:
                connection, _ = self._socket.accept()
            except OSError:
                return  # closed: the engine is stopping
            threading.Thread(target=self._serve, args=(connection,), daemon=True).start()

    def _serve(self, connection: socket.socket) -> None:
        with connection:
            try:
                stream = connection.makefile("rwb")
                request = json.loads(stream.readline(MAX_REQUEST).decode("utf-8", "replace"))
                if not hmac.compare_digest(str(request.get("token") or ""), self._token):
                    answer = {"text": "codemode: wrong token", "is_error": True, "parts": []}
                elif request.get("op") == "stop":
                    answer = {"text": "codemode: engine of this session stopped", "is_error": False, "parts": []}
                    self.stop_requested.set()
                else:
                    answer = self.engine.handle(str(request.get("op") or ""), request.get("args") or {})
                answer["parts"] = []  # images stay files on the command line
                stream.write(json.dumps(answer, ensure_ascii=False, default=str).encode("utf-8") + b"\n")
                stream.flush()
            except (OSError, ValueError):
                return  # the client left or sent garbage

    def close(self) -> None:
        """Stop listening and remove the run file when it is still ours."""
        info = read_json(run_file(self.key), {})
        if isinstance(info, dict) and info.get("pid") == os.getpid():
            try:
                run_file(self.key).unlink()
            except OSError:
                pass  # already removed
        try:
            self._socket.close()
        except OSError:
            pass  # already closed

def locate(key: str) -> dict | None:
    """The engine of a session when its process is still there."""
    info = read_json(run_file(key), None)
    if not isinstance(info, dict) or not pid_alive(int(info.get("pid") or 0)):
        try:
            run_file(key).unlink()
        except OSError:
            pass  # there was none
        return None
    return info

def spawn(key: str, cwd: str) -> None:
    """Start the daemon of a session, detached from the command that asked for it."""
    owner = os.environ.get("CLAUDE_PID") or "0"
    argv = [*script_command(), "--session", key, "daemon", "--cwd", cwd, "--owner", owner]
    with open(sub("logs") / "daemon.log", "ab") as log:
        options: dict = {"creationflags": 0x08000208} if sys.platform == "win32" else {"start_new_session": True}
        subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=log, stderr=log, cwd=cwd, close_fds=True, **options)

def exchange(info: dict, op: str, args: dict, timeout: float) -> dict:
    with socket.create_connection(("127.0.0.1", int(info["port"])), timeout=10) as connection:
        connection.settimeout(timeout)
        stream = connection.makefile("rwb")
        stream.write(json.dumps({"token": info["token"], "op": op, "args": args}, ensure_ascii=False, default=str).encode("utf-8") + b"\n")
        stream.flush()
        line = stream.readline()
    if not line:
        raise OSError("the engine closed the connection without an answer")
    return json.loads(line.decode("utf-8", "replace"))

def request(key: str, op: str, args: dict, cwd: str, autostart: bool = True) -> dict:
    """Send one request to the engine of the session, starting its daemon when there is none."""
    config = load_config()
    waits = [float(args.get("wait") or 0), float(config["inline_wait_s"]), float(config["connect_timeout_s"]) * 2]
    timeout = max(waits) + 60.0
    info = locate(key)
    for attempt in range(2):
        if info is None:
            if not autostart:
                raise NoEngine(f"no codemode engine runs for session {key}")
            spawn(key, cwd)
            deadline = time.time() + START_WAIT
            while info is None and time.time() < deadline:
                time.sleep(0.1)
                info = locate(key) if run_file(key).is_file() else None
            if info is None:
                raise NoEngine(f"the codemode daemon did not start (see {sub('logs') / 'daemon.log'})")
        try:
            return exchange(info, op, args, timeout)
        except ConnectionRefusedError:
            if attempt:
                raise
            info = None  # a run file left by a process that no longer listens
            try:
                run_file(key).unlink()
            except OSError:
                pass  # already removed
    raise NoEngine("the codemode engine does not answer")

def run_daemon(key: str, cwd: str, owner: int) -> int:
    """The daemon of a session: one engine, until its owner ends or nothing was asked for a while."""
    engine = Engine(cwd, key)
    control = Control(engine, key, "daemon")
    control.start()
    idle = float(engine.config["daemon_idle_s"])
    while not control.stop_requested.wait(3):
        if owner and not pid_alive(owner):
            break
        if engine.idle() and time.time() - engine.last_request > idle:
            break
    control.close()
    engine.close()
    return 0

def engines() -> list[dict]:
    """Every engine running on this computer (for status)."""
    found = []
    for path in sorted(sub("run").glob("*.json")):
        if path.name.startswith("hook-"):
            continue
        info = read_json(path, None)
        if isinstance(info, dict) and pid_alive(int(info.get("pid") or 0)):
            found.append({"session": path.stem, **{name: info.get(name) for name in ("pid", "kind", "cwd", "started")}})
    return found
