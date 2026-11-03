---
name: better-html-doc
description: A whole documentation site in the better-html-style (the look and engine of the claude-cluster docs) generated from Markdown - site.toml (name, languages EN / FR / both, sections with icons, home, kinds of pages, contextual help map), one Markdown file per page and language with TOML front matter, optional gen/*.py generators reading the code, a home with hero search, section tiles and changelog, numbered section cards, a sidebar by section, previous / next, updated date, a typo-tolerant search over every page and section (/ or Ctrl+K), sitemap, --split mode, explanation and protocol pages in the same site, checks (links, anchors, both languages, html-style rules) and a search test; responsive. Use whenever the user wants a better / searchable / multi-page documentation site, "une doc comme celle de claude-cluster", docs with search, or to move a project's docs to that style; a single page: better-html-style; the older single-page technical doc: html-doc.
---

# Documentation site (better-html-doc)

`SKILL_DIR` = directory of this file. Built on **`better-html-style`** (search, home, site navigation) and **`html-style`**
(tokens, components, rules): both must sit next to this skill (`../`). Every fact of the site is **read from the project**
(its code, README, config, CHANGELOG, commands): never invented; what is planned is marked `> [!PLANNED]`.

## 1. Ask (one AskUserQuestion call, French), unless already given
1. **Langues** : `Bilingue EN / FR (Recommandé)` · `Anglais` · `Français`.
2. **Emplacement** : `docs/site/ (Recommandé)` (sources) + `docs/` or the gh-pages branch for the built pages · other.
3. **Sections** (multiSelect, the ones the project needs pre-described): `Guide utilisateur` · `Concepts / explications`
   (pages of `better-explain-doc`) · `Référence` (configuration, commands, API, formats; `Protocole` with `better-protocol-doc`)
   · `Ressources` (FAQ, glossary, changelog, downloads) · `Sécurité`.
4. **Logo**: as `html-style` "Logo of the brand" (repository logo / custom SVG / default), only when the site has none yet.

## 2. Create the site
```bash
cp -r SKILL_DIR/templates/site docs/site            # site.toml, src/pages/{guide,reference,resources}/*.md, gen/_example.py
# fill site.toml ({{PROJECT}}, {{REPO_URL}}, taglines, sections), write the pages, then:
python3 SKILL_DIR/scripts/build_site.py --site docs/site --out docs/site/out [--version X.Y.Z] [--split]
python3 SKILL_DIR/scripts/check_site.py docs/site/out              # 0 error
node SKILL_DIR/scripts/test_search.js docs/site/out [cases.json]   # every page found by its title, also with a typo
```
- `--split`: the style / scripts shared by every page go to `site.css` / `site.js` (a big site: smaller pages, one cache);
  without it each page is self-contained (offline, `file://`, attachable alone).
- The build is deterministic (same sources, same output): commit the sources; build the pages in CI or before publishing.
- Version: `--version`, else `[site] version`, else the last git tag; the commit is added when the site is in a git repository.

## 3. site.toml
| Table | Keys |
|---|---|
| `[site]` | `name`, `langs` (`["en","fr"]` / `["en"]` / `["fr"]`), `repo` (code icon), `edit_url` (empty: no "Edit this page" link - set it, `…/edit/main/docs/site/{path}`, only when the user asks for that link), `pages_url` (sitemap), `storage_key`, `logo` (SVG), `version`, `footer`, `css` / `js` (project additions, tokens only), `copy` (files copied next to the pages), `search_endpoint` (a server search) |
| `[home]` | `title`, `tagline`, `changelog` (a Keep a Changelog file: its last 5 versions), `links` (`{href, text}`) |
| `[[sections]]` | `id`, `title`, `desc`, `icon` (the inside of a 24x24 stroke SVG) - in reading order |
| `[kinds.<name>]` | `css`, `js`, `actions`, `attrs`, `label` - a kind of page with its own assets (`explain`, `protocol` are built in) |
| `[contexts]` | `screen = "page#id"` → `docs-map.json` (the `?` buttons of an application open the right page; checked) |
A text is a string (every language) or `{ en = "...", fr = "..." }`. `@skill/<skill>/<path>` points into a sibling skill.

## 4. Pages
`src/pages/<folder>/<slug>.<en|fr>.md` (`<slug>.md` on a site of one language); the folders are free, the **slug** is the file
name of the page (`<slug>.html`). Front matter (`+++` TOML `+++`):
`title`, `summary` (the lead, the search description), `section` (an id of site.toml), `order`, `keywords` (search only),
`kind` (`explain` / `protocol` / a kind of site.toml), `level` (default level of an explanation page). The FR file needs only
the translated `title` / `summary` / `keywords`; the sections must have **the same ids** in every language (`## Title {#id}`).

Markdown (`scripts/mdlib.py`): `#`-`######` headings (`{#id}`; `#` is a section like `##`, the page has one h1, its title: a first `# Title` equal
to it is dropped, a number typed in a title is dropped when it is the number of the section), paragraphs (two spaces at the end of a line: a break),
lists (`-` `*` `+` `1.` `1)`, any depth, blocks inside an item: a fence, a table, a callout, tasks `- [ ]` `- [x]`), tables (`:---:` aligned, outer pipes
optional, `| | |` = no head row), fences ` ``` ` `~~~` (` ```lang "File title" ` → title bar + Copy), indented code, `>` quotes and callouts `> [!NOTE]`
`[!TIP]` `[!IMPORTANT]` `[!WARNING]` `[!CAUTION]` `[!WARN]` `[!OK]` `[!PLANNED]` `[!SOON]` (or `{: .warn}` / `{: .ok}` under a quote; two kinds in a row are
two callouts), `---`, raw HTML (a line opening a block tag: `<details>`, `<figure>`, `<table>`, `<svg>`...; closed by its end tag), `<!-- comments -->`;
inline `code` (``` ``a`b`` ```), **bold**, *italic*, `~~deleted~~`, links `[text](url "title")` and `[text](page:slug#id)` (checked), images
`![alt](src "title")`, `<https://autolink>`, `\*` escapes, `&entities;`, `<kbd>` `<mark>` `<abbr title>` `<small>` `<sub>`... (a tag without its end tag is shown
as text); blocks closed by `:::`: `:::html` (raw HTML: figures, SVG, steppers), `:::details Title` (collapsed), `:::simple` / `:::technical` [`tldr`]
(explanation levels), `:::message NAME code` (a protocol message).
- A table keeps what its columns need (`min-width` on its first row, from its text: words whole, a phrase up to 20 characters on one line, never a word per line). One that
  can fit a phone stays on it; else it scrolls in its box there and a long text gets a column it reads well in; one too wide even for a tablet lets its long words break at a
  `/` or a `-`. The words are never squeezed, and on paper the table takes the width of the page.
- What this Markdown writes and the style skills do not know (a task item, the title of the pager...) is in `templates/markdown.css`, added to every page after `site.css`.
- Structure of a page: the lead says what the page gives; 2-8 `##` sections, each a card numbered in the sidebar; a section
  answers one question; tables for anything with columns; one example per notion; links to the other pages instead of
  repeating them.
- Writing: short sentences, the reader's words, the exact names of the code in `code`; every page in both languages says
  the same (same sections, same examples).

## 5. Generated pages (`gen/*.py`)
What exists in the code is **generated, not copied**: options from the default config, commands from the command table,
API / MCP tools from their definitions, keys from the key map, the changelog. A generator is `gen/<name>.py` with
`pages(ctx) -> [page dicts]` (`slug`, `section`, `order`, `kind`, and per language `title`, `summary`, `md`, `keywords`) and
optionally `search_entries(ctx)` (results that are not pages: a setting, a command → `{k, l, t, d, u, x}`). `ctx.site`,
`ctx.config`, `ctx.langs`, `ctx.version`, `ctx.tr(value, lang)`, `ctx.git(...)`. See `templates/site/gen/_example.py`
(a leading `_` = not run). A slug can't be both a Markdown page and a generated one.

## 6. Search
`search-index.js` holds every page and every section of every page (title, summary, keywords, text) per language, plus the
generator entries; `site.js` (better-html-style) ranks it (typos, prefixes, letters in order). `test_search.js` checks that
every page comes first for its title and stays in the top 3 with a typo; add the project's own cases in a JSON file
(`[["en", "sesend", "session_send"]]`) for the words users type.

## 6b. Accessibility (RGAA 4.1)
Compliant without JavaScript, by the generator and the templates (skip link and `<main id="main">`, `scope` on every `th`,
focusable and named scrolling blocks, no skipped heading level, `lang`), plus the search shortcut `/` that the reader can turn off
(footer checkbox; <kbd>Ctrl</kbd>+<kbd>K</kbd> stays). Do not add a script that patches them: fix the template. A project
page written as raw `:::html` must follow the same rules (a `th` has its `scope`, a scrolling block its `tabindex="0"`).

## 7. Check before delivering
- `check_site.py`: links / anchors / `page:` links, the RGAA structure of every page (`lang`, one `h1`, no skipped heading level, skip link, `th scope`, focusable scrolling blocks), EN and FR blocks balanced, no placeholder (`{{X}}`, TODO, lorem), every page
  in the index, `docs-map.json` targets, and `check_style.py` of html-style on every page (contrast, tokens, responsive) -
  0 error.
- `test_search.js`: 0 FAIL.
- `check_site.py --render` (or `check_render.py` of html-style on the pages): every page really drawn, both themes, 1280 / 768 / 375 px, both languages: 0 error. It runs
  Firefox on every page: on a big site, first on the home, a page with big tables and a phone width (`--render-pages index.html a.html`).
- Screenshots (`check_render.py --shots DIR`): the home and a page, light and dark, desktop and phone, looked at; the search opened with a typo. A long or crowded content is
  worth a try: a table of 20 columns, a word of 100 characters, a very long title or project name.
- The site of claude-cluster (`ai/claude-cluster/docs/site` of the ai-utils repository) is the reference of what a finished
  site looks like (generated pages, contextual help, embedded in a binary).
