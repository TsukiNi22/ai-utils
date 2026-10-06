---
name: coverage
description: Measure and raise the test coverage of a C / C++ project built the cpp-tests way (GoogleTest, tests/ + BUILD_TESTS, unit_tests) - instrumented build in a copy (clang source based coverage, llvm-profdata / llvm-cov), lines / functions / branches per file, the functions never called and the line ranges never run, then new tests written module by module (cpp-tests rules) and re-measured until the coverage stops growing, and at the end always the question of a report (report skill) on what is still missing and why. Use whenever the user asks for the coverage, what is not tested, to fill / raise / maximize the coverage, or for a coverage report.
---

# Coverage (cpp-tests projects)

`SKILL_DIR` = the directory of this file. Requires `cpp-tests` (the test setup and the rules for writing tests: load it),
`report` for the final report. Scripts: `scripts/coverage.sh` (build + run + llvm-cov), `scripts/uncovered.py` (what is
missing, as tables). Tools: `clang++`, `llvm-profdata`, `llvm-cov` (`llvm` package: ask before installing, like
`benchmark`). Nothing is written in the project except the new tests; the build happens in a `/tmp` copy.

## 1. Measure (baseline)
```bash
bash SKILL_DIR/scripts/coverage.sh <root> --out /tmp/coverage-<name>-before [--html] [--run "./unit_tests --gtest_filter=..."]
python3 SKILL_DIR/scripts/uncovered.py /tmp/coverage-<name>-before --md /tmp/coverage-<name>-before/uncovered.md
```
- No `tests/` / `BUILD_TESTS` yet: set them up with `cpp-tests` first (ask the user).
- The build is Debug + `-fprofile-instr-generate -fcoverage-mapping` (clang), one profile per process (forked tests and
  sub-processes counted), `tests/`, `/usr`, `_deps` and `generated_*` left out of the numbers (`--ignore` to change).
- Failing tests don't stop the measure (see `tests.log`), but say them: a failing test is a bug or a known bug kept
  as failing (`cpp-tests` rule), never "fixed" by weakening the test.
- `summary.txt` (per file), `uncovered.md` (functions never called, line ranges never run), `--html` for the annotated
  sources (`html/index.html`: open it to read which branch is missed).

## 2. Fill (maximize, module by module)
Order: the files with the most missed lines first, the public API before the internals (an internal reached through the
public API needs no direct test). For each module:
1. Read `uncovered.md` for the file and the source of the missed ranges (HTML or the file itself).
2. Write the tests with `cpp-tests` (file mirroring the tree, nominal / edge / error cases, libutils error codes,
   sub-process isolation with timeout for threads / IPC / `exit`, mocks of the interfaces, `TempDir` / `ScopedEnv`),
   register them in `tests/CMakeLists.txt`.
3. Build and run the tests normally (`cmake -B build -DBUILD_TESTS=ON`...) until they pass, then re-measure
   (`coverage.sh` with the same options) and note the gain of the module.
Stop when a pass gains nothing (only what can't be reached is left). What can't (or shouldn't) be covered is listed
with its reason, not forced:
- defensive branches of impossible states, `FatalException` paths needing a broken system (allocation / syscall
  failure: only when a cheap injection exists, ex: a closed fd, an unreadable file);
- code that blocks forever, needs hardware / network not available in a test, or is platform specific;
- dead code (never called by anything): report it, propose to remove it, never test it.
**Never change the production code to raise the coverage.** A bug found by a new test: keep the test failing (known bug),
tell the user and fix it only on request.

## 3. End: always ask for the report
When the tests are written (or when the user only asked for the measure), always ask (AskUserQuestion, French):
`Veux-tu un rapport de la couverture et de ce qui manque ?` · `Oui (Recommandé)` · `Non`. If yes, `report` skill
(format asked there), location `audit/<YYYY-MM-DD>-coverage.md|pdf`:
1. **Summary**: before / after (lines, functions, branches), tests added, tests failing (known bugs).
2. **Coverage per file**: the table of `uncovered.md` (after), with the before column for the files changed.
3. **Still missing**: per file, the functions / ranges left and **why** (the categories above), with what would cover
   them (refactor for testability, fault injection, integration test).
4. **Bugs found** (if any) and **dead code** found.
5. **Reproduce**: the `coverage.sh` command.
