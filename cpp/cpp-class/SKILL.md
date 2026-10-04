---
name: cpp-class
description: Generate C++ (C++20) .hpp/.cpp files and class architectures in Tsukini's style (Xartania header, indented include guards, INCLUDE/CLASS sections, Pre-Function/Function/Operator/Constructor/Destructor blocks, libutils attributes or standard [[...]]), with empty bodies only (never the logic), then register the .cpp in the CMakeLists.txt. Also creates the main.cpp entry point in the same style. Use whenever the user asks to create, scaffold or add a C++ class, a main / entry point, interface (I*), abstract class (A*), template, struct/Type or Define header, module or set of .hpp/.cpp files.
allowed-tools: Read, Write, Edit, AskUserQuestion, Bash(python3:*), Bash(bash:*), Bash(clang++:*), Bash(g++:*)
---

# C++ files in Tsukini's style

Read before writing the first file:
- `reference/layout.md`: file/class layout, header, includes, attributes, **empty bodies rule**;
- the `cpp-style` skill (`SKILL_DIR/../cpp-style/SKILL.md`): naming, formatting, spacing;
- the `cpp-comments` skill (`SKILL_DIR/../cpp-comments/SKILL.md`): comments.
The files of `examples/` (real libutils code) are the ground truth for the layout.
`SKILL_DIR` below = the directory of this file.

> **Architecture only: never write the content/logic of a function.** Every body is
> `/* Nothing */` (+ the minimal `return {};` when non-void), even if the user describes the
> behavior: it goes in a trailing comment of the declaration. The user writes the logic.
> Allowed: `= default`/`= delete`/`= 0`, init lists forwarding the parameters, one-member
> accessors, a public wrapper forwarding to its `name_` implementation. Details in `reference/layout.md`.

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
  | family / umbrella | `X.hpp` (includes the family, no namespace) | `templates/family.hpp` |
  | entry point | `src/<root>/main.cpp` + the core class (`Core` or the project class) | `templates/main.cpp` (`main-std.cpp` in `std` mode) |
- Entry point (new project, or when asked for a `main`): `src/<root>/main.cpp` from `templates/main.cpp`
  (the same in context-forge, virtual-os, cpp_project_template): it only creates the core class, calls
  `init(argc, argv)` then `run()`, catches `utils::exception::IException` (`Exit` code -> `OK`, otherwise print
  `formated()` and return `KO`) and returns `core.exit()` (`return OK;` when the core class has no `exit`).
  This wiring is the only body written by the skill. Create the core class with it if it doesn't exist:
  `void init(int argc, const char *const argv[]);`, `void run(void);`, `int exit(void) const;` as
  Pre-Functions with empty bodies. Register `main.cpp` first in `SRC`, under `# Entry`
  (`cmake_add.py ... --group Entry`). `std` mode: `templates/main-std.cpp` (`std::exception`, `EXIT_FAILURE`).
- An "architecture" (ex: "a socket with an interface, an abstract and a TCP impl") = several of the above,
  chained `IX` -> `AX: public IX` -> `X: public AX`.
- Project layout: find the project root (closest `CMakeLists.txt`) and follow the existing tree.
  Default: `include/<root>/<section>/X.hpp` and `src/<root>/<section>/X.cpp`.
- Namespaces: apply the "Namespaces" section of `reference/layout.md` (root = project name, one level
  per section folder, grouping folders skipped, a family `I/A/impl` folder stays in the parent namespace,
  internals in a nested namespace, family umbrella header `templates/family.hpp`). Write the planned
  tree `include/<path>/X.hpp -> namespace` before generating.

## 2. Ask (one AskUserQuestion call, before writing anything)
Always ask the header question, in French, and add in the same call any missing info from step 1
(name, module, kind, namespace when ambiguous). Header question:
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
- `libutils` -> libutils macros, everything from libutils through `#include <utils/utils.hpp>` (never a
  libutils header one by one, except the ones `utils.hpp` does not give like the observers; see
  "libutils includes" in `reference/layout.md`).
- `std` -> standard `[[...]]` attributes (mapping table in `cpp-style`), no Attribute include.
Same logic for errors: libutils exceptions only in `internal`/`libutils` mode.
Any attribute (alignment `_alignas(std::hardware_destructive_interference_size)` included) follows the lookup
order of `cpp-style` "Attributes": libutils default macros, then the ones really available in the project /
system (`bash SKILL_DIR/scripts/list_attributes.sh <project_root>`), then only the standard `[[...]]` form.

## 4. Generate each file
1. Header (skip if "Aucun header"):
   ```bash
   python3 SKILL_DIR/scripts/header.py --file X.hpp --banner xartania|none|"<NAME>" --desc "<one line>"
   ```
   `.hpp`: a real one-line description. `.cpp`: omit `--desc` (default sentence) unless asked.
2. Start from the matching template, replace the placeholders:
   `{{HEADER}}` header output, `{{GUARD}}` upper-cased name without separators,
   `{{NAMESPACE}}`, `{{CLASS}}`/`{{NAME}}`, `{{UTILS_INCLUDE}}` (with its delimiters: `<utils/utils.hpp>` in
   `libutils` mode, `"../attribute/Attribute.hpp"` relative in `internal` mode), `{{UTILS_INCLUDE_ROOT}}` (.cpp:
   `<utils/utils.hpp>`, or `"utils/attribute/Attribute.hpp"` in `internal` mode), `{{HPP_INCLUDE_ROOT}}` (path from `include/`), `{{KIND}}` (group label of the implementations in a family header).
   The template members/methods are placeholders: replace them by the real API asked by the user
   (keep the block order and comments style), remove the include lines that become unused,
   and keep the include comments aligned.
3. In `std` mode, convert every macro (`_hot` -> `[[gnu::hot]]`, ...) and drop the Attribute include.
4. Bodies: always empty (`/* Nothing */` + minimal return), never logic, see the rule at the top.
5. Remove every section with nothing linked to it (class blocks `// --- X --- //`, file sections
   `/* DEFINE */`, `/* PROTOTYPE */`..., empty visibilities and `/* group */` labels): the template
   blocks are not mandatory, only the ones holding a declaration are kept.
6. Short functions (getters, setters, accessors, `name_` forwarders, one-liners) go in the
   `Function` block of the `.hpp` with `inline` after the attributes, not in the .cpp, when it can
   help the compiler inline them (not on `virtual` methods). Rules in `reference/layout.md`.
7. Private / protected / internal functions are named `<name>(<Name>)*_` (camelCase + trailing `_`:
   `allocate_`, `computeHash_`), public methods plain camelCase (`cpp-style` naming table).

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
  `clang++ -std=c++20 -Wall -Wextra -Wno-unused-parameter -fsyntax-only -I<project_root>/include <file>`
  (`g++` as fallback). Fix every error/warning caused by the generated code (the unused parameters
  of the empty bodies are expected, never silence them in the code).
- Re-read the files against `reference/layout.md` and `cpp-style` (braces, `(void)`, `this->`,
  `;` after one-liners, one-liner bodies aligned tight (longest signature + 1 space), aligned include comments, section separators, guard name, namespace comments)
  and check that **no body contains logic**, that **no section separator is left empty** and that
  the short non-virtual functions are `inline` in the header, and that every
  non-public function ends with `_`.
- Report the created files, the attribute mode used and the CMake change.
