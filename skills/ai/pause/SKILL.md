---
name: pause
description: Stop everything cleanly in the middle of a task so that it can go on later - no new step started, the step under way brought to a consistent state (never a file half edited or a build left broken), every background job / monitor / subagent / workflow / build / loop / scheduled wake-up started for the task stopped, nothing committed or pushed that the rules of the user do not allow, then a checkpoint written (goal, rules, done, interrupted, the exact next steps, what was stopped and how to start it again, files changed, decisions) and one short message - the work resumes at the next "continue" (continue-work). Use whenever the user says /pause, "stop tout proprement", "arrête-toi proprement", "mets en pause", "on reprendra plus tard", "stop et reprends au prochain continue", "pause the work", "stop cleanly".
metadata:
  summary: 'Stops the task cleanly to resume it later: no new step, the current one left consistent, every background job / monitor / agent / build / loop it started stopped, nothing committed beyond the rules, then a checkpoint (done, interrupted, exact next steps, what to restart) and one short message; the next `continue` resumes it (`continue-work`).'
---

# Pause: stop cleanly, resume at the next "continue"

`SKILL_DIR` = the directory of this file. The user wants the work **stopped now, cleanly**, and taken up again later
by a plain `continue` (the `continue-work` skill), in this session or a later one, without re-explaining anything.
It is not a handover to another machine or model (that is `export`), and not the end of the task.

## 1. Stop starting things
- From now on, start **no new step**: no new edit, command, build, agent or question.
- The step under way is brought to the **nearest consistent state**, the shortest way:
  - a file edit in progress: finish it so the file is valid (syntax, matching blocks), or put the file back as it
    was before the step; never leave a file half written;
  - a change spread over several files (a rename, an API change): finish it if it is a few edits, otherwise revert
    the part of this step and note it in the checkpoint as not started;
  - a long command (build, test run, download): stop it (below) rather than wait, unless it ends within a minute.
- No question to the user (the popup included): what would need an answer goes in the checkpoint.

## 2. Stop what runs for the task
Only what this session started for the task, never a process of the user:
- background shell commands and monitors: `TaskStop` each one still running;
- subagents, forks, workflows: `TaskStop` (a workflow keeps its run id: note it, it can be resumed);
- a `/loop` running: `ScheduleWakeup` with `stop: true`; crons created for the task: `CronDelete`;
- processes it launched outside the task tools (a server, a build in another folder): stop them by their exact
  command line (`pgrep -fa` to check first), never a broad `pkill`;
- a temporary branch or worktree it made: leave it, write its path in the checkpoint (never delete work).
For each one, note how to start it again (the command), or that it is not needed any more.

## 3. Leave the repository as the rules say
- Commit or push **only** what the rules of the user already allow for this repository (a project rule granting
  commits after each batch: commit the finished part, never a half step). Otherwise nothing is committed: the
  changes stay in the working tree and are listed in the checkpoint.
- Nothing destructive to "clean up" (no `git reset`, no deleted file the user did not ask to delete).

## 4. Write the checkpoint
```bash
bash SKILL_DIR/scripts/checkpoint.sh path      # the file of this project (~/.claude/pauses/<project>.md)
```
Fill `templates/checkpoint.md` into that file (one per project, a new pause replaces the old one), terse like `rtk`
(facts, one per line, no prose), every path and command exact:
- the goal in the user's words, the rules that apply (git, language, style, what needs a confirmation);
- **done** (verified how), **interrupted** (its exact state), **next** in order - the first item is what
  `continue` runs first;
- what was stopped and how to restart it, the files changed and not committed, the temp files, decisions, open points.
A secret is never written in it (a token, a key, a password: say where it comes from instead).

## 5. Answer and stop
One short message in the user's language, then **no more tool call**:
- `En pause.` + what was stopped (one line), the state of the step under way, where the checkpoint is;
- `Dis « continue » pour reprendre à : <next step>.`
Other checkpoints waiting on this computer: `checkpoint.sh list`.
