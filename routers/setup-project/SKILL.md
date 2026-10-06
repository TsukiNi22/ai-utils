---
name: setup-project
description: Router to set up a whole new project (or complete a young one) in Tsukini's way - detects or asks the language, then chains the skills - the architecture of the language (cpp-project, python-project, else coding-style + the tests layout), the base unit tests (at least one test that compiles and runs), the license (license, asked when not given), the git setup (git-conventions - init, .gitignore, CHANGELOG, branches - and the xstyle pre-commit hook), the CI/CD (ci-cd, asked) on GitHub, the README - asking every choice once up front, then ends with a very simplified audit of the architecture that was set up. Invoke manually with /setup-project.
disable-model-invocation: true
---

# Project setup router

`<name>` (a skill) = the folder of that skill: `~/.claude/skills/<name>` (setup.sh) or `${CLAUDE_SKILL_DIR}/../../*/<name>` (plugin of the marketplace).

**Request given with `/setup-project`:** $ARGUMENTS

Do not answer from this file: it decides the order and the questions, the work is done by the skills, **loaded with the
Skill tool when their step comes** (never the other routers: they are manual-only). The request is the text given with
`/setup-project` (or, if empty, the latest request of the user).

## 1. Look at the context (quickly, in parallel)
- Target folder: empty / missing (new project) or existing (complete what is missing, never overwrite: list what is
  already there - build files, `src/`, `tests/`, `LICENSE`, `.git`, `.github/workflows/`, `README.md`, hooks).
- Language: from the request, else the files (`CMakeLists.txt` / `*.cpp` -> C++, `pyproject.toml` / `*.py` -> Python,
  `Cargo.toml` -> Rust, `package.json` -> JS/TS...), else asked.
- Git: `git rev-parse --show-toplevel`, `git remote -v` (`github.com` -> GitHub), `gh auth status` (for a new GitHub
  repository).
- Tools: `command -v xstyle cmake clang++ python3 gh`.

## 2. Ask everything once (AskUserQuestion, French, recommended first)
Only what the request and the context don't already say; up to 4 questions per call, two calls at most here. The
answers are **passed to the skills**: their own questions already answered are skipped (say "déjà répondu" mentally,
never ask twice).
- **Langage / type** (when unknown): `C++ (cpp-project)` · `Python (python-project)` · `Autre (préciser)`; then the
  type question of the project skill (binary / packages / library / header-only, package / application / script).
- **Licence** (when not given in the request): the first question of `license` (needs: commercial use, copyleft,
  attribution...), or `Pas de licence (tous droits réservés)`; a license named in the request is used directly.
- **Git** (multiSelect): `git init + .gitignore + CHANGELOG (Recommandé)` · `Dépôt GitHub (gh repo create)` ·
  `Hook pre-commit xstyle (style vérifié en local à chaque commit)` · `Premier commit`.
- **CI/CD** (GitHub only): `Oui, choisir les jobs (ci-cd)` · `Build seulement` · `Aucune`; the job checklist itself is
  asked by `ci-cd` (build, tests, xstyle, coverage, release, packages...).
- **Extras** (multiSelect): `README (readme-style)` · `Doc HTML` · `libutils (C++)`.
The pre-commit options (fail / warn levels, scope...) are asked by `pre-commit` when it is chosen.

## 3. Run the steps in this order (each skill loaded at its step, its rules followed)
| # | Step | Skill(s) | Done when |
|---|---|---|---|
| 1 | architecture of the language | C++ `cpp-project` (+ `cmake-style`, `cpp-class`, `libutils-setup`); Python `python-project` (+ `python-class`); other: `coding-style` (layout of its section for the language: `src/`, entry point, build file) | the project builds / runs (empty logic) |
| 2 | base unit tests | C++ `cpp-tests` (`tests/` + `BUILD_TESTS`, one test of the core class); Python / other `tests` (pytest...) | **at least one test compiles and passes** |
| 3 | license | `license` (`LICENSE` file, SPDX id in the build file / `pyproject.toml`, headers if the license asks it) | `LICENSE` present (or "no license" said) |
| 4 | git | `git-conventions` (`git init -b main`, `.gitignore` of the build outputs, `CHANGELOG.md`, branches solo / team), `pre-commit` if chosen | `git status` clean of build outputs |
| 5 | CI/CD | `ci-cd` (GitHub only; it asks the jobs) | workflows valid (its `check_workflows.py`) |
| 6 | README | `readme-style` | README with setup / usage |
| 7 | first commit / GitHub | `git-conventions` (only if chosen: never commit / push / create a repository otherwise) | |
A step already done by the project skill (ex: `cpp-project` writes the tests, the README or the workflows when they are
chosen) is not repeated: check it and complete it.

## 4. Simplified architecture audit (always, at the end, in the answer - no report file)
Fast checks of what was set up, one line each, `OK` / `KO` + the fix (fix the `KO` that belong to this setup):
| Check | How |
|---|---|
| layout of the project skill (folders, entry point, one class per file, names) | `ls -R` limited to 3 levels, compared to the skill |
| builds from clean | C++ `cmake -S . -B /tmp/<name>-build && cmake --build ...`; Python `pip install -e .` in a venv or `python3 src/main.py --help` |
| tests compile and pass | `ctest` / `pytest -q` |
| style | `xstyle --rtk -S -r .` (installed only): no unforgivable / major |
| build outputs ignored, no source ignored | `git status --ignored --short` after the build |
| no placeholder left | `grep -rn '{{' . --exclude-dir=.git` and the template names (`template`, `Core`) |
| license / README / CHANGELOG present and consistent (name, version, license id) | read them |
| CI files valid | `ci-cd/scripts/check_workflows.py` |
End with the tree (2 levels), the chosen options, what the user still has to do (secrets, `RUNNER` variable, push,
`pre-commit` demo) and propose `/audit` (`audit-quality`) for a full audit later.

## Say it
Start the answer with one line: the language detected and the skills that will be loaded in order (ex: "Projet C++ :
cpp-project, cpp-tests, license, git-conventions, pre-commit, ci-cd, readme-style").
