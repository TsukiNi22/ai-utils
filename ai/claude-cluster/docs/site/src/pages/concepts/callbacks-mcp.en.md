+++
title = "Callbacks, questions and the MCP"
summary = "How answers come back, how the agents' questions are handled, the MCP server and its settings page."
section = "concepts"
order = 100
keywords = "callback reply mcp server questions askuserquestion permission review chat global answers mcp_tools custom servers job tasks"
+++

## Callbacks {#callbacks}

Nothing in claude-cluster polls. When the global session sends a prompt (`session_send`, `session_spawn`, a task, a job) the result **comes back to it** as a message it can read:

| Message | When |
|---|---|
| `[reply from <name> (<id>)] ...` | a sub-session finished the turn started for the global session |
| `[reply from ...]` with a note | a later turn the agent started by itself, when a background task completes - or claude-cluster woke it with the `<task-notification>` of its command done or killed, a `[job ...]` / `[tasks]` result: its real result after a "waiting for my build" answer (that answer noted "its background still runs"). A background task ending during another turn (the user's, a keep-warm wake-up) makes the end of that turn its result, told the same way |
| `[closed: ...]` / `[stopped: ...]` | the session was closed before it answered / its agent stopped: no answer will come. `[stopped: ...]` also comes after the answer when something ran in the background, and names what stopped with it |
| `[status from <name> (<id>)] ...` | a sub-session telling something now, before the end of its turn (its tool `report_status`, one every 30 s) |
| `[error from ...]` | an API error (a limit, the weekly limit...) |
| `[interrupted: ...]` | an interrupted turn or an agent that exited |
| `[job jN finished\|failed\|killed]` | a job added by `jobs_add`, with exit code, duration, tail |
| `[tasks] ...` | the result of a task, to the sub-session that queued it |
| `[form "<title>" (<id>) filled by the user] {...}` | a display form you submitted |

The callback owed goes with its turn: a turn of the user ending while the prompt of the global session waits in the queue is not its answer. It is not dropped when the session told is away for a moment (starting again): tried for two minutes, then a notice. A shell or an agent of the background that ends or is killed is also a line of the conversation, its last output kept in its log (`F7`), even when no model says anything about it.

What no session can tell (a process outside, a deploy, a time) has its own wake-up: `task_add` with `on_event` and `schedule_add`; the system prompts tell the agents to ask for one rather than promise to look later. The end of a turn woken that way (a schedule, a task a session queued for itself) goes to whoever follows the session, like a background task done.

`reply: false` opts out of a callback. The tests `callbacks.sh`, `send_prompt.sh` and `callbacks_background.py` check the behaviour with a fake `claude`.

## Questions of the agents {#questions}

An agent can ask a question (`AskUserQuestion` of Claude Code, even in `bypassPermissions`). It shows as a **popup** on the session shown, in every interface: one question at a time with its options, `1`-`9` to pick, Enter to validate, `Other` to type, Esc to decline; several choices tick with Enter or Space, and a **Next →** row after `Other` (*Review →* / *Submit →* on the last question, like Claude Code) goes on with the ticks kept. What is written on **Other** stays without Enter, in every interface: an arrow away, another question and back, the page drawn again; it is an answer once validated.

- **One at a time for the whole cluster**, shared by keys, mouse, voice and web page. The question of a sub-session is shown on the global session (`[sessions] questions_to_global`, on) with the sub-session's own view greyed.
- **Review**: after the last question every answer is shown on one page: *Submit*, *Cancel*, or back to edit.
- **Chat about this** (`Ctrl+T`, a button, a field): talk about the question with the model of the session without touching its conversation (no fork, no resume), shared by the interfaces.
- **The global session answers** for you when you press `Ctrl+G` (terminal), *Global answers* (window, web page, notification) or say *la globale*: it chooses from the conversation, the project and the map of your computer and tells you what it answered. `[sessions] global_answers` (off) lets it be told the questions and choose by itself.
- **By voice**: a number or the label of an option, *next*, *back*, *other ...*, *cancel*.

The **safety check of Claude Code** (a removal it cannot resolve) can only be approved by a person: a sub-session in `bypassPermissions` asks the global session (`bypass_to_global`, on), which asks you when unsure.

## The MCP server {#mcp}

The global session has the MCP server `claude-cluster mcp`, whose tools are listed in the [MCP tools reference](page:mcp-tools) (generated from the code). A sub-session of the claude driver gets a smaller one (`task_*`, `schedule_*`, job tools) scoped to what it created.

**Settings > MCP** (`F9 > MCP`) lists every tool with what it does and an on / off switch (`[mcp_tools] <tool> = false`: refused at once with a message, left out of its tool list at its next start), and the **custom MCP servers** it gets too (`[mcp] servers`: `"name: command args"` for a local program or `"name: https://url"`), written in its MCP config at its next start. Adding a server is a protected setting, confirmed at save.

The tools are allowed without a prompt (`--allowedTools mcp__claude-cluster`); what they may do is bounded: see [Security](page:security#global).

## The docs tool {#docs}

The tool `docs` lets the global session search this documentation or open a page for you (`search`, `page`, `open`). See the [MCP tools reference](page:mcp-tools).
