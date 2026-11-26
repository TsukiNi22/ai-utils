---
name: memory-guard
description: Make the current session careful with the memory of the device it runs on (RAM, swap, tmpfs /tmp, GPU memory, parallel jobs) so that nothing gets the session killed, freezes the desktop or crashes the machine - check what is free before a heavy command, run every build, render, test suite, model or big data job in its own memory-limited systemd scope (memguard.py run --max, the job killed alone over its limit, nice and idle IO), size --parallel / -j from the free memory, one heavy job at a time, big outputs on disk and not in tmpfs, files streamed instead of loaded whole, and the job stopped when the machine runs short. Applies to the rest of the conversation once loaded. Use whenever the user says to be careful with the RAM / swap / memory, "ne crash pas le pc", "fais gaffe à la mémoire", "limite la mémoire", after a session or a build was killed (OOM, systemd-oomd), or before a heavy job on a small machine.
metadata:
  summary: 'The session keeps every heavy job inside a memory budget: free memory checked first, jobs in a limited scope (killed alone), --parallel sized, one at a time, outputs off tmpfs.'
---

# Memory guard

From now on, for the rest of the conversation, **the device comes first**: a job that runs out of memory is killed
alone and said, never the session, the desktop or the machine. `SKILL_DIR` = the directory of this file,
`memguard.py` = `python3 SKILL_DIR/scripts/memguard.py`.

## Before a heavy command
A build, a render, a test suite, an AI model, a big conversion, a scan over a big tree, a second session.
```bash
memguard.py status            # RAM / swap free, pressure, tmpfs, GPU, top processes, Claude sessions
memguard.py budget --per 800M # free for a new job, and how many parallel jobs fit (--parallel N / -jN)
```
- Budget = available RAM - 2 GB reserve (the desktop, the other sessions). Not enough: say it, and propose to close
  something (named from `status`) or a smaller job; never "try and see".
- Memory pressure (`some avg10`) over 10, or swap filling: wait or stop, no new heavy job.

## Run it limited
```bash
memguard.py run --max 4G -- cmake --build build --parallel 4   # own scope: MemoryMax 4G, swap 2G, nice 10, IO idle
memguard.py run --max 50% -- blender -b scene.blend -a          # half of what is available now
```
- Over its limit the kernel kills **that scope only** (exit 137): make the job smaller, then run it again.
- No systemd: the same command uses a watchdog that kills the process group over the limit.
- Long jobs: in the background, with a check of `status` between steps; resumable with `sliced-work`.

## Rules
| Point | Rule |
|---|---|
| parallel jobs | always a number: `--parallel N` / `-jN` from `budget --per` (a clang of a big C++ file ~0.8 GB); never bare `-j` or `$(nproc)` |
| one at a time | never two builds, renders or models at once, in this session or next to another one running |
| `/tmp`, `/dev/shm` | tmpfs = RAM: big outputs (renders, builds, datasets) go on disk (`~/.cache/<task>/`, the project's build folder) |
| big files | streamed (`grep`, `head`, `jq`, line by line), never read whole into Python or the context |
| GPU | `nvidia-smi` first, one GPU job, lower tile size / resolution / batch before a bigger card |
| tests | the suite limited too (`memguard.py run --max 3G -- ctest -j2`); a leak test with a small limit |
| a job killed | said with the limit and what was done to fit (fewer jobs, smaller slices), never restarted bigger blindly |
