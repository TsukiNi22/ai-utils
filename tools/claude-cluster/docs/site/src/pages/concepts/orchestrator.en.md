+++
title = "Orchestrator and cluster-run"
summary = "The queue for heavy jobs: why, how to use cluster-run and heavy_run, the limits that follow your hardware and the pressure rules."
section = "concepts"
order = 50
keywords = "orchestrator cluster-run heavy_run jobs queue build tests container gpu pressure load nice ionice priority slots hardware"
+++

With several sessions building and testing at once a computer saturates: load far above the cores, swap full, compilers crashing. The orchestrator is a small kernel scheduler for the **heavy jobs** (builds, tests, containers, media, GPU): they wait in a queue, a limited number run at once, and the limit follows the hardware, the live load and your settings. Light commands (`ls`, `grep`, `git status`) never go through it.

## Using it {#use}

| Where | How |
|---|---|
| a shell (a session's or yours) | `cluster-run [--class build\|tests\|container\|gpu\|custom] [--prio N] [--name x] -- <command>`: waits (`cluster-run: queued: position 2, waiting for slot (2/2 running)` on stderr), then runs in the low profile (nice, ionice, `-j` / `--parallel` / `MAKEFLAGS` / `CARGO_BUILD_JOBS` lowered to the share granted); output and exit code pass through |
| MCP, a command that waits | `heavy_run`: returns the result (finished with exit code, duration, tail; failed; killed by whom) or the waiting status |
| MCP, no waiting | `jobs_add` (woken with `[job jN ...]`), `jobs_list`, `job_status`, `job_wait`, `job_kill`, `job_stop`, `job_resume`, `job_prio` |
| the computer | `hardware_scan`, `orchestrator_config` |
| interface | the **Tasks** session (a Jobs section: queue, gauges, kill / stop / resume / priority), settings *Orchestrator* |

`cluster-run` is installed next to `claude-cluster` and is in the `PATH` of every session. Without a running core it runs the command directly with a warning.

## Who sees what {#scope}

The global session sees and manages every job. A sub-session adds, follows, waits for and kills **its own** jobs (`[orchestrator] sub_add`, `sub_kill_own`, `sub_see_others`). A closed or archived session loses its jobs.

## Job states {#states}

```text
queued -> running -> done | failed | killed
              \-> stopped (by you, or by pressure) -> running
```

A job starts only when the orchestrator is enabled, there is no pressure hold, active jobs are below the slots, its session and its class are below their limits and its RAM estimate fits. When nothing runs one job starts anyway so nothing starves. The order is `10 * prio` plus session priority, a boost for the session you look at, and aging (a waiting job gains a point every `aging_seconds`), then FIFO.

## Limits {#limits}

- **Automatic**: slots = min(usable cores x factor, (RAM - reserved) / RAM of a job), with factor 0.5 / 1 / 1.5 for the profile `low` / `normal` / `max`; RAM per class (build 2000 MB, tests 1200, container 1500, gpu 2000, custom 1000).
- **Manual**: `max_jobs`, `max_per_session`, per class (`gpu` 1, `container` 2 by default), `reserved_cpu`, `reserved_ram_mb`, `threads`.
- **Pressure** (load, `MemAvailable`, swap, `/proc/pressure`): at *hold* nothing new starts; at *reduce* running jobs are reniced and the lowest-priority newest one is stopped (SIGSTOP) until the load falls, then resumed with hysteresis. Never killed unless `kill_on_oom = true`.
- **Auto-detect**: a process group outside the queue that burned `detect_cpu_seconds` of CPU is listed and its session is told once to use `cluster-run`.

## Memory guard {#memory-guard}

`[orchestrator] memory_guard` (on) watches, every 5 s and without any model, everything the sessions run - agents, their shells and background commands, sub-agents, heavy jobs, the global session too, never the core. Under `guard_free_mb` of memory available (0: 5 % of the RAM, at least 500 MB) it kills the biggest of them, its whole process tree, before the system's out-of-memory killer does: a command or job first (never one under `guard_min_mb`), an agent only when nothing else is big enough, the global session last. The session is told what was killed, how much it used and that it may run it again with less memory; a notice and a notification say it too. Settings > About / Processes shows the memory of each session, the free / total memory, the guard and its last kills.

## Hardware {#hardware}

`hardware_scan` records the CPU, cores, RAM, swap, disk, GPUs and power in `hardware.json`, at start and on demand. The sample for pressure is read every second from `/proc`.

> [!NOTE]
> The full design (state machine, wrapper protocol, persistence) is in `docs/orchestrator.md` of the sources; the [protocol](page:protocol) page summarises the wrapper messages.
