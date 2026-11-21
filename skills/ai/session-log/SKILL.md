---
name: session-log
description: Log the progress of the current Claude session to several places at once, each in its own way - Discord (a thread per session, mentions, a status edited in place, files), Telegram (a topic per session, sound only when needed, status edited in place), Slack, ntfy, Gotify, an API, a TCP / UDP socket, a file or folder (text, Markdown or JSON journal, a status file next to it); the request and each end of turn summarized by Haiku, steps, results, files and links sent by Claude, questions, permissions and errors by Claude Code hooks; what waits for the user calls them the way of the place. /session-log with targets starts it, /session-log stop ends it, nothing given = asked through interactive-questions (Discord recommended); its hooks are installed at the first launch once the user accepts, a refusal cancels. Use whenever the user wants to log, follow or mirror a session's progress to Discord, Telegram, Slack, a phone, a webhook, a file, an endpoint or a port, or summons /session-log.
metadata:
  summary: 'Logs the progress of the current session to one or several targets at once, each in its own way: Discord (a thread per session reused, @here / the user mentioned, a status edited in place, files attached), Telegram (a topic per session, sound only when the user is needed, status edited in place, photos and documents), Slack, ntfy, Gotify, a JSON API, TCP / UDP sockets, a .log / .md / .jsonl file or a folder (a status file rewritten next to it); Claude Code hooks send the request and the end of each turn (summarized by Haiku, no fork), the questions, permissions and errors, Claude sends its steps, results, files and links; whatever waits for the user calls them (mention, sound, high priority, "à toi"); `/session-log <target>...` starts, `/session-log stop` ends, `clear` deletes what can be; the hooks installed at launch after asking (refused = no log).'
---

# Session log: the progress of this session, sent where the user wants

`SKILL_DIR` = the directory of this file. `log.py` = `python3 SKILL_DIR/scripts/log.py` (Python 3 only, `claude` for the
Haiku summaries). It needs a shell on the user's computer (Claude Code, the desktop app, claude-cluster); claude.ai on the
web cannot run it: say so and stop there. Outbound only: to receive requests from another session, `session-link`.

## 1. Read what is given
`/session-log <info> <info>...` - each info is a target, an option or a command:

| Given | Means |
|---|---|
| `stop`, `close`, `off`, `fin` (+ `s2`) | stop every target (or that one): section 5 |
| `status`, `list` | `log.py list`: the targets of this session and the last errors |
| `uninstall` | `log.py hooks uninstall`: the hooks out of `~/.claude/settings.json` |
| `clear` | `log.py clear`: delete what this session's log posted where a sender may (Discord, Telegram), even after a stop |
| a Discord webhook URL (`https://discord.com/api/webhooks/...`) | Discord |
| `https://api.telegram.org/bot<token>/sendMessage?chat_id=<id>` | Telegram (a bot) |
| a Slack / Mattermost webhook, `https://ntfy.sh/<topic>`, a Gotify `…/message?token=…` | that service (read from the address, or `#format=slack|ntfy|gotify`) |
| another `https://...` | an API: one JSON record per event (`#format=text`: the readable lines) |
| `tcp://HOST:PORT`, `udp://HOST:PORT` (`ip:port` + the word tcp / udp) | a socket: one JSON line per event (`#format=text`: coloured readable lines) |
| a path: `~/logs/work.log` (aligned text), `work.md` (a Markdown journal), `x.jsonl` (JSON lines), `~/logs/` (a folder: a Markdown journal + a copy of each file sent) | a file / a folder (a status file rewritten next to it) |
| `discord`, `telegram`, `ntfy`, `file`, `api`, `tcp`, `udp` alone | that kind, its value asked (section 2) |
| `level=minimal|key|all`, `mention=here|<id>|role:<id>|no`, `name=<name>` | options (per target: after `#`, ex: `<url>#mention=1234,level=all`) |

Other options after `#`: `thread=<id>` (write in that Discord thread / Telegram topic), `thread=no` (none),
`ping=question+permission+...` (the kinds that call the user), `maxmb=N` (attachments: Discord 10, ntfy 15), `upload`
(JSON API: files in a multipart body), `header=Authorization:Bearer X` (API, several with `;`), `color=yes|no` (ANSI: a
text socket yes, a `.log` no), `bell` (a text socket rings on what calls), `emoji=no`, `avatar=<url>|no`, `format=...`.

### Each destination in its own way
The same facts per event (icon, label, text, the progress of an `Étape n/m`, the choices of a question, the counters at
the end, whether it waits for the user), rendered the way of each place:

| Destination | Grouped by session | Calls the user (questions, permissions, idle, done, blocking error, end) | Live status | Files, links | `clear` |
|---|---|---|---|---|---|
| Discord | a thread per session in a forum channel, reused | `@here` / `<@id>` / role, with a text the phone shows | a dashboard message edited in place | attached, image shown inside; link as `🔗 github.com › user › repo` | yes |
| Telegram | a topic per session in a forum supergroup (made, reused), else the session name on each message | sound and notification; the rest silent; `<@id>` link when an id is given | a message edited in place | photos / documents with the text as caption; `<a>` link | yes |
| Slack / Mattermost | the session in a context line | `<!here>` / `<@U…>` / user group | (level `all`: status messages) | path; `<url\|github.com › …>` | no |
| ntfy (phone) | the session in the title | priority 5 (question, permission, idle) / 4 (done, error, end), the rest 2 (no sound) | — | each file attached; click + an « Ouvrir » button | no |
| Gotify (phone) | the session in the title | priority 8, the rest 2 | — | path; a click opens the link | no |
| JSON API, socket JSON, `.jsonl` | `session`, `name` in each record | `attention: true`, `priority: high` | `status` in each record; `.status.json` next to a `.jsonl` | `files` (name, size, sha256), `link_label`; `upload` | no |
| `.log`, socket `format=text` | a banner per session (« Connexion rétablie » when it comes back) | `◀ à toi` (yellow on a socket, `bell` rings) | `<log>.status` rewritten | `📎 path`, `🔗 label  url` | no (a file is the user's) |
| `.md`, a folder | a heading per session | `👉 à toi` | `<log>.status.md` rewritten (one block per session) | image shown, files copied (folder), links | no |

Every target: **Connexion établie** at start, **Connexion rétablie** when this session already logged there, **Fin du
log** (duration, steps, errors, files) at the end; the progress line has the shape of the Discord one
(`━━━━━━━━──────── 2/4 · 50 %`).

## 2. Ask what is missing (`interactive-questions`, never guessed)
Nothing given: one popup -
1. **Où logger** (multi): `Discord webhook (Recommended)` · `Telegram (bot)` · `Fichier sur la machine` ·
   `Téléphone (ntfy)`; *Other* for Slack, Gotify, an API, a socket.
2. **Détail**: `Étapes clés (Recommended)` (the request, each step, errors, questions, the end and the files) ·
   `Minimal` (start, end, errors, questions; the steps only in the status) · `Tout` (every error, the status changes too).

Then the values, each asked when not given (an option `Je la colle (Other)` to paste it):
- **Discord**: the webhook URL - channel settings > Integrations > Webhooks > New webhook > Copy URL. `start` finds by
  itself, without posting anything, what the webhook writes into (forum / text channel) and whether it still exists (a
  deleted webhook fails at once and is left out). **One thread per session by default**: in a **forum** channel the
  thread is made at the first message, remembered (`~/.local/state/session-log/threads.json`) and used again - with the
  same status message - whenever this session logs there again (a new `start`, a resumed session); a thread deleted or
  locked is replaced by a new one. A **text** channel cannot get a thread from a webhook (Discord error 220003):
  recommend a forum channel, or a thread made by hand with `#thread=<its id>` (right click > Copy Thread ID).
- **Telegram**: a bot (BotFather > /newbot > the token) and the chat: the user writes to the bot (or adds it to a group),
  then `https://api.telegram.org/bot<token>/getUpdates` shows the chat id. A **forum supergroup** (topics on) with the
  bot as an admin allowed to manage topics gets one topic per session, reused; elsewhere the messages carry the session
  name. The token is a password: kept `0600`, never shown.
- **ntfy**: a topic name hard to guess (anyone who knows it reads it): `https://ntfy.sh/<topic>` or a self-hosted
  server (`#header=Authorization:Bearer tk_…`); the user subscribes to it in the ntfy app.
- **Mention** (Discord, Slack, Telegram), asked with the URL: `@here (Recommended)` · `Un ID` (*Other*: the user's ID,
  or `role:<id>`) · `Pas de mention` -> `#mention=here` (the default), `#mention=<id>`, `#mention=role:<id>`,
  `#mention=no`. On Discord, **@here in a thread only notifies the thread's members**: in a forum, the user follows
  the post, or gives their ID (a mention adds them to the thread) - `start` warns about it. ntfy and Gotify call by
  priority, Telegram by sound: no ID needed. A webhook only sends: answering stays in the terminal.
- **File**: the path (default `~/.local/state/session-log/logs/<project>.log`), `.md` for a journal, `.jsonl` for JSON
  lines, a folder for copies.
- **API / Slack / Gotify**: the URL, a header if it needs a key, whether files are uploaded.
- **Socket**: host, port, TCP or UDP, JSON or text. Any target outside this computer receives summaries of the requests:
  say it once. Every URL or token is a password: never written in a repository or a shared place, kept `0600`.

**Hooks, checked at each launch** (before anything else, stop and list excepted): `log.py hooks show` (exit 3 = missing).
Missing -> ask in the same popup as the rest: `Installer les hooks (Recommended)` · `Annuler`, saying what it does: they
go in `~/.claude/settings.json` (a backup `settings.json.bak-session-log` next to it) for UserPromptSubmit, the
AskUserQuestion questions, PermissionRequest, Notification, PostToolUseFailure, Stop, StopFailure, SessionEnd; they do
nothing in a session without a running log. Accepted -> `log.py hooks install`, then go on. Refused -> **cancel**: no log
is started, one line says so (`log.py start` refuses to run without them anyway). Installed now: this session sees them
once the user opens `/hooks` (or from the next session) - say it; until then only what Claude sends leaves.

## 3. Start
```bash
log.py hooks show || log.py hooks install    # only after the user accepted (section 2)
log.py start <target> [<target>...] [--name "<project> · <task>"] [--level key] [--text "<what this session is about>"]
```
Each target gets its first message at once and prints `ok` (with where the session writes: the Discord thread / Telegram
topic, new / reused / given, the kind of chat) or `FAIL <why>` (a wrong URL or token, a deleted webhook, a closed port):
a target that fails is left out; fix it and start it again. `start` again later adds targets to the running log. Tell
the user in one line what is logged where.

## 4. While working (what Claude sends)
The hooks send by themselves: the request (summarized by Haiku), the questions (they call the user), the permissions
asked (the tool and the command, they call), the errors of the tools (one per minute at `key`, the others counted, they
do not call), the end of each turn (summarized by Haiku), the idle session (it calls), the end of the session. Claude adds
what only it knows - one short line each, in the user's language:
```bash
log.py send "Étape 2/4 : parser écrit et testé"                       # a step: "n/m" gives the progress line everywhere
log.py status "Rendu Blender en cours (≈ 6 min)"                      # a long step starting: the live status only
log.py send "Rapport prêt" --file out/report.pdf                      # a result file (attached / copied where possible)
log.py send "Maquette" --kind info --link https://claude.ai/...       # an artefact, a PR, a page: its link
log.py send "Fait : ... ; reste : ..." --kind done                    # the end of the task: what, where, what is left
log.py send "Bloqué : ... (raison)" --kind error                      # an error that stops the work (it calls)
```
- The key steps only (one Bash call each), never a message about the logging itself.
- Never a secret, a key, a token, the content of a private file (`log.py` also hides the usual secret patterns), never
  the whole diff: names and results.

## 5. Stop
`/session-log stop` -> `log.py stop [s2] --text "<short summary of the session>"`: a last message 💤 **Fin du log**
(duration since the first start, steps, errors, files), the status turns to *Arrêté*, then that target (or all) stops;
the end of the session stops it too. `/session-log clear` -> `log.py clear`: the messages the log of this session posted
on Discord (the webhook's) and Telegram (the bot's; within 48 h in a group) are deleted - their ids are kept in
`posted.jsonl` (`0600`); it then lists what stays (Slack, ntfy, Gotify, sockets cannot be undone by the sender; the
files are the user's to delete). The hooks stay installed and idle (no running log = nothing done);
`/session-log uninstall` -> `log.py hooks uninstall` removes them.

## How it works
- State: `~/.local/state/session-log/<session id>/` (`config.json` `0600`, the queue, `errors.log`, `status.json`); the
  session is `$CLAUDE_CODE_SESSION_ID`, the hooks get it from Claude Code, so several sessions log at the same time.
  `threads.json` remembers per target and session the thread / topic, the status message and that it logged there.
- One queue per session and one worker in the background: the events leave in order, the session never waits; rate
  limits (Discord, Telegram) are waited for; a failing target is written to `errors.log` (`log.py list`), the others go on.
- Haiku: `claude -p --model haiku` in a separate process (no tools, no saved session, never a fork), only for a text
  longer than ~220 characters; it fails -> the text cut short. `--no-summary` at start turns it off.
- The live status: a message edited in place on Discord and Telegram; next to a log file, a status file rewritten at
  each event with one block per session that logs there (`boards.json` keeps them a week); in each JSON record.
