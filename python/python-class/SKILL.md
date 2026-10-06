---
name: python-class
description: Scaffold Python files and class architectures in Tsukini's style - the Python equivalent of cpp-class - class (one per file, attributes with their defaults in __init__), abstract class (abc, abstract methods), tool (one function per file, MAGIC tool/), module of free functions, exception (subclass of the project error with its code), const category (frozen dataclass + instance), pytest test file, with the Xartania header, the ##### sections #####, the checked imports and the guards, typed signatures and empty bodies only (never the logic: the behavior goes in the docstring / comment), and the public API of a package updated (__all__). Use whenever the user asks to create, add or scaffold a Python class, module, tool function, exception, constants or test file.
---

# Python files in Tsukini's style

`SKILL_DIR` = the directory of this file. Load `python-style` (code) and `python-comments` (comments) first; a project
layout comes from `python-project`. Generator (`--help` for every option):
```bash
python3 SKILL_DIR/scripts/new_file.py <kind> <Name> --dir <folder> [--desc "..."] [--base Parent] \
    [--attributes "name: type = default # comment; ..."] [--methods "name(params) -> type # what it does; ..."] \
    [--relative] [--export] [--banner xartania|none|<text>]
```

> **Architecture only: never write the logic of a function.** Every body is `pass` (no return value), the smallest
> return of its type (`return 0`, `""`, `False`, `[]`, `{}`, `None` for `X | None`), `...` (abstract method) or
> `raise NotImplementedError` (any other type), even if the user describes the behavior: the description goes in the
> docstring of a public method or the trailing comment of the `def`. The user writes the logic. Allowed: attributes
> with their defaults in `__init__`, `super().__init__()`, the constants of a `const.py` category.

## 1. Understand the request
| Kind | File | Content |
|---|---|---|
| `class` | `<folder>/<snake_name>.py` (`Class/` of an application, `src/<pkg>/` of a package) | class `PascalCase`, docstring, `__init__` with the typed attributes (`self._x: int = 0 # meaning`), the methods |
| `abstract` | same | `class X(ABC)` + `@abstractmethod` methods with `...` (the `I` / `A` of `cpp-class`: the interface of a family) |
| `tool` | `tool/<function>.py` (MAGIC) | one function named like the file under `##### Program #####` |
| `module` | `<folder>/<name>.py` | free functions under `##### Tools #####` |
| `exception` | appended to `exception.py` | `class XError(ProjectError)` with its `ERROR.*` code |
| `const` | appended to `const.py` | a frozen dataclass category + its instance in `##### Declaration #####` (aligned) |
| `test` | `tests/test_<name>.py` | one `test_<function>` per listed name (bodies written with `tests` / pytest) |
- `--relative`: file of a package (`from .const import ...`, no `__main__` guard); without it: application / script
  (`from const import ...`, checked imports, `if __name__ == "__main__"` guard: the module can only be imported).
- `--base`: parent class; imported automatically when it is in the same folder, else add the import by hand.
- `--export`: adds the class to `__all__` and the imports of the package `__init__.py` (public API).
- Types used in a signature that are not builtins (another class of the project) are imported by hand in the
  `# Import that can be checked` block with their `# Used for` comment.

## 2. Decide before generating
- Names: `snake_case` file / function / method / attribute, `PascalCase` class, `_name` for the private ones (methods
  only used by the class, attributes not part of its API), `UPPER_SNAKE` constants. One class per file.
- Where: application (MAGIC) `src/Class/`, `src/tool/`, feature folders (`src/window_build/`); package `src/<pkg>/` with
  sub-packages per concern. Show the tree in the AskUserQuestion when a place is ambiguous.
- Signatures fully typed (`-> None` included), defaults on the parameters of a mode (`failsafe: bool = False`).

## 3. After
`xstyle <files>` (python-style rules), `python3 -c "import <module>"` (or `PYTHONPATH=src` for a package) to check that
the skeleton imports; tests created with `--kind test` are filled with the `tests` skill.
