---
name: session-log
description: Log the progress of the current Claude session to one or several places at once - a Discord webhook (one thread per session, a ping when a question or a permission waits, a status message edited in place, files attached), a file or a folder on the machine, an API (JSON POST, files uploaded), a TCP / UDP socket ip and port; the request received and the end of each turn summarized by Haiku, the steps, results, files and artefact links sent by Claude, the questions, permission waits and errors sent by Claude Code hooks. /session-log with one or several targets starts it, /session-log stop or close ends it; nothing given, what to log to is asked through interactive-questions (Discord recommended); its hooks are installed at the first launch once the user accepts, a refusal cancels. Use whenever the user wants to log, follow, mirror or report the progress of a session to Discord, a webhook, a file, an endpoint or a port, or summons /session-log.
metadata:
  summary: 'Logs the progress of the current session to one or several targets at once: a Discord webhook (one thread per session, a ping when a question or a permission waits, a status message edited in place, files attached), a file or a folder, an API (JSON POST, uploads), a TCP / UDP socket; Claude Code hooks send the request and the end of each turn (summarized by Haiku, no fork), the questions, permission waits and errors, Claude sends its steps, results, files and artefact links; `/session-log <target> <target>...` starts, `/session-log stop` ends, nothing given = a QCM (Discord recommended); the hooks installed at launch when missing, after asking (refused = no log).'
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
| `clear` | `log.py clear`: delete every Discord message this session's log posted (even after a stop) |
| a Discord webhook URL (`https://discord.com/api/webhooks/...`) | Discord |
| another `https://...` | an API, its format read from the address (below) |
| `tcp://HOST:PORT`, `udp://HOST:PORT` (`ip:port` + the word tcp / udp) | a socket: one JSON line per event (`#format=text`: the readable lines) |
| a path: `~/logs/work.log` (aligned text), `work.md` (a Markdown journal), `x.jsonl` (JSON lines), `~/logs/` (a folder: a Markdown journal + a copy of each file sent) | a file / a folder |
| `discord`, `file`, `api`, `tcp`, `udp` alone | that kind, its value asked (section 2) |
| `level=minimal|key|all`, `mention=<user id>` or `role:<id>`, `name=<name>` | options (per target: after `#`, ex: `<url>#mention=1234,level=all`) |

Other options after `#`: `thread=<id>` (Discord: write in that existing thread), `thread=no` (no thread), `maxmb=25` (Discord attachments, 10 by default), `upload` (JSON API: files
in a multipart body), `header=Authorization:Bearer X` (API, several with `;`), `emoji=no` (no icons), `avatar=<url>|no` (Discord), `format=...`.

Each destination gets its own layout, from one set of facts per event (icon, label, text, the progress of an `n/m` step,
the choices of a question, the counters at the end):

| Destination | Layout |
|---|---|
| Discord | the session-log avatar, an embed per event: emoji + label, the session as author, Discord's timestamp, colour by kind; for `Étape n/m` a progress line (green done, grey left, `n/m · %`), the error in a code block, the numbered choices, an image shown inside, a link as a short clickable path (`🔗 github.com › user › repo`), the counters at the end; the **status** message edited in place as a small dashboard (state, progress, steps / errors / files, started and last activity as relative times) |
| Slack, Mattermost (`hooks.slack.com`, `#format=slack`) | mrkdwn section + a context line (session, time); `mention=<!here>` or `<@U…>` |
| ntfy (`ntfy.sh/<topic>`, `#format=ntfy`) | title = icon + label, Markdown body, priority high for questions, waits and errors, click = the link |
| Gotify (`…/message?token=…`) | title, Markdown message, priority 8 for the urgent ones |
| Telegram (`api.telegram.org/bot…/sendMessage?chat_id=…`) | HTML message (bold label, italics, code) |
| another API (`#format=json`, `text`) | the JSON record: raw fields + `line` (ready to show), `progress`, `choices`, `counters`, `state` |
| `.log` / socket `format=text` | a banner at the start, then one aligned line per event (date, icon + label, text, progress bar), details indented |
| `.md` / a folder | a Markdown journal: a heading per session, a bullet per event, images shown, files and links as links |

## 2. Ask what is missing (`interactive-questions`, never guessed)
Nothing given: one popup -
1. **Où logger** (multi): `Discord webhook (Recommended)` · `Fichier sur la machine` · `API (endpoint HTTP)` ·
   `Socket TCP / UDP (ip:port)`; *Other* for anything else.
2. **Détail**: `Étapes clés (Recommended)` (the request, each step, errors, questions, the end and the files) ·
   `Minimal` (start, end, errors, questions; the steps only in the status) · `Tout` (every error, the status changes too).

Then the values, each asked when not given (an option `Je la colle (Other)` to paste it):
- **Discord**: the webhook URL - channel settings > Integrations > Webhooks > New webhook > Copy URL. **One thread per
  session by default**: in a **forum** channel the thread is made at the first message, remembered
  (`~/.local/state/session-log/threads.json`) and used again - with the same status message - whenever this session
  logs there again (a new `start`, a resumed session); a thread deleted or locked is replaced by a new one. A **text**
  channel cannot get a thread from a webhook (Discord error 220003): recommend a forum channel, or a thread made by hand
  in it with `#thread=<its id>` (right click > Copy Thread ID); `start` says it when it falls back to plain messages. Optional: the user's
  Discord ID for the ping (Developer mode > right click on the name > Copy User ID). The URL is a password (anyone who
  has it can post there): never written in a repository or a shared place, kept `0600` by `log.py`.
- **File**: the path (default `~/.local/state/session-log/logs/<project>.log`), `.jsonl` for JSON lines, a folder for copies.
- **API**: the URL, a header if it needs a key, whether files are uploaded.
- **Socket**: host, port, TCP or UDP. Any target outside this computer receives summaries of the requests: say it once.

**Hooks, checked at each launch** (before anything else, stop and list excepted): `log.py hooks show` (exit 3 = missing).
Missing -> ask in the same popup as the rest: `Installer les hooks (Recommended)` · `Annuler`, saying what it does: they
go in `~/.claude/settings.json` (a backup `settings.json.bak-session-log` next to it) for UserPromptSubmit, the
AskUserQuestion questions, Notification, PostToolUseFailure, Stop, StopFailure, SessionEnd; they do nothing in a session
without a running log. Accepted -> `log.py hooks install`, then go on. Refused -> **cancel**: no log is started, one line
says so (`log.py start` refuses to run without them anyway). Installed now: this session sees them once the user opens
`/hooks` (or from the next session) - say it; until then only what Claude sends leaves.

## 3. Start
```bash
log.py hooks show || log.py hooks install    # only after the user accepted (section 2)
log.py start <target> [<target>...] [--name "<project> · <task>"] [--level key] [--text "<what this session is about>"]
```
Each target gets its first message at once - **Connexion établie** (or **Connexion rétablie** in the thread this session
already had) - and prints `ok` (with its thread: new / reused / given) or `FAIL <why>` (a wrong URL, a closed port); fix
and start it again. `start` again later adds targets to the running log. Tell the user in one line what is logged where.

## 4. While working (what Claude sends)
The hooks send by themselves: the request (summarized by Haiku), the questions (with the ping), the permission waits,
the errors of the tools (one per minute at `key`, the others counted), the end of each turn (summarized by Haiku), the end
of the session. Claude adds what only it knows - one short line each, in the user's language:
```bash
log.py send "Étape 2/4 : parser écrit et testé"                       # a step: "n/m" gives the progress bar everywhere
log.py status "Rendu Blender en cours (≈ 6 min)"                      # a long step starting: the live status only
log.py send "Rapport prêt" --file out/report.pdf                      # a result file (Discord: attached; folder: copied)
log.py send "Maquette" --kind info --link https://claude.ai/...       # an artefact, a PR, a page: its link
log.py send "Fait : ... ; reste : ..." --kind done                    # the end of the task: what, where, what is left
log.py send "Bloqué : ... (raison)" --kind error                      # an error that stops the work
```
- The key steps only (one Bash call each), never a message about the logging itself.
- Never a secret, a key, a token, the content of a private file (`log.py` also hides the usual secret patterns), never
  the whole diff: names and results.

## 5. Stop
`/session-log stop` -> `log.py stop [s2] --text "<short summary of the session>"`: a last message **Fin du log** (duration,
steps, errors, files), the status turns to *Arrêté*, then that target (or all) stops; the end of the session stops it too.
`/session-log clear` -> `log.py clear`: every Discord message the log of this session posted is deleted (their ids are kept
in `posted.jsonl`, `0600`); a webhook cannot read the channel, so only those it posted itself. The hooks stay installed and idle (no running log = nothing done);
`/session-log uninstall` -> `log.py hooks uninstall` removes them.

## How it works
- State: `~/.local/state/session-log/<session id>/` (`config.json` `0600`, the queue, `errors.log`); the session is
  `$CLAUDE_CODE_SESSION_ID`, the hooks get it from Claude Code, so several sessions log at the same time, each its own.
- One queue per session and one worker in the background: the events leave in order, the session never waits; Discord
  rate limits are waited for; a failing target is written to `errors.log` (`log.py list`), the others go on.
- Haiku: `claude -p --model haiku` in a separate process (no tools, no saved session, never a fork), only for a text
  longer than ~220 characters; it fails -> the text cut short. `--no-summary` at start turns it off.
- Discord: the status message is edited in place (Working / Waiting / Question / Done, steps, errors, last activity);
  the important events are messages of their own, coloured by kind.
