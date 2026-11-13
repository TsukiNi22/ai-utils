---
name: explain-doc
description: Explanation pages in the user's HTML style (html-style) that make a concept understood rather than list an API - an algorithm, a data structure, a protocol, an architecture, a piece of math or physics, how a system or a feature works - each page in two versions switched in place (Simple - analogies, plain words, concrete numbers; Technical - precise terms, formulas, complexity, edge cases, code, sources) with diagrams (inline SVG), step-by-step animations (stepper with play / step controls), formulas (MathML, offline), interactive examples (sliders / inputs computing live results and charts, simulations checking the formulas), worked examples, pitfalls, check-yourself questions and a glossary. Static self-contained page. Use whenever the user asks to explain how something works with a page / visual / interactive explanation, a tutorial for understanding, an explainer, or "une explication simple et une technique".
metadata:
  summary: 'Explanation pages (how something works) in two levels switched in place, **Simple** (analogies, plain words, numbers) and **Technical** (terms, formulas, complexity, edge cases, code, sources), with SVG diagrams, step-by-step animations, MathML formulas, interactive playgrounds / simulations checking the formulas, quizzes and a glossary. Built on `html-style`.'
---

# Explanation pages (Simple | Technical)

`SKILL_DIR` = directory of this file. Built on **`html-style`** (`../html-style`: tokens, components, rules,
`new_page.py`, `check_style.py`): load it first. Not for API references or user guides (`html-doc`), nor for
the reference of a protocol - its frames, codes, commands (`protocol-doc`; an explanation page can link to it), nor
for reports (`report`).

## 1. Ask (one AskUserQuestion call, French), unless already given
1. **Public visé par défaut** (level shown at the first visit): `Simple (Recommandé)` · `Technique`.
2. **Langue** : `Anglais (Recommandé)` · `Français` · `Bilingue EN / FR` (`--lang-switch`, `html-style`). Not asked
   when the folder already holds `html-doc` pages: the page follows the site (bilingual EN / FR like them).
3. **Emplacement** : `docs/explain/<sujet>.html (Recommandé)` (in a repository) · `Dossier courant` · other.
Both levels are always written; the switch (and `?level=simple|technical` in the address) picks the one shown.

## 2. Build
```bash
python3 ../html-style/scripts/new_page.py SKILL_DIR/templates/explain.body.html -o <out>.html \
    --css SKILL_DIR/templates/explain.css --js SKILL_DIR/templates/explain.js \
    --actions SKILL_DIR/templates/actions.html --attr data-page=explain data-nav-title="<tab name>" \
    --set PROJECT="..." TITLE="..." SUBTITLE="..." FOOTER="..."
python3 ../html-style/scripts/sync_nav.py <folder of the page>
```
`explain.body.html` is a complete working example (hash table: stepper, formulas, simulation playground, hash
playground, quiz, glossary): keep its structure and components, replace its content. Default level:
`--attr data-default-level=technical` when the user chose Technique (simple otherwise).
- **Linked with the other pages**: `sync_nav.py` puts in the top tabs (and previous / next cards) every page of the
  folder: the `html-doc` pages (Guide, Technical, Graph) first, then the explanations (`data-nav-title`, order by
  `data-nav-order` then file name), and rewrites the tabs of the `html-doc` pages too. Run it after every new /
  removed page, whichever skill made it (`html-doc` runs it as well). An older explanation page without
  `data-page` is detected by its level switch and gets the attributes.
- **Same features on every page**: `sync_nav.py` adds to every page the shared controls of the top bar (GitHub
  link, EN / FR switch) found on the others; the level switch stays on the explanations. A page that gets the
  EN / FR switch must be bilingual (`data-lang="en"` / `data-lang="fr"` content, as `html-doc`): the script warns
  when it has no French. The UI of the explanations (Play, levels, steps) is translated by `explain.js`.
- **Same top bar on every page**: the brand of the site (its logo, project name, version badge; the name alone when
  there is no version), never a logo / name per explanation: `sync_nav.py` copies the brand of the `html-doc`
  pages on the explanations. A new site without a chosen logo: ask the user (repository / custom / documentation
  logo, see "Logo of the brand" in `html-style`) and apply it with `set_logo.py`. Without `html-doc`:
  `--set VERSION=vX.Y.Z` or nothing (badge hidden).
- Same project as an `html-doc` site: same `PROJECT`, logo and `STORAGE_KEY` (`--set STORAGE_KEY=<doc key>`) so the
  theme / language choices are shared.
- Keep the `html-style` layout: never redefine `.layout` (wide screens center an 880px column; a wider column
  scales the diagrams up). Figures are never drawn above their `viewBox` width (`base.js`).

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
| animation of a process | `<figure class="stepper" data-steps="N"><svg>` + `<ol class="captions"><li>` per step (shown **above** the drawing with the controls, cross-faded); in the SVG `data-show="2"`, `"2-"`, `"1-3"`, `"1,3"` (fade in / out), `data-hl` / `data-warn="3,5"` (highlight), `data-at="0:0,0;3:85,0"` (moves: slide, or fade out / in for a long or wrap-around move; `data-fade` / `data-slide` to force), `data-text="0:19;2:d4"` (value per step: cross-fade + pulse on change) |
| formula | `<div class="formula"><math display="block">…</math><span class="num">(1)</span></div>` + a table of the symbols (MathML is native in every current browser: no KaTeX / MathJax CDN); a long one scrolls in its box, from its start |
| interactive example | `<div class="playground" data-play="name">` with `[data-in="x"]` inputs and `[data-out="y"]` outputs; script `explain.play('name', function (v, el) { return { y: ... }; })` (run at load and on input). Inputs in `<div class="inputs">`: `<label>Text = <output data-out="y"></output><input ...></label>` (the text and its value on one line, the field under them; a box or a radio in front of its text), the result boxes in `<div class="outputs"><div class="out"><span>Label</span><b data-out="y"></b></div>` (as wide as their longest word: a value is not cut) |
| chart | inline `<svg>` drawn by the playground script with the classes `axis`, `grid`, `curve`, `curve-2`, `dot`, `dot-2`, `mark` |
| term | `<dfn data-def="definition">term</dfn>` (a box on hover, focus or tap, placed inside the screen by `explain.js`, above the term when there is no room under it; a long definition scrolls in it, Escape closes it, no scrolling block clips it) |
| one-sentence summary | `<p class="tldr" data-level="...">` |
| side by side | `<div class="compare"><div><h4>…</h4>…</div>…</div>` |
| question | `<details class="quiz"><summary>Question</summary><p>Answer</p></details>` |
| static diagram, callouts, tables, code | the `html-style` components (`figure svg`, `.callout(.warn/.ok)`, `.table-wrap`, `pre[data-title]`) |
- Page scripts go in a `<script>` at the end of the body, inside `document.addEventListener('DOMContentLoaded', ...)`
  (`explain.js` is inlined after the body).
- Animations: SVG + CSS transitions driven by the stepper (no autoplay, no infinite loop); the stepper slows down
  under `prefers-reduced-motion` (no movement). A continuous animation (particles, flow) only with a pause button.
- **Readable steps** (the reader must see what happened between two states):
  - **one change per step**: a transformation touching everything (a full round, a whole matrix) is split into
    sub-steps (row by row, column by column) or shown on one highlighted element first, then on all;
  - every caption starts with the step name in bold, then **what changed**, with one concrete value
    (`<b>ShiftRows.</b> Row 1 slides one cell left: <code>bf</code> leaves column 0 and comes back in column 3.`);
  - the elements that change are marked at that step (`data-hl`, or `data-text` which pulses on change); moving
    elements use `data-at` so the eye follows them: never a cut where things are just different;
  - no element leaves the drawing or crosses it: a wrap-around (rotation, modulo, ring) fades out and back in at
    its new place (automatic with `data-at` beyond 2 element sizes); nothing is clipped by an `overflow` box;
  - one state = the drawing + its caption on screen together (no caption hidden below the fold).
  - the controls + caption bar stays in place when the figure fits the screen; it follows the scroll (sticky)
    only for a figure taller than the window (`explain.js` decides, also after a resize / level change).
- Arrows with `data-from` / `data-to` (ids on the shapes, `:top` / `:bottom`... sides, `data-bend`), as in the
  template: computed from the shapes, they always touch them; `check_style.py` rejects a typed arrow that
  misses its shape.
- Size: `viewBox="0 0 760 H"` with H <= ~360, cells / boxes 36-48px, labels 12-14px (the stepper caps the drawing
  at 400px high); bigger drawings are split into several figures (over `H` = 480 the drawing is not fitted, its labels would be
  unreadable: it is drawn at its size and the head follows the scroll).
- A bilingual drawing: one `<text data-lang="en">` / `<text data-lang="fr">` per language, the longest one inside its box; never a
  `<span>` in a `<text>` (the browser leaves the drawing there: all that follows falls out of it). A select: one
  `<option data-lang="...">` per language (an option holds text only), a term: one `<dfn data-lang="..." data-def="...">` per language.

### Phones and touch
Pages follow the responsive rules of `html-style` (viewport, safe areas, 44px targets, no hover-only content). Here:
every drawing keeps readable labels: steppers, static figures and charts scroll in their box under 860px instead of shrinking
(560px at least, never above their `viewBox`: `explain.js` wraps them in a `.view`), and a step brings what it lights into that box; a
block that really scrolls gets `tabindex` / `role="region"`; the controls and the progress bar wrap on a phone, sliders are full
width with 16px fields, a `<dfn>` shows its definition on **tap**, nothing relies on hover or the keyboard alone, the top bar holds
the level switch, the EN / FR switch and the theme down to 320px (the brand gives way), the sticky head of a stepper never takes more
than 45 % of the screen. On paper: no control, each stepper at its last state with all its captions, the light theme.

## 6. Accuracy
- Every number of the page is computed, never written from memory: the playground formulas are the same functions
  as the ones of the text, and the static tables / captions are checked with them (`node -e` with the page
  functions) before delivering.
- A formula comes with its assumptions and a source; when it can be simulated (probability, cost, physics), the
  playground shows **predicted vs measured** side by side, as in the template.
- Mark what is approximate (`≈`, "on average", "under uniform hashing").

## 7. Check
- `python3 ../html-style/scripts/check_style.py <page>`: 0 error.
- `python3 ../html-style/scripts/check_render.py <page> [--shots DIR]`: 0 error. It measures both levels, both themes, each language
  and width, but only the **first step** of a stepper, with the definitions closed: the other steps and the longest definitions are looked at.
- Screenshots at both levels (`?level=simple`, `?level=technical`), light / dark, 1280px and 390px
  (`firefox --headless --screenshot out.png --window-size 1280,1600 "file://<page>?level=technical"`; a running
  Firefox needs `--profile $(mktemp -d)`): every step of each stepper readable, playground outputs
  filled (no empty `[data-out]`), no console error.
- Read each level alone: the Simple version must be understandable without the Technical one.
