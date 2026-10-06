---
name: python-style
description: Tsukini's Python style (from his real scripts - libutils cmake/scripts const.py / generate_exception_header.py, MAGIC - and the xstyle PY rules) - Xartania header, ##### sections #####, imports split in the ones that can't fail and a checked try block with a "# Used for" comment each, __main__ guards for scripts and modules, const.py with frozen dataclasses and their instances, Return / Error exit codes, errors on stderr with an explicit exit, type hints, naming, 4 spaces and English. Use whenever writing, editing, refactoring or reviewing Python code for the user (a script, a module, const.py, a class), with python-project for a whole project.
---

# Tsukini Python style

Measured on the user's own scripts (libutils `cmake/scripts/const.py`, `generate_exception_header.py`, 2026) and on
MAGIC (2025, older form: `""" Import """` banners, plain classes for the constants, `print` errors): **the 2026 form
wins**, the older one is only kept when editing a file already written that way (the local style of a file wins).
Common rules of every language: `coding-style`; comments: `python-comments`; new files / classes: `python-class`. Everything in **English**.
Checked by `xstyle` (`PY-*` + the generic rules): run `xstyle --rtk <files>` after writing, `xstyle --rtk --fix` for the fixable ones.

## File layout
```python
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""   # header: <cpp-class>/scripts/header.py --file x.py

 ██╗  ██╗ ...                                                      # (banner, optional: --banner none)

Edition:
##  06/10/2026 by Tsukini

File Name:
##  generate.py

File Description:
##  Generate the exception header from the config file
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Import #####
# Import that can't be in the try
from const import RETURN, ERROR, FILES
from sys import exit, stderr

# Import that can be checked
try:
    from pathlib import Path # Used to create & edit files
    import json # Used to get the json data
    import re # Used for pattern matching
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(ERROR.FATAL)

# Check if the program is call and not imported
if __name__ != "__main__":
    stderr.write(f"The {__file__} can only be executed and not imported!\n")
    exit(ERROR.FATAL)

##### Tools #####
def uint64_hash(s: str) -> int:
    digest = sha256(s.encode()).digest()
    value = int.from_bytes(digest[:8], "big") # 8 octets = 64 bits
    return 1 + (value % VALUES.SIZE_MAX) # limit values

##### Program #####
...
exit(RETURN.OK)
```
- Header: `python3 <cpp-class>/scripts/header.py --file <name>.py [--desc "..."] [--banner none]` (63 `"`, no
  `@date` / `@file` tags in Python), never written by hand.
- Sections, in this order, only the ones used: `##### Import #####`, `##### Const #####`, `##### Tools #####`
  (free functions), `##### Class #####`, `##### Program #####` (scripts), `##### Declaration #####` (const.py: the
  instances). One empty line between blocks, **never two** (the user's rule, stricter than PEP 8; `G-EMPTY-LINES`).
- Imports: first `# Import that can't be in the try` (the project constants, `sys`), then `# Import that can be checked`
  in a `try` with **one trailing `# Used for ...` per import**, `except ImportError as e:` -> message with `__file__`
  on stderr + `exit(ERROR.FATAL)` (or `exit(255)` without const). `from module import name` explicit, never `import *`
  (`PY-WILDCARD-IMPORT`). Order of `coding-style` (personal libs, project, external; longest first).
- Optional dependency install of a build script (libutils pattern, never in a library): a `try` that runs
  `pip install -r requirements.txt` only when not root (`geteuid() != 0`), `except Exception: pass` (failsafe, the only
  accepted silent `except`).
- Guards right after the imports: a script exits when imported (`if __name__ != "__main__"`), a module exits when
  executed (`if __name__ == "__main__": ... can only be imported and not executed!`). An installable package uses a
  `main() -> int` function instead (`python-project`).

## const.py
```python
##### Const #####
@dataclass(frozen=True)
class Return():
    """
        Return values
    """
    OK: int = 0 # Return value upon success on a call function
    KO: int = 1 # Return value upon fail on a call function

@dataclass(frozen=True)
class Error():
    """
        Error values
    """
    FATAL: int  = 0b1000    # Global error, the whole execution stops                  (100% execution stop)
    LOCAL: int  = 0b100     # Local error, the local execution probably can't go on   (some chance of execution stop)
    ACTION: int = 0b10      # Same~~ as Return.KO, an action of the program fails      (low chance of execution stop)

@dataclass(frozen=True)
class Files:
    """
        Different files path
    """
    REQUIREMENTS: str = "cmake/scripts/requirements.txt"

##### Declaration #####
RETURN  = Return()
ERROR   = Error()
FILES   = Files()
```
- One frozen dataclass per category (`Return`, `Error`, `Values`, `Files`, `Names`...), PascalCase, short indented
  docstring, fields `UPPER_SNAKE` **typed** with a trailing comment (aligned in a group), mutable values through
  `ClassVar[dict[...]]`, groups inside a class under a `# Comment` line; instances in UPPER at the end, aligned.
- Exit codes: `RETURN.OK` (0) / `RETURN.KO` (1), the `ERROR.*` bit flags, `255` only for the import failures.

## Code
- **Naming**: `snake_case` functions / variables / modules, `PascalCase` classes, `UPPER_SNAKE` constants, `_private`
  attributes and methods, special mode parameters `snake_case` with a default (`failsafe: bool = False`).
- **Types**: type hints on every new parameter and return (`-> None` included, `PY-TYPE-HINTS`), `str | None` for an
  optional value (Python 3.10+), builtin generics (`list[str]`, `dict[str, int]`).
- **Errors**: never silent. `stderr.write(f"Error: <what> ({path})\n")` then `exit(<code>)` in a script
  (`PY-PRINT-ERROR`: never `print(..., file=stderr)`), exceptions with a context message in a module / library, the
  expected exceptions only (`PY-BARE-EXCEPT`: never `except:`), early returns.
- **Files**: `pathlib.Path`, `with open(path, "r", encoding="utf-8") as f:` (`PY-OPEN-ENCODING`), binary modes explicit.
- **Strings**: f-strings, double quotes (single quotes inside f-strings), `str.format` never.
- **Comments**: a block comment `# Capitalized, no final period` above each logical block, trailing comments for a
  precision; no docstring on the functions of a script (the trailing comment of the `def` or the block comment is
  enough); a class keeps a short indented docstring (`"""\n        What it is\n    """`), methods of a class too when
  public (one line: what it does). Never `:param` / Sphinx / Google docstrings in new code (MAGIC's older form).
- **Classes**: attributes created in `__init__` (defaults there), methods `snake_case`, one class per file in a
  `Class/` folder of an application (MAGIC), `@dataclass` for plain data.
- 4 spaces, no tabs, no trailing space, `exit(RETURN.OK)` as the last line of a script.

## Review
List the deviations with the rule (this skill / `xstyle` code); `xstyle --rtk --fix -c PY,G` applies the safe ones,
`--dangerous-force` the guessed type hints (check them).
