---
name: benchmark
description: Benchmark and profile the current program (C / C++ first, also Rust and Python) with the tools available (perf stat / record / report, hyperfine, valgrind callgrind / massif, heaptrack, flame graphs), asking before installing a missing tool - optimized build with symbols in a copy, wall time, hardware counters, hot functions (self and inclusive), hot lines, memory - then the analysis of what is slow and why, each finding classified as a quick win or a complex change with its estimated gain, delivered as a PDF / Markdown report (report skill), and optionally the optimizations with a before / after comparison. Use whenever the user asks for a benchmark, a profiling, a performance analysis, what is slow, which function takes the time, or how to optimize a program.
---

# Benchmark & profiling

`SKILL_DIR` = the directory of this file. Scripts: `scripts/tools.sh` (tools available / install commands),
`scripts/profile.sh` (build + measures), `scripts/hotspots.py` (tables for the report).
Reports: the `report` skill. Nothing is committed, the outputs stay in `/tmp/benchmark-*` unless the user asks.

## 1. Ask (one AskUserQuestion call, French) - only what the request doesn't say
- **Scénario mesuré**: the command(s) and the inputs, representative of the real use (a big input, the test suite,
  a benchmark target...); propose the obvious one from the project (`./<binary> <typical args>`, the GTest binary).
- **Ce qui compte**: temps CPU / latence, mémoire, les deux.
- **Profondeur**: `Rapide (perf + temps) (Recommandé)` · `Complète (+ callgrind + mémoire, plus lent)`.
The report format is asked by the `report` skill (once).

## 2. Tools
```bash
bash SKILL_DIR/scripts/tools.sh
```
Lists `perf`, `hyperfine`, `valgrind`, `heaptrack`, `flamegraph`, `/usr/bin/time`, `py-spy` with the install command of
the OS and the `perf_event_paranoid` level. When a tool **useful for the chosen depth** is missing, ask
(AskUserQuestion): `Installer (je lance la commande)` · `Tu les installes (commande donnée)` · `Continuer sans`.
Never install without a yes. Root commands go through the sudo method of the environment (on this computer:
`SUDO_ASKPASS=~/.local/bin/sudo-askpass sudo -A <command>`, see the global instructions). `perf_event_paranoid` > 2:
same question for `sudo sysctl kernel.perf_event_paranoid=2` (until the next reboot).
Without perf: wall time + callgrind still work; without anything: `/usr/bin/time` only, say it in the report.

## 3. Measure
```bash
bash SKILL_DIR/scripts/profile.sh --project <root> --cmd "<binary> <args>" [--cwd <inputs dir>] [--runs 10] \
    [--callgrind] [--memory] --out /tmp/benchmark-<name>
python3 SKILL_DIR/scripts/hotspots.py /tmp/benchmark-<name> --md /tmp/benchmark-<name>/hotspots.md --json /tmp/benchmark-<name>/hotspots.json
```
- CMake project: rebuilt in a `/tmp` copy (uncommitted changes included) in the `Optimized` mode of the user's
  CMake (`Release` otherwise) with `-g -fno-omit-frame-pointer`; the copy's binaries come first in the `PATH` while the
  command runs. **Never profile a Debug / Asan build.** Already built program: `--no-build`.
- Measures: wall time (hyperfine, else `/usr/bin/time` with the max RSS), `perf stat` (user space counters: IPC, branch
  and cache misses), `perf record` (cpu-clock samples with call graphs: one stream on hybrid CPUs), self / inclusive
  reports, flame graph (`flame.svg`) when the FlameGraph scripts exist, callgrind (exact instruction counts, use a
  **small** input: 20-50x slower) and heaptrack / massif with `--memory`.
- Variance: stddev > 5 % of the mean -> more `--runs`, close the heavy programs, say it in the report.
- **Rust**: `cargo build --release` with `[profile.release] debug = true` (or `CARGO_PROFILE_RELEASE_DEBUG=true`), then
  `profile.sh --no-build --cmd "target/release/<bin> ..."`. **Python**: `py-spy record -o /tmp/py.svg -- python3 x.py`
  (flame graph) and `python3 -m cProfile -s cumtime x.py | head -40`.

## 4. Analyse (the real work: the tables only say where, the code says why)
For every hot spot (top 10 self + the top functions of the program in the inclusive table):
- who calls it: `perf report -i <out>/perf.data --no-children -S '<symbol>' -G --stdio | head -60`;
- which lines: `perf annotate -i <out>/perf.data '<symbol>' --stdio | head -80` (or `callgrind_annotate --auto=yes`);
- read the code, find the cause, estimate the gain (share of the time x expected speedup) and the risk.
Classify each finding:
- **Quick win** (local change, low risk): regex compiled per call / `std::regex` in a loop (hand-written scan, compiled
  once, or a faster engine), copies by value of big objects, allocations in a hot loop (`reserve`, reuse buffers), two
  lookups instead of one (`find` + `[]`), `std::endl` flushes, `std::map` where `unordered_map` / a sorted vector fits,
  `std::function` / virtual calls in the innermost loop, string concatenation in a loop, exceptions as control flow,
  logging / verbose formatting done even when disabled, I/O without buffering, lock taken per item.
- **Complex** (design change): algorithm complexity (O(n^2) on big inputs), data layout (AoS -> SoA, cache misses),
  parallelism (threads, `utils::pool::Cluster`), I/O strategy (mmap, batching), caching / memoization, an external
  library (SIMD, a regex engine).
Counters help: IPC < 1 -> memory bound (cache misses, layout); high branch misses -> unpredictable branches; high
`sys` time -> syscalls (`strace -c`).

## 5. Report (report skill)
Location `audit/<YYYY-MM-DD>-benchmark.md|pdf` at the root of the repository. Structure:
1. **Summary**: table `Point | Conclusion` (scenario, wall time mean +- stddev, IPC, the 3 hottest spots with their
   share, the best gains expected).
2. **Measures**: environment (`info.txt`: CPU, kernel, build type, command), wall time and counters tables
   (`hotspots.md`).
3. **Hot spots**: the self and the program tables of `hotspots.md`, the flame graph (`![](flame.svg)` when it exists).
4. **Findings**: numbered, `1. **Finding.** cause (file:line), share of the time, fix, expected gain, risk`, split in
   `### Quick wins` and `### Complex changes`.
5. **Recommendations**: table ordered by gain / effort (`Change | Gain | Effort | Risk`).
6. **Reproduce**: the exact `profile.sh` command, the output directory.

## 6. Optimize (only if the user asks, after the report)
Ask which findings to apply (multiSelect). Apply them one by one, keep the behavior (run the tests), re-run the **same**
`profile.sh` command (same inputs, same runs) and add a `Before | After | Gain` table to the report. A change that
doesn't gain more than the variance is reverted and said so.
