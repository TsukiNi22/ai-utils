+++
title = "One-shot sessions"
summary = "Sessions made for one precise task that archive themselves once their answer was relayed."
section = "concepts"
order = 150
keywords = "one-shot one_shot archive_one_shot session_spawn temporary session trash"
+++

## What it is {#what}

When the global session starts a session for **one precise task** (a lookup, a review, a single build), never for a project it will talk to again, it sets `one_shot: true` in `session_spawn` (or `spawn: {one_shot: true}` in a task). Once the session's answer was relayed, the session is **closed to the trash** (restorable) instead of piling up in your list.

## When it stays {#stays}

The flag is honoured only while `[sessions] archive_one_shot` is on (default). The session is kept when:

- you typed in it, or you are looking at it right now;
- it waits for a permission or a question;
- another prompt is queued for it;
- its turn failed or was interrupted.

It needs `reply` to be on, since the archive follows the relayed answer. Restore it from the trash like any session: `Ctrl+Shift+T`, `/restore`, `claude-cluster restore <s>`.
