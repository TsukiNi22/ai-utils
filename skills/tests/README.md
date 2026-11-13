# Test skills

Unit tests: GoogleTest the libutils way for C++, the framework of the project (or the one chosen) for the other
languages, and the coverage loop that writes tests until it stops growing.

## Skills
<!-- generated:skills -->
| Skill | What it does | Installs with it |
|---|---|---|
| [`coverage`](coverage/SKILL.md) | Test coverage of a `cpp-tests` project (clang / llvm-cov in a copy): per file, functions never called, ranges never run; then tests written module by module until it stops growing, and always the question of a report on what is still missing. | [`cpp-tests`](../tests/cpp-tests/SKILL.md), [`report`](../docs/report/SKILL.md) |
| [`cpp-tests`](cpp-tests/SKILL.md) | C++ unit tests like libutils: GoogleTest setup (CMake, CI, `unit_tests`), one test file per module, isolation of the blocking cases, known bugs kept as failing tests, `scripts/untested.py` to list what is never tested. | [`cmake-style`](../cpp/cmake-style/SKILL.md), [`cpp-style`](../cpp/cpp-style/SKILL.md), [`cpp-comments`](../cpp/cpp-comments/SKILL.md), [`cpp-class`](../cpp/cpp-class/SKILL.md), [`cpp-project`](../cpp/cpp-project/SKILL.md), [`report`](../docs/report/SKILL.md) |
| [`tests`](tests/SKILL.md) | Unit tests for any language: detects the existing framework or asks which one to use, then setup, tests, CI (defers to `cpp-tests` for C++). | [`cpp-tests`](../tests/cpp-tests/SKILL.md), [`report`](../docs/report/SKILL.md) |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## Examples
- *set up the unit tests of this project*
- *write the tests of `src/Parser.cpp` (nominal, edge and error cases)*
- *coverage of the project, then tests until it stops growing*

[All the categories](../README.md) · [Installation](../../INSTALL.md)
