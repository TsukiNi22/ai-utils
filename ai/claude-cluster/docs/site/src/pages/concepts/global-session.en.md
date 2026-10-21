+++
title = "The global session"
summary = "The session that drives the others: its prompt, the map of your computer, callbacks, fan-out, and what it may and may not do."
section = "concepts"
order = 20
keywords = "global session orchestrator mcp toolbox machine map fanout callback reply remote control start over system prompt global.md"
+++

The **global session** is a Claude Code session of its own (`Ctrl+G`; `[global]`: folder, model, effort, permission mode). You talk to it in plain words or by voice; it dispatches work to the [sub-sessions](page:sub-sessions) through its own MCP server and reports back. It is not a separate program: it is a session whose tools are the cluster itself.

## What it has {#has}

- A **system prompt** `~/.config/claude-cluster/global.md`, written once and yours to edit (settings *Global session > System prompt*; it restarts on the same conversation), added to your global `CLAUDE.md`. It ends with a compact **toolbox**: the MCP tools by family and when to use each. Rendering and routing guides come from the binary (`rendering.md`, `routing.md`).
- The **map of your computer** `machine.md`: you, your OS and hardware, tools and versions, shell aliases (the dangerous ones and the ones replacing a common command), git identity, Claude Code config, containers, SSH hosts and the git repositories of your home folder. Local only; read when a request touches a local project or the system. Note that what it reads is sent to its API provider.
- The **tools** of [the MCP server](page:mcp-tools) `claude-cluster mcp`, allowed without a prompt but bounded (see [Security](page:security#global)).

## How it works with the others {#callbacks}

- It sends a prompt with `session_send`; the answer **comes back to it** as `[reply from <name> (<id>)]` when the sub-session is done. It never polls (`session_read` re-reads a whole context).
- Every end of a turn started for it calls it back once: the first prompt of a spawn, a task of the queue, an API error (`[error from ...]`), an interrupted turn, an agent that exited, a background task that completes.
- **Several projects at once**: a request touching several projects becomes a group (`session_fanout`): a task per project (sessions or folders created for them, up to a pattern `match`), a chain (`after`), a wait it chooses (`all`, `first`, `count`, with a timeout), a failed task tried once more. It is woken **once** with all the answers and merges them (`merge: session` for a big merge, `table` for a comparison). `[sessions] max_parallel` sub-sessions work at once; the rest wait in the [task queue](page:tasks-schedules).
- Between sessions it speaks tersely (`key: value` lines, compact JSON without empty fields), like `rtk`: what you read stays normal.

## What it may and may not do {#rights}

It can spawn, prompt, interrupt, close, restore, rename, move, describe and categorise sessions, change modes and models within your [policy](page:permissions-models), manage tasks, schedules and jobs, show displays and pins, read and change most settings. It never changes the **protected settings**, and a few powers are off until you allow them: forking sessions (`global_fork`), editing a sub-session's conversation (`global_edit`), rewriting its own system prompt (`rewrite_global_prompt`), answering the agents' questions for you (`global_answers`). Autonomy follows its prompt: what you ask explicitly is done without asking again; what it decides alone is proposed first.

## Staying alive {#alive}

It stays: if its process ends by itself it starts again on its conversation (once a minute at most). **Start over** (palette) sends every sub-session to the trash, stops the queued tasks and groups, and starts a new conversation, after a yes / no. `/clear` only starts the context of claude again; the transcript stays. **Remote Control** (`remote on`) puts the global session itself on claude.ai and the Claude app, live both ways.

## Its permission mode {#mode}

By default it runs in `bypassPermissions` (`[global] permission_mode`): it never asks, which is the point of an orchestrator and its risk, because a prompt injected in a sub-session (a web page it read) comes back to it as a reply. Setting `auto` makes it ask before a command; the MCP tools of claude-cluster stay bounded either way.
