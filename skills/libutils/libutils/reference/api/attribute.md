# libutils `attribute`

Generated from libutils `v5.0.0` (commit `6aac89f`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/attribute/Attribute.hpp`

Redirection for the c or c++ attribute

## `utils/attribute/c++.hpp`

Include the right file from the c++ version used or selected

```cpp
#define _legacy __attribute__((deprecated("Only kept for backward compatibility; no removal in sight ._.")))
#define _migration(major, minor, fix) __attribute__((deprecated("Retained for backward compatibility; will be removed in a future version (estimated: ~v" #major "." #minor "." #fix ")")))
#define _legacy [[deprecated("Only kept for backward compatibility; no removal in sight ._.")]]
#define _migration(major, minor, fix) [[deprecated("Retained for backward compatibility; will be removed in a future version (estimated: ~v" #major "." #minor "." #fix ")")]]
#define _legacy // Not defined with this flag / compiler
#define _migration(major, minor, fix) // Not defined with this flag / compiler

```

## `utils/attribute/c++11.hpp`

Different attribute used for optimisation & other thing Version for c++11 and above. Same macros as `utils/attribute/c++20.hpp` for another standard.

## `utils/attribute/c++14.hpp`

Different attribute used for optimisation & other thing Version for c++14 and above. Same macros as `utils/attribute/c++20.hpp` for another standard.

## `utils/attribute/c++17.hpp`

Different attribute used for optimisation & other thing Version for c++17 and above. Same macros as `utils/attribute/c++20.hpp` for another standard.

## `utils/attribute/c++20.hpp`

Different attribute used for optimisation & other thing Version for c++20 and above

```cpp
#define _nodiscard [[nodiscard]] // Warn for unused return
#define _unused [[maybe_unused]] // Signal an unused variable
#define _noreturn [[noreturn]] // Signal a function that never return
#define _fallthrough [[fallthrough]] // Ingore warn for no break in switch
#define _deprecated(info) [[deprecated(info)]] // Signal a deprecated function
#define _deprecated(info) // Not defined with this flag
#define _hidden // No ELF visibility on Windows (dllexport/dllimport model)
#define _export __declspec(dllexport) // Export a symbol from the dll (C API)
#define _import __declspec(dllimport) // Import a symbol from the dll (C API)
#define _hidden [[gnu::visibility("hidden")]] // Change the visibility on a shared lib
#define _export [[gnu::visibility("default")]] // Export a symbol from the shared lib (C API)
#define _import // Nothing to import with the ELF visibility model
#define _ctor [[gnu::constructor]] // Execute before the main
#define _dtor [[gnu::destructor]] // Execute after the main
#define _cleanup(fn) [[gnu::cleanup(fn)]] // Call fn(&var) when the variable goes out of scope
#define _always_inline [[gnu::always_inline]] // Always inline the function (with inline)
#define _noinline [[gnu::noinline]] // Cancel any auto inline from the compiler
#define _flatten [[gnu::flatten]] // Inline every call made inside the function
#define _hot [[gnu::hot]] // Signal a function that has a huge number of use
#define _cold [[gnu::cold]] // Signal a function that has a small number of use
#define _const [[gnu::const]] // Function without side effect that only reads its arguments (no memory)
#define _pure [[gnu::pure]] // Function without side effect that can read the memory
#define _assume(expr) __builtin_assume(expr)
#define _assume(expr) __attribute__((assume(expr)))
#define _assume(expr) ((expr) ? (void)0 : __builtin_unreachable()) // expr evaluated
#define _unreachable() __builtin_unreachable() // Signal a code that can never be reached
#define _likely [[likely]] // Signal a condition that has a bigger probability of appening
#define _unlikely [[unlikely]] // Signal a condition that has a smallest probability of appening
#define _likely_c(c) __builtin_expect(!!(c), 1) // Signal a condition that has a bigger probability of appening
#define _unlikely_c(c) __builtin_expect(!!(c), 0) // Signal a condition that has a smallest probability of appening
#define _expect(c, v) __builtin_expect(c, v) // Signal a condition that has a high probability of having the given value
#define _format(archetype, i_fmt, i_args) [[gnu::format(archetype, i_fmt, i_args)]] // Check the format string of the given argument (printf, scanf, ...) against the arguments from i_args
#define _sentinel [[gnu::sentinel]] // Warn when a variadic function is not ended by a NULL
#define _noaddress [[no_unique_address]] // Remove unique address for elements
#define _packed [[gnu::packed]] // Remove the memory padding in a struct
#define _malloc [[gnu::malloc]] // Signal a function that return a new memory (no alias)
#define _alloc_size(i_size) [[gnu::alloc_size(i_size)]] // Signal a malloc of the given size in the argument
#define _alloc_size_mul(i_mul, i_size) [[gnu::alloc_size(i_mul, i_size)]] // Signal a malloc of the given size in the argument multiply by another argument
#define _nonnull(i_ptr) [[gnu::nonnull(i_ptr)]] // Warn on given null value on the given argument
#define _returns_nonnull [[gnu::returns_nonnull]] // Signal a function that never return a null pointer
#define _read_only(i_ptr, i_size) // Not supported by clang
#define _write_only(i_ptr, i_size) // Not supported by clang
#define _read_only(i_ptr, i_size) [[gnu::access(read_only, i_ptr, i_size)]] // Set the mode of a ptr in the argument so that it can only be readed in the limit of the size
#define _write_only(i_ptr, i_size) [[gnu::access(write_only, i_ptr, i_size)]] // Set the mode of a ptr in the argument so that it can only be writed in the limit of the size

```

## `utils/attribute/c++23.hpp`

Different attribute used for optimisation & other thing Version for c++23 and above. Same macros as `utils/attribute/c++20.hpp` for another standard.

## `utils/attribute/c.h`

Include the right file from the c version used or selected (same macros as c++.hpp) Only included by Attribute.hpp in c (c++: c++.hpp)

```c
#define _legacy __attribute__((deprecated("Only kept for backward compatibility; no removal in sight ._.")))
#define _migration(major, minor, fix) __attribute__((deprecated("Retained for backward compatibility; will be removed in a future version (estimated: ~v" #major "." #minor "." #fix ")")))
#define _legacy // Not defined with this flag / compiler
#define _migration(major, minor, fix) // Not defined with this flag / compiler

```

## `utils/attribute/c11.h`

Different attribute used for optimisation & other thing Version for c11 and c17 (same macros as c++14.hpp). Same macros as `utils/attribute/c++20.hpp` for another standard.

## `utils/attribute/c23.h`

Different attribute used for optimisation & other thing Version for c23 and above (same macros as c++20.hpp). Same macros as `utils/attribute/c++20.hpp` for another standard.

## `utils/attribute/c99.h`

Different attribute used for optimisation & other thing Version for c99 (or gnu89) with a GNU compiler (same macros as c++11.hpp). Same macros as `utils/attribute/c++20.hpp` for another standard.

## `utils/attribute/common.h`

Keywords & values that are not attributes, common to every c version (same as common.hpp) (thread_local, restrict, alignas, hardware interference size)

```c
#define _thread_local thread_local
#define _thread_local _Thread_local
#define _thread_local __thread
#define _thread_local __declspec(thread)
#define _thread_local // Not available with this compiler: not thread local
#define _restrict restrict
#define _restrict __restrict__
#define _restrict __restrict
#define _restrict // Not available with this compiler
#define _alignas(n) alignas(n)
#define _alignas(n) _Alignas(n)
#define _alignas(n) __attribute__((aligned(n)))
#define _alignas(n) __declspec(align(n))
#define _alignas(n) // Not available with this compiler

// global
enum {hardware_destructive_interference_size, hardware_constructive_interference_size}
```

## `utils/attribute/common.hpp`

Keywords & values that are not attributes, common to every c++ version (thread_local, restrict, alignas, hardware interference size)

Namespace: `std`

```cpp
#define _thread_local thread_local
#define _thread_local __thread
#define _thread_local __declspec(thread)
#define _thread_local // Not available with this compiler: not thread local
#define _restrict __restrict__
#define _restrict __restrict
#define _restrict // Not available with this compiler
#define _alignas(n) alignas(n)
#define _alignas(n) __attribute__((aligned(n)))
#define _alignas(n) __declspec(align(n))
#define _alignas(n) // Not available with this compiler

// namespace std
constexpr std::size_t hardware_destructive_interference_size = 64; // xstyle: ignore CPP-NAMESPACE-INDENT (fallback in the include region)
constexpr std::size_t hardware_constructive_interference_size = 64; // xstyle: ignore CPP-NAMESPACE-INDENT
```

## `utils/attribute/fallback.hpp`

Different attribute used for optimisation & other thing Fallback without a GNU compiler or with a too old version (c & c++). Same macros as `utils/attribute/c++20.hpp` for another standard.
