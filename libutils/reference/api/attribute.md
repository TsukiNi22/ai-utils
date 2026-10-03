# libutils `attribute`

Generated from libutils `v2.14.0` (commit `7506acd`, 2026-10-01) by `scripts/gen_api.py`, do not edit by hand.

## `utils/attribute/Attribute.hpp`

Include the right file from the c++ version used or selected

Namespace: `std`

```cpp
#define _legacy [[deprecated("Only kept for backward compatibility; no removal in sight ._.")]]
#define _migration(major, minor, fix) [[deprecated("Retained for backward compatibility; will be removed in a future version (estimated: ~v" #major "." #minor "." #fix ")")]]
#define _legacy
#define _migration(major, minor, fix)

// namespace std
constexpr std::size_t hardware_destructive_interference_size = 64;
constexpr std::size_t hardware_constructive_interference_size = 64;
```

## `utils/attribute/c++14.hpp`

Different attribute used for optimisation & other thing Version for c++14 and above

```cpp
#define _nodiscard __attribute__((warn_unused_result)) // Warn for unused return
#define _noinline __attribute__((noinline)) // Cancel any auto inline from the compiler
#define _unused __attribute__((unused)) // Signal an unused variable
#define _hidden __attribute__((visibility("hidden"))) // Change the visibility on a shared lib
#define _ctor __attribute__((constructor)) // Execute before the main
#define _dtor __attribute__((destructor)) // Execute after the main
#define _fallthrough __attribute__((fallthrough)) // Ingore warn for no break in switch
#define _deprecated(info) [[deprecated(info)]] // Signal a deprecated function
#define _deprecated(info) // Not defined with this flag
#define _likely // Not defined in this version
#define _unlikely // Not defined in this version
#define _likely_c(c) __builtin_expect(!!(c), 1) // Signal a condition that has a bigger probability of appening
#define _unlikely_c(c) __builtin_expect(!!(c), 0) // Signal a condition that has a smallest probability of appening
#define _expect(c, v) __builtin_expect(c, v) // Signal a condition that has a high probability of having the given value
#define _assume(expr) __builtin_assume(expr) // Assume a given expr as true
#define _cold __attribute__((cold)) // Signal a function that has a small number of use
#define _hot __attribute__((hot)) // Signal a function that has a huge number of use
#define _noaddress // Not defined in this version
#define _packed __attribute__((packed)) // Remove the memory padding in a struct
#define _alignas(n) alignas(n) // Set the memory padding in a struct
#define _alloc_size(i_size) __attribute__((alloc_size(i_size))) // Signal a malloc of the given size in the argument
#define _alloc_size_mul(i_mul, i_size) __attribute__((alloc_size(i_mul, i_size))) // Signal a malloc of the given size in the argument multiply by another argument
#define _write_only(i_ptr, i_size) __attribute__((access(write_only, i_ptr, i_size))) // Set the mode of a ptr in the argument so that it can only be writed in the limit of the size
#define _read_only(i_ptr, i_size) __attribute__((access(read_only, i_ptr, i_size))) // Set the mode of a ptr in the argument so that it can only be readed in the limit of the size
#define _nonnull(i_ptr) __attribute__((nonnull(i_ptr))) // Warn on given null value on the given argument

```

## `utils/attribute/c++17.hpp`

Different attribute used for optimisation & other thing Version for c++17 and above

```cpp
#define _nodiscard [[nodiscard]] // Warn for unused return
#define _noinline [[noinline]] // Cancel any auto inline from the compiler
#define _unused [[maybe_unused]] // Signal an unused variable
#define _hidden [[gnu::visibility("hidden")]] // Change the visibility on a shared lib
#define _ctor [[gnu::constructor]] // Execute before the main
#define _dtor [[gnu::destructor]] // Execute after the main
#define _fallthrough [[fallthrough]] // Ingore warn for no break in switch
#define _deprecated(info) [[deprecated(info)]] // Signal a deprecated function
#define _deprecated(info) // Not defined with this flag
#define _likely // Not defined in this version
#define _unlikely // Not defined in this version
#define _likely_c(c) __builtin_expect(!!(c), 1) // Signal a condition that has a bigger probability of appening
#define _unlikely_c(c) __builtin_expect(!!(c), 0) // Signal a condition that has a smallest probability of appening
#define _expect(c, v) __builtin_expect(c, v) // Signal a condition that has a high probability of having the given value
#define _assume(expr) __builtin_assume(expr) // Assume a given expr as true
#define _cold [[gnu::cold]] // Signal a function that has a small number of use
#define _hot [[gnu::hot]] // Signal a function that has a huge number of use
#define _noaddress // Not defined in this version
#define _packed [[gnu::packed]] // Remove the memory padding in a struct
#define _alignas(n) alignas(n) // Set the memory padding in a struct
#define _alloc_size(i_size) [[gnu::alloc_size(i_size)]] // Signal a malloc of the given size in the argument
#define _alloc_size_mul(i_mul, i_size) [[gnu::alloc_size(i_mul, i_size)]] // Signal a malloc of the given size in the argument multiply by another argument
#define _write_only(i_ptr, i_size) [[gnu::access(write_only, i_ptr, i_size)]] // Set the mode of a ptr in the argument so that it can only be writed in the limit of the size
#define _read_only(i_ptr, i_size) [[gnu::access(read_only, i_ptr, i_size)]] // Set the mode of a ptr in the argument so that it can only be readed in the limit of the size
#define _nonnull(i_ptr) [[gnu::nonnull(i_ptr)]] // Warn on given null value on the given argument

```

## `utils/attribute/c++20.hpp`

Different attribute used for optimisation & other thing Version for c++20 and above

```cpp
#define _nodiscard [[nodiscard]] // Warn for unused return
#define _noinline [[noinline]] // Cancel any auto inline from the compiler
#define _unused [[maybe_unused]] // Signal an unused variable
#define _hidden [[gnu::visibility("hidden")]] // Change the visibility on a shared lib
#define _ctor [[gnu::constructor]] // Execute before the main
#define _dtor [[gnu::destructor]] // Execute after the main
#define _fallthrough [[fallthrough]] // Ingore warn for no break in switch
#define _deprecated(info) [[deprecated(info)]] // Signal a deprecated function
#define _deprecated(info) // Not defined with this flag
#define _likely [[likely]] // Signal a condition that has a bigger probability of appening
#define _unlikely [[unlikely]] // Signal a condition that has a smallest probability of appening
#define _likely_c(c) __builtin_expect(!!(c), 1) // Signal a condition that has a bigger probability of appening
#define _unlikely_c(c) __builtin_expect(!!(c), 0) // Signal a condition that has a smallest probability of appening
#define _expect(c, v) __builtin_expect(c, v) // Signal a condition that has a high probability of having the given value
#define _assume(expr) [[assume(expr)]] // Assume a given expr as true
#define _cold [[gnu::cold]] // Signal a function that has a small number of use
#define _hot [[gnu::hot]] // Signal a function that has a huge number of use
#define _noaddress [[no_unique_address]] // Remove unique address for elements
#define _packed [[gnu::packed]] // Remove the memory padding in a struct
#define _alignas(n) alignas(n) // Set the memory padding
#define _alloc_size(i_size) [[gnu::alloc_size(i_size)]] // Signal a malloc of the given size in the argument
#define _alloc_size_mul(i_mul, i_size) [[gnu::alloc_size(i_mul, i_size)]] // Signal a malloc of the given size in the argument multiply by another argument
#define _write_only(i_ptr, i_size) [[gnu::access(write_only, i_ptr, i_size)]] // Set the mode of a ptr in the argument so that it can only be writed in the limit of the size
#define _read_only(i_ptr, i_size) [[gnu::access(read_only, i_ptr, i_size)]] // Set the mode of a ptr in the argument so that it can only be readed in the limit of the size
#define _nonnull(i_ptr) [[gnu::nonnull(i_ptr)]] // Warn on given null value on the given argument

```

## `utils/attribute/fallback.hpp`

Different attribute used for optimisation & other thing Fallback if the version used is inferior to c++14

```cpp
#define _nodiscard
#define _noinline
#define _unused
#define _hidden
#define _ctor
#define _dtor
#define _deprecated(info)
#define _fallthrough
#define _likely
#define _unlikely
#define _likely_c(c) (c)
#define _unlikely_c(c) (c)
#define _expect(c, v) (c)
#define _assume(expr)
#define _cold
#define _hot
#define _noaddress
#define _packed
#define _alignas(n)
#define _alloc_size(i_size)
#define _alloc_size_mul(i_mul, i_size)
#define _write_only(i_ptr, i_size)
#define _read_only(i_ptr, i_size)
#define _nonnull(i_ptr)

```
