---
name: computer-clean
description: Free space on the user's computer and in the data of Claude Code / claude-cluster - finds the caches (pip, npm, ccache, cargo, Firefox, shaders, the builds of ai-utils...), the trash, the build outputs of projects untouched for a while (never a git-tracked file), Docker / Podman images and build cache no container uses, the journal, the dnf cache, unused flatpak runtimes, big old downloads, old Claude Code conversations, finished background jobs, worktrees of .claude/worktrees without changes, claude-cluster transcripts of deleted sessions, its archives and showcase - shows them with their size, age and why they can go, asks which ones in a QCM, then deletes the chosen ones for good. Use whenever the user wants to clean / free space / tidy their computer, their disk, their caches, Docker, or the leftovers of Claude Code / claude-cluster (old sessions, jobs, worktrees).
---

# Computer clean

`SKILL_DIR` = the directory of this file. The script finds, the user chooses, the script deletes: nothing is deleted
that the user did not tick. Deletion is **for good** (no trash: the user's choice), so the list must be right.

## 1. Find
```bash
python3 SKILL_DIR/scripts/computer_clean.py scan [--days 30] [--min-mb 5] [--root ~/projects]... [--json]
```
Two groups, each candidate with an id, its size, its age, `safe` (made again when needed: caches, build outputs,
trash, finished jobs) or `review` (to check: big downloads, Docker / Podman images, old conversations, worktrees,
archives), why it can go and, for Docker / the journal / dnf / flatpak, the command run:
- **This computer**: `~/.cache/*` (pip, npm, yarn, pnpm, uv, poetry, go-build, ccache, the builds of ai-utils, Firefox,
  Chrome, shaders, JetBrains, Hugging Face...), `~/.npm`, cargo downloads, the desktop trash, build outputs (`build`,
  `build-*`, `cmake-build-*`, `node_modules`, `__pycache__`, `.pytest_cache`...) next to a project file, untouched for
  `--days` and never tracked by git (`--root`: where to look, default the home folder; hidden folders and installed
  programs skipped), big downloads (> 200 MB, old), Docker / Podman (`system prune -af`), the user journal (2 weeks
  kept), `/var/cache/dnf` (root), flatpak runtimes no app uses.
- **Claude Code / claude-cluster**: conversations of `~/.claude/projects` older than `--days`, finished jobs of
  `~/.claude/jobs` (never the running one), old shell snapshots / file history / todos, worktrees of
  `.claude/worktrees` with nothing uncommitted (`git worktree remove`, the branch kept), claude-cluster: transcripts
  of sessions it no longer has, `logs/archive`, the showcase session, the debug trace, old pasted pictures.
`--json` gives the same list to read programmatically.

## 2. Show and ask (AskUserQuestion, French, interactive-questions rules)
- First the summary in text: the total per group, the biggest items, what is `review` and why (ex: "les images Docker
  servent peut-être à un projet arrêté").
- Then the QCM (multiSelect), **1 to 4 questions of up to 4 options**, grouped by theme: *Caches*, *Builds et
  corbeille*, *Docker / système*, *Claude / claude-cluster*; each option = one candidate or a set of the same kind
  (label: what + size, description: why it can go / what to check). The `safe` ones pre-recommended, the `review` ones
  explained. More candidates than fit: several rounds, or a set per kind ("6 dossiers build anciens, 115 Mo").
- No answer for a group: nothing of it is deleted.

## 3. Delete the chosen ones
```bash
python3 SKILL_DIR/scripts/computer_clean.py clean <id> <id>... [--days 30 ...]   # the same options as the scan
```
The scan is made again with the same options: an id that changed or disappeared is skipped (said). Root ones (dnf,
system journal): `SUDO_ASKPASS=~/.local/bin/sudo-askpass` exported, run with `sudo -A` (a password window on this
computer); without it, the command to run is printed. `--dry-run` first when the selection is big.

## 4. Report
A short table: what was deleted, the space freed (`df -h ~` before / after), what failed and why, what was left
(unticked, `review`). Never delete anything outside the list of the scan; a request beyond it (a project folder, a
whole directory): say it is not part of this skill and ask explicitly.
