+++
title = "Work method"
summary = "Tell claude-cluster how you like the work organised: presets, your own text, and the optional proposal from your habits."
section = "concepts"
order = 90
keywords = "work method preset list-review one-task plan-first custom detect work_method propose"
+++

**Settings > Agent > Work method** (`[agent] work_method`, `work_method_text`) states how you like work to be organised. The global session splits what it dispatches accordingly. Only you change it: the setting is protected.

## Presets {#presets}

| Preset | Meaning |
|---|---|
| `free` | no method (default) |
| `list-review` | a task list, then a review, repeated until good: the global session splits a request into a numbered list (or a `session_fanout`), asks a session for a review, sends the defects back as a new list, until a review finds nothing |
| `one-task` | one task at a time: one task per prompt, its answer awaited before the next |
| `plan-first` | a plan first (no edits), shown to you, then the go-ahead |
| `custom` | only your own text |

Your **free text** (400 characters at most) is added to the preset. The method is injected, short, in the system prompt addition of the sub-sessions and in the routing guidance of the global session; each takes it at its next start. The MCP tool `work_method` lets the global session read it.

## Proposals from your habits {#detect}

With `work_method_detect = true` (off by default) the global session may **propose** a method from how you phrase requests. Its tool gets a small sample read from the transcripts with **no model call**: counts of prompts with task lists, asking a review, asking a plan, one-liners, and about ten of your recent prompts, 220 characters each. This happens only when detection is on, once per `work_method_every` sessions it started (20), or when you ask.

A proposal is a question on every interface (a popup: **Allow** / **Deny**, with a notification). Only Allow writes the setting; Deny is final for that proposal and is told to the global session. It is never applied silently. State: `method.json` in the data folder.
