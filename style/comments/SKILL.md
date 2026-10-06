---
name: comments
description: How Tsukini writes comments and file headers in any language - C, C++, CMake, Makefile, shell, Python, Lua, YAML (GitHub workflows), Markdown/HTML docs, and a fallback for any other language - section separators/banners, trailing vs above comments, notations, tone. Use whenever writing, adding, rewriting or reviewing comments or file headers in any file of the user (for C++ details also see cpp-comments).
---

# Tsukini comments (every language)

Extracted from the user's real code: libutils / context-forge (C++, CMake, shell, Python, YAML), the C project
`personal_delivery/c/compressor`, the nvim config (Lua, header templates). When a file already has comments,
**its local style wins**; otherwise apply the rules below.

## 1. General rules (all languages)
- **English**, short, factual, no final period, no emoji. Comment the **why**, a non-obvious behavior, a value
  meaning, a limit; never paraphrase the code.
- Above-the-block comment: one line, starts with an upper case letter (`# Determine branches`,
  `// Get a new id`), placed right above the block it describes, after an empty line.
- Trailing comment: on declarations / values / options to give the meaning, unit, default or constraint
  (`SOCKET_TIMEOUT 7 // Timeout in seconds`, `find_package(OpenSSL REQUIRED) # Used for encryption purpose`).
- Notations: `->` consequence / mapping (`# compinit already done -> only register the completion`),
  `=` value meaning (`0 = unlimited`, `-1 = closed`), `<a, b>` tuple/pair fields, `a | b` alternatives,
  `(...)` precision (`(failsafe)`, `(default: 4096)`), `word: precision` for a nuance (`Not an error: only ...`).
- Emphasis on a danger: upper case + `!!!` or `(only!)` (`reset fd (DOES NOT CLOSE!!!)`).
- Sections: each language has its separator/banner (below); sub-groups with a small label comment.
- Disabled code is kept commented, without space after the marker (`//add_library(...)`, `#require 'x'`).
- No `TODO`/`FIXME`/`XXX` tags, no Doxygen/docstring blocks, no decorative boxes except the file header and
  the separators of the language.
- File header: generated, never handwritten (see 9).

## 2. C++ (`.cpp`, `.hpp`) - details in the `cpp-comments` skill
- Separator + upper case title: `//` + 64 `-` + `//` then `/* INCLUDE */`, `/* DEFINE */`, `/* CLASS */`...
- Class blocks: `// ---------- Pre-Function -------- //`, `// ------------ Function ---------- //`, ...
- Group labels in lower case: `/* type */`, `/* getter */`, `/* setup */`.
- Include comments aligned listing what is used: `#include <mutex>   // std::mutex`.
- Block comments `// Uppercase start`, trailing `//` comments, `/* Nothing */` for an empty body.

## 3. C (`.c`, `.h`)
Same header layout as C++ (`#ifndef X_H` / indented `    #define X_H`, `//---...---//` + `/* INCLUDE */`,
`/* DEFINE */`, `/* TYPEDEF */`, `/* PROTOTYPE */`, `/* GLOBAL_CONST */`, `#endif /* X_H */`), but the comments
of the code are **C block comments in lower case** (style of the C projects):
```c
/* check the arg who start with '-' or '--' is valid flag */
static int flag(main_data_t *data, int const argc, char const *argv[], int const i)
{
    int index = 0;

    /* function argument check */
    if (!data || !argv)
        return KO;

    /* main execution */
    res = compressor(argc, argv, &data);
}
```
- Every function is preceded by a `/* what it does */` comment; the first block of a function taking pointers is
  `/* function argument check */`.
- Prototypes in the header grouped by source file (`/* init_flag */`) with the error return in trailing comment:
  `int init_data(main_data_t *data); // Error: KO` (`// Error: none` when it can't fail).
- Struct fields grouped by `/* group */` labels, trailing `//` comments for ranges/meaning
  (`char precision; // 1 <-> 17`), includes commented with the type they bring (`#include <stdbool.h> // boolean`).
- Sub-parts inside a long function: `// compresion //` style markers.

## 4. CMake (`CMakeLists.txt`, `*.cmake`)
```cmake
# =========================
# Requirement
# =========================
find_package(OpenSSL REQUIRED) # Used for encryption purpose
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Moderne compilation -fPIE
```
- Section banner = `# ` + 25 `=`, title, banner (exact strings in `cmake-style`).
- Trailing `# Comment` on `find_package`, `set`, options; above-line `# library`, `# header` sub-labels in lower case
  inside `Installation`; source groups `# Group` / `## Group (sub-part: x)` in `set(SRC ...)`.
- Kept alternatives: `#add_library(${TARGET} SHARED ${SRC})`.

## 5. Shell (`*.sh`) and Makefile
```bash
# =========================
# Parse arguments
# =========================
# The rc files belong to the user who launched the script (even through sudo)
mkdir -p "$REPO_DIR" # Security
```
- Same 25 `=` banners as CMake for the sections of a script; short scripts use `# Vars definition`-like labels.
- Usage block at the top of a script: `# Usage: ...`, `# Commands:`, `# Options:` aligned columns (read by `usage()`).
- Blocks written into user files are fenced: `# >>> name completion >>>` ... `# <<< name completion <<<`.
- Makefile: header `#***...***#` / `#Edition:` / `#**  DD/MM/YYYY by Tsukini` (see 9), no other comment unless
  a target is not obvious.

## 6. Python (`*.py`)
```python
##### Import #####
# Import that can't be in the try
from sys import exit, stderr

try:
    from hashlib import sha256 # Used to identify different error
except ImportError as e:
    ...

##### Program #####
data = {} # {code: [message, info, restriction], ...}
```
- Sections: `##### Title #####` (`Import`, `Tools`, `Program`...); `# Uppercase` above blocks; trailing `# Used for ...`
  on imports and data structures (shape of the data in `{...}` / `[...]`).
- No docstrings in scripts (the file header gives the description); a module documented for CLI usage may have one
  usage docstring at the top.

## 7. Lua (nvim config)
```lua
-- Set leader key
vim.o.shiftwidth = 4                                                        -- The number of spaces inserted for each indentation (default: 8)
vim.keymap.set('n', '<leader>e', '<cmd>Ex<CR>', opts)       --Go to file explorer
```
- `-- Uppercase` above blocks; long option lists get trailing comments aligned on a far column with the default
  value `(default: x)`; mapping lists use a compact trailing `--Label`.

## 8. YAML (GitHub workflows), Markdown, HTML, JSON
- YAML: the step `name:` is the documentation (`- name: Build packages (Release)`); `# Comment` only for a non-obvious
  trigger or step (`workflow_dispatch: # Triggered by Dispatch (CI/CD)`, `# Determine branches` inside `run: |`).
- Markdown: no hidden comments; remarks are GitHub callouts `> [!NOTE]`, `> [!TIP]`, `> [!IMPORTANT]`,
  `> [!WARNING]`, `> [!CAUTION]` (one idea each, see `readme-style`).
- HTML docs: `<!-- N -->` before each numbered section (see `html-doc`), nothing else.
- JSON: no comments possible; put the explanation in the README/skill or in a `"description"` field when the
  format has one.

## 9. File headers
| Files | Header | Generated by |
|---|---|---|
| `.cpp`, `.hpp` | Xartania box + banner, `##  @date DD/MM/YYYY by @author Tsukini`, `##  @file X.hpp` | `cpp-class/scripts/header.py` (or nvim `<C-h>`) |
| `.c`, `.h` | same box, without tags: `##  DD/MM/YYYY by Tsukini`, `##  main.c` | nvim Xartania C header |
| `.py` | box of 63 `"` + banner + `##` lines | nvim Xartania Python header |
| `Makefile`, `.mk` | `#***...***#` box, `#Edition:` / `#**  date by Tsukini` | nvim Xartania Makefile header |
| school projects (`~/delivery`, Epitech) | `EPITECH PROJECT, <year>` header (`/*` `**` `*/`, `##` for Makefile/Python, `{-` `-}` Haskell) | nvim Epitech header |
- Description line: a real one-line description, or the default `You know, I don t think there are good or bad
  descriptions,` / `for me, life is all about functions...`.
- Shell scripts, CMake, YAML, Lua, Markdown: no file header (a `#!` line and a usage block for scripts).

## 10. Any other language
1. Copy the comment style already present in the file / project (separators, case, trailing vs above).
2. Otherwise apply section 1 with the language comment syntax, banners like the closest language above
   (`#` languages -> shell/CMake banner, `//` languages -> C++ separator), English, no docstring blocks.
3. Say in the answer that no specific rule exists for this language.

## Checking (xstyle)
When `xstyle` is installed (`command -v xstyle`), check what was written or reviewed with it instead of re-reading the
rules by hand: `xstyle --rtk <files|dirs> [-r]` (**always `--rtk`**: compact output made for the assistant),
`xstyle --rtk --fix [CODES] <paths>` for the fixable ones (`-n` to preview), `-c CPP-DOXYGEN,CPP-COMMENT-ALIGN,G-TODO` for the rules of this
skill, `xstyle -x CODE` to explain one. Not installed: apply the rules by hand and say once that
`./setup.sh install xstyle` would check them. Its findings are heuristic: the local style of a file wins.
