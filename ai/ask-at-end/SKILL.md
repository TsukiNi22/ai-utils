---
name: ask-at-end
description: Run the whole task without interrupting the user once - no question, no confirmation, no "should I go on?", no permission asked, no plan to approve, no pause - every doubt decided with the sensible default (noted), every blocker worked around or set aside, then at the very end one report and every open point asked at once through the questions popup (interactive-questions, or ideation for ideas / options). Use whenever the user says "ne pose aucune question", "ne m'interromps pas", "pose tes questions à la fin", "fais tout d'une traite", "je ne suis pas là", "no questions until the end", or summons /ask-at-end with a task.
---

# Ask at end: no interruption until the work is done

The user is away or wants the work done in one go. **Nothing may stop the run to wait for them**; what would have been a
question is decided, written down, and asked only at the end.

**Task given with the skill:** the text after `/ask-at-end` if any, else the latest request of the user.

## During the run: never interrupt
- **No question** (no AskUserQuestion, no question in the text that waits for an answer), **no confirmation**, no "should
  I go on / is it OK?", no plan sent for approval, no stop halfway to report.
- **A doubt** (a name, a scope, an approach, a detail): take the sensible default - the one the code, the conventions, the
  rules files, memory and earlier requests point to, else the safest and most reversible one - and **write it down** (the
  decision, why, what the other option would change).
- **A blocker** (a missing tool, a failing build, an access refused, a file locked by another session): try the
  alternatives (another approach, a workaround, a mock), else set that part aside, note it with its exact error, and go on
  with everything else. Never wait for the user.
- **Long work** (builds, tests, downloads): run it in the background and keep working; wait only when the next step needs it.
- **The user's hard rules still hold**: git (no commit / push unless allowed), no destructive action, no system change, no
  install, no secret sent, no fork / subagent spending without leave. An action that needs their consent is **not done**:
  it is prepared (the exact command, the diff) and put in the questions of the end. Waiting for the end never means
  doing something risky without asking.
- Keep a short list as you go: decisions taken, things set aside, things to confirm, ideas met.

## At the end: one report, then every question at once
1. **The report** (terse, `rtk`): what was done (files, results, tests with their real output), what was set aside and why,
   every decision taken by default (one line each), what is ready but needs their go (the exact command / diff).
2. **The questions**, all of them now, through the popup (`interactive-questions`; `ideation` when they are ideas, options
   or variants to choose from): 1-4 questions per call, 2-4 options each, the default already taken marked
   `(Recommended)` or "(déjà appliqué)", several rounds if there are more. Typical ones: confirm or change a default
   taken, the go for an action that needed it (commit, push, install, delete), the next step, the ideas met on the way.
3. **Act on the answers** at once (a changed default redone, the allowed action run), then a short final recap.
- Nothing open at the end: the report alone, no popup.
