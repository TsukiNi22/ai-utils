---
name: interactive-questions
description: Ask the user through the built-in interactive questions (AskUserQuestion, a QCM / Q&A popup with options) instead of a list in the text - questions, proposals, ideas, choices, rounds of ideas, a decision between approaches, what to do next - in Tsukini's way (French labels, 1-4 questions per call, 2-4 options each, the recommended one first, multi choice when they add up, previews to compare, the answers read and followed exactly). Use whenever the user says "propose en qcm", "q&a", "demande-moi", "pose-moi des questions", "propose plusieurs choix / idées", "laisse-moi choisir", or when several options exist and the choice is really theirs.
---

# Interactive questions (QCM / Q&A)

When the user has to choose, ask with the **AskUserQuestion** tool (the built-in popup: options, *Other* typed,
multi choice, previews), never with a numbered list in the answer. It also works in claude-cluster: the question of a
sub-session pops up there (terminal, window, web page, phone, the voice), and *Global answers* lets its global session
answer it.

## When
- The user asks for it: "propose en qcm / q&a", "demande-moi", "pose-moi des questions", "propose plusieurs idées /
  rendus / choix", "laisse-moi choisir", "si tu as d'autres idées propose-les".
- A real decision of theirs: several valid approaches with different costs, a name, a visual style, what to keep,
  the scope of a change, which ideas of a brainstorm go in the backlog.
- Not for what can be decided from the request, the code or a sensible default (pick it, say it in one line), nor to
  ask "should I go on?" / "is it OK?" (do it; a plan is confirmed by its own mechanism).

## How
- One call with **1 to 4 questions** asked together (related ones in the same popup), each with **2 to 4 options**;
  *Other* is added by the tool: never write an "Other" option.
- Text in **the language of the user** (French here): `question` ends with `?`, `header` ≤ 12 characters (a chip:
  "Nom", "Style", "Portée"), `label` 1-5 words, `description` = what it means, its trade-off (cost, risk, time).
- The **recommended** option first, its label ending with `(Recommended)`; at most one per question.
- `multiSelect: true` when the options add up (ideas to keep, features, files, notifications...); false when they
  exclude each other.
- `preview` (single choice only) to compare concrete things side by side: ASCII mockups of a UI, code, a config, a
  diagram, an icon / logo drawn in text.
- Many ideas: several rounds (4 questions × 4 options max per call), grouped by theme ("Claude Code", "Confort",
  "Contrôles"...), then a short recap of what was kept (and stored as tasks / backlog when asked).
- A question whose answer needs explaining first: explain in a few lines, then ask. "Explique plus" / "clarifie" as an
  answer: detail each option (what happens, an example, its risks), then ask again with the same options refined.

## The answers
- Read them **exactly**: a free text (*Other*) or the notes often change the option (a constraint, "mais...", "et
  aussi...", a mix of two): follow what is written, not the label alone.
- No answer to a question (skipped): don't guess what matters; ask it again alone, or take the recommended one only
  when it is harmless and say so.
- Remember them for the rest of the conversation (never ask the same thing twice); a choice that must last
  (preferences, style, a name) goes where it lasts (memory, CLAUDE.md, the project config) when the user wants it.
- Then act at once on the choices, and say in one line what was chosen.

## Example
```json
{"questions": [
  {"question": "Quel nom pour le skill ?", "header": "Nom", "multiSelect": false, "options": [
    {"label": "ask-choices (Recommended)", "description": "Court et explicite"},
    {"label": "qcm", "description": "Le mot que tu utilises, moins parlant en anglais"}]},
  {"question": "Quelles idées garder ?", "header": "Idées", "multiSelect": true, "options": [
    {"label": "Rewind", "description": "Revenir à un point de la conversation"},
    {"label": "Revue du plan", "description": "Valider un plan avant qu'il soit exécuté"}]}
]}
```
