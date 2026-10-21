+++
title = "First start and first session"
summary = "Start claude-cluster, answer the first questions, talk to the global session, open a session on a project."
section = "guide"
order = 20
keywords = "first start quick start spawn new session global session machine map restore trash permissions"
+++

## First start {#start}

```bash
claude-cluster
```

In a terminal this opens the terminal interface, otherwise the window (`--tty` / `--gui` force one). At the first start two questions are asked:

- **Restore the previous run?** (`[ui] restore`: `ask`, `always`, `never`). Esc restores.
- **Map this computer?** The map (`machine.md`) is what the global session reads to know your projects, tools and shell: your OS and hardware, installed tools, the dangerous shell aliases, your git repositories. It is local, shown before it is read, and proposed again when older than `[ui] machine_days` (30). Answering no is remembered.

The interface shows the list of sessions on the left, the conversation in the middle and optional panels (tokens, context, cost, git...) on the right. `F1` lists every key, `Ctrl+K` opens the command palette, `F9` the settings.

## Talk to the global session {#global}

The first session is the **global session** (`Ctrl+G`). It is a Claude Code session that drives all the others through its own tools: ask it in plain words, or by [voice](page:voice).

> Open a session on the libutils project and ask it to run its tests.

It looks the project up in the map of your computer, spawns a sub-session in that folder, sends the prompt, and tells you the answer when the sub-session is done (no polling). See [The global session](page:global-session).

## Open a session yourself {#session}

| Way | Effect |
|---|---|
| `Ctrl+T`, `/new <folder>`, palette *New session on a project* | a new session in a folder or a project of the map (the closest name wins, typos tolerated) |
| `claude-cluster spawn <folder> --name x --prompt "..."` | the same from a shell, on the running instance |
| `/cd <folder>` | move the current session to another project |

Then type in the prompt and press `Enter`. While a turn runs your next prompts are queued under the transcript; `Esc` takes the last one back.

## Permissions {#permissions}

A new session starts in `auto` mode: the classifier of Claude Code allows the safe actions. When something needs your answer a popup appears on the session, a notification says so, and you answer with `Alt+Y` (allow), `Alt+A` (always allow) or `Alt+D` (deny). `Shift+Tab` cycles the mode. See [Permissions and models](page:permissions-models).

## Close, restore, quit {#close}

- `Ctrl+W` or `/close` closes a session to the **trash** (restorable for `[sessions] trash_days`, 14 by default): `Ctrl+Shift+T` / `Alt+T` reopens one; `/delete` removes it for good after a question.
- `Ctrl+Q` quits. A session still working asks whether to keep it running in the background or stop everything.
- `claude-cluster serve` runs the core with no interface; the commands below and the [MCP server](page:callbacks-mcp) drive it.

```bash
claude-cluster list                      # the sessions and their state
claude-cluster send s2 "run the tests"   # a prompt to a session
claude-cluster status s2                 # details and the last messages
```

Next: [the interfaces](page:interfaces) and [the keys](page:keys).
