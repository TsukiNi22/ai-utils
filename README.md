# ai-utils

> [!TIP]
> [Graph explorer](https://tsukini22.github.io/ai-utils/): the project graph of any public GitHub / GitLab repository,
> in the browser. [claude-cluster docs](https://tsukini22.github.io/ai-utils/claude-cluster/).

[Claude Code skills](https://docs.claude.com/en/docs/claude-code/skills) built from my own projects and habits: each
one teaches the assistant how I work (code style, file layout, tooling), so that what it writes looks like my code.
One installer, [`setup.sh`](setup.sh), for the skills, the [tools](#tools) and the global [context](#context).

<!-- generated:counts -->
**60 skills** in 12 categories and **9 routers**.
<!-- /generated:counts -->

### Table of Contents
 - [Quick Setup - 1 (without cloning)](#quick-setup---1-without-cloning)
 - [Quick Setup - 2 (from a clone)](#quick-setup---2-from-a-clone)
 - [Usage](#usage)
 - [Routers](#routers)
 - [Skills](#skills)
 - [Tools](#tools)
 - [Context](#context)
 - [Layout](#layout)

## Quick Setup - 1 (without cloning)
> Clones (or updates) the repository into `~/.local/share/tsukini-skills`, then links everything from there

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install
```

or with `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install
```

> [!NOTE]
> Requires `git` and `bash`. `install` alone takes everything; replace it, or add names after it, to choose:
>
> | After `bash -s --` | Effect |
> |---|---|
> | `list` | what is available: categories, skills, tools, context (nothing installed) |
> | `install cpp-class git` | only these skills (+ the skills they require) |
> | `install cpp/ docs/` | whole categories (`@cpp` works too) |
> | `install skills` / `install tools` / `install context` | one kind only: every skill, every tool (claude-cluster), the global context |
> | `install skills context` | kinds and names mix |
> | `status`, `update`, `doctor --fix`, `remove` | the other commands, the same as from a clone (next part) |

## Quick Setup - 2 (from a clone)

```bash
git clone git@github.com:TsukiNi22/ai-utils.git
cd ai-utils
./setup.sh install              # every skill (symlinks in ~/.claude/skills), the tools and the context
```

```bash
./setup.sh list                 # what is available: categories, skills, tools, context
./setup.sh status               # what is installed
./setup.sh install cpp-class    # one skill, and the skills it requires
./setup.sh install cpp/ docs/   # whole categories
./setup.sh update               # pull the last version, repair the links, rebuild the tools
./setup.sh doctor --fix         # repair the installed skills (moved, removed, missing requirements)
./setup.sh remove               # remove everything this repository installed
```

> [!TIP]
> Every command and option, the plugin marketplace (no `setup.sh`: another machine, Claude Code on the web) and
> claude.ai: [`INSTALL.md`](INSTALL.md).

## Usage
Once installed, a skill is used **automatically** when the request matches it, or explicitly with `/<skill>`.

| I want to... | Ask or run |
|---|---|
| set up a whole new project (C++, Python, other) | `/setup-project` |
| add C++ classes / files | *"create a `Timer` class in `utils::system`"* (`cpp-class`) |
| write in my style, check the style | *"rewrite `install.sh` in my style"*, `/style` |
| add unit tests | *"set up the unit tests of this project"* (`cpp-tests`, `tests`) |
| commit the pending changes | *"commit the changes"* (`git-commit`) |
| a CI / CD workflow | *"add a CI with the tests and xstyle"* (`ci-cd`) |
| audit a project (bugs, security, quality, dependencies) | `/audit` |
| document a project (README, wiki, HTML docs, a site) | `/doc` |
| update every doc after a change | *"update all the docs"* (`docs-update`) |
| a PDF report, a presentation | *"make a PDF report of ..."* (`report`), *"make slides on ..."* (`presentation`) |
| be asked the doubts first, get ideas | `/clarify`, `/ideation` |
| stop cleanly now, resume later | `/pause`, then *"continue"* (`continue-work`) |
| hand the work to another session | *"export the session"* (`export`) |
| free disk space | *"clean my computer"* (`computer-clean`) |

## Routers
Invoked by hand, they read the request, the project and the language, then load the right skills by themselves.
`/dev` is enough most of the time.

<!-- generated:routers -->
| Router | What it does |
|---|---|
| [`/dev`](skills/routers/dev/SKILL.md) | Global development entry point: looks at the request, the project and the language, then loads the right skills of every category (and the other routers). |
| [`/audit`](skills/routers/audit/SKILL.md) | Asks which audits to run (checklist: `audit-bugs`, `audit-security`, `audit-quality`, `audit-deps`, `legal-check`, `coverage`, `benchmark`; coverage and benchmark checked and run automatically) and the report format, runs them and writes a summary report in `audit/`. |
| [`/cpp`](skills/routers/cpp/SKILL.md) | Loads `cpp-project`, `cpp-class`, `cpp-style`, `cpp-comments`, `cmake-style`, `cpp-tests`, `audit-bugs`, `audit-quality`, `libutils`, `libutils-exception`, `libutils-install`, `libutils-setup`, `coverage`, `benchmark`, `ci-cd`. |
| [`/doc`](skills/routers/doc/SKILL.md) | Loads `readme-style`, `html-doc`, `html-style`, `explain-doc`, `protocol-doc`, `better-html-doc`, `better-html-style`, `better-explain-doc`, `better-protocol-doc`, `report`, `docs-update`, `git-conventions` (CHANGELOG), `comments`, `cpp-comments`. |
| [`/git`](skills/routers/git/SKILL.md) | Loads `git-conventions`, `git-commit`, `ci-cd`, `pre-commit`. |
| [`/legal`](skills/routers/legal/SKILL.md) | Loads `license`, `audit-deps`, `legal-check`, `legal-setup`. |
| [`/python`](skills/routers/python/SKILL.md) | Loads `python-project`, `python-class`, `python-style`, `python-comments`, `tests`, `ci-cd`, `benchmark`. |
| [`/setup-project`](skills/routers/setup-project/SKILL.md) | A whole new project in order: architecture (`cpp-project` / `python-project` / `coding-style`), base unit tests (`cpp-tests` / `tests`), `license`, `git-conventions` + `pre-commit`, `ci-cd`, `readme-style`, then a quick architecture audit. |
| [`/style`](skills/routers/style/SKILL.md) | Loads `cpp-style`, `python-style`, `python-comments`, `coding-style`, `cpp-comments`, `comments`, `cmake-style` (by file type), `audit-quality`. |
<!-- /generated:routers -->

## Skills
One folder per category in [`skills/`](skills/README.md); the README of each category has the detailed table (what
each skill does, what it installs with it) and examples.

<!-- generated:categories -->
| Category | What it covers | Skills |
|---|---|---|
| [C++](skills/cpp/README.md) | Project setup, classes / architecture, style, comments, CMake | `cmake-style`, `cpp-class`, `cpp-comments`, `cpp-project`, `cpp-style` |
| [Python](skills/python/README.md) | Project setup, files / classes scaffolding, style and comments | `python-class`, `python-comments`, `python-project`, `python-style` |
| [libutils](skills/libutils/README.md) | API reference, exceptions, install / update, setup in a project | `libutils`, `libutils-exception`, `libutils-install`, `libutils-setup` |
| [Tests](skills/tests/README.md) | GoogleTest (C++), the other frameworks, coverage | `coverage`, `cpp-tests`, `tests` |
| [Style](skills/style/README.md) | Code style and comments for the other languages | `coding-style`, `comments` |
| [Documentation](skills/docs/README.md) | README, GitHub wiki, uniform HTML docs with the project graph, HTML style, explanation pages (simple + technical), protocol / wire format docs, Markdown / PDF reports, update of every doc / site / package / install script after changes, documentation sites with search in the claude-cluster style | `better-explain-doc`, `better-html-doc`, `better-html-style`, `better-protocol-doc`, `docs-update`, `explain-doc`, `html-doc`, `html-style`, `protocol-doc`, `readme-style`, `report`, `wiki-style` |
| [Git](skills/git/README.md) | Git conventions and local commit of the pending diff (commits, branches, pull requests, changelog), the pre-commit hooks and the CI/CD workflows | `ci-cd`, `git-commit`, `git-conventions`, `pre-commit` |
| [Audit](skills/audit/README.md) | Bugs / UB, security (passes fixing every flaw until a clean one), quality, dependencies (licenses, vulnerabilities), benchmark / profiling | `audit-bugs`, `audit-deps`, `audit-quality`, `audit-security`, `benchmark` |
| [Legal](skills/legal/README.md) | Its license, and the compliance check and setup - GDPR / RGPD, cookies, legal notice, terms, consumer law, accessibility (RGAA, EAA), language, licenses and credits, mandatory mentions, DSA, AI Act, CRA, NIS2, Data Act - global or web | `legal-check`, `legal-setup`, `license` |
| [UX](skills/ux/README.md) | Several languages (multilingual-project, multilingual-website) and visual modes light / dark / system / high contrast (color-modes-project, color-modes-website) | `color-modes-project`, `color-modes-website`, `multilingual-project`, `multilingual-website` |
| [System](skills/system/README.md) | Free space on the computer and in the data of Claude Code / claude-cluster, chosen in a QCM | `computer-clean` |
| [AI workflow](skills/ai/README.md) | --rtk outputs, QCM questions, clarification, ideation, a run without interruption, a clean pause resumed at the next continue, session context, writing in the user's voice, handover, onboarding, presentations (PDF / HTML / PPTX), full git permission | `ask-at-end`, `clarify`, `continue-work`, `export`, `full-permission`, `ideation`, `interactive-questions`, `onboard`, `pause`, `presentation`, `rtk`, `setup-context`, `write-like-me` |
<!-- /generated:categories -->

## Tools

| Tool | Description |
|---|---|
| [`claude-cluster`](tools/claude-cluster/README.md) | Several Claude Code sessions (and other agents) driven from one terminal (FTXUI) or window (Qt6): transcripts, skills, git, tokens, permissions, a global session driving the others, voice, headless commands. Built and installed by `setup.sh` (`~/.local/bin`). |
| [`xstyle`](https://github.com/TsukiNi22/xstyle) | Own repository: checker / fixer of my coding style used by the skills, the pre-commit hook and the CI images (`sudo dnf install xstyle` from [my-universe](https://tsukini22.github.io/my-universe/)). |
| [Graph explorer](https://tsukini22.github.io/ai-utils/) | Web page of the `gh-pages` branch: the project graph of [`html-doc`](skills/docs/html-doc/SKILL.md) for any public GitHub / GitLab repository (`?repo=owner/name`). |

## Context
What every Claude Code session loads on my computers, not a skill: the global `CLAUDE.md`, `RTK.md`, the session
hooks and `sudo-askpass`. Installed by the same `setup.sh` in `~/.claude` (`./setup.sh context <command>` for it
alone); details in [`context/README.md`](context/README.md).

## Layout

```text
ai-utils/
├── skills/       the skills, one folder per category (skills/README.md); the routers in skills/routers/
├── tools/        the programs built by setup.sh (claude-cluster)
├── context/      the global context of Claude Code (CLAUDE.md, RTK.md, hooks, sudo-askpass)
├── claude-ai/    the claude.ai packaging (build.py, preferences.md)
├── scripts/      repository scripts: the README tables (readme.py), the checks run by the CI (check.py)
├── audit/        reports of the audits run on this repository
├── setup.sh      installer of the skills, the tools and the context
└── INSTALL.md    every command of setup.sh, the plugin marketplace, claude.ai
```

## Credits
`skills/cpp/cpp-class/scripts/fonts/ansi_shadow.flf`: ANSI Shadow FIGlet font, as distributed with
[pyfiglet](https://github.com/pwaller/pyfiglet) (MIT). Everything else is based on my own work, mainly
[libutils](https://github.com/TsukiNi22/libutils).
