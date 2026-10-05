---
name: html-style
description: The user's visual style for any HTML page (taken from html-doc - R-Type doc colors + context-forge layout) - color tokens for the light and dark themes, system font stacks and type scale, spacing / radius / layout / breakpoints, components (top bar, cards, callouts, tables, code blocks with copy, content tabs, sidebar, pager, SVG diagram classes, sun/moon theme switch, EN/FR switch), rules (self-contained, no CDN / web font / gradient / emoji, WCAG contrast in both themes) - with scripts to build a self-contained page (new_page.py) and check a page or a palette (check_style.py). Use whenever creating or restyling an HTML page, a small web tool, a dashboard or a page component for the user, or when defining / changing a palette, color, font or theme, unless the user asks for another style.
---

# HTML style

`SKILL_DIR` = directory of this file. The source of the style is `templates/style.css` (CSS) + `templates/base.js`
(behaviour), extracted from `html-doc/templates/technical.html`. `html-doc` (documentation pages) and `explain-doc`
(explanation pages) are built on it; any other HTML page of the user uses it too.

## 1. New page
```bash
python3 SKILL_DIR/scripts/new_page.py <body.html> -o <out.html> [--css extra.css] [--js extra.js] \
    [--actions actions.html] [--lang-switch] --set PROJECT="Name" TITLE="Page title" FOOTER="..."
```
`<body.html>` = what goes inside `.layout`: an optional `<aside class="sidebar" id="sidebar">` (contents list) and
`<main>` (`<header>` with `.page-eyebrow`, `h1`, `p.lead`, then one `<section id>` card per topic). The script
inlines `style.css` + `base.js` (+ the extra files, after them) into `templates/page.html`: the result is **one
self-contained file** that works offline and from `file://`. Without a sidebar, add
`.layout { grid-template-columns: minmax(0, 1fr); max-width: 880px; }` in the extra CSS.

## 2. Tokens (never a raw color in a component: always `var(--x)`)
| Token | Light | Dark | Use |
|---|---|---|---|
| `--bg` | `#f7f7f5` | `#15171b` | page background |
| `--surface` | `#ffffff` | `#1d2025` | cards, top bar, buttons |
| `--text` | `#1d1f23` | `#e6e7ea` | text |
| `--muted` | `#5d636e` | `#9aa0ab` | secondary text, captions, table heads |
| `--border` | `#dfe1e5` | `#30343b` | borders, separators |
| `--accent` / `--accent-soft` | `#2f5bd3` / `#e6ecfb` | `#7d9cf0` / `#232c45` | links, current tab, highlights / their background |
| `--warn` / `--warn-soft` | `#a8540a` / `#fbefe3` | `#f0a868` / `#3a2a1b` | limits, risks |
| `--ok` / `--ok-soft` | `#1f7a4a` / `#e3f4ea` | `#6fcf97` / `#1b3326` | validated, good practice |
| `--code-bg` | `#f1f2f4` | `#24272d` | inline code, code blocks, hover |
| `--node` / `--node-alt` / `--line` | `#ffffff` / `#eef1f8` / `#8a909b` | `#23262c` / `#283043` / `#747a85` | SVG boxes / highlighted box / edges |

Dark theme: the same tokens in `@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {...} }`
**and** `:root[data-theme="dark"] {...}` (the switch forces one or the other). A new token is added in the 3 blocks.

## 3. Type, space, layout
- Fonts (no web font): text `system-ui, -apple-system, "Segoe UI", Roboto, sans-serif` 16px / 1.6; code
  `ui-monospace, SFMono-Regular, Menlo, Consolas, monospace` (0.9em inline, 13.5px blocks).
- Scale: `h1` 34px (27px mobile), `h2` 22px, `h3` 17px, lead 17px muted, body 16px, tables 14.5px, captions /
  footer 13px, eyebrow / table heads / sidebar title 12-13px uppercase with letter spacing (`.04em` - `.1em`).
- Text width `75ch` max (`70ch` for the lead). Radius: cards 10px, code blocks 8px, tabs / buttons 6px, inline
  code 4px, pills 999px. Card padding 28px 32px (20px 16px mobile), 24px between cards.
- Layout `.layout`: sidebar 250px + content, max 1180px; from 1340px the content column (880px) is centered and
  the sidebar sits on its left. Breakpoints 860px (sidebar off-canvas, menu button, tabs hidden) and 480px.
- Top bar 56px sticky: `.brand` (inline SVG logo in `currentColor` = accent, name, `.ver` badge), `nav.tabs`,
  `.spacer`, `.top-actions` (icon buttons | divider | EN/FR pill, theme switch).

## 4. Components (classes of `style.css`)
| Need | Markup |
|---|---|
| card per topic | `<section id="x"><h2>N. Title</h2>...</section>` (anchor `#` added on hover) |
| page header | `<header><div class="page-eyebrow">Kind</div><h1>...</h1><p class="lead">...</p></header>` |
| remark / limit / validated | `<div class="callout">`, `.callout.warn`, `.callout.ok` |
| table | `<div class="table-wrap"><table><thead>...` (muted uppercase heads, row lines) |
| code | `<code>` inline; `<pre data-title="File"><code>` block (title bar, **Copy** button; `data-nocopy` to remove) |
| alternatives | `<div class="ctabs" data-tabs="g"><button data-tab="a">` + `<div class="ctab-panel" data-tabs="g" data-tab="a">` |
| contents list | `<aside class="sidebar" id="sidebar"><p>Contents</p><a href="#x">1. ...</a>` (visible section highlighted) |
| previous / next | `<nav class="pager"><a href="..."><span>Previous</span>Title</a><a class="next">` |
| diagram | `<figure><svg viewBox="0 0 760 H" role="img" aria-label="...">` + `<figcaption>Figure N: ...` |
| icon button | `<a class="icon-btn" aria-label="...">` + 16-18px inline SVG in `currentColor` |
- SVG classes: `box`, `box-alt` (accent border), `group` (dashed frame), `lbl`, `small`, `title`, `edge`,
  `edge-accent`, `dash`, `arrow`, `arrow-accent` (markers once in `<defs>`). They follow the theme by themselves.
- Bilingual page: `--lang-switch`, content as `<span data-lang="en">..</span><span data-lang="fr">..</span>`, UI
  strings with `data-i18n` translated by `window.DOC_FR` (`html-doc` explains it in full). English only by default.
- A new component: built from the tokens, the same radius / spacing, a hover with `--accent` border or
  `--code-bg` background, a visible `:focus-visible` (2px accent outline), works at 860px / 480px and in both themes.

## 5. Rules
- Self-contained: inline CSS / JS / SVG, no CDN, no framework, no web font, no external image (inline SVG or
  `data:` URI); no gradient, no emoji, no shadow except the floating ones (open menu, popovers).
- Contrast WCAG AA in **both** themes: text >= 4.5:1, lines / meaningful graphics >= 3:1.
- An element with its own background (`code`, `kbd`, `mark`, badge) inside an element that sets a light text
  color (accent button, dark table head, colored banner) gets an explicit override
  (`th code { background: rgba(255,255,255,.18); color: #fff; }`): it never inherits a color unreadable on its
  own background (`check_style.py` finds them).
- Motion: transitions 0.15-0.3s; animations stop under `@media (prefers-reduced-motion: reduce)`.
- `localStorage` (theme, language, view state) always in `try / catch` under `<storage-key>-<name>`.

## 6. Define or change a style (palette, color, font)
1. Ask (AskUserQuestion, French) when not given: **"Quelle palette ?"** - `Style par défaut (Recommandé)` ·
   `Couleur du projet / de la marque` (hex or logo given) · `Palette personnalisée`. Font: system stacks unless the
   user asks (then a local / system font, never a downloaded one).
2. Change only the tokens, never the components: for a project color, set `--accent` (light: dark enough for
   4.5:1 on `--surface`; dark: a lighter tint of the same hue), `--accent-soft` (accent at ~10% on `--surface`,
   light; ~20% on `--bg`, dark), keep the neutrals. A full palette defines every token of section 2 in both themes.
3. Write it as a `:root` override in the page extra CSS (or in `templates/style.css` when the default itself
   changes, then the same values in the `<style>` of the `html-doc` templates).
4. `python3 SKILL_DIR/scripts/check_style.py <page.html|css> [--compare SKILL_DIR/templates/style.css]`: 0 error
   (tokens of both themes, contrast, inherited colors, self-contained); `--compare` lists what differs from the
   default.

## 7. Check before delivering
- `check_style.py` on the page: 0 error.
- Open the page (or a headless screenshot, `firefox --headless --screenshot out.png --window-size 1280,900 file://...`)
  in light and dark, at 1280px and at 390px: no horizontal scroll, every text readable, switches working.
- No `{{...}}` left (`new_page.py` reports them).
