# ai-utils

AI skills, routers, tools and context based on my work.

This repository holds [Claude Code skills](https://docs.claude.com/en/docs/claude-code/skills)
built from my own projects and habits. Each skill teaches the assistant how I work (code style,
file layout, tooling), so that what it generates looks like something I wrote myself.
Four kinds of things, all installed by the same [`setup.sh`](#installation): the [routers](#routers), the
[skills](#skills), the [tools](#tools) and the global [context](#context).

## Routers
Skills invoked **manually** that look at the request, the project and the language, then load the right
skills below by themselves. `/dev` is enough most of the time.

| Router | Loads |
|---|---|
| [`/dev`](routers/dev/SKILL.md) | everything below, depending on the context (global development entry point) |
| [`/cpp`](routers/cpp/SKILL.md) | `cpp-project`, `cpp-class`, `cpp-style`, `cpp-comments`, `cmake-style`, `cpp-tests`, `audit-bugs`, `audit-quality`, `libutils`, `libutils-exception`, `libutils-install`, `libutils-setup`, `coverage`, `benchmark`, `ci-cd` |
| [`/python`](routers/python/SKILL.md) | `python-project`, `python-class`, `python-style`, `python-comments`, `tests`, `ci-cd`, `benchmark` |
| [`/setup-project`](routers/setup-project/SKILL.md) | a whole new project in order: architecture (`cpp-project` / `python-project` / `coding-style`), base unit tests (`cpp-tests` / `tests`), `license`, `git-conventions` + `pre-commit`, `ci-cd`, `readme-style`, then a quick architecture audit |
| [`/git`](routers/git/SKILL.md) | `git-conventions`, `git-commit`, `ci-cd`, `pre-commit` |
| [`/doc`](routers/doc/SKILL.md) | `readme-style`, `html-doc`, `html-style`, `explain-doc`, `protocol-doc`, `report`, `docs-update`, `git-conventions` (CHANGELOG), `comments`, `cpp-comments` |
| [`/style`](routers/style/SKILL.md) | `cpp-style`, `python-style`, `python-comments`, `coding-style`, `cpp-comments`, `comments`, `cmake-style` (by file type), `audit-quality` |
| [`/legal`](routers/legal/SKILL.md) | `license`, `audit-deps`, `legal-check`, `legal-setup` |
| [`/audit`](routers/audit/SKILL.md) | asks which audits to run (checklist: `audit-bugs`, `audit-security`, `audit-quality`, `audit-deps`, `legal-check`, `coverage`, `benchmark`; coverage and benchmark checked and run automatically) and the report format, runs them and writes a summary report in `audit/` |

## Graph explorer

**https://tsukini22.github.io/ai-utils/** (branch [`gh-pages`](https://github.com/TsukiNi22/ai-utils/tree/gh-pages)) - the
project graph of [`html-doc`](docs/html-doc/SKILL.md) for **any public GitHub or GitLab repository**: give its link
(`owner/repo`, a GitHub / GitLab URL, a branch with `/tree/<branch>`) and the page reads its sources in the browser to
draw its files, classes, functions, calls, tests and packages in 2D / 3D, plus the execution flow, inheritance tree and
flame graph diagrams. C / C++, Python, JS / TS / web, Java / Kotlin, C#, Go, Rust, Zig, Swift, Dart, PHP, Ruby, Lua,
Julia, Elixir, Haskell, Objective-C and shell are read. The link stays editable (and `?repo=owner/name` opens one
directly); it is remembered by the browser until **Ctrl + Shift + R** (forgets it and reloads the page).
## Skills

| Skill | Description |
|---|---|
| [`cpp-class`](cpp/cpp-class/SKILL.md) | Sets up a C++20 architecture / new `.hpp` & `.cpp` files (interface `I*`, abstract `A*`, class, template, `Type`/`Define`, family headers, `main.cpp` entry point) with the namespaces, the Xartania header and **empty bodies only** (never the logic), picks libutils attributes or standard `[[...]]` ones and registers the sources in the `CMakeLists.txt`. Requires `cpp-style` & `cpp-comments`. |
| [`cpp-style`](cpp/cpp-style/SKILL.md) | My C++ coding style: naming, indentation, braces, spacing, loops, switch, lambdas, const correctness, attributes. |
| [`coding-style`](style/coding-style/SKILL.md) | My coding style for every language other than C++ (Python, shell, Lua, JS/TS, C, Rust/Go, YAML/JSON), derived from `cpp-style` and my real scripts. |
| [`cpp-comments`](cpp/cpp-comments/SKILL.md) | How I comment C++ code: section separators, `/* group */` labels, aligned trailing comments, no Doxygen. |
| [`git-conventions`](git/git-conventions/SKILL.md) | Commit messages `type(scope): message`, CI keywords, tags, GitHub releases, CHANGELOG, branches (`main` alone when solo; `main`/`dev`/`sub/`/`feat/`/`fix/` in a team) and PRs (CHANGELOG-style body, `gh` assignee/labels), no AI attribution. |
| [`readme-style`](docs/readme-style/SKILL.md) | README / Markdown docs structure: Table of Contents, Dependencies, Packages, Quick Setup, Usage, GitHub callouts, tables. |
| [`wiki-style`](docs/wiki-style/SKILL.md) | GitHub wiki of a project, from the libutils wiki: the `<repo>.wiki.git` repository, `Home` (version stamp, pages, dependencies), `_Sidebar` / `_Footer`, guide pages, the `Sections` / `Tools-Preview` overviews and one page per section (opening callout, Table of Contents, tables of members, Example, Errors); templates and `scripts/check_wiki.py` (links, anchors, sidebar coverage, code fences, tables). |
| [`html-doc`](docs/html-doc/SKILL.md) | One uniform style for the HTML documentation: an optional user guide, the technical documentation and an interactive 2D/3D project graph of any common stack (C / C++, Python, JS / TS / web, Java, C#, Go, Rust, PHP, Ruby) with execution / inheritance / flame diagrams (filters by category/group/relation, rebuilt from the GitHub repository with an update button), all static self-contained pages with a sun/moon theme button. |
| [`html-style`](docs/html-style/SKILL.md) | My visual style for any HTML page (from `html-doc`): light / dark color tokens, system fonts and type scale, layout, components, SVG diagram classes, sun/moon and EN/FR switches, rules (self-contained, WCAG contrast, **responsive on phones / tablets / TV / touch**: viewport, safe areas, 44px targets, no hover-only content, print); `scripts/new_page.py` builds a self-contained page, `scripts/check_style.py` checks a page or a palette (tokens, contrast, unreadable inherited colors, external resources). |
| [`explain-doc`](docs/explain-doc/SKILL.md) | Explanation pages (how something works) in two levels switched in place, **Simple** (analogies, plain words, numbers) and **Technical** (terms, formulas, complexity, edge cases, code, sources), with SVG diagrams, step-by-step animations, MathML formulas, interactive playgrounds / simulations checking the formulas, quizzes and a glossary. Built on `html-style`. |
| [`protocol-doc`](docs/protocol-doc/SKILL.md) | Protocol / wire format documentation (from the 2ETP doc of my networking projects): framing with the control bytes, the payload types table, one section per message type (grammar + fields), the code tables (errors, events, commands), every frame in one block, the handshake step by step with a sequence diagram, the client / server commands, the storage format; always asks the output - Markdown, PDF (report style) or sections of the existing HTML docs (html-doc) - and checks it (`check_protocol.py`: anchors, types, frames, codes). |
| [`docs-update`](docs/docs-update/SKILL.md) | Brings every piece of documentation up to date after changes or a new version: README, CHANGELOG, HTML docs and graph, `gh-pages`, wiki, packages / manifests / version numbers, setup and install scripts, `--help` texts, completions, marketplace and skill lists, claude.ai bundles - finds every place the change is written, takes the newest version from the code, verifies against the code (`scripts/inventory.sh`), commits nothing on its own. |
| [`computer-clean`](system/computer-clean/SKILL.md) | Frees space on the computer and in the data of Claude Code / claude-cluster: caches, trash, old build outputs (never a git-tracked file), Docker / Podman, journal, dnf, flatpak, big old downloads, old conversations, finished jobs, worktrees without changes, claude-cluster leftovers - listed with size and why, chosen in a QCM, then deleted for good. |
| [`cpp-project`](cpp/cpp-project/SKILL.md) | Sets up a new C++ project with the `setup.sh` of [cpp_project_template](https://github.com/TsukiNi22/cpp_project_template) (never committing): renaming, binary / `.a` / `.so` / header-only, RPM/DEB packages of every variant, Linux + Windows (MinGW-w64) builds and packages, libutils (v4.1) or not, exceptions, GTest, the template's workflows (dispatch, unit tests, build, packages, GitHub release from the CHANGELOG, docs), optional gh-pages branch (signed packages repository + docs), README/CHANGELOG/docs, using the other skills. |
| [`cmake-style`](cpp/cmake-style/SKILL.md) | `CMakeLists.txt` in my style: section order, explicit sources, Debug/Asan/Optimized modes, release targets, GTest tests, install + `find_package` config, CPack RPM/DEB stable/pre channels, with app/lib/tests templates. |
| [`cpp-tests`](tests/cpp-tests/SKILL.md) | C++ unit tests like libutils: GoogleTest setup (CMake, CI, `unit_tests`), one test file per module, isolation of the blocking cases, known bugs kept as failing tests, `scripts/untested.py` to list what is never tested. |
| [`tests`](tests/tests/SKILL.md) | Unit tests for any language: detects the existing framework or asks which one to use, then setup, tests, CI (defers to `cpp-tests` for C++). |
| [`comments`](style/comments/SKILL.md) | My comment style for every language (C, C++, CMake, Makefile, shell, Python, Lua, YAML, Markdown) and which file header goes where. |
| [`audit-bugs`](audit/audit-bugs/SKILL.md) | Bug / UB audit loop (sanitizers, tests, static tools, code review, verification of each finding), single agent or subagents on request, fixes only after confirmation. |
| [`audit-security`](audit/audit-security/SKILL.md) | Security / memory-safety audit of the whole project in passes back to back (sanitizers, dependencies, static tools, fuzzing, attacks on the running program, a review of memory, DoS, injection, auth, secrets, crypto, files, network), each finding verified and fixed at once, a new pass while the last one still found something; a PDF report of what was fixed. |
| [`audit-quality`](audit/audit-quality/SKILL.md) | Cleanliness / conventions audit of a project (`scripts/collect.py` metrics + review against the style skills) delivered as a PDF report. |
| [`report`](docs/report/SKILL.md) | Reports in my style as Markdown and/or PDF (format asked: both recommended), English by default (Markdown -> WeasyPrint, A4, Noto, navy title rule, tables and callouts) with `scripts/md2pdf.py`. |
| [`license`](legal/license/SKILL.md) | Finds the license matching the needs (comparison matrix), negotiates the close ones, fetches the official text, replaces the current LICENSE only after confirmation, checks dependencies compatibility. |
| [`legal-check`](legal/legal-check/SKILL.md) | Deep legal compliance check, global (any project handling data) or web (a site / web app, on top): GDPR (bases, consent, notices, rights incl. erasure, records, DPAs, DPIA, retention, security, breaches, transfers), cookies (ePrivacy / CNIL), legal notice (LCEN), terms, consumer law, accessibility (RGAA, EAA), language (loi Toubon), licenses and credits of every dependency / font / asset, mandatory mentions, DSA, AI Act, CRA, NIS2, Data Act, UK / Swiss / US laws; each finding with its article, the evidence in the project and the fix; dates checked on the primary texts; a report. |
| [`legal-setup`](legal/legal-setup/SKILL.md) | Makes it compliant: writes the documents from the real data flows (privacy policy, mentions légales, cookie policy, CGU / CGV, accessibility statement, records of processing, retention, rights and breach procedures, credits, security.txt) after asking every fact it cannot know (never invented), and implements the code (consent banner blocking trackers until consent, refuse = accept, export and real erasure, purge jobs, form notices, unsubscribe, AI disclosure, accessibility fixes, footer links). |
| [`audit-deps`](audit/audit-deps/SKILL.md) | Dependencies (direct + transitive): licenses to credit, copyleft / non-commercial / paid / unknown ones, THIRD_PARTY_NOTICES generation, and known vulnerabilities / compromised versions (OSV, dnf, GitHub advisories), as a PDF report. |
| [`python-style`](python/python-style/SKILL.md) | My Python style, measured on my scripts (libutils `cmake/scripts`, MAGIC): header, `##### sections #####`, checked imports with a `# Used for` each, `__main__` guards, `const.py` with frozen dataclasses, exit codes, errors on stderr, type hints; checked by the `PY-*` rules of `xstyle`. |
| [`python-class`](python/python-class/SKILL.md) | Python equivalent of `cpp-class`: scaffolds class / abstract / tool / module / exception / const / test files (`scripts/new_file.py`) with the header, sections, checked imports, typed signatures and **empty bodies only**, the parent import and the `__all__` of a package. |
| [`python-comments`](python/python-comments/SKILL.md) | Python equivalent of `cpp-comments`: `##### Section #####` banners, import labels, a `# Used for` on every import, aligned trailing comments, block comments, short indented docstrings, never Sphinx fields. |
| [`python-project`](python/python-project/SKILL.md) | New Python project: script, application in the MAGIC layout (`main.py` / `app.py` / `const.py`, `Class/`, `tool/`, `data/`) or installable package (src layout, single source version, `main()` returning the exit code, console command, pytest), generated by `scripts/new_project.py`. |
| [`ci-cd`](git/ci-cd/SKILL.md) | CI **and CD** of a GitHub repository: always asks what to run (build, tests, xstyle style check, coverage / GitHub release from the CHANGELOG, RPM/DEB packages, container image, PyPI, docs, dispatch), my Docker images first (`ghcr.io/tsukini22/*`) or the smallest official one, my conventions, a missing tool added to the docker-image repository with a local commit. Used by `cpp-project` / `python-project` on GitHub repositories. |
| [`pre-commit`](git/pre-commit/SKILL.md) | xstyle pre-commit hook of a repository: asks every option (fail on major / warn on minor by default, staged lines, safe fixes, missing xstyle = warning only, local or shared `.githooks`), keeps an existing hook, installs the missing tools on request, demo repository to try it (`scripts/demo_repo.sh`); also `./setup.sh hook` from the repository. |
| [`git-commit`](git/git-commit/SKILL.md) | Commits the diff that exists right now, locally: reads the changes, groups them in logical commits (CHANGELOG last), stages by path, one title-only message each (`type(scope): message`, no body, no co-author, per `git-conventions`), never pushes; the ai-utils repository keeps its own trailer. |
| [`interactive-questions`](ai/interactive-questions/SKILL.md) | Asks through the built-in QCM / Q&A popup (AskUserQuestion) instead of a list in the text: questions, proposals, ideas, choices, rounds of ideas; French labels, 1-4 questions × 2-4 options, the recommended one first, multi choice, previews to compare, the answers (and their notes) followed exactly. Works in claude-cluster too (its popups, the voice, *Global answers*). |
| [`clarify`](ai/clarify/SKILL.md) | Summoned before a request: lists everything unclear (scope, ambiguous words, missing details, several approaches, contradictions, risky actions), asks it through `interactive-questions` in as many rounds as needed (structuring questions first), then acts on the answers; no popup when nothing is ambiguous. |
| [`ideation`](ai/ideation/SKILL.md) | Ideas, recommendations, an honest opinion or variants of ideas, asked through `interactive-questions`: grounded in the real project, 8-16 ideas by theme (effort / gain each) or the best few ranked, per idea keep / change / drop, variants compared with previews; rounds of which to keep and when (now, backlog, POC first), then a recap and the kept ones stored as tasks. |
| [`export`](ai/export/SKILL.md) | A handover file for another session / machine / model (`PASSATION.md` by default): goal, rules of the user, done / running / todo (each with its exact next step) / blocked, decisions, files and repo state, how to build / test / run, tools and scripts used, traps met - terse for an AI (`rtk`), never a secret |
| [`onboard`](ai/onboard/SKILL.md) | The whole context of a project before working on it: the folder read entirely (tree, docs, code, build / CI, license and credits, links), its git history (branches, tags, worktrees, stashes, pending and unpushed work), every earlier session on this computer that touched it (Claude Code, claude-cluster, Codex, Qwen, opencode, PASSATION.md) - merged, checked against the code, reported tersely; read-only |
| [`multilingual-project`](ux/multilingual-project/SKILL.md) | Any project in several languages: the languages asked (a few, Europe, the most spoken, as many as possible, RTL), every user string in standard catalogs (Qt `.ts`, gettext `.po`, Fluent, Android / iOS, ICU JSON) with named placeholders, plurals, locale formats, RTL and fonts, the language setting + system locale + fallbacks, AI-drafted translations marked for review, missing / unused keys checked in the CI. |
| [`multilingual-website`](ux/multilingual-website/SKILL.md) | The web part of `multilingual-project`: the framework's library, `/fr/` URLs and translated slugs, hreflang + x-default, sitemaps, `lang` / `dir`, a switcher with the languages written in themselves (no flags), Accept-Language suggested never forced, SEO meta, emails in the user's language, legal pages per language. |
| [`color-modes-project`](ux/color-modes-project/SKILL.md) | Light / dark / system (and high contrast, sepia, palettes, an accent) for any project: the modes asked, semantic colour tokens, WCAG contrast checked in each mode, the system scheme followed live (Qt, GTK portal, Windows, macOS, Android, iOS, terminals), persisted and switched without restart, icons / charts / highlighting adapted. |
| [`color-modes-website`](ux/color-modes-website/SKILL.md) | The web part of `color-modes-project`: CSS variables per mode, `prefers-color-scheme` + a system / light / dark switch on `data-theme`, no flash (inline head script), `color-scheme` / `theme-color`, `light-dark()`, `prefers-contrast` / `forced-colors` / reduced motion, images and charts per mode, Tailwind / component libraries, an accessible sun / moon toggle. |
| [`rtk`](ai/rtk/SKILL.md) | Token-minimal text for an AI: compacts a text / output / log (legend once, one record per line, shared values written once, one summary line; `scripts/compact.py` first pass) and designs / implements an `--rtk` mode on a program or script, like `xstyle --rtk`. |
| [`benchmark`](audit/benchmark/SKILL.md) | Profiling of the current program (perf stat / record, hyperfine, callgrind, heaptrack, flame graph; missing tools installed only after a yes): hot functions, why they are slow, quick wins vs complex changes with their gain, PDF report, optional before / after. |
| [`coverage`](tests/coverage/SKILL.md) | Test coverage of a `cpp-tests` project (clang / llvm-cov in a copy): per file, functions never called, ranges never run; then tests written module by module until it stops growing, and always the question of a report on what is still missing. |
| [`libutils`](libutils/libutils/SKILL.md) | Reference of my library [libutils](https://github.com/TsukiNi22/libutils): every section, header and public API generated from a recorded version/commit (`reference/VERSION.md`), integration and conventions, plus `scripts/update.sh` to see what changed since that commit and regenerate. |
| [`libutils-exception`](libutils/libutils-exception/SKILL.md) | libutils exceptions: classes, codes, throw/catch patterns, and how to add new exception codes to a project (JSON, generator script, CMake block). Requires `libutils`. |
| [`libutils-install`](libutils/libutils-install/SKILL.md) | Installs / updates / removes libutils on the computer by itself from the OS (dnf, apt, or build from the sources), the newest version (stable or `libutils-pre`, whichever is newer), debug/asan variants, mirror setup, `/usr/local` shadowing check, libutils for Windows (v4, MinGW-w64 cross compilation, no root). |
| [`libutils-setup`](libutils/libutils-setup/SKILL.md) | Adds libutils to the current project: `find_package`/`utils::utils` in the CMake and optionally the custom exceptions (JSON, scripts, generation block), installing libutils first if needed. |

Everything is based on my own work, mainly [libutils](https://github.com/TsukiNi22/libutils).

## Tools

Programs stored next to the skills they enforce, built and installed by the same `setup.sh` (binary in
`~/.local/bin`, `--prefix <dir>` to change it), and the web tools published on the `gh-pages` branch.

| Tool | Description |
|---|---|
| [`xstyle`](style/xstyle/README.md) | C++20 / libutils checker and fixer of my coding style (C++ first, then Python, shell, Rust and the generic rules): issues with file, line, hyperlink, rule and proposed fix, summary by severity (unforgivable / major / minor / negligible) and by rule (plus the mandatory, never counted, reminder R34: `--r34` or `34` in the code to acknowledge it), `--fix` for every fixable rule or only some codes, files or directories, libutils rules (sections, attributes, deprecated names, code libutils already gives) when libutils is installed and used, CMake and comment layout rules, `--diff` / `--staged` (changed lines only, pre-commit hook), `--commit` of the fixes, `--libutils-check`, bash / zsh / fish completion. Requires libutils, clang++ and CMake. |
| [`claude-cluster`](ai/claude-cluster/README.md) | Several Claude Code sessions in parallel (one or more per project) from one terminal (FTXUI) or window (Qt6), list / grid / tabs: per session the transcript, loaded skills, git graph and diff, tokens (last prompt, current, total), context, cost and limits, live permissions; a **global session** drives the others through its MCP server (spawn, prompts, permissions, modes, task queue, trash / restore, search, export) with its own system prompt and Remote Control; other agents too (Ollama local, Anthropic- / OpenAI-compatible endpoints, opencode, Codex) with `auth login / logout`; voice (push-to-talk, always, or a wake phrase like "ok claude"); headless commands (`--rtk`). Requires libutils, nlohmann/json, toml++ (Qt6 optional). |
| [Graph explorer](https://tsukini22.github.io/ai-utils/) | Web page (branch [`gh-pages`](https://github.com/TsukiNi22/ai-utils/tree/gh-pages), nothing to install): the project graph of [`html-doc`](docs/html-doc/SKILL.md) for any public GitHub / GitLab repository (`?repo=owner/name`), see [Graph explorer](#graph-explorer). |

## Context

The global / default context of Claude Code: what every session loads on my computers, not a skill. It lives in
[`context/`](context/README.md) and is installed by the same `setup.sh` (`install` / `remove` / `status` / `list`,
name `context`), as symlinks in `~/.claude` and `~/.local/bin`: the files it replaces are saved in
`~/.claude/backups/context/` and restored by `remove`, the hooks are merged in `~/.claude/settings.json` without
touching the other settings.

| Item | Installed as | Content |
|---|---|---|
| [`claude/CLAUDE.md`](context/claude/CLAUDE.md) | `~/.claude/CLAUDE.md` | Global instructions: git rules, language of the outputs, shell aliases, subagents cost, root access without TTY (graphical `sudo`), `xstyle --rtk` |
| [`claude/RTK.md`](context/claude/RTK.md) | `~/.claude/RTK.md` | How to read the command output condensed by [rtk](https://github.com/rtk-ai/rtk) (imported by `CLAUDE.md`) |
| [`claude/hooks/session-aliases.sh`](context/claude/hooks/session-aliases.sh) | `~/.claude/hooks/session-aliases.sh` | `SessionStart` hook: gives the zsh aliases to Claude (and the ones that lock the session) |
| [`claude/settings.hooks.json`](context/claude/settings.hooks.json) | merged in `~/.claude/settings.json` | Hooks: `SessionStart` aliases + `PreToolUse` `rtk hook claude` |
| [`bin/sudo-askpass`](context/bin/sudo-askpass) | `~/.local/bin/sudo-askpass` | Graphical password prompt (zenity) for `sudo -A` when there is no terminal |

> [!NOTE]
> The context is global: `install --project` never installs it. `--no-context` leaves it aside, `--no-rtk` /
> `--no-hooks` are given to it, `./setup.sh context <command>` runs only the context (details in
> [`context/README.md`](context/README.md)). `rtk` is installed with its official installer when it is missing.
> It is not in the plugin marketplace (a plugin can't write `~/.claude/CLAUDE.md`).

## Installation

### Quick Setup - 1 (without cloning)
Run the setup script directly: it clones (or updates) the repository into `~/.local/share/tsukini-skills`
(`SKILLS_HOME` to change it) and links the skills and the context from there, nothing to clone or clean by hand.

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install
```

or with `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install
```

Every command works the same way, arguments are given after `bash -s --`:

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install cpp-class  # only one skill (+ its requirements)
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- update             # pull the last version
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- remove --purge     # remove everything (skills, tools, context) and the clone
```

> [!NOTE]
> Requires `git` and `bash`. Each run updates the managed clone first (fast-forward only).

### Quick Setup - 2 (from a clone)

```bash
git clone git@github.com:TsukiNi22/ai-utils.git
cd ai-utils
./setup.sh install            # every skill in ~/.claude/skills (symlinks), the tools and the context
```

### Commands

```bash
./setup.sh list                           # available skills, tools and context
./setup.sh status                         # what is installed (skills, tools, context)
./setup.sh install cpp-class              # only one skill (and the skills it requires)
./setup.sh install skills                 # every skill, nothing else (no tool, no context)
./setup.sh install tools                  # every tool (xstyle, claude-cluster...)
./setup.sh install cpp/ docs/             # whole categories: every skill and tool of cpp/ and docs/ (also @cpp)
./setup.sh install skills context         # kinds and names mix: every skill + the context
./setup.sh remove tools                   # the same selections for remove
./setup.sh install xstyle                 # only one tool (built with CMake, installed in ~/.local/bin, + bash / zsh / fish completion)
./setup.sh install xstyle --prefix /usr/local
./setup.sh install context                # only the context (CLAUDE.md, RTK.md, hooks, sudo-askpass), --no-rtk / --no-hooks
./setup.sh install --no-context           # everything but the context
./setup.sh install --project ~/my/project # in <project>/.claude/skills instead
./setup.sh install --copy                 # copy instead of symlink
./setup.sh install xstyle --verbose       # the raw build output instead of the progress display (--quiet: one line per tool)
SETUP_PROGRESS=plain ./setup.sh install   # force the display: plain | bar | off (default: bar on a terminal, plain in a pipe / CI)
SETUP_JOBS=2 ./setup.sh install tools     # parallel jobs of the builds (default: the processors, lowered under ~1.5 GB free per job)
./setup.sh update                         # git pull of the repository (+ rebuild of the installed tools)
./setup.sh remove                         # remove every skill, tool and the context of this repo
./setup.sh remove cpp-class               # remove only one (`remove context` restores the replaced files)
./setup.sh remove                         # a tool: its binary (and completion) only; --purge also deletes the build trees / logs (tmp, cache) of the tools
./setup.sh remove --purge                 # (curl/wget mode) also delete the managed clone
./setup.sh hook                           # xstyle pre-commit hook in the git repository of the current directory
./setup.sh hook --fail-on minor --shared  # options (git/pre-commit/scripts/install_hook.sh --help), hook status | remove
```

By default the skills are **symlinked**: an update (or a local edit) is used right away,
no need to reinstall. `remove` only deletes skills that come from this repository
(use `--force` otherwise).

The tools show their build progress (steps configure / build / install / completion + post-install, percentage, bar,
current file, elapsed time and ETA parsed from the build output, which goes to `~/.cache/tsukini-skills/build/<tool>.log`):
one updating line on a terminal (`NO_COLOR` is honoured), a line per 10% and per step in a pipe or CI. A failed build
shows its step and the last 20 lines of the log, and the tool is skipped.

## Plugin marketplace (Claude Code)

The repository is also a Claude Code plugin marketplace ([`.claude-plugin/marketplace.json`](.claude-plugin/marketplace.json)),
handy on another machine or in a Claude Code on the web session (no `setup.sh` needed):

```text
/plugin marketplace add TsukiNi22/ai-utils
/plugin install cpp-skills@tsukini-skills
/plugin install dev@tsukini-skills
```

| Plugin | Skills |
|---|---|
| `cpp-skills` | cpp-project, cpp-class, cpp-style, cpp-comments, cmake-style |
| `libutils-skills` | libutils, libutils-exception, libutils-install, libutils-setup |
| `doc-skills` | readme-style, wiki-style, html-doc, html-style, explain-doc, protocol-doc, report, docs-update |
| `git-skills` | git-conventions, git-commit, ci-cd, pre-commit |
| `ai-skills` | interactive-questions, clarify, ideation, export, onboard, rtk |
| `style-skills` | coding-style, comments |
| `python-skills` | python-project, python-class, python-style, python-comments |
| `test-skills` | tests, cpp-tests, coverage |
| `audit-skills` | audit-bugs, audit-security, audit-quality, audit-deps, benchmark |
| `legal-skills` | license, legal-check, legal-setup |
| `ux-skills` | multilingual-project, multilingual-website, color-modes-project, color-modes-website |
| `system-skills` | computer-clean |
| `dev`, `cpp`, `python`, `setup-project`, `doc`, `git`, `style`, `legal`, `audit` | the routers (`/dev`, `/cpp`...) |

The skills of a plugin are named `<plugin>:<skill>` (e.g. `cpp-skills:cpp-class`); each plugin carries the whole
repository, so a skill finds the scripts of the others. In a cloud session the marketplace has to be added again in
each session; `/plugin marketplace update tsukini-skills` gets the new versions.

## claude.ai

The same skills in the claude.ai chat (web, desktop, mobile): `python3 claude-ai/build.py` builds one archive per
skill to upload in **Customize > Skills**, and [`claude-ai/preferences.md`](claude-ai/preferences.md) is the
`CLAUDE.md` adapted to the chat, to paste in the personal preferences. Details: [`claude-ai/README.md`](claude-ai/README.md).

## Usage

Once installed, the skill is used automatically when relevant
(ex: *"create a `Timer` class in `utils::system`"*, *"write the commit message"*),
or explicitly with `/<skill>` (`/cpp-class`, `/git-conventions`...).

A skill can require other ones (`<skill>/requires.txt`): `./setup.sh install <skill>` installs them too.

## Layout

```
routers/   dev, cpp, python, setup-project, git, doc, style, legal, audit   (manual /commands that load the skills below)
cpp/       cpp-project, cpp-class, cpp-style, cpp-comments, cmake-style
python/    python-project, python-class, python-style, python-comments
libutils/  libutils, libutils-exception, libutils-install, libutils-setup
tests/     tests, cpp-tests, coverage
style/     coding-style, comments, xstyle (tool)
docs/      readme-style, wiki-style, html-doc, html-style, explain-doc, protocol-doc, report, docs-update
git/       git-conventions, git-commit, ci-cd, pre-commit
audit/     audit-bugs, audit-security, audit-quality, audit-deps, benchmark
legal/     license, legal-check, legal-setup
ux/        multilingual-project, multilingual-website, color-modes-project, color-modes-website
system/    computer-clean
ai/        interactive-questions, clarify, ideation, export, onboard, rtk, claude-cluster (tool)
context/   global context of Claude Code (CLAUDE.md, RTK.md, hooks, sudo-askpass), installed by setup.sh
```

Every skill is `<category>/<skill>/` and is installed flat as `~/.claude/skills/<skill>` (the category is only for
the repository):

```
<category>/<skill>/
├── SKILL.md       # instructions loaded by the assistant
├── requires.txt   # other skills needed (optional, installed with it)
├── reference/     # detailed rules, read on demand
├── templates/     # file templates
├── examples/      # real files used as ground truth
└── scripts/       # helpers called by the skill
```

## Credits

`cpp-class/scripts/fonts/ansi_shadow.flf`: ANSI Shadow FIGlet font, as distributed with
[pyfiglet](https://github.com/pwaller/pyfiglet) (MIT).
