---
name: cpp
description: Router for every C++ task of the user - looks at the request and the project, then loads the right C++ skills (cpp-project, cpp-class, cpp-style, cpp-comments, cmake-style, libutils, libutils-exception). Invoke manually with /cpp.
disable-model-invocation: true
---

# C++ router

**Request given with `/cpp`:** $ARGUMENTS

Do not answer from this file: decide which skills apply, **load them with the Skill tool** (never the other routers: they are manual-only), then do the task
following them. The request is the text given with `/cpp` (or, if empty, the latest request of the user;
if there is none, inspect the project and propose what can be done).

## 1. Look at the context (quickly, in parallel)
- `CMakeLists.txt`, `include/`, `src/`, `tests/`, `cmake/config/exceptions/` present? Empty folder = new project.
- libutils used? `bash ~/.claude/skills/cpp-class/scripts/detect_libutils.sh <root>` (`internal` / `libutils` / `std`),
  `grep -rl "utils::" src include`.
- Files the request is about (open them before deciding).

## 2. Pick the skills (several can apply, load all of them before writing)
| Situation | Skills |
|---|---|
| new project / repository / "setup" | `cpp-project` (it loads the others it needs) |
| new class, interface, abstract, struct, enum, module, main, architecture | `cpp-class` (+ `cpp-style`, `cpp-comments`) |
| write / edit / refactor / fix C++ code | `cpp-style` + `cpp-comments` (+ `libutils` if the project uses it) |
| comments only | `cpp-comments` |
| review C++ code against the user's style | `cpp-style` + `cpp-comments` (+ `cmake-style` if CMake changed) |
| `CMakeLists.txt`, tests CMake, packaging, CPack, `find_package` | `cmake-style` |
| "how to do X", which class/function, links/includes of libutils, what's new in libutils | `libutils` |
| throw/catch/print errors, new error codes, exception JSON, generated exception header | `libutils-exception` (+ `libutils`) |
| install / update / remove libutils, build fails because libutils is missing or too old | `libutils-install` |
| add libutils to the current project (CMake, custom exceptions setup) | `libutils-setup` |
| new `.cpp` added | also `cmake-style` (registration in `SRC`, `cpp-class/scripts/cmake_add.py`) |
| commit / docs asked at the end | `git-conventions` / `readme-style`, `html-doc` |

Rules: architecture skills never write function logic (`cpp-class`); when unsure between two rows, load both;
when the project doesn't use libutils, don't load `libutils*` and use standard C++ (`cpp-style` `std` mode).

## 3. Say it
Start the answer with one line: which skills were loaded and why (ex: "Skills : cpp-class, cpp-style,
cpp-comments (nouvelle classe dans un projet libutils)").
