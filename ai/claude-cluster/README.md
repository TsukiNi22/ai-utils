# claude-cluster

Run and drive several Claude Code sessions in parallel, one or more per project, from one terminal or one window.
A **global session** manages the others (spawn, prompts, permissions, modes, tasks) through its own MCP server, by
text or by voice. Claude Code is the default agent; Ollama (local), Anthropic-compatible endpoints, OpenAI-compatible
ones (Qwen Code), opencode and the OpenAI Codex CLI work too.

### Table of Contents
 - [Dependencies](#dependencies)
 - [Installation](#installation)
 - [Usage](#usage)
 - [Backends](#backends)
 - [Global session](#global-session)
 - [Voice](#voice)
 - [Configuration](#configuration)
 - [How it works](#how-it-works)

## Dependencies

| Name | Version | Fedora (`dnf`) | Debian/Ubuntu (`apt`) |
|---|---|---|---|
| [libutils](https://github.com/TsukiNi22/libutils) | >= 3.0.0 | `libutils` | `libutils` |
| nlohmann/json | >= 3.10 | `json-devel` | `nlohmann-json3-dev` |
| toml++ | >= 3.0 | `tomlplusplus-devel` | `libtomlplusplus-dev` |
| Qt6 Widgets (optional: window) | >= 6.2 | `qt6-qtbase-devel` | `qt6-base-dev` |
| [FTXUI](https://github.com/ArthurSonzogni/FTXUI) (terminal) | 6.1.9 | fetched by CMake | fetched by CMake |
| clang++, CMake | C++20, >= 3.20 | `clang cmake` | `clang cmake` |
| an agent | | [`claude`](https://docs.claude.com/en/docs/claude-code) (default), `ollama`, `qwen`, `opencode`, `codex` | |

> [!NOTE]
> Without Qt6 the window front-end is not built (`--gui` then says so); the terminal one is always there.
> The voice uses the local `voice-listen` / `voice-say` tools (sherpa-onnx, Piper) and is optional.

## Installation

Built and installed by the `setup.sh` of the repository, like `xstyle` (binary in `~/.local/bin`, bash / zsh / fish
completion):

```bash
./setup.sh install claude-cluster          # from a clone of ai-utils
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install claude-cluster
./setup.sh remove claude-cluster
```

By hand: `cmake -S ai/claude-cluster -B build -DCMAKE_BUILD_TYPE=Optimized && cmake --build build --parallel && cmake --install build --prefix ~/.local`.

## Usage

```bash
claude-cluster                          # terminal interface when in a terminal, else a window
claude-cluster --gui                    # window (fails when Qt6 is not built in or there is no display)
claude-cluster --tty --layout grid      # terminal, sessions in a grid (list | grid | tabs)
claude-cluster --backend ollama/qwen3-coder   # backend of the new sessions for this run
claude-cluster --restore | --fresh      # previous sessions: restore / to the trash (default: asked)
claude-cluster serve                    # no interface: driven by the commands below and the MCP server
```

Headless commands, on the running instance (`--rtk` compact output for an AI, `--json` raw):

| Command | Effect |
|---|---|
| `list` | sessions: state, backend, tokens, context %, cost, pending permissions |
| `status <s>` | details and the last 20 messages (`<s>`: id, name or name prefix) |
| `spawn [folder]` | new session (`--name`, `--backend`, `--mode`, `--profile`, `--prompt`) |
| `send <s> <text...>` | prompt (queued when the session works; `--force` ignores the budget) |
| `allow <s> [allow\|always\|deny]` | answer the pending permission |
| `mode <s> <mode>` | `default`, `acceptEdits`, `plan`, `bypassPermissions` |
| `interrupt <s>`, `rename <s> <name>`, `cd <s> <folder>` | stop the turn, rename, move to another project |
| `close <s>`, `trash`, `restore <s>`, `purge [s]` | to the trash (kept `trash_days`), list, restore, delete for good |
| `task add [--target T] [--after t1,t2] <prompt>`, `task list`, `task cancel <t>` | task queue |
| `search <text>`, `export <s> [file]` | full-text search in every history, Markdown export |
| `remote on\|off` | Remote Control of the global session |
| `providers`, `notices` | backends and their state, last events |

In the interfaces: **Ctrl+K** opens the command palette (every action: new session, modes, panels, styles,
layouts, restore from the trash, providers, voice...), **Ctrl+N / Ctrl+P** next / previous session, **Ctrl+G** the
global session, **Ctrl+T** new session, **Ctrl+Y / Ctrl+D** allow / deny the permission, **Ctrl+C** interrupt,
**F2** layout, **F5** push-to-talk, **F6** stop speaking, **Ctrl+Q** quit (the sessions are kept); **PageUp /
PageDown** or the wheel scroll the transcript. Every key is set in the config.

Panels of a session (each one enabled per session from the palette): **tokens** (last prompt, current one, total),
**context** (used / window), **cost** (session, last turn, budget, 5 h / 7 d limits), **skills** (loaded, available),
**git** (branch, graph of the commits), **diff** (`git diff --stat` + diff), **tools** (timeline), **files** (modified).

## Backends

`--backend` / `spawn --backend` / `[agent] default` take `<provider>[/<model>]`:

| Provider | Driver | What | Credentials |
|---|---|---|---|
| `claude` = `anthropic` | claude | Claude Code, its own login (default) | `claude-cluster auth login anthropic` (= `claude auth login`) |
| `anthropic-api` | claude | Claude Code with an API key | key stored by `auth login` |
| `ollama` | claude | Claude Code on a local Ollama model (Anthropic API of Ollama) | none (`ollama serve`, `ollama pull <model>`) |
| `openrouter`, `deepseek`, `kimi`, `zai`, `minimax` | claude | Claude Code on an Anthropic-compatible endpoint | key |
| `openai` | qwen | Qwen Code on the OpenAI API | key |
| `ollama-openai` | qwen | Qwen Code on a local Ollama model (OpenAI API) | none |
| `opencode` | opencode | opencode and its providers (`opencode/<provider>/<model>`) | `opencode auth login` |
| `codex` | codex | OpenAI Codex CLI | `codex login` |
| own `[providers.<name>]` | claude / qwen | any Anthropic- or OpenAI-compatible endpoint (LM Studio, vLLM...) | optional key |

```bash
claude-cluster auth list                 # every provider: ready or not, and why
claude-cluster auth login openrouter     # API key asked without echo, stored in the keyring (secret-tool) or credentials.json (0600)
claude-cluster auth logout openrouter    # key removed (login providers: their own logout)
```

> [!IMPORTANT]
> The **claude** driver is the complete one: one process per session, live permissions in the interface, the MCP
> server, Remote Control. The other drivers start one process per turn (the conversation is resumed with its id):
> their permissions follow the mode (`plan` read-only ... `bypassPermissions`), they can't be asked live.

> [!WARNING]
> The cost shown for a non-Anthropic provider through Claude Code is a guess of claude (Claude prices): it is shown as
> `local / unknown`. A small local model may not follow the tools of Claude Code well (7B+ coder models recommended).

## Global session

A session of its own (`[global]`: folder, backend - a claude-driver one -, model) with:
- its **system prompt** `~/.config/claude-cluster/global.md` (written once, customizable), added to the global `CLAUDE.md`;
- the **MCP server** `claude-cluster mcp`: `session_list`, `session_status`, `session_spawn`, `session_send`,
  `session_allow`, `session_set_mode`, `session_interrupt`, `session_close`, `session_restore`, `session_trash`,
  `session_rename`, `session_cd`, `task_add`, `task_list`, `task_cancel`, `search`, `export`, `providers`,
  allowed without asking (`--allowedTools mcp__claude-cluster`);
- autonomy (from its prompt): what the user asks explicitly is done without asking again; what it decides alone is
  proposed first;
- **Remote Control** (`remote on` / palette): Remote Control needs an interactive Claude Code, so the conversation
  goes on in a background session (`claude --bg --resume <id> --remote-control <name>`), reachable from claude.ai or
  the app; `remote off` stops it and resumes the conversation here.

## Voice

`[voice] enabled = true`, then one of three listening modes:

| Mode | Listening | Prompt |
|---|---|---|
| `push` | push-to-talk key (F5: start, F5 again: stop) | every sentence heard while listening |
| `auto` | always | every sentence heard |
| `wake` | always | only after the wake phrase (`wake_word = "ok claude"`, variants separated by commas): "ok claude, list the sessions" sends it; "ok claude" alone arms the listening `wake_seconds` (optional spoken `wake_reply`) and the next sentence is the prompt |

- `only_me = true`: only the enrolled voice of the user (`voice-enroll`).
- The transcription is shown `confirm_seconds` before it is sent (palette: send now / drop / correct); the global
  session gets it as `[voice] ...` and asks when a word looks wrong or the request is ambiguous.
- `session <name> <text>` (or `<name>: <text>`) goes straight to that session.
- Answers of the global session are spoken as a short summary, in their language (`voice_fr`, `voice_en`); F6 stops.
- The palette action **Voice: what is missing** lists what to set up (tools, enrollment, voices, English speech model).

## Configuration

`~/.config/claude-cluster/config.toml` (written commented on the first run, reloaded when saved): interface
(`frontend`, `layout`, `style`, default `panels`, `restore = ask | always | never`, `notify`), `[agent]` default backend
and permission mode, `[commands]` of the drivers, `[global]`, `[sessions]` (`trash_days`, `max_parallel` of the task
queue, `budget_usd`), `[voice]`, `[keys]`, own `[styles.<name>]` (built-in: `html-light`, `html-dark`, `nord`,
`gruvbox`, `solarized-light`, `mono`), `[profiles.<name>]` (folder, backend, mode, first prompt, extra arguments) and
own `[providers.<name>]`. See [`templates/config.toml`](templates/config.toml).

| Variable | Effect |
|---|---|
| `CLAUDE_CLUSTER_CONFIG_DIR` | config folder (default `~/.config/claude-cluster`) |
| `CLAUDE_CLUSTER_DATA_DIR` | state, trash, history (default `~/.local/share/claude-cluster`) |
| `CLAUDE_CLUSTER_SOCKET` | control socket (default `$XDG_RUNTIME_DIR/claude-cluster/control.sock`) |
| `CLAUDE_CLUSTER_BACKEND` | same as `--backend` |
| `CLAUDE_CLUSTER_RTK` | same as `--rtk` |

## How it works

```
claude-cluster (one per user: control socket)
├── Manager ── Session g (global) ── claude -p --input-format stream-json ... --mcp-config mcp.json
│          ├── Session s1 ── claude -p stream-json (permissions: --permission-prompt-tool stdio)
│          ├── Session s2 ── qwen -p ... -o stream-json -r <id>       (one process per turn)
│          └── Session s3 ── opencode run --format json -s <id>        (one process per turn)
├── Control (unix socket) <── headless commands, claude-cluster mcp (MCP server of the global session)
├── Voice ── voice-listen --json / voice-say
└── front-end: terminal (FTXUI) | window (Qt6) | none (serve)
```

- The agents run through `env -C <folder> VAR=...` (provider environment), every pipe end of the cluster is
  close-on-exec: a session never keeps the pipes of another one.
- History: `~/.local/share/claude-cluster/logs/<id>.jsonl` (search, export, restore); `state.json` keeps the
  sessions (agent session id for `--resume`, cost, tokens), the trash, the tasks and the interface choices.
