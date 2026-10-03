---
name: html-doc
description: Uniform HTML documentation of the user's projects (docs/ folder, GitHub Pages) - up to 3 static self-contained pages sharing one style based on the R-Type architecture doc - an optional user guide for non-technical users, the technical documentation (sticky summary, numbered cards, tables, callouts, SVG diagrams) and an interactive 2D/3D project graph (files, classes, functions, tests, CMake targets, externals; filters by category/group/relation; update button fetching the GitHub repository) - with a light/dark sun/moon toggle. Use whenever creating, updating or restyling an HTML doc, unless the user explicitly asks for another style.
---

# Uniform HTML documentation

Up to **3 pages**, each a single static self-contained `.html` (inline CSS/JS, no CDN, no web font,
no framework, works offline and from `file://`), in `docs/`:

| Page | File | When | Template |
|---|---|---|---|
| Guide (user documentation) | `index.html` | **only if** the tool is meant to be used by non-technical users | `templates/user.html` |
| Technical documentation | `technical.html` (`index.html` when there is no guide) | always | `templates/technical.html` |
| Project graph | `graph.html` | always (C/C++ project) | `templates/graph.html` + `scripts/build_graph.mjs` |

Keep the CSS of the templates **as is** (style of `r-type/docs/architecture.html`), unless the user
explicitly asks for another style. Only the content changes. Ask (AskUserQuestion) whether the guide page
is needed when the audience of the tool is not obvious.

## Common to the 3 pages
- Style: the R-Type doc colors (CSS variables, never changed) merged with the layout of the context-forge site.
- Top bar: mobile menu button (guide / technical), **project logo** (inline SVG in `.brand`, `currentColor` = accent;
  replace the default glyph by the project logo when there is one) + name + **version badge** (`{{VERSION}}`), page
  tabs (`<nav class="tabs">`, hidden on mobile where the menu replaces them), then the grouped actions
  `.top-actions`: GitHub icon (`{{REPO_URL}}`) | divider | language switch, theme switch (same height).
- **Repository data fetched**: `<html data-repo="{{GITHUB_REPO}}">` (`owner/repo`); at load the page asks the GitHub
  API (cached 1 h) for the repository name, latest release (or tag), license and URL and replaces the written values
  in `.repo-name` (brand, footer, page title), `.repo-version`, `.repo-license`, `a.repo-link`. Offline / private /
  API limit: the written placeholders stay, so still fill them correctly.
- No scroll beyond the page (`overscroll-behavior: none`); on wide screens the content column is centered on the
  page, the contents list sits on its left.
- **The pages are independent**: each declares its kind (`<html data-page="guide|technical|graph">`). After creating
  or removing a page, run `python3 SKILL_DIR/scripts/sync_nav.py docs`: it rebuilds the tabs of every page from the
  pages present (Guide, Technical, Graph), marks the current one and **hides the tabs when there is a single page**
  (`build_graph.mjs` runs it by itself). Online, a `HEAD` check also hides the tab of a missing page.
  File names: guide = `index.html`; technical = `index.html` when there is no guide, else `technical.html`;
  graph = `graph.html`. Generate the technical page and the graph by default (both linked to each other).
- **Language switch EN | FR** (pill at the top right, before the theme switch), **English by default**, the choice
  is saved under `{{STORAGE_KEY}}-lang` and shared by the pages:
  - the content is written in **both languages**: every text node of the page (summary links, titles, paragraphs,
    table cells, captions, `{{TITLE}}` / `{{SUBTITLE}}`) as a pair `<span data-lang="en">...</span><span
    data-lang="fr">...</span>` (or a block `<div data-lang="en">` + `<div data-lang="fr">` for long parts); only the
    active language is displayed;
  - UI strings carry `data-i18n` (text) / `data-i18n-title|aria-label|placeholder` (attributes) and are translated
    by `window.DOC_FR` (English text -> French text) at the top of the page: add the new UI strings there;
  - scripts translate their generated texts with `window.docI18n.tr()` and re-render on `docI18n.onChange()`
    (the graph does it for its legends, status, details, hint and help: `HELP` / `HELP_FR`).
  Another language than EN/FR only on request (add a button and a dictionary).
- **Theme switch** at the top right: a sliding toggle, blue track with a white sun (light), black track with a yellow
  moon and stars (dark), `role="switch"` + `aria-checked`; choice saved under `{{STORAGE_KEY}}-theme`, default =
  system preference.
- Placeholders: `{{PROJECT}}`, `{{VERSION}}` (`vX.Y.Z`), `{{REPO_URL}}`, `{{STORAGE_KEY}}` (project slug),
  `{{TITLE}}`, `{{SUBTITLE}}`, `{{LICENSE}}` (SPDX id, footer), `{{GITHUB_REPO}}` (`owner/repo`).
- Page header: `.page-eyebrow` (small accent uppercase label: "User guide", "Technical documentation", "Architecture"),
  `h1`, `p.lead` (muted introduction).
- Footer `footer.site` (guide / technical): "<project> documentation · generated from the repository (version,
  license)" in both languages; previous / next page cards `nav.pager` above it (generated by `sync_nav.py`).
- Before delivering: valid HTML, every tab/summary link points to an existing page/`id`, readable in both themes
  and at phone width (860px breakpoint, no horizontal scroll).

## 1. Guide (`templates/user.html`, optional)
For people who just want to **use** the tool: no jargon, no internal detail, task oriented.
- Sections: About, Installation, First steps, Usage (one sub-part per task, "I want to… / I do…"
  table), FAQ. Adapt the titles, keep 4-6 sections.
- Components: numbered steps `<ol class="steps">` (each `<li><strong>Action</strong> detail`), `<kbd>` keys,
  commands to copy in `<pre><code>`, FAQ `<details class="faq"><summary>`, callouts for warnings/results,
  `p.lead` for the intro sentence.
- Say what the user sees when it works; one action per step.

## 2. Technical documentation (`templates/technical.html`)
- **Contents list** `<aside class="sidebar">` on the left (sticky, off-canvas menu on mobile): title "Contents", one
  link per section of the page (same numbering, left border), the visible section highlighted. The pages are reached
  by the top tabs and the previous / next cards.
- `h2` / `h3` with an `id` (or in a `section[id]`) get a `#` anchor link on hover.
- One `<section id="...">` card per topic, preceded by `<!-- N -->`, `<h2>N. Title</h2>`, sub-parts `<h3>N.M</h3>`.
  Usual order: overview, split into libraries/modules, each module, network/protocol, build. 5-9 sections.
- Components:

  | Need | Component |
  |---|---|
  | list of elements with attributes | `<div class="table-wrap"><table>` with `<thead>` |
  | remark / info | `<div class="callout">` |
  | limit, risk, planned-not-done | `<div class="callout warn">` |
  | validated / good practice | `<div class="callout ok">` |
  | code, command, path, type | `<code>` inline, `<pre><code>` block (a **Copy** button is added on hover) |
  | titled code block (file name, command role) | `<pre data-title="Install"><code>...</code></pre>` |
  | alternatives (OS, package manager, language) | content tabs `<div class="ctabs" data-tabs="g"><button data-tab="a">` + `<div class="ctab-panel" data-tabs="g" data-tab="a">` |
  | architecture, flow, tree, loop | inline `<svg>` in `<figure>` + `<figcaption>Figure N: ...` |
- SVG: `viewBox="0 0 760 H"`, `role="img"` + `aria-label`, only the template classes (`box`, `box-alt`, `group`,
  `lbl`, `small`, `title`, `edge`, `edge-accent`, `dash`, `arrow`, `arrow-accent`), markers once in `<defs>`.
  Draw the real mechanism, not decoration. Link to the graph page for the full dependency view.
- Short factual paragraphs, present tense, explicit **planned** vs **implemented**; every identifier in `<code>`;
  no emoji, no gradient, only the CSS variables.

## 3. Project graph (`templates/graph.html`)
Interactive graph drawn on a canvas by an embedded engine (no library); the panel starts with the eyebrow
"Architecture", and the categories / groups / relations are **two-column legends** (dot swatch for the nodes and the
groups, line swatch for the relations, count on the right, disabled entries faded, `all` / `none` buttons; hovering
an entry highlights its nodes / relations; clicking it shows / hides them, a relation toggle doesn't move the
layout): 2D (pan, zoom, drag nodes) and
3D (rotate, Shift+drag to move the target in the screen plane, drag a node to place it, zoom; spheres lit by a
light fixed in the scene, depth fog, floor grid following the camera, X/Y/Z gizmo ball resetting the angle on
click; bottom right switch **Realistic** (default) / **Technical** = flat nodes + arrows, no light / fog / floor,
remembered); key **F** centers the camera on the selected node (2D and 3D), search, presets (overview, files & includes, classes & inheritance,
classes & methods, tests, build, all), toggles per **category** (header, source, test file, class, struct,
interface, abstract, enum, function, method, test, test helper, executable, library, external), per **group**
(module folder) and per **relation** (includes, defines, member, inherits, implements, compiled-into, links,
tests), colour by category or group, cluster by group, labels, isolate the selection, details panel with
clickable in/out relations, and an **"Animate the direction of the relations"** option (off by default) moving dots along every
relation from the source to the target.
Every control has a **tooltip** (short description after 0.45 s of hover) and a **help card on double click** (longer
description + an animated SVG example), defined in the `HELP` / `EX` tables of the page: when a category, relation or
option is added, add its entry there too.

Generation (never fill the snapshot by hand):
```bash
node SKILL_DIR/scripts/build_graph.mjs --repo <project> [--out <project>/docs/graph.html] [--ref HEAD] \
     [--github owner/repo] [--branch main] [--project name] [--version vX.Y.Z]
```
- Reads the sources **from the git commit** (`--ref`), extracts the graph with the `extractGraph()` of the page
  (single source of truth, between the `// <extractor>` markers), embeds the snapshot (commit hash + date) and the
  config (GitHub repo from `origin`, branch, version from `project(... VERSION)`).
- Re-running it regenerates the page from the template (new features included); `--keep-html` keeps the HTML/CSS of
  an existing page and only replaces the script, the config and the snapshot.
- The page shows the embedded snapshot, then **once at load** (and on the "Update" button) asks the
  GitHub API for the last commit of the branch: if it is newer, it downloads the sources
  (`raw.githubusercontent.com`) and rebuilds the graph in the browser (cached in `localStorage`). Offline, API
  limit (60 requests/h), private repository or local commits not pushed: the snapshot is kept and the status says why.
- The extractor targets C/C++ projects in the user's style (`include/`, `src/`, `tests/`, `CMakeLists.txt` with
  `add_subdirectory`, `foreach`, `set(SRC ...)`). For another language, adapt `extractGraph()` in the template
  (files -> nodes/links) and keep the rest.
- Check after generation: open it (or screenshot it) and look at the node/link counts printed by the script.

## 4. Publication (GitHub Pages)
Only when the project is a git repository (`git rev-parse --is-inside-work-tree`): ask (AskUserQuestion, French)
**"Publier la documentation sur la branche `gh-pages` ?"** - `Oui, commit + push` · `Oui, commit local seulement` ·
`Non (garder docs/ seulement)`. If yes:
- work in a temporary worktree of `gh-pages` (`git worktree add <tmp> gh-pages`, or `--orphan gh-pages` when it
  doesn't exist), copy the pages of `docs/` at its root (or in a sub-folder when the root is used: ask), **never
  delete the other files of the branch** (packages mirror, `.repo`, workflows of libutils/context-forge live there),
  commit with `git-conventions` (`docs(pages): update the documentation (<short sha of main>)`), push only with
  the "commit + push" answer, then remove the worktree.
- GitHub Pages not enabled yet (`gh api repos/<o>/<r>/pages` -> 404): propose
  `gh api -X POST repos/<o>/<r>/pages -f "source[branch]=gh-pages" -f "source[path]=/"` and give the URL
  `https://<owner>.github.io/<repo>/`.
