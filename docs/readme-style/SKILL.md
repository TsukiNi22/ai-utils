---
name: readme-style
description: Tsukini's style for README.md and Markdown documentation (from libutils, context-forge, c2dmp-hsm, s.o.s) - title + tagline, Table of Contents, Dependencies/Packages/Quick Setup/Usage/Workflows sections, GitHub callouts, tables, bash blocks. Use whenever writing, rewriting or reviewing a README or any Markdown doc for one of the user's projects. For a GitHub wiki use wiki-style; for CHANGELOG/release notes use git-conventions; for HTML docs use html-doc.
---

# Tsukini README / Markdown doc style

English (unless the user asks for another language), direct, technical, no emoji, no marketing.
**Location**: `README.md` at the root of the repository (`git rev-parse --show-toplevel`), or in the current folder
outside a repository; never in `docs/` unless the user asks for it. Templates: `templates/README.md` (application or
full library) and `templates/README-lib.md` (small header-only/algorithm library).
CHANGELOG and release descriptions: the `git-conventions` skill. HTML documentation: the `html-doc` skill.

## Structure (full project: libutils, context-forge)
```markdown
# <project-name>

> [!TIP]
> Documentation [<owner>/<repo>](<doc url>) (vX.Y.Z).          <- only if an HTML/wiki doc exists

<1-3 lines: what it is, how it works, the fallback/limits>

### Table of Contents
 - [Dependencies](#dependencies)
 - [Packages](#packages)
 - [Quick Setup 1 (All)](#quick-setup---1-all)
 - [Quick Setup 2 (Limited)](#quick-setup---2-limited)
 - [Usage](#usage)
 - [Unit tests](#unit-tests)
 - [Workflows/Release](#workflowsrelease)

## Dependencies
## Packages
## Quick Setup - 1 (all)
## Quick Setup - 2 (Limited)
## Usage
## Unit tests
## Workflows/Release
```
- `# title` = repository name as is (`context-forge`, `c2dmp-hsm`, or the display name `Utils`, `S.O.S`).
- Small library (c2dmp-hsm, s.o.s): `# name`, then the acronym tagline in a quote with the bold letters
  (`> **S**teganography **O**ptimized and **S**ecurized`), then `## Installation` with `####` steps
  (`#### Cloning the repository`, `#### Build & Installation`, `#### Include`).
- Table of Contents: `### Table of Contents`, items ` - [Name](#anchor)` (one leading space).
- Only the sections that make sense for the project, always in this order.

## Sections content
- **Dependencies**: `> [!CAUTION]` license disclaimer, then a table
  `| Name + Link | Status | Last Update |` with the workflow badge and
  `![](https://img.shields.io/github/last-commit/<owner>/<repo>)`.
  Build requirements: `| Name | Version | Fedora (\`dnf\`) | Debian/Ubuntu (\`apt\`) |`.
- **Packages**: `> [!NOTE]` about the `-pre` packages, then `| File Name | Content |`.
- **Quick Setup - 1 (all)**: subtitle quote `> Setup into \`/usr/local\``, `### Clone the repository`,
  `### Build & install` (or `### Install the lib`) with bash blocks:
  ```bash
  export BUILD_DIR=build
  cmake -S . -B $BUILD_DIR
  sudo cmake --build $BUILD_DIR --target install --parallel $(nproc)
  ```
- **Quick Setup - 2 (Limited)**: `> Setup into \`/usr\``, the warnings (`invalid hash`, restriction
  `fedora-based (rpm)`, `debian-based (deb)`), the `--no-sudo` note, a numbered list of what the script does,
  the `wget -qO- ... | bash -s` command then `or with \`curl\`:` and the curl one, arguments table
  `| Argument | Effect |`.
- **Usage**: one bash block with commented commands aligned (`cmd   # comment`), then a table of modes/flags.
- **Include** (libraries): `> [!WARNING]` `Everything is defined within the namespace \`x::\``, table
  `| Include | Content |` with the header path and the main signatures in backticks.
- **Workflows/Release**: `### Workflows` (one bullet per workflow, name in backticks + what it does),
  `### Pre-Release (unstable)`, `### Release (stable)` (tag regex, `[build]`/`[release]` keywords).

## Formatting rules
- GitHub callouts everywhere a remark is needed, one idea per callout:
  `> [!NOTE]` info, `> [!TIP]` shortcut/doc link, `> [!IMPORTANT]` required action,
  `> [!WARNING]` restriction/known issue, `> [!CAUTION]` license/danger.
  Multi-paragraph callout: empty `> ` line between paragraphs.
- Tables for every list of items with attributes (packages, flags, includes, paths); header separator
  `| ---- |` sized like the header; code values in backticks.
- Code blocks always with a language (`bash`, `cpp`, `cmake`); comments inside the block rather than prose.
- Inline code for every path, command, flag, type, package, namespace, version.
- `**bold**` only for the key concept of a sentence; no italics.
- Links: relative for repo files, full URLs for other repos; anchors `#quick-setup---1-all` style.
- Sentences short, present tense, `ex:` for examples, `->` for consequences.
