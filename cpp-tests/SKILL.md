---
name: cpp-tests
description: Set up and write the unit tests of a C++ project the way libutils does it (GoogleTest) - tests/CMakeLists.txt with BUILD_TESTS + gtest_discover_tests, unit_tests binary, one test file per module mirroring the tree, nominal/edge/error cases with libutils exception codes, sub-process isolation with timeout for threads/IPC, fixtures, parametrized cases, mocks of the interfaces, known bugs kept as failing tests, CI workflow, and an inventory script of what is not tested yet. Use whenever the user wants to add, write, complete, run or set up unit tests (GTest) for C++ code, or "test the whole project".
---

# C++ unit tests (GoogleTest, libutils way)

Reference: `~/personal_delivery/cpp/libutils/tests/` (whole library) and `~/personal_delivery/cpp/context-forge/tests/`
(application: parent sources reused, mocks, plugin path). `SKILL_DIR` = directory of this file.
Code style of the tests: `cpp-style` + `cpp-comments`, file header with `cpp-class/scripts/header.py`.

## 1. Setup (once per project)
- `tests/CMakeLists.txt` from `cmake-style/templates/tests/CMakeLists.txt`:
  `set(TARGET unit_tests)`, `CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}` (binary at the root),
  `find_package(GTest REQUIRED)`, explicit `SRC` grouped like the tree, `include(GoogleTest)` + `gtest_discover_tests`,
  `target_compile_options(... -g -ggdb3)`.
  - library: link the library target (`target_link_libraries(${TARGET} PRIVATE <lib> GTest::gtest GTest::gtest_main)`);
  - executable: reuse the parent `SRC` without `main.cpp` (`list(REMOVE_ITEM ...)` + `list(TRANSFORM ... PREPEND ${CMAKE_SOURCE_DIR}/)`),
    same includes/links/`add_dependencies` (exception header) as the main target;
  - runtime paths needed by the tests (plugins, data): `target_compile_definitions(${TARGET} PRIVATE TESTS_X_DIR="${CMAKE_SOURCE_DIR}/x")`
    and `#error` in the test if it is missing.
- Root CMake: `option(BUILD_TESTS "Build unit tests" OFF)` + `if(BUILD_TESTS) enable_testing() add_subdirectory(tests) endif()`.
- `.gitignore`: `python3 ~/.claude/skills/cpp-project/scripts/update_gitignore.py .` (adds `/unit_tests`).
- CI: `cpp-project/templates/workflows/unit-tests.yml` (Debug + `BUILD_TESTS=ON`, checks `./unit_tests`, `ctest --output-on-failure --timeout 30`).
- Helpers shared by the tests in `tests/tools/` (namespace `tests::tools`): `templates/tools/TempDir.hpp`
  (temporary directory + `ScopedEnv` for environment variables), `templates/tools/StepSynchronizer.hpp` (order the
  steps of several threads), mocks `tests/tools/Mock<Interface>.hpp`.

## 2. Plan: test the whole project
```bash
python3 SKILL_DIR/scripts/untested.py <root> [--methods] [--all]
```
Inventory of the public classes / structs / enums / free functions (and methods) of `include/`, per module, with the
names never referenced by `tests/`. It is an inventory, not a coverage metric: read the headers before writing tests.
Then one test file per module, mirroring the tree: `include/utils/system/Scheduler.hpp` -> `tests/system/Scheduler.cpp`
(a file can group close classes of a module: `Scheduler.cpp` covers `Scheduler`, `LoadBalancer`, `IdHandler`).
For a big project, list the files to create and the main cases per class to the user before writing them.

## 3. Write the tests (`templates/Module.cpp`)
- Header: `File Description: ##  Unit tests of the <Class>, the <Other> & ...`; includes: the project umbrella
  (`"utils.hpp"`) or the header under test, `<gtest/gtest.h>`, the STL.
- One block per class: `/* ------------------------------- Scheduler ------------------------------- */`.
- Names: suite = class name (`TEST(Scheduler, CancelOne)`), case = PascalCase behavior (`ExecuteAfterDelay`,
  `CancelUnknown`, `DestructionCancelPendingTasks`); fixtures `<Class>Test`, parametrized `<Class><Aspect>` with a
  `struct <Class>Case` + `operator<<` (readable name) + `INSTANTIATE_TEST_SUITE_P(Cases, ...)`.
- `TEST(...) {` brace on the same line; static helpers (`waitFor(condition, timeout)`, `<Name>Scenario(...)`) brace on
  their own line; `using namespace std::chrono_literals;` is accepted in tests only (time literals).
- For every public class / function, cover:
  1. default state / construction;
  2. nominal behavior (each public method);
  3. edge cases (empty, 0, max, overflow, unicode, separators, reentrancy);
  4. errors: the exact exception code
     `try { ...; FAIL() << "Expected an exception"; } catch (const utils::exception::IException& e) {EXPECT_EQ(e.getCode(), utils::exception::InternalCode::UnknownId);}`
     (`EXPECT_THROW` only when the type is enough);
  5. threads / timing: `waitFor` with a timeout instead of fixed sleeps, `std::atomic` flags, `StepSynchronizer`;
  6. anything that can hang or crash (IPC, sockets, destruction while working, `fork`): sub-process with a timeout
     `#define ISOLATED(...) EXPECT_EXIT({::alarm(10); __VA_ARGS__; std::exit(::testing::Test::HasFailure() ? 1 : 0);}, ::testing::ExitedWithCode(0), "")`
     around a static `<Name>Scenario` function;
  7. global state (verbose level, env, singletons): set and restore it in the fixture `SetUp`/`TearDown`;
  8. interfaces: test the users of `IX` with a `MockX` (records the calls in a journal, configurable result).
- **The tests describe the expected behavior.** A test that reveals a bug stays failing (comment
  `// Expected behavior: fails until <bug> is fixed`), the library code is never changed to make a test pass unless
  the user asks; code that can't even be instantiated is reported by a failing test, not by breaking the build.
  List the failing tests (= bugs found) in the report.

## 4. Run
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON && cmake --build build --parallel $(nproc)
ctest --test-dir build --output-on-failure --timeout 30      # or ./unit_tests --gtest_filter='Scheduler.*'
```
- Asan pass for memory errors: `-DCMAKE_BUILD_TYPE=Asan` (needs `libutils-as[-pre]` when libutils is used).
- Optional coverage: `-DCMAKE_CXX_FLAGS="--coverage"` then `gcovr -r . --exclude tests` (or `llvm-cov` with clang).
- Report: tests added per module, results (passed / failing = bugs found, with the reason), what remains untested
  Deliver the report through the `pdf-report` skill (ask the output format of `pdf-report` (`Markdown + PDF` recommended, PDF only, Markdown only)), in English, in `audit/` at the root of the repository (or of the current folder outside a repository), never `docs/` unless asked (`audit/<YYYY-MM-DD>-tests.*`), not only in the chat.
  (`untested.py`).
