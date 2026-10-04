---
name: cpp-style
description: Tsukini's personal C++ coding style (from libutils) - naming (PascalCase types, camelCase methods, _member, snake_case tools, UPPER macros), 4-space indentation, brace placement, spacing, one-liners, loops, switch, lambdas, const correctness, casts, attributes. Use whenever writing, editing, refactoring or reviewing C++ code (.cpp/.hpp) for the user, so the code looks like theirs.
---

# Tsukini C++ coding style

Extracted from the recent libutils code (C++20, clang++, `-W -Wall -Wextra -Wpedantic -Wshadow`).
When editing an existing file, its local style wins over this document.
File/class layout (header, sections, blocks): the `cpp-class` skill. Comments: the `cpp-comments` skill.

## Naming
| Element | Style | Examples |
|---|---|---|
| class / struct / enum / alias | PascalCase | `IdHandler`, `ParsedUsage`, `ParsedUsages` |
| interface / abstract | `I` / `A` prefix | `ISocket`, `ASocket` -> `TCPSocket` |
| method | camelCase, verb first | `setPayloadSeparator`, `getFd`, `hasRecvOverload`, `resetUsages` |
| private / protected / internal function (wrapper impl included) | `<name>(<Name>)*_`: camelCase + trailing `_` | `allocate_`, `cancel_`, `computeHash_`, `readChunkHeader_` |
| free tool function | snake_case | `is_ip`, `resolve_hostname`, `rotate_point_3D` |
| hook / callback function | camelCase | `defaultHelpHook`, `defaultInt32ParsingHook` |
| member | `_camelCase` | `_usedIds`, `_helpHook`, `_fd` |
| local variable / parameter | camelCase (bool included), short names fine in small scopes (`i`, `j`, `f`, `it`, `e`, `s`) | `alreadyFailed`, `equalFound`, `argOrigin` |
| special bool parameter (a mode / switch with a default value) | snake_case, **only** for these | `const bool safe_mode = true`, `const bool ignore_case = false`, `failsafe` |
| macro / define | UPPER_SNAKE | `SOCKET_CHUNK_SIZE`, `OVERFLOW_LIMIT` |
| attribute macro (libutils) | `_lower` | `_hot`, `_nodiscard`, `_migration(4, 0, 0)` |
| enum value | PascalCase, trailing comma | `Status::Up`, `InternalCode::UnknownId` |
| template parameter | `T`, `Fn`, `<Name>T` | `template<typename T>`, `ByteT` |
| namespace | lowercase, nested with `::` | `utils::network`, `utils::system` |
| file | class name, or lowercase for a free-function module | `IdHandler.hpp`, `format.hpp` |

Getters `getX(void) const`, setters `setX(...)`, boolean queries `isX` / `hasX`, removal `removeX`,
`resetX` to restore the default. Plural for collections (`_options`, `removeFlags`).

## Indentation & braces
- 4 spaces, never tabs. No trailing whitespace. Lines can be long (no hard limit), but split
  conditions/arguments that become unreadable.
- `public:`/`private:`/`protected:` indented by 4 inside the class, members by 8.
- Out-of-line function / method / constructor: opening brace **on its own line**.
- Class, struct, enum, namespace, `if`, `else`, `for`, `while`, `switch`, `do`, lambda: brace **on the same line**.
- `} else {` / `} else if (...) {` on the closing brace line.
- Namespace content is **not** indented.
- Preprocessor nested inside a guard is indented (`    #define X`, `    #include ...`, `        #define` in nested `#if`).

## Spacing
- `if (`, `for (`, `while (`, `switch (`: space after the keyword; no space inside the parentheses.
- Binary operators surrounded by spaces, no space for unary (`!x`, `++i`, `*it`).
- `for (const std::string& x: list)`, `class A: public B`, `enum class E: std::size_t`: **no space before `:`**.
- Init list on its own line, `: ` at column 0: `: _binary{binary}, _description{description}`.
- `&`/`*` glued to the type: `const std::string& name`, `char* buf`, `auto &[a, b]` for structured bindings.
- `template<typename T>` without space. `(void)` for an empty parameter list, always.
- One-liners: no space inside the braces and a `;` after: `{return this->_fd;};`, `{this->_help = false;};`.
- Consecutive similar lines are aligned in columns (overloads, getters, `using`, `case`, include comments):
  ```cpp
  _hot _nodiscard bool hasAcceptOverload(void) const override {return false;};
  _hot _nodiscard bool hasRecvOverload(void) const override   {return false;};
  ```
- One-liner functions of a group are aligned **tight**: the `{` of every body sits on the column right
  after the longest signature of the group + **one space**, never a wider fixed column:
  ```cpp
  _cold _nodiscard bool hasWindow(void) const              {return this->_window != nullptr;};
  _cold _nodiscard rtype::engine::IWindow& getWindow(void) {return *this->_window;};
  _cold _nodiscard rtype::engine::Layout& getRoot(void)    {return this->_root;};
  ```
  Include comments keep their own alignment (see `cpp-comments`), this rule is only for functions.
- One empty line between functions and between logical blocks; never two.

## Statements
- `this->` on **every** member access (members and methods).
- Short `if`/`else`/loop with one statement: on the same line or on the next indented line without braces:
  ```cpp
  if (safe_mode) lock.lock();
  else (void)lock.try_lock();

  for (const std::string& id: ids)
      this->removeUsage(id);
  ```
  Several statements on one line inside braces are fine for tiny cases: `{ids.push_back(fid); unknown = false; break;}`.
- Early return for trivial cases: `if (this->_finished.empty()) return;`.
- Loops:
  - range-for first, with the explicit type (`for (const std::string& id: ids)`),
    structured bindings `for (const auto &[fid, flag]: this->_flags)` (the only syntax possible) with `_` for unused parts;
  - index loops `for (std::size_t i = 0; i < n; ++i)` (prefer `++i`, `std::size_t` for indexes);
  - `while (true)` + `break` with a comment per exit condition;
  - `do { ... } while (cond);` when the first iteration is mandatory;
  - empty-body search loop is accepted: `for (index = 0; index < n && cond; ++index);`.
- `switch`: one line per `case` with `break` aligned when short:
  ```cpp
  switch (cb & 0b11) {
      case 0: event.button = utils::iomanip::MouseButton::Left;     break;
      case 1: event.button = utils::iomanip::MouseButton::Middle;   break;
      default: event.button = utils::iomanip::MouseButton::Unknown; break;
  }
  ```
- Lambdas: `[&](const std::pair<std::string, bool>& p) {return p.first == id;}` (explicit parameter types), captures listed explicitly when stored
  (`[this, id, delay, fn](std::stop_token stoken) {`).
- Ternaries for short choices, parenthesized when nested.
- Discarded return values are cast: `(void)lock.try_lock();`.

## Types & C++ usage
- Fully qualified names, never `using namespace` (`std::size_t`, `utils::exception::ErrorException`).
- **No `auto`**: always write the type, even when it is long. Allowed only for an iterator
  (`auto it = this->_tasks.find(id);`), the structured bindings (`const auto &[a, b]`, mandatory syntax), or when
  the user explicitly asks for it.
- **No `using`** except to name a **custom type** of the project (`using Payloads = std::vector<utils::network::Payload>;`,
  `using ParsedUsages = ...;`, migration aliases), or when the user explicitly asks for it. Never `using std::...;`,
  never a `using` to shorten a standard type in the code.
- `const` everywhere it applies: by-value parameters too (`const bool safe_mode`, `const std::size_t i`),
  `const&` for objects, `const` methods, `mutable` for the mutex.
- Default values at the declaration (`int _fd = -1;`, `std::string _description = "...";`), `"[None]"` as
  default text.
- `nullptr` (never `NULL`), `static_cast` (never C casts except `(void)`), `std::size_t` for sizes.
- Modern C++20: `contains`, `starts_with`, `std::erase`, `std::jthread` + `std::stop_token`,
  structured bindings, `if constexpr`, `static_assert` + type traits / concepts, `std::optional`, `enum class`.
- Return `std::optional<std::string>` from check hooks: `std::nullopt` = valid, the string = error message.
- Templates with a `bool` non-type parameter for behavior switches: `template<bool force = false>`.
- `noexcept` on `close`, destructors and functions that must not throw. `final`/`override` on every override.
- Locks: `std::lock_guard lock(this->_lock);`, `std::unique_lock<std::mutex> lock(this->_lock, std::defer_lock);`.
- Rule of five explicit (`= default` / `= delete`), copy/move deleted by default.

## Errors
- With libutils: `throw utils::exception::ErrorException(utils::exception::InternalCode::X, "info");`
  (`FatalException` unrecoverable, `WarningException(...).formated()` printed on `std::cerr` for warnings,
  `failsafe` parameter to switch between warning and throw).
- Without libutils: standard exceptions (`std::runtime_error`, `std::invalid_argument`...) or the project ones.
- Re-throw hook/callback errors wrapped with context:
  `catch (const std::exception& e) {throw utils::exception::ErrorException(InternalCode::ArgParserHook, e.what());}`.

## Attributes
Every attribute / alignment / hint goes through the libutils macros when the project uses libutils
(`cpp-class/scripts/detect_libutils.sh`), alignment included:
`_alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> _head = 0;`, never a bare
`alignas(...)` (`<utils/utils.hpp>` also guarantees `std::hardware_*_interference_size`, fallback 64).

Lookup order before writing any attribute:
1. the libutils default macros (table below, `libutils/reference/api/attribute.md`);
2. not there: the attribute files really used, hard imported in the project or installed on the system
   (`bash cpp-class/scripts/list_attributes.sh <project_root>`, `cpp-class` = that skill folder, lists every `#define _x`), a
   newer libutils may have it;
3. still not there (or no libutils): the standard form, `[[...]]` (`[[gnu::...]]`), `alignas(...)`, or
   `__attribute__((...))` / `__builtin_*` when no standard spelling exists.


| libutils | standard |
|---|---|
| `_hot` / `_cold` | `[[gnu::hot]]` / `[[gnu::cold]]` |
| `_nodiscard` | `[[nodiscard]]` |
| `_unused` | `[[maybe_unused]]` |
| `_likely` / `_unlikely` | `[[likely]]` / `[[unlikely]]` |
| `_fallthrough` | `[[fallthrough]]` |
| `_noinline` | `[[noinline]]` (`[[gnu::noinline]]` for gcc) |
| `_deprecated(info)` | `[[deprecated(info)]]` |
| `_noaddress` / `_packed` | `[[no_unique_address]]` / `[[gnu::packed]]` |
| `_assume(expr)` | `[[assume(expr)]]` |
| `_alignas(n)` | `alignas(n)` (`n` often `std::hardware_destructive_interference_size`) |
| `_hidden` | `[[gnu::visibility("hidden")]]` |
| `_ctor` / `_dtor` | `[[gnu::constructor]]` / `[[gnu::destructor]]` |
| `_likely_c(c)` / `_unlikely_c(c)` / `_expect(c, v)` | `__builtin_expect(!!(c), 1)` / `(!!(c), 0)` / `(c, v)` |
| `_alloc_size(i)` / `_alloc_size_mul(i, j)` | `[[gnu::alloc_size(i)]]` / `[[gnu::alloc_size(i, j)]]` |
| `_read_only(p, s)` / `_write_only(p, s)` | `[[gnu::access(read_only, p, s)]]` / `[[gnu::access(write_only, p, s)]]` |
| `_nonnull(p)` | `[[gnu::nonnull(p)]]` |
| `_legacy` / `_migration(x, y, z)` | `[[deprecated("...")]]` |

- Placement: before the return type (`_cold _nodiscard int getFd(void) const`), `_likely`/`_unlikely`
  after the condition (`if (id == 0) _unlikely {`, `} else _likely {`), `_unused` before the parameter type.
- `_hot` on hot paths (alloc, send, parse, compute), `_cold` on setup/setters/getters/cancel,
  `_nodiscard` on every getter and function whose result matters.
