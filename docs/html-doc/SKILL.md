---
name: html-doc
description: Uniform HTML documentation of the user's projects (docs/ folder, GitHub Pages) - up to 3 static self-contained pages sharing one style based on the R-Type architecture doc - an optional user guide for non-technical users, the technical documentation (sticky summary, numbered cards, tables, callouts, SVG diagrams) and an interactive 2D/3D project graph of the common languages (C / C++, Python, JS / TS / web, Java / Kotlin, C#, Go, Rust, Zig, Swift, Dart, PHP, Ruby, Lua, Julia, Elixir, Haskell, Objective-C, shell: files, classes, functions, calls, tests, build targets / packages, externals; filters by category/group/relation; update button fetching the GitHub repository) - with a light/dark sun/moon toggle. Use whenever creating, updating or restyling an HTML doc, unless the user explicitly asks for another style.
---

# Uniform HTML documentation

Up to **3 pages**, each a single static self-contained `.html` (inline CSS/JS, no CDN, no web font,
no framework, works offline and from `file://`), in `docs/`:

| Page | File | When | Template |
|---|---|---|---|
| Guide (user documentation) | `index.html` | **only if** the tool is meant to be used by non-technical users | `templates/user.html` |
| Technical documentation | `technical.html` (`index.html` when there is no guide) | always | `templates/technical.html` |
| Project graph | `graph.html` | always (any supported language) | `templates/graph.html` + `scripts/build_graph.mjs` |

Keep the CSS of the templates **as is** (style of `r-type/docs/architecture.html`, defined in the `html-style` skill:
tokens, components, rules, `check_style.py`), unless the user explicitly asks for another style; a change of the
style is made in `html-style/templates/style.css` and in the `<style>` of these templates. Only the content changes. Ask (AskUserQuestion) whether the guide page
is needed when the audience of the tool is not obvious.

## Common to the 3 pages
- Style: the R-Type doc colors (CSS variables, never changed) merged with the layout of the context-forge site.
- Top bar: mobile menu button (guide / technical), **logo** = the real logo of the project when the repository has one, else the default documentation
  logo, never a pictogram invented from the topic (no padlock for a crypto project): `python3
  ../html-style/scripts/set_logo.py docs` sets it on every page + name + **version badge** (`{{VERSION}}`), page
  tabs (`<nav class="tabs">`, hidden on mobile where the menu replaces them), then the grouped actions
  `.top-actions`: GitHub icon (`{{REPO_URL}}`) | divider | language switch, theme switch (same height).
- **Repository data fetched**: `<html data-repo="{{GITHUB_REPO}}">` (`owner/repo`); at load the page asks the GitHub
  API (cached 1 h) for the repository name, latest release (or tag), license and URL and replaces the written values
  in `.repo-name` (brand, footer, page title), `.repo-version`, `.repo-license`, `a.repo-link`. Offline / private /
  API limit: the written placeholders stay, so still fill them correctly.
- No scroll beyond the page (`overscroll-behavior: none`); on wide screens the content column is centered on the
  page, the contents list sits on its left.
- **The pages are independent**: each declares its kind (`<html data-page="guide|technical|graph">`). After creating
  or removing a page, run `python3 SKILL_DIR/scripts/sync_nav.py docs` (shared with `explain-doc`, in `html-style`): it
  rebuilds the tabs of every page from the pages present (Guide, Technical, Graph, then the explanation pages of
  `explain-doc` in the same folder, which get the same tabs, brand, GitHub link and EN / FR switch: both sides
  are linked whichever is generated last), marks the current one and **hides the tabs when there is a single page**
  (`build_graph.mjs` runs it by itself). At load, each page also checks the other tabs (online: `HEAD` request; `file://`: the page loaded as a hidden
  script) and removes the tab of a missing page, and the whole bar when a single page remains.
  File names: guide = `index.html`; technical = `index.html` when there is no guide, else `technical.html`;
  graph = `graph.html`. Generate the technical page and the graph by default (both linked to each other).
- **Language switch EN | FR** (pill at the top right, before the theme switch), **English by default**, the choice
  is saved under `{{STORAGE_KEY}}-lang` and shared by the pages (also the theme, and the level of the `explain-doc`
  pages): the links between the pages carry it (`?lang=fr&theme=dark`), so it applies to every page even from
  `file://` where each page has its own storage:
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
- SVG arrows: computed with `data-from` / `data-to` (see `html-style`), never typed coordinates; run
  `python3 ../html-style/scripts/check_style.py docs/*.html` (0 error: arrows on their shapes, text inside).
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
click; arrow heads on the relations in both styles; bottom right switch **Realistic** (default) / **Technical** = the nodes of context-forge (3d-force-graph): 8 x 8 faceted sphere, Lambert light per pixel with
an ambient 0.8 + a directional 0.6 from the top, opacity 0.95) + cone arrows, no fog / floor,
remembered); key **F** centers and zooms the camera on the selected node, framed with its close neighbours (2D and 3D) and keeps the focus on it while you only zoom / dezoom (another move of the camera ends it); the 3D orbit pivots around the nearest nodes in front of the camera; 2D: the scroll zooms towards the mouse (or the selected node);
3D: free flight, the scroll moves the camera forward / backward towards the mouse (or the selected node), no limit;
keys (physical positions, labels read from the keyboard layout): arrows = move in the screen plane, ZQSD (WASD on
QWERTY) = move along the floor, A / E (Q / E) = height (zoom in 2D), Shift = faster; click on the axes = reset the angle, double
click on the axes or key R = reset the whole view (position, zoom, angle; R also in 2D);
limits: in 3D the camera stays in a box around the nodes (the floor is its bottom), whose walls appear as an accent
grid when the camera gets close; in 2D the view center stays near the graph and the zoom out is limited; the hover
card follows the view when it moves under a still mouse; hovering a node shows a card (kind in the node colour with the
file extension, name, path); slightly transparent nodes in 3D, a near plane (nodes the camera passes are
clipped like behind a WebGL camera, the camera can fly through the graph); no hint text: a **"?" button** (bottom left, key ? or H) opens a card listing the
controls of the current mode, each with a small drawing of the gesture (mouse / wheel / keys) and its description, search, presets (overview, files & includes, classes & inheritance,
classes & methods, tests, build, all), toggles per **category** (header, source, test file, class, struct,
interface, abstract, enum, function, method, test, test helper, executable, library, external), per **group**
(module folder) and per **relation** (includes, defines, member, inherits, implements, compiled-into, links,
tests, **calls**: read in the bodies of the functions / methods, resolved by name, by the type of the local variables
and of the class fields, `make_unique<T>`; the GoogleTest macros are not functions; `main()` is a node marked entry), colour by category or group, cluster by group, labels, isolate the selection, details panel with
clickable in/out relations, and an **"Animate the direction of the relations"** option (off by default) moving dots along every
relation from the source to the target.
**Diagram mode** (third button after 2D / 3D, bar at the top left of the graph): readable diagrams built from the
relations, drawn as boxes with the 2D camera:
- **Execution**: the calls from an entry point (list: `main()` first, then the uncalled functions, the biggest
  first), one level per call depth, the methods of a class grouped in a dashed box, dashed arrows back to an earlier
  level; double click a box to start from it;
- **Inheritance**: the class tree of the sources (bases on top, UML hollow triangles), wide levels wrap;
- **Flame graph**: the static call tree (width = size of the subtree, not a measured time), double click a frame
  to zoom into it, the ancestor rows below to go back.
  The boxes can be moved (a box, a dashed class group, or several selected boxes: Ctrl / Shift + click, Shift /
  Ctrl + drag on the background for an area).
  In this mode the filters of the panel (views, categories, groups, relations, graph options, "Color by": the boxes
  use the category colours) are greyed; the search stays active (matching boxes outlined, Enter centers the first one or starts the diagram from it).
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
- The extractor (regex parsers, no AST) reads:

  | Stack | Files | What is extracted |
  |---|---|---|
  | C / C++ | `.hpp .h .cpp .c...`, `CMakeLists.txt` | includes, namespaces, classes + bases, methods, functions, bodies, CMake targets (`add_subdirectory`, `foreach`, `set(SRC ...)`), linked libraries |
  | Python | `.py`, `pyproject.toml`, `setup.py`, `requirements*.txt` | imports (relative / package / `src/`), classes + bases (ABC / Protocol), methods, functions, `if __name__ == "__main__"` |
  | JS / TS / web | `.js .ts .jsx .tsx .vue .svelte`, `.html`, `.css .scss`, `package.json` | import / require / dynamic import (`@/` alias), classes / interfaces (extends / implements), functions / arrow functions, methods; pages (script / link / a) and stylesheets (@import, partials) |
  | Java / Kotlin | `.java .kt .scala`, `pom.xml`, `build.gradle` | imports (packages), classes / interfaces / records / objects, methods, `main` |
  | C# | `.cs`, `*.csproj` | `using` (namespaces of the project), classes / interfaces / records, methods, `Main` |
  | Go | `.go`, `go.mod` | imports (packages of the module), structs / interfaces, functions, methods (receivers, even in another file) |
  | Rust | `.rs`, `Cargo.toml` (workspaces: one package per crate) | `mod` / `use crate::` / `self::` / `super::` / sibling modules, structs / enums / traits (+ their methods), `impl` (trait for type = base, std traits ignored), functions, methods (even in another file), `Self::f()` calls, raw strings `r#"..."#` skipped, `#[test]` |
  | PHP | `.php`, `composer.json` | `use` (PSR-4 paths), require / include, classes / interfaces / traits, methods |
  | Ruby | `.rb`, `Gemfile`, `*.gemspec` | require / require_relative, classes (`<` base), modules, methods (`def ... end`) |
  | Zig | `.zig`, `build.zig.zon` | `@import`, `const X = struct / enum / union`, functions, methods, `test "..."` blocks |
  | Swift | `.swift`, `Package.swift` | imports (modules), classes / structs / protocols / actors, `extension` (methods + conformances), functions |
  | Dart / Flutter | `.dart`, `pubspec.yaml` | `package:` / relative imports, classes (extends / with / implements), mixins, extensions, functions |
  | Lua | `.lua`, `*.rockspec` | require, `function M.f` / `Class:method` (OOP tables, `class()` / `:extend()` / `:subclass()`), `... end` blocks |
  | Julia | `.jl`, `Project.toml` | include / using, modules, structs `<:` base, abstract types, functions (long and short form) |
  | Elixir | `.ex .exs`, `mix.exs` | alias / import / use, `defmodule` / `defprotocol` / `defimpl`, `def` / `defp`, `Module.fn()` calls |
  | Haskell | `.hs`, `*.cabal` | imports (modules), data / newtype / class / instance, top-level functions, calls without parentheses |
  | Objective-C | `.m .mm .h`, `*.podspec` | `#import`, `@interface` / `@protocol` / `@implementation`, methods, message sends `[obj msg]` |
  | Shell | `.sh .bash .zsh` | `source` / `.`, functions, commands calling the functions |
  | C / C++ (Meson) | `meson.build` | `executable()` / `library()`, their sources (lists and `files()`), `dependency()` |

  Common to all: test files (`tests/`, `__tests__/`, `*_test.*`, `*.test.*`, `*Test.java`...) and test cases
  (GoogleTest, pytest, Jest / Vitest, Go, Rust, JUnit, xUnit / NUnit, PHPUnit, RSpec / Minitest), the classes /
  functions of the tests as test helpers, the **calls** (by name, `this` / `self` / receiver, the type of the local
  variables and fields, constructions), one **package** per manifest folder (its files compiled into it, its
  dependencies as externals, an import of a package of the repository points to it, a package with an entry point
  is an executable), the externals deduplicated by name, the groups from the folders (generic ones skipped: `src`,
  `lib`, `main`, `java`, `com`...). Ignored folders: `node_modules`, `dist`, `build`, `target`, `venv`, `vendor`,
  `__pycache__`, `.next`, `coverage`, `bin`, `obj`, `docs`... (`extractGraph.wanted()`, shared by the script and the
  update of the page). Another language: add its regexes in `extractGraph()` of the template and keep the rest.
- Check after generation: open it (or screenshot it) and look at the node/link counts printed by the script.

## 4. Publication (GitHub Pages)
Only when the project is a git repository (`git rev-parse --is-inside-work-tree`): ask (AskUserQuestion, French)
**"Publier la documentation sur la branche `gh-pages` ?"** - `Oui, commit + push` · `Oui, commit local seulement` ·
`Non (garder docs/ seulement)`. If yes:
- **The Pages URL must open the documentation directly**: `https://<owner>.github.io/<repo>/` serves the `index.html`
  at the root of `gh-pages`; pages left only in a sub-folder are never reached by default (404 or the mirror
  listing). Look at the root of the branch first (`git ls-tree --name-only gh-pages`), then ask (AskUserQuestion,
  French) **"Où placer la documentation sur `gh-pages` ?"**:
  - `Dans docs/ + index à la racine qui redirige (Recommandé)`: pages in `docs/`, plus a root `index.html`
    redirecting to `docs/` (URL of the doc: `.../<repo>/docs/`, the root link works too);
  - `À la racine`: pages copied at the root of the branch (URL `.../<repo>/`); not possible when another root
    `index.html` / `technical.html` / `graph.html` exists that isn't the doc: say so and propose the first choice;
  - `Toujours dans docs/ (sans redirection)`: pages in `docs/` only, the root link does not lead to them (give
    the `.../<repo>/docs/` URL and add it to the README).
  Remember the answer for the project: next publications reuse the same layout without asking again (the layout
  already on the branch tells it: `docs/` + redirect, root pages, or `docs/` alone).
- Root redirect (`index.html`, only when the root has no other `index.html`; an existing one that isn't a redirect
  is never overwritten: ask):
  ```html
  <!DOCTYPE html>
  <html lang="en"><head><meta charset="utf-8"><title>{{PROJECT}} documentation</title>
  <meta http-equiv="refresh" content="0; url=docs/">
  <link rel="canonical" href="docs/">
  <script>location.replace("docs/" + location.search + location.hash);</script>
  </head><body><a href="docs/">{{PROJECT}} documentation</a></body></html>
  ```
- Add an empty `.nojekyll` at the root of the branch when missing (files served as they are).
- Work in a temporary worktree of `gh-pages` (`git worktree add <tmp> gh-pages`, or `--orphan gh-pages` when it
  doesn't exist), copy the pages of `docs/` to the chosen place, **never delete the other files of the branch**
  (packages mirror, `.repo`, workflows of libutils/context-forge live there), commit with `git-conventions`
  (`docs(pages): update the documentation (<short sha of main>)`), push only with the "commit + push" answer, then
  remove the worktree.
- GitHub Pages not enabled yet (`gh api repos/<o>/<r>/pages` -> 404): propose
  `gh api -X POST repos/<o>/<r>/pages -f "source[branch]=gh-pages" -f "source[path]=/"` (always the root of the
  branch, the layout above decides where the doc sits). Give the URL that opens the doc
  (`https://<owner>.github.io/<repo>/`, or `.../<repo>/docs/` without redirect) and check it once deployed
  (`gh api repos/<o>/<r>/pages/builds/latest`, then a `HEAD` request on the URL).

## 5. Graph explorer (any public repository)
`scripts/build_explorer.mjs --out graph.html` builds the graph page without embedded project: at the first visit it
asks for a public **GitHub or GitLab** repository (`owner/repo`, `github.com/...(/tree/<branch>)`,
`gitlab.com/...(/-/tree/<branch>)`, a self-hosted GitLab, or `?repo=` in the address), reads its sources from the
browser (GitHub: 3 API requests per load, raw files from `raw.githubusercontent.com`; GitLab: API v4) and builds the
graph with the same extractor. The link stays editable in the panel (Open / Update), is remembered by the browser
with the graph of each repository until **Ctrl + Shift + R** (forgets them and reloads the page, which asks again). It is published on the
`gh-pages` branch of the skills repository (`graph.html` + `index.html` redirecting to it):
https://tsukini22.github.io/skills/ - regenerate it after a change of the template.
- Big graphs (thousands of nodes): Barnes-Hut repulsion above 300 nodes (typed arrays; the exact loop below, same
  look), the simulation in a Web Worker above 3000 nodes (the page stays fluid while it settles), cached sphere
  sprites (rebuilt per ~1.5° of camera angle, fog baked in), and above 2000 nodes / 2500 relations: relations batched
  as hairlines (off-screen and < 3 px skipped, readable arrow heads only), tiny nodes batched per colour, 150 names
  at most; while the view or the layout moves, a light drawing (square nodes, part of the relations, no heads /
  names) and the full drawing as soon as it is still. Smaller graphs keep the exact drawing.
- Fetch of the sources: GitHub raw files 32 at a time; GitLab file list pages in parallel and the contents by batches
  of 90 files through GraphQL (~12 requests for 1000 files; the archive endpoint refuses cross-origin pages), the
  limits of the APIs waited and retried.
