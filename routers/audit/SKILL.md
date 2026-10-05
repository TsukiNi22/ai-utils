---
name: audit
description: Router running the audits of the current project - asks which ones to trigger (checklist - bugs/UB, quality/conventions, dependencies licenses + vulnerabilities, tests coverage) and the report format, then runs the selected audit skills one after the other and writes a summary report linking them. Invoke manually with /audit.
disable-model-invocation: true
---

# Audit router

**Request given with `/audit`:** $ARGUMENTS

Load the skills with the Skill tool (never the other routers: they are manual-only). Never start fork agents; an
audit that offers parallel agents asks the user first (its own rule).

## 1. Context
- Project root: `git rev-parse --show-toplevel` (or the current folder outside a repository), languages / build
  system, `git log --oneline -1` (the commit audited).
- Output folder: `audit/` at that root (created if missing), never `docs/` unless asked.

## 2. Ask (one AskUserQuestion call, French questions)
1. **Quels audits lancer ?** (`multiSelect: true`, all checked = recommended):
   - `Bugs / UB` -> `audit-bugs` (sanitizers, tests, review; slow on big projects)
   - `Propreté / conventions` -> `audit-quality`
   - `Dépendances` -> `audit-deps` (licenses to credit / restrictive, known vulnerabilities, transitive)
   - `Couverture des tests` -> `cpp-tests` (C++, `scripts/untested.py` + test run) or `tests` (other languages)
2. **Format du rendu**: `Markdown + PDF (Recommandé)` · `PDF seulement` · `Markdown seulement` (passed to every audit,
   they don't ask again).
If the request already names the audits / format, skip what is known. Audit specific questions (ex: `audit-bugs`
scope, mode, iterations, fixes) are asked by that audit when it starts, in one call.

## 3. Run (in this order, sequentially)
1. `audit-deps` first when selected: its JSON (`/tmp/deps.json`, `/tmp/vulns.json`) is reused by `audit-quality`
   and `audit-bugs` instead of running the scripts again.
2. `audit-quality`, 3. tests coverage, 4. `audit-bugs` (the longest, it may fix things only after confirmation).
Each audit writes its own report `audit/<YYYY-MM-DD>-<audit>.{md,pdf}` (English unless asked, `report` style).
An audit that fails or can't run (no build system, missing tool) is reported as such, the others continue.

## 4. Summary report
`audit/<YYYY-MM-DD>-summary.{md,pdf}` through `report`: **Summary** table `| Audit | Verdict | High | Medium |
Low | Report |` (link to each report), the **top 10 actions** across all the audits (deduplicated, highest gain
first), what was not run and why, the commit audited. In the chat: the verdict per audit and the paths only.
