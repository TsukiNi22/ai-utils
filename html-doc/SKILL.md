---
name: html-doc
description: Uniform style for every HTML documentation page of the user's projects (architecture docs, design docs, project/user docs, docs/ folders, GitHub Pages) - one self-contained file based on the R-Type architecture doc - sticky summary, numbered card sections, light/dark theme with a sun/moon icon button, tables, callouts, inline SVG diagrams. Use whenever creating or restyling an HTML doc, unless the user explicitly asks for another style.
---

# Uniform HTML documentation style

Always start from `templates/doc.html` (style extracted from `r-type/docs/architecture.html`,
the most recent doc of the user). Keep its CSS **as is**, unless the user explicitly asks for
another style. Only the content changes.

## File
- One self-contained `.html` (inline `<style>` and `<script>`), no CDN, no web font
  (system fonts), no framework, works offline and from `file://`.
- Default place: `docs/<name>.html` (`docs/index.html` for the main doc of a project / GitHub Pages).
- `lang` = the language of the request (French by default, `<html lang="fr">`); the code,
  identifiers and file names stay as they are in English.
- Placeholders: `{{TITLE}}` (also in `<title>`), `{{SUBTITLE}}` (1-2 sentences under the title: what
  the document describes, planned vs implemented), `{{STORAGE_KEY}}` (project slug, ex: `rtype`).

## Layout
- `<nav aria-label="Sommaire">` on the left (sticky, `Sommaire` label), one link per section,
  same numbering as the sections. The visible section is highlighted by the script.
- `<header>`: `h1` + subtitle paragraph.
- One `<section id="...">` card per topic, preceded by `<!-- N -->`, titled `<h2>N. Title</h2>`,
  sub-parts `<h3>N.M Title</h3>`. Usual order for a project: overview, split into libraries/modules,
  each module, network/protocol, build. Keep the number of sections reasonable (5-9).
- Theme button at the top right: round icon button, **moon** in light mode, **sun** in dark mode
  (`aria-label="Changer de thème"`), never a text button. Choice saved in `localStorage`
  (`<STORAGE_KEY>-theme`), default = system preference.

## Content components
| Need | Component |
|---|---|
| list of elements with attributes | `<div class="table-wrap"><table>` with `<thead>` |
| remark / info | `<div class="callout">` |
| limit, risk, planned-not-done | `<div class="callout warn">` |
| validated / good practice | `<div class="callout ok">` |
| code, command, path, type | `<code>` inline, `<pre><code>` block |
| architecture, flow, tree, loop | inline `<svg>` inside `<figure>` + `<figcaption>Figure N : ...` |

SVG diagrams: `viewBox="0 0 760 H"`, `role="img"` + `aria-label` describing the diagram, only the
classes of the template (`box`, `box-alt` for the highlighted node, `group` for dashed groups, `lbl`,
`small`, `title`, `edge`, `edge-accent`, `dash`, `arrow`, `arrow-accent`) so both themes work, markers
defined once in `<defs>`. Draw the real mechanism (components and messages), not decoration.

## Writing
- Short paragraphs, factual, present tense; say explicitly what is **planned** vs **implemented**.
- Every identifier in `<code>`; tables rather than long lists.
- No emoji, no gradient, no extra colors: only the CSS variables of the template.
- Before delivering: valid HTML, every nav link points to an existing `id`, readable in both themes
  and at phone width (the template has the 860px breakpoint).
