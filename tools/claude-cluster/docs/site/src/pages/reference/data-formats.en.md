+++
title = "Data formats"
summary = "Where claude-cluster keeps its files, and the format of each: state, transcripts, history, stash, taxonomy, usage, devices."
section = "reference"
order = 60
keywords = "data formats files state.json logs jsonl history stash taxonomy usage stats trusted banned hardware method rewind attachments folders paths"
+++

## Folders {#folders}

| Folder | Default | Override |
|---|---|---|
| config | `~/.config/claude-cluster` | `CLAUDE_CLUSTER_CONFIG_DIR` |
| data | `~/.local/share/claude-cluster` | `CLAUDE_CLUSTER_DATA_DIR` |
| socket | `$XDG_RUNTIME_DIR/claude-cluster/control.sock` | `CLAUDE_CLUSTER_SOCKET` |

The config and data folders are `0700`. Besides them an installed binary writes its icon and desktop file in the icon theme, and the service its systemd user unit.

## Config folder {#config}

| File | Content |
|---|---|
| `config.toml` | the configuration, written commented on the first run; only values change when saved ([reference](page:config-reference)) |
| `global.md`, `sessions.md` | the system prompts of the global session and of the sub-sessions (copies written once, yours to edit) |
| `machine.md` | the map of this computer |
| `vocabulary.txt`, `voice-words.txt` | the voice vocabulary and the send / cancel words |
| `credentials.json` | API keys when the keyring is not used (`0600`) |
| `web/` | `ca.pem` and its key (`0600`), the server certificate, `trusted.json`, `banned.json` (`0600`) |
| `android/release.jks` | the signing key of the Android app |

## Data folder {#data}

| File | Format and content |
|---|---|
| `state.json` | one JSON object: `sessions` (each: `id`, `name`, `cwd`, `backend`, `claude_id` for `--resume`, `model`, `mode`, `effort`, `panels`, `global`, `created`, `cost`, `tokens`, `summary`, `category`, `tags`, `turns`, `pin`...), `trash`, `tasks`, `schedules`, `next*` counters, `ui` |
| `logs/<id>.jsonl` | the transcript of a session, one JSON entry per line (kept within `transcript_limit_mb`, 25 MB; in memory its newest entries within `memory_mb`, 1 MB, all of them within `memory_total_mb`, 100 MB - the older ones read again from here when its top is reached); `logs/archive/` holds conversations ended by `/clear`; each entry has its `origin` (user, mcp, task, scheduled) |
| `history.jsonl` | the prompts you sent: `{time, session, text, images, pastes}` per line, newest `history_limit` (1000) kept |
| `stash.json` | the stashed prompts, global, with time and session |
| `taxonomy.json` | the forest of categories and tags: node to parent |
| `usage.jsonl` | the ledger of tokens and cost per turn, with the breakdown (input, cache written 5 min / 1 h, cache read, output, thinking) |
| `stats.jsonl` | activity events (start, a sign of life each minute, stop, tool, permission decision, limit, error) |
| `prices.json`, `limits.json` | prices learned from Claude Code; limit state |
| `orchestrator.json`, `hardware.json` | the job history (last `history`); the hardware scan with `scan_version` |
| `method.json` | work method detection: counter, the proposal waiting |
| `core.log`, `debug.log` | the core log; the debug trace (only with `[debug] enabled`) |
| `attachments/<session>/` | uploaded files, `0600` ([Attachments](page:attachments)) |
| `rewind/<session>-<time>.json` | what a rewind removed (the old leaf, the removed log lines) |
| `plugins/` | cloned sources and `index.json` ([Settings](page:settings)) |
| `icons/` | the SVG faces |
| `wake.py` | the detector of the wake phrase |

## Transcript entries {#entries}

An entry of `logs/<id>.jsonl` is a JSON object `{time, kind, text, tool, origin}`: `kind` is a number naming the type of entry (prompt, answer, tool call, thinking, display block, notice...), `tool` the tool concerned and `origin` where a prompt came from. The display blocks of the global session (`session_display`) are stored in it too, so they are searched and exported as text. Old transcripts without `origin` are read from the markers of their text (`[from the global session]`, `[reply from`, `[scheduled w`, `[tasks]`). `claude-cluster export <s> [file]` writes Markdown; `claude-cluster search <text>` searches every history.

## Interchange {#interchange}

Everything is plain JSON, JSONL, TOML or Markdown: nothing needs claude-cluster to be read. The conversation of the agent itself stays in the agent's own store (for Claude Code `~/.claude/projects`): claude-cluster resumes it by id and never rewrites it, except the explicit `session_edit` and rewind, which write a new conversation next to the old one.
