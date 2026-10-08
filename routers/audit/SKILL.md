---
name: audit
description: Router running the audits of the current project - asks which ones to trigger (checklist - bugs/UB, quality/conventions, dependencies licenses + vulnerabilities, tests coverage, performance - coverage and benchmark checked and run automatically) and the report format, then runs the selected audit skills one after the other and writes a summary report linking them. Invoke manually with /audit.
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
   - `Couverture des tests` -> `coverage` (C++ with tests: measured with llvm-cov) or `tests` (other languages)
   - `Performance` -> `benchmark` (profiling of the main scenario of the project)
2. **Format du rendu**: `Markdown + PDF (Recommandé)` · `PDF seulement` · `Markdown seulement` (passed to every audit,
   they don't ask again).
If the request already names the audits / format, skip what is known. Audit specific questions (ex: `audit-bugs`
scope, mode, iterations, fixes) are asked by that audit when it starts, in one call.

`coverage` and `benchmark` are **checked by default and run automatically** like the others, in audit mode:
- `coverage`: measure + `uncovered.md` + its report only (no test written during an audit: the "fill" part of the skill
  is proposed in the summary, its end-of-run question is answered by the audit's format choice); skipped with the reason
  when the project has no tests or no clang / llvm-cov.
- `benchmark`: the obvious scenario of the project without asking (the main binary with a typical input found in the
  README / tests / examples, else the test suite in an optimized build), quick depth (perf + time), report only, no
  optimization applied; skipped with the reason when nothing can be run (library without benchmark target or tests).

`xstyle` (when installed: `command -v xstyle`) is always run with **`--rtk`** by the audits (compact output made for
the assistant; `-o <file>.json` for counters kept in a report): `audit-quality` uses all its rules, `audit-bugs` the ones
that hide bugs. Not installed: say once that `curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install xstyle`
(no curl: `wget -qO-` instead of `curl -fsSL`) would add those checks.

## 3. Run (in this order, sequentially)
1. `audit-deps` first when selected: its JSON (`/tmp/deps.json`, `/tmp/vulns.json`) is reused by `audit-quality`
   and `audit-bugs` instead of running the scripts again.
2. `audit-quality`, 3. tests coverage, 4. `benchmark`, 5. `audit-bugs` (the longest, it may fix things only after
confirmation).
Each audit writes its own report `audit/<YYYY-MM-DD>-<audit>.{md,pdf}` (English unless asked, `report` style).
An audit that fails or can't run (no build system, missing tool) is reported as such, the others continue.

## 4. Summary report
`audit/<YYYY-MM-DD>-summary.{md,pdf}` through `report`: **Summary** table `| Audit | Verdict | High | Medium |
Low | Report |` (link to each report), the **top 10 actions** across all the audits (deduplicated, highest gain
first), what was not run and why, the commit audited. In the chat: the verdict per audit and the paths only.
