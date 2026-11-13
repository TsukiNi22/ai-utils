+++
title = "Keys and the palette"
summary = "The keys you need every day, the command palette, and where to change them."
section = "guide"
order = 40
keywords = "keys shortcuts palette ctrl+k f1 f9 alt mouse prompt text editing"
+++

Every binding below is a default: change it in `[keys]` of `config.toml` or in the settings page. The complete table is the [keys reference](page:keys-reference). `F1` shows the live list in the app.

## The essentials {#essentials}

| Key | Action |
|---|---|
| <kbd>Ctrl+K</kbd> | command palette: every action (new session, modes, panels, styles, layouts, providers, voice, docs...) |
| <kbd>F1</kbd> / <kbd>F9</kbd> / <kbd>F10</kbd> | every key / settings / statistics |
| <kbd>Ctrl+G</kbd> | the global session |
| <kbd>Ctrl+T</kbd> / <kbd>Ctrl+W</kbd> | new session / close the session (to the trash) |
| <kbd>Ctrl+N</kbd> or <kbd>Alt+N</kbd> / <kbd>Alt+P</kbd> | next / previous session |
| <kbd>Alt+1</kbd> .. <kbd>Alt+9</kbd> | session N |
| <kbd>Ctrl+B</kbd> then an arrow | the session on that side (grid: rows and columns) |
| <kbd>Alt+Y</kbd> / <kbd>Alt+A</kbd> / <kbd>Alt+D</kbd> | allow / always allow / deny a permission |
| <kbd>Shift+Tab</kbd> | next permission mode |
| <kbd>Ctrl+C</kbd> | stop the running turn; none: clear the prompt; twice on an empty prompt: quit |
| <kbd>Ctrl+Q</kbd> | quit |
| <kbd>F2</kbd> / <kbd>Alt+G</kbd> | layout / graph view |
| <kbd>F5</kbd> / <kbd>F6</kbd> | push-to-talk / stop speaking |

## Prompt {#prompt}

`Enter` sends (a turn running: the prompt is queued), `Alt+Enter` or `Shift+Enter` adds a new line. `@` completes files, `/` commands, skills and agent commands, `::` sessions. A first character changes the mode: `!command` runs your shell in the session folder, `:` gives vi-like commands (`:s/old/new/`, `:r file`, `:w`, `:set key=value`). Full details: [Prompt editing](page:prompt-editing).

| Key | Action |
|---|---|
| <kbd>Ctrl+R</kbd> | history of the prompts of the session |
| <kbd>Ctrl+E</kbd> | edit the prompt in your editor |
| <kbd>Ctrl+S</kbd> / <kbd>Ctrl+P</kbd> / <kbd>Ctrl+L</kbd> | stash the prompt / take the last one back / list the stash |
| <kbd>Esc</kbd> | take a queued or running prompt back; on an empty prompt: rewind the conversation |
| <kbd>Ctrl+O</kbd> | thinking and tool results in full / folded |
| <kbd>Ctrl+F</kbd> | search in the transcript |
| <kbd>Ctrl+Home</kbd> / <kbd>Ctrl+End</kbd> | start / end of the transcript |

## The palette {#palette}

`Ctrl+K` lists every action; type words to filter. The matching is typo tolerant (`[ui] matching = c2dmp`, the string matching of libutils): `modle` finds *Model*. A prefix narrows to a kind: `mode:`, `setting:` (only the settings, Enter opens the page at it), `share:`. The same matching completes `/` commands, `@` files and `!` programs.

## Mouse {#mouse}

In the terminal a click chooses a session, a key of the top bar, an answer of a permission, an item of a list, a setting; drag selects text (copied on release); the wheel scrolls the transcript, the lists and the side panels. Right click on a session opens its menu (open, rename, describe, move, archive, delete); right click on a message or block pins it ([Pins](page:pins)).
