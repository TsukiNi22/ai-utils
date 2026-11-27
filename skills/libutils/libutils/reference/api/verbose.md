# libutils `verbose`

Generated from libutils `v5.0.0` (commit `c129a5e`, 2026-11-27) by `scripts/gen_api.py`, do not edit by hand.

## `utils/verbose/CApi.h`

C API of utils::verbose (level of the global verbose, printf style logs on the locked output) (C & C++ compatible) The logs go through std::cout like the c++ macros (onVerbose, ...), so the C & C++ logs stay in order

```c
// global
typedef int (*utils_verbose_fn_t)(void* data); // UTILS_OK / UTILS_KO (info: utils_fail / utils_exception_set)
typedef enum utils_verbose { C_API_VERBOSE_NONE = 0, C_API_VERBOSE_BASIC, C_API_VERBOSE_ADVANCED, C_API_VERBOSE_DEBUG };
void utils_verbose_set(utils_verbose_t level);
utils_verbose_t utils_verbose_get(void);
bool utils_verbose_is(utils_verbose_t level); // true when the level is displayed (level <= the global one)
int utils_verbose_log(utils_verbose_t level, const char* fmt, ...);
int utils_verbose_vlog(utils_verbose_t level, const char* fmt, va_list args);
int utils_verbose_log_to(FILE* output, utils_verbose_t level, const char* fmt, ...);
int utils_verbose_vlog_to(FILE* output, utils_verbose_t level, const char* fmt, va_list args);
int utils_verbose_locked(utils_verbose_fn_t fn, void* data);
int utils_verbose_on(utils_verbose_t level, utils_verbose_fn_t fn, void* data); // only when the level is displayed (like onVerboseFn), UTILS_OK when not run
```

## `utils/verbose/Verbose.hpp`

Marco & Define used for verbose usage

Namespace: `utils::verbose`

```cpp
#define set_verbose(v) {utils::verbose::verbose = utils::verbose::Verbose::v;}
#define LOCK_OUTPUT std::lock_guard<std::recursive_mutex> lock_(utils::verbose::output_lock)
#define onBasicVerbose(info) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(utils::verbose::Verbose::Basic))    std::cout << info << std::endl;});}
#define onAdvancedVerbose(info) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(utils::verbose::Verbose::Advanced)) std::cout << info << std::endl;});}
#define onDebugVerbose(info) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(utils::verbose::Verbose::Debug))    std::cout << "debug: " << info << std::endl;});}
#define onVerbose(level, info) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(level))                             std::cout << info << std::endl;});}
#define onBasicVerboseC(output, info) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(utils::verbose::Verbose::Basic))    output << info << std::endl;});}
#define onAdvancedVerboseC(output, info) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(utils::verbose::Verbose::Advanced)) output << info << std::endl;});}
#define onDebugVerboseC(output, info) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(utils::verbose::Verbose::Debug))    output << "debug: " << info << std::endl;});}
#define onVerboseC(output, level, info) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(level))                             output << info << std::endl;});}
#define onBasicVerboseFn(fn) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(utils::verbose::Verbose::Basic))    {fn}});}
#define onAdvancedVerboseFn(fn) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(utils::verbose::Verbose::Advanced)) {fn}});}
#define onDebugVerboseFn(fn) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(utils::verbose::Verbose::Debug))    {fn}});}
#define onVerboseFn(level, fn) {utils::verbose::locked([&](void) {if (static_cast<std::size_t>(utils::verbose::verbose.load(std::memory_order_relaxed)) >= static_cast<std::size_t>(level))                             {fn}});}

// namespace utils::verbose
enum class Verbose: std::size_t {None, Basic, Advanced, Debug}
extern std::recursive_mutex output_lock;
template<typename Fn> void locked(Fn&& fn);
extern std::atomic<utils::verbose::Verbose> verbose; // atomic: read by every thread that logs
```
