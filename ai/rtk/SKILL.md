---
name: rtk
description: Reduce to the minimum the tokens of a text, a command output, a report, a log or a program's output so it costs the least in an AI context - every human-readable part dropped (colors, ANSI, tables, rulers, banners, prose, repeated fields, absolute paths), only the signal kept in a compact machine-oriented form (legend once, one record per line, shared values written once, counters on one line), even if unreadable for a human - and design / implement an --rtk mode (flag + env var) on a program or script, like xstyle --rtk. Use whenever the user asks to compact / minify / compress a text or an output for an AI, to save tokens or context, for an "rtk" / "AI" / "token-optimized" output or mode, to add such a flag to a binary or a script, or to write a file only an AI will read.
---

# rtk: token-minimal text for an AI

`SKILL_DIR` = the directory of this file. Helper: `python3 SKILL_DIR/scripts/compact.py [files] [--stats]
[--max-line N] [--head N]` (stdin -> stdout): strips ANSI / OSC 8 links / rulers / box lines / runs of spaces, merges
identical consecutive lines (`xN`), factors the common directory (`# $P=<dir>` then `$P/...`); `--max-line` and
`--head` are lossy, only on request. It is the generic first pass; a format designed for the content (sections 2-3)
always gets much further (xstyle: 147 KB -> 29 KB).

Unrelated to the `rtk` CLI of `RTK.md` (proxy that condenses the outputs of the shell commands Claude runs): this
skill **produces** compact formats; that tool consumes them.

## 1. Rules (the reader is an AI, never a human)
- **Keep every signal**: identifiers, locations (`file:line:col`), codes, exact error messages, numbers, counts,
  versions, what changes a decision. **Drop the rest**: colors, ANSI, hyperlinks, banners, boxes, tables (-> one
  record per line), alignment spaces, blank lines, progress bars, timestamps that don't matter, greetings / prose,
  repeated headers, the source line when the file can be read.
- **Legend once** in a first `#` line (field order, abbreviations: `U/M/m/n`, `a=auto -=manual`); never per record.
- **Write once what repeats**: a key heading its records (`>file` then `line:col ...`), a text shared by every record
  of a group written once at the end (`* CODE message => hint`), a common prefix as `$P`, `xN` for duplicates,
  ranges (`12-18`) instead of lists.
- **Short fixed tokens**: one-letter enums from the legend, separators ` ` / `:` / `|`, no quotes unless needed, no
  units when the legend gives them, relative paths, `=` for the summary line.
- **Truncate long values** with what was cut: `…(+120)` / `(+3 lines)`; never truncate an identifier or a location.
- **One summary line** with every counter (`= 68 issues in 6 files: U1 M15 m39 n13 | fix a41 -23`) and the next
  command when there is an obvious one (`| xstyle --fix`).
- **Errors on one plain line**: `<tool>: error: <what>: <detail>`, no color, no stack.
- **Lossless by default**; a lossy step (sampling, `--head`, dropping a field) is said in the legend (`# first 50 of
  812`). Same exit codes as the normal mode.
- Measure: `wc -c` before / after (~4 chars per token) and give the ratio; check by reading the result as the next
  AI would: can it locate and fix / decide everything without the original?

## 2. Transform a given text / output
1. Identify the records (issues, log events, test results, entries) and their fields; what is constant.
2. First pass `compact.py --stats`, then the format of section 1 (legend, keys, shared values, summary) written by hand
   or with a small `awk` / `jq` / Python one-off for a big input.
3. Give the result in a code block (or a file when asked) + the ratio, nothing else.
Examples: build log -> `file:line:col severity message` lines + `= 3 errors 12 warnings`; test run -> only the
failures (`suite.test file:line message`) + `= 412 passed 3 failed 2 skipped`; JSON API answer -> the useful fields
as `key=value` records; a doc / README for a context file -> terse bullet facts, no prose, no examples kept twice.

## 3. Add an `--rtk` mode to a program / script (ask before changing its interface)
- **Interface**: flag `--rtk` + env `<TOOL>_RTK=1` (activated from the start, before the parsing, so the parsing
  errors are plain too); implies no color and no hyperlink; stays combinable with the filters (`-S` -> only the
  summary line); help text `Compact output for an AI (token saving): ...`; shell completion updated.
- **Output**: a separate renderer (ex: `Reporter::rtk_()` in `style/xstyle/src/xstyle/Reporter.cpp`), never `if`s
  scattered in the human output; reuses the same data, so both modes never disagree. Format of section 1.
- **Errors**: `main` catches and prints `<tool>: error: <what>: <info>` in rtk mode (argv / env checked directly).
- **Tests**: the rtk output of a sample (golden file or asserts on the lines), the size ratio against the human
  output, the summary only, the error line, the exit codes unchanged.
- **Docs**: README line (`--rtk` = for an AI), and the skills / `CLAUDE.md` that run the tool use `--rtk`.
- A script (bash / Python): same flag, `NO_COLOR` honored, the human prints behind one `if rtk` at the output level.

## 4. Other uses
- Wrap a noisy command for the assistant: a pipe (`cmd 2>&1 | python3 SKILL_DIR/scripts/compact.py`), or a tiny wrapper
  script / alias when the user wants it permanent.
- Files only read by an AI (`CLAUDE.md`, memories, agent prompts, context dumps): terse facts, one per line, no
  repetition, only on the user's request for user-facing files (a README stays human).
- Compare formats: show 2 candidate formats on a real sample with their sizes, recommend the smallest that stays
  lossless for the decisions.
