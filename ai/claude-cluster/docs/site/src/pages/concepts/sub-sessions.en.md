+++
title = "Sub-sessions"
summary = "What a sub-session is, and how it differs from a fork and from a Claude Code subagent."
section = "concepts"
order = 10
keywords = "sub-session subsession session fork subagent task tool agent process global"
+++

Everything in claude-cluster is a **session**: one running agent (Claude Code by default) in one project folder, with its own conversation, mode, model and permissions. The session you talk to through the [global session](page:global-session) are its **sub-sessions**.

## What a sub-session is {#what}

A sub-session is a full agent process of its own:

- one **folder** (a project of the map of your computer), one conversation that survives restarts (`--resume`), one **mode** (`auto`, `manual`, `acceptEdits`, `plan`, `dontAsk`, `bypassPermissions`), one **model** and thinking effort;
- it is shown in every interface (terminal, window, web page, phone) as a row with its state, its short description, its category and tags;
- it is created by you (`/new`, Ctrl+T, `claude-cluster spawn`) or by the global session (`session_spawn`), and closed to the trash (restorable for `trash_days`) or deleted for good;
- the claude driver keeps **one process per session** with live permissions and its own small MCP server; the other drivers (Codex, opencode, Qwen Code) start one process per turn and resume the conversation by id.

The global session stays one level above: it dispatches your requests, reads the answers (they come back as `[reply from <name> (<id>)]`, no polling) and merges them.

## Compared with a fork {#fork}

A **fork** (`claude-cluster fork <s>`, palette *Fork the session*) is only a way to *start* a session: a copy of a Claude Code conversation so far, after which each copy goes its own way. The result is two ordinary sub-sessions. Use it to give a new session the context of another one. The global session may fork only when you allowed it (*Global session can fork*, off by default).

## Compared with a Claude Code subagent {#subagent}

A **subagent** is a feature *inside* one Claude Code session: the agent delegates a job to a helper with its own context window and gets a summary back. It is invisible to claude-cluster.

| | Sub-session | Fork | Claude Code subagent |
|---|---|---|---|
| Visible in the interfaces | yes, a row of its own | yes, once started | no, inside its parent |
| Own conversation kept | yes | yes (copied at start) | no, a summary returns |
| Own mode / model / permissions | yes | yes | those of its parent |
| You can talk to it | yes | yes | no |
| Started by | you or the global session | you | the agent itself |
| Costs | its own tokens | its own tokens | tokens of the parent session |

## Limits and good practice {#limits}

- `[sessions] max_parallel` caps the sub-sessions working at once; more tasks wait in the queue (`task_queue` bounds it). See [Tasks and schedules](page:tasks-schedules).
- Heavy builds and tests go through the [orchestrator](page:orchestrator), never several at once from several sessions.
- A session made for one precise job can be created `one_shot`: it goes to the trash by itself once its answer was relayed. See [One-shot sessions](page:one-shot).
- Moving a large session to a stronger model re-reads its whole context at the new price (asked first while it is loaded): prefer a new sub-session with a short brief ([Permissions and models](page:permissions-models#model-change)).
- The context of a sub-session is capped by `[sessions] context` (`auto`, or 100k to 1M: compacted past it); the global session may give each one its own when it starts it (`session_spawn` `context`: `150k` for a small fix), so every turn and a wake-up after its cache expired cost what the task needs ([Settings](page:settings#context)).

> [!NOTE]
> A sub-session of the claude driver gets a small scoped MCP (`task_*`, `schedule_*`): it sees and manages only the tasks and schedules it made.
