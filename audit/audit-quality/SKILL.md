---
name: audit-quality
description: Audit the cleanliness and conventions of a project (the user's coding style, comments, file layout, CMake, git history, tests coverage, docs, hygiene) and deliver the result as a PDF report in the user's report style. Use whenever the user asks for an audit of cleanliness / quality / conventions / style / code hygiene, a "rapport de propreté", or a review of a whole project against his conventions (not for bugs/UB: audit-bugs).
---

# Cleanliness / convention audit -> PDF report

`<name>` (a skill) = the folder of that skill: `~/.claude/skills/<name>` (setup.sh) or `${CLAUDE_SKILL_DIR}/../../*/<name>` (plugin of the marketplace).

Report only: **never modify the project** during the audit (fixes only if asked afterwards).
`SKILL_DIR` = directory of this file.

## 1. Ask (one AskUserQuestion call, French) - only what the request doesn't say
- Scope: whole project / a module / the changes since a ref.
- Compiler warnings pass (`--build`, slower: separate build dir in /tmp).
- Output: ask the output format of `report` (`Markdown + PDF` recommended, PDF only, Markdown only); location `audit/` at the root of the repository (or of the current folder outside a repository), never `docs/` unless asked (`audit/<YYYY-MM-DD>-quality.{md,pdf}`); language English unless asked.
Never start subagents (forks) by yourself: propose them only for very big projects and wait for the user's yes.

## 2. Objective metrics
```bash
python3 SKILL_DIR/scripts/collect.py <root> --json /tmp/audit.json --md /tmp/audit-tables.md [--build]
```
Categories (every finding has `file:line`): `headers` (Xartania/Epitech header missing), `guards` (`NAME_H` not
matching the file), `auto` (outside iterators / structured bindings / lambdas), `using-namespace`, `void-params`
(function with a return type and `()`), `c-cast`, `null`, `tabs`, `trailing-spaces`, `todo`, `cmake` (source not
listed, GLOB), `gitignore` (outputs not ignored), `commits` (not `type(scope): message`), `ai-attribution`,
`untested` (public names never referenced by the tests, via `cpp-tests`), `docs` (README/CHANGELOG/LICENSE missing,
CHANGELOG version != CMake version), `warnings` (with `--build`).
The script is heuristic: check a sample of each category before reporting it, drop the false positives.
When `xstyle` is installed (`command -v xstyle`, built by the repository `setup.sh`), also run
`xstyle --rtk -r <root> -S -o /tmp/xstyle.json` (the JSON for the counters, `xstyle --rtk -r <root>` to read the issues): ~65 rules of `cpp-style` / `cpp-comments` / `coding-style` / libutils usage
with a severity (unforgivable / major / minor / negligible) and the counters by rule; use its counters in the report
(sample-check them as well) and point to `xstyle --fix [CODES]` for the fixable ones.

## 2b. Dependencies (licenses + vulnerabilities, transitive)
```bash
python3 <audit-deps>/scripts/deps.py <root> --transitive --json /tmp/deps.json --md /tmp/deps.md
python3 <audit-deps>/scripts/vulns.py <root> --json /tmp/vulns.json --md /tmp/vulns.md
```
Licenses to credit / restrictive / unknown (see `audit-deps`), known vulnerabilities and compromised versions
(OSV, dnf advisories, GitHub advisories), **recent** ones (< 90 days) first; for each one check whether the project
uses the vulnerable part. Dependencies of the dependencies are included.

## 3. Manual review (what a script can't see)
Load the convention skills and read a representative sample of each module (most recent files first):
- `cpp-style` (naming, braces, `this->`, one-liners, const, alignment), `cpp-comments` / `comments`, `cpp-class`
  (`reference/layout.md`: sections, class blocks order, rule of five, include comments, namespaces),
- `cmake-style` (section order, banners, modes, packaging), `git-conventions` (history, tags, CHANGELOG),
- `readme-style` / `html-doc` (docs), `cpp-tests` (tests structure), `license` (`scripts/license.py identify`).
Also: dead code, duplicated code, very long functions, magic numbers, inconsistent naming between modules,
files in the wrong folder, leftovers at the root (`tmp.cpp`, `a.out`...).

## 4. Score and priorities
Per category: number of findings, severity (**high**: breaks the build/CI/conventions everywhere, **medium**:
recurrent deviation, **low**: cosmetic), a 0-10 score, the fix (and if it can be automated: `sed`, script,
clang-format...). Then a **top 10** of the actions with the best gain / effort ratio.

## 5. Report (report skill)
Write the Markdown from `report/templates/report.md`, sections:
1. **Summary**: table `| Category | Findings | Severity | Score /10 |` + global score + 3-line verdict.
2. **Method**: scope, commit (`git rev-parse --short HEAD`), date, tools and skills used, limits (heuristics).
3. **Findings by category**: one `###` per category, a short explanation of the rule (with the skill it comes
   from), a table `| File:line | Finding | Fix |` (max ~15 rows, the rest in the annex).
4. **Dependencies**: licenses (table + obligations) and vulnerabilities (table by severity, recent first, fixed
   version, action).
5. **Priorities**: the top 10, numbered `1. **Action.** gain, effort, files`.
6. **Appendix**: full lists per category (from the JSON), module sizes table.
7. **Sources**: the skills / rules / tools used.
Then `python3 <report>/scripts/md2pdf.py <report.md> --footer "<Project> — quality audit" --format <answer>`,
check the rendering (pdftoppm on 2 pages) and give both paths + page count.
