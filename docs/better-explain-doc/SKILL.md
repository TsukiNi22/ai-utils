---
name: better-explain-doc
description: Explanation pages (Simple | Technical in one page, analogies vs exact terms, step-by-step animated steppers, formulas, playgrounds, quizzes, glossary) written as pages of a better-html-doc site - Markdown with :::simple / :::technical blocks (tldr summary highlighted), the level switch in the top bar next to the search, the explain-doc components (stepper, formula, playground, dfn, compare, quiz) through :::html, found by the site search, linked by the sidebar and previous / next, responsive. Use whenever the user wants an explanation, a tutorial for understanding or "une explication simple et une technique" inside a documentation site in the claude-cluster style (better-html-doc); a standalone explanation page in the older style: explain-doc.
---

# Explanation pages in a better-html-doc site

`SKILL_DIR` = directory of this file. The **content rules are those of `explain-doc`** (`../explain-doc/SKILL.md`: the two
levels, the 8 sections, readable steps, accuracy - every number computed, never from memory): load it first. This skill
only changes **where** the page lives: a page of a `better-html-doc` site (`../better-html-doc`), written in Markdown, with
the components of `explain-doc` (`explain.css` / `explain.js`, loaded only on these pages by the built-in `explain` kind).

## 1. Ask (AskUserQuestion, French) unless known
- **Niveau par défaut** : `Simple (Recommandé)` · `Technique`.
- The site, its languages and its sections come from its `site.toml` (no site yet: create it with `better-html-doc` first,
  a `Concepts` section for the explanations).

## 2. Write the page
`src/pages/concepts/<slug>.<lang>.md` - start from `SKILL_DIR/templates/pages/concepts/how-caching-works.*.md`:
```markdown
+++
title = "How the cache works"
summary = "Why a second run is fast, explained simply or technically."
section = "concepts"
kind = "explain"
level = "simple"          # the level shown at the first visit (the reader's choice is kept after)
+++

:::simple tldr
**In one sentence:** ...
:::

:::technical tldr
**Definition:** ...
:::

## The idea {#idea}
:::simple
analogy, plain words
:::
:::technical
model, vocabulary, comparison
:::
```
- `:::simple` / `:::technical` = shown at that level only; text outside them is shown at both levels (diagrams, playgrounds,
  tables that fit both). A `###` inside a level block belongs to that level.
- The `explain-doc` components go in `:::html` blocks, unchanged: stepper (`figure.stepper` + `ol.captions` + `svg` with
  `data-show` / `data-hl` / `data-at`), formula (`div.formula` + MathML), playground (`div.playground` + a page script calling
  `explain.play`), `dfn data-def`, `div.compare`, `details.quiz`. Arrows by `data-from` / `data-to` (computed).
- **Bilingual site**: each language file has its own copy of the figures - give their ids a language prefix (`en-q`, `fr-q`)
  so they stay unique on the page.
- Sections in the order of `explain-doc` §3 (the idea, how it works, how fast, try it, worked example, pitfalls, check
  yourself, glossary and sources), 6-9 of them.

## 3. Build and check
The commands of `better-html-doc` (build_site.py, check_site.py, test_search.js), then look at the page: both levels, the
stepper played to its end, a playground moved, at 1280 and 390 px, light and dark. The level switch sits in the top bar
(`?level=technical` in a link opens that level; the choice is shared with the other explanation pages of the site).
