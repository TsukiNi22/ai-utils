---
name: cpp-class
description: Generate C++ (C++20) .hpp/.cpp files and class architectures in Tsukini's style (Xartania header, indented include guards, INCLUDE/CLASS sections, Pre-Function/Function/Operator/Constructor/Destructor blocks, libutils attributes or standard [[...]]), then register the .cpp in the CMakeLists.txt. Use whenever the user asks to create, scaffold or add a C++ class, interface (I*), abstract class (A*), template, struct/Type or Define header, module or set of .hpp/.cpp files.
allowed-tools: Read, Write, Edit, AskUserQuestion, Bash(python3:*), Bash(bash:*), Bash(clang++:*), Bash(g++:*)
---

# C++ files in Tsukini's style

Every rule is in `reference/style.md`. Read it before writing the first file, then use the
files of `examples/` (real libutils code) as the ground truth for anything not covered.
`SKILL_DIR` below = the directory of this file.

## 1. Understand the request
- Name(s), module/folder, kind of each file:
  | kind | files | template |
  |---|---|---|
  | class | `X.hpp` + `X.cpp` | `templates/class.hpp`, `templates/class.cpp` |
  | template / header-only | `X.hpp` | `templates/template.hpp` |
  | interface | `IX.hpp` | `templates/interface.hpp` |
  | abstract | `AX.hpp` (+ `AX.cpp` if it has Pre-Functions) | `templates/abstract.hpp` |
  | types | `XType.hpp` | `templates/type.hpp` |
  | defines / enums | `XDefine.hpp` | `templates/define.hpp` |
- An "architecture" (ex: "a socket with an interface, an abstract and a TCP impl") = several of the above,
  chained `IX` -> `AX: public IX` -> `X: public AX`.
- Project layout: find the project root (closest `CMakeLists.txt`) and follow the existing tree.
  Default: `include/<ns path>/X.hpp` and `src/<ns path>/X.cpp`, namespace = folder path
  (`include/utils/system/` -> `utils::system`). If the project has no namespace convention, use the
  project/module name.

## 2. Ask (one AskUserQuestion call, before writing anything)
Always ask the header question, in French, and add in the same call any missing info from step 1
(name, module, kind). Header question:
- **Xartania (Recommandé)**: default `XARTANIA` banner.
- **Nom personnalisé**: banner with another name (the user types it with "Other", or ask it right after).
- **Sans bandeau**: header box (Edition / File Name / File Description) without the ascii banner.
- **Aucun header**: no header at all.

Ask it once per request, even when many files are generated.

## 3. Detect the attributes mode
```bash
bash SKILL_DIR/scripts/detect_libutils.sh <project_root>
```
- `internal` -> libutils macros, Attribute include **relative** in headers.
- `libutils` -> libutils macros, `#include "utils/attribute/Attribute.hpp"`.
- `std` -> standard `[[...]]` attributes (mapping table in `reference/style.md`), no Attribute include.
Same logic for errors: libutils exceptions only in `internal`/`libutils` mode.

## 4. Generate each file
1. Header (skip if "Aucun header"):
   ```bash
   python3 SKILL_DIR/scripts/header.py --file X.hpp --banner xartania|none|"<NAME>" --desc "<one line>"
   ```
   `.hpp`: a real one-line description. `.cpp`: omit `--desc` (default sentence) unless asked.
2. Start from the matching template, replace the placeholders:
   `{{HEADER}}` header output, `{{GUARD}}` upper-cased name without separators,
   `{{NAMESPACE}}`, `{{CLASS}}`/`{{NAME}}`, `{{ATTRIBUTE_INCLUDE}}` (relative path from the header),
   `{{ATTRIBUTE_INCLUDE_ROOT}}` / `{{HPP_INCLUDE_ROOT}}` (path from `include/`).
   The template members/methods are placeholders: replace them by the real API asked by the user
   (keep the block order and comments style), remove the include lines that become unused,
   and keep the include comments aligned.
3. In `std` mode, convert every macro (`_hot` -> `[[gnu::hot]]`, ...) and drop the Attribute include.
4. Write real code when the behavior is described; otherwise keep minimal bodies (`/* Nothing */`).

## 5. Register the sources in CMake
For each new `.cpp`:
```bash
python3 SKILL_DIR/scripts/cmake_add.py <project_root>/CMakeLists.txt src/<path>/X.cpp
```
It inserts the file next to the files of the same folder (or in a new `## Module (sub-part: x)` block),
does nothing if already listed or if sources are collected with `file(GLOB ...)`. Exit code 2 = no
source list found: add it by hand in the right `add_executable`/`add_library`/`target_sources`.

## 6. Check
- Syntax check every new `.cpp` (and header-only `.hpp`) when a compiler is available:
  `clang++ -std=c++20 -fsyntax-only -I<project_root>/include <file>` (`g++` as fallback).
  Fix every error/warning caused by the generated code.
- Re-read the files against `reference/style.md` (braces, `(void)`, `this->`, `;` after one-liners,
  aligned include comments, section separators, guard name, namespace comments).
- Report the created files, the attribute mode used and the CMake change.
