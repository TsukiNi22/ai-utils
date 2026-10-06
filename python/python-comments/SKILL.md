---
name: python-comments
description: How Tsukini writes comments in Python - the Python equivalent of cpp-comments, from his scripts (libutils cmake/scripts, MAGIC) - the Xartania header (63 double quotes), the ##### Section ##### banners, the "# Import that can't be in the try" / "# Import that can be checked" labels, the "# Used for ..." trailing comment of every import, trailing comments on constants (meaning, unit, values, aligned in a group), block comments above the code, short indented docstrings for classes and public methods, no Sphinx / Google docstrings, notations and tone. Use whenever adding, rewriting or reviewing comments or docstrings in Python code for the user, or when generating Python code that needs comments.
---

# Tsukini comment style (Python)

Always in **English**, short, no final period. Comment the **why** and the non-obvious, never paraphrase the code.
General rules of every language: `comments` (section 1). Code style: `python-style`. When a file already has its own
comment style (an older file of MAGIC with `""" Import """` banners), the local style wins.

## 1. Structure comments (exact strings)
- File header: `python3 <cpp-class>/scripts/header.py --file <name>.py --desc "..."` (63 `"`, banner, `Edition:` /
  `File Name:` / `File Description:` with `##` lines), never written by hand.
- Section banners, at column 0, one empty line before: `##### Import #####`, `##### Const #####`, `##### Tools #####`,
  `##### Class #####`, `##### Program #####`, `##### Declaration #####`, `##### Tests #####` (only the used ones).
- Import labels (exact): `# Import that can't be in the try`, `# Import that can be checked`; the optional install block
  `# Try to install dependencies (failsafe)`; the guard comments `# Check if the program is call and not imported` /
  `# Check if the program is imported and not call` (the user's wording, kept as is).

## 2. Trailing comments (the most used)
```python
    from pathlib import Path # Used to create & edit files and to get the file name
    exit(255) # Special exit code (only place used)
data = {} # {code: [message, info, restriction], ...}
    FATAL: int  = 0b1000    # Global error, the whole execution stops          (100% execution stop)
    LOCAL: int  = 0b100     # Local error, the local execution probably stops  (some chance of execution stop)
    SIZE_MAX: int = 2**64 - 1  # size_t max on 64 bits machine
```
- **Every checked import**: `# Used for <what>` / `# Used to <do what>`.
- Constants: meaning, unit, values (`0 = unlimited`), the shape of a container (`{code: [message, info], ...}`,
  `<id, name>` for a tuple), alternatives with `|` (`# dark | light`), precisions in `(...)`; aligned in a group of
  constants (fields and comments in columns).
- Private / free functions and the `def` of a script: the behavior in a trailing comment of the `def` line
  (`def _parse(self, line: str) -> int: # one device value`).
- Danger in upper case: `# DOES NOT CLOSE THE PORT!!!`, `(only!)`.

## 3. Block comments above the code
```python
    # Init the loading overlay
    loading = LoadingOverlay(window, ...)

    # Wait until at least one device have been found or timeout
    while time() - start <= timeout and not card.values:
```
One line, upper case first letter, no final period, describes the intent of the block, an empty line before the block.
`word: precision` form for a nuance (`# Not an error: the port can be closed by the user`). Small labels in lower case
are fine for the parts of a block (`# code`, `# message`, `# info` in a loop building several strings).

## 4. Docstrings (rare, short)
```python
class Parameters:
    """
        Class to handle the parameters edition / reading
    """

    def get_parameter(self, name: str) -> str | None:
        """
            Get the value of a parameter
        """
```
- Classes and **public** methods only: one or two lines, the text indented by 4 more than the quotes (8 spaces inside
  a class), the quotes alone on their lines. Frozen dataclasses of `const.py` too (`"""\n        Return values\n    """`).
- **Never** Sphinx / Google / NumPy fields (`:param x:`, `Args:`, `Returns:`): the types are in the signature, the
  meaning of a parameter goes in its name or a trailing comment. MAGIC's older `:param` docstrings are not reproduced.
- Scripts: no module docstring (the header is the description), no docstring on the functions.

## 5. Misc
- Disabled code kept only when it is a real alternative, with `#` and no space (`#&& dnf remove -y gnupg2`).
- No `TODO` / `FIXME` (`G-TODO` of xstyle): describe the limitation in a normal comment.
- `pass  # Nothing` is never written: `pass` alone is the empty body.
