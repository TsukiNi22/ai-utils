# libutils `verbose`

Generated from libutils `v3.0.0` (commit `3b53ede`, 2026-10-05) by `scripts/gen_api.py`, do not edit by hand.

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
