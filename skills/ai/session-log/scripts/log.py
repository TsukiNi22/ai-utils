#!/usr/bin/env python3
"""Log the progress of a Claude session to Discord, Telegram, Slack, ntfy, Gotify, an API, a socket or a file - several
at once, each in its own way.

    log.py start SINK [SINK...] [--name N] [--level minimal|key|all] [--lang fr|en] [--no-summary] [--text T]
    log.py send TEXT [--kind step|done|info|question|error|file] [--file PATH]... [--link URL]
    log.py status TEXT                  # the live status (edited in place: Discord, Telegram, the status file of a log)
    log.py stop [SINK_ID|all] [--text TEXT]
    log.py list | clear                 # clear: delete what this log posted where a sender may (Discord, Telegram)
    log.py hooks install|uninstall|show [--settings FILE]
    log.py hook                         # what the Claude Code hooks call (event JSON on stdin)

SINK: a Discord webhook URL; https://api.telegram.org/bot<token>/sendMessage?chat_id=<id>; a Slack / Mattermost
webhook; https://ntfy.sh/<topic>; a Gotify .../message?token=...; another https:// address (one JSON record per event);
tcp://HOST:PORT or udp://HOST:PORT (one JSON line per event, format=text: readable coloured lines); a file (.log:
aligned text, .md: a Markdown journal, .jsonl: JSON lines; a status file is rewritten next to it); a folder ending with /
(a Markdown journal + a copy of each file sent).
What waits for the user (questions, permissions, the idle session, done, a blocking error, the end) calls them the way
of the place: a Discord / Slack mention, a Telegram notification with sound (the rest silent), a high ntfy / Gotify
priority (the rest low), "attention" in JSON, "◀ à toi" in a text log.
Options after #, comma separated: level=minimal|key|all, mention=here|<user id>|role:<id>|no (here by default; Slack
also <!here> / <@U...>), ping=question+permission+waiting+done+error+stop (the kinds that call), thread=<id>|no (Discord
thread / Telegram topic), maxmb=N (attachments: Discord 10, ntfy 15), upload (JSON API: files in a multipart body),
header=Name:Value;Name:Value (API), format=json|slack|ntfy|gotify|telegram|text (API), format=json|text (socket),
color=yes|no (ANSI: a text socket yes, a .log no), bell (a text socket: a bell on what calls), emoji=no, avatar=<url>|no.
The session is $CLAUDE_CODE_SESSION_ID (--session for another); the state is in ~/.local/state/session-log/<session>/
($SESSION_LOG_DIR), the config 0600 (a webhook URL, a bot token are secrets). Events are queued and sent in order by one
worker in the background; the request and the end of each turn are summarized by Haiku (claude -p, a separate process,
no fork).
"""
import argparse
import base64
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
AVATAR = "https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/skills/ai/session-log/assets/avatar.png"

STATE = Path(os.environ.get("SESSION_LOG_DIR", Path.home() / ".local" / "state" / "session-log")).expanduser()
SETTINGS = Path(os.environ.get("SESSION_LOG_SETTINGS", Path.home() / ".claude" / "settings.json")).expanduser()
DISCORD = re.compile(r"^https?://[^/]+/api(?:/v\d+)?/webhooks/\d+/[\w-]+$")
TELEGRAM = re.compile(r"^(https?://[^/?#]+/bot[^/?#]+)(?:/\w*)?/?(?:\?(.*))?$")  # the bot base, then the query (chat_id)
LEVELS = {"minimal": {"start", "stop", "done", "question", "permission", "waiting", "error"}}
LEVELS["key"] = LEVELS["minimal"] | {"request", "step", "file", "turn", "info"}
LEVELS["all"] = LEVELS["key"] | {"status"}
PING = ("question", "permission", "waiting", "done", "error", "stop")  # what waits for the user (or ends): mentioned
COLOURS = {"start": 0x5865F2, "stop": 0x747F8D, "request": 0x3B82F6, "step": 0x22C55E, "done": 0x8B5CF6, "question": 0xF59E0B, "permission": 0xF97316,
           "waiting": 0xF59E0B, "error": 0xEF4444, "turn": 0x14B8A6, "file": 0x06B6D4, "info": 0x94A3B8, "status": 0x64748B}
ICONS = {"start": "🟢", "stop": "💤", "request": "💬", "step": "✅", "done": "🏁", "question": "❓", "permission": "🔐", "waiting": "⏳",
         "error": "❌", "turn": "🔁", "file": "📎", "info": "📌", "link": "🔗", "status": "📊"}
STATES = {  # the live status: state -> (icon, colour); the state comes from the last event
    "working": ("🔵", 0x3B82F6), "waiting": ("🟠", 0xF59E0B), "question": ("🟠", 0xF59E0B), "error": ("🔴", 0xEF4444),
    "done": ("🟢", 0x22C55E), "stopped": ("💤", 0x747F8D)}
STATE_OF = {"turn": "waiting", "waiting": "waiting", "permission": "waiting", "question": "question", "error": "error", "done": "done", "stop": "stopped"}
STEP = re.compile(r"^\s*(?:(?:étape|etape|step)\s*)?(\d+)\s*/\s*(\d+)\s*[:·.-]?\s*(.*)$", re.I | re.S)  # "Étape 2/4 : ..."
IMAGE = (".png", ".jpg", ".jpeg", ".gif", ".webp")
API_FORMATS = ("json", "slack", "ntfy", "gotify", "telegram", "text")
ANSI = {"red": "\u001b[31m", "green": "\u001b[32m", "yellow": "\u001b[33m", "magenta": "\u001b[35m", "cyan": "\u001b[36m",
        "grey": "\u001b[90m", "reset": "\u001b[0m"}
TONE = {"error": "red", "step": "green", "done": "magenta", "question": "yellow", "permission": "yellow", "waiting": "yellow",
        "request": "cyan", "turn": "cyan", "stop": "grey"}  # the colour of a label in a terminal (a socket in text)
LABELS = {
    "fr": {"start": "Connexion établie", "restart": "Connexion rétablie", "stop": "Fin du log", "request": "Demande reçue", "step": "Étape", "done": "Terminé",
           "question": "Question en attente", "permission": "Permission demandée", "waiting": "Claude t'attend", "error": "Erreur", "turn": "Fin de tour", "file": "Fichier",
           "info": "Info", "status": "Statut", "steps": "Étapes", "errors": "Erreurs", "files": "Fichiers", "last": "Dernière activité",
           "started_at": "Démarré", "duration": "Durée", "folder": "Dossier", "level": "Niveau", "targets": "Cibles",
           "more_errors": "autres erreurs", "too_big": "trop gros pour être joint", "open": "Ouvrir",
           "working": "En cours", "waiting_you": "En attente de toi", "stopped": "Arrêté", "answer": "Réponds dans la session", "idle": "La session attend ton prochain message", "your_turn": "à toi"},
    "en": {"start": "Connected", "restart": "Reconnected", "stop": "Log ended", "request": "Request received", "step": "Step", "done": "Done",
           "question": "Question waiting", "permission": "Permission needed", "waiting": "Claude is waiting", "error": "Error", "turn": "End of turn", "file": "File",
           "info": "Info", "status": "Status", "steps": "Steps", "errors": "Errors", "files": "Files", "last": "Last activity",
           "started_at": "Started", "duration": "Duration", "folder": "Folder", "level": "Level", "targets": "Targets",
           "more_errors": "more errors", "too_big": "too big to attach", "open": "Open",
           "working": "Working", "waiting_you": "Waiting for you", "stopped": "Stopped", "answer": "Answer in the session", "idle": "The session waits for your next message", "your_turn": "your turn"},
}
SECRETS = [
    (re.compile(r"https?://[^\s/]+/api(?:/v\d+)?/webhooks/\d+/[\w-]+"), "[webhook]"),
    (re.compile(r"(?i)\b(api[_-]?key|token|secret|password|passwd|authorization)(\s*[=:]\s*)(?:bearer\s+)?\S+"), r"\1\2[hidden]"),
    (re.compile(r"\b(sk-[\w-]{20,}|gh[pousr]_\w{20,}|xox[abprs]-[\w-]{10,}|AKIA[0-9A-Z]{16}|eyJ[\w-]{10,}\.[\w-]{10,}\.[\w-]+)"), "[hidden]"),
    (re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----.*?-----END [A-Z ]*PRIVATE KEY-----", re.S), "[private key]"),
]
HOOK_EVENTS = [("UserPromptSubmit", None), ("PreToolUse", "AskUserQuestion"), ("PermissionRequest", None), ("Notification", None),
               ("PostToolUseFailure", None), ("Stop", None), ("StopFailure", None), ("SessionEnd", None)]
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
        sink = {**sink, "kind": "api", "url": raw, "format": api_format(raw, options)}
        if sink["format"] == "telegram":
            match = TELEGRAM.match(raw)
            chat = dict(urllib.parse.parse_qsl(match.group(2) or "")).get("chat_id", "") if match else ""
            if not chat:
                sys.exit("session-log: a Telegram target is https://api.telegram.org/bot<token>/sendMessage?chat_id=<chat> (the chat "
                         "id: write to the bot, then open https://api.telegram.org/bot<token>/getUpdates)")
            sink.update(tg=match.group(1), chat=chat)
        return sink
    if raw.startswith("http://"):
        sys.exit(f"session-log: {raw}: plain http only to this computer (127.0.0.1); use https")
    match = re.fullmatch(r"(tcp|udp)://(\[[^\]]+\]|[^:/]+):(\d{1,5})", raw)
    if match:
        if options.get("format", "json") not in ("json", "text"):
            sys.exit("session-log: a socket takes format=json (one JSON line per event) or format=text (readable lines)")
        return {**sink, "kind": match.group(1), "host": match.group(2).strip("[]"), "port": int(match.group(3)),
                "format": options.get("format", "json")}
    if re.match(r"^[a-z]+://", raw):
        sys.exit(f"session-log: {raw}: unknown sink (Discord webhook, https, tcp://, udp://, a file or a folder/)")
    path = Path(raw).expanduser().absolute()
    if raw.endswith("/") or path.is_dir():
        return {**sink, "kind": "dir", "path": str(path)}
    return {**sink, "kind": "file", "path": str(path)}

def api_format(url: str, options: dict) -> str:
    """The body an API expects, from #format= or from its address: Slack (and Mattermost / Rocket.Chat, the same
    webhooks), ntfy, Gotify, a Telegram bot, else the generic JSON record."""
    chosen = options.get("format", "")
    if chosen:
        if chosen not in API_FORMATS:
            sys.exit(f"session-log: format must be one of {', '.join(API_FORMATS)}")
        return chosen
    parts = urllib.parse.urlsplit(url)
    if parts.netloc == "hooks.slack.com" or "/hooks/" in parts.path and "mattermost" in parts.netloc:
        return "slack"
    if parts.netloc == "ntfy.sh" or parts.netloc.startswith("ntfy."):
        return "ntfy"
    if parts.path.rstrip("/").endswith("/message") and "token=" in parts.query:
        return "gotify"
    if parts.netloc == "api.telegram.org" and parts.path.startswith("/bot"):
        return "telegram"
    return "json"

def shown(sink: dict) -> str:
    """A sink as it can be printed: the token of a webhook, the query and the headers of an API hidden."""
    if sink["kind"] == "discord":
        return re.sub(r"/[\w-]+$", "/****", sink["url"])
    if sink["kind"] == "api":
        parts = urllib.parse.urlsplit(sink["url"])
        path = re.sub(r"/bot[^/]+", "/bot****", re.sub(r"(/services/[^/]+/[^/]+/)[^/]+", r"\1****", parts.path))
        return f"{parts.scheme}://{parts.netloc}{path} ({sink.get('format', 'json')})" + (" (+headers)" if sink["options"].get("header") else "")
    if sink["kind"] in ("tcp", "udp"):
        return f"{sink['kind']}://{sink['host']}:{sink['port']}"
    return sink["path"] + ("/" if sink["kind"] == "dir" else "")

def request(method: str, url: str, body: bytes | None, kind: str, headers: dict | None = None) -> dict:
    """One HTTP call; a 429 waits the time asked (Discord: retry_after, Telegram: parameters.retry_after; 3 tries).
    The JSON answer, {} when there is none."""
    for _ in range(3):
        call = urllib.request.Request(url, data=body, method=method, headers={"Content-Type": kind, "User-Agent": AGENT, **(headers or {})})
        try:
            with urllib.request.urlopen(call, timeout=30) as answer:
                text = answer.read()
            return json.loads(text) if text.strip().startswith(b"{") else {}
        except urllib.error.HTTPError as error:
            detail = error.read()[:400].decode("utf-8", "replace")
            if error.code == 429:
                try:
                    data = json.loads(detail)
                    wait = data.get("retry_after") or data.get("parameters", {}).get("retry_after", 2)
                except (ValueError, AttributeError):
                    wait = 2
                time.sleep(min(30.0, float(wait)))
                continue
            raise RuntimeError(f"HTTP {error.code} {detail}") from None
    raise RuntimeError("HTTP 429 (rate limited)")

def multipart(fields: dict, files: list, prefix: str) -> tuple[bytes, str]:
    """A multipart/form-data body: the fields, then the files as <prefix>[n] (Discord: files[0], files[1]...); a file is
    a Path or (Path, the name it is sent under)."""
    boundary = uuid.uuid4().hex
    out = b""
    for name, value in fields.items():
        out += f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"\r\nContent-Type: application/json\r\n\r\n'.encode() + value.encode() + b"\r\n"
    for index, item in enumerate(files):
        file, name = item if isinstance(item, tuple) else (item, item.name)
        head = f'--{boundary}\r\nContent-Disposition: form-data; name="{prefix}[{index}]"; filename="{name}"\r\nContent-Type: application/octet-stream\r\n\r\n'
        out += head.encode() + file.read_bytes() + b"\r\n"
    return out + f"--{boundary}--\r\n".encode(), f"multipart/form-data; boundary={boundary}"

def file_info(path: str) -> dict:
    file = Path(path)
    if not file.is_file():
        return {"name": file.name, "path": path, "missing": True}
    return {"name": file.name, "path": str(file), "size": file.stat().st_size, "sha256": hashlib.sha256(file.read_bytes()).hexdigest()}

def hook_id(sink: dict) -> str:
    return re.search(r"/webhooks/(\d+)/", sink["url"]).group(1)

def threads(update: tuple[str, str, dict] | None = None) -> dict:
    """The thread and the status message of each session per webhook (~/.local/state/session-log/threads.json, kept
    after a stop): a log started again in the same session writes in the same thread and edits the same status."""
    file = STATE / "threads.json"
    with Locked(STATE / "threads.lock"):
        known = json.loads(file.read_text(encoding="utf-8")) if file.is_file() else {}
        if update:
            webhook, session, fields = update
            known.setdefault(webhook, {}).setdefault(session, {}).update({**fields, "time": time.time()})
            file.write_text(json.dumps(known, indent=1), encoding="utf-8")
            file.chmod(0o600)
    return known

def remember(base: Path, sinks: list[dict]) -> None:
    """Keep the id of each message posted where it can be deleted (Discord, Telegram) in posted.jsonl (0600: it holds
    the webhook / the bot) for `log.py clear`."""
    lines = [json.dumps({"kind": kind_of(s), "url": s.get("tg") or s["url"], "chat": s.get("chat"), "id": i, "thread": th})
             for s in sinks for i, th in s.pop("posted", [])]
    if lines:
        file = base / "posted.jsonl"
        with open(file, "a", encoding="utf-8") as out:
            out.write("\n".join(lines) + "\n")
        file.chmod(0o600)

def discord_url(sink: dict, extra: str = "", **params: str) -> str:
    query = {k: v for k, v in params.items() if v}
    if sink.get("thread_id"):
        query["thread_id"] = sink["thread_id"]
    return sink["url"] + extra + ("?" + urllib.parse.urlencode(query) if query else "")

def span(seconds: float) -> str:
    seconds = int(max(0, seconds))
    if seconds >= 3600:
        return f"{seconds // 3600} h {seconds % 3600 // 60:02d} min"
    return f"{seconds // 60} min {seconds % 60:02d} s" if seconds >= 60 else f"{seconds} s"

def bar(done: int, total: int, width: int = 24) -> str:
    """A Discord progress bar: a line in an ANSI block, green for the part done, grey for the rest, then the count."""
    full = round(width * min(done, total) / max(total, 1))
    escape = "\u001b"
    return (f"```ansi\n{escape}[32m{'━' * full}{escape}[0m{escape}[30m{'━' * (width - full)}{escape}[0m  "
            f"{done}/{total} · {round(100 * min(done, total) / max(total, 1))} %\n```")

def link_line(url: str) -> str:
    """A link shown as a short path (host › part › part), clickable, instead of the raw address."""
    return f"🔗 [{link_label(url)}]({url})"

def avatar(sink: dict) -> dict:
    url = sink["options"].get("avatar", AVATAR)
    return {} if url == "no" else {"avatar_url": url}

def stamp(moment: float) -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(moment))

def icon(sink: dict, key: str) -> str:
    return "" if sink["options"].get("emoji") == "no" else ICONS.get(key, "") + " "

def embed_of(sink: dict, event: dict, config: dict, words: dict, status: dict | None) -> tuple[dict, list[Path]]:
    """The embed of one event: icon + label, the session as author, Discord's own timestamp, a layout per kind (a
    progress bar for "n/m" steps, a code block for an error, the numbered choices of a question, the counters at the
    end, the first image shown inside); the files to attach with it."""
    kind, text = event["kind"], event["text"]
    title = event.get("title") or words.get(kind, kind)
    description, fields = text, []
    if kind == "step" and STEP.match(text):
        done, total, rest = STEP.match(text).groups()
        title, description = f"{words['step']} {done}/{total}", (rest.strip() + "\n" if rest.strip() else "") + bar(int(done), int(total))
    elif kind in ("request", "turn"):
        description = "\n".join(f"> {line}" for line in text.splitlines() if line.strip())
    elif kind == "error":
        description = f"```\n{text[:3900]}\n```"
    elif kind == "question":
        choices = event.get("choices", [])
        description = text + ("\n\n" + "\n".join(f"**{n}.** {c}" for n, c in enumerate(choices, 1)) if choices else "") + f"\n-# {words['answer']}"
    elif kind in ("waiting", "permission"):
        description = f"{text}\n-# {words['answer']}"
    if kind == "start":
        fields = [{"name": words["folder"], "value": f"`{config['cwd'][-200:]}`", "inline": False},
                  {"name": words["level"], "value": config["level"], "inline": True},
                  {"name": words["targets"], "value": str(len(config["sinks"])), "inline": True}]
        description = text if text != f"{config['name']} · {config['cwd']}" else ""
        description = description.split("\n", 1)[1] if description.startswith(config["name"]) else description
    if kind in ("done", "stop") and status:
        fields = [{"name": words["duration"], "value": span(event["time"] - config.get("started", event["time"])), "inline": True},
                  {"name": words["steps"], "value": str(status.get("steps", 0)), "inline": True},
                  {"name": words["errors"], "value": str(status.get("errors", 0)), "inline": True},
                  {"name": words["files"], "value": str(status.get("files", 0)), "inline": True}]
    limit = float(sink["options"].get("maxmb", 10)) * (1 << 20)
    files = [Path(f) for f in event.get("files", []) if Path(f).is_file()]
    big = [f for f in files if f.stat().st_size > limit]
    files = [f for f in files if f not in big]
    if big:
        description += "\n" + "\n".join(f"`{f.name}`: {words['too_big']} (`{f}`)" for f in big)
    if event.get("link"):
        description += f"\n\n{link_line(event['link'])}"
    key = "link" if event.get("link") and kind == "info" else kind
    embed = {"author": {"name": config["name"][:250]}, "title": (icon(sink, key) + title)[:250], "description": description.strip()[:4000],
             "color": COLOURS.get(kind, 0x94A3B8), "timestamp": stamp(event["time"])}
    if fields:
        embed["fields"] = fields
    if event.get("link"):
        embed["url"] = event["link"]
    image = next((f for f in files if isinstance(f, Path) and f.suffix.lower() in IMAGE), None)
    if image:
        embed["image"] = {"url": f"attachment://{image.name}"}
    return embed, files

def discord(sink: dict, event: dict, config: dict, words: dict, status: dict | None = None) -> None:
    """A message with one embed (embed_of), the mention when it waits for the user, the files attached; a forum
    channel gets one post per session at the first message, a text channel falls back to plain messages."""
    embed, files = embed_of(sink, event, config, words, status)
    payload: dict = {"username": f"Claude · {config['name']}"[:80], "embeds": [embed], "allowed_mentions": {"parse": []}, **avatar(sink)}
    tag, allowed = mention_of(sink, event)
    if tag:  # the mention and a short text: what the push notification shows (an embed is not shown there)
        snippet = re.sub(r"\s+", " ", restyle(event["text"], "plain").replace("`", "")).strip()
        label = event.get("title") or words.get(event["kind"], event["kind"])
        payload["content"] = f"{tag} {icon(sink, event['kind'])}{label}" + (f" · {snippet[:160]}" if snippet else "")
        payload["allowed_mentions"] = allowed
    def body() -> tuple[bytes, str]:
        return multipart({"payload_json": json.dumps(payload)}, files, "files") if files else (json.dumps(payload).encode(), "application/json")

    answer: dict = {}
    if sink.get("thread_id"):  # the thread of this session (reused, or given with #thread=<id>)
        try:
            answer = request("POST", discord_url(sink, wait="true"), *body())
        except RuntimeError as error:
            if not re.search(r"HTTP (400|403|404)", str(error)):
                raise
            sink.pop("thread_id")  # deleted, locked or not in this channel: a new one below
            sink["thread_state"] = "lost"
    if not sink.get("thread_id"):
        create = sink["options"].get("thread", "") not in ("no", "none") and sink.get("thread_state") != "text-channel"
        if create:
            payload["thread_name"] = thread_name(config)
        try:
            answer = request("POST", discord_url(sink, wait="true"), *body())
        except RuntimeError as error:
            if not create or "220003" not in str(error):
                raise
            payload.pop("thread_name")  # a text channel: a webhook makes threads in a forum channel only
            sink["thread_state"] = "text-channel"
            answer = request("POST", discord_url(sink, wait="true"), *body())
        if create and "thread_name" in payload and answer.get("channel_id"):
            sink["thread_id"], sink["thread_state"] = answer["channel_id"], "new"
            threads((hook_id(sink), config["session"], {"thread": answer["channel_id"], "status": None}))
    if answer.get("id"):
        sink.setdefault("posted", []).append((answer["id"], sink.get("thread_id")))
    sink["started"] = True

def probe(sink: dict) -> str:
    """What the Discord webhook writes into, found without posting anything: the webhook itself (deleted -> an error),
    then a message with a thread name and no content - a forum / media channel refuses it as empty (50006), a text or
    announcement channel refuses the thread (220003). "forum", "text", or "" when the answer says neither (the first
    message then tries a thread and falls back)."""
    try:
        info = request("GET", sink["url"], None, "application/json")
    except RuntimeError as error:
        if "HTTP 404" in str(error):
            raise RuntimeError("unknown webhook (deleted, or the channel was deleted): make a new one") from None
        raise
    sink["channel"] = info.get("channel_id", "")
    try:
        request("POST", sink["url"] + "?wait=true", json.dumps({"thread_name": "session-log probe"}).encode(), "application/json")
    except RuntimeError as error:
        if "220003" in str(error):
            return "text"
        if "50006" in str(error):
            return "forum"
    return ""

def mention_of(sink: dict, event: dict) -> tuple[str, dict]:
    """Discord: who is mentioned on an event that calls the user (#mention=here by default, an id, role:<id>) and the
    allowed_mentions that lets exactly that one ping; ("", no ping) otherwise."""
    if not attention(sink, event):
        return "", {"parse": []}
    mention = sink["options"].get("mention", "here")
    if mention in ("here", "everyone"):
        return f"@{mention}", {"parse": ["everyone"]}
    if mention.startswith("role:"):
        return f"<@&{mention[5:]}>", {"roles": [mention[5:]]}
    return f"<@{mention}>", {"users": [mention]}

def thread_name(config: dict) -> str:
    """The name of the thread: the session name, with the short session id when the name does not have it."""
    short = config["session"][:8]
    return (config["name"] if short in config["name"] else f"{config['name']} · {short}")[:100]

def discord_status(sink: dict, status: dict, config: dict, words: dict) -> None:
    """The live status, one message edited in place (posted again when deleted): the state (working, waiting for the
    user, error, done, stopped) as icon and colour, what is going on, the progress bar of the last "n/m" step, the
    counters and Discord relative times (they tick by themselves: "2 minutes ago")."""
    state = STATE_OF.get(status.get("kind", ""), "working")
    mark, colour = STATES[state]
    label = state_label(state, words)
    lines = [status.get("text", "")[:900]]
    if status.get("progress"):
        lines.append(bar(*status["progress"]))
    started = int(config.get("started", status["time"]))
    emoji = "" if sink["options"].get("emoji") == "no" else mark + " "
    embed = {"author": {"name": config["name"][:250]}, "title": emoji + label,
             "description": "\n".join(line for line in lines if line), "color": colour, "timestamp": stamp(status["time"]),
             "fields": [{"name": words["steps"], "value": str(status.get("steps", 0)), "inline": True},
                        {"name": words["errors"], "value": str(status.get("errors", 0)), "inline": True},
                        {"name": words["files"], "value": str(status.get("files", 0)), "inline": True},
                        {"name": words["started_at"], "value": f"<t:{started}:R>", "inline": True},
                        {"name": words["last"], "value": f"<t:{int(status['time'])}:R>", "inline": True},
                        {"name": words["duration"], "value": span(status["time"] - started), "inline": True}],
             "footer": {"text": f"{words['status']} · session-log"}}
    def body(payload: dict) -> tuple[bytes, str]:
        return json.dumps({**payload, "attachments": []}).encode(), "application/json"  # an edit drops an old attachment

    if sink.get("status_id"):
        try:
            request("PATCH", discord_url(sink, f"/messages/{sink['status_id']}"), *body({"embeds": [embed], "allowed_mentions": {"parse": []}}))
            return
        except RuntimeError as error:
            if "HTTP 404" not in str(error):
                raise
    payload = {"username": f"Claude · {config['name']}"[:80], "embeds": [embed], "allowed_mentions": {"parse": []}, **avatar(sink)}
    sink["status_id"] = request("POST", discord_url(sink, wait="true"), *body(payload)).get("id")
    sink.setdefault("posted", []).append((sink["status_id"], sink.get("thread_id")))

def facts(sink: dict, event: dict, config: dict, status: dict | None) -> dict:
    """What every format shows of an event, computed once: icon, label, text, progress of a "n/m" step, choices of a
    question, counters at the end, the start details, whether it calls the user, the reconnection, the hint to answer."""
    words, kind, text = LABELS[config["lang"]], event["kind"], event["text"]
    key = "link" if event.get("link") and kind == "info" else kind
    out = {"kind": kind, "icon": "" if sink["options"].get("emoji") == "no" else ICONS.get(key, ""),
           "label": event.get("title") or words.get(kind, kind), "text": text, "progress": None, "choices": event.get("choices", []),
           "link": event.get("link", ""), "files": [file_info(f) for f in event.get("files", [])], "counters": None, "time": event["time"],
           "attention": attention(sink, event), "reconnect": bool(event.get("reconnect")),
           "hint": words["answer"] if kind in ("question", "permission", "waiting") else ""}
    step = STEP.match(text) if kind == "step" else None
    if step:
        out.update(label=f"{words['step']} {step.group(1)}/{step.group(2)}", text=step.group(3).strip(),
                   progress=[int(step.group(1)), int(step.group(2))])
    if kind == "start":
        out["text"] = text.split("\n", 1)[1] if "\n" in text else ""
        out["counters"] = {words["folder"]: config["cwd"], words["level"]: config["level"], words["targets"]: str(len(config["sinks"]))}
    if kind in ("done", "stop") and status:
        out["counters"] = {words["duration"]: span(event["time"] - config.get("started", event["time"])), words["steps"]: str(status.get("steps", 0)),
                           words["errors"]: str(status.get("errors", 0)), words["files"]: str(status.get("files", 0))}
    return out

def text_lines(f: dict, config: dict, colour: bool = False, bell: bool = False) -> list[str]:
    """A readable log: a banner when a session starts (or comes back), then one aligned line per event (date, icon +
    label, text, progress line, "◀ à toi" on what waits for the user), its details indented under it; ANSI colours for
    a terminal (a socket), a bell on what calls the user when asked."""
    words = LABELS[config["lang"]]

    def paint(text: str, tone: str) -> str:
        return f"{ANSI[tone]}{text}{ANSI['reset']}" if colour and tone else text

    when = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(f["time"]))
    pad = " " * 45
    if f["kind"] == "start":
        banner = f"── {f['icon']} {config['name']} · {when}" + (f" · {words['restart']}" if f["reconnect"] else "") + " "
        head = [paint(banner + "─" * max(4, 72 - len(banner)), "cyan")]
        return head + [f"{pad}{k}: {v}" for k, v in (f["counters"] or {}).items()] + ([f"{pad}{f['text']}"] if f["text"] else [])
    label = f"{f['icon']} {f['label']}".strip()
    body = [line for line in restyle(f["text"], "plain").splitlines() if line.strip()] or [""]
    first = f"{when}  {paint(f'{label:<24}', TONE.get(f['kind'], ''))} {body[0]}"
    if f["progress"]:
        first += "  " + line_bar(f["progress"], colour)
    if f["attention"]:
        first += "  " + paint(f"◀ {words['your_turn']}", "yellow") + ("\a" if bell else "")
    lines = [first] + [pad + line for line in body[1:]]
    lines += [f"{pad}{n}. {restyle(c, 'plain')}" for n, c in enumerate(f["choices"], 1)]
    lines += [f"{pad}📎 {x['path']}" for x in f["files"]] + ([f"{pad}🔗 {link_label(f['link'])}  {f['link']}"] if f["link"] else [])
    if f["counters"]:
        lines.append(pad + " · ".join(f"{k} {v}" for k, v in f["counters"].items()))
    if f["kind"] == "stop":
        lines.append(paint("─" * 72, "grey"))
    return lines

def markdown(f: dict, config: dict, files: list[str]) -> str:
    """A Markdown journal: a heading per session (and a line when it comes back), one bullet per event (time, icon,
    bold label, text, progress, "👉 à toi" on what waits for the user), a command or an error as a block under it, the
    choices and counters as sub-lists, images shown, other files and links as links."""
    words = LABELS[config["lang"]]
    when = time.strftime("%H:%M:%S", time.localtime(f["time"]))
    if f["kind"] == "start":
        head = f"\n## {f['icon']} {config['name']} · {time.strftime('%Y-%m-%d %H:%M', time.localtime(f['time']))}\n\n"
        head += f"*{words['restart']}*\n\n" if f["reconnect"] else ""
        return head + "".join(f"- **{k}**: `{v}`\n" for k, v in (f["counters"] or {}).items()) + (f"\n{f['text']}\n" if f["text"] else "") + "\n"
    lines = f["text"].splitlines() or [""]
    lead, rest = (lines[0], lines[1:]) if not lines[0].startswith("```") else ("", lines)
    if f["kind"] == "error" and rest and not any(line.startswith("```") for line in rest):
        rest = ["```", *rest, "```"]
    if f["kind"] == "error" and lead:
        lead = f"`{lead}`"
    line = f"- `{when}` {f['icon']} **{f['label']}**" + (f" · {lead}" if lead else "")
    line += f" `{line_bar(f['progress'])}`" if f["progress"] else ""
    line += f" · **👉 {words['your_turn']}**" if f["attention"] else ""
    if rest and not any(part.lstrip().startswith("```") for part in rest):
        line += "  \n" + "".join(f"  {part}  \n" for part in rest)  # plain lines: hard breaks under the bullet
    else:
        line += "\n" + "".join(f"  {part}\n" for part in rest)  # a block: indented under the bullet
    line += "".join(f"  {n}. {c}\n" for n, c in enumerate(f["choices"], 1))
    for path in files:
        name = Path(path).name
        line += f"  ![{name}]({path})\n" if name.lower().endswith(IMAGE) else f"  - [{name}]({path})\n"
    if f["link"]:
        line += f"  - {link_line(f['link'])}\n"
    if f["counters"]:
        line += "  - " + " · ".join(f"**{k}** {v}" for k, v in f["counters"].items()) + "\n"
    return line + ("\n---\n" if f["kind"] == "stop" else "")

def record(f: dict, config: dict, status: dict | None = None) -> dict:
    """The event as JSON (generic API, socket, .jsonl): the raw fields, what a consumer shows as is (line, link_label),
    whether it calls the user (attention, priority), the reconnection and the live status of the session."""
    out = {"session": config["session"], "name": config["name"], "cwd": config["cwd"], "kind": f["kind"], "icon": f["icon"],
           "label": f["label"], "text": f["text"], "line": " ".join(f"{f['icon']} {f['label']}".split() + (["·", *restyle(f["text"], "plain").split()] if f["text"] else [])),
           "time": time.strftime("%Y-%m-%dT%H:%M:%S%z", time.localtime(f["time"])), "files": f["files"], "link": f["link"],
           "attention": f["attention"], "priority": "high" if f["attention"] else "normal"}
    for key in ("progress", "choices", "counters", "hint"):
        if f[key]:
            out[key] = f[key]
    if f["link"]:
        out["link_label"] = link_label(f["link"])
    if f["reconnect"]:
        out["reconnect"] = True
    if status:
        out["state"] = STATE_OF.get(status.get("kind", ""), "working")
        out["status"] = status_record(status, config)
    return out

def chat_text(f: dict, markup: str, label: bool = True, files: bool = True) -> str:
    """One message for a chat (Slack mrkdwn, Telegram HTML, ntfy / Gotify Markdown): the label in bold, the text, the
    progress line, the choices, the counters, the hint to answer, the files, the link as a short clickable path."""
    def bold(text: str) -> str:
        if markup == "html":
            return f"<b>{html_escape(text)}</b>"
        return f"*{text}*" if markup == "slack" else f"**{text}**"

    def esc(text: str) -> str:
        return restyle(text, markup)

    one = f["text"] and "\n" not in f["text"]
    lines = [f"{f['icon']} {bold(f['label'])}".strip() + (f" · {esc(f['text'])}" if one else "")] if label else ([esc(f["text"])] if one else [])
    if f["text"] and not one:
        lines.append(esc(f["text"]))
    if f["progress"]:
        lines.append(line_bar(f["progress"]))
    lines += [f"{n}. {esc(c)}" for n, c in enumerate(f["choices"], 1)]
    if f["counters"]:
        lines.append(" · ".join(f"{bold(k)} {html_escape(v) if markup == 'html' else v}" for k, v in f["counters"].items()))
    if f["hint"]:
        lines.append({"html": f"<i>↪ {html_escape(f['hint'])}</i>", "slack": f"_↪ {f['hint']}_"}.get(markup, f"*↪ {f['hint']}*"))
    if files:
        lines += [f"📎 {html_escape(x['name']) if markup == 'html' else x['name']} ({x['path']})" for x in f["files"]]
    if f["link"]:
        name = link_label(f["link"])
        lines.append({"slack": f"🔗 <{f['link']}|{name}>", "html": f'🔗 <a href="{html_escape(f["link"])}">{html_escape(name)}</a>'}
                     .get(markup, f"🔗 [{name}]({f['link']})"))
    return "\n".join(lines)

def restyle(text: str, markup: str) -> str:
    """The Markdown of a text (**bold**, *italic*, `code`, ``` blocks) in the markup of the destination: Slack mrkdwn,
    Telegram HTML (<pre> blocks), or plain (a text log: the fences dropped)."""
    if markup == "md":
        return text
    out = []
    for part in re.split(r"(```.*?```)", text, flags=re.S):
        if len(part) >= 6 and part.startswith("```") and part.endswith("```"):
            code = re.sub(r"^```\w*\n?|\n?```$", "", part)
            out.append(f"<pre>{html_escape(code)}</pre>" if markup == "html" else part if markup == "slack" else code)
        else:
            out.append(inline_style(part, markup))
    return "".join(out)

def html_escape(text: str) -> str:
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def api_body(sink: dict, f: dict, config: dict, status: dict | None) -> tuple[str, bytes]:
    """The URL and the JSON body an API expects (its format); each calls the user its own way on what waits for them:
    Slack a mention, ntfy / Gotify a high priority (a low one for the rest)."""
    form, words = sink.get("format", "json"), LABELS[config["lang"]]
    picture = sink["options"].get("avatar", AVATAR)
    if form == "slack":
        text = chat_text(f, "slack")
        if f["attention"]:
            text = f"{slack_mention(sink['options'].get('mention', 'here'))} {text}"
        context = f"{config['name']} · {time.strftime('%H:%M:%S', time.localtime(f['time']))}"
        payload = {"text": text, "username": f"Claude · {config['name']}"[:80],
                   "blocks": [{"type": "section", "text": {"type": "mrkdwn", "text": text[:2900]}},
                              {"type": "context", "elements": [{"type": "mrkdwn", "text": context}]}]}
        if picture != "no":
            payload["icon_url"] = picture
        return sink["url"], json.dumps(payload).encode()
    if form == "ntfy":  # JSON publishing: the topic in the body, posted to the server root
        parts = urllib.parse.urlsplit(sink["url"])
        calls = f["attention"]
        body = {"topic": parts.path.strip("/"), "title": f"{f['icon']} {f['label']} · {config['name']}".strip(),
                "message": chat_text(f, "md", label=False) or f["label"], "markdown": True, "tags": [f["kind"]],
                "priority": (5 if f["kind"] in ("question", "permission", "waiting") else 4) if calls else 2}
        if picture != "no":
            body["icon"] = picture
        if f["link"]:
            body["click"] = f["link"]
            body["actions"] = [{"action": "view", "label": words["open"], "url": f["link"], "clear": False}]
        return f"{parts.scheme}://{parts.netloc}/", json.dumps(body).encode()
    if form == "gotify":
        extras: dict = {"client::display": {"contentType": "text/markdown"}}
        if f["link"]:
            extras["client::notification"] = {"click": {"url": f["link"]}}
        return sink["url"], json.dumps({"title": f"{f['icon']} {f['label']} · {config['name']}".strip(), "message": chat_text(f, "md", label=False) or f["label"],
                                        "priority": 8 if f["attention"] else 2, "extras": extras}).encode()
    if form == "text":
        return sink["url"], json.dumps({"text": "\n".join(text_lines(f, config))}).encode()
    return sink["url"], json.dumps(record(f, config, status)).encode()

def form_data(fields: dict, files: list[tuple[str, Path]]) -> tuple[bytes, str]:
    """A plain multipart/form-data body (Telegram): text fields, then each file under its field name."""
    boundary = uuid.uuid4().hex
    out = b""
    for name, value in fields.items():
        out += f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"\r\n\r\n'.encode() + str(value).encode() + b"\r\n"
    for field, file in files:
        name = file.name.replace('"', "'")
        out += (f'--{boundary}\r\nContent-Disposition: form-data; name="{field}"; filename="{name}"\r\n'
                "Content-Type: application/octet-stream\r\n\r\n").encode() + file.read_bytes() + b"\r\n"
    return out + f"--{boundary}--\r\n".encode(), f"multipart/form-data; boundary={boundary}"

def sink_key(sink: dict) -> str:
    """The name a target is remembered under (threads.json): the webhook id for Discord, a hash of the address for the
    others (a token never written as is)."""
    if sink["kind"] == "discord":
        return hook_id(sink)
    identity = f"{sink['tg']}|{sink['chat']}" if sink.get("format") == "telegram" else sink.get("url") or sink.get("path") \
        or f"{sink.get('host')}:{sink.get('port')}"
    return f"{sink['kind']}:" + hashlib.sha256(identity.encode()).hexdigest()[:16]

def kind_of(sink: dict) -> str:
    return sink["format"] if sink["kind"] == "api" and sink.get("format") not in (None, "json", "text") else sink["kind"]

def line_bar(progress: list | None, colour: bool = False, width: int = 16) -> str:
    """The progress as a line, the shape of the Discord one: thick for the part done, thin for the rest, then n/m and
    the percent; green / grey in ANSI for a terminal."""
    if not progress:
        return ""
    done, total = progress
    full = round(width * min(done, total) / max(total, 1))
    tail = f"{done}/{total} · {round(100 * min(done, total) / max(total, 1))} %"
    if colour:
        return f"{ANSI['green']}{'━' * full}{ANSI['grey']}{'─' * (width - full)}{ANSI['reset']} {tail}"
    return f"{'━' * full}{'─' * (width - full)} {tail}"

def link_label(url: str) -> str:
    parts = urllib.parse.urlsplit(url)
    return " › ".join([parts.netloc.removeprefix("www.")] + [p for p in parts.path.split("/") if p][:3])

def attention(sink: dict, event: dict) -> bool:
    """Whether an event waits for the user (or ends something) and must call them: its kind in #ping (every kind of
    PING by default), not a quiet tool failure, and #mention not "no". Each destination calls in its own way."""
    kinds = sink["options"].get("ping", "+".join(PING)).split("+")
    return event["kind"] in kinds and not event.get("quiet") and sink["options"].get("mention", "here") != "no"

def slack_mention(value: str) -> str:
    """Slack / Mattermost: here -> <!here>, everyone / channel -> <!channel>, role:<id> -> a user group, else a user id
    (or a raw <...> given as is)."""
    if value.startswith("<"):
        return value
    if value in ("here", "everyone", "channel"):
        return "<!here>" if value == "here" else "<!channel>"
    return f"<!subteam^{value[5:]}>" if value.startswith("role:") else f"<@{value}>"

def state_label(state: str, words: dict) -> str:
    return {"working": words["working"], "waiting": words["waiting_you"], "question": words["question"], "error": words["error"],
            "done": words["done"], "stopped": words["stopped"]}[state]

def inline_style(text: str, markup: str) -> str:
    if markup == "html":
        text = html_escape(text)
        text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
        text = re.sub(r"(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])", r"<i>\1</i>", text)
        return re.sub(r"`([^`]+)`", r"<code>\1</code>", text)
    if markup == "slack":
        text = re.sub(r"(?<![\w*])\*(?!\s)([^*]+?)(?<!\s)\*(?![\w*])", r"_\1_", text)
        return re.sub(r"\*\*(.+?)\*\*", r"*\1*", text)
    return re.sub(r"\*\*(.+?)\*\*|(?<![\w*])\*(?!\s)([^*]+?)(?<!\s)\*(?![\w*])", lambda m: m.group(1) or m.group(2), text)

def status_record(status: dict, config: dict) -> dict:
    """The live status as JSON: state, what is going on, progress, counters, times."""
    started, last = config.get("started", status.get("time", 0)), status.get("time", 0)

    def iso(moment: float) -> str:
        return time.strftime("%Y-%m-%dT%H:%M:%S%z", time.localtime(moment))

    return {"session": config.get("session", ""), "name": config.get("name", ""), "state": STATE_OF.get(status.get("kind", ""), "working"),
            "activity": restyle(status.get("text", ""), "plain"), "progress": status.get("progress"), "steps": status.get("steps", 0),
            "errors": status.get("errors", 0), "files": status.get("files", 0), "started": iso(started), "last": iso(last),
            "duration_s": int(last - started)}

def header_value(text: str) -> str:
    """A header value kept ASCII: UTF-8 text as an RFC 2047 encoded word (ntfy decodes it)."""
    if text.isascii() and "\n" not in text:
        return text
    return "=?UTF-8?B?" + base64.b64encode(text.encode()).decode() + "?="

def ntfy_attach(sink: dict, f: dict, config: dict, file: Path, headers: dict, first: bool) -> None:
    """A file sent to ntfy as an attachment (one per message: a PUT with the file as body), the title, message,
    priority, tags, click and icon as headers."""
    url, data = api_body(sink, f, config, None)
    body = json.loads(data)
    head = {"Filename": header_value(file.name), "Title": header_value(body["title"] if first else f"📎 {file.name}"),
            "Priority": str(body["priority"] if first else 2), "Tags": ",".join(body["tags"]), "Markdown": "yes"}
    if first:
        head["Message"] = header_value(body["message"][:1500])
    for key, name in (("click", "Click"), ("icon", "Icon")):
        if body.get(key):
            head[name] = body[key]
    request("PUT", f"{url}{body['topic']}", file.read_bytes(), "application/octet-stream", {**headers, **head})

def tg_call(sink: dict, method: str, payload: dict, files: list | None = None) -> dict:
    """One Telegram Bot API call (JSON, or multipart with files [(field, path)]); its result, an error when not ok."""
    if files:
        body, kind = form_data({k: v if isinstance(v, str) else json.dumps(v) for k, v in payload.items()}, files)
    else:
        body, kind = json.dumps(payload).encode(), "application/json"
    answer = request("POST", f"{sink['tg']}/{method}", body, kind)
    if answer and not answer.get("ok", True):
        raise RuntimeError(f"telegram {method}: {answer.get('description', 'error')}")
    result = answer.get("result", {})
    return result if isinstance(result, dict) else {}

def tg_probe(sink: dict) -> str:
    """The kind of Telegram chat (getChat): "forum" (a supergroup with topics), "private", "group", "supergroup",
    "channel"; a wrong token or chat is an error at once."""
    chat = tg_call(sink, "getChat", {"chat_id": sink["chat"]})
    return "forum" if chat.get("is_forum") else chat.get("type", "")

def tg_topic(sink: dict, config: dict) -> None:
    """The topic of this session in a forum supergroup, made once (the bot must be an admin allowed to manage topics);
    refused -> the messages carry the session name instead."""
    try:
        result = tg_call(sink, "createForumTopic", {"chat_id": sink["chat"], "name": thread_name(config)[:128]})
        sink["thread_id"], sink["thread_state"] = str(result["message_thread_id"]), "new"
    except (RuntimeError, KeyError) as error:
        sink.pop("thread_id", None)
        sink["thread_state"], sink["topic_error"] = "no-topic", str(error)[:120]

def tg_send(sink: dict, method: str, payload: dict, files: list | None = None) -> dict:
    """Send with HTML; a text Telegram cannot parse (a tag cut by a length limit) is sent again as plain text."""
    try:
        return tg_call(sink, method, payload, files)
    except RuntimeError as error:
        if "parse" not in str(error).lower():
            raise
        key = "caption" if "caption" in payload else "text"
        plain = {k: v for k, v in payload.items() if k != "parse_mode"}
        plain[key] = re.sub(r"<[^>]+>", "", payload.get(key, ""))
        return tg_call(sink, method, plain, files)

def telegram(sink: dict, event: dict, f: dict, config: dict) -> None:
    """A Telegram message in HTML: in the topic of the session (a forum supergroup) or headed by the session name (a
    private chat, a group); with sound only when it calls the user (+ tg://user when #mention is an id), silent
    otherwise; files as photos / documents with the text as caption; the ids kept for clear."""
    for attempt in range(2):
        target = {"chat_id": sink["chat"], **({"message_thread_id": int(sink["thread_id"])} if sink.get("thread_id") else {})}
        text = chat_text(f, "html", files=False)
        if not sink.get("thread_id"):
            text = f"<i>{html_escape(config['name'])}</i>\n{text}"
        mention = sink["options"].get("mention", "here")
        if f["attention"] and mention.isdigit():
            text = f'<a href="tg://user?id={mention}">@</a> {text}'
        common = {**target, "parse_mode": "HTML", "disable_notification": not f["attention"]}
        files = [Path(x) for x in event.get("files", []) if Path(x).is_file()]
        big = [x for x in files if x.stat().st_size > 50 << 20]
        files = [x for x in files if x not in big]
        if big:
            text += "\n" + "\n".join(f"📎 {html_escape(x.name)} ({html_escape(str(x))})" for x in big)
        sent = []
        try:
            for index, file in enumerate(files):
                photo = file.suffix.lower() in (".png", ".jpg", ".jpeg") and file.stat().st_size <= 10 << 20
                method, field = ("sendPhoto", "photo") if photo else ("sendDocument", "document")
                sent.append(tg_send(sink, method, {**common, **({"caption": text[:1024]} if index == 0 else {})}, [(field, file)]))
            if not files or len(text) > 1024:  # no file, or a caption too long: the whole text as a message
                sent.append(tg_send(sink, "sendMessage", {**common, "text": text[:4096], "link_preview_options": {"is_disabled": True}}))
        except RuntimeError as error:
            if attempt or not sink.get("thread_id") or "thread not found" not in str(error).lower():
                raise
            sink.pop("thread_id")  # the topic was deleted: a new one, then once more
            tg_topic(sink, config)
            continue
        for message in sent:
            if message.get("message_id"):
                sink.setdefault("posted", []).append((message["message_id"], sink.get("thread_id")))
        sink["started"] = True
        return

def status_text(status: dict, config: dict, words: dict, markup: str) -> str:
    """The live status as text (Telegram HTML, Markdown or plain): state and session, what is going on, the progress
    line, the counters, the times."""
    state = STATE_OF.get(status.get("kind", ""), "working")
    started, last = config.get("started", status["time"]), status["time"]

    def bold(text: str) -> str:
        return f"<b>{html_escape(text)}</b>" if markup == "html" else f"**{text}**" if markup == "md" else text

    def clock(moment: float) -> str:
        return time.strftime("%H:%M:%S", time.localtime(moment))

    name = html_escape(config.get("name", "")) if markup == "html" else config.get("name", "")
    lines = [f"{STATES[state][0]} {bold(state_label(state, words))} · {name}", restyle(status.get("text", ""), markup)]
    if status.get("progress"):
        lines.append(line_bar(status["progress"]))
    lines.append(" · ".join(f"{words[k]} {status.get(k, 0)}" for k in ("steps", "errors", "files")))
    lines.append(f"{words['started_at']} {clock(started)} · {words['last']} {clock(last)} · {words['duration']} {span(last - started)}")
    return ("  \n" if markup == "md" else "\n").join(line for line in lines if line)  # Markdown: hard breaks

def telegram_status(sink: dict, status: dict, config: dict, words: dict) -> None:
    """The live status in Telegram: one silent message edited in place (sent again when it was deleted)."""
    text = status_text(status, config, words, "html")
    if sink.get("status_id"):
        try:
            tg_call(sink, "editMessageText", {"chat_id": sink["chat"], "message_id": int(sink["status_id"]), "text": text,
                                              "parse_mode": "HTML", "link_preview_options": {"is_disabled": True}})
            return
        except RuntimeError as error:
            if "not modified" in str(error):
                return
            if not re.search(r"not found|can't be edited|MESSAGE_ID_INVALID", str(error)):
                raise
    target = {"chat_id": sink["chat"], **({"message_thread_id": int(sink["thread_id"])} if sink.get("thread_id") else {})}
    result = tg_call(sink, "sendMessage", {**target, "text": text, "parse_mode": "HTML", "disable_notification": True,
                                           "link_preview_options": {"is_disabled": True}})
    sink["status_id"] = str(result.get("message_id", ""))
    sink.setdefault("posted", []).append((sink["status_id"], sink.get("thread_id")))

def board_file(sink: dict, config: dict) -> Path:
    """The status file next to a log: <log>.status (text), <log>.status.md, <log>.status.json; in a folder, per session."""
    if sink["kind"] == "dir":
        return Path(sink["path"]) / f"{slug(config['name'])}.status.md"
    path = Path(sink["path"])
    if path.suffix.lower() in (".jsonl", ".json"):
        return path.with_name(path.stem + ".status.json")
    if path.suffix.lower() in (".md", ".markdown"):
        return path.with_name(path.stem + ".status.md")
    return path.with_name(path.name + ".status")

def write_board(sink: dict, status: dict, config: dict) -> None:
    """The live status of a file target - what Discord / Telegram edit in place - rewritten at each event: one block per
    session that logs there (the latest first, a week kept), text, Markdown or JSON like the log."""
    file = board_file(sink, config)
    store = STATE / "boards.json"
    with Locked(STATE / "boards.lock"):
        boards = json.loads(store.read_text(encoding="utf-8")) if store.is_file() else {}
        board = boards.setdefault(str(file), {})
        board[config["session"]] = {"status": status, "config": {k: config.get(k) for k in ("session", "name", "started", "lang")}}
        week = time.time() - 7 * 86400
        boards[str(file)] = board = {s: e for s, e in board.items() if e["status"].get("time", 0) > week}
        store.write_text(json.dumps(boards), encoding="utf-8")
    entries = sorted(board.values(), key=lambda e: e["status"].get("time", 0), reverse=True)
    if file.name.endswith(".json"):
        content = json.dumps([status_record(e["status"], e["config"]) for e in entries], ensure_ascii=False, indent=1) + "\n"
    else:
        markup = "md" if file.name.endswith(".md") else "plain"
        blocks = [status_text(e["status"], e["config"], LABELS[e["config"].get("lang") or "en"], markup) for e in entries]
        content = ("\n\n---\n\n" if markup == "md" else "\n\n").join(blocks) + "\n"
    file.parent.mkdir(parents=True, exist_ok=True)
    temp = file.with_name(file.name + ".tmp")
    temp.write_text(content, encoding="utf-8")
    temp.replace(file)

def deliver(sink: dict, event: dict, config: dict, status: dict | None = None) -> None:
    """Send one event to one sink, in the format of that destination (raises on failure)."""
    words = LABELS[config["lang"]]
    kind = sink["kind"]
    if kind == "discord":
        discord(sink, event, config, words, status)
        return
    f = facts(sink, event, config, status)
    if kind == "api":
        if sink.get("format") == "telegram":
            telegram(sink, event, f, config)
            return
        headers = {k.strip(): v.strip() for k, v in (h.split(":", 1) for h in sink["options"].get("header", "").split(";") if ":" in h)}
        files = [Path(x) for x in event.get("files", []) if Path(x).is_file()]
        if sink.get("format") == "ntfy" and files:  # ntfy: each file as an attachment, the first one with the message
            limit = float(sink["options"].get("maxmb", 15)) * (1 << 20)
            small = [x for x in files if x.stat().st_size <= limit]
            f = {**f, "files": [x for x in f["files"] if Path(x["path"]).stat().st_size > limit]} if small else f
            for index, file in enumerate(small):
                ntfy_attach(sink, f, config, file, headers, index == 0)
            if small:
                return
        url, data = api_body(sink, f, config, status)
        upload = files if "upload" in sink["options"] and sink.get("format", "json") == "json" else []
        body, form = multipart({"payload_json": data.decode()}, upload, "files") if upload else (data, "application/json")
        request("POST", url, body, form, headers)
    elif kind in ("tcp", "udp"):
        if sink.get("format") == "text":
            text = "\n".join(text_lines(f, config, colour=sink["options"].get("color", "yes") != "no", bell="bell" in sink["options"]))
        else:
            text = json.dumps(record(f, config, status), ensure_ascii=False)
        line = (text + "\n").encode()
        if kind == "tcp":
            with socket.create_connection((sink["host"], sink["port"]), timeout=8) as link:
                link.sendall(line)
        else:
            family = socket.AF_INET6 if ":" in sink["host"] else socket.AF_INET
            with socket.socket(family, socket.SOCK_DGRAM) as link:
                link.sendto(line[:60000], (sink["host"], sink["port"]))
    else:
        target, files = Path(sink["path"]), list(event.get("files", []))
        if kind == "dir":
            target.mkdir(parents=True, exist_ok=True)
            copies = target / f"{slug(config['name'])}-files"
            for index, file in enumerate(files):
                if Path(file).is_file():
                    copies.mkdir(exist_ok=True)
                    shutil.copy2(file, copies / Path(file).name)
                    files[index] = f"{copies.name}/{Path(file).name}"  # relative: the journal opens it from the folder
            target = target / f"{slug(config['name'])}.{sink['options'].get('format', 'md')}"
        target.parent.mkdir(parents=True, exist_ok=True)
        suffix = target.suffix.lower()
        if suffix in (".jsonl", ".json"):
            text = json.dumps(record(f, config, status), ensure_ascii=False) + "\n"
        elif suffix in (".md", ".markdown"):
            text = markdown(f, config, files)
        else:
            text = "\n".join(text_lines(f, config, colour=sink["options"].get("color") == "yes")) + "\n"
        with open(target, "a", encoding="utf-8") as log:
            log.write(text)

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
    """One event: summary, secrets hidden, then each sink whose level takes it, and the live status where the place
    has one (Discord / Telegram: a message edited in place, a file: the status file next to it)."""
    with Locked(base / "config.lock"):
        config = load(base)
    if not config:
        return
    if event.get("summarize") and config.get("summary", True):
        event["text"] = summarize(event["text"], event["summarize"], config["lang"])
    if event.get("idle"):
        event["text"] = LABELS[config["lang"]]["idle"]
    event["text"] = redact(event["text"])
    status_file = base / "status.json"
    status: dict = json.loads(status_file.read_text(encoding="utf-8")) if status_file.is_file() else {"steps": 0, "errors": 0, "folded": 0, "last_error": 0}
    kind = event["kind"]
    status.setdefault("started", config.get("started", event["time"]))
    status["steps"] += kind == "step"
    status["files"] = status.get("files", 0) + len(event.get("files", []))
    progress = STEP.match(event["text"]) if kind == "step" else None
    if progress:
        status["progress"] = [int(progress.group(1)), int(progress.group(2))]
    folded = False
    if kind == "error":
        status["errors"] += 1
        # the failed tools are grouped (one a minute at key); an error that blocks the work is always sent, with the count
        folded = bool(event.get("quiet")) and config["level"] != "all" and time.time() - status.get("last_error", 0) < ERROR_GAP
        status["folded"] = status.get("folded", 0) + folded
        if not folded:
            status["last_error"] = time.time()
            if status.get("folded"):
                event["text"] += f"\n(+{status['folded']} {LABELS[config['lang']]['more_errors']})"
                status["folded"] = 0
    words = LABELS[config["lang"]]
    if kind == "status":
        status["text"] = event["text"][:300]
    else:
        short, label = event["text"].replace("\n", " ")[:300], event.get("title") or words.get(kind, kind)
        if progress:
            short, label = progress.group(3).strip()[:300], f"{words['step']} {progress.group(1)}/{progress.group(2)}"
        status["text"] = f"**{label}**" + (f" · {short}" if short else "")
    status["time"] = event["time"]
    if kind != "status":
        status["kind"] = kind
    targets = event.get("targets") or [s["id"] for s in config["sinks"]]
    for sink in config["sinks"]:
        if sink["id"] not in targets:
            continue
        level = sink["options"].get("level", config["level"])
        board = sink["kind"] in ("discord", "file", "dir") or sink.get("format") == "telegram"
        try:
            if kind in LEVELS[level] and not folded and not (kind == "status" and board):  # a status goes to the board there
                deliver(sink, event, config, status)
            if sink["kind"] == "discord":
                discord_status(sink, status, config, words)
            elif sink.get("format") == "telegram":
                telegram_status(sink, status, config, words)
            elif sink["kind"] in ("file", "dir"):
                write_board(sink, status, config)
        except (RuntimeError, OSError, ValueError) as error:
            note_error(base, sink["id"], f"{kind}: {error}")
    status_file.write_text(json.dumps(status), encoding="utf-8")
    remember(base, [s for s in config["sinks"] if s.get("posted")])
    for sink in config["sinks"]:
        if sink.get("thread_id") and sink.get("status_id"):
            threads((sink_key(sink), config["session"], {"thread": sink["thread_id"], "status": sink["status_id"]}))
    with Locked(base / "config.lock"):
        current = load(base)
        if not current:
            return
        by_id = {s["id"]: s for s in config["sinks"]}
        for sink in current["sinks"]:  # what the delivery learned: the thread / topic, the status message
            for key in ("thread_id", "status_id", "started", "thread_state"):
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
        questions = data.get("tool_input", {}).get("questions", [])
        text = "\n".join(f"**{q.get('question', '')}**" for q in questions) or "?"
        choices = [o.get("label", "") + (f" · *{o['description'][:120]}*" if o.get("description") else "")
                   for q in questions[:1] for o in q.get("options", [])]
        return {"kind": "question", "text": text, "choices": choices}
    if name == "PermissionRequest":  # what exactly waits for an approval (the Notification only names the tool)
        tool, given = str(data.get("tool_name", "tool")), data.get("tool_input", {}) or {}
        detail = given.get("command") or given.get("file_path") or given.get("notebook_path") or given.get("url") or given.get("pattern") \
            or json.dumps(given, ensure_ascii=False)[:300]
        return {"kind": "permission", "text": f"**{tool}**\n```\n{str(detail)[:900]}\n```"}
    if name == "Notification":
        message = str(data.get("message", ""))
        if "permission" in message.lower() or data.get("notification_type") == "permission_prompt":
            return None  # PermissionRequest already said it, with the command
        idle = data.get("notification_type") == "idle_prompt" or "waiting for your input" in message.lower()
        return {"kind": "waiting", "text": message, "idle": True} if idle else {"kind": "status", "text": message}
    if name == "PostToolUseFailure":  # a tool that failed: logged, no mention (Claude goes on; a real block is sent as an error)
        error = str(data.get("error") or data.get("tool_response") or "")
        first = error.strip().splitlines()[0][:300] if error.strip() else "failed"
        return {"kind": "error", "text": f"{data.get('tool_name', 'tool')}: {first}", "quiet": True}
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

def mine(entry: dict) -> bool:
    """A hook entry of session-log (whatever the path the script was installed from)."""
    return any("session-log" in h.get("command", "") and h.get("command", "").endswith(" hook") for h in entry.get("hooks", []))

def hooked(file: Path) -> list[str]:
    """The events whose session-log hook is in a settings file."""
    try:
        hooks = json.loads(file.read_text(encoding="utf-8")).get("hooks", {}) if file.is_file() else {}
    except ValueError:
        return []
    return [name for name, entries in hooks.items() if any(mine(e) for e in entries)]

def settings_hooks(action: str, file: Path) -> None:
    """Add / remove / show the hooks of session-log in a Claude Code settings file (a backup first)."""
    settings = json.loads(file.read_text(encoding="utf-8")) if file.is_file() else {}
    hooks = settings.setdefault("hooks", {})
    command = f"python3 {SCRIPT} hook"
    present = hooked(file)
    if action == "show":
        missing = [name for name, _ in HOOK_EVENTS if name not in present]
        print(f"{file}: session-log hooks " + ("installed" if not missing else f"MISSING on {', '.join(missing)}") + f" ({command})")
        sys.exit(0 if not missing else 3)
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
def prepare(sink: dict, config: dict, session: str) -> None:
    """Before the first message of a target: what it is and where this session writes in it. Discord: forum or text
    channel (probe), the thread of the session reused; Telegram: the chat (getChat), the topic of the session in a forum
    supergroup, reused or made. Raises when the target cannot be reached (a deleted webhook, a wrong bot / chat)."""
    entry = threads().get(sink_key(sink), {}).get(session, {})
    given = sink["options"].get("thread", "")
    if sink["kind"] == "discord":
        sink["channel_type"] = probe(sink)
        if sink["channel_type"] == "text" and not given.isdigit():
            sink["thread_state"] = "text-channel"  # no thread_name tried: plain messages in the channel
    elif sink.get("format") == "telegram":
        sink["channel_type"] = tg_probe(sink)
        if sink["channel_type"] != "forum" or given in ("no", "none"):
            return
    else:
        return
    if given.isdigit() or (entry.get("thread") and sink["channel_type"] != "text"):  # a thread given / the one of this session
        sink["thread_id"], sink["thread_state"] = (given, "given") if given.isdigit() else (str(entry["thread"]), "reused")
        if sink["thread_state"] == "reused" and entry.get("status"):
            sink["status_id"] = entry["status"]  # the same dashboard, edited again
    elif sink.get("format") == "telegram":
        tg_topic(sink, config)

def start(args: argparse.Namespace, session: str) -> None:
    missing = [name for name, _ in HOOK_EVENTS if name not in hooked(SETTINGS)]
    if missing:  # the hooks are part of the log: ask the user to install them, a refusal cancels the log
        sys.exit(f"session-log: hooks not installed in {SETTINGS} ({', '.join(missing)}): ask the user, then "
                 "`log.py hooks install`; refused -> no log is started")
    base = folder(session)
    with Locked(base / "config.lock"):
        config = load(base) or {"session": session, "name": args.name or f"{Path.cwd().name} · {session[:8]}", "cwd": str(Path.cwd()),
                                "level": args.level, "lang": args.lang, "summary": not args.no_summary, "sinks": [], "started": time.time()}
        if args.name:
            config["name"] = args.name
        if (base / "status.json").is_file():  # this session logged before: its duration counts from that first start
            config["started"] = min(config["started"], json.loads((base / "status.json").read_text(encoding="utf-8")).get("started", config["started"]))
        first = max([int(s["id"][1:]) for s in config["sinks"]], default=0) + 1
        new = [parse_sink(spec, first + i) for i, spec in enumerate(args.sinks)]
        config["sinks"] += new
        save(base, config)
    event = {"kind": "start", "time": time.time(), "text": f"{config['name']} · {config['cwd']}" + (f"\n{args.text}" if args.text else "")}
    words = LABELS[config["lang"]]
    failed = 0
    for sink in new:  # the first message right away: a wrong address is said now, not later
        known = threads().get(sink_key(sink), {}).get(session, {})
        back = bool(known.get("seen") or known.get("thread"))  # this session already logged there: "Connexion rétablie"
        try:
            prepare(sink, config, session)
            deliver(sink, {**event, "title": words["restart"] if back else "", "reconnect": back}, config)
            print(f"ok   {sink['id']} {kind_of(sink):8} {shown(sink)}" + thread_note(sink))
        except (RuntimeError, OSError, ValueError) as error:
            failed += 1
            sink["failed"] = True
            print(f"FAIL {sink['id']} {kind_of(sink):8} {shown(sink)}: {error}")
            continue
        threads((sink_key(sink), session, {"seen": True, "kind": kind_of(sink), "where": shown(sink), "thread": sink.get("thread_id")}))
    remember(base, new)
    with Locked(base / "config.lock"):
        current = load(base) or config
        by_id = {s["id"]: s for s in new}
        # a target that failed is left out: started again once fixed, it is not there twice
        current["sinks"] = [by_id.get(s["id"], s) for s in current["sinks"] if not by_id.get(s["id"], {}).get("failed")]
        if current["sinks"]:
            save(base, current)
        else:
            (base / "config.json").unlink(missing_ok=True)
    print(f"session {session[:8]} · level {config['level']} · {len(current['sinks'])} sink(s)" + (f" · {failed} failed" if failed else ""))
    if failed:
        sys.exit(1)

def thread_note(sink: dict) -> str:
    """What start says of where the session writes: the Discord channel / thread, the Telegram chat / topic."""
    state, where = sink.get("thread_state", ""), sink.get("channel_type", "")
    if sink.get("format") == "telegram":
        place = {"forum": "forum supergroup", "private": "private chat", "group": "group", "supergroup": "group", "channel": "channel"}.get(where, where)
        if state == "no-topic":
            return (f"  telegram {place}: no topic ({sink.get('topic_error', '')}) - the bot must be an admin allowed to manage"
                    " topics; the messages carry the session name")
        if where != "forum":
            return f"  telegram {place} (the messages carry the session name)"
        return f"  telegram {place}" + thread_note_rest(sink, state, "topic")
    if state == "text-channel":
        return ("\n     no thread: a text channel (a webhook makes threads in a forum channel only) - a forum channel's webhook,"
                " or a thread made by hand there and #thread=<its id>")
    kind = {"forum": "  forum channel", "text": "  text channel"}.get(where, "")
    warn = ""
    if where == "forum" and sink["options"].get("mention", "here") in ("here", "everyone"):
        warn = ("\n     @here in a thread only notifies its members: follow the post (Follow), or #mention=<your user id> (a mention"
                " adds you to the thread)")
    return kind + thread_note_rest(sink, state, "thread") + warn

def thread_note_rest(sink: dict, state: str, noun: str) -> str:
    notes = {"new": f"new {noun}", "reused": f"the {noun} of this session, reused", "given": f"the {noun} given", "lost": f"its {noun} was gone: a new one"}
    return f" ({notes.get(state, state)} {sink.get('thread_id', '')})" if state else ""

def clear(session: str) -> None:
    """Delete what the log of this session posted where a sender may delete it - Discord (the webhook's messages) and
    Telegram (the bot's messages; within 48 h in a group) - even after a stop; says the other places (a file is the
    user's to delete; Slack, ntfy, Gotify and sockets cannot be undone by the sender)."""
    base = folder(session)
    file = base / "posted.jsonl"
    items = [json.loads(line) for line in file.read_text(encoding="utf-8").splitlines() if line.strip()] if file.is_file() else []
    for sink in (load(base) or {}).get("sinks", []):
        if sink.get("status_id"):
            items.append({"kind": kind_of(sink), "url": sink.get("tg") or sink["url"], "chat": sink.get("chat"), "id": sink["status_id"],
                          "thread": sink.get("thread_id")})
    done, gone, seen = 0, 0, set()
    for item in items:
        kind, url = item.get("kind", "discord"), item.get("url") or item.get("hook")  # "hook": the lines written before Telegram
        if (kind, str(item["id"])) in seen:
            continue
        seen.add((kind, str(item["id"])))
        try:
            if kind == "telegram":
                request("POST", f"{url}/deleteMessage", json.dumps({"chat_id": item["chat"], "message_id": int(item["id"])}).encode(), "application/json")
            else:
                request("DELETE", url + f"/messages/{item['id']}" + (f"?thread_id={item['thread']}" if item.get("thread") else ""), b"", "application/json")
            done += 1
        except OSError as error:
            sys.stderr.write(f"session-log: {item['id']}: {error}\n")
        except RuntimeError as error:
            if not re.search(r"HTTP 404|message to delete not found|MESSAGE_ID_INVALID", str(error)):
                sys.stderr.write(f"session-log: {item['id']}: {error}\n")
                continue
            gone += 1
    file.unlink(missing_ok=True)
    with Locked(base / "config.lock"):
        config = load(base)
        if config:
            for sink in config["sinks"]:
                sink.pop("status_id", None)
            save(base, config)
    print(f"{done} message(s) deleted" + (f", {gone} already gone" if gone else ""))
    kept = sorted({(e.get("kind", ""), e.get("where", "")) for sessions in threads().values() for s, e in sessions.items()
                   if s == session and e.get("kind") not in ("discord", "telegram")})
    for kind, where in kept:
        print(f"kept: {kind} {where}" + (" (yours to delete)" if kind in ("file", "dir") else " (cannot be deleted by the sender)"))

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
    sub.add_parser("clear", help="delete the Discord messages this session's log posted")
    five = sub.add_parser("hooks", help="install / remove / show the Claude Code hooks")
    five.add_argument("action", choices=["install", "uninstall", "show"])
    five.add_argument("--settings", default=str(SETTINGS))
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
    elif args.command == "clear":
        clear(args.session)
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
            plain_file = args.file and args.kind == "step" and not STEP.match(args.text)
            enqueue(args.session, {"kind": "file" if plain_file else args.kind, "title": args.title, "text": args.text,
                                   "files": [str(Path(f).absolute()) for f in args.file], "link": args.link})
        elif args.command == "status":
            enqueue(args.session, {"kind": "status", "text": args.text})
        else:
            ids = [s["id"] for s in config["sinks"]]
            if args.target != "all" and args.target not in ids:
                sys.exit(f"session-log: no sink {args.target} (sinks: {', '.join(ids)})")
            enqueue(args.session, {"kind": "stop", "text": args.text, "targets": ids if args.target == "all" else [args.target]})
        spawn(args.session)
        print("queued")

if __name__ == "__main__":
    main()
