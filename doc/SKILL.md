---
name: doc
description: Router for documentation tasks of the user - README and Markdown docs, wiki pages, CHANGELOG / release notes, HTML documentation (user guide, technical doc, project graph), code comments - loads readme-style, html-doc, git-conventions or cpp-comments depending on what is documented. Invoke manually with /doc.
disable-model-invocation: true
---

# Documentation router

**Request given with `/doc`:** $ARGUMENTS

Load the skills with the Skill tool (never the other routers: they are manual-only), then write. The request is the text given with `/doc` (or the latest
request; if none, inspect `README.md`, `CHANGELOG.md`, `docs/` and propose what is missing or outdated).

## Context
- Existing docs: `README.md`, `CHANGELOG.md`, `docs/*.html`, wiki (`<repo>.wiki`), `Doxyfile`.
- What changed since the docs were written: `git log --oneline -- . ':!docs' | head`, version in `CMakeLists.txt`.
- Audience: users (non-technical) or developers.

## Skills
| Situation | Skills |
|---|---|
| README, wiki page, Markdown guide | `readme-style` |
| HTML documentation, `docs/` site, GitHub Pages, project graph | `html-doc` (+ `readme-style` for the README link) |
| CHANGELOG, release notes, PR description | `git-conventions` |
| comments / documentation inside C++ code | `cpp-comments` (no Doxygen) |
| doc of a libutils based project | also `libutils` (exact APIs) |
| full documentation pass | `readme-style` + `html-doc` + `git-conventions` (CHANGELOG) |

## Always
- Check every name/command/API against the code before writing it; mark planned vs implemented.
- Start the answer with one line listing the loaded skills.
