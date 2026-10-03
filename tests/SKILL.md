---
name: tests
description: Set up and write unit tests for a project in any language - detects the languages and build system, reuses the test framework already in place or asks the user which framework/method to use (GTest, Catch2, doctest, Criterion, pytest, vitest/jest, cargo test, go test, bats...), then sets it up, inventories what to test, writes test files mirroring the source tree, runs them and adds the CI step. For C++ it follows cpp-tests. Use whenever the user wants unit tests, a test setup, more coverage or "test the whole project" and the project is not (only) C++.
---

# Unit tests (any language)

C++ (CMake) project or C++ part of a project: load and follow **`cpp-tests`** (GoogleTest, libutils way).
Everything else follows this file.

## 1. Detect
- Languages / build: `CMakeLists.txt`, `Makefile`, `*.c`, `package.json` (+ lock file), `pyproject.toml` / `setup.cfg` /
  `requirements*.txt`, `Cargo.toml`, `go.mod`, `*.sh`, `*.lua`...
- Existing tests: `tests/`, `test/`, `__tests__/`, `*_test.go`, `test_*.py`, `*.spec.ts`, framework in the dependencies,
  `ctest`/`pytest`/`vitest` config, CI step running tests.
- **A framework is already used: reuse it, say so, don't ask.**

## 2. Ask (AskUserQuestion, French) when nothing is in place
One question per language to set up, the default first with "(Recommandé)":

| Language | Options (default first) |
|---|---|
| C++ | GoogleTest (Recommandé, user's choice in libutils) · Catch2 · doctest |
| C | Criterion (Recommandé) · CUnit · Unity |
| Python | pytest (Recommandé) · unittest |
| JS / TS | vitest (Recommandé for Vite/ESM) · jest · node:test |
| Rust | cargo test (built-in) · + proptest |
| Go | go test (built-in) · + testify |
| Shell | bats-core (Recommandé) · shunit2 |

Also ask, in the same call when relevant: scope (whole project / given modules), and whether the CI must run them.
Don't install anything system-wide without confirmation (package managers: give the command; root -> graphical
`sudo -A`, see the global CLAUDE.md).

## 3. Set up
- Add the dependency the way the project manages them (dev dependency, CMake `find_package`, `Cargo.toml` `[dev-dependencies]`...),
  the config file only if needed, a `test` entry point (`npm test`, `make tests`, `ctest`, `pytest`), the produced files
  in `.gitignore` (coverage, caches, test binaries).
- CI: one step/job that builds and runs the tests on push (in the project's existing workflow when there is one;
  `git-conventions` / `cpp-project` workflows for the user's projects).

## 4. Write
- Inventory first: list the public modules / classes / functions and which ones have tests; for a big project show
  the plan (files + main cases) before writing.
- One test file per source file/module, **mirroring the source tree** (`src/net/client.py` -> `tests/net/test_client.py`,
  `src/parser.ts` -> `src/parser.test.ts` or `tests/parser.test.ts` per the framework convention).
- Per public unit: default state, nominal behavior, edge cases, errors (exact error type/code/message), concurrency /
  timing with timeouts (never fixed sleeps), anything that can hang isolated (sub-process / timeout option of the
  framework), global state restored after each test, fakes/mocks only at the boundaries (network, files, time, processes).
- Test names say the behavior (`test_cancel_unknown_id_raises`, `it("returns null on empty input")`).
- **Tests describe the expected behavior**: a test revealing a bug stays failing and is reported; never change the
  production code to make a test pass unless the user asks.
- Comments and style of the test code: the user's comment style (`comments` / `cpp-comments`) and the conventions of the project.

## 5. Run & report
Run the whole suite (and the new tests alone), report: tests added per module, passed / failing (= bugs found, with
the reason), what remains untested, how to run them (`<command>`), CI added or not.
Deliver the report as a **PDF** through the `pdf-report` skill (`.md` + `.pdf`, both paths given), not only in the chat.
