---
name: better-html-doc
description: A whole documentation site in the better-html-style (the look and engine of the claude-cluster docs) generated from Markdown - site.toml (name, languages EN / FR / both, sections with icons, home, kinds of pages, contextual help map), one Markdown file per page and language with TOML front matter, optional gen/*.py generators reading the code, a home with hero search, section tiles and changelog, numbered section cards, a sidebar by section, previous / next, updated date and edit link, a typo-tolerant search over every page and section (/ or Ctrl+K), sitemap, --split mode, explanation and protocol pages in the same site, checks (links, anchors, both languages, html-style rules) and a search test; responsive. Use whenever the user wants a better / searchable / multi-page documentation site, "une doc comme celle de claude-cluster", docs with search, or to move a project's docs to that style; a single page: better-html-style; the older single-page technical doc: html-doc.
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
| `[site]` | `name`, `langs` (`["en","fr"]` / `["en"]` / `["fr"]`), `repo` (code icon), `edit_url` (`…/edit/main/docs/site/{path}`: "Edit this page"), `pages_url` (sitemap), `storage_key`, `logo` (SVG), `version`, `footer`, `css` / `js` (project additions, tokens only), `copy` (files copied next to the pages), `search_endpoint` (a server search) |
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

Markdown (`scripts/mdlib.py`): `#`-`####` headings (`{#id}`), paragraphs, lists (one nested level), tables, fences
(` ```lang "File title" ` → title bar + Copy), `>` quotes and callouts `> [!NOTE]` `[!WARN]` `[!OK]` `[!PLANNED]` `[!SOON]`,
`---`, inline `code`, **bold**, *italic*, links `[text](url)` and `[text](page:slug#id)` (checked), `<kbd>`;
blocks closed by `:::`: `:::html` (raw HTML: figures, SVG, steppers), `:::details Title` (collapsed), `:::simple` /
`:::technical` [`tldr`] (explanation levels), `:::message NAME code` (a protocol message).
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
- Screenshots (`html-style` §7): the home and a page at 1280 / 768 / 390 px, light and dark; the search opened with a typo.
- The site of claude-cluster (`ai/claude-cluster/docs/site` of the ai-utils repository) is the reference of what a finished
  site looks like (generated pages, contextual help, embedded in a binary).
