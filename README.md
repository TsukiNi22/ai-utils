# skills

AI skills based on my work.

This repository holds [Claude Code skills](https://docs.claude.com/en/docs/claude-code/skills)
built from my own projects and habits. Each skill teaches the assistant how I work (code style,
file layout, tooling), so that what it generates looks like something I wrote myself.

## Routers
Skills invoked **manually** that look at the request, the project and the language, then load the right
skills below by themselves. `/dev` is enough most of the time.

| Router | Loads |
|---|---|
| [`/dev`](routers/dev/SKILL.md) | everything below, depending on the context (global development entry point) |
| [`/cpp`](routers/cpp/SKILL.md) | `cpp-project`, `cpp-class`, `cpp-style`, `cpp-comments`, `cmake-style`, `cpp-tests`, `audit-bugs`, `audit-quality`, `libutils`, `libutils-exception`, `libutils-install`, `libutils-setup` |
| [`/git`](routers/git/SKILL.md) | `git-conventions`, `ci-workflows` |
| [`/doc`](routers/doc/SKILL.md) | `readme-style`, `html-doc`, `html-style`, `explain-doc`, `report`, `git-conventions` (CHANGELOG), `comments`, `cpp-comments` |
| [`/style`](routers/style/SKILL.md) | `cpp-style`, `python-style`, `coding-style`, `cpp-comments`, `comments`, `cmake-style` (by file type), `audit-quality` |
| [`/legal`](routers/legal/SKILL.md) | `license`, `audit-deps` (and the future legal skills) |
| [`/audit`](routers/audit/SKILL.md) | asks which audits to run (checklist: `audit-bugs`, `audit-quality`, `audit-deps`, `coverage`, `benchmark`) and the report format, runs them and writes a summary report in `audit/` |


## Graph explorer

**https://tsukini22.github.io/skills/** (branch [`gh-pages`](https://github.com/TsukiNi22/skills/tree/gh-pages)) - the
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
| [`html-doc`](docs/html-doc/SKILL.md) | One uniform style for the HTML documentation: an optional user guide, the technical documentation and an interactive 2D/3D project graph of any common stack (C / C++, Python, JS / TS / web, Java, C#, Go, Rust, PHP, Ruby) with execution / inheritance / flame diagrams (filters by category/group/relation, rebuilt from the GitHub repository with an update button), all static self-contained pages with a sun/moon theme button. |
| [`html-style`](docs/html-style/SKILL.md) | My visual style for any HTML page (from `html-doc`): light / dark color tokens, system fonts and type scale, layout, components, SVG diagram classes, sun/moon and EN/FR switches, rules (self-contained, WCAG contrast); `scripts/new_page.py` builds a self-contained page, `scripts/check_style.py` checks a page or a palette (tokens, contrast, unreadable inherited colors, external resources). |
| [`explain-doc`](docs/explain-doc/SKILL.md) | Explanation pages (how something works) in two levels switched in place, **Simple** (analogies, plain words, numbers) and **Technical** (terms, formulas, complexity, edge cases, code, sources), with SVG diagrams, step-by-step animations, MathML formulas, interactive playgrounds / simulations checking the formulas, quizzes and a glossary. Built on `html-style`. |
| [`cpp-project`](cpp/cpp-project/SKILL.md) | Sets up a new C++ project from [cpp_project_template](https://github.com/TsukiNi22/cpp_project_template): renaming, binary / packaged binary / `.a` / `.so` / header-only, libutils or not, exceptions, GTest, CI/CD workflows (build check or full packages + gh-pages mirror), install script, README/CHANGELOG/docs, using the other skills. |
| [`cmake-style`](cpp/cmake-style/SKILL.md) | `CMakeLists.txt` in my style: section order, explicit sources, Debug/Asan/Optimized modes, release targets, GTest tests, install + `find_package` config, CPack RPM/DEB stable/pre channels, with app/lib/tests templates. |
| [`cpp-tests`](tests/cpp-tests/SKILL.md) | C++ unit tests like libutils: GoogleTest setup (CMake, CI, `unit_tests`), one test file per module, isolation of the blocking cases, known bugs kept as failing tests, `scripts/untested.py` to list what is never tested. |
| [`tests`](tests/tests/SKILL.md) | Unit tests for any language: detects the existing framework or asks which one to use, then setup, tests, CI (defers to `cpp-tests` for C++). |
| [`comments`](style/comments/SKILL.md) | My comment style for every language (C, C++, CMake, Makefile, shell, Python, Lua, YAML, Markdown) and which file header goes where. |
| [`audit-bugs`](audit/audit-bugs/SKILL.md) | Bug / UB audit loop (sanitizers, tests, static tools, code review, verification of each finding), single agent or subagents on request, fixes only after confirmation. |
| [`audit-quality`](audit/audit-quality/SKILL.md) | Cleanliness / conventions audit of a project (`scripts/collect.py` metrics + review against the style skills) delivered as a PDF report. |
| [`report`](docs/report/SKILL.md) | Reports in my style as Markdown and/or PDF (format asked: both recommended), English by default (Markdown -> WeasyPrint, A4, Noto, navy title rule, tables and callouts) with `scripts/md2pdf.py`. |
| [`license`](legal/license/SKILL.md) | Finds the license matching the needs (comparison matrix), negotiates the close ones, fetches the official text, replaces the current LICENSE only after confirmation, checks dependencies compatibility. |
| [`audit-deps`](audit/audit-deps/SKILL.md) | Dependencies (direct + transitive): licenses to credit, copyleft / non-commercial / paid / unknown ones, THIRD_PARTY_NOTICES generation, and known vulnerabilities / compromised versions (OSV, dnf, GitHub advisories), as a PDF report. |
| [`python-style`](python/python-style/SKILL.md) | My Python style, measured on my scripts (libutils `cmake/scripts`, MAGIC): header, `##### sections #####`, checked imports with a `# Used for` each, `__main__` guards, `const.py` with frozen dataclasses, exit codes, errors on stderr, type hints; checked by the `PY-*` rules of `xstyle`. |
| [`python-project`](python/python-project/SKILL.md) | New Python project: script, application in the MAGIC layout (`main.py` / `app.py` / `const.py`, `Class/`, `tool/`, `data/`) or installable package (src layout, single source version, `main()` returning the exit code, console command, pytest), generated by `scripts/new_project.py`. |
| [`ci-workflows`](git/ci-workflows/SKILL.md) | GitHub CI/CD workflows: always asks what to run (build, tests, xstyle style check, coverage, dispatch, packages, release, docs), my Docker images first (`ghcr.io/tsukini22/*`) or the smallest official one, my conventions, a missing tool added to the docker-image repository with a local commit. |
| [`benchmark`](audit/benchmark/SKILL.md) | Profiling of the current program (perf stat / record, hyperfine, callgrind, heaptrack, flame graph; missing tools installed only after a yes): hot functions, why they are slow, quick wins vs complex changes with their gain, PDF report, optional before / after. |
| [`coverage`](tests/coverage/SKILL.md) | Test coverage of a `cpp-tests` project (clang / llvm-cov in a copy): per file, functions never called, ranges never run; then tests written module by module until it stops growing, and always the question of a report on what is still missing. |
| [`libutils`](libutils/libutils/SKILL.md) | Reference of my library [libutils](https://github.com/TsukiNi22/libutils): every section, header and public API generated from a recorded version/commit (`reference/VERSION.md`), integration and conventions, plus `scripts/update.sh` to see what changed since that commit and regenerate. |
| [`libutils-exception`](libutils/libutils-exception/SKILL.md) | libutils exceptions: classes, codes, throw/catch patterns, and how to add new exception codes to a project (JSON, generator script, CMake block). Requires `libutils`. |
| [`libutils-install`](libutils/libutils-install/SKILL.md) | Installs / updates / removes libutils on the computer by itself from the OS (dnf, apt, or build from the sources), last version through `libutils-pre`, debug/asan variants, mirror setup, `/usr/local` shadowing check. |
| [`libutils-setup`](libutils/libutils-setup/SKILL.md) | Adds libutils to the current project: `find_package`/`utils::utils` in the CMake and optionally the custom exceptions (JSON, scripts, generation block), installing libutils first if needed. |

Everything is based on my own work, mainly [libutils](https://github.com/TsukiNi22/libutils).

## Tools

Programs stored next to the skills they enforce, built and installed by the same `setup.sh` (binary in
`~/.local/bin`, `--prefix <dir>` to change it).

| Tool | Description |
|---|---|
| [`xstyle`](style/xstyle/README.md) | C++20 / libutils checker and fixer of my coding style (C++ first, then Python, shell, Rust and the generic rules): issues with file, line, hyperlink, rule and proposed fix, summary by severity (unforgivable / major / minor / negligible) and by rule, `--fix` for every fixable rule or only some codes, files or directories, libutils rules (sections, attributes, deprecated names, code libutils already gives) when libutils is installed and used, CMake and comment layout rules, `--diff` / `--staged` (changed lines only, pre-commit hook), `--commit` of the fixes, `--libutils-check`, bash / zsh / fish completion. Requires libutils, clang++ and CMake. |

## Installation

### Quick Setup - 1 (without cloning)
Run the setup script directly: it clones (or updates) the repository into `~/.local/share/tsukini-skills`
(`SKILLS_HOME` to change it) and links the skills from there, nothing to clone or clean by hand.

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- install
```

or with `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- install
```

Every command works the same way, arguments are given after `bash -s --`:

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- install cpp-class  # only one skill (+ its requirements)
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- update             # pull the last version
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- remove --purge     # remove every skill and the clone
```

> [!NOTE]
> Requires `git` and `bash`. Each run updates the managed clone first (fast-forward only).

### Quick Setup - 2 (from a clone)

```bash
git clone git@github.com:TsukiNi22/skills.git
cd skills
./setup.sh install            # every skill, in ~/.claude/skills (symlinks)
```

### Commands

```bash
./setup.sh list                           # available skills and tools
./setup.sh status                         # what is installed
./setup.sh install cpp-class              # only one skill (and the skills it requires)
./setup.sh install xstyle                 # only one tool (built with CMake, installed in ~/.local/bin, + bash / zsh / fish completion)
./setup.sh install xstyle --prefix /usr/local
./setup.sh install --project ~/my/project # in <project>/.claude/skills instead
./setup.sh install --copy                 # copy instead of symlink
./setup.sh update                         # git pull of the repository (+ rebuild of the installed tools)
./setup.sh remove                         # remove every skill of this repo
./setup.sh remove cpp-class               # remove only one
./setup.sh remove --purge                 # (curl/wget mode) also delete the managed clone
```

By default the skills are **symlinked**: an update (or a local edit) is used right away,
no need to reinstall. `remove` only deletes skills that come from this repository
(use `--force` otherwise).

## Plugin marketplace (Claude Code)

The repository is also a Claude Code plugin marketplace ([`.claude-plugin/marketplace.json`](.claude-plugin/marketplace.json)),
handy on another machine or in a Claude Code on the web session (no `setup.sh` needed):

```text
/plugin marketplace add TsukiNi22/skills
/plugin install cpp-skills@tsukini-skills
/plugin install dev@tsukini-skills
```

| Plugin | Skills |
|---|---|
| `cpp-skills` | cpp-project, cpp-class, cpp-style, cpp-comments, cmake-style |
| `libutils-skills` | libutils, libutils-exception, libutils-install, libutils-setup |
| `doc-skills` | readme-style, html-doc, html-style, explain-doc, report |
| `git-skills` | git-conventions, ci-workflows |
| `style-skills` | coding-style, comments |
| `python-skills` | python-style, python-project |
| `test-skills` | tests, cpp-tests, coverage |
| `audit-skills` | audit-bugs, audit-quality, audit-deps, benchmark |
| `legal-skills` | license |
| `dev`, `cpp`, `doc`, `git`, `style`, `legal`, `audit` | the routers (`/dev`, `/cpp`...) |

The skills of a plugin are named `<plugin>:<skill>` (e.g. `cpp-skills:cpp-class`); each plugin carries the whole
repository, so a skill finds the scripts of the others. In a cloud session the marketplace has to be added again in
each session; `/plugin marketplace update tsukini-skills` gets the new versions.

## claude.ai

The same skills in the claude.ai chat (web, desktop, mobile): `python3 claude-ai/build.py` builds one archive per
skill to upload in **Customize > Skills**, and [`claude-ai/preferences.md`](claude-ai/preferences.md) is the
`CLAUDE.md` adapted to the chat, to paste in the personal preferences. Details: [`claude-ai/README.md`](claude-ai/README.md).

## Global context (`context` branch)
The global / default context of Claude Code (`CLAUDE.md`, `RTK.md`, session hooks, rtk, `sudo-askpass`) is not a
skill: it lives on the [`context`](https://github.com/TsukiNi22/skills/tree/context) branch with its own installer.

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/context/setup.sh | bash -s -- install
./setup.sh context install        # same, from this branch (status | update | remove [--purge] too)
```

## Usage

Once installed, the skill is used automatically when relevant
(ex: *"create a `Timer` class in `utils::system`"*, *"write the commit message"*),
or explicitly with `/<skill>` (`/cpp-class`, `/git-conventions`...).

A skill can require other ones (`<skill>/requires.txt`): `./setup.sh install <skill>` installs them too.

## Layout

```
routers/   dev, cpp, git, doc, style, legal, audit   (manual /commands that load the skills below)
cpp/       cpp-project, cpp-class, cpp-style, cpp-comments, cmake-style
libutils/  libutils, libutils-exception, libutils-install, libutils-setup
tests/     tests, cpp-tests
style/     coding-style, comments
docs/      readme-style, html-doc, html-style, explain-doc, report
git/       git-conventions
audit/     audit-bugs, audit-quality, audit-deps
legal/     license
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
