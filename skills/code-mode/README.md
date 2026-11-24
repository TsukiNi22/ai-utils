# Code mode skills

<img src="code-mode/assets/logo.png" alt="code mode" width="120">

The local MCP servers driven by code. A direct MCP call puts its whole result in the context and costs one model
turn; with `code-mode` one Python script calls the same tools and **only what it prints comes back**. The idea is the
one of Cloudflare ([Code Mode](https://blog.cloudflare.com/code-mode/)) and Anthropic
([Code execution with MCP](https://www.anthropic.com/engineering/code-execution-with-mcp)), made for the servers already
declared on a computer, without anything to host.

## Skills
<!-- generated:skills -->
| Skill | What it does | Installs with it |
|---|---|---|
| [`code-mode`](code-mode/SKILL.md) | Code mode for the local MCP servers: one Python script calls the tools and only what it prints enters the context (-50 to -90% tokens on batches, chains and big results). One engine without dependency, two entries picked by themselves (MCP tools `search` / `call` / `execute` / `job`, or a command line), background jobs, usage limits waited and partial failures resumed without redoing what succeeded, recipes kept like a cache, measured result sizes, profiles for Blender and Roblox Studio, hooks (reminder, compressor), optional OS sandbox; `/code-mode` for this session, `/code-mode setup` for a project, the whole computer or Claude Desktop. | [`interactive-questions`](../ai/interactive-questions/SKILL.md) |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## What it saves
Measured on real servers (ffmpeg, kdenlive, godot), tokens added to the context, discovery included:

| Task | Direct calls | Code mode |
|---|---:|---:|
| media inventory of 20 files (20 calls) | 26,155 | 2,005 (-92 %) |
| a Godot scene built node by node (10 calls in sequence, 11 turns) | 5,990 | 1,401 (-77 %, 2 turns) |
| a Kdenlive montage (36 calls, 7 turns) | 6,453 | 2,981 (-54 %, 3 turns) |
| one small call | 165 | 523 (+217 %) |

> [!NOTE]
> The last line is why the skill chooses per task: a single small call stays direct, a loop, a chain or a big result
> goes through a script. The 533 tool definitions of those 7 servers cost 92,290 tokens when loaded upfront; the four
> tools of codemode cost about 400.

## How it works
One engine in Python (standard library only, Linux / macOS / Windows), two entries on it:

| Entry | Where | What it is |
|---|---|---|
| MCP tools `search`, `call`, `execute`, `job` | Claude Code and Claude Desktop, after `setup` | `codemode.py serve`: a stdio MCP server whose `instructions` carry the rules of use |
| command line `codemode.py search / call / run / job` | Claude Code, any agent with a shell, cron, CI | the same engine through a local daemon of the session (or `--standalone`) |

- The servers are the ones **already declared**: Claude Code (user, local, approved `.mcp.json`), Claude Desktop, and
  the ones `setup --move` took behind codemode. They start at first use and their tool list is kept in an index.
- The code runs in a **separate process** that only talks to the engine: it can be killed, it never sees the secrets
  of the servers, and it can be closed in an OS sandbox (bubblewrap, sandbox-exec) when the user turns it on.
- **One entry for short and long work**: `execute` answers when the script ends; after 40 s it goes on as a background
  job followed with `job`.
- **Failures in the middle**: a usage limit is waited and tried again, a batch stops sending when a server stays
  limited or goes down, what succeeded is kept, and the same `pmap(..., key=...)` run again only sends what is missing.
- **Recipes**: a script that worked is kept under a name, like a cache (unused 30 days or over the limits, the oldest
  go), found by `search` and run again without a model (`run --recipe name --standalone`).
- **Measured sizes**: the size of every result is learned; `search` shows `~1.3k tok` next to a tool, so the choice
  between a direct call and a script rests on numbers.
- **Profiles**: notes per kind of server, shown once per session by `search`. Shipped: Blender and Roblox Studio, whose
  `[code]` tools already run a script inside the application (the whole task in one script, a summary printed). Yours go
  in `<data>/profiles/*.md`.
- **Hooks** (Claude Code): a reminder after 3 direct calls in a row on one server, and a compressor that replaces a
  direct result over ~2,500 tokens by a preview and the path of the full text.

## Where it works

| | Skill and command line | MCP tools |
|---|---|---|
| Claude Code (terminal, the Code tab of the desktop app) | yes | yes, after `/code-mode setup` |
| Claude Desktop (chat) | no shell there | yes, after `setup --desktop` (restart the app) |
| claude.ai (web, mobile) | no | no: it only reaches public remote servers, never the ones of a computer |

> [!IMPORTANT]
> Written to run on Linux, macOS and Windows, but only run on Linux so far: `python3 scripts/selftest.py` checks the
> engine on any machine in a minute.

## Usage
```bash
/code-mode                      # this session, nothing installed
/code-mode setup                # asks: this project, the whole computer, Claude Desktop, which servers, hooks, sandbox
/code-mode status | stats | doctor | off | uninstall
```
```bash
python3 scripts/codemode.py search "thumbnail" --server ffmpeg
python3 scripts/codemode.py run --write script.py
python3 scripts/codemode.py run --recipe thumbnails --args '{"dir": "/videos"}' --standalone
python3 scripts/selftest.py     # the engine against a fake server: both entries, the hook, the setup, the security rules
python3 scripts/bench.py        # direct calls against code mode on scripted tasks (sets: generic, blender, roblox)
```

## Config
`<data>/config.json` (`$CODEMODE_HOME`, default `~/.local/share/codemode`, `~/Library/Application Support/codemode`,
`%LOCALAPPDATA%\codemode`); only what differs from the defaults is written.

| Key | Default | Meaning |
|---|---|---|
| `inline_wait_s` | 40 | a run still going after this continues as a job |
| `max_output_chars` | 12000 | a longer output goes to a file, its head and tail come back |
| `retry_attempts`, `retry_max_wait_s` | 3, 30 | tries on a usage limit; a longer wait is reported, not slept |
| `compress_over_tokens`, `remind_after` | 2500, 3 | thresholds of the two hooks |
| `recipes` | `ttl_days` 30, `max_count` 200, `max_bytes` 5 MB | the recipe cache |
| `rules` | `deny`, `read_only`, `write`: `[]` | patterns `server.tool` on top of read only by default |
| `sandbox`, `sandbox_hide`, `sandbox_writable` | off | the OS sandbox of the code and its extra paths |
| `sources`, `disabled_servers` | all on, none | where the servers are read from, which to ignore |

## Security
- Read only by default: a tool that changes things needs `--write` / `allow_write`; `rules.deny` always wins.
- A `.mcp.json` server is used only when it was approved in Claude Code: a cloned repository starts nothing.
- The engine listens on `127.0.0.1` with a token kept in a file only the user can read; nothing leaves the computer.
- Without the sandbox the scripts run with the rights of the user, like any command of the session. With it: no
  network, writes limited to the project folder, the usual secret folders and the codemode data hidden.
- Every run is in the audit journal (`<data>/audit/audit.jsonl`: the code, the tools called, the sizes).

[All the categories](../README.md) · [Installation](../../INSTALL.md)
