---
name: continue-work
description: Resume a task stopped with the pause skill at the user's next "continue" - reads the checkpoint of the project (~/.claude/pauses, checkpoint.sh of pause), checks it against the real state (git status and diff, the files, what still runs) and trusts the real state when they differ (the user may have changed things meanwhile), starts again what was stopped and is still needed, redoes the interrupted step from its start, then goes on with the next steps in order without redoing what is done and without a recap; closes the checkpoint when the task is over. Without a checkpoint it simply continues the current task of the conversation. Use whenever the user says "continue", "reprends", "on reprend", "vas-y continue", "resume", "go on", "/continue-work" after a pause, or opens a session in a project that has a checkpoint waiting.
metadata:
  summary: 'Resumes at the next `continue` what `pause` stopped: reads the checkpoint of the project, checks it against the real state (git, files, what runs, the real state wins), restarts what is still needed, redoes the interrupted step, goes on with the next steps without redoing or recapping, closes the checkpoint at the end.'
---

# Continue: resume a paused task

`<name>` (a skill) = the folder of that skill: `~/.claude/skills/<name>` (setup.sh) or `${CLAUDE_SKILL_DIR}/../../*/<name>` (plugin of the marketplace).

The user says `continue` (or "reprends"...): go on with the task where it stopped, at once, without asking what to
do and without telling the whole story again.

## 1. Find where it stopped
```bash
bash <pause>/scripts/checkpoint.sh show       # the checkpoint of the current project, exit 1 when there is none
```
- A checkpoint: it is the plan. Several projects paused and the current folder is not one of them:
  `checkpoint.sh list`, take the one the conversation is about, else ask which one (`interactive-questions`).
- No checkpoint: a plain `continue` of the current task. Go on from the last step of the conversation (its todo
  list, the last message), nothing else of this skill applies.

## 2. Check it against what is there now
The checkpoint can be old: the user may have edited, committed, pulled or run things since.
- `\git status`, `\git log -3 --oneline`, `\git diff --stat`: compare with *State of the files* and the commit noted.
- The files of *Interrupted* and of the first *Next* steps: read them before editing.
- What was stopped: still stopped? (`pgrep -fa <command>`), a workflow run id to resume.
- A difference: **the real state wins**. A step the user did meanwhile is done; a change of theirs is kept (never
  overwritten, never reverted); a plan that no longer fits is adapted, said in one line. A contradiction that changes
  what to do (the goal itself moved, a decision undone): ask once, briefly (`interactive-questions`).

## 3. Resume
- Start again what was stopped and is still needed (the commands of *Stopped*, a workflow with its run id).
- The **interrupted** step: redo it from its start (its half-done part was finished or reverted by the pause).
- Then the **next** steps in order, under the same rules (git, language, style, confirmations) as before the pause.
- One line at the start at most (`Reprise : <step>.`), no recap of what was done.
- A new `pause` during the work: the `pause` skill again (the checkpoint is replaced).

## 4. Close
When the task is over (its last step done and verified): `bash <pause>/scripts/checkpoint.sh done` (moved to
`~/.claude/pauses/done/`, kept), then the usual final report of the task.
