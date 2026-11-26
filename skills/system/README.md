# System skills

The computer itself: free space, memory kept safe, long jobs that survive a crash.

## Skills
<!-- generated:skills -->
| Skill | What it does | Installs with it |
|---|---|---|
| [`computer-clean`](computer-clean/SKILL.md) | Frees as much space as possible: a deep scan of the caches (~/.cache, language tools, Chromium / Electron and flatpak apps), old build outputs and envs (never a git-tracked file), trash, downloads, duplicates, big old files, core dumps, Docker / Podman, flatpak, dnf, journals, old kernels and Claude Code / claude-cluster leftovers - each with its size (real gain on btrfs), what it is for and what deleting it does, chosen in a QCM, then deleted for good. | [`interactive-questions`](../ai/interactive-questions/SKILL.md) |
| [`memory-guard`](memory-guard/SKILL.md) | The session keeps every heavy job inside a memory budget: free memory checked first, jobs in a limited scope (killed alone), --parallel sized, one at a time, outputs off tmpfs. | — |
| [`sliced-work`](sliced-work/SKILL.md) | Long jobs in resumable slices (frames, tiles, items, chunks), written atomically, resumed after a crash, merged into the same result as one pass. | [`memory-guard`](../system/memory-guard/SKILL.md) |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## Examples
- *clean my computer*
- *free space in the old Claude Code sessions and worktrees*
- *fais gaffe à la RAM, ne crash pas le pc*
- *render the 240 frames en slicing, resumable after a crash*

[All the categories](../README.md) · [Installation](../../INSTALL.md)
