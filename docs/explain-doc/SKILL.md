---
name: explain-doc
description: Explanation pages in the user's HTML style (html-style) that make a concept understood rather than list an API - an algorithm, a data structure, a protocol, an architecture, a piece of math or physics, how a system or a feature works - each page in two versions switched in place (Simple - analogies, plain words, concrete numbers; Technical - precise terms, formulas, complexity, edge cases, code, sources) with diagrams (inline SVG), step-by-step animations (stepper with play / step controls), formulas (MathML, offline), interactive examples (sliders / inputs computing live results and charts, simulations checking the formulas), worked examples, pitfalls, check-yourself questions and a glossary. Static self-contained page. Use whenever the user asks to explain how something works with a page / visual / interactive explanation, a tutorial for understanding, an explainer, or "une explication simple et une technique".
---

# Explanation pages (Simple | Technical)

`SKILL_DIR` = directory of this file. Built on **`html-style`** (`../html-style`: tokens, components, rules,
`new_page.py`, `check_style.py`): load it first. Not for API references or user guides (`html-doc`) nor for
reports (`report`).

## 1. Ask (one AskUserQuestion call, French), unless already given
1. **Public visé par défaut** (level shown at the first visit): `Simple (Recommandé)` · `Technique`.
2. **Langue** : `Anglais (Recommandé)` · `Français` · `Bilingue EN / FR` (`--lang-switch`, `html-style`).
3. **Emplacement** : `docs/explain/<sujet>.html (Recommandé)` (in a repository) · `Dossier courant` · other.
Both levels are always written; the switch (and `?level=simple|technical` in the address) picks the one shown.

## 2. Build
```bash
python3 ../html-style/scripts/new_page.py SKILL_DIR/templates/explain.body.html -o <out>.html \
    --css SKILL_DIR/templates/explain.css --js SKILL_DIR/templates/explain.js \
    --actions SKILL_DIR/templates/actions.html --set PROJECT="..." TITLE="..." SUBTITLE="..." FOOTER="..."
```
`explain.body.html` is a complete working example (hash table: stepper, formulas, simulation playground, hash
playground, quiz, glossary): keep its structure and components, replace its content. Default level:
`data-default-level="technical"` on `<html>` when the user chose Technique (simple otherwise).

## 3. Structure (adapt titles, keep the order; 6-9 sections, each a `<section id>` card)
| # | Section | Simple | Technical |
|---|---|---|---|
| — | `.tldr` one sentence | what it does for you | definition + key property / complexity |
| 1 | The idea | everyday analogy, why it exists | model, vocabulary, comparison with the alternatives (`.compare`) |
| 2 | How it works | stepper animation + plain captions | same stepper, precise captions (states, invariants) |
| 3 | How fast / how much / why | table of concrete numbers, a sentence rule | formulas (MathML) + variables table + assumptions |
| 4 | Try it | playground: move it, see the result | same playground + a simulation measuring the formula |
| 5 | Worked example | a real case followed step by step | same case with the exact values, code (`pre`) |
| 6 | Mistakes / pitfalls | misconceptions | edge cases, failure modes, costs, security |
| 7 | Check yourself | 2-4 `details.quiz` | 2-4 `details.quiz` |
| 8 | Glossary & sources | terms in plain words | + references (books, papers, specs, source code) |

## 4. Two levels in one page
- `data-level="simple"` / `data-level="technical"` on any element = shown only at that level; no attribute =
  both (diagrams, playgrounds, tables shared when they fit both). The sidebar titles can differ too.
- **Simple**: no jargon (or a `<dfn data-def="...">` at its first use), one idea per sentence, analogies chosen from
  daily life and **kept consistent** through the page, concrete numbers instead of symbols, a formula becomes a
  sentence ("twice as full = four times longer") or a small table. Never false: simplify, don't change the facts.
- **Technical**: exact terms, notation defined once, assumptions stated, complexity / units / orders of magnitude,
  edge cases, code in the user's style (`cpp-style` / `coding-style`), sources.
- Both levels say the same thing: same examples, same numbers, same conclusions.

## 5. Components (`explain.css` / `explain.js`, on top of the `html-style` ones)
| Need | Markup |
|---|---|
| animation of a process | `<figure class="stepper" data-steps="N"><svg>` + `<ol class="captions"><li>` per step; in the SVG `data-show="2"`, `"2-"`, `"1-3"`, `"1,3"`; `data-hl` / `data-warn="3,5"` highlight (accent / warn); controls, progress, ←/→ keys added by the script |
| formula | `<div class="formula"><math display="block">…</math><span class="num">(1)</span></div>` + `table.where` of the symbols (MathML is native in every current browser: no KaTeX / MathJax CDN) |
| interactive example | `<div class="playground" data-play="name">` with `[data-in="x"]` inputs and `[data-out="y"]` outputs; script `explain.play('name', function (v, el) { return { y: ... }; })` (run at load and on input) |
| chart | inline `<svg>` drawn by the playground script with the classes `axis`, `grid`, `curve`, `curve-2`, `dot`, `dot-2`, `mark` |
| term | `<dfn data-def="definition">term</dfn>` (tooltip on hover / focus) |
| one-sentence summary | `<p class="tldr" data-level="...">` |
| side by side | `<div class="compare"><div><h4>…</h4>…</div>…</div>` |
| question | `<details class="quiz"><summary>Question</summary><p>Answer</p></details>` |
| static diagram, callouts, tables, code | the `html-style` components (`figure svg`, `.callout(.warn/.ok)`, `.table-wrap`, `pre[data-title]`) |
- Page scripts go in a `<script>` at the end of the body, inside `document.addEventListener('DOMContentLoaded', ...)`
  (`explain.js` is inlined after the body).
- Animations: SVG + CSS transitions driven by the stepper (no autoplay, no infinite loop); the stepper slows down
  under `prefers-reduced-motion`. A continuous animation (particles, flow) only with a pause button.

## 6. Accuracy
- Every number of the page is computed, never written from memory: the playground formulas are the same functions
  as the ones of the text, and the static tables / captions are checked with them (`node -e` with the page
  functions) before delivering.
- A formula comes with its assumptions and a source; when it can be simulated (probability, cost, physics), the
  playground shows **predicted vs measured** side by side, as in the template.
- Mark what is approximate (`≈`, "on average", "under uniform hashing").

## 7. Check
- `python3 ../html-style/scripts/check_style.py <page>`: 0 error.
- Screenshots at both levels (`?level=simple`, `?level=technical`), light / dark, 1280px and 390px
  (`firefox --headless --screenshot out.png --window-size 1280,1600 "file://<page>?level=technical"`; a running
  Firefox needs `--profile $(mktemp -d)`): no overflow, every step of each stepper readable, playground outputs
  filled (no empty `[data-out]`), no console error.
- Read each level alone: the Simple version must be understandable without the Technical one.
