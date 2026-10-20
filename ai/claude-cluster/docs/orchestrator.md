# Resource orchestrator

A kernel-like scheduler for the **heavy jobs** of the sessions (builds, tests, containers / media / GPU): a queue where jobs
wait, run or are partly run (stopped), a limited number run at once, the limit following the hardware, the live load and the
settings. Light commands (`ls`, `grep`, `git status`...) never go through it.

Why: with ~10 sessions building at once the computer saturates (load 30 on 12 cores, swap full, clang crashing). The
orchestrator makes the sessions take turns instead of fighting.

## Table of contents
- [Parts](#parts)
- [Job](#job)
- [State machine](#state-machine)
- [Scheduling](#scheduling)
- [Limits and pressure](#limits-and-pressure)
- [Hardware scan](#hardware-scan)
- [Wrapper protocol](#wrapper-protocol)
- [MCP tools and scoping](#mcp-tools-and-scoping)
- [Callbacks](#callbacks)
- [Persistence and cleanup](#persistence-and-cleanup)
- [Settings](#settings)
- [Interface](#interface)
- [Tests](#tests)

## Parts

| Part | Where | Role |
|------|-------|------|
| `Orchestrator` | `Orchestrator*.cpp` | queue, state machine, scheduler thread, persistence, control commands |
| `Hardware` | `Hardware.cpp` | scan at start / on demand (cores, RAM, swap, disk, GPU, power) + live sample of `/proc` |
| `cluster-run` | `src/run/cluster-run.cpp` | the wrapper a session puts before a heavy command; its own small binary |
| `RunProfile` | `RunProfile.cpp` | shared: the parallelism of a command lowered to its granted share, nice / ionice |
| MCP tools | `Orchestrator-Mcp.cpp` | `heavy_run`, `jobs_list`, `job_*`, `jobs_add`, `hardware_scan`, `orchestrator_config` |
| Tasks session | the interfaces | a Jobs section next to the task queue (queue, gauges, kill / stop / resume / priority) |

The core owns one `Orchestrator`; an interface of another process reads it through the control socket (`job_*`).

## Job

`id` (`j1`...), `name`, `class` (build | tests | container | gpu | custom), `prio` (-10..10, higher first), `session` (the owner:
`g`, `s3`, empty = the user), `origin` (`wrap`: a `cluster-run` of a session; `run`: started by the core for `heavy_run` /
`jobs_add`), `command` / `argv`, `cwd`, `state`, `reason`, `by` (who stopped / killed it), `created` / `started` / `ended`,
`pid` (process group), `exit` code, `threads` granted, `ram_mb` estimated, `rss_mb` seen, `callback`, `tail` (last 4 KB of
output), `log` (run origin: the whole output).

## State machine

```text
            +-- kill / session archived --> killed
 queued ----+
   |        +-- granted --> running --+-- exit 0 -------> done
   |                          |  ^    +-- exit != 0 ----> failed
   |                    stop  v  | resume                 +-- kill --> killed
   |                        stopped (user | pressure)
   +-- core restart: run jobs re-queued, wrap jobs failed ("core restarted")
```

- `queued -> running`: only the scheduler. `running <-> stopped`: SIGSTOP / SIGCONT of the process group, by the user / the
  global session (`user`: only a user resume lifts it) or by the pressure (`pressure`: resumed by the scheduler).
- `killed.by`: `user`, `global`, `session` (its owner asked), `pressure` (OOM risk, setting), `archived` (session closed /
  purged), `timeout`, `restart`.
- A finished job stays in the list (the last `history` ones, saved) with its exit code, duration, tail.

## Scheduling

One thread (`Orchestrator::loop_`) ticks every 250 ms (children reaped, outputs read) and every second (sample, pressure,
schedule). A job starts when **all** of these hold:

1. the orchestrator is enabled and no pressure hold (see below);
2. active jobs (running, not stopped) `<` `slots` (manual `max_jobs`, else auto);
3. its session has fewer than `max_per_session` (or its quota) active jobs;
4. its class has fewer than the class limit (`gpu`: 1, `container`: 2 by default);
5. its RAM estimate fits in `MemAvailable - reserved_ram - RAM of the jobs started less than 20 s ago` (their memory is
   not used yet);
6. exception: when nothing runs, one job is started anyway unless the memory is under the OOM threshold (nothing starves).

Order = descending score: `10 * prio + 10 * session_prio + focus_boost (the session shown by the user / with an interactive
turn) + waited_seconds / aging_seconds`, then FIFO. The aging term makes any job pass the others in the end.
`waiting_for` tells why the first blocked job waits (slot 4/4, session limit, class busy, RAM, pressure, priority) and the
ETA comes from the average duration of the finished jobs of the class.

Granted parallelism per job: `low` = 1 thread; `normal` = `usable cores / active` clamped to 1..4; `max` = `usable cores /
active`; the `threads` setting overrides. It is given as environment (`MAKEFLAGS=-jN`, `CMAKE_BUILD_PARALLEL_LEVEL`,
`CARGO_BUILD_JOBS`, `CTEST_PARALLEL_LEVEL`, `OMP_NUM_THREADS`, `OMP_THREAD_LIMIT` (so `$(nproc)` is N), `RAYON_NUM_THREADS`,
`GOMAXPROCS`...) and by lowering an explicit `-jN` / `--parallel N` / `--jobs N` / `-n N` of make, ninja, cmake --build,
ctest, cargo, pytest to N (ninja gets `-jN` when it has none). Profiles: low = nice 19 + ionice idle, normal = nice 10 +
best-effort 7, max = nice 5 + best-effort 4.

## Limits and pressure

`slots` (auto) = `min(usable cores * factor, (RAM - reserved) / RAM of a job)` with factor 0.5 / 1 / 1.5 for low / normal /
max, usable cores = cores - `reserved_cpu`. RAM per job (MB): build 2000, tests 1200, container 1500, gpu 2000, custom 1000
(settings per class, or `ram_mb` of a job). The live part is the gate 5 above and the pressure.

The sample (every second) is `/proc/loadavg`, `/proc/meminfo`, `/proc/pressure/{cpu,memory,io}` (PSI, when present). Level:

| Level | Condition (any) | Effect |
|-------|-----------------|--------|
| 0 ok | below every hold threshold | jobs start |
| 1 hold | load/core >= `hold_load`, MemAvailable <= `hold_mem_mb`, swap used >= `hold_swap_mb` **and** memory short (PSI some >= hold/4 or MemAvailable < 2x hold), PSI >= hold | nothing new starts |
| 2 reduce | load/core >= `reduce_load`, MemAvailable <= `reduce_mem_mb`, PSI >= reduce | running jobs reniced to 19, the lowest-priority newest one SIGSTOPped every 2 s (never the last active one) |

Hysteresis: back to a lower level only after the sample stayed under 80% of the thresholds for `resume_after` seconds; then
the stopped-by-pressure jobs resume one at a time (highest score first). A stale swap alone never blocks (the memory must
be short too). Never killed by default; `kill_on_oom = true`: under `oom_mem_mb` the newest job is killed (`by pressure`).

Auto-detect: every 5 s the process table is read; a process group under a session, outside the queue, with more than
`detect_cpu_seconds` CPU is listed as `detected` and the session is told once to use `cluster-run` (`detect_action`:
off | notice | tell).

## Hardware scan

`hardware_scan` at start (and on demand: the `hardware_scan` tool, the palette): CPU model, logical / physical cores, RAM, swap, disk
(data folder), GPUs (`nvidia-smi`, `lspci`, `rocm-smi` when present), power (AC / battery), highest temperature. Saved in
`<data>/hardware.json` with `scan_version` (1) and the claude-cluster version. `proc_dir` / `sys_dir` in the settings point the
sample to a fake tree (tests: pressure on demand).

## Wrapper protocol

`cluster-run [--class c] [--prio n] [--name x] [--cwd d] [--session id] -- cmd...` (session: `$CLUSTER_RUN_SESSION`, set with
the `PATH` of the core's own `bin` folder in the environment of every session). Over the control socket, one JSON per line:

| Direction | Message |
|-----------|---------|
| wrapper -> core | `{"cmd":"job_run","session","class","prio","name","cwd","argv","wrapper_pid"}` |
| core -> wrapper | `{"ok":true,"event":"queued","id","position","total","waiting_for","eta_s"}` (again when it changes) |
| core -> wrapper | `{"ok":true,"event":"go","id","threads","profile","nice","io_class","io_level"}` (the wrapper builds the environment and rewrites the `-j` itself, `RunProfile`) |
| wrapper -> core | `{"event":"started","pid"}` (the command's process group), later `{"event":"exit","code","signal","tail"}` |
| core -> wrapper | `{"event":"killed","by","reason"}` (the core already signalled the group) |

The wrapper prints `cluster-run: queued: position N, waiting for X` on stderr, runs the command (own process group, stdout /
stderr / exit code passed through; a tee keeps the tail when they are pipes), forwards its own SIGINT / SIGTERM to the group
and exits with the code of the command (143 when killed). The core releases the slot at the `exit` event **or when the
connection drops** (the wrapper died: the command's group is terminated). When the core is not running the wrapper runs the
command directly (low profile, a warning); when the connection drops while queued it retries for 30 s, then runs directly.

## MCP tools and scoping

`heavy_run` (add + wait up to 100 s, the result or the waiting status), `jobs_add` (no wait, with a callback), `jobs_list`,
`job_status` (cheap), `job_wait` (up to 100 s), `job_kill`, `job_stop`, `job_resume`, `job_prio`, `hardware_scan`,
`orchestrator_config` (get / set). All are toggleable in `[mcp_tools]`.

- The **global session** sees and manages every job.
- A **sub-session** gets `heavy_run`, `jobs_add`, `jobs_list`, `job_status`, `job_wait`, `job_kill` and sees / kills its own
  jobs only (`[orchestrator] sub_add`, `sub_kill_own`, `sub_see_others`; default: add + own only; no stop / resume / prio / config).

Every answer is explicit: `finished` (exit code, duration, tail), `killed` (by whom), `failed`, `waiting` (position, ETA,
what it waits for), `running`, `stopped` (partial: stopped / resumed, since when).

## Callbacks

A job added with `jobs_add` (callback on by default) wakes its owner when it ends or is killed: a prompt
`[job j3 done] exit 0 in 4m12s` + the tail, sent with the same path as the `[task ...]` messages (`Manager::send`: a woken-up
turn of the owner; when the global session follows that sub-session, its answer is relayed by `replyEnd_` as usual). A
sub-session may also poll `job_status` (one line) or block in `job_wait`. Jobs run through `cluster-run` need no callback
(the command of the session returns by itself). `notify_killed` / `notify_done` also raise a desktop / app notification.

## Persistence and cleanup

`<data>/orchestrator.json`: the jobs (queued, running, the last finished) + the id counter, written on every change (at most
once a second). At start: running `run` jobs of a dead core are reaped (their group killed when `/proc/<pid>` still has the
recorded start time) and marked failed (`core restarted`), queued `run` jobs are queued again, queued / running `wrap` jobs
are failed (`core restarted: run it again`; their wrapper retries its connection).

A session closed / archived: its queued jobs are removed, its running ones killed (`by archived`). Purged: its records go too.

## Settings

`[orchestrator]` of `config.toml` (settings page tab **Orchestrator**, live): `enabled`, `profile`, `max_jobs` (0 auto),
`max_per_session`, `reserved_cpu`, `reserved_ram_mb`, `threads`, `<class>_max`, `<class>_ram_mb`, `patterns` (`"text"` or
`"text = class"`: a command containing it is heavy), `session_prio` / `session_quota` (`"s3=5"`), the thresholds, `kill_on_oom`,
`aging_seconds`, `focus_boost`, `detect_cpu_seconds`, `detect_action`, `sub_add`, `sub_kill_own`, `sub_see_others`,
`notify_killed`, `notify_done`, `history`, `proc_dir`, `sys_dir`. The shipped `templates/config.toml` documents each.

## Interface

Ctrl+K / the **Tasks** session: a Jobs section (running / waiting / stopped, the gauges load, memory, swap, PSI, per session)
with kill / stop / resume / priority; the settings page tab; terminal, window, web page and app (notification when a job is
killed / finished, if enabled).
Not done yet (TODO): a headless `claude-cluster jobs ...` command (the MCP tools and the interfaces cover it), the Android app's own jobs view (its notifications come through the `task` kind).

## Tests

`tests/orchestrator.sh` (isolated core, fake claude, `sleep` / busy loops as heavy commands, a fake `/proc` through
`proc_dir`): limits, queue order and priority, kill, stop / resume, wrapper exit code and loss, archive cleanup, pressure
hold / reduce / resume, scoping of the sub-sessions, callbacks, restart reaping.
