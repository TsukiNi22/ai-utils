---
name: clarify
description: Before doing a request, find everything in it that is unclear, ambiguous, missing, contradictory or open to several readings (scope, target, names, behavior, language, format, risky or irreversible actions, "etc."), and ask the user what is really wanted through the built-in questions popup (interactive-questions), in several rounds when it does not fit in one, before changing anything. Use whenever the user summons it (/clarify, "clarifie", "pose-moi des questions avant", "si ce n'est pas clair demande-moi", "vérifie que tu as bien compris") or when a request has real doubts that change what must be done.
---

# Clarify the request before acting

The request is **not** done yet: first make sure it is understood. Everything unclear is asked to the user with
`interactive-questions` (AskUserQuestion popup, never a list in the text), then the work starts on the answers.

**Request given with the skill:** the text after `/clarify` if any, otherwise the latest request of the user.

## 1. Read the request and look
- Read the request as written, then the context that can answer by itself (the code, `git status`, the README,
  the conversation, memory): **never ask what can be found or decided from them**; a harmless default is taken and
  said in one line.
- Quick read-only look only (files, grep, `git`): no change, no build, no commit during this step.

## 2. List the doubts
Go through the request and keep only the points whose answer **changes what will be done**:
- **Target / scope**: which files, modules, projects, repositories, branches; how far (only this or everything
  similar); what must not be touched.
- **Ambiguous words**: "this", "it", "the repo", "etc.", "clean", "update", "like before", two possible meanings.
- **Missing details**: name, version, format, language, output (file / text / PDF), limits, deadline, defaults.
- **Several valid approaches** with different cost, risk or result (the choice is the user's).
- **Contradictions** between parts of the request, or with the code / earlier instructions / conventions.
- **Risky or irreversible actions** (delete, overwrite, force-push, publish, release, send, system change):
  what is allowed exactly.
- **Order and dependencies** between several tasks, what to do first, what to skip.
- **Assumptions** you would otherwise make silently and that could be wrong.

No real doubt left: say so in one line ("Rien d'ambigu : je fais ...") and do the task, without a popup.

## 3. Ask (skill `interactive-questions`, load it)
- Group the doubts by theme (scope, approach, details, risks) and ask them with AskUserQuestion: **1 to 4 questions
  per call, 2 to 4 options each**, French labels, the recommended option first, multi choice when the options add
  up, previews to compare concrete things.
- More than 4 doubts, or a doubt that depends on an earlier answer: **several rounds**, the blocking / structuring
  questions first (scope, approach), the details after, each round using what was answered before.
- Write each question so it can be answered without reading anything else: say what the doubt is and what each
  option changes. Never ask the same thing twice, never ask what the user already stated.
- "Explique plus" / a free text that opens a new doubt: answer it and ask again (see `interactive-questions`).

## 4. Confirm and act
- Read the answers exactly (the *Other* text and notes often change the option).
- Say in a few lines what will be done now (scope, approach, details chosen). Ask once more only when an answer
  left a real contradiction or the plan became risky; otherwise **do the task at once**, with the skills that fit
  (`/dev` routing, the style and git skills of the user...).
- A choice that must last (a preference, a convention, a name) goes to memory / CLAUDE.md / the project config
  only when the user wants it.
- Git rules stay the ones of the user's CLAUDE.md and `git-conventions` (nothing committed or pushed unless asked
  in the current request or allowed by the repository).
