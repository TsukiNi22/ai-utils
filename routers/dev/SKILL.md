---
name: dev
description: Global development router - invoked manually with /dev, it inspects the request, the project and the languages involved, then loads the matching skills of the user (using the tables of the cpp, python, git, doc, style, audit and legal routers: cpp-project, cpp-class, python-project, python-class, python-style, python-comments, ci-cd, pre-commit, rtk, interactive-questions, clarify, coverage, benchmark, cpp-style, coding-style, cpp-comments, comments, cmake-style, cpp-tests, tests, audit-bugs, audit-quality, libutils, libutils-exception, libutils-install, libutils-setup, git-conventions, readme-style, wiki-style, html-doc, html-style, explain-doc, report, license, audit-deps) before doing the task.
disable-model-invocation: true
---

# Development router

`<name>` (a skill) = the folder of that skill: `~/.claude/skills/<name>` (setup.sh) or `${CLAUDE_SKILL_DIR}/../../*/<name>` (plugin of the marketplace).

**Request given with `/dev`:** $ARGUMENTS

Single entry point: the user only invokes `/dev` (with or without a request) and Claude decides which skills
to use. Do not answer from this file: **load the skills with the Skill tool first**, then work.

## 1. Understand the request
The text given with `/dev`, otherwise the latest request of the user. With nothing at all: inspect the project
(step 2) and propose the next useful actions (uncommitted changes to commit, missing tests/docs, outdated
README/CHANGELOG, new libutils version...), without changing anything.

## 2. Inspect the context (in parallel, quick)
- `git status --short`, `git log --oneline -5`, `git remote -v`.
- Languages / build: `CMakeLists.txt`, `*.cpp/*.hpp` (C++), `package.json`, `pyproject.toml`, `Cargo.toml`...
- libutils: `find_package(utils` in the CMake or `utils::` in the sources.
- Docs: `README.md`, `CHANGELOG.md`, `docs/`.

## 3. Route
The category routers (`cpp`, `python`, `setup-project`, `git`, `doc`, `style`, `legal`, `audit`) are manual-only: don't call them with the Skill tool.
**Read** their decision table when the case is not obvious (`<router>/SKILL.md`), then load the
**specific skills** with the Skill tool:

| Need | Load (Skill tool) | Detailed table |
|---|---|---|
| new C++ project | `cpp-project` | `cpp` |
| C++ code, classes, main, architecture | `cpp-class`, `cpp-style`, `cpp-comments` | `cpp` |
| CMake, tests CMake, packaging | `cmake-style` | `cpp` |
| libutils APIs / exceptions | `libutils`, `libutils-exception` | `cpp` |
| install / update libutils on the computer, add it to the project | `libutils-install`, `libutils-setup` | `cpp` |
| unit tests (C++: GoogleTest like libutils) | `cpp-tests` (C++) or `tests` (any language, asks the framework) | `cpp` |
| test coverage: measure, what is not tested, fill it | `coverage` (C++, cpp-tests projects) | `audit` |
| performance: benchmark, profiling, what is slow, optimize | `benchmark` | `audit` |
| CI / CD, GitHub workflows, actions, Docker images of the CI | `ci-cd` | `git` |
| pre-commit hook (xstyle) | `pre-commit` | `git` |
| compact a text / output for an AI, save tokens, add an `--rtk` mode to a program | `rtk` | - |
| choices / ideas / questions to ask the user ("propose en qcm", "demande-moi", several approaches) | `interactive-questions` | - |
| request unclear / ambiguous / incomplete, "clarifie", "vérifie que tu as compris", ask before acting | `clarify` (uses `interactive-questions`) | - |
| ideas, recommendations, an opinion on ideas, variants ("des idées ?", "ton avis", "améliore cette idée", brainstorm) | `ideation` (uses `interactive-questions`) | - |
| set up a whole new project (architecture + tests + license + git + CI/CD + README) | the steps of `setup-project` (its table, skills loaded in that order) | `setup-project` |
| several / all audits of the project at once | follow the `audit` router (checklist of audits + format) | `audit` |
| bugs / UB / crashes / sanitizers audit | `audit-bugs` | `cpp` |
| cleanliness / conventions audit with a PDF report | `audit-quality` | `style` |
| comments of any language, file headers | `comments` (+ `cpp-comments`) | `doc` |
| a deliverable as PDF (report, study, benchmark) | `report` | `doc` |
| license (choose, add, replace, compatibility) | `license` | `legal` |
| dependencies: licenses to credit / restrictive / paid, vulnerabilities | `audit-deps` | `legal` |
| legal compliance check: GDPR / RGPD, cookies, mentions légales, CGU, RGAA, privacy, erasure, AI Act, licenses and mentions | `legal-check` | `legal` |
| make it compliant: privacy policy, legal notice, cookie banner, terms, accessibility statement, data deletion / export | `legal-setup` | `legal` |
| several languages / translate a project (`i18n`) or a website (`i18n-web` + `i18n`) | `i18n`, `i18n-web` | - |
| dark / light / system modes, themes, high contrast: a project (`theming`) or a website (`theming-web` + `theming`) | `theming`, `theming-web` | - |
| style only (format / review like the user) | `cpp-style`, `cpp-comments`, `coding-style` (other languages), `cmake-style`, `comments` by file type | `style` |
| Python code | `python-style` + `python-comments` | `python` |
| new Python class / module / tool / exception / test file | `python-class` | `python` |
| new Python project / package / tool | `python-project` | `python` |
| code in another language than C++ (shell, Lua, JS/TS, Rust, C...) | `coding-style` + `comments` | `style` |
| README, Markdown docs | `readme-style` | `doc` |
| GitHub wiki | `wiki-style` | `doc` |
| HTML docs, project graph | `html-doc` | `doc` |
| explanation page (how something works), simple + technical | `explain-doc` | `doc` |
| protocol of a networking project (frames, handshake, codes, commands, storage) | `protocol-doc` | `doc` |
| any other HTML page, palette / color / font | `html-style` | `doc` |
| free space: caches, Docker, old builds, Claude Code / claude-cluster leftovers | `computer-clean` | - |
| commit the current diff locally ("fais le commit") | `git-commit` | `git` |
| update every doc / site / wiki / package / install script after changes or a new version | `docs-update` | `doc` |
| commit message, branch, PR, tag, release, CHANGELOG | `git-conventions` | `git` |
| several needs (ex: "add the feature, document it and prepare the commit") | every matching skill, in the order of the work: code -> docs -> git | - |
| no skill matches (other language/tool) | none: say it, follow the conventions of the project | - |

## 4. Work
- Every audit, review, analysis or summary delivered to the user (including the "no request" state of the project)
  goes through `report` (format asked once: Markdown + PDF recommended / PDF / Markdown), English by
  default, in `audit/` at the repository root unless asked; the chat only gives the verdict and the paths.
- Follow the loaded skills strictly; they override generic habits.
- Never commit/push/tag/release/open a PR without an explicit request (`git-conventions`).
- Start the answer with one line: `Skills : <list> (<reason>)`, then do the task.
