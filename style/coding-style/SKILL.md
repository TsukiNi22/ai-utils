---
name: coding-style
description: Tsukini's coding style for every language other than C++ (derived from cpp-style and from his real Python, shell, Lua, YAML and CMake code) - naming, 4-space indentation, layout in sections, error handling, explicit types, English. Python, shell/bash, Lua, JavaScript/TypeScript, Rust, Go, Java/C#, C, YAML/JSON and a fallback for any language. Use whenever writing, editing, refactoring or reviewing non-C++ code for the user (C++: cpp-style; CMake: cmake-style; comments: comments).
---

# Tsukini coding style (any language except C++)

The C++ rules (`cpp-style`) are the reference: the same spirit applies everywhere. When editing an existing file, its
local style wins. Comments: the `comments` skill. Everything (code, identifiers, comments, messages) in **English**.

## Common rules
- **4 spaces**, never tabs (except where the language requires them: Makefile recipes, Go `gofmt`). No trailing
  whitespace, one empty line between blocks, never two.
- **Explicit over implicit**: write the types when the language allows it (TS types, Python type hints for
  parameters/returns, Rust/Go explicit types on public items); no `any` / untyped `var` / inferred public APIs unless
  the user asks (same rule as no `auto` in C++, iterators and destructuring excepted).
- **Naming**: types `PascalCase`, functions/methods `camelCase` (Python, shell, Rust, C: `snake_case` as the
  language wants), constants `UPPER_SNAKE`, private members `_name` where the language has no `private`,
  special mode flags `snake_case` with a default value.
- **Layout in sections** with the banner of the language (see below): imports / constants / tools / program.
- **Errors**: never silent. Message on stderr naming the file/context, explicit exit code (`0` ok, `1` error,
  `255` special/fatal), early returns, failsafe options rather than crashes.
- Short functions, early returns, no magic number (named constant with a trailing comment), no dead code (kept
  commented only when it is a real alternative).
- Imports / includes explicit and grouped (no wildcard import).

## Python (from libutils `cmake/scripts/*.py`)
```python
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""   # Xartania header (comments skill)
...
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Import #####
# Import that can't be in the try
from sys import exit, stderr

# Import that can be checked
try:
    from pathlib import Path # Used to create & edit files
    import json # Used to get the json data
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(255)

# Check if the program is call and not imported
if __name__ != "__main__":
    stderr.write(f"The {__file__} can only be executed and not imported!\n")
    exit(255)

##### Tools #####
def uint64_hash(s: str) -> int:
    ...

##### Program #####
```
- `from module import name` explicit, each import with a trailing `# Used for ...`; optional dependencies in a
  `try` with an explicit error.
- Constants in `@dataclass(frozen=True)` classes (`Return.OK`, `Error.FATAL`), shared in a `const.py`.
- `snake_case` functions/variables, f-strings, `with open(..., encoding="utf-8")`, `stderr.write` + `exit(code)`
  for errors (no bare `print` for errors), guard `__main__` (script) or the opposite (module only imported).
- Type hints on new functions (`-> int`), no docstrings in scripts (a trailing comment is enough); a class keeps a
  short indented docstring like `const.py`.

## Shell / bash (from libutils / context-forge `setup.sh`, `check_files.sh`)
```bash
#!/bin/bash
set -euo pipefail

BASE_URL="https://..."
SUDO="sudo"

# =========================
# Parse arguments
# =========================
usage() {
    echo "Usage: $0 [--no-sudo]"
    exit 1
}

for arg in "$@"; do
    case "$arg" in
        --no-sudo) SUDO="" ;;
        -h|--help) usage ;;
        *) echo "Error: unknown argument '$arg'" >&2; usage ;;
    esac
done
```
- `set -euo pipefail`, `#!/bin/bash` (bash features used), configuration variables `UPPER_SNAKE` at the top,
  functions `snake_case` (`install_rpm_repo`, `is_rpm_based`), sections with the 3-line `# =====` banner.
- Always quote expansions (`"$var"`, `"$@"`), `[[ ]]` for tests, `command -v` to check tools (`require curl`),
  errors on stderr (`>&2`) with `Error: ...`, `usage()` printing the help and exiting.
- Long options `--name`, a `-h|--help`, `$SUDO` variable to allow `--no-sudo`; idempotent scripts (re-running
  changes nothing), markers `# >>> name >>>` / `# <<< name <<<` for blocks written in user files.

## Lua (from `~/.config/nvim`)
- 4 spaces, `local` everything, `snake_case` / `camelCase` like the surrounding file, `require 'module'` for
  configs, a module returns a table or a function (`return function() ... end`).
- Settings one per line with an **aligned** trailing comment (`-- What it does (default: x)`).

## JavaScript / TypeScript
- TypeScript preferred, `strict`, explicit types on parameters/returns/exports, no `any`; `const` by default,
  `camelCase` functions, `PascalCase` classes/types, `UPPER_SNAKE` constants, 4 spaces, semicolons, double quotes.
- Errors: throw `Error` subclasses with a context message, never swallow them; `async`/`await` over raw promises.
- Self-contained HTML pages: inline script in an IIFE with `"use strict"` (like the html-doc templates).

## Rust / Go / Java / C# / C
- Language conventions for naming (`rustfmt`, `gofmt`, ...) but the user's spirit: explicit types on public items,
  early returns, errors propagated with context (`Result` + `?` with a message, Go `fmt.Errorf("...: %w", err)`),
  no panic/exit in libraries, constants named.
- C: same layout as C++ (`cpp-class` sections, Epitech/Xartania header per `comments`), `snake_case`, functions
  returning `OK`/`KO` (or an error code) and checking their pointer arguments first.

## YAML / JSON / config
- YAML: 2 spaces (format constraint), keys `snake_case` or the tool's convention, GitHub workflows named
  `Name - Kind (CI|CD)`, every step has a `name:`, `set -e` / `set -euo pipefail` in multi-line `run:`.
- JSON: 4 spaces, no trailing comma, keys `snake_case` (or the format's convention).

## Any other language
Follow the closest existing file of the project; otherwise the common rules above with the language's official
formatter conventions, and say that no specific rule covers it.
