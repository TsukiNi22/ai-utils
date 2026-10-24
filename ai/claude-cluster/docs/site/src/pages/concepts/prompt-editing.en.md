+++
title = "Prompt editing"
summary = "Esc to take back or rewind, the stash, history, the line editor keys, vim mode, pasted text and the prompt modes."
section = "concepts"
order = 130
keywords = "prompt editing esc rewind take back stash history vim line editor keys paste chips shell command vi commands queued"
+++

## Esc: take back or rewind {#esc}

One rule, chosen by what is going on (`[keys] take_back`):

- A **queued** prompt goes back to the editor, removed from the queue.
- A turn **running** (or waiting on a permission): `Esc` `Esc` (the first `Esc` says so) interrupts it and only that prompt is taken back - never `↑`, which takes back only a queued prompt: it leaves the conversation (claude goes on from the message before it) and its text returns to the editor to be edited and sent again (`prompt taken back`).
- **Nothing running, empty prompt**: the **rewind chooser** lists the prompts of the conversation, newest first, each with its time, first line and the size of its answer; `↑` / `↓` choose, Enter goes back to just before it (the prompt returns to the editor, later messages leave the conversation the agent resumes from), the `Cancel` row or Esc changes nothing. A line shows what will be dropped. `/rewind` opens it too. Never while a turn runs.
- **Nothing running, a prompt being written**: `Esc` `Esc` clears it.

What a rewind removed is kept in `rewind/<session>-<time>.json` of the data folder (claude's own transcript is never touched, so the old conversation can be resumed at its old leaf); with `[ui] stash_rewound = true` the dropped prompts also go to the stash.

## Commands and skills of the same name {#same-name}

The completion of `/` lists every command and skill with its source (`local`, `built-in`, a plugin, an imported link). When two have the same name - claude-cluster's `/context` and the agent's, your skill `export` and claude-cluster's `/export` - both are listed, and the one you choose keeps a hidden id after its name: invisible tag characters that the cursor and Backspace treat as one block with the last letter. The core then runs claude-cluster's command or sends the agent's command / the skill to the agent; the id is removed before anything reaches the agent or the transcript. A `/name` typed by hand stays claude-cluster's command.

## The stash {#stash}

`Ctrl+S` puts the prompt aside and clears the editor, `Ctrl+P` takes the last one back, `Ctrl+L` opens the **list**: newest first with time, first line and the session it was written in. Enter loads one into the session in focus and removes it from the stash (with a prompt already written: *Replace* - the one being written goes to the stash -, *Append* or *Cancel*); `Del` / `x` removes one (`u` undoes); `X` clears all in two steps. The stash is global (`stash.json` of the data folder, over restarts): at most `[ui] stash_max` (10); when full `Ctrl+S` refuses and nothing is lost (`stash_full = drop_oldest` makes room instead). Limits: 256 KB a prompt, 4 MB in all.

## History {#history}

`↑` / `↓` move by **visual row** in a wrapped prompt, keeping the column; on the first or last row the **history** of the session shows, never looped; what you were writing is kept while browsing. `Ctrl+R` opens the history list (type to filter, `Tab`: every session). It keeps the newest 1000 prompts (`history_limit`) with their images and pasted texts.

## Line editor keys {#keys}

The prompt and every popup that takes text (the *Other* answer, the chat of a question, forms, setting values, filters, rename, describe, cd...) use the same line editor:

| Keys | Action |
|---|---|
| `←` `→`, `Ctrl+←` `Ctrl+→` / `Alt+b` `Alt+f` | a character (an emoji, an accent: one cell), a word |
| `Home` `End` / `Ctrl+A` `Ctrl+E`, `Ctrl+Home` `Ctrl+End` | the line, the text |
| `Backspace` `Delete`, `Ctrl+Backspace` / `Alt+Backspace` / `Ctrl+W`, `Ctrl+Delete` / `Alt+d` | a character, the word before, the next word |
| `Ctrl+U` / `Ctrl+K`, `Ctrl+Y` | to the start / end of the line (kept), put it back |
| `Ctrl+Z` / `Ctrl+Shift+Z` | undo / redo |
| `Shift` (+ `Ctrl`) + arrows / `Home` / `End` | select; typing replaces the selection |
| `Ctrl+Shift+C` / `Ctrl+X` / `Ctrl+Shift+V` (`Ctrl+V`) | copy / cut / paste (bracketed paste too) |
| mouse | click places the cursor, drag selects, double click takes a word |

Words and the kill ring work in the prompt where no `[keys]` action uses the key; a password is typed blind. `Ctrl+E` edits the whole prompt in your editor (`[ui] editor`, else nvim, vim, vi, `$VISUAL`, `$EDITOR`, emacs -nw, nano).

## Vim mode {#vim}

`/vim`, the palette or `[ui] vim = true`: the prompt starts in Insert, `Esc` goes to Normal (`h j k l`, `w b e`, `0 ^ $`, `gg G`, `f t F T`, `x X s S D C`, `dd cc yy`, operators with motions, `p P`, `u` / `Ctrl+R`, counts, `v` / `V` visual). In Normal, Enter sends.

## Pasted text and chips {#paste}

A paste of more than `[ui] paste_lines` (3) lines or `paste_chars` (800) characters is one `[Pasted text #N +L lines]` block, sent, stashed and edited (`Ctrl+E`) as its full text. Images pasted from the clipboard are `[Image #N]`, saved by content so the same image is one reference. A chip per image or pasted text sits above the prompt: hover shows it, click puts it back, right click opens an image in the viewer of the desktop. `Backspace` on a block removes it whole.

## Prompt modes {#modes}

A first character changes what the prompt does: `!<command>` runs your shell in the folder of the session (the output goes in the conversation and is given to the agent with your next prompt if you agreed: *this time / always / not this time / never*; a command over 8 s goes to the background as a shell `b1`, listed by `/shells`); `:s/old/new/` rewrites your last prompt; `:r <file>` / `:r !<command>` reads a file or output into the prompt; `:w`, `:e`, `:q`, `:qa`; `:set key=value`; `:g/text/` searches. `@` completes files, `/` commands and skills, `::` sessions. From the web page and the app `!` is refused unless `[web] shell = true`.
