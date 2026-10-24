---
name: export
description: Export everything needed to hand the work over to another machine, session or model into one compact file (PASSATION.md by default) - the goal and context, what is done, what runs, what is left (with the exact next step), the decisions and constraints of the user, the files and branches touched, the commands / tools / scripts used and how to build, test, run, the state of the environment, the traps met - written for an AI reader in the rtk way (terse facts, no prose, no token wasted), never a secret. Use whenever the user asks to export / hand over / pass the work on ("export", "passation", "handoff", "fais une passation", "exporte le contexte", "pour une autre session / machine / IA"), or summons /export.
---

# Export: a handover file for another session / machine / model

Write **one file** another AI reads to continue the work **without this conversation**: what is wanted, what is done,
what is left and exactly how to go on. Default `PASSATION.md` at the root of the project (the cwd); another name or
place only when the user says so. The reader is an AI: apply `rtk` (terse facts, one per line, no prose, no
decoration, nothing said twice) - complete, not pretty.

## 1. Gather (read-only)
- The conversation: the requests of the user (their words for the important ones), their answers to questions,
  their preferences and refusals, what was decided and why.
- The task lists (TaskList / todos) and their states; the background work still running (jobs, shells, builds,
  tests) and how to check it.
- The repository: `git status` / branch / last commits / unpushed work / worktrees / stashes, the files changed and
  not committed (`git diff --stat`), generated or temp files that matter (scripts kept in a temp folder: copy their
  path, or their content when the other machine will not have them).
- The environment: OS, the tools and versions that matter, how to build / test / run / install, the services or
  processes in use (never stop them), the settings / env vars changed.
- The memory and the project instructions (CLAUDE.md, rules the user gave) that the next one must follow.

## 2. Write it (sections in this order, empty ones dropped)
```
# PASSATION <project> <date> (<from: model / machine>)
## goal          the request(s) in one or two lines each; the user's own words when they matter
## rules         constraints of the user: git (commit / push allowed?), language, style, what never to do
## state         = done N · running N · todo N · blocked N
## done          - <what> (<where: file / commit / version>), checked how
## running       - <what> · check: <command> · expected: <result>
## todo          - [P1] <what> · next step: <exact action> · files: <paths>  (ordered: what to do first)
## blocked       - <what> · why · what unblocks it
## decisions     - <choice> · why · rejected: <alternative>
## files         $P=<common dir>; - $P/a.cpp: <what changed, uncommitted?>
## repo          branch · last commit · uncommitted / unpushed · worktree · how the release / commit is done
## env           os · tools (versions) · build: <cmd> · test: <cmd> · run: <cmd> · install: <cmd>
## tools         the scripts / helpers / skills used and for what (path + one line), the MCP / services relied on
## traps         - <what went wrong> → <how it is avoided> (flaky tests, quotas, guards, aliases...)
## refs          docs, issues, URLs, paths of logs / screenshots / reports
```
- Every item actionable: a path, a command, an id, a version, an exact error text - never "see above" or "as
  discussed". Relative paths under a `$P` written once.
- `todo` is the core: ordered, each with **its next step** precise enough to start without asking.
- What the next one must **not** do (destructive commands, processes to keep, settings not to change) goes in `rules`.
- **Never a secret**: no token, password, key, cookie, private URL with credentials, personal data beyond what the
  project needs; write where it lives instead (`keyring: claude-cluster`, `env: API_KEY`).
- Long logs / diffs: never pasted; their path, or the 1-3 lines that matter.

## 3. Check and report
- Read the file as the next AI: can it continue alone (what, where, how, in which order, what not to touch)? Fill
  the holes; remove what it does not need.
- Measure (`wc -c`, ~4 chars per token) and keep it short: a big context compressed, not copied.
- Tell the user the path, the size and the counts in one or two lines; not committed unless they ask (or the
  project's rules allow it and they asked for a commit).
