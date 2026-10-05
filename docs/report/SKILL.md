---
name: report
description: Deliver a result (report, audit, review, study, benchmark, summary) as Markdown and/or PDF in the user's report style (measured on the Convertly docs - WeasyPrint, A4, Noto Sans 9 pt, navy (1a3d7c) title with a navy rule under it, navy table heads, light blue callout with navy bar, zebra tables, mono code on grey, footer "title - page / pages"), English by default, converted with scripts/md2pdf.py. Use whenever the user asks for a PDF, a report/benchmark/audit/study, or another skill needs a report output.
---

# Reports in the user's style (Markdown + PDF)

`SKILL_DIR` = directory of this file. Requires `weasyprint` + `python3-markdown` (installed here) and the Noto fonts.
The style is `templates/report.css`: never restyle unless the user asks. Start from `templates/report.md`.

## 1. Ask once (AskUserQuestion, French question, per request)
**Format du rendu** : `Markdown + PDF (Recommandé)` · `PDF seulement` · `Markdown seulement`.
Skip the question when the user already said it. Several reports in the same request share the answer.

## 2. Language and location
- **English by default** (titles, tables, text), another language only when the user asks for it.
- Location: the one given by the user, otherwise the default of the calling skill (audits: `audit/` at the root of
  the repository, or of the current folder outside a repository); never `docs/` unless asked.
- File name: `<YYYY-MM-DD>-<subject>.md` / `.pdf` (ex: `audit/2026-10-03-quality.pdf`).

## 3. Convert
```bash
python3 SKILL_DIR/scripts/md2pdf.py <report.md> --format both|pdf|md [-o report.pdf] [--footer "Project — subject"] [--lang en]
```
`both` keeps the `.md` next to the `.pdf`; `pdf` removes the `.md` after a successful conversion; `md` writes no PDF.

## 4. Structure (from the reference documents)
1. `# Project: subject` (one H1, underlined by the navy rule; it is also the footer text unless `--footer`).
2. Meta line: `**Date**: October 3, 2026 · **Sources**: ...` (code paths in backticks).
3. Callout `> **How to read this document**` + numbered list of the parts + the assumptions / limits / what is
   **not** included.
4. `## Contents`: plain numbered list of the H2.
5. `## 1. Summary`: table `| Point | Conclusion |`, first column in **bold**, key numbers in **bold**.
6. Numbered parts `## N. Title`, sub-parts `### N.M Title`, tables for every comparison, numbered findings
   `1. **Finding.** explanation`, short `> **Key takeaway**: ...` callouts.
7. `## N. Sources`: grouped by `**Category**:` with bullet links `<https://...>`, then what is an assumption.

## 5. Writing
- Factual, short paragraphs, every number sourced or marked as an **assumption**, units and dates explicit.
- Tables rather than prose for anything with several attributes; `—` for empty cells, `n/a` when not measurable.
- Code, paths, commands, identifiers in backticks.
- Variants: `{: .warn}` / `{: .ok}` on the line after a callout (orange / green bar), `{: .page}` after an H2 to
  start it on a new page.

## 6. Check
- Convert a page to an image (`pdftoppm -r 60 -png -f 1 -l 2 report.pdf /tmp/p`) and look at it: title rule present,
  no table overflowing, lists rendered as lists, footer present.
- Give the user the path(s) and the page count (`pdfinfo`).
