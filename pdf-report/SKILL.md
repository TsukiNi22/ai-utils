---
name: pdf-report
description: Deliver a result as a PDF in the user's report style (measured on the Convertly docs - WeasyPrint, A4, Noto Sans 9 pt, navy #1a3d7c titles and table heads, light blue callout with navy bar, zebra tables, mono code on grey, footer "title - page / pages") written in Markdown then converted with scripts/md2pdf.py. Use whenever the user asks for a PDF, a report/benchmark/audit/study "en pdf", or another skill needs a PDF output.
---

# PDF report in the user's style

Write the report in **Markdown** (kept next to the PDF, like `docs/<name>.md` + `docs/<name>.pdf`), then:
```bash
python3 SKILL_DIR/scripts/md2pdf.py <report.md> [-o report.pdf] [--footer "Projet — sujet"] [--css extra.css] [--html out.html]
```
`SKILL_DIR` = directory of this file. Requires `weasyprint` and `python3-markdown` (installed here), Noto fonts.
The style is `templates/report.css`: never restyle unless the user asks. Start from `templates/report.md`.

## Structure (from the reference documents)
1. `# Projet : sujet` (one H1, it is also the footer text unless `--footer`).
2. Meta line: `**Date** : 2 octobre 2026 · **Sources** : ...` (code paths in backticks).
3. Callout `> **Comment lire ce document**` + numbered list of the parts + the hypotheses / limits / what is
   **not** included.
4. `## Sommaire`: plain numbered list of the H2.
5. `## 1. Résumé`: table `| Point | Conclusion |`, first column in **bold**, key numbers in **bold**.
6. Numbered parts `## N. Titre`, sub-parts `### N.M Titre`, tables for every comparison, numbered findings
   `1. **Constat.** explication`, short `> **À retenir** : ...` callouts.
7. `## N. Sources`: grouped by `**Catégorie** :` with bullet links `<https://...>`, then what is an assumption.

## Writing
- Language of the request (French by default), factual, short paragraphs, every number sourced or marked as
  **hypothèse**, units and dates explicit (`HT`, `02/10/2026`).
- Tables rather than prose for anything with several attributes; `—` for empty cells, `n/a` when not measurable.
- Code, paths, commands, identifiers in backticks.
- Variants: `{: .warn}` / `{: .ok}` on the line after a callout (orange / green bar), `{: .page}` after an H2 to
  start it on a new page.

## Check
- Open/convert a page to an image (`pdftoppm -r 60 -png -f 1 -l 2 report.pdf /tmp/p`) and look at it: no table
  overflowing, lists rendered as lists, footer present.
- Give the user both paths (`.md` and `.pdf`) and the page count (`pdfinfo`).
