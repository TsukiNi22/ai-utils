---
name: theming-web
description: Light / dark (and other) modes for a website or web app - on top of the common rules of theming (modes asked through the interactive questions, semantic tokens, WCAG contrast in each mode) - CSS custom properties on :root redefined per mode, prefers-color-scheme as the default with a three-state switch (system / light / dark) stored on data-theme, no flash of the wrong theme (a tiny inline script in the head before the first paint), the choice in localStorage with try / catch, color-scheme and meta theme-color per mode, light-dark(), form controls / scrollbars / iframes / embeds / emails adapted, images with picture + media, SVG with currentColor, prefers-contrast, forced-colors and prefers-reduced-motion respected, Tailwind / CSS-in-JS / component libraries configured, an accessible sun / moon toggle, screenshots and contrast checks of both modes. Use whenever a site / web app needs a dark mode, a theme switch, "mode sombre", high contrast, or palettes; the user's own pages use the tokens of html-style.
---

# theming-web - visual modes for a website / web app

Load **`theming` first** (its questions, tokens and contrast rules apply as they are); this skill adds the web.
The user's own pages: the colour tokens and the sun / moon switch of `html-style` are the reference.

## 1. CSS
- Tokens as custom properties: `:root { --bg: ...; --text: ...; }` for light, then dark under
  `@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { ... } }` **and** `:root[data-theme="dark"]
  { ... }` (the explicit choice wins over the system in both directions); other modes as `[data-theme="sepia"]`.
- `color-scheme: light dark` on `:root` (and `color-scheme: dark` in the dark block) so form controls, scrollbars and
  the default canvas follow; `light-dark(<light>, <dark>)` where supported for one-off values; `accent-color` for
  checkboxes / radios.
- `<meta name="color-scheme" content="light dark">` and `<meta name="theme-color" content="..." media="(prefers-color-scheme: dark)">`
  (one per mode) for the browser bar.
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
- Print: a light print style whatever the screen mode.

## 4. Check
- Screenshots of the main pages in each mode (headless browser, `?theme=` or emulated `prefers-color-scheme`), the
  first paint without flash (slow 3G throttling), the contrast of every token pair (a script; the user's pages:
  `html-style/scripts/check_style.py`), forced colours (Windows high contrast emulation), keyboard on the switch.
- Recap as in `theming` + the files touched and how to add a mode.
