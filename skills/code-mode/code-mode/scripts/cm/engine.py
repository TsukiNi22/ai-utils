"""The engine behind both entries (MCP tools and command line): search, call, execute, job, recipes.

execute runs the code in a separate process (worker.py). It answers as soon as the code ends; a run still going after
the inline wait goes on as a background job, so one entry covers the short interactive script and the long batch.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import sys
import threading
import time

from . import audit, profiles, recipes, sandbox, sizes
from .hub import Hub, ToolFailure, code_tool
from .paths import compact, load_config, safe_name, script_command, shape, sub, tokens_label

SECRET_ENV = re.compile(r"key|token|secret|passw|credential|auth|cookie|session", re.I)  # never given to the code
OUTPUT_LIMIT = 4000000  # characters of output kept for one job
PASS_MEDIA = 1500000  # base64 characters of images handed over as they are by `call`
MAX_LINES = 25

class EngineError(Exception):
    """Something codemode itself could not do (bad argument, no runner, unknown job)."""

class Job:
    """One run of some code."""

    def __init__(self, ident: str, code: str, allow_write: bool, fresh: bool, recipe: str) -> None:
        self.id = ident
        self.code = code
        self.allow_write = allow_write
        self.fresh = fresh
        self.recipe = recipe
        self.state = "running"
        self.started = time.time()
        self.ended = 0.0
        self.calls: list[dict] = []
        self.error = ""
        self.note = ""
        self.vars: list[str] = []
        self.save_as = ""
        self.description = ""
        self.done = threading.Event()
        self.worker: Worker | None = None
        self._chunks: list[str] = []
        self._size = 0
        self._cursor = 0
        self._lock = threading.Lock()

    def add(self, text: str) -> None:
        with self._lock:
            if self._size < OUTPUT_LIMIT:
                self._chunks.append(text)
            self._size += len(text)

    def take(self) -> str:
        """The output not yet returned to the model."""
        with self._lock:
            text = "".join(self._chunks)
            new, self._cursor = text[self._cursor:], len(text)
            return new

    def size(self) -> int:
        return self._size

class Worker:
    """The child process running the code (worker.py), optionally inside the OS sandbox."""

    def __init__(self, engine: Engine) -> None:
        self.engine = engine
        self.busy = threading.Lock()
        files = sub("files", safe_name(engine.session))
        argv = [*script_command(), "worker"]
        if engine.config.get("sandbox"):
            try:
                argv = sandbox.wrap(argv, engine.cwd, [files], engine.config)
            except sandbox.SandboxError as error:
                raise EngineError(str(error)) from error
        environment = {name: value for name, value in os.environ.items() if not SECRET_ENV.search(name)}
        environment["CODEMODE_FILES"] = str(files)
        environment["PYTHONIOENCODING"] = "utf-8"
        self._log = open(sub("logs") / "worker.log", "ab")
        options: dict = {"creationflags": 0x08000000} if sys.platform == "win32" else {}
        try:
            self.proc = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self._log, cwd=engine.cwd,
                                         env=environment, **options)
        except OSError as error:
            self._log.close()
            raise EngineError(f"cannot start the code runner: {error}") from error
        ready = self._read()
        if not ready or ready.get("op") != "ready":
            self.close()
            raise EngineError("the code runner did not start" + (" (sandbox: see `codemode.py doctor`)" if engine.config.get("sandbox") else ""))

    def _read(self) -> dict | None:
        line = self.proc.stdout.readline()
        if not line:
            return None
        try:
            return json.loads(line.decode("utf-8", "replace"))
        except ValueError:
            return {}

    def _write(self, message: dict) -> None:
        self.proc.stdin.write(json.dumps(message, ensure_ascii=False, default=str).encode("utf-8") + b"\n")
        self.proc.stdin.flush()

    def alive(self) -> bool:
        return self.proc.poll() is None

    def run(self, job: Job, args: dict) -> None:
        """Run the code of a job to its end, serving the tool calls it makes."""
        try:
            self._write({"op": "exec", "id": job.id, "code": job.code, "args": args, "cwd": self.engine.cwd})
            while True:
                message = self._read()
                if message is None:
                    job.error = job.error or "the code runner stopped before the end (killed, crashed or out of memory)"
                    return
                kind = message.get("op")
                if kind == "out":
                    job.add(str(message.get("text") or ""))
                elif kind == "rpc":
                    self._write(self._serve(job, message))
                elif kind == "done":
                    job.error = str(message.get("error") or "")
                    job.vars = [str(name) for name in message.get("vars") or []]
                    return
        except (OSError, ValueError) as error:
            job.error = job.error or f"the code runner stopped before the end ({error})"

    def _serve(self, job: Job, message: dict) -> dict:
        """One tool request of the running code, done by the hub."""
        hub = self.engine.hub
        try:
            if message.get("fn") == "call":
                outcome = hub.call(str(message.get("server")), str(message.get("tool")), message.get("args") or {}, job.allow_write,
                                   record=job.calls)
                return {"op": "reply", "result": outcome.data}
            if message.get("fn") == "pmap":
                results = hub.pmap(str(message.get("server")), str(message.get("tool")), message.get("items") or [],
                                   message.get("limit"), job.allow_write, bool(message.get("stop_on_error")), job.calls)
                return {"op": "reply", "result": [{"__failure__": r.as_dict()} if isinstance(r, ToolFailure) else r for r in results]}
            return {"op": "reply", "error": {"kind": "tool_error", "message": f"unknown request {message.get('fn')}"}}
        except ToolFailure as failure:
            return {"op": "reply", "error": failure.as_dict()}

    def kill(self) -> None:
        try:
            self.proc.kill()
        except OSError:
            pass  # already dead

    def close(self) -> None:
        try:
            self._write({"op": "exit"})
            self.proc.wait(1)
        except (OSError, ValueError, subprocess.TimeoutExpired):
            self.kill()
        try:
            self._log.close()
        except OSError:
            pass  # nothing to save from the log handle

def pluck(data: object, path: str) -> object:
    """One field of a result: `a.b`, `list[].name`, `list[0]`; a list met on the way is walked item by item."""
    tokens = re.findall(r"[^.\[\]]+|\[-?\d*\]|\[\*\]", path)

    def walk(value: object, position: int) -> object:
        if position == len(tokens):
            return value
        token = tokens[position]
        if token in ("[]", "[*]", "*"):
            items = value if isinstance(value, list) else list(value.values()) if isinstance(value, dict) else []
            return [walk(item, position + 1) for item in items]
        if token.startswith("["):
            index = int(token[1:-1])
            return walk(value[index], position + 1) if isinstance(value, list) and -len(value) <= index < len(value) else None
        if isinstance(value, dict):
            return walk(value[token], position + 1) if token in value else None
        if isinstance(value, list):
            return [walk(item, position) for item in value]
        return None

    return walk(data, 0)

def summary(spec: dict) -> str:
    """The first sentence of a tool description."""
    first = str(spec.get("description") or "").strip().split("\n")[0]
    return re.split(r"(?<=[a-z0-9)\]])\. ", first)[0].rstrip(".")[:110]

def schema_lines(spec: dict) -> list[str]:
    """The arguments of a tool, one short line each instead of its raw JSON schema."""
    schema = spec.get("inputSchema") or {}
    required = set(schema.get("required") or [])
    lines = []
    for name, prop in (schema.get("properties") or {}).items():
        prop = prop if isinstance(prop, dict) else {}
        kinds = prop.get("type") or [p.get("type", "?") for p in prop.get("anyOf") or [] if isinstance(p, dict)] or "any"
        kind = "|".join(str(k) for k in kinds if k != "null") if isinstance(kinds, list) else str(kinds)
        if prop.get("enum"):
            kind = "enum(" + "|".join(str(value) for value in prop["enum"][:12]) + ")"
        elif kind == "array" and isinstance(prop.get("items"), dict):
            kind = f"array<{prop['items'].get('type') or 'object'}>"
        nested = prop.get("properties") or (prop.get("items") or {}).get("properties") or (prop.get("items") or {}).get("$ref")
        text = f"  {name}{'' if name in required else '?'}: {kind}"
        text += f" = {compact(prop['default'])}" if prop.get("default") is not None else ""
        text += f" - {' '.join(str(prop['description']).split())[:160]}" if prop.get("description") else ""
        text += f" {compact({key: value for key, value in prop.items() if key != "description"})[:500]}" if nested else ""
        lines.append(text)
    if schema.get("$defs"):
        lines.append(f"  $defs: {compact(schema['$defs'])[:900]}")
    return lines

class Engine:
    """One engine per session: the hub, the persistent code runner, the jobs."""

    def __init__(self, cwd: str, session: str, config: dict | None = None) -> None:
        self.cwd = cwd
        self.session = session
        self.config = config if config is not None else load_config()
        self.hub = Hub(cwd, self.config, session)
        self.jobs: dict[str, Job] = {}
        self.last_request = time.time()
        self._worker: Worker | None = None
        self._guard = threading.Lock()
        self._busy = threading.Lock()  # one piece of code at a time in the shared namespace
        self._count = 0
        self._calls = 0
        self._noted: set[str] = set()  # servers whose profile notes were already shown in this session
        self._loaded: list[dict] | None = None

    def handle(self, op: str, args: dict) -> dict:
        """One request of either entry: {"text", "is_error", "parts"} (parts: images of `call` handed over as they are)."""
        self.last_request = time.time()
        parts: list[dict] = []
        try:
            if op == "search":
                text, failed = self.search(str(args.get("query") or ""), str(args.get("server") or ""), bool(args.get("detail")),
                                           bool(args.get("refresh"))), False
            elif op == "call":
                text, parts, failed = self.call(str(args.get("server") or ""), str(args.get("tool") or ""), args.get("args") or {},
                                                str(args.get("pick") or ""), bool(args.get("allow_write")))
            elif op == "execute":
                text, failed = self.execute(args)
            elif op == "job":
                text, failed = self.job(str(args.get("id") or ""), str(args.get("action") or "status"), args.get("wait"))
            elif op == "recipe":
                text, failed = self.recipe(str(args.get("action") or "list"), str(args.get("name") or "")), False
            elif op == "stats":
                text, failed = audit.stats(None if args.get("all") else self.session), False
            elif op == "status":
                text, failed = self.status(), False
            else:
                text, failed = f"unknown request {op}", True
        except (EngineError, ToolFailure, KeyError, ValueError) as error:
            text, failed = f"codemode: {error.args[0] if error.args else error}", True
        return {"text": text, "is_error": failed, "parts": parts}

    def _fit(self, text: str, data: object, label: str) -> str:
        """A text short enough for the context; the whole of a longer one goes to a file."""
        limit = int(self.config["max_output_chars"])
        if len(text) <= limit:
            return text
        path = sub("files", safe_name(self.session)) / f"{safe_name(label)}.txt"
        with open(path, "w", encoding="utf-8") as file:
            file.write(text)
        head, tail = text[:int(limit * 0.7)], text[-int(limit * 0.15):]
        form = f" | shape: {compact(shape(data))[:600]}" if isinstance(data, (dict, list)) else ""
        return f"{head}\n[... {len(text) - len(head) - len(tail)} characters cut ({tokens_label(len(text))} in all); full text: {path}{form}]\n{tail}"

    def search(self, query: str, server: str = "", detail: bool = False, refresh: bool = False) -> str:
        """Tools (and recipes) matching words or a regex; without a query, the servers and what they hold."""
        tools, errors = self.hub.index(refresh)
        known = sizes.load()
        saved = recipes.index()
        lines: list[str] = []
        extra: list[str] = []
        quiet = False
        if not query.strip():
            for name in sorted(tools):
                if server and name != server:
                    continue
                specs = list(tools[name].values())
                read = sum(self.hub.read_only(name, spec) for spec in specs)
                more = ", ..." if len(specs) > 6 else ""
                lines.append(f"{name} ({len(specs)} tools, {read} read only): {', '.join(spec['name'] for spec in specs[:6])}{more}")
                if server:
                    said = " ".join(self.hub.instructions(name).split())
                    lines += [f"its own instructions: {said[:1500]}"] if said else []
                    lines += self._notes(name, tools[name], True)
            lines += [f"recipes ({len(saved)}): {', '.join(sorted(saved)[:10])}"] if saved and not server else []
        else:
            entries = []
            for name in sorted(tools):
                if server and name != server:
                    continue
                for spec in tools[name].values():
                    names = (spec.get("inputSchema") or {}).get("properties") or {}
                    required = set((spec.get("inputSchema") or {}).get("required") or [])
                    head = f"{name}.{spec['name']}({', '.join(n if n in required else n + '?' for n in names)})"
                    average = sizes.average(known, f"{name}.{spec['name']}")
                    flags = (" [code]" if code_tool(spec) else "") + (" [ro]" if self.hub.read_only(name, spec) else "")
                    flags += f" {tokens_label(average)}" if average else ""
                    entries.append((head, f"{head} - {summary(spec)}{flags}", name, spec))
            entries += [(f"recipe:{name}", recipes.line(name, entry), "", None) for name, entry in sorted(saved.items()) if not server]
            found = self._match(query, entries)
            limit = 5 if detail else MAX_LINES
            for _, text, name, spec in found[:limit]:
                lines.append(text)
                extra += self._notes(name, tools[name], detail) if name else []
                if detail and spec is not None:
                    described = " ".join(str(spec.get("description") or "").split())
                    lines += [f"  {described[:700]}"] if len(described) > len(summary(spec)) + 5 else []
                    lines += schema_lines(spec)
            lines += [f"... {len(found) - limit} more: narrow the query" + ("" if detail else " or name a server")] if len(found) > limit else []
            lines += extra
            quiet = bool(found) and not server
            lines = lines or [f"no tool matches {query!r}" + (f" on {server}" if server else "") + " (an empty query lists the servers)"]
        lines += [f"! {name}: unavailable ({message[:160]})" for name, message in sorted(errors.items())
                  if not quiet and (not server or name == server)]
        lines += [f"! {note}" for note in self.hub.notes] if not quiet else []
        return "\n".join(lines) if lines else "no MCP server is declared (Claude Code, Claude Desktop or the codemode config)"

    def _notes(self, server: str, tools: dict[str, dict], always: bool) -> list[str]:
        """The profile notes of a server: once per session, or each time they are asked for."""
        if not always and server in self._noted:
            return []
        text = profiles.notes(server, list(tools), self._profiles())
        if not text:
            return []
        self._noted.add(server)
        return [f"notes({server}): {text}"]

    def _profiles(self) -> list[dict]:
        if self._loaded is None:
            self._loaded = profiles.load()
        return self._loaded

    def _match(self, query: str, entries: list[tuple]) -> list[tuple]:
        """The entries a query selects: a regex first, its words when it is not one or finds nothing; names first."""
        words = query.lower().split()
        try:
            regex = re.compile(query, re.I)
            found = [entry for entry in entries if regex.search(entry[1])]
        except re.error:
            found = []
        if not found:
            found = [entry for entry in entries if all(word in entry[1].lower() for word in words)]
        return sorted(found, key=lambda entry: not any(word in entry[0].lower() for word in words))

    def call(self, server: str, tool: str, args: dict, pick: str = "", allow_write: bool = False) -> tuple[str, list[dict], bool]:
        """One tool call through the proxy: its result compact, optionally reduced to some fields."""
        if "." in server and not tool:
            server, tool = server.split(".", 1)
        record: list[dict] = []
        self._calls += 1
        try:
            outcome = self.hub.call(server, tool, args, allow_write, record=record)
        except ToolFailure as failure:
            wait = f" (retry after {failure.retry_after:.0f}s)" if failure.retry_after else ""
            tries = f" after {failure.attempts} tries" if failure.attempts > 1 else ""
            self._audit_call(server, tool, record, 0, False)
            return f"{failure.kind}{tries}: {failure.message}{wait}", [], True
        data = outcome.data
        if pick.strip():
            paths = [path.strip() for path in pick.split(",") if path.strip()]
            data = pluck(data, paths[0]) if len(paths) == 1 else {path: pluck(data, path) for path in paths}
        text = self._fit(data if isinstance(data, str) else compact(data), data, f"call-{os.getpid()}-{self._calls}")
        parts = outcome.parts if sum(len(str(part.get("data") or "")) for part in outcome.parts) <= PASS_MEDIA else []
        self._audit_call(server, tool, record, len(text), True)
        return text or "(empty result)", parts, False

    def _audit_call(self, server: str, tool: str, record: list[dict], out: int, success: bool) -> None:
        raw = sum(int(entry.get("out") or 0) for entry in record)
        audit.log({"op": "call", "session": self.session, "server": server, "tool": tool, "ok": success, "out": out, "raw": raw},
                  self.config)

    def _session_worker(self) -> Worker:
        if self._worker is None or not self._worker.alive():
            self._worker = Worker(self)
        return self._worker

    def execute(self, args: dict) -> tuple[str, bool]:
        """Run code (or a recipe); answer at its end, or after the inline wait with the job that goes on."""
        code = str(args.get("code") or "")
        recipe = str(args.get("recipe") or "")
        if recipe:
            code = recipes.load(recipe)
        if not code.strip():
            raise EngineError("execute needs `code` (Python) or `recipe` (a saved name)")
        if args.get("save_as") and not recipes.NAME.match(str(args["save_as"])):
            raise EngineError(f"save_as {args['save_as']!r}: lowercase letters, digits, - and _, 40 characters at most")
        wait = float(args["wait"]) if args.get("wait") not in (None, "") else float(self.config["inline_wait_s"])
        wait = max(1.0, min(wait, 3600.0))
        with self._guard:
            self._count += 1
            job = Job(f"j{self._count}", code, bool(args.get("allow_write")), bool(args.get("fresh")), recipe)
        job.save_as = str(args.get("save_as") or "")
        job.description = str(args.get("description") or "")
        if job.fresh:
            worker = Worker(self)
        else:
            if not self._busy.acquire(timeout=wait):
                running = next((other.id for other in self.jobs.values() if other.state == "running"), "?")
                raise EngineError(f"the code runner is busy with job {running}: wait for it (job), kill it, or pass fresh=true for a separate run")
            try:
                worker = self._session_worker()
            except EngineError:
                self._busy.release()
                raise
        job.worker = worker
        self.jobs[job.id] = job
        threading.Thread(target=self._run, args=(job, worker, args.get("args") or {}), name=f"job-{job.id}", daemon=True).start()
        finished = job.done.wait(wait)
        if finished:
            return self._final(job)
        text = job.take()
        text += "" if not text or text.endswith("\n") else "\n"
        return (f"{text}[job {job.id} still running after {wait:.0f}s ({len(job.calls)} tool calls so far): `job` with this id returns "
                f"what it printed since, action=kill stops it]"), False

    def _run(self, job: Job, worker: Worker, args: dict) -> None:
        """Thread of a job: run it, time it out, record it."""
        timer = threading.Timer(float(self.config["job_timeout_s"]), self._expire, args=(job,))
        timer.daemon = True
        timer.start()
        try:
            worker.run(job, args)
        finally:
            timer.cancel()
            job.ended = time.time()
            if job.state == "running":
                job.state = "failed" if job.error else "done"
            if job.fresh:
                worker.close()
            else:
                if not worker.alive():
                    self._worker = None
                    job.note = "the code runner was restarted: the variables of the earlier runs are lost"
                    job.vars = []
                self._busy.release()
            if job.save_as and job.state == "done":
                try:
                    job.note = (job.note + " | " if job.note else "") + recipes.save(job.save_as, job.code, job.description, self.config)
                except (ValueError, OSError) as error:
                    job.note = (job.note + " | " if job.note else "") + f"recipe not saved: {error}"
            self._audit_job(job)
            job.done.set()

    def _expire(self, job: Job) -> None:
        if job.state == "running" and job.worker is not None:
            job.state = "killed"
            job.error = f"stopped after {float(self.config['job_timeout_s']):.0f}s (job_timeout_s)"
            job.worker.kill()

    def _audit_job(self, job: Job) -> None:
        calls = [[c["server"], c["tool"], 1 if c["ok"] else 0, c["kind"], c["in"], c["out"], c["ms"], c["attempts"]] for c in job.calls]
        limit = int(self.config.get("audit", {}).get("code_chars", 4000))
        audit.log({"op": "execute", "session": self.session, "job": job.id, "recipe": job.recipe, "state": job.state,
                   "write": job.allow_write, "fresh": job.fresh, "sandbox": bool(self.config.get("sandbox")),
                   "ms": int((job.ended - job.started) * 1000), "out": job.size(), "code_chars": len(job.code),
                   "sha": hashlib.sha256(job.code.encode("utf-8")).hexdigest()[:12], "code": job.code[:limit],
                   "error": job.error[:300], "calls": calls[:500], "calls_total": len(calls)}, self.config)

    def _footer(self, job: Job) -> str:
        """What failed, was retried or was not sent, and what the namespace still holds: enough to resume."""
        good = sum(1 for call in job.calls if call["ok"])
        skipped = sum(1 for call in job.calls if call["kind"] == "skipped")
        bad = [call for call in job.calls if not call["ok"] and call["kind"] != "skipped"]
        retried = sum(1 for call in job.calls if call["ok"] and call["attempts"] > 1)
        if not bad and not skipped and not retried and not job.error:
            return ""
        parts = [f"calls: {good} ok" + (f", {len(bad)} failed" if bad else "") + (f", {skipped} not sent" if skipped else "")
                 + (f", {retried} ok after a retry" if retried else "")]
        groups: dict[tuple, list[dict]] = {}
        for call in bad:
            groups.setdefault((call["server"], call["tool"], call["kind"]), []).append(call)
        for (server, tool, kind), members in list(groups.items())[:4]:
            waits = [float(call["retry_after"]) for call in members if call.get("retry_after")]
            wait = f", retry after {max(waits):.0f}s" if waits else ""
            parts.append(f"{server}.{tool} x{len(members)} {kind}: {members[0]['message'][:120]!r}{wait}")
        if job.vars:
            parts.append("variables kept: " + ", ".join(job.vars[:12]))
        return "[" + " | ".join(parts) + "]"

    def _final(self, job: Job) -> tuple[str, bool]:
        """The report of a finished job: what it printed and not yet returned, its error, its footer."""
        text = self._fit(job.take(), None, f"job-{os.getpid()}-{job.id}")
        blocks = [text.rstrip("\n")] if text.strip() else []
        if job.error:
            blocks.append(f"ERROR: {job.error}")
        elif not blocks:
            blocks.append("(no output: print what you need)")
        footer = self._footer(job)
        blocks += [footer] if footer else []
        blocks += [f"[{job.note}]"] if job.note else []
        return "\n".join(blocks), bool(job.error)

    def job(self, ident: str, action: str = "status", wait: object = None) -> tuple[str, bool]:
        """Follow, wait for, kill or list the jobs."""
        if action == "list" or not ident:
            if not self.jobs:
                return "no job in this session", False
            return "\n".join(f"{job.id} {job.state} {time.time() - job.started if job.state == 'running' else job.ended - job.started:.0f}s"
                             f" {len(job.calls)} calls {job.size()} chars printed" for job in self.jobs.values()), False
        job = self.jobs.get(ident)
        if job is None:
            raise EngineError(f"no job {ident} in this session (known: {', '.join(self.jobs) or 'none'})")
        if action == "kill":
            if job.state == "running" and job.worker is not None:
                job.state = "killed"
                job.error = "killed on request"
                job.worker.kill()
                job.done.wait(5)
            return self._final(job)
        seconds = float(wait) if wait not in (None, "") else (float(self.config["inline_wait_s"]) if action == "wait" else 0.0)
        if job.done.wait(max(0.0, min(seconds, 3600.0))):
            return self._final(job)
        text = job.take()
        text += "" if not text or text.endswith("\n") else "\n"
        return f"{text}[job {job.id} still running: {time.time() - job.started:.0f}s, {len(job.calls)} tool calls so far]", False

    def recipe(self, action: str, name: str = "") -> str:
        """List, show, remove or prune the saved recipes (running one is execute with recipe=)."""
        if action == "show":
            with open(recipes.folder() / f"{safe_name(name)}.py", "r", encoding="utf-8") as file:
                return file.read()
        if action in ("rm", "remove", "delete"):
            return f"recipe {name} removed" if recipes.remove(name) else f"no recipe named {name}"
        if action == "prune":
            removed = recipes.prune(self.config)
            return "removed: " + ", ".join(removed) if removed else "nothing to remove"
        known = recipes.index()
        return "\n".join(recipes.line(key, entry) for key, entry in sorted(known.items())) or "no recipe saved yet"

    def status(self) -> str:
        """Where this engine stands: its servers, runner and jobs."""
        servers = self.hub.servers()
        connected = sorted(name for name, client in self.hub._clients.items() if not client.connection.closed)
        running = [job.id for job in self.jobs.values() if job.state == "running"]
        declared = ", ".join(f"{name} [{spec.get('source')}]" for name, spec in sorted(servers.items())) or "none"
        lines = [f"codemode session {self.session} (pid {os.getpid()}) in {self.cwd}",
                 f"servers: {len(servers)} declared ({declared})",
                 f"connected now: {', '.join(connected) or 'none'}",
                 f"code runner: {'alive' if self._worker is not None and self._worker.alive() else 'not started'}"
                 f" | sandbox: {'on' if self.config.get('sandbox') else 'off'} | jobs running: {', '.join(running) or 'none'}"]
        return "\n".join(lines + [f"! {note}" for note in self.hub.notes])

    def idle(self) -> bool:
        """Nothing running: the daemon of an unused session may stop."""
        return not any(job.state == "running" for job in self.jobs.values())

    def close(self) -> None:
        """Stop the runner, the running jobs and every server started here."""
        for job in self.jobs.values():
            if job.state == "running" and job.worker is not None:
                job.worker.kill()
        if self._worker is not None:
            self._worker.close()
            self._worker = None
        self.hub.close()
