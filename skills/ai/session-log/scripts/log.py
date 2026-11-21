#!/usr/bin/env python3
"""Log the progress of a Claude session to Discord, a file, an API or a socket - several at once.

    log.py start SINK [SINK...] [--name N] [--level minimal|key|all] [--lang fr|en] [--no-summary]
    log.py send TEXT [--kind step|done|info|question|error] [--file PATH]... [--link URL]
    log.py status TEXT                  # the live status (one Discord message edited in place)
    log.py stop [SINK_ID|all] [--text TEXT]
    log.py list | clear                 # clear: delete the Discord messages this log posted
    log.py hooks install|uninstall|show [--settings FILE]
    log.py hook                         # what the Claude Code hooks call (event JSON on stdin)

SINK: a Discord webhook URL, an https:// address (an API: Slack, ntfy, Gotify, Telegram read from the address, else one
JSON record per event), tcp://HOST:PORT or udp://HOST:PORT (one JSON line per event), a file (.log: aligned text, .md:
a Markdown journal, .jsonl: JSON lines), a folder ending with / (a Markdown journal + a copy of each file sent).
Options after #, comma separated: level=minimal|key|all, mention=here|<user id>|role:<id>|no (Discord: who is pinged
on what waits for the user - questions, permissions, the idle session, done, a blocking error, the end; here by default;
Slack: <!here> or <@U...>), ping=question+permission+waiting+done+error+stop (the kinds that ping), thread=<id>|no,
maxmb=N (Discord attachments, 10 by default), upload (JSON API: files in a multipart body), header=Name:Value;Name:Value
(API), format=json|slack|ntfy|gotify|telegram|text (API), format=json|text (socket), emoji=no (no icons), avatar=<url>|no.
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
AVATAR = "https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/skills/ai/session-log/assets/avatar.png"

STATE = Path(os.environ.get("SESSION_LOG_DIR", Path.home() / ".local" / "state" / "session-log")).expanduser()
SETTINGS = Path(os.environ.get("SESSION_LOG_SETTINGS", Path.home() / ".claude" / "settings.json")).expanduser()
DISCORD = re.compile(r"^https?://[^/]+/api(?:/v\d+)?/webhooks/\d+/[\w-]+$")
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
URGENT = ("question", "permission", "waiting", "error")  # a higher priority where the destination has one (ntfy, Gotify)
LABELS = {
    "fr": {"start": "Connexion établie", "restart": "Connexion rétablie", "stop": "Fin du log", "request": "Demande reçue", "step": "Étape", "done": "Terminé",
           "question": "Question en attente", "permission": "Permission demandée", "waiting": "Claude t'attend", "error": "Erreur", "turn": "Fin de tour", "file": "Fichier",
           "info": "Info", "status": "Statut", "steps": "Étapes", "errors": "Erreurs", "files": "Fichiers", "last": "Dernière activité",
           "started_at": "Démarré", "duration": "Durée", "folder": "Dossier", "level": "Niveau", "targets": "Cibles",
           "more_errors": "autres erreurs", "too_big": "trop gros pour être joint", "open": "Ouvrir",
           "working": "En cours", "waiting_you": "En attente de toi", "stopped": "Arrêté", "answer": "Réponds dans la session", "idle": "La session attend ton prochain message"},
    "en": {"start": "Connected", "restart": "Reconnected", "stop": "Log ended", "request": "Request received", "step": "Step", "done": "Done",
           "question": "Question waiting", "permission": "Permission needed", "waiting": "Claude is waiting", "error": "Error", "turn": "End of turn", "file": "File",
           "info": "Info", "status": "Status", "steps": "Steps", "errors": "Errors", "files": "Files", "last": "Last activity",
           "started_at": "Started", "duration": "Duration", "folder": "Folder", "level": "Level", "targets": "Targets",
           "more_errors": "more errors", "too_big": "too big to attach", "open": "Open",
           "working": "Working", "waiting_you": "Waiting for you", "stopped": "Stopped", "answer": "Answer in the session", "idle": "The session waits for your next message"},
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
        return {**sink, "kind": "api", "url": raw, "format": api_format(raw, options)}
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
    if parts.netloc == "api.telegram.org" and parts.path.endswith("/sendMessage"):
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
    """Keep the id of each Discord message posted (posted.jsonl, 0600: it holds the webhook) for `log.py clear`."""
    lines = [json.dumps({"hook": s["url"], "id": i, "thread": th}) for s in sinks for i, th in s.pop("posted", [])]
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
    parts = urllib.parse.urlsplit(url)
    crumbs = [parts.netloc.removeprefix("www.")] + [p for p in parts.path.split("/") if p][:3]
    return f"🔗 [{' › '.join(crumbs)}]({url})"

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
    """Who is mentioned on an event that waits for the user (#mention=here by default, an id, role:<id>, no) and the
    allowed_mentions that lets exactly that one ping; ("", {}) when the event pings nobody (#ping=kind+kind narrows)."""
    mention = sink["options"].get("mention", "here")
    kinds = sink["options"].get("ping", "+".join(PING)).split("+")
    if mention == "no" or event["kind"] not in kinds or event.get("quiet"):
        return "", {"parse": []}
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
    label = {"working": words["working"], "waiting": words["waiting_you"], "question": words["question"], "error": words["error"],
             "done": words["done"], "stopped": words["stopped"]}[state]
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
    question, counters and duration at the end, the start details."""
    words, kind, text = LABELS[config["lang"]], event["kind"], event["text"]
    key = "link" if event.get("link") and kind == "info" else kind
    out = {"kind": kind, "icon": "" if sink["options"].get("emoji") == "no" else ICONS.get(key, ""),
           "label": event.get("title") or words.get(kind, kind), "text": text, "progress": None, "choices": event.get("choices", []),
           "link": event.get("link", ""), "files": [file_info(f) for f in event.get("files", [])], "counters": None, "time": event["time"]}
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

def plain_bar(progress: list | None) -> str:
    if not progress:
        return ""
    done, total = progress
    full = round(10 * min(done, total) / max(total, 1))
    return f"[{'▰' * full}{'▱' * (10 - full)} {done}/{total}]"

def text_lines(f: dict, config: dict) -> list[str]:
    """A readable log: the start as a banner, then one aligned line per event (date, icon + label, text, progress),
    the other lines of a text, the choices, the files, the link and the counters indented under it."""
    when = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(f["time"]))
    pad = " " * 22
    if f["kind"] == "start":
        head = [f"── {f['icon']} {config['name']} · {when} ".replace("  ", " ") + "─" * 20]
        return head + [f"{pad}{k}: {v}" for k, v in (f["counters"] or {}).items()] + ([f"{pad}{f['text']}"] if f["text"] else [])
    label = f"{f['icon']} {f['label']}".strip()
    body = restyle(f["text"], "plain").splitlines() or [""]
    lines = [f"{when}  {label:<24} {body[0]}" + (f"  {plain_bar(f['progress'])}" if f["progress"] else "")]
    lines += [f"{pad}{' ' * 23}{line}" for line in body[1:]]
    lines += [f"{pad}{' ' * 23}{n}. {restyle(c, 'plain')}" for n, c in enumerate(f["choices"], 1)]
    lines += [f"{pad}{' ' * 23}📎 {x['path']}" for x in f["files"]] + ([f"{pad}{' ' * 23}🔗 {f['link']}"] if f["link"] else [])
    if f["counters"]:
        lines.append(f"{pad}{' ' * 23}" + " · ".join(f"{k} {v}" for k, v in f["counters"].items()))
    if f["kind"] == "stop":
        lines.append("─" * 60)
    return lines

def markdown(f: dict, config: dict, files: list[str]) -> str:
    """A Markdown journal: a heading per session, one bullet per event (time, icon, bold label, text), the choices and
    the counters as sub-lists, images shown, other files and links as links."""
    when = time.strftime("%H:%M:%S", time.localtime(f["time"]))
    if f["kind"] == "start":
        head = f"\n## {f['icon']} {config['name']} · {time.strftime('%Y-%m-%d %H:%M', time.localtime(f['time']))}\n\n"
        return head + "".join(f"- **{k}**: `{v}`\n" for k, v in (f["counters"] or {}).items()) + (f"\n{f['text']}\n" if f["text"] else "") + "\n"
    text = f["text"].replace("\n", "  \n  ")
    if f["kind"] == "error":
        text = f"`{text}`"
    line = f"- `{when}` {f['icon']} **{f['label']}**" + (f" · {text}" if text else "") + (f" `{plain_bar(f['progress'])}`" if f["progress"] else "") + "\n"
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
    """The event as JSON (generic API, socket, .jsonl): the raw fields plus what a consumer shows as is (line)."""
    out = {"session": config["session"], "name": config["name"], "cwd": config["cwd"], "kind": f["kind"], "icon": f["icon"],
           "label": f["label"], "text": f["text"], "line": f"{f['icon']} {f['label']}".strip() + (f" · {f['text']}" if f["text"] else ""),
           "time": time.strftime("%Y-%m-%dT%H:%M:%S%z", time.localtime(f["time"])), "files": f["files"], "link": f["link"]}
    for key in ("progress", "choices", "counters"):
        if f[key]:
            out[key] = f[key]
    if status:
        out["state"] = STATE_OF.get(status.get("kind", ""), "working")
    return out

def chat_text(f: dict, markup: str, label: bool = True) -> str:
    """One message for a chat (Slack mrkdwn, Telegram HTML, ntfy / Gotify Markdown): the label in bold, the text, the
    progress, the choices, the counters, the links."""
    def bold(text: str) -> str:
        if markup == "html":
            return f"<b>{html_escape(text)}</b>"
        return f"*{text}*" if markup == "slack" else f"**{text}**"
    def esc(text: str) -> str:
        return restyle(text, markup)

    one = f["text"] and "\n" not in f["text"]
    lines = [f"{f['icon']} {bold(f['label'])}".strip() + (f" · {esc(f['text'])}" if one else "")] if label else ([esc(f["text"])] if one else [])
    if "\n" in f["text"]:
        lines += [esc(line) for line in f["text"].splitlines()]
    if f["progress"]:
        lines.append(plain_bar(f["progress"]))
    lines += [f"{n}. {esc(c)}" for n, c in enumerate(f["choices"], 1)]
    if f["counters"]:
        lines.append(" · ".join(f"{bold(k)} {esc(v)}" for k, v in f["counters"].items()))
    lines += [f"📎 {html_escape(x['name']) if markup == 'html' else x['name']} ({x['path']})" for x in f["files"]]
    if f["link"]:
        lines.append(f"<{f['link']}>" if markup == "slack" else f["link"])
    return "\n".join(lines)

def restyle(text: str, markup: str) -> str:
    """The Markdown of a text (**bold**, *italic*, `code`) in the markup of the destination: Slack mrkdwn, Telegram
    HTML, or plain (a text log)."""
    if markup == "md":
        return text
    if markup == "html":
        text = html_escape(text)
        text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
        text = re.sub(r"(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])", r"<i>\1</i>", text)
        return re.sub(r"`([^`]+)`", r"<code>\1</code>", text)
    if markup == "slack":
        text = re.sub(r"(?<![\w*])\*(?!\s)([^*]+?)(?<!\s)\*(?![\w*])", r"_\1_", text)
        return re.sub(r"\*\*(.+?)\*\*", r"*\1*", text)
    return re.sub(r"\*\*(.+?)\*\*|(?<![\w*])\*(?!\s)([^*]+?)(?<!\s)\*(?![\w*])", lambda m: m.group(1) or m.group(2), text)

def html_escape(text: str) -> str:
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def api_body(sink: dict, f: dict, config: dict, status: dict | None) -> tuple[str, bytes]:
    """The URL and the JSON body an API expects (its format)."""
    form, mention = sink.get("format", "json"), sink["options"].get("mention", "")
    if form == "slack":
        text = chat_text(f, "slack")
        if f["kind"] in PING and mention:
            text = f"{mention} {text}"
        context = f"{config['name']} · {time.strftime('%H:%M:%S', time.localtime(f['time']))}"
        return sink["url"], json.dumps({"text": text, "blocks": [{"type": "section", "text": {"type": "mrkdwn", "text": text[:2900]}},
                                                                 {"type": "context", "elements": [{"type": "mrkdwn", "text": context}]}]}).encode()
    if form == "ntfy":  # JSON publishing: the topic in the body, posted to the server root
        parts = urllib.parse.urlsplit(sink["url"])
        body = {"topic": parts.path.strip("/"), "title": f"{f['icon']} {f['label']}".strip(), "message": chat_text(f, "md", label=False) or f["label"],
                "priority": 4 if f["kind"] in URGENT else 3, "markdown": True, "tags": [f["kind"]]}
        if f["link"]:
            body["click"] = f["link"]
        return f"{parts.scheme}://{parts.netloc}/", json.dumps(body).encode()
    if form == "gotify":
        return sink["url"], json.dumps({"title": f"{f['icon']} {f['label']} · {config['name']}".strip(), "message": chat_text(f, "md", label=False) or f["label"],
                                        "priority": 8 if f["kind"] in URGENT else 4,
                                        "extras": {"client::display": {"contentType": "text/markdown"}}}).encode()
    if form == "telegram":
        chat = dict(urllib.parse.parse_qsl(urllib.parse.urlsplit(sink["url"]).query)).get("chat_id", "")
        return sink["url"].split("?")[0], json.dumps({"chat_id": chat, "text": chat_text(f, "html")[:4000], "parse_mode": "HTML",
                                                      "disable_web_page_preview": True}).encode()
    if form == "text":
        return sink["url"], json.dumps({"text": "\n".join(text_lines(f, config))}).encode()
    return sink["url"], json.dumps(record(f, config, status)).encode()

def deliver(sink: dict, event: dict, config: dict, status: dict | None = None) -> None:
    """Send one event to one sink, in the format of that destination (raises on failure)."""
    words = LABELS[config["lang"]]
    kind = sink["kind"]
    if kind == "discord":
        discord(sink, event, config, words, status)
        return
    f = facts(sink, event, config, status)
    if kind == "api":
        headers = {k.strip(): v.strip() for k, v in (h.split(":", 1) for h in sink["options"].get("header", "").split(";") if ":" in h)}
        url, data = api_body(sink, f, config, status)
        files = [Path(x) for x in event.get("files", []) if Path(x).is_file()] if "upload" in sink["options"] and sink.get("format", "json") == "json" else []
        body, form = multipart({"payload_json": data.decode()}, files, "files") if files else (data, "application/json")
        request("POST", url, body, form, headers)
    elif kind in ("tcp", "udp"):
        text = "\n".join(text_lines(f, config)) if sink.get("format") == "text" else json.dumps(record(f, config, status), ensure_ascii=False)
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
            text = "\n".join(text_lines(f, config)) + "\n"
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
    """One event: summary, secrets hidden, then each sink whose level takes it, and the live status."""
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
        try:
            if kind in LEVELS[level] and not folded:
                deliver(sink, event, config, status)
            if sink["kind"] == "discord":
                discord_status(sink, status, config, words)
        except (RuntimeError, OSError, ValueError) as error:
            note_error(base, sink["id"], f"{kind}: {error}")
    status_file.write_text(json.dumps(status), encoding="utf-8")
    remember(base, [s for s in config["sinks"] if s["kind"] == "discord"])
    for sink in config["sinks"]:
        if sink["kind"] == "discord" and sink.get("thread_id") and sink.get("status_id"):
            threads((hook_id(sink), config["session"], {"thread": sink["thread_id"], "status": sink["status_id"]}))
    with Locked(base / "config.lock"):
        current = load(base)
        if not current:
            return
        by_id = {s["id"]: s for s in config["sinks"]}
        for sink in current["sinks"]:  # what the delivery learned: the thread, the status message
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
        first = max([int(s["id"][1:]) for s in config["sinks"]], default=0) + 1
        new = [parse_sink(spec, first + i) for i, spec in enumerate(args.sinks)]
        config["sinks"] += new
        save(base, config)
    event = {"kind": "start", "time": time.time(), "text": f"{config['name']} · {config['cwd']}" + (f"\n{args.text}" if args.text else "")}
    words = LABELS[config["lang"]]
    failed = 0
    for sink in new:  # the first message right away: a wrong address is said now, not later
        title = ""
        if sink["kind"] == "discord":
            try:
                sink["channel_type"] = probe(sink)
            except RuntimeError as error:
                failed += 1
                sink["failed"] = True
                print(f"FAIL {sink['id']} {sink['kind']:7} {shown(sink)}: {error}")
                continue
            if sink["channel_type"] == "text" and not sink["options"].get("thread", "").isdigit():
                sink["thread_state"] = "text-channel"  # no thread_name tried: plain messages in the channel
            given = sink["options"].get("thread", "")
            entry = threads().get(hook_id(sink), {}).get(session, {})
            known = entry.get("thread") or ""
            if given.isdigit() or (known and sink["channel_type"] != "text"):  # a thread given / the one of this session
                sink["thread_id"], sink["thread_state"] = (given, "given") if given.isdigit() else (known, "reused")
                if sink["thread_state"] == "reused" and entry.get("status"):
                    sink["status_id"] = entry["status"]  # the same dashboard, edited again
                threads((hook_id(sink), session, {"thread": sink["thread_id"]}))
                title = words["restart"] if sink["thread_state"] == "reused" else ""
        try:
            deliver(sink, {**event, "title": title}, config)
            print(f"ok   {sink['id']} {sink['kind']:7} {shown(sink)}" + thread_note(sink))
        except (RuntimeError, OSError, ValueError) as error:
            failed += 1
            sink["failed"] = True
            print(f"FAIL {sink['id']} {sink['kind']:7} {shown(sink)}: {error}")
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
    state = sink.get("thread_state", "")
    kind = {"forum": "  forum channel", "text": "  text channel"}.get(sink.get("channel_type", ""), "")
    if state != "text-channel" and kind:
        return kind + thread_note_rest(sink, state)
    return thread_note_rest(sink, state)

def thread_note_rest(sink: dict, state: str) -> str:
    if state == "text-channel":
        return ("\n     no thread: a text channel (a webhook makes threads in a forum channel only) - a forum channel's webhook,"
                " or a thread made by hand there and #thread=<its id>")
    notes = {"new": "new thread", "reused": "the thread of this session, reused", "given": "the thread given", "lost": "its thread was gone: a new one"}
    return f" ({notes.get(state, state)} {sink.get('thread_id', '')})" if state else ""

def clear(session: str) -> None:
    """Delete every Discord message the log of this session posted (posted.jsonl + the status messages), even after a
    stop; a message already deleted is skipped. The threads themselves stay (a webhook cannot delete them)."""
    base = folder(session)
    file = base / "posted.jsonl"
    items = [json.loads(line) for line in file.read_text(encoding="utf-8").splitlines() if line.strip()] if file.is_file() else []
    for sink in (load(base) or {}).get("sinks", []):
        if sink.get("status_id"):
            items.append({"hook": sink["url"], "id": sink["status_id"], "thread": sink.get("thread_id")})
    done, gone, seen = 0, 0, set()
    for item in items:
        if item["id"] in seen:
            continue
        seen.add(item["id"])
        url = item["hook"] + f"/messages/{item['id']}" + (f"?thread_id={item['thread']}" if item.get("thread") else "")
        try:
            request("DELETE", url, b"", "application/json")
            done += 1
        except OSError as error:
            sys.stderr.write(f"session-log: {item['id']}: {error}\n")
            continue
        except RuntimeError as error:
            if "HTTP 404" not in str(error):
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
            enqueue(args.session, {"kind": "stop", "text": args.text or config["name"], "targets": ids if args.target == "all" else [args.target]})
        spawn(args.session)
        print("queued")

if __name__ == "__main__":
    main()
