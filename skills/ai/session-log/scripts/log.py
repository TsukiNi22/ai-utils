#!/usr/bin/env python3
"""Log the progress of a Claude session to Discord, a file, an API or a socket - several at once.

    log.py start SINK [SINK...] [--name N] [--level minimal|key|all] [--lang fr|en] [--no-summary]
    log.py send TEXT [--kind step|done|info|question|error] [--file PATH]... [--link URL]
    log.py status TEXT                  # the live status (one Discord message edited in place)
    log.py stop [SINK_ID|all] [--text TEXT]
    log.py list
    log.py hooks install|uninstall|show [--settings FILE]
    log.py hook                         # what the Claude Code hooks call (event JSON on stdin)

SINK: a Discord webhook URL, an https:// address (an API: one JSON POST per event), tcp://HOST:PORT or udp://HOST:PORT
(one JSON line per event), a file (.log: text lines, .jsonl: JSON lines), a folder ending with / (a log file + a copy
of each file sent). Options after #, comma separated: level=minimal|key|all, mention=<user id>|role:<id> (Discord ping
when a question or a permission waits), thread=no, maxmb=N (Discord attachments, 10 by default), upload (API: files
sent in a multipart body), header=Name:Value;Name:Value (API).
The session is $CLAUDE_CODE_SESSION_ID (--session for another); the state is in ~/.local/state/session-log/<session>/
($SESSION_LOG_DIR), the config 0600 (a webhook URL is a secret). Events are queued and sent in order by one worker in
the background; the request and the end of each turn are summarized by Haiku (claude -p, a separate process, no fork).
"""
import argparse
import fcntl
import hashlib
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
from pathlib import Path

SCRIPT = Path(__file__).absolute()
STATE = Path(os.environ.get("SESSION_LOG_DIR", Path.home() / ".local" / "state" / "session-log")).expanduser()
DISCORD = re.compile(r"^https?://[^/]+/api(?:/v\d+)?/webhooks/\d+/[\w-]+$")
LEVELS = {"minimal": {"start", "stop", "done", "question", "waiting", "error"}}
LEVELS["key"] = LEVELS["minimal"] | {"request", "step", "file", "turn", "info"}
LEVELS["all"] = LEVELS["key"] | {"status"}
PING = ("question", "waiting")
COLOURS = {"start": 0x5865F2, "stop": 0x747F8D, "request": 0x3B82F6, "step": 0x22C55E, "done": 0x8B5CF6, "question": 0xF59E0B,
           "waiting": 0xF59E0B, "error": 0xEF4444, "turn": 0x14B8A6, "file": 0x06B6D4, "info": 0x94A3B8, "status": 0x64748B}
LABELS = {
    "fr": {"start": "Log démarré", "stop": "Log arrêté", "request": "Demande reçue", "step": "Étape", "done": "Terminé",
           "question": "Question en attente", "waiting": "En attente", "error": "Erreur", "turn": "Fin de tour", "file": "Fichier",
           "info": "Info", "status": "Statut", "steps": "étapes", "errors": "erreurs", "last": "dernière activité",
           "more_errors": "autres erreurs", "too_big": "trop gros pour être joint", "working": "En cours"},
    "en": {"start": "Log started", "stop": "Log stopped", "request": "Request received", "step": "Step", "done": "Done",
           "question": "Question waiting", "waiting": "Waiting", "error": "Error", "turn": "End of turn", "file": "File",
           "info": "Info", "status": "Status", "steps": "steps", "errors": "errors", "last": "last activity",
           "more_errors": "more errors", "too_big": "too big to attach", "working": "Working"},
}
SECRETS = [
    (re.compile(r"https?://[^\s/]+/api(?:/v\d+)?/webhooks/\d+/[\w-]+"), "[webhook]"),
    (re.compile(r"(?i)\b(api[_-]?key|token|secret|password|passwd|authorization)(\s*[=:]\s*)(?:bearer\s+)?\S+"), r"\1\2[hidden]"),
    (re.compile(r"\b(sk-[\w-]{20,}|gh[pousr]_\w{20,}|xox[abprs]-[\w-]{10,}|AKIA[0-9A-Z]{16}|eyJ[\w-]{10,}\.[\w-]{10,}\.[\w-]+)"), "[hidden]"),
    (re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----.*?-----END [A-Z ]*PRIVATE KEY-----", re.S), "[private key]"),
]
HOOK_EVENTS = [("UserPromptSubmit", None), ("PreToolUse", "AskUserQuestion"), ("Notification", None), ("PostToolUseFailure", None),
               ("Stop", None), ("StopFailure", None), ("SessionEnd", None)]
ERROR_GAP = 60  # seconds: at level key, one error message per minute, the others counted in the status
AGENT = "session-log/1.0"

# ======================================================================= state
def folder(session: str) -> Path:
    if not re.fullmatch(r"[\w-]{4,80}", session or ""):
        sys.exit("session-log: no session id (run it from a Claude Code session, or give --session)")
    return STATE / session

class Locked:
    """An exclusive lock on a file of the session folder (the config is read and written by the CLI and the worker)."""

    def __init__(self, path: Path) -> None:
        self.path = path

    def __enter__(self) -> "Locked":
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.handle = open(self.path, "a", encoding="utf-8")
        fcntl.flock(self.handle, fcntl.LOCK_EX)
        return self

    def __exit__(self, *_: object) -> None:
        fcntl.flock(self.handle, fcntl.LOCK_UN)
        self.handle.close()

def load(base: Path) -> dict | None:
    file = base / "config.json"
    return json.loads(file.read_text(encoding="utf-8")) if file.is_file() else None

def save(base: Path, config: dict) -> None:
    base.mkdir(parents=True, exist_ok=True, mode=0o700)
    file = base / "config.json"
    temp = file.with_suffix(".tmp")
    temp.write_text(json.dumps(config, indent=2, ensure_ascii=False), encoding="utf-8")
    temp.chmod(0o600)
    temp.replace(file)

def note_error(base: Path, sink: str, error: str) -> None:
    with open(base / "errors.log", "a", encoding="utf-8") as log:
        log.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')} {sink}: {error}\n")

def redact(text: str) -> str:
    for pattern, hidden in SECRETS:
        text = pattern.sub(hidden, text)
    return text

def slug(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")[:50] or "session"

# ======================================================================= sinks
def parse_sink(spec: str, number: int) -> dict:
    """A sink from its text: the kind is read from its shape, the options from what follows '#'."""
    raw, _, fragment = spec.partition("#")
    options = {}
    for part in filter(None, (p.strip() for p in fragment.split(","))):
        key, _, value = part.partition("=")
        options[key.strip()] = value.strip() or "1"
    if options.get("level", "key") not in LEVELS:
        sys.exit(f"session-log: level must be one of {', '.join(LEVELS)}")
    sink = {"id": f"s{number}", "options": options}
    local = re.match(r"^http://(127\.0\.0\.1|localhost|\[::1\])[:/]", raw)
    if DISCORD.match(raw):
        if raw.startswith("http://") and not local:
            sys.exit("session-log: a Discord webhook goes over https")
        return {**sink, "kind": "discord", "url": raw}
    if raw.startswith("https://") or local:
        return {**sink, "kind": "api", "url": raw}
    if raw.startswith("http://"):
        sys.exit(f"session-log: {raw}: plain http only to this computer (127.0.0.1); use https")
    match = re.fullmatch(r"(tcp|udp)://(\[[^\]]+\]|[^:/]+):(\d{1,5})", raw)
    if match:
        return {**sink, "kind": match.group(1), "host": match.group(2).strip("[]"), "port": int(match.group(3))}
    if re.match(r"^[a-z]+://", raw):
        sys.exit(f"session-log: {raw}: unknown sink (Discord webhook, https, tcp://, udp://, a file or a folder/)")
    path = Path(raw).expanduser().absolute()
    if raw.endswith("/") or path.is_dir():
        return {**sink, "kind": "dir", "path": str(path)}
    return {**sink, "kind": "file", "path": str(path)}

def shown(sink: dict) -> str:
    """A sink as it can be printed: the token of a webhook, the query and the headers of an API hidden."""
    if sink["kind"] == "discord":
        return re.sub(r"/[\w-]+$", "/****", sink["url"])
    if sink["kind"] == "api":
        parts = urllib.parse.urlsplit(sink["url"])
        return f"{parts.scheme}://{parts.netloc}{parts.path}" + (" (+headers)" if sink["options"].get("header") else "")
    if sink["kind"] in ("tcp", "udp"):
        return f"{sink['kind']}://{sink['host']}:{sink['port']}"
    return sink["path"] + ("/" if sink["kind"] == "dir" else "")

def request(method: str, url: str, body: bytes, kind: str, headers: dict | None = None) -> dict:
    """One HTTP call; a 429 waits the time asked (3 tries). The JSON answer, {} when there is none."""
    for _ in range(3):
        call = urllib.request.Request(url, data=body, method=method, headers={"Content-Type": kind, "User-Agent": AGENT, **(headers or {})})
        try:
            with urllib.request.urlopen(call, timeout=20) as answer:
                text = answer.read()
            return json.loads(text) if text.strip().startswith(b"{") else {}
        except urllib.error.HTTPError as error:
            detail = error.read()[:300].decode("utf-8", "replace")
            if error.code == 429:
                try:
                    time.sleep(min(30.0, float(json.loads(detail).get("retry_after", 2))))
                except (ValueError, AttributeError):
                    time.sleep(2)
                continue
            raise RuntimeError(f"HTTP {error.code} {detail}") from None
    raise RuntimeError("HTTP 429 (rate limited)")

def multipart(fields: dict, files: list[Path], prefix: str) -> tuple[bytes, str]:
    """A multipart/form-data body: the fields, then the files as <prefix>[n] (Discord: files[0], files[1]...)."""
    boundary = uuid.uuid4().hex
    out = b""
    for name, value in fields.items():
        out += f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"\r\nContent-Type: application/json\r\n\r\n'.encode() + value.encode() + b"\r\n"
    for index, file in enumerate(files):
        head = f'--{boundary}\r\nContent-Disposition: form-data; name="{prefix}[{index}]"; filename="{file.name}"\r\nContent-Type: application/octet-stream\r\n\r\n'
        out += head.encode() + file.read_bytes() + b"\r\n"
    return out + f"--{boundary}--\r\n".encode(), f"multipart/form-data; boundary={boundary}"

def file_info(path: str) -> dict:
    file = Path(path)
    if not file.is_file():
        return {"name": file.name, "path": path, "missing": True}
    return {"name": file.name, "path": str(file), "size": file.stat().st_size, "sha256": hashlib.sha256(file.read_bytes()).hexdigest()}

def discord_url(sink: dict, extra: str = "", **params: str) -> str:
    query = {k: v for k, v in params.items() if v}
    if sink.get("thread_id"):
        query["thread_id"] = sink["thread_id"]
    return sink["url"] + extra + ("?" + urllib.parse.urlencode(query) if query else "")

def discord(sink: dict, event: dict, config: dict, words: dict) -> None:
    """A message with one embed (its colour by kind), the mention when it waits for the user, the files attached
    under the size limit (the others named); a forum channel gets one post per session at the first message."""
    text = event["text"]
    big = [f for f in event.get("files", []) if Path(f).is_file() and Path(f).stat().st_size > float(sink["options"].get("maxmb", 10)) * (1 << 20)]
    small = [Path(f) for f in event.get("files", []) if Path(f).is_file() and f not in big]
    if big:
        text += "\n" + "\n".join(f"`{Path(f).name}`: {words['too_big']} ({f})" for f in big)
    embed = {"title": event.get("title") or words.get(event["kind"], event["kind"]), "description": text[:4000],
             "color": COLOURS.get(event["kind"], 0x94A3B8), "footer": {"text": f"{config['name']} · {time.strftime('%H:%M:%S', time.localtime(event['time']))}"}}
    if event.get("link"):
        embed["url"] = event["link"]
    payload: dict = {"username": f"Claude · {config['name']}"[:80], "embeds": [embed], "allowed_mentions": {"parse": []}}
    mention = sink["options"].get("mention", "")
    if event["kind"] in PING and mention:
        role = mention.startswith("role:")
        target = mention.split(":", 1)[1] if role else mention
        payload["content"] = f"<@&{target}>" if role else f"<@{target}>"
        payload["allowed_mentions"] = {"roles": [target]} if role else {"users": [target]}
    first = not sink.get("started") and not sink.get("thread_id") and sink["options"].get("thread") != "no"
    if first:
        payload["thread_name"] = config["name"][:100]
    body, kind = multipart({"payload_json": json.dumps(payload)}, small, "files") if small else (json.dumps(payload).encode(), "application/json")
    try:
        answer = request("POST", discord_url(sink, wait="true"), body, kind)
    except RuntimeError as error:
        if not first or "HTTP 400" not in str(error):
            raise
        payload.pop("thread_name")  # a text channel: no thread from a webhook (only a forum channel makes one)
        sink["options"]["thread"] = "no"
        body, kind = multipart({"payload_json": json.dumps(payload)}, small, "files") if small else (json.dumps(payload).encode(), "application/json")
        answer = request("POST", discord_url(sink, wait="true"), body, kind)
    if first and "thread_name" in payload and answer.get("channel_id"):
        sink["thread_id"] = answer["channel_id"]
    sink["started"] = True

def discord_status(sink: dict, status: dict, config: dict, words: dict) -> None:
    """The live status: one message edited in place (posted again when it was deleted)."""
    title = {"turn": "waiting", "question": "question", "waiting": "waiting", "done": "done"}.get(status.get("kind", ""), "working")
    embed = {"title": words[title], "description": status["text"][:1000], "color": COLOURS["status"],
             "footer": {"text": f"{status['steps']} {words['steps']} · {status['errors']} {words['errors']} · {words['last']} "
                                f"{time.strftime('%H:%M:%S', time.localtime(status['time']))}"}}
    body = json.dumps({"embeds": [embed], "allowed_mentions": {"parse": []}}).encode()
    if sink.get("status_id"):
        try:
            request("PATCH", discord_url(sink, f"/messages/{sink['status_id']}"), body, "application/json")
            return
        except RuntimeError as error:
            if "HTTP 404" not in str(error):
                raise
    payload = {"username": f"Claude · {config['name']}"[:80], "embeds": [embed], "allowed_mentions": {"parse": []}}
    sink["status_id"] = request("POST", discord_url(sink, wait="true"), json.dumps(payload).encode(), "application/json").get("id")

def record(event: dict, config: dict) -> dict:
    """The event as JSON for an API, a socket or a .jsonl file."""
    return {"session": config["session"], "name": config["name"], "cwd": config["cwd"], "kind": event["kind"],
            "title": event.get("title", ""), "text": event["text"], "time": time.strftime("%Y-%m-%dT%H:%M:%S%z", time.localtime(event["time"])),
            "files": [file_info(f) for f in event.get("files", [])], "link": event.get("link", "")}

def deliver(sink: dict, event: dict, config: dict) -> None:
    """Send one event to one sink (raises on failure)."""
    words = LABELS[config["lang"]]
    kind = sink["kind"]
    if kind == "discord":
        discord(sink, event, config, words)
    elif kind == "api":
        headers = dict(h.split(":", 1) for h in sink["options"].get("header", "").split(";") if ":" in h)
        files = [Path(f) for f in event.get("files", []) if Path(f).is_file()] if "upload" in sink["options"] else []
        data = json.dumps(record(event, config))
        body, form = multipart({"payload_json": data}, files, "files") if files else (data.encode(), "application/json")
        request("POST", sink["url"], body, form, {k.strip(): v.strip() for k, v in headers.items()})
    elif kind in ("tcp", "udp"):
        line = (json.dumps(record(event, config), ensure_ascii=False) + "\n").encode()
        if kind == "tcp":
            with socket.create_connection((sink["host"], sink["port"]), timeout=8) as link:
                link.sendall(line)
        else:
            family = socket.AF_INET6 if ":" in sink["host"] else socket.AF_INET
            with socket.socket(family, socket.SOCK_DGRAM) as link:
                link.sendto(line[:60000], (sink["host"], sink["port"]))
    else:
        target = Path(sink["path"])
        if kind == "dir":
            target.mkdir(parents=True, exist_ok=True)
            copies = target / f"{slug(config['name'])}-files"
            for file in event.get("files", []):
                if Path(file).is_file():
                    copies.mkdir(exist_ok=True)
                    shutil.copy2(file, copies / Path(file).name)
            target = target / f"{slug(config['name'])}.log"
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.suffix == ".jsonl":
            line = json.dumps(record(event, config), ensure_ascii=False)
        else:
            stamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(event["time"]))
            extra = "".join(f" -> {f}" for f in event.get("files", [])) + (f" -> {event['link']}" if event.get("link") else "")
            line = f"{stamp} [{event['kind']}] {config['name']}: " + (event.get("title", "") + ": " if event.get("title") else "") + event["text"].replace("\n", " / ") + extra
        with open(target, "a", encoding="utf-8") as log:
            log.write(line + "\n")

# ======================================================================= queue and worker
def enqueue(session: str, event: dict) -> None:
    queue = folder(session) / "queue"
    queue.mkdir(parents=True, exist_ok=True)
    event = {"time": time.time(), **event}
    name = f"{time.time_ns()}-{uuid.uuid4().hex[:6]}.json"
    (queue / (name + ".part")).write_text(json.dumps(event, ensure_ascii=False), encoding="utf-8")
    (queue / (name + ".part")).replace(queue / name)

def spawn(session: str) -> None:
    """Start the worker of the session in the background (it leaves at once when one is already running)."""
    base = folder(session)
    with open(base / "worker.log", "a", encoding="utf-8") as log:
        subprocess.Popen([sys.executable, str(SCRIPT), "_worker", session], stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                         start_new_session=True, close_fds=True)

def summarize(text: str, what: str, lang: str) -> str:
    """One log line from a long text, by Haiku in a separate `claude -p` (no tools, no saved session, no fork); the text
    cut short when it fails."""
    text = text.strip()
    if len(text) <= 220 or not shutil.which("claude"):
        return text[:600]
    task = {"request": "Summarize this request of the user as one log line: what is asked.",
            "turn": "Summarize this answer of the assistant as one log line: what was done, the result, what is waiting."}[what]
    system = (f"You write short progress notes for a log channel. Reply with the note only, in {'French' if lang == 'fr' else 'English'}, "
              "at most 30 words, plain text, no heading, no quotes.")
    try:
        done = subprocess.run(["claude", "-p", "--model", "haiku", "--no-session-persistence", "--tools", "", "--system-prompt", system, task],
                              input=text[:12000], capture_output=True, text=True, timeout=90, env={**os.environ, "SESSION_LOG_INNER": "1"}, check=False)
        if done.returncode == 0 and done.stdout.strip():
            return done.stdout.strip()[:600]
    except (OSError, subprocess.TimeoutExpired):
        pass
    return text[:300] + "…"

def process(base: Path, event: dict) -> None:
    """One event: summary, secrets hidden, then each sink whose level takes it, and the live status."""
    with Locked(base / "config.lock"):
        config = load(base)
    if not config:
        return
    if event.get("summarize") and config.get("summary", True):
        event["text"] = summarize(event["text"], event["summarize"], config["lang"])
    event["text"] = redact(event["text"])
    status_file = base / "status.json"
    status: dict = json.loads(status_file.read_text(encoding="utf-8")) if status_file.is_file() else {"steps": 0, "errors": 0, "folded": 0, "last_error": 0}
    kind = event["kind"]
    status["steps"] += kind in ("step", "done")
    folded = False
    if kind == "error":
        status["errors"] += 1
        folded = config["level"] != "all" and time.time() - status.get("last_error", 0) < ERROR_GAP
        status["folded"] = status.get("folded", 0) + folded
        if not folded:
            status["last_error"] = time.time()
            if status.get("folded"):
                event["text"] += f"\n(+{status['folded']} {LABELS[config['lang']]['more_errors']})"
                status["folded"] = 0
    words = LABELS[config["lang"]]
    if kind != "status":
        status["text"] = f"{event.get('title') or words.get(kind, kind)}: {event['text'][:300]}"
    else:
        status["text"] = event["text"][:300]
    status["time"] = event["time"]
    if kind != "status":
        status["kind"] = kind
    targets = event.get("targets") or [s["id"] for s in config["sinks"]]
    for sink in config["sinks"]:
        if sink["id"] not in targets:
            continue
        level = sink["options"].get("level", config["level"])
        try:
            if kind in LEVELS[level] and not folded:
                deliver(sink, event, config)
            if sink["kind"] == "discord" and kind not in ("stop",):
                discord_status(sink, status, config, words)
        except (RuntimeError, OSError, ValueError) as error:
            note_error(base, sink["id"], f"{kind}: {error}")
    status_file.write_text(json.dumps(status), encoding="utf-8")
    with Locked(base / "config.lock"):
        current = load(base)
        if not current:
            return
        by_id = {s["id"]: s for s in config["sinks"]}
        for sink in current["sinks"]:  # what the delivery learned: the thread, the status message
            for key in ("thread_id", "status_id", "started"):
                if key in by_id.get(sink["id"], {}):
                    sink[key] = by_id[sink["id"]][key]
            if by_id.get(sink["id"], {}).get("options", {}).get("thread") == "no":
                sink["options"]["thread"] = "no"
        if kind == "stop":
            current["sinks"] = [s for s in current["sinks"] if s["id"] not in targets]
        if current["sinks"]:
            save(base, current)
        else:
            (base / "config.json").unlink(missing_ok=True)

def worker(session: str) -> None:
    """Send the queued events in order; one worker per session (a lock), it leaves after a few idle seconds."""
    base = folder(session)
    with open(base / "worker.lock", "a", encoding="utf-8") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        idle = 0
        while idle < 4:
            items = sorted((base / "queue").glob("*.json"))
            if not items:
                idle += 1
                time.sleep(1)
                continue
            idle = 0
            for item in items:
                try:
                    process(base, json.loads(item.read_text(encoding="utf-8")))
                except (ValueError, KeyError) as error:
                    note_error(base, "worker", f"{item.name}: {error}")
                item.unlink(missing_ok=True)

# ======================================================================= hooks
def from_hook(data: dict) -> dict | None:
    """The event a Claude Code hook stands for, None when it says nothing worth logging."""
    name = data.get("hook_event_name", "")
    if name == "UserPromptSubmit":
        prompt = str(data.get("prompt", ""))
        if not prompt.strip() or prompt.lstrip().startswith(("/session-log", "<command-name>/session-log")):
            return None
        return {"kind": "request", "text": prompt, "summarize": "request"}
    if name == "PreToolUse" and data.get("tool_name") == "AskUserQuestion":
        lines = []
        for question in data.get("tool_input", {}).get("questions", []):
            lines.append(f"**{question.get('question', '')}**")
            lines += [f"- {option.get('label', '')}" for option in question.get("options", [])]
        return {"kind": "question", "text": "\n".join(lines) or "?"}
    if name == "Notification":
        message = str(data.get("message", ""))
        waiting = "permission" in message.lower() or data.get("notification_type") == "permission_prompt"
        return {"kind": "waiting" if waiting else "status", "text": message}
    if name == "PostToolUseFailure":
        error = str(data.get("error") or data.get("tool_response") or "")
        first = error.strip().splitlines()[0][:300] if error.strip() else "failed"
        return {"kind": "error", "text": f"{data.get('tool_name', 'tool')}: {first}"}
    if name == "Stop" and not data.get("stop_hook_active"):
        text = str(data.get("last_assistant_message") or last_reply(data.get("transcript_path", "")))
        return {"kind": "turn", "text": text, "summarize": "turn"} if text.strip() else None
    if name == "StopFailure":
        return {"kind": "error", "text": str(data.get("error") or data.get("message") or "the turn stopped on an error")[:300]}
    if name == "SessionEnd":
        return {"kind": "stop", "text": f"session ended ({data.get('reason', 'exit')})"}
    return None

def last_reply(path: str) -> str:
    """The text of the last assistant message of a transcript (JSON lines), read from the end."""
    file = Path(path)
    if not path or not file.is_file():
        return ""
    for line in reversed(file.read_text(encoding="utf-8", errors="replace").splitlines()[-200:]):
        try:
            entry = json.loads(line)
        except ValueError:
            continue
        message = entry.get("message", {})
        if entry.get("type") == "assistant" and isinstance(message.get("content"), list):
            text = " ".join(part.get("text", "") for part in message["content"] if part.get("type") == "text").strip()
            if text:
                return text
    return ""

def hook() -> None:
    """What the hooks run: fast, silent, never in the way (no log active -> nothing)."""
    if os.environ.get("SESSION_LOG_INNER"):
        return
    try:
        data = json.loads(sys.stdin.read() or "{}")
        session = str(data.get("session_id", ""))
        if not re.fullmatch(r"[\w-]{4,80}", session) or not (STATE / session / "config.json").is_file():
            return
        event = from_hook(data)
        if event:
            enqueue(session, event)
            spawn(session)
    except (OSError, ValueError):
        return

def settings_hooks(action: str, file: Path) -> None:
    """Add / remove / show the hooks of session-log in a Claude Code settings file (a backup first)."""
    settings = json.loads(file.read_text(encoding="utf-8")) if file.is_file() else {}
    hooks = settings.setdefault("hooks", {})
    command = f"python3 {SCRIPT} hook"

    def mine(entry: dict) -> bool:
        return any("session-log" in h.get("command", "") and h.get("command", "").endswith(" hook") for h in entry.get("hooks", []))

    present = [name for name, entries in hooks.items() if any(mine(e) for e in entries)]
    if action == "show":
        print(f"{file}: session-log hooks on {', '.join(present) if present else 'nothing'} ({command})")
        return
    if file.is_file():
        shutil.copy2(file, file.with_name(file.name + ".bak-session-log"))
    for name in list(hooks):
        hooks[name] = [e for e in hooks[name] if not mine(e)]
        if not hooks[name]:
            del hooks[name]
    if action == "install":
        for name, matcher in HOOK_EVENTS:
            entry = {"hooks": [{"type": "command", "command": command, "timeout": 10}]}
            hooks.setdefault(name, []).append({"matcher": matcher, **entry} if matcher else entry)
    if not hooks:
        settings.pop("hooks")
    file.write_text(json.dumps(settings, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{file}: session-log hooks {'installed on ' + ', '.join(n for n, _ in HOOK_EVENTS) if action == 'install' else 'removed'} "
          f"(backup: {file.name}.bak-session-log); they work from the next session or after /hooks")

# ======================================================================= commands
def start(args: argparse.Namespace, session: str) -> None:
    base = folder(session)
    with Locked(base / "config.lock"):
        config = load(base) or {"session": session, "name": args.name or f"{Path.cwd().name} · {session[:8]}", "cwd": str(Path.cwd()),
                                "level": args.level, "lang": args.lang, "summary": not args.no_summary, "sinks": [], "started": time.time()}
        if args.name:
            config["name"] = args.name
        first = max([int(s["id"][1:]) for s in config["sinks"]], default=0) + 1
        new = [parse_sink(spec, first + i) for i, spec in enumerate(args.sinks)]
        config["sinks"] += new
        save(base, config)
    event = {"kind": "start", "time": time.time(), "text": f"{config['name']} · {config['cwd']}" + (f"\n{args.text}" if args.text else "")}
    failed = 0
    for sink in new:  # the first message right away: a wrong address is said now, not later
        try:
            deliver(sink, dict(event), config)
            print(f"ok   {sink['id']} {sink['kind']:7} {shown(sink)}")
        except (RuntimeError, OSError, ValueError) as error:
            failed += 1
            print(f"FAIL {sink['id']} {sink['kind']:7} {shown(sink)}: {error}")
    with Locked(base / "config.lock"):
        current = load(base) or config
        by_id = {s["id"]: s for s in new}
        current["sinks"] = [by_id.get(s["id"], s) for s in current["sinks"]]
        save(base, current)
    print(f"session {session[:8]} · level {config['level']} · {len(current['sinks'])} sink(s)" + (f" · {failed} failed" if failed else ""))
    settings = Path.home() / ".claude" / "settings.json"
    if not (settings.is_file() and f"{SCRIPT} hook" in settings.read_text(encoding="utf-8", errors="ignore")):
        print("hooks not installed: only what Claude sends is logged (log.py hooks install)")
    if failed:
        sys.exit(1)

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--session", default=os.environ.get("CLAUDE_CODE_SESSION_ID", ""))
    sub = parser.add_subparsers(dest="command", required=True)
    one = sub.add_parser("start", help="start logging to one or several sinks (adds them when a log runs)")
    one.add_argument("sinks", nargs="+")
    one.add_argument("--name", default="")
    one.add_argument("--level", choices=list(LEVELS), default="key")
    one.add_argument("--lang", choices=list(LABELS), default="fr" if os.environ.get("LANG", "").startswith("fr") else "en")
    one.add_argument("--no-summary", action="store_true", help="no Haiku summary: the raw text, cut short")
    one.add_argument("--text", default="", help="a line for the first message (what the session is about)")
    two = sub.add_parser("send", help="send a message (a step, the end, a file, a link)")
    two.add_argument("text")
    two.add_argument("--kind", choices=["step", "done", "info", "question", "error", "file"], default="step")
    two.add_argument("--title", default="")
    two.add_argument("--file", action="append", default=[])
    two.add_argument("--link", default="")
    three = sub.add_parser("status", help="the live status line")
    three.add_argument("text")
    four = sub.add_parser("stop", help="stop one sink (s2) or all")
    four.add_argument("target", nargs="?", default="all")
    four.add_argument("--text", default="")
    sub.add_parser("list", help="the sinks of this session and the last errors")
    five = sub.add_parser("hooks", help="install / remove / show the Claude Code hooks")
    five.add_argument("action", choices=["install", "uninstall", "show"])
    five.add_argument("--settings", default=str(Path.home() / ".claude" / "settings.json"))
    sub.add_parser("hook")
    six = sub.add_parser("_worker")
    six.add_argument("worker_session")
    args = parser.parse_args()
    if args.command == "hook":
        hook()
    elif args.command == "_worker":
        worker(args.worker_session)
    elif args.command == "hooks":
        settings_hooks(args.action, Path(args.settings).expanduser())
    elif args.command == "start":
        start(args, args.session)
    else:
        base = folder(args.session)
        config = load(base)
        if not config:
            sys.exit("session-log: no log running in this session (log.py start SINK...)")
        if args.command == "list":
            for sink in config["sinks"]:
                print(f"{sink['id']} {sink['kind']:7} {shown(sink)}  level {sink['options'].get('level', config['level'])}")
            errors = (base / "errors.log").read_text(encoding="utf-8").splitlines()[-5:] if (base / "errors.log").is_file() else []
            print("\n".join(["last errors:", *errors]) if errors else "no error")
            return
        if args.command == "send":
            missing = [f for f in args.file if not Path(f).is_file()]
            if missing:
                sys.exit(f"session-log: not found: {', '.join(missing)}")
            enqueue(args.session, {"kind": "file" if args.file and args.kind == "step" else args.kind, "title": args.title, "text": args.text,
                                   "files": [str(Path(f).absolute()) for f in args.file], "link": args.link})
        elif args.command == "status":
            enqueue(args.session, {"kind": "status", "text": args.text})
        else:
            ids = [s["id"] for s in config["sinks"]]
            if args.target != "all" and args.target not in ids:
                sys.exit(f"session-log: no sink {args.target} (sinks: {', '.join(ids)})")
            enqueue(args.session, {"kind": "stop", "text": args.text or config["name"], "targets": ids if args.target == "all" else [args.target]})
        spawn(args.session)
        print("queued")

if __name__ == "__main__":
    main()
