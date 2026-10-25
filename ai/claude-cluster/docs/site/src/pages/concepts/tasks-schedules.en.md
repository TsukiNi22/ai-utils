+++
title = "Tasks and schedules"
summary = "The task queue, todo lists and scheduled prompts: options, conditions, the Tasks session and who may do what."
section = "concepts"
order = 40
keywords = "task queue schedule cron after on_event priority lane retry timeout reminder tasks session task_add schedule_add max_parallel"
+++

## The Tasks session {#tasks-session}

`☰ Tasks` is the first entry of the list (`/tasks`, palette *Tasks*): it shows what the sessions do and what waits instead of a conversation.

- **Todo lists** of each session (the checklists of its own TodoWrite tools).
- **Task queue**: running, waiting, done, failed, cancelled, with who made each task, where it goes and why it was cancelled.
- **Scheduled**: reminders and prompts at a time, waiting or run.
- **Jobs**: the [orchestrator](page:orchestrator) queue, with gauges.

A click on a row opens its actions: start now (past the parallel limit), cancel or delete (a reason is asked and told to the session that made it), archive, open its session. Without the mouse: `↑↓` / `j` `k`, `Tab`, `Enter`. A closed session loses its waiting tasks and schedules.

## Tasks {#tasks}

A task is a prompt queued for a session. `claude-cluster task add [--target T] [--after t1,t2] <prompt>`, `task list`, `task cancel`, or the MCP tool `task_add`. `[sessions] max_parallel` sub-sessions work at once and `task_queue` bounds how many wait.

| `task_add` option | What it does |
|---|---|
| `after`, `after_sessions` | wait for tasks to be done / sessions to be idle |
| `on_event` | `session_idle`, `git_push` (a new commit on the remote), `command`, `file` (changed); checked in the background. A `command` is met when it exits 0 (or `code`) and prints what is asked (`stdout` exactly, `contains`, `regex`), run `every` N s (`constantly` = 2 s); `fail_after: N` gives up after N failures in a row (the task fails, its owner is told) |
| `spawn` | the task makes its own session (`cwd`, `name`, `backend`, `mode`, `effort`, `profile`, `one_shot`) |
| `priority`, `max_concurrency` + `lane` | higher first; at most N tasks of a lane at once |
| `timeout_minutes`, `retry`, `max_cost`, `max_tokens` | stopped and reported when exceeded, tried again after an error |
| `expect`, `notify`, `reply` | a short answer or a file as the result; a notification at its end; whether the author is told |

The answer is kept in the task (`result`, up to `[sessions] task_result_chars`) and `{{t3.result}}` in a prompt is replaced by that of task `t3`, which then runs first. `task_act`: cancel, delete, archive, `retry`, `set`, `force`. A task whose target does not exist, or whose dependency failed, fails with its reason, told to its author. Finished tasks are archived after 30 minutes.

## Schedules {#schedules}

`claude-cluster schedule add <when> <text> [--target s] [--every N] [--notify]`, `schedule list`, `schedule cancel`, or `schedule_add`, `schedule_list`, `schedule_edit`, `schedule_act`.

| `schedule_add` option | What it does |
|---|---|
| `when` / `every` / `cron` / `reset` | once (`+30m`, `HH:MM`, `YYYY-MM-DD HH:MM`), every N minutes, a cron expression (`*/15 9-17 * * mon-fri`), or at each reset of the 5 h / 7 d limit window |
| `days`, `window`, `quiet`, `tz` | only on these days, inside / never inside these hours, in this timezone |
| `until`, `max_runs` | ended after a time or a number of runs |
| `if` | `session_idle`, `command` (exits 0, or the `code` / `stdout` / `contains` / `regex` asked; `fail_after`: the schedule cancelled after N failures in a row), `file_changed`, `usage` (`5h>80`): not met, the run is skipped |
| `action` | `prompt` (default), `notify`, `spawn`, `task`, `command` (run locally with **no model**; only a non-empty output is sent or notified) |
| `ack`, `nag` | asked again every `nag` minutes until acknowledged |
| `dry_run` | nothing made: tells when it would fire |

`schedule_act`: pause, resume, snooze, ack, trigger (run now), cancel, delete, archive.

## Who may do what {#scope}

The global session manages everything. A sub-session of the claude driver gets its own small MCP (`task_add`, `task_list`, `task_cancel`, `task_act`, `schedule_*`), scoped to the tasks and schedules **it made**: its task runs for itself after its turn and its result comes back as `[tasks] ...`, a schedule wakes only itself, and local commands (`command` actions, `if.command`, `on_event: command`) and `force` are never its own. The `[agent]` settings: `sub_read` (on), `sub_tasks` (on), `sub_schedules` (off), `sub_spawn` (off), `sub_spawn_cap` (2).
