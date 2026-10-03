---
name: coding-style
description: Router for the user's coding style - picks the style skills matching the language/files (C++ code and comments, CMake) to write, format or review code like the user. Invoke manually with /coding-style.
disable-model-invocation: true
---

# Coding style router

**Request given with `/coding-style`:** $ARGUMENTS

Load the skills with the Skill tool (never the other routers: they are manual-only), then write / reformat / review. The request is the text given with
`/coding-style` (or the latest request; if none, review the uncommitted changes: `git diff --stat`, `git diff`).

## Skills by file / language
| Files | Skills |
|---|---|
| `.cpp`, `.hpp`, `.h`, `.tpp`, `.inl` | `cpp-style` + `cpp-comments` |
| comments of any file (C, CMake, Makefile, shell, Python, Lua, YAML...) | `comments` |
| new C++ files / classes | `cpp-class` (layout, header, sections) + the two above |
| `CMakeLists.txt`, `*.cmake`, `*.cmake.in` | `cmake-style` |
| Markdown docs | `readme-style` |
| HTML docs | `html-doc` |
| commit messages | `git-conventions` |
| another language (Python, shell, Lua...) | no dedicated style skill yet: follow the closest existing file of the project (indentation 4 spaces) + `comments` for the comments, and say that no skill covers the rest |
| whole project review with a report | `audit-quality` (PDF) |

## Review mode
The review is delivered as a **PDF** through `pdf-report` (`audit-quality` for a whole project).
List the deviations per file with the rule they break (skill + rule), most important first, then propose the
fixes; only apply them if asked. Never change the logic while restyling.
Start the answer with one line listing the loaded skills.
