---
name: dev
description: Global development router - invoked manually with /dev, it inspects the request, the project and the languages involved, then loads the matching skills of the user (using the tables of the cpp, git, doc, coding-style and legal routers: cpp-project, cpp-class, cpp-style, cpp-comments, comments, cmake-style, cpp-tests, tests, audit-bugs, audit-quality, libutils, libutils-exception, libutils-install, libutils-setup, git-conventions, readme-style, html-doc, pdf-report, license, deps-license) before doing the task.
disable-model-invocation: true
---

# Development router

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
The category routers (`cpp`, `git`, `doc`, `coding-style`, `legal`) are manual-only: don't call them with the Skill tool.
**Read** their decision table when the case is not obvious (`~/.claude/skills/<router>/SKILL.md`), then load the
**specific skills** with the Skill tool:

| Need | Load (Skill tool) | Detailed table |
|---|---|---|
| new C++ project | `cpp-project` | `cpp` |
| C++ code, classes, main, architecture | `cpp-class`, `cpp-style`, `cpp-comments` | `cpp` |
| CMake, tests CMake, packaging | `cmake-style` | `cpp` |
| libutils APIs / exceptions | `libutils`, `libutils-exception` | `cpp` |
| install / update libutils on the computer, add it to the project | `libutils-install`, `libutils-setup` | `cpp` |
| unit tests (C++: GoogleTest like libutils) | `cpp-tests` (C++) or `tests` (any language, asks the framework) | `cpp` |
| bugs / UB / crashes / sanitizers audit | `audit-bugs` | `cpp` |
| cleanliness / conventions audit with a PDF report | `audit-quality` | `coding-style` |
| comments of any language, file headers | `comments` (+ `cpp-comments`) | `doc` |
| a deliverable as PDF (report, study, benchmark) | `pdf-report` | `doc` |
| license (choose, add, replace, compatibility) | `license` | `legal` |
| dependencies: licenses to credit / restrictive / paid, vulnerabilities | `deps-license` | `legal` |
| style only (format / review like the user) | `cpp-style`, `cpp-comments`, `cmake-style` by file type | `coding-style` |
| README, wiki, Markdown docs | `readme-style` | `doc` |
| HTML docs, project graph | `html-doc` | `doc` |
| commit, branch, PR, tag, release, CHANGELOG | `git-conventions` | `git` |
| several needs (ex: "add the feature, document it and prepare the commit") | every matching skill, in the order of the work: code -> docs -> git | - |
| no skill matches (other language/tool) | none: say it, follow the conventions of the project | - |

## 4. Work
- Every audit, review, analysis or summary delivered to the user (including the "no request" state of the project)
  is a **PDF** through `pdf-report` (Markdown kept next to it), the chat only gives the verdict and the paths.
- Follow the loaded skills strictly; they override generic habits.
- Never commit/push/tag/release/open a PR without an explicit request (`git-conventions`).
- Start the answer with one line: `Skills : <list> (<reason>)`, then do the task.
