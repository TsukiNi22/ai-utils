---
name: computer-clean
description: Free as much space as possible on the user's computer and in the data of Claude Code / claude-cluster - a deep scan of every ~/.cache folder, the caches of the language tools and of the Chromium / Electron / flatpak apps, build outputs and envs of old projects (never a git-tracked file), the trash, downloads (installers, archives already extracted), duplicated or big old files, core dumps, Docker / Podman (images, orphan volumes), flatpak (unused runtimes, SDKs, data of removed apps), dnf (cache, updates not installed), journals, old kernels, old Claude Code conversations, finished jobs, clean worktrees, claude-cluster leftovers - each with its size (real gain on btrfs), what it is for and what deleting it does, chosen in a QCM, then deleted for good. Use whenever the user wants to clean / free space / tidy their computer, their disk, their caches, Docker, or the leftovers of Claude Code / claude-cluster (old sessions, jobs, worktrees).
metadata:
  summary: 'Frees as much space as possible: a deep scan of the caches (~/.cache, language tools, Chromium / Electron and flatpak apps), old build outputs and envs (never a git-tracked file), trash, downloads, duplicates, big old files, core dumps, Docker / Podman, flatpak, dnf, journals, old kernels and Claude Code / claude-cluster leftovers - each with its size (real gain on btrfs), what it is for and what deleting it does, chosen in a QCM, then deleted for good.'
---

# Computer clean

`SKILL_DIR` = the directory of this file. The script finds, the user chooses, the script deletes: nothing is deleted
that the user did not tick. Deletion is **for good** (no trash: the user's choice), so the list must be right and each
item explained: what it is for, what deleting it does.

## 1. Find
```bash
python3 SKILL_DIR/scripts/computer_clean.py scan [--days 30] [--min-mb 5] [--root ~/projects]... [--top 12] [--json]
```
Run `df -h ~ /` first (the "before" of the report). A deep clean: `--days 7`. Each candidate has an id, its size, its
age, `safe` (made again when needed) or `review` (to check), **for** (what it is for), **after** (what deleting it
does), its command when it is not a plain `rm -rf`, and on btrfs **frees at least** (the part no other file shares:
uv venvs, `cp --reflink` copies and snapshots keep the shared part alive - ex: a 6 GB uv cache that frees 45 MB).
Five groups:
- **Caches**: every folder of `~/.cache` (known ones `safe` with their own text, unknown ones `review`: some tools keep
  real data there), the caches of the language tools (`~/.npm`, pnpm store via `pnpm store prune`, cargo, `go clean
  -modcache`, gradle, maven, nuget, conan, yarn, bun, `~/.debug` of perf, Android, PlatformIO, pipenv envs, ollama
  models), the web caches of the Chromium / Electron apps in `~/.config` (Discord, VS Code, Chrome...) and the caches of
  the flatpak apps (`~/.var/app/*/cache`).
- **Build outputs and envs of old projects** under `--root` (default the home folder; hidden folders and installed
  programs skipped): `build`, `build-*`, `cmake-build-*`, `node_modules`, `target`, `.venv` / `venv` (`review`), `dist`
  (`review`), `.next`, `__pycache__`, `.pytest_cache`, `.gradle`, zig / dune / stack / cabal / .NET outputs... each next
  to its project file, untouched for `--days`, never tracked by git.
- **Files**: the trash, the downloads folder of the desktop (`XDG_DOWNLOAD_DIR`: `~/Téléchargements` in French) -
  archives already extracted next to their folder, installers and disk images (`.iso`, `.rpm`, `.deb`, `.AppImage`...),
  big downloads (> 200 MB) - big files present twice in the desktop folders (same content, every copy but one), big
  files untouched for months (> 500 MB, 90 days), core dumps (`core`, `vgcore.*`), big logs (> 50 MB).
- **System and containers**: Docker / Podman (`system prune -af`: unused images, stopped containers, build cache) and
  their orphan volumes (one by one: the data of an old compose project), the user journal and the system journal (2
  weeks kept), dnf cache (`/var/cache/libdnf5`, `/var/cache/dnf`), the GNOME Software cache, **updates downloaded but
  not installed** (`/var/lib/dnf/offline`, often GBs), Fedora upgrade leftovers, system crash dumps, old kernels (never
  the running one nor one fallback), flatpak runtimes no app uses, flatpak SDKs no app runs on (`--unused` keeps them),
  data of flatpak apps no longer installed. Root ones say so.
- **Claude Code / claude-cluster**: conversations of `~/.claude/projects` older than `--days`, finished jobs of
  `~/.claude/jobs` (never the running one), old shell snapshots / file history / todos, worktrees of
  `.claude/worktrees` with nothing uncommitted (`git worktree remove`, the branch kept), claude-cluster: transcripts of
  sessions it no longer has, `logs/archive`, the showcase session, the debug trace, old pasted pictures.

Last, the **biggest folders of the home** (information only, never candidates): what is big but not a leftover (AI
models, venvs of tools, wine prefixes, projects). `--json` gives the candidates to read programmatically.

## 2. Show and ask (AskUserQuestion, French, interactive-questions rules)
- First the summary in text: `df` (used / free), the total per group, the biggest items with their **real gain** on
  btrfs, what is `review` and why (ex: "un volume Docker orphelin = la base de données d'un ancien projet compose"),
  and the biggest folders worth knowing about.
- Then the QCM (multiSelect), **1 to 4 questions of up to 4 options**, by theme: *Caches*, *Builds*, *Fichiers*,
  *Système / conteneurs*, *Claude*; each option = one candidate or a set of the same kind (label: what + size, or the
  real gain when it is much smaller; description: **what it is for and what deleting it does**, from `for` / `after`).
  The `safe` ones recommended, the `review` ones explained. More candidates than fit: several rounds, or a set per kind
  ("18 dossiers target de Rust de la piscine, 180 Mo").
- Updates waiting to be installed: recommend installing them (restart and update in GNOME Software: they go by
  themselves) when there is room, deleting them when the disk is full.
- No answer for a group: nothing of it is deleted. A question about an item ("c'est quoi uv ?"): answer it (what it
  is, what uses it, the real gain) and ask again before deleting it.

## 3. Delete the chosen ones
```bash
python3 SKILL_DIR/scripts/computer_clean.py clean <id> <id>... [--days 30 ...]   # the same options as the scan
```
The scan is made again with the same options: an id that changed or disappeared is skipped (said). Before a `review`
one, look at it: a Docker volume (`docker volume inspect`: compose project, date; which volume the running container
mounts), a `node_modules` / venv mounted in a running container (`docker inspect -f '{{.Mounts}}'`), a download or a
big file (its name, what folder it belongs to). Root ones (dnf, journals, crash dumps, kernels, PackageKit):
`SUDO_ASKPASS=~/.local/bin/sudo-askpass` exported, run with `sudo -A` (a password window on this computer); without it,
the command to run is printed. Flatpak SDKs are removed with `--no-related` (their related extensions, like
`codecs-extra`, may be needed by a Platform). `--dry-run` first when the selection is big.

## 4. Report
A short table: what was deleted, the space freed (`df -h ~` before / after; on btrfs the space can come back a few
seconds later), what failed and why, what was left (unticked, `review`). Never delete anything outside the list of the
scan; a request beyond it (a project folder, a whole directory, one of the biggest folders): say it is not part of
this skill and ask explicitly.
