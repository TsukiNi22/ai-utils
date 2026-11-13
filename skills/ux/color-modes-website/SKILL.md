---
name: color-modes-website
description: Light / dark (and other) modes for a website or web app, on top of color-modes-project (modes asked through the interactive questions, semantic tokens, WCAG contrast): CSS custom properties per mode, prefers-color-scheme with a system / light / dark switch on data-theme, no flash of the wrong theme (an inline head script), the choice in localStorage, color-scheme and theme-color, light-dark(), images / SVG / charts / embeds / emails adapted, prefers-contrast, forced-colors and reduced motion respected, Tailwind and component libraries configured, an accessible sun / moon toggle, screenshots and contrast checks of each mode. Use whenever a site / web app needs a dark mode, a theme switch, "mode sombre", high contrast or palettes; the user's own pages use the tokens of html-style.
metadata:
  summary: 'The web part of `color-modes-project`: CSS variables per mode, `prefers-color-scheme` + a system / light / dark switch on `data-theme`, no flash (inline head script), `color-scheme` / `theme-color`, `light-dark()`, `prefers-contrast` / `forced-colors` / reduced motion, images and charts per mode, Tailwind / component libraries, an accessible sun / moon toggle.'
---

# theming-web - visual modes for a website / web app

Load **`color-modes-project` first** (its questions, tokens and contrast rules apply as they are); this skill adds the web.
The user's own pages: the colour tokens and the sun / moon switch of `html-style` are the reference.

## 1. CSS
- Tokens as custom properties: `:root { --bg: ...; --text: ...; }` for light, then dark under
  `@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { ... } }` **and** `:root[data-theme="dark"]
  { ... }` (the explicit choice wins over the system in both directions); other modes as `[data-theme="sepia"]`.
- **Every token in every block** (light, both dark blocks, each other mode), the same list: a token missing from one is an invalid
  `var()`, the property is dropped without a word (black on black, a transparent box). A new token: added in all of them at once.
- **`color-scheme` per theme**: `color-scheme: light` in the light block and each light mode, `color-scheme: dark` in both dark blocks,
  so fields, scrollbars, the default canvas and `light-dark(<light>, <dark>)` follow the **switch**. Never one `color-scheme: light dark`
  on `:root` of a page with a switch: they would follow the system (dark fields on a page forced light, white ones on a page forced
  dark). `accent-color` from a token for checkboxes / radios.
- **What the browser draws gets its colours from the tokens, both at once**: `input`, `select`, `textarea`, `button` (`color` and
  `background`, `::placeholder`, a visible border), `mark`, `kbd`, `code`, `a`, `hr`, `blockquote`; a `color` set without a `background` (or the
  reverse) on a thing with its own default is half a theme. An SVG text or shape with no class has a black fill: a token or
  `currentColor`, always. A chip (`code`, a badge) in a coloured banner gets its own text colour.
- `<meta name="color-scheme" content="light dark">` and `<meta name="theme-color" content="..." media="(prefers-color-scheme: dark)">`
  (one per mode) for the browser bar; the meta follows the system only: updated by the switch when the choice is forced.
- `@media (prefers-contrast: more)` → the high-contrast tokens; `@media (forced-colors: active)` → system colours
  (`CanvasText`, `ButtonText`, `Highlight`), borders kept visible, no information carried by background colour only.
- `@media (prefers-reduced-motion: reduce)` → no theme transition / animation.

## 2. The switch and no flash
- **Three states**: system (default) / light / dark (+ the other modes chosen) - a radio group or a menu button,
  labelled ("Theme: system"), `aria-pressed` / `aria-checked`, keyboard reachable, a sun / moon icon with a text
  alternative; in the header or the settings.
- The choice on `<html data-theme="...">` and in `localStorage` (wrapped in `try / catch`: private mode, blocked
  storage) - a preference the user set: no consent needed; logged-in users: also on the server (same theme on every
  device, server-rendered).
- **No flash of the wrong theme**: a tiny inline `<script>` in `<head>`, before the style sheets paint, reads the saved
  choice and sets `data-theme` (with a CSP: its hash / nonce); server rendering: the cookie / account value written
  into the HTML. System changes followed live: `matchMedia("(prefers-color-scheme: dark)").addEventListener("change", ...)`
  when the choice is "system".

## 3. Content
- Images: `<picture><source srcset="logo-dark.svg" media="(prefers-color-scheme: dark)">` (+ a class switch when the
  user forced the other mode), transparent backgrounds; inline SVG / icons with `fill: currentColor`.
- Charts (Chart.js, ECharts, D3, Recharts): read the tokens from `getComputedStyle` and redraw on a theme change;
  code highlighting: a theme per mode (Shiki / Prism / highlight.js dual themes); maps: a dark style; embeds / iframes
  (YouTube, maps, widgets): their own theme parameter when they have one.
- Frameworks: Tailwind `darkMode: "selector"` (`dark:` variants driven by `data-theme` / a class), MUI / Chakra /
  Vuetify / shadcn theme providers bound to the same state, CSS-in-JS themes from the same tokens (one source).
- Emails: `@media (prefers-color-scheme: dark)` in the HTML emails + `color-scheme` meta, logos that work on both.
- Print: a light print style whatever the screen mode: `color-scheme: light` and the light tokens set again under `@media print`, on the
  selectors of the dark blocks too (`:root[data-theme="dark"]` wins over a plain `:root`).

## 4. Check
- Screenshots of the main pages in each mode (headless browser, `?theme=` or emulated `prefers-color-scheme`), the
  first paint without flash (slow 3G throttling), the contrast of every token pair (a script; the user's pages:
  `html-style/scripts/check_style.py`: tokens of each theme, a `var()` defined nowhere, an invalid colour), forced colours (Windows high
  contrast emulation), keyboard on the switch, a native field and an open `select` in each mode with the switch **against** the system.
- What is really drawn: `html-style/scripts/check_render.py <page> [--themes light,dark] [--shots DIR]` on any page, 0 error: each text against what
  is behind it in each mode (black on black, a chip in a banner, an SVG text on its box), at 1280, 768 and 375px. A screenshot of `--shots` shows
  the dark theme only on a page that reads `?theme=` (the pages of `html-style` do); the others: the screenshots of the first line.
- Recap as in `color-modes-project` + the files touched and how to add a mode.
