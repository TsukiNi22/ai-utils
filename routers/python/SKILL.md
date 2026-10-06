---
name: python
description: Router for every Python task of the user - looks at the request and the project, then loads the right Python skills (python-project, python-class, python-style, python-comments, tests, ci-cd, benchmark). Invoke manually with /python.
disable-model-invocation: true
---

# Python router

`<name>` (a skill) = the folder of that skill: `~/.claude/skills/<name>` (setup.sh) or `${CLAUDE_SKILL_DIR}/../../*/<name>` (plugin of the marketplace).

**Request given with `/python`:** $ARGUMENTS

Do not answer from this file: decide which skills apply, **load them with the Skill tool** (never the other routers: they
are manual-only), then do the task following them. The request is the text given with `/python` (or, if empty, the
latest request of the user; if there is none, inspect the project and propose what can be done).

## 1. Look at the context (quickly, in parallel)
- `pyproject.toml` (package), `src/main.py` + `src/app.py` (MAGIC application), a lone script, `requirements.txt`,
  `tests/`, `.venv/`. Empty folder = new project.
- GitHub repository? `git remote -v` (`github.com` -> CI/CD through `ci-cd`).
- Files the request is about (open them before deciding).

## 2. Pick the skills (several can apply, load all of them before writing)
| Situation | Skills |
|---|---|
| new project / package / tool / application / "setup" | `python-project` (it loads the others it needs, `ci-cd` on GitHub) |
| new class, abstract class, tool function, module, exception, constants, test file | `python-class` (+ `python-style`, `python-comments`) |
| write / edit / refactor / fix Python code | `python-style` + `python-comments` |
| comments / docstrings only | `python-comments` |
| review Python code against the user's style | `python-style` + `python-comments` (`xstyle --rtk -c PY,G` first) |
| unit tests (pytest): setup, write, run | `tests` |
| slow code, profiling, benchmark | `benchmark` (py-spy / cProfile part) |
| CI / CD (tests, style, PyPI, release) | `ci-cd` |
| commit / docs asked at the end | `git-conventions` / `readme-style` |

Rules: `python-class` never writes function logic; the local style of an existing file wins.

## 3. Say it
Start the answer with one line: which skills were loaded and why (ex: "Skills : python-class, python-style,
python-comments (nouvelle classe dans une application MAGIC)").
