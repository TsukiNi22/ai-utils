---
name: readme-style
description: Tsukini's style for README.md and Markdown documentation (from his own READMEs - libutils before AI help, context-forge, c2dmp-hsm, s.o.s, my_language - and the concise READMEs of YetAnotherMechanicusEnjoyer) - the sections and anchors of the existing READMEs kept (Dependencies, Packages, Quick Setup 1 (all) / 2 (Limited), Usage, Unit tests, Workflows/Release) plus optional centered badges, short Features, one Quick Setup per install channel and a closing section (License, Credits, Contributors...), the advanced detail (algorithms, script steps, pseudo code, workflows) in readme/ sub-docs, GitHub callouts, tables, bash blocks, and little text with limits by place (tagline, cells, callouts, paragraphs, sub-docs) checked by check_text.py. Use whenever writing, rewriting or reviewing a README or any Markdown doc for one of the user's projects. For a GitHub wiki use wiki-style; for CHANGELOG/release notes use git-conventions; for HTML docs use html-doc.
metadata:
  summary: 'README / Markdown docs: little text by place (checked), the usual sections kept (Dependencies, Packages, Quick Setup, Usage, Workflows), badges, Features, License, detail in readme/.'
---

# Tsukini README / Markdown doc style

English (unless the user asks for another language), direct, technical, no emoji, no marketing.
`SKILL_DIR` = the directory of this file.
**Location**: `README.md` at the root of the repository (`git rev-parse --show-toplevel`), or in the current folder
outside a repository; never in `docs/` unless the user asks for it. The detail goes in `readme/<subject>.md` next to it.
Templates: `templates/README.md` (application or full library), `templates/README-lib.md` (small header-only /
algorithm library), `templates/readme/workflows.md` (a sub-doc).
CHANGELOG and release descriptions: the `git-conventions` skill. HTML documentation: the `html-doc` skill.

**Models** (measured): the libutils README before AI help (branch `before-ai-help`) is ~580 words, 2294 now;
casm, vimcord, wayra (YetAnotherMechanicusEnjoyer) give 0-100 words of prose per section and one line per install
channel. A root README stays around **300-600 words of prose** (code blocks and tables not counted).

## Little text (first rule, every Markdown file)
A README is read at a glance: what it is, how to install it, how to use it. The rest is cut or moved to `readme/`.
- **Said once**: never what the heading, the table or the command already says (no "This section explains...", no
  sentence before a code block that repeats it, no callout repeating a table row).
- **Only what the reader acts on**: no history, no rationale, no plans, no speculation ("not tested on X yet"), no
  boilerplate ("Contributions are welcome..."), no list of every option when `--help` has them.
- **Commands over prose**: a commented bash block, not "To build the project, run the following command...".
- **Detail one level down**: step lists of a script, algorithms, pseudo code, complexity, every workflow, long option
  or keybind lists -> `readme/<subject>.md`, linked by one line in the README. The root README never copies them.
- **No filler**: `in order to` -> `to`, `allows you to build` -> `builds`, no `note that`, `simply`, `just`, `very`,
  `basically`, `currently`, `the following`.

The length depends on the place: tight where the reader scans, looser where they came to read.

| Place | Sentence | Block |
| ----- | -------- | ----- |
| tagline, intro under the title | 15 words | 1-2 lines |
| table cell | - | 8 words |
| list item, a feature | 15 words | one line |
| callout | 20 words | one idea, 2 lines |
| paragraph of a section (Usage, Description) | 20 words | 3 sentences |
| sub-doc `readme/*.md` (algorithm, steps, details) | 25 words | 4 sentences, 15-word cells |

| Before | After |
| ------ | ----- |
| This tool allows you to easily convert your images in order to use them on every platform. | Converts images for every platform. |
| To build the project, you simply need to run the following commands: | *(nothing: the bash block says it)* |
| Run the setup script directly, without cloning the repository manually. It: 1. Setup the mirror 2. Import the GPG key used to sign the packages/metadata... (+ 2 warnings, a note) | No clone needed. The script: 1. adds the mirror 2. imports its key 3. installs; one restriction callout, the errors in `readme/install.md` |

Check every Markdown file written or changed: `python3 SKILL_DIR/scripts/check_text.py <files.md>` (`LONG` sentence,
`PARA` paragraph, `NOTE` callout, `CELL` table cell, `ITEM` list item, `FILLER` word, `SIZE` README over 600 words of
prose). A file under `readme/` or `docs/` (or `--sub`) gets the sub-doc limits. Fix each line, or keep it on purpose
and say why; a `(generated)` line is fixed in its source (ex: the frontmatter).

## Structure (full project: libutils, context-forge)
The sections and anchors of the existing READMEs (libutils, context-forge, the READMEs of `cpp_project_template`)
stay as they are: a README is shortened section by section, never renamed or reordered.
```markdown
# <project-name>

<p align="center">                                              <- optional: release, CI, last commit, license
  <img alt="Version" src="https://img.shields.io/github/v/release/<owner>/<repo>">
  <img alt="CI" src="https://github.com/<owner>/<repo>/actions/workflows/dispatch.yml/badge.svg">
  <img alt="Last commit" src="https://img.shields.io/github/last-commit/<owner>/<repo>">
  <img alt="License" src="https://img.shields.io/github/license/<owner>/<repo>">
</p>

> [!TIP]
> Documentation [<owner>/<repo>](<doc url>) (vX.Y.Z).          <- only if an HTML/wiki doc exists

<1-2 lines: what it does; the limit or fallback only if any>

### Table of Contents
 - [Features](#features)                                       <- an application with several features only
 - [Dependencies](#dependencies)
 - [Packages](#packages)
 - [Quick Setup 1 (All)](#quick-setup---1-all)
 - [Quick Setup 2 (Limited)](#quick-setup---2-limited)
 - [Usage](#usage)
 - [Unit tests](#unit-tests)
 - [Workflows/Release](#workflowsrelease)
 - [License](#license)                                         <- or Credits / Contributors / Acknowledgements

## Features
## Dependencies
## Packages
## Quick Setup - 1 (all)
## Quick Setup - 2 (Limited)
## Usage
## Unit tests
## Workflows/Release
## License                                                     <- the closing section
```
- `# title` = repository name as is (`context-forge`, `c2dmp-hsm`, or the display name `Utils`, `S.O.S`).
- Small library (c2dmp-hsm, s.o.s): `# name`, then the acronym tagline in a quote with the bold letters
  (`> **S**teganography **O**ptimized and **S**ecurized`), then `## Installation` with `####` steps
  (`#### Cloning the repository`, `#### Build & Installation`, `#### Include`), `## Description` (1-3 sentences and
  the core principles as short bullets), `## Complexity` (the parameters and the results as tables), the closing section (`## License`, `## Credits`...);
  the derivation, the pseudo code and the long explanations in `readme/algorithm.md`.
- Table of Contents: `### Table of Contents`, items ` - [Name](#anchor)` (one leading space).
- Only the sections that make sense for the project, always in this order.

## Sections content
- **Badges** (optional): one centered `<p align="center">` block under the title; the badges of the dependencies stay
  in their table.
- **Features**: 3-6 bullets `- **Label:** a few words`; none for a library whose Include table says it all.
- **Dependencies** (always, written as before): `> [!CAUTION]`
  `This project's license does not apply to the content of the dependencies used by \`<project>\`.`, then a table
  `| Name + Link | Status | Last Update |` with the workflow badge and
  `![](https://img.shields.io/github/last-commit/<owner>/<repo>)` (own repositories), then
  `Build requirements (only needed for the [Quick Setup 1](#quick-setup---1-all)):` and
  `| Name | Version | Fedora (\`dnf\`) | Debian/Ubuntu (\`apt\`) |` (tools and system packages). A project without
  dependency of its own keeps the section with the requirements table only.
- **Packages**: `> [!NOTE]` `<name>-pre` is the pre-release (one line), then `| File Name | Content |`.
- **Quick Setup**: one section per install channel, the old names kept - `1 (all)` from source into `/usr/local`,
  `2 (Limited)` the `setup.sh` packages into `/usr`; another channel adds `3 (AUR)`, `4 (binaries)`...
  - **1 (all)**: subtitle quote `> Setup into \`/usr/local\``, `### Clone the repository`, `### Build & install`
    (or `### Install the lib`), each one bash block:
    ```bash
    export BUILD_DIR=build
    cmake -S . -B $BUILD_DIR
    sudo cmake --build $BUILD_DIR --target install --parallel $(nproc)
    ```
  - **2 (Limited)**: `> Setup into \`/usr\``, one `> [!WARNING]` with the restriction (`fedora-based (rpm)`,
    `debian-based (deb)`), what the script does in 3 short numbered steps, the `curl -fsSL ... | bash -s` command
    then `or with \`wget\`:` and the wget one, the table `| Argument | Effect |` (3 rows at most). The known errors
    (`invalid hash`), the other arguments and the details of the steps go in `readme/install.md`.
- **Usage**: one bash block with commented commands aligned (`cmd   # comment`), then a table of the main modes /
  flags (the full list: `--help` or `readme/usage.md`).
- **Include** (libraries): `> [!WARNING]` `Everything is defined within the namespace \`x::\``, table
  `| Include | Content |` with the header path and the main signatures in backticks.
- **Workflows/Release**: `### Workflows` (one bullet per workflow, name in backticks + what it does in a few words;
  past 3 workflows, the table in `readme/workflows.md`), `### Pre-Release (unstable)` and `### Release (stable)` (one
  line each: the tag `vX.Y.Z-pre` / `vX.Y.Z`, the `[build]` / `[release]` keywords).
- **Closing section** (always last, a few lines): `## License` (`[MIT](LICENSE)`), `## Credits` (the third-party files
  and where they come from), `## Contributors` (names and links), or another name that fits (`## Acknowledgements`);
  several of them only when each has its own content, ex: `## Contributors` then `## License`.

## Formatting rules
- GitHub callouts where a remark is needed, one idea per callout:
  `> [!NOTE]` info, `> [!TIP]` shortcut/doc link, `> [!IMPORTANT]` required action,
  `> [!WARNING]` restriction/known issue, `> [!CAUTION]` license/danger.
- Tables for every list of items with attributes (packages, flags, includes, paths); header separator
  `| ---- |` sized like the header; code values in backticks.
- Code blocks always with a language (`bash`, `cpp`, `cmake`); comments inside the block rather than prose.
- Inline code for every path, command, flag, type, package, namespace, version.
- `**bold**` only for the key concept of a sentence or a feature label; no italics.
- Links: relative for repo files (`readme/install.md`), full URLs for other repos; anchors `#quick-setup---1-all` style.
- Present tense, `ex:` for examples, `->` for consequences, `Requires X` rather than a sentence.
