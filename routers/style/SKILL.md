---
name: style
description: Router for the user's coding style in every language - picks the style skills matching the files (C++, CMake, Python, shell, Lua, JS/TS, C, YAML, comments, docs) to write, format or review code like the user. Invoke manually with /style.
disable-model-invocation: true
---

# Coding style router

**Request given with `/style`:** $ARGUMENTS

Load the skills with the Skill tool (never the other routers: they are manual-only), then write / reformat / review. The request is the text given with
`/style` (or the latest request; if none, review the uncommitted changes: `git diff --stat`, `git diff`).

## Skills by file / language
| Files | Skills |
|---|---|
| `.cpp`, `.hpp`, `.tpp`, `.inl` (and `.h` of a C++ project) | `cpp-style` + `cpp-comments` |
| `.py` | `python-style` + `python-comments` (new files: `python-class`) |
| any other language: `.sh`, `.lua`, `.js`/`.ts`, `.rs`, `.go`, `.c`/`.h` (C), `.yml`, `.json`... | `coding-style` + `comments` |
| comments of any file (C, CMake, Makefile, shell, Python, Lua, YAML...) | `comments` |
| new C++ files / classes | `cpp-class` (layout, header, sections) + the two above |
| `CMakeLists.txt`, `*.cmake`, `*.cmake.in` | `cmake-style` |
| Markdown docs | `readme-style` |
| HTML docs | `html-doc` |
| commit messages | `git-conventions` |
| whole project review with a report | `audit-quality` (PDF) |

## Review mode
When `xstyle` is installed (`command -v xstyle`), start with `xstyle <paths> -r -o /tmp/xstyle.md` (or `-S` for the
counters): its findings (rule code + severity + fix) are the base of the review, checked and completed by hand;
`xstyle --fix [CODES]` applies the fixable ones when the user asks for the fixes.
The review is delivered through `report` (format asked once, English, `audit/` at the repository root) (`audit-quality` for a whole project).
List the deviations per file with the rule they break (skill + rule), most important first, then propose the
fixes; only apply them if asked. Never change the logic while restyling.
Start the answer with one line listing the loaded skills.
