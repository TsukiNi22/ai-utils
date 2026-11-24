---
name: code-mode
description: Code mode for the local MCP servers (the Cloudflare / Anthropic idea, made for this computer) - instead of one tool call at a time with every result in the context, one Python script calls the tools and only what it prints comes back (-50 to -90% tokens on batches, chains and big results). One engine, two entries chosen by themselves (MCP tools search / call / execute / job when registered, else a command line), background jobs, usage limits waited and partial failures resumed, recipes kept like a cache, measured result sizes, profiles for Blender and Roblox Studio, Claude Code hooks (reminder, compressor), optional OS sandbox. Use whenever a task needs 3+ MCP calls, a loop over items, a chain of calls or a big MCP result to filter, when the user says "code mode", "/code-mode", "économise les tokens des MCP", or asks to set it up (this session, a project, the whole computer, Claude Desktop), its status, stats or removal.
argument-hint: "[task | setup | status | doctor | stats | recipes | off | uninstall]"
hooks:
  PostToolUse:
    - matcher: "mcp__.*"
      hooks:
        - type: command
          command: >-
            for d in "${CLAUDE_SKILL_DIR}" "$HOME/.claude/skills/code-mode" "${CLAUDE_PLUGIN_ROOT}/skills/code-mode/code-mode"; do
            [ -f "$d/scripts/codemode.sh" ] && exec sh "$d/scripts/codemode.sh" hook remind compress; done; exit 0
          timeout: 15
metadata:
  summary: 'Code mode for the local MCP servers: one Python script calls the tools and only what it prints enters the context (-50 to -90% tokens on batches, chains and big results). One engine without dependency, two entries picked by themselves (MCP tools `search` / `call` / `execute` / `job`, or a command line), background jobs, usage limits waited and partial failures resumed without redoing what succeeded, recipes kept like a cache, measured result sizes, profiles for Blender and Roblox Studio, hooks (reminder, compressor), optional OS sandbox; `/code-mode` for this session, `/code-mode setup` for a project, the whole computer or Claude Desktop.'
---

# Code mode: the local MCP servers driven by code

`SKILL_DIR` = the directory of this file. `cm` = `python3 SKILL_DIR/scripts/codemode.py --session ${CLAUDE_SESSION_ID}`
(no `python3`, ex: Git Bash on Windows: `sh SKILL_DIR/scripts/codemode.sh --session ...`). Python 3.9+, standard library
only, Linux / macOS / Windows. `interactive-questions` (a skill) = the folder of that skill, for the setup questions.

A direct MCP call puts its whole result in the context and costs one turn; a script run by codemode calls the same
tools and returns only what it prints. Measured on real servers (ffmpeg, kdenlive, godot): 49,500 -> 11,900 tokens over
8 tasks, but a single small call costs 3x more than a direct one: **choose per task** (section 3).

| Where | What works |
|---|---|
| Claude Code (terminal, the Code tab of the desktop app) | everything: this skill, the command line, the MCP tools after `setup` |
| Claude Desktop (chat) | the MCP tools only, after `setup --desktop` (no shell there); the rules travel with the server |
| claude.ai (web, mobile) | not possible: it only reaches public remote servers, never the ones of this computer - say so and stop |

## 1. What is given with the skill
**Given with `/code-mode`:** $ARGUMENTS

| Given | Do |
|---|---|
| nothing, or a task | use code mode in this session (sections 2-4), then do the task with it |
| `setup` (+ `project`, `pc`, `desktop`) | section 5 |
| `status`, `doctor`, `stats` (+ `all`), `recipes` | run `cm status` / `cm doctor` / `cm stats [--all]` / `cm recipe list` and show the result |
| `off` | `cm off`: stops the engine of this session (it also stops by itself with the session) |
| `uninstall` | section 5, last line |

## 2. Which entry in this session
- Tools named `mcp__codemode__search` / `call` / `execute` / `job` exist in this session (codemode is registered): use
  them. Their arguments are the ones of the command line below.
- Otherwise the command line, through Bash. Nothing is installed: the first command starts the engine of this session
  (a small local daemon), it stops with the session. Both entries drive the same engine: same variables, same jobs.
- **In a Bash heredoc, never let a brace touch a quote** (`{"`, `{'`, `"}`, `'}`): Claude Code asks for an approval of
  such a command (refused outright when nobody can answer). Write `dict(file_path=f)` instead of `{"file_path": f}`,
  `"%s %d" % (...)` or a variable instead of a quoted key inside an f-string field. One command per Bash call (no `;`
  or `&&` with other programs). The MCP entry has none of these limits: for daily use, `/code-mode setup` once.
- Invoking this skill also turns on its hooks for the session (Claude Code): the sizes of direct MCP results are
  measured, a reminder comes after 3 direct calls in a row on one server, a direct result over ~2,500 tokens is replaced
  by a preview and the path of the full text, and a smaller one handed over escaped twice comes back as its plain text.

```bash
cm search                                  # the servers and what they hold
cm search --server kdenlive                # every tool name of one server, in one line (cheap)
cm search "thumbnail|duration" --server ffmpeg      # one line per tool: server.tool(args) - summary [ro] ~1.3k tok
cm search "timeline_add_clips" --detail             # + the arguments (5 tools at most)
cm call ffmpeg.get_media_info file_path=/x.mp4 --pick "format.duration,streams[].codec_name"
cm run - <<'EOF'                           # a script; --write when it calls tools without [ro]
...
EOF
cm job j3 --wait 60                        # a run that went to the background: what it printed since
```
Search with precise words or the tool names, never with the server name alone (it would match every tool of it).
`[ro]` = read only, `[code]` = the tool already runs a script inside its application, `~N tok` = the measured average
size of its results. `notes(server): ...` under a result = the profile of that server (Blender, Roblox Studio, yours in
`<data>/profiles/*.md`): read it once, it says how to use that server cheaply.

## 3. Direct call, `call` or `execute`: tokens first, seconds after
| The task | Use |
|---|---|
| 1-2 calls, small result needed as it is | the server's own tool when it is loaded here, else `call` |
| one call with a big result (`~Nk tok` in search, a list, a log, a scene) | `call` with `--pick`, or `execute` printing the few fields needed |
| 3+ calls, a loop over items, a chain where a result feeds the next call | `execute`: one script |
| results to filter, count, sort, compare | `execute` |
| minutes of work, hundreds of items | `execute`: it goes on as a job by itself, follow it with `job` |
| an image to look at (viewport, screenshot, bitmap) | `call` (MCP entry: the image comes back; command line: its file path, then Read) |
| a server with a `[code]` tool (Blender, Roblox Studio) | the whole task in ONE script inside the application, a summary printed (its notes) |

A few seconds more are fine when the token gain is large; a single small call never goes through `execute`.

## 4. Writing the script
```python
files = sorted(glob.glob("/abs/media/*.mp4"))                 # any Python, any import
infos = pmap("ffmpeg", "get_media_info", [dict(file_path=f) for f in files], key="infos")   # parallel, resumable
for f, info in zip(files, infos):
    if isinstance(info, ToolError):
        print(os.path.basename(f), "FAILED", info.kind, info.message[:80]); continue
    print(os.path.basename(f), info["format"]["duration"])
print(len(ok(infos)), "ok,", len(failed(infos)), "failed")   # print only what the answer needs
```
- `call(server, tool, **args)` -> the result as data (dict / list / str; with images: `{"result", "files": [paths]}`);
  it raises `ToolError` (`.kind`, `.message`, `.retry_after`). `pmap(server, tool, items, limit=8, key=None)` -> one
  result per item, a `ToolError` at the place of each failed one (`Skipped` when it was not sent); `key` is a name
  for the batch (or a function giving the id of an item). `ok(results)`,
  `failed(results)`, `args` (the values given with `--args` / a recipe), `json`.
- **Variables persist** between runs of the session: first a tiny run that prints the shape of ONE result, then the
  real run on everything. Never print raw results; a long output is cut and saved to a file anyway.
- **Failures, usage limits, a call that fails in the middle**: a usage limit is waited and tried again by itself (a
  transient failure too when the tool only reads); when it lasts, the batch stops sending, what succeeded is kept and
  the footer says it: `[calls: 12 ok, 1 failed, 7 not sent | server.tool x1 rate_limit: "..." , retry after 60s |
  variables kept: ...]`. Then **never redo what succeeded**: run the same `pmap(..., key="same")` again later, it only
  sends the failed and not sent items; after a script error, continue from the kept variables. A write that failed with
  `timeout` may have happened: check before sending it again. Report to the user what is still missing.
- **Writes**: a tool without `[ro]` needs `--write` (`allow_write=true`); only when the user's request asks for a
  change, never "to be safe". The rules of `<data>/config.json` (`rules.deny`, `rules.read_only`, `rules.write`) win.
- **Long runs**: after 40 s (`--wait N`) the run goes on as a job and the answer says so; `cm job <id> --wait N` gives
  what it printed since, `--kill` stops it. `--fresh`: a separate clean process (no shared variables).
- **Recipes** (a cache: unused 30 days or over the size limit, the oldest go): `--save name --desc "..."` keeps a script
  that worked, `search` lists it as `recipe:name(args)`, `cm run --recipe name --args '{"dir": "/x"}'` runs it again;
  `cm run --recipe name --standalone` works without a session (cron, CI). Save what will clearly be asked again.

## 5. Setup, status, removal
`/code-mode` alone needs no setup. `setup` makes it permanent: ask with `interactive-questions` (one popup, French):
1. **"Où l'installer ?"** - `Ce projet` (`--scope project`) · `Tout le PC (Recommended)` (`--scope user`) · `Claude
   Desktop aussi` (adds `--desktop`).
2. **"Quels serveurs passer derrière codemode ?"** (multi choice, only with `--scope user`; list them with
   `cm setup --scope user --move all --dry-run`): the chosen ones leave Claude Code's own list (their tool names leave
   the context, they only start when used) and stay usable through codemode; say that direct calls to them are gone.
   Keep direct a server whose images are looked at all the time.
3. **"Quels hooks ?"** (multi) - `Rappel (Recommended)` · `Compresseur` -> `--hooks remind,compress` (`none`: only sizes).
4. **"Sandbox du code ?"** - `Non (Recommended)` · `Oui` (`cm doctor` says whether it exists here: bubblewrap on Linux,
   sandbox-exec on macOS, none on Windows). **No answer = off**: `--sandbox off`.

Then `cm setup --scope user --move ffmpeg,kdenlive --hooks remind,compress --sandbox off [--desktop] [--dry-run]`, show
its report, and say: open sessions keep their tools until restarted; Claude Desktop must be quit and reopened.
`cm status` shows what is installed and running; `cm uninstall [--scope project|user|all] [--purge]` gives the moved
servers back and removes the registration and the hooks (ask which scope first).

## 6. Rules
- Tokens first: follow section 3 even when a direct call "would be simpler" for a loop; never `execute` for one call.
- Never `--write` / `allow_write` unless the request implies a change; never `--move`, `uninstall` or `--purge` without
  the user's answer; a `.mcp.json` server not approved in Claude Code is refused: approve it there, never around it.
- The code runs on the user's computer with his rights (inside the OS sandbox only when he turned it on: no network,
  writes only in the project, every hidden entry of the home out of sight): write scripts that do what was asked and
  nothing else, no network or file access the task does not need.
- Everything is local: the engine listens on 127.0.0.1 with a token, the audit journal (`cm stats`, `<data>/audit/`)
  stays on the computer. Data folder: `$CODEMODE_HOME`, default `~/.local/share/codemode` (Linux),
  `~/Library/Application Support/codemode` (macOS), `%LOCALAPPDATA%\codemode` (Windows).
- Measure when asked: `cm stats` (this session), `python3 SKILL_DIR/scripts/bench.py` (direct calls against code mode
  on scripted tasks, with sets for Blender and Roblox Studio), `python3 SKILL_DIR/scripts/selftest.py` (the engine).
