---
name: setup-context
description: Set up everything this conversation needs before the work starts - read the current repository (tree, docs, rules files, build, code, git state), search the earlier and archived sessions on this computer (Claude Code transcripts and memory, claude-cluster sessions and their trash, Codex / Qwen / opencode, handover files) for what was asked, done, left and decided, then load the skills that fit the project (style, language, build, tests, docs, git) and check the MCP servers / tools it would use (configured, connected, missing), and hold it all as the working context; the questions only at the end. Use whenever the user says "setup le contexte", "prépare-toi", "charge tout ce qu'il faut", "lis le repo et les anciennes sessions", "setup les skills / mcp utiles", or summons /setup-context before a task.
---

# Setup context: the project, its history, the right skills and tools

Read what the project is and what was already done, then **equip the session**: the skills loaded, the MCP servers / tools
checked. Read-only on the project: nothing built, changed, committed, stopped or resumed.

**Scope given with the skill:** the folder / task after `/setup-context` if any, else the cwd and the latest request.

## 1. The project and its history
- The `onboard` skill installed: follow its steps 0-4 (the project, its git history, the earlier sessions, merged and
  checked). Else do the same by hand:
  - **Tree** (`git ls-files`, sizes, languages), **docs read entirely** (README*, CLAUDE.md / AGENTS.md and the other
    rules files: rules to follow from now on, CHANGELOG, `docs/`, TODO / NOTES / `.info` / PASSATION.md), **build and CI**
    (CMakeLists, package files, workflows, hooks), **code** (entry points, module layout, tests, TODO / FIXME).
  - **Git**: log (recent `--stat`), branches and worktrees, tags, `git status` / diff of the pending work, unpushed commits,
    stashes (listed, never popped).
- Secrets: their existence and place only, never their content.

## 2. Earlier and archived sessions
Search by the absolute path of the project (its parents, subfolders, worktrees too), newest first:
- **Claude Code**: `~/.claude/projects/<path with every / and . turned into ->/*.jsonl` (worktrees:
  `<repo>--claude-worktrees-<name>`), its `memory/` (MEMORY.md and its files), `~/.claude/jobs/*/state.json` (background
  jobs: their state, what they wait for).
- **claude-cluster**: `claude-cluster list --rtk` and `claude-cluster external --rtk`; every instance
  (`~/.local/share/claude-cluster/instances/*`); **the archived sessions too** (the trash of each instance, kept 7 days by
  default) and their transcripts (`logs/`), the tasks, schedules and todo lists of the project. A running session is only
  read, never sent anything.
- **Other agents**: Codex `~/.codex/sessions`, Qwen Code `~/.qwen/projects`, opencode `opencode session list`, Gemini
  `~/.gemini` (grep the project path); handover files (PASSATION.md, HANDOFF.md, NOTES.md) in the project or its parent.
- From each one: the user's requests (their words for the important ones), what was done, left or failed, the decisions,
  refusals and rules. A long transcript is never dumped: extract the user messages, the last assistant messages and the
  errors (`jq` / a short python script), read what matters.
- Every claim checked against the code and git: "done" in a transcript but absent from the code is not done.

## 3. The skills that fit
- List the installed skills (the listing of this session) and pick the ones the project and the task call for: its
  languages' style skills (`cpp-style`, `python-style`, `coding-style`, `cmake-style`, the comments skills), its build /
  tests / docs / git skills, `libutils` when it uses libutils, the routers when the work spans several (`dev`, `audit`).
- **Load them now** (Skill tool), the most central first; never more than the task needs (each costs context). Say in
  one line which ones are loaded and why.
- The rules files of the project win over a skill when they disagree: say so.

## 4. MCP servers and tools
- The MCP servers configured (`claude mcp list`, the project's `.mcp.json`, `~/.claude.json`), connected or failing, and the
  deferred tools of this session (ToolSearch): which ones the task would use (GitHub, a browser, docs, a database, the
  claude-cluster MCP...).
- The CLI tools the project needs (compilers, `cmake`, `gh`, `xstyle`, package managers, linters): present
  (`command -v`) and their version, missing ones noted - **never installed or configured without asking**.
- A useful server or tool missing: noted with its exact setup command, proposed at the end (never added silently).

## 5. Report, then the questions
- Terse (`rtk`): the project in two lines, its state (branch, last commit, pending work), what earlier sessions did and
  left (dated), the rules to follow, the skills loaded, the tools ready / missing, the possible next steps.
- **Every question only at the end**, through `interactive-questions` (a popup, never a list in the text): which next step,
  a contradiction found, a missing tool / MCP to install, an old session to resume. Nothing unclear left: no popup.
- A fact that must last (a rule, a convention) goes to memory / CLAUDE.md only when the user wants it.
