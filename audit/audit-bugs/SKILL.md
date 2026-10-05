---
name: audit-bugs
description: Audit a project for bugs in a loop - undefined behaviour, memory errors and leaks, data races, error handling, logic and security bugs - with sanitizer builds (ASan/LSan/UBSan/TSan), unit tests, compiler warnings, static analysers and code review, alone or with parallel subagents (asked to the user), every finding verified with evidence, then reported (optionally fixed after confirmation). Use whenever the user asks for an audit, bug hunt, UB / memory error / crash / race search, a sanitizer run, or a correctness review of a project, module or diff.
---

# Bug & UB audit

`<name>` (a skill) = the folder of that skill: `~/.claude/skills/<name>` (setup.sh) or `${CLAUDE_SKILL_DIR}/../../*/<name>` (plugin of the marketplace).

`SKILL_DIR` = the directory of this file. Main target: C/C++20 projects of the user (clang++, CMake, libutils),
the checklist also covers other languages.

## 1. Ask first (one AskUserQuestion call, French)
1. **Périmètre**: `Tout le projet` · `Un module / dossier` (which one) · `Le diff depuis une ref` (`git diff <ref>`).
2. **Mode**: `Agent seul (Recommandé)` (sequential, cheapest) · `Sous-agents en parallèle` (one Agent per module
   or per dimension, ~N times the tokens: give the estimated N) · `Workflow multi-agents` (only if the user wants
   orchestration explicitly; much more tokens).
3. **Boucle**: `1 passe` · `3 itérations` · `Jusqu'à plus de nouveau finding` (with a hard cap of 5).
4. **Corrections**: `Rapport seulement` · `Proposer des patchs` · `Corriger après confirmation` (each fix
   confirmed, then the project rebuilt/retested). Never modify code without this choice.

## 2. Tooling pass (every iteration)
```bash
bash SKILL_DIR/scripts/sanitize_build.sh <project> [asan|ubsan|tsan|all] [--run "<cmd>"] [--keep] \
     [--lsan-supp <file>] [--tsan-supp <file>]
```
Copies the working tree to `/tmp` (nothing written in the project, even the outputs the CMake writes in the
sources), builds in `Debug` with clang + the sanitizers (`all` = ASan+UBSan build, then a TSan build), unit tests
on (`-DBUILD_TESTS=ON`) when `tests/` exists, runs `ctest` (and `--run`) with
`halt_on_error=0`/`print_stacktrace=1`, then summarizes: counts per sanitizer, distinct issues, most frequent
project frames. Exit 0 = clean, 1 = reports, 2 = build failure. `--keep` to read the full logs.
- Release-only bugs: also build the project's `Asan` type (`-DCMAKE_BUILD_TYPE=Asan`) and `Optimized`.
- Warnings at the maximum on the copy: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wcast-align -Wold-style-cast` (add to `CMAKE_CXX_FLAGS`).
- Other tools when installed (on this PC: `valgrind`, `gdb`, `llvm-symbolizer` yes; `clang-tidy`, `cppcheck`,
  `scan-build` no -> `SUDO_ASKPASS=~/.local/bin/sudo-askpass command sudo -A dnf install clang-tools-extra cppcheck clang-analyzer`
  after asking): `clang-tidy -p build` (`bugprone-*,cert-*,clang-analyzer-*,concurrency-*,cppcoreguidelines-*,performance-*`),
  `cppcheck --enable=all --inconclusive`, `scan-build cmake --build`, `valgrind --leak-check=full --track-origins=yes`
  on a non-sanitized build.
- Other languages: their linters / type checkers / test suites with race or debug modes.

### Dependencies (security)
```bash
python3 <audit-deps>/scripts/vulns.py <project> --json /tmp/vulns.json --md /tmp/vulns.md
```
Known vulnerabilities and compromised versions of the direct **and transitive** dependencies (OSV, dnf security
advisories, GitHub advisories of the upstreams), recent ones (< 90 days) first. For each one: is the vulnerable
code reachable from the project (grep the API used)? -> finding `security/dependency` with the fixed version.

## 3. Review pass (reading the code)
Checklist (C/C++), each item searched explicitly in the scope:
| Category | Look for |
|---|---|
| UB / memory | out of bounds (`[]`, `at`, pointer arithmetic, `memcpy` sizes), use after free / move, dangling references, iterators and `std::string_view` / `std::span` on temporaries, uninitialized reads, signed overflow, shifts >= width, strict aliasing / `reinterpret_cast`, misaligned access, null deref, double free, invalid `static_cast` down a hierarchy, `std::optional` / `std::get` without check, object lifetime in lambdas captured by reference and stored |
| Leaks / resources | raw `new`/`malloc` without owner, fd / mmap / socket not closed on every path (and on throw), missing virtual destructor |
| Concurrency | shared state without lock, lock order inversion, `std::condition_variable` without predicate, `std::jthread` / `stop_token` misuse, members used by a thread after destruction, non-atomic flags, `volatile` used as synchronisation |
| Errors | unchecked syscalls / libc returns (`read`, `write`, `fork`, `mmap`...), partial reads/writes, `EINTR`, exceptions thrown from destructors / `noexcept`, swallowed exceptions, libutils exception code restrictions (`libutils-exception`) |
| Logic | inverted conditions, off-by-one, wrong operator (`==`/`!=`, `&`/`&&`), unreachable branches, copy-paste divergences, wrong defaults, API contract violations (documented behaviour vs code, CHANGELOG claims) |
| Security | injection (shell, format strings), unchecked sizes from the network/files, integer conversions on sizes, TOCTOU, secrets in logs, weak crypto use |
Other languages: the same categories adapted (null/None, races, resource leaks, injection, error swallowing).

## 4. Subagent mode
Split by module directory (`include/<root>/<section>` + matching `src/`, `tests/`) or by dimension (the table rows)
when the project is small. One Agent per part (fresh `general-purpose` agents with a self-contained prompt,
not forks), in parallel, prompt template:
```
Audit <scope paths> of <project path> (C++20, <libs>) for: <dimensions>.
Read the code, use the sanitizer logs in <logs path> when they point to your scope.
Report ONLY verified issues as a JSON list: {file, line, category, severity (critical|high|medium|low),
title, evidence (code excerpt + reasoning or log), reproduction (input/test/command), fix (minimal)}.
No style remarks, no speculative issue without evidence. Don't modify any file.
```
Then: merge, deduplicate (same file/line/root cause), and run a **verification pass** on every finding
(another agent or yourself, adversarial: try to prove it wrong; keep it only with evidence).

## 5. Loop
iteration = tooling pass + review pass -> verify each finding (reproduce with a test / the sanitizer, or a precise
reasoning with the code path) -> drop false positives (ex: leaks made on purpose by a leak-detection test: verify,
then add a suppression `leak:<TestName>` for the next runs) -> fixes when chosen (minimal, in the user's style:
`cpp-style`, `cpp-comments`; add a regression test when tests exist) -> rebuild + rerun. Stop when an iteration
finds nothing new or at the chosen budget. Never claim a bug without evidence.

## 6. Report
- Findings ranked by severity: `file:line`, category, title, evidence, reproduction, proposed fix (or applied fix +
  test), status (confirmed / fixed / false positive with the reason).
- Summary table: per category x severity, iterations done, tools run (and the ones missing), false positives dropped.
- Always delivered through the `report` skill: ask the output format of `report` (`Markdown + PDF` recommended, PDF only, Markdown only), location `audit/` at the root of the repository (or of the current folder outside a repository), never `docs/` unless asked (`audit/<YYYY-MM-DD>-bugs.{md,pdf}`),
  in English unless asked; the chat only gives the verdict (counts per severity) and the paths.
