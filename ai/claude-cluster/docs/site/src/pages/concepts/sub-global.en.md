+++
title = "Sub-global per category"
summary = "Planned: one lead session per category that drives the sessions of its category, under the global session."
section = "concepts"
order = 30
keywords = "sub-global subglobal lead category planned max_leads ask auto off integrate compile git push"
+++

> [!PLANNED]
> The sub-global is a design, not a feature of the current version: nothing below exists in the code yet, and the setting names may change. The global session, categories and [taxonomy](page:taxonomy) it builds on do exist.

## The idea {#idea}

With many projects the global session knows everything and carries the context of everything. A **sub-global** is a lead for **one category** (for example `work`, `perso`, `tools`): it knows only that category, drives the sessions of its projects and reports to the global session, which keeps the overview.

```text
global
├── sub-global · work   ── session A, session B
├── sub-global · perso  ── session C
└── sub-global · tools  ── session D
```

## Design {#design}

- **At most one** sub-global per category.
- It is a normal session of the claude driver with a restricted tool set and a prompt that names its category; it costs tokens like any session, so it is created only when it is useful.
- The global session **judges whether it is useful** (many active sessions in a category, a long running theme) and **always asks you by a popup** before creating one; it never creates one silently.

## Rights {#rights}

| It may | Condition |
|---|---|
| manage the sessions of its category: prompt, interrupt, close, move, describe | its category only |
| integrate and compile **once** for its category | through the [orchestrator](page:orchestrator), so builds still take turns |
| create tasks and schedules for its category | [tasks and schedules](page:tasks-schedules) of its category only |
| `git push` | **only with your OK**, asked each time |

It cannot change settings, other categories, the policy or the protected settings.

## Setting {#setting}

A setting chooses the behaviour: `ask` (the global session proposes and you decide, the default), `off` (never), `auto` (created without asking, within `max_leads`, the maximum number of sub-globals alive at once). Even in `auto` the global session judges usefulness first.

## Until then {#today}

You can get a similar result today with a category (`session_describe`), a dedicated sub-session as a lead, and tasks addressed to it; the global session then talks to that lead instead of every session.
