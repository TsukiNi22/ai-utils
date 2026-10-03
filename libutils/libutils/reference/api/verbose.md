# libutils `verbose`

Generated from libutils `v2.14.0` (commit `7506acd`, 2026-10-01) by `scripts/gen_api.py`, do not edit by hand.

## `utils/verbose/Verbose.hpp`

Marco & Define used for verbose usage

Namespace: `utils::verbose`

```cpp
#define set_verbose(v) {utils::verbose::verbose = utils::verbose::Verbose::v;}
#define LOCK_OUTPUT std::lock_guard<std::mutex> lock_(utils::verbose::output_lock)
#define onBasicVerbose(info) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(utils::verbose::Verbose::Basic))    std::cout << info << std::endl;}
#define onAdvancedVerbose(info) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(utils::verbose::Verbose::Advanced)) std::cout << info << std::endl;}
#define onDebugVerbose(info) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(utils::verbose::Verbose::Debug))    std::cout << "debug: " << info << std::endl;}
#define onVerbose(level, info) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(level))                             std::cout << info << std::endl;}
#define onBasicVerboseC(output, info) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(utils::verbose::Verbose::Basic))    output << info << std::endl;}
#define onAdvancedVerboseC(output, info) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(utils::verbose::Verbose::Advanced)) output << info << std::endl;}
#define onDebugVerboseC(output, info) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(utils::verbose::Verbose::Debug))    output << "debug: " << info << std::endl;}
#define onVerboseC(output, level, info) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(level))                             output << info << std::endl;}
#define onBasicVerboseFn(fn) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(utils::verbose::Verbose::Basic))    {fn}}
#define onAdvancedVerboseFn(fn) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(utils::verbose::Verbose::Advanced)) {fn}}
#define onDebugVerboseFn(fn) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(utils::verbose::Verbose::Debug))    {fn}}
#define onVerboseFn(level, fn) {LOCK_OUTPUT; if (static_cast<std::size_t>(utils::verbose::verbose) >= static_cast<std::size_t>(level))                             {fn}}

// namespace utils::verbose
enum class Verbose: std::size_t {None, Basic, Advanced, Debug}
extern std::mutex output_lock;
extern volatile utils::verbose::Verbose verbose;
```
