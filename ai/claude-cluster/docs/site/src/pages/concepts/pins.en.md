+++
title = "Pins"
summary = "Keep a message or a display block above the prompt, whatever the scroll."
section = "concepts"
order = 70
keywords = "pin unpin session_pin pinned block display checklist sticky"
+++

## What a pin is {#what}

A message or a display block of a transcript (a task list, a form, a steps block, a table, a note, a plain answer) can be **pinned**: it stays at the bottom of the view of its session, just above the prompt, in a compact bordered area, whatever the scroll of the transcript. Its title folds it, *Unpin* removes it, it scrolls inside when tall. A live block (checklist, steps, progress, form) stays interactive and is updated in place while pinned.

One pin per session: pinning another replaces it. The pin is kept with the session (restart, archive / restore; `/clear` drops it) and shown by `session_list`, `session_status` and the session info.

## How to pin {#how}

| Where | How |
|---|---|
| terminal | right click on a message or block, or `Ctrl+K`: *Pin the last answer*, *Pin: <block>*, *Unpin: <title>* |
| window | right click: *Pin this ...* / *Unpin* |
| web page | right click (long press on a phone): *Pin* / *Unpin*; `Alt+P` pins the last answer or unpins |
| global session | MCP `session_pin` (`display`: an id or a block `{type, ...}`, or `index`: an entry of `session_transcript`; `title`), `session_unpin`, `session_pin_get` |

## Who may pin {#rights}

You always may. The settings `[agent] global_pin` (the global session may pin on any session, on) and `sub_pin` (a sub-session may pin on itself only, on) control the agents.
