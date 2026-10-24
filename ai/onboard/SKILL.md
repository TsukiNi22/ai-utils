---
name: onboard
description: Set up the whole context of a project before working on it - read the folder (the current one or the one given) entirely - its tree, docs, code, build / config files, license and credits, links - its git history (log, branches, tags, worktrees, stashes, uncommitted and unpushed work), then find every earlier session on this computer that touched it (Claude Code, claude-cluster, Codex, Qwen, opencode, handover files like PASSATION.md) and read what was asked, done, left and decided; merge it all into a checked picture of the project (what it is, its state, what is done / in progress / left, the rules of the user, the traps) and report it tersely. Read-only. Use whenever the user asks to set up / load / get the context of a project or folder ("setup le contexte", "lis tout le repo", "prends connaissance du projet", "onboard", "regarde ce qui a été fait"), or starts working on a project this conversation knows nothing about.
---

# Onboard: the context of a project, before working on it

Read everything that says **what the project is**, **where it stands** and **what earlier sessions already did**, then hold
it as the context of this conversation. Read-only: nothing built, changed, committed, stopped or resumed.

## 0. Scope
- The folder given, else the cwd. Inside a git repository: its root (a subfolder of a monorepo: that part, plus the root
  files that govern it - README, CLAUDE.md, build, CI).
- Huge (thousands of files): the structure and the central parts read fully, the rest skimmed; propose a fan-out to
  subagents before using one (say how many and why), never on your own.

## 1. The project
- **Tree**: `git ls-files` (else `find`, skipping `.git`, `build*`, `node_modules`, `.venv`, `dist`, caches), sizes, languages.
- **Docs, read entirely**: README*, CLAUDE.md / AGENTS.md / `.cursorrules` / `.github/copilot-instructions.md` (rules to
  follow from now on), CONTRIBUTING, CHANGELOG, `docs/`, wiki pages in the repo, TODO / NOTES / PASSATION.md / handover files.
- **License and credits**: LICENSE / COPYING / NOTICE, SPDX headers and file banners, third-party credits, the licenses
  of the dependencies when they matter.
- **Build and config**: CMakeLists / Makefile / package.json / pyproject / Cargo.toml / go.mod..., setup / install scripts,
  CI workflows (`.github/workflows`, `.gitlab-ci.yml`), Dockerfiles, config templates, `.gitignore`, pre-commit hooks.
- **Code**: entry points and the module layout first, then every file (a big one: its structure and the parts that
  matter); the tests (what they cover, how they run); `TODO` / `FIXME` / `XXX` / `HACK` (grep, with their place).
- **Links**: the remotes, homepage, issues, CI badges, docs sites, submodules, dependencies. With a GitHub remote and
  `gh`: `gh repo view`, `gh issue list`, `gh pr list`, `gh release list` (read-only); other URLs only when they matter.
- **Secrets**: never copied - `.env`, keys, tokens, credentials: their existence and place only.

## 2. Its history (git)
- `git log` (subjects of everything, `--stat` of the recent ones), the conventions of the messages, the authors, the pace.
- Branches (local and remote) and how far each is from the main one; tags / releases; worktrees (`git worktree list`);
  stashes (`git stash list`, never popped); submodules.
- The work not in the history: `git status`, `git diff --stat` (and the diff of what matters), unpushed commits,
  untracked files that look like work in progress.

## 3. Earlier sessions on this computer
Search by the absolute path of the project (and its parents / subfolders / worktrees), newest first:
- **Claude Code**: `~/.claude/projects/<the path, every / and . turned into ->/*.jsonl` (worktrees:
  `<repo>--claude-worktrees-<name>`), and its `memory/` (MEMORY.md and the files it lists); `~/.claude/todos`, plans.
- **claude-cluster**: `claude-cluster list --rtk` and `claude-cluster external --rtk` (every instance: `instances/*` in
  `~/.local/share/claude-cluster`), the sessions whose folder is the project, their transcripts (`logs/`), tasks, schedules
  and todo lists; a running session is only read, never sent anything.
- **Other agents**: Codex `~/.codex/sessions`, Qwen Code `~/.qwen/projects`, opencode `opencode session list`, Gemini
  `~/.gemini` - grep the project path.
- **Handover files** anywhere in the project or its parent: PASSATION.md, HANDOFF.md, NOTES.md.
- From each conversation: the requests of the user (their words for the important ones), what was done, what was left
  or failed, the decisions and refusals, the rules given. A long transcript is never dumped: extract the user messages,
  the last assistant messages and the errors (`jq` / a short python script), then read what matters.

## 4. Merge and check
- Every claim of a session or a doc checked against the code and git ("done" in a transcript but absent from the code:
  not done). Contradictions: the code and the history win - say so.
- Sort into: what the project is; done; in progress (with the files touched); left to do; blocked; the decisions; the
  rules of the user (git, style, language, what never to do); the traps met (flaky tests, quotas, tools missing).

## 5. Report
- To the user, terse (`rtk`): the project in two lines, its state (branch, last commit, pending work), what earlier
  sessions did and left (dated), the rules to follow, then the possible next steps - the most useful first. Open questions
  only when something really cannot be settled from what was read.
- Kept in this conversation as its context. A file (CONTEXT.md, PASSATION.md) only when the user asks; facts worth
  keeping across sessions offered for the memory, never written without being asked.
