---
name: libutils-setup
description: Set up libutils in the current C++ project - find_package(utils <installed version>), utils::utils on every target, and optionally the custom exception codes (JSON config, generator scripts, CMake generation block, include/exception, .gitignore) - in the user's CMake layout, installing libutils first when it is missing. Use whenever the user wants to add/use libutils in an existing project, link it in the CMake, or set up custom libutils exceptions in a repository.
---

# libutils in the current project

`SKILL_DIR` = the directory of this file.

## 1. Is libutils installed?
```bash
bash ~/.claude/skills/libutils-install/scripts/libutils.sh status
```
Not installed (or older than needed) -> **libutils-install** skill (`libutils.sh install`, last version through
`libutils-pre`; root needed: give the user the command to run with `! `), then come back.
A `/usr/local` source install shadowing the packages is reported: tell the user (it decides which version compiles).

## 2. Set it up
```bash
python3 SKILL_DIR/scripts/setup_project.py <project root> [--exceptions] [--targets a,b] [--version X.Y.Z] [--dry-run]
```
Run it with `--dry-run` first when the CMake isn't in the user's layout, show the diff, then for real.
Idempotent (a second run changes nothing). In the `cmake-style` sections (created when missing):
- `# Requirement`: `find_package(utils <installed version> CONFIG REQUIRED) # Check utils dependencies existance`
  (+ `find_package(Python3 REQUIRED)` with `--exceptions`), before the ccache block;
- `# Special Targets` (`--exceptions`): the generation of `include/exception/generated_external_exception_header.hpp`
  from `cmake/config/exceptions/*.json`;
- `# Dependencies`: `utils::utils` appended to the existing `target_link_libraries(<target> PRIVATE ...)` or a new line,
  and with `--exceptions` `target_include_directories(<target> PRIVATE include include/exception)` +
  `add_dependencies(<target> generated_external_exception_header)`, for every `add_executable`/`add_library`
  (or `--targets`; add `unit_tests` in `tests/CMakeLists.txt` by hand the same way);
- files (`--exceptions`, only if missing): `cmake/scripts/{const.py, generate_exception_header.py, requirements.txt}`
  and an example `cmake/config/exceptions/global.json` (from the `libutils-exception` templates), `include/exception/`,
  the generated header in `.gitignore`.

## 3. Then
- Replace the example codes of `global.json` with the project ones and use them: **libutils-exception** skill.
- Code: `#include <utils/utils.hpp>` (or partial: `#define _Exception` ... before it), APIs: **libutils** skill;
  a `main` in the user's style: **cpp-class** (`templates/main.cpp`).
- `Debug`/`Asan` builds need the `libutils-db[-pre]` / `libutils-as[-pre]` packages (libutils-install `--variants`).
- Check: `cmake -S . -B build && cmake --build build` (the header is generated at the first build), then report the
  libutils version used and what was added.
