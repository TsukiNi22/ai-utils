---
name: better-html-style
description: The documentation-site look of the user's HTML (from the claude-cluster docs site) on top of html-style - a search box in the top bar and a big one on the home (typo tolerant, ranked on titles / descriptions / text, / or Ctrl+K, arrows + Enter, results with a kind pill, works from a site index, a server endpoint or the sections of the page itself), a home hero with version badge, section tiles with page counts, sidebar grouped by section with the current page, page meta (updated, edit link), changelog list, collapsible details, kbd keys, embed mode (?embed=1) and ?q= / ?lang= / ?theme= in the address, phone layout (search under the bar, 44px targets, 16px fields), print. Use for any page or small tool that should look like the claude-cluster docs ("le style de la doc de claude-cluster", "better", "avec recherche"); a whole documentation site: better-html-doc.
---

# Better HTML style (the claude-cluster docs look)

`SKILL_DIR` = directory of this file. A **layer on `html-style`** (`../html-style`: tokens, components, rules, responsive,
`check_style.py` - load it first, everything it says stays true). This skill adds `templates/site.css` + `templates/site.js`
(the search, the home, the site navigation) and `templates/search.html` (the search box of the top bar). The engine of a whole
site is `better-html-doc`; explanation and protocol pages in that site: `better-explain-doc`, `better-protocol-doc`.

## 1. One page
```bash
python3 SKILL_DIR/scripts/new_page.py <body.html> -o <out.html> [--lang-switch] [--css extra.css] [--js extra.js] \
    [--actions more.html] [--no-search] --set PROJECT="Name" TITLE="Page title" FOOTER="..." [VERSION=1.2.0]
```
The same body as `html-style` (`<aside class="sidebar" id="sidebar">` + `<main>` with `<section id>` cards). The search box of
the top bar searches **the sections of the page** (h2 and h3 with an id) when there is no site index: a long page becomes
searchable for free. Self-contained, offline, `file://`.

## 2. Components (on top of the `html-style` ones)
| Need | Markup |
|---|---|
| home hero | `<header class="hero"><div class="page-eyebrow">Name <span class="badge">v1.2</span></div><h1>…</h1><p class="lead">…</p><div class="sbox">…</div></header>` in a `.layout.wide` |
| big search | `<div class="sbox"><svg>…</svg><input type="search" placeholder="Search…  ( / )" data-ph-fr="Rechercher…"><ul class="results" hidden></ul></div>` |
| section tiles | `<div class="grid"><a class="tile" href><svg>icon</svg><span class="count">3 pages</span><h3>…</h3><p>…</p></a></div>` |
| two columns (stack under 860px) | `<div class="two">…</div>` |
| changelog | `<ul class="cl"><li><b>1.2.0</b><span>what changed</span></li></ul>` |
| row of links | `<div class="links"><a href>FAQ</a>…</div>` |
| sidebar groups | `<div class="grp">Section</div><a href class="cur" aria-current="page">Page</a>`, `<hr>` between contents and groups |
| page meta | `<div class="page-meta"><span>Updated 2026-10-24</span><a class="edit" href>Edit this page</a></div>` in the header |
| collapsed block | `<details class="more"><summary>Every option</summary><div>…</div></details>` |
| keys | `<kbd>Ctrl</kbd>+<kbd>K</kbd>` |
| pill / kind | `<span class="pill">beta</span>`, `<span class="kind docs">docs</span>` (`docs`, `page`, `settings`, `config`, `tools`, `api`, `message`, `keys`, `explain`) |

## 3. Search (`site.js`)
- **Index**: `window.DOCS_INDEX = {entries: [{k, l, t, d, u, x}]}` (`search-index.js` of a site: kind, language, title,
  description, url, text), else a server endpoint (`<meta name="docs-search" content="/docs/search">`, answers
  `{results: [...]}` for `?q=&lang=`), else the sections of the page.
- **Ranking**: every word of the query must match; prefix > start of a word > inside > one / two typos (a missing, doubled,
  wrong or swapped letter) > letters in order (`sesend` finds `session_send`); title > description > text. Accents kept.
- Keys: <kbd>/</kbd> or <kbd>Ctrl</kbd>+<kbd>K</kbd> focus the search (the big one on the home), <kbd>↑</kbd> <kbd>↓</kbd>
  move, <kbd>Enter</kbd> opens, <kbd>Esc</kbd> closes; the query words are marked in the results; `?q=text` opens with results.
- Phone (≤ 480px): the small search leaves the bar for a sticky row under it (hidden on the home, which has its big one).
- `?embed=1` hides the top bar and footer and keeps it on every link (docs shown in an app panel); `postMessage({docsLang,
  docsTheme})` from the host switches language / theme.

## 4. Rules
Those of `html-style` (tokens only, both themes, contrast, self-contained, responsive on phone / tablet / TV / touch, print,
reduced motion), plus: the search works without a server and without the index (never an empty box); every interactive
element has a visible `:focus-visible`; results and tiles are 44px tall on touch screens; the UI words are translated by
`DOC_FR` (`site.js` adds its own: Search, Home, Edit this page, Updated, Changelog...).

## 5. Check
`python3 ../html-style/scripts/check_style.py <page.html>` (0 error), then the screenshots of `html-style` §7 (1280 / 768 /
375 px, light and dark), and type a query with a typo in the search: the right section comes first.
