---
name: cpp-comments
description: How Tsukini writes comments in C++ (from libutils) - section separators, /* group */ labels, trailing comments on declarations and includes, short English block comments, no Doxygen. Use whenever adding, rewriting or reviewing comments in C/C++ code for the user, or when generating C++ code that needs comments.
---

# Tsukini comment style (C++)

Always in **English**, short, no final period, never Doxygen (`@brief`/`@param`/`///` are never used,
the only `@` tags are `@date`/`@author`/`@file` of the file header).
Comment the **why** and the non-obvious behavior, never paraphrase the code.

## 1. Structure comments (exact strings)
Section separator (64 `-`), then the section title in upper case:
```cpp
    //----------------------------------------------------------------//
    /* INCLUDE */
```
Titles: `INCLUDE`, `DEFINE`, `MACRO`, `TYPEDEF`, `TYPE`, `ENUM`, `STRUCT`, `PROTOTYPE`, `CLASS`, `MIGRATION`.
Indented by 4 inside the include guard (INCLUDE/DEFINE), at column 0 after `namespace x {`.

Class blocks (exact strings, aligned like this):
```cpp
        // ---------- Pre-Function -------- //
        // ------------ Function ---------- //
        // ------------ Operator ---------- //
        // ---------- Constructor --------- //
        // ----------- Destructor --------- //
```
A section separator or class block is only written when something is linked to it: never leave one empty.

Group labels, lower case, `/* ... */`, on their own line above the group:
`/* type */`, `/* setup */`, `/* setter */`, `/* getter */`, `/* raw */`, `/* parsing */`, `/* tools */`,
`/* limits */`, `/* sub parsing */`, `/* hook handling */`, `/* destruction */`...
Sub-group in the code of the `.cpp` with a plain line comment: `// Long`, `// Short & Flag`.

Namespace delimiters: `namespace utils::system { // namespace start` ... `} // namespace end`.
Guard end: `#endif /* NAME_H */`.

## 2. Trailing comments (the most used)
On declarations, to give the meaning, the default, the unit or the constraint:
```cpp
int _fd = -1; // -1 = closed, can exec connect or listen
bool _mode = false; // true: server | false: client
#define SOCKET_TIMEOUT 7 // Timeout in seconds
void cancel(std::size_t id); // cancel given task
virtual void reset(void) = 0; // reset fd (DOES NOT CLOSE!!!)
std::vector<std::tuple<std::string, bool, std::vector<std::string>>> arguments; // <id, option(true)|flag(false), {option}>
template<bool force = false> // Can't override an exiting one by default, throw of error
```
- Tuples / pairs described with `<field, field>`, alternatives with `|`, mappings with `->`,
  values with `=` (`0 = unlimited`, `-1 = every fd`), precisions in `(...)`.
- Emphasis on a dangerous behavior in upper case + `!!!` or `(only!)`.
- Includes: every include of a header has an **aligned** trailing comment listing what is used:
  ```cpp
  #include "../attribute/Attribute.hpp"   // _hot, _cold, _unlikely, _likely, _nodiscard
  #include <mutex>                        // std::mutex
  ```
  Use `utils::exception::* (Type)` when many symbols come from the same header.
  `.cpp` includes have no comment.
- In code, at the end of a line for a precision: `if (...) return; // called by the task itself: nothing to cancel`.

## 3. Line comments above a block
```cpp
    // Get a new id
    std::size_t id = this->_idHandler.allocate();

    // Setup the new thread
    ...

    // Only if the id wasn't already free
    if (!this->_freeIds.insert(id).second) _unlikely {
```
- Start with an upper case letter, no final period, one line, describe the intent of the block.
- `word: precision` form for a nuance: `// Not an error: only this usage is invalid, the other usages can still accept the flag`.
- Tiny labels in lower case are fine: `// child`, `// parent`, `// left`.

## 4. Multi-line comments (rare)
For a note, a legend or a kept formula:
```cpp
/*
 * DEFAULT -> Basic term
 * TERM1   -> Advanced term
*/
    /* --- Different Messages ---
     * legacy -> Only kept for backward compatibility; no removal in sight ._.
     * migration -> Retained for backward compatibility; will be removed in a future version
    */
```
`/*` alone (or with a `--- Title ---`), lines starting with ` * `, closing `*/` aligned on the `/*`.

## 5. Misc
- Empty body: `/* Nothing */`.
- Disabled code kept with `//` and no space (`//add_library(${TARGET} SHARED ${SRC})`).
- No `TODO`/`FIXME` tags in the code: describe the limitation in a normal comment instead.
- No decorative boxes or banners except the file header and the separators above.

## Checking (xstyle)
When `xstyle` is installed (`command -v xstyle`), check what was written or reviewed with it instead of re-reading the
rules by hand: `xstyle --rtk <files|dirs> [-r]` (**always `--rtk`**: compact output made for the assistant),
`xstyle --rtk --fix [CODES] <paths>` for the fixable ones (`-n` to preview), `-c CPP-DOXYGEN,CPP-INCLUDE-COMMENT,CPP-COMMENT-ALIGN,CPP-EMPTY-SECTION,CPP-ORPHAN-GROUP,CPP-NAMESPACE-COMMENT,G-TODO` for the rules of this
skill, `xstyle -x CODE` to explain one. Not installed: apply the rules by hand and say once that
`curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- install xstyle` (no curl: `wget -qO-` instead of `curl -fsSL`) would check them. Its findings are heuristic: the local style of a file wins.
