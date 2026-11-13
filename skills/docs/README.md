# Documentation skills

README, GitHub wiki, HTML documentation (single pages, or a whole site with search), explanation and protocol
pages, PDF reports, and the update of every doc after a change. `html-style` is the base of every HTML page; the
`better-*` skills are the documentation-site look of the claude-cluster docs on top of it. Router:
[`/doc`](../routers/doc/SKILL.md).

## Skills
<!-- generated:skills -->
| Skill | What it does | Installs with it |
|---|---|---|
| [`better-explain-doc`](better-explain-doc/SKILL.md) | Explanation pages (Simple \| Technical, steppers, formulas, playgrounds of `explain-doc`) as pages of a `better-html-doc` site: `:::simple` / `:::technical` blocks, the level switch next to the search. | [`better-html-doc`](../docs/better-html-doc/SKILL.md), [`explain-doc`](../docs/explain-doc/SKILL.md) |
| [`better-html-doc`](better-html-doc/SKILL.md) | A whole documentation site in that style, generated from Markdown (`site.toml`, pages `<slug>.<en\|fr>.md`, generators reading the code): home, numbered section cards, search over every page and section, EN / FR, sitemap, contextual help map, `--split`; `check_site.py` (links, anchors, languages, html-style rules) and `test_search.js`. | [`better-html-style`](../docs/better-html-style/SKILL.md), [`html-style`](../docs/html-style/SKILL.md) |
| [`better-html-style`](better-html-style/SKILL.md) | The look of the claude-cluster docs site on top of `html-style`: a typo-tolerant search (top bar + big home search, `/` or Ctrl+K, a site index, a server endpoint or the sections of the page itself), home hero, section tiles, sidebar by section, page meta, changelog, embed mode, phone layout. | [`html-style`](../docs/html-style/SKILL.md) |
| [`better-protocol-doc`](better-protocol-doc/SKILL.md) | Protocol references (`protocol-doc` content) as pages of a `better-html-doc` site: one `:::message` per message (byte layout sized by the fields, fields table), handshake sequences, code tables. | [`better-html-doc`](../docs/better-html-doc/SKILL.md), [`protocol-doc`](../docs/protocol-doc/SKILL.md) |
| [`docs-update`](docs-update/SKILL.md) | Brings every piece of documentation up to date after changes or a new version: README, CHANGELOG, HTML docs and graph, `gh-pages`, wiki, packages / manifests / version numbers, setup and install scripts, `--help` texts, completions, marketplace and skill lists, claude.ai bundles - finds every place the change is written, takes the newest version from the code, verifies against the code (`scripts/inventory.sh`), commits nothing on its own. | [`git-conventions`](../git/git-conventions/SKILL.md), [`readme-style`](../docs/readme-style/SKILL.md), [`wiki-style`](../docs/wiki-style/SKILL.md), [`html-doc`](../docs/html-doc/SKILL.md), [`report`](../docs/report/SKILL.md) |
| [`explain-doc`](explain-doc/SKILL.md) | Explanation pages (how something works) in two levels switched in place, **Simple** (analogies, plain words, numbers) and **Technical** (terms, formulas, complexity, edge cases, code, sources), with SVG diagrams, step-by-step animations, MathML formulas, interactive playgrounds / simulations checking the formulas, quizzes and a glossary. Built on `html-style`. | [`html-style`](../docs/html-style/SKILL.md) |
| [`html-doc`](html-doc/SKILL.md) | One uniform style for the HTML documentation: an optional user guide, the technical documentation and an interactive 2D/3D project graph of any common stack (C / C++, Python, JS / TS / web, Java, C#, Go, Rust, PHP, Ruby) with execution / inheritance / flame diagrams (filters by category/group/relation, rebuilt from the GitHub repository with an update button), all static self-contained pages with a sun/moon theme button. | [`html-style`](../docs/html-style/SKILL.md) |
| [`html-style`](html-style/SKILL.md) | My visual style for any HTML page (from `html-doc`): light / dark color tokens, system fonts and type scale, layout, components, SVG diagram classes, sun/moon and EN/FR switches, rules (self-contained, WCAG contrast, **responsive on phones / tablets / TV / touch**: viewport, safe areas, 44px targets, no hover-only content, print); `scripts/new_page.py` builds a self-contained page, `scripts/check_style.py` checks a page or a palette (tokens, contrast, unreadable inherited colors, external resources). | — |
| [`protocol-doc`](protocol-doc/SKILL.md) | Protocol / wire format documentation (from the 2ETP doc of my networking projects): framing with the control bytes, the payload types table, one section per message type (grammar + fields), the code tables (errors, events, commands), every frame in one block, the handshake step by step with a sequence diagram, the client / server commands, the storage format; always asks the output - Markdown, PDF (report style) or sections of the existing HTML docs (html-doc) - and checks it (`check_protocol.py`: anchors, types, frames, codes). | [`readme-style`](../docs/readme-style/SKILL.md), [`report`](../docs/report/SKILL.md), [`html-doc`](../docs/html-doc/SKILL.md), [`html-style`](../docs/html-style/SKILL.md) |
| [`readme-style`](readme-style/SKILL.md) | README / Markdown docs structure: Table of Contents, Dependencies, Packages, Quick Setup, Usage, GitHub callouts, tables. | — |
| [`report`](report/SKILL.md) | Reports in my style as Markdown and/or PDF (format asked: both recommended), English by default (Markdown -> WeasyPrint, A4, Noto, navy title rule, tables and callouts) with `scripts/md2pdf.py`. | — |
| [`wiki-style`](wiki-style/SKILL.md) | GitHub wiki of a project, from the libutils wiki: the `<repo>.wiki.git` repository, `Home` (version stamp, pages, dependencies), `_Sidebar` / `_Footer`, guide pages, the `Sections` / `Tools-Preview` overviews and one page per section (opening callout, Table of Contents, tables of members, Example, Errors); templates and `scripts/check_wiki.py` (links, anchors, sidebar coverage, code fences, tables). | [`readme-style`](../docs/readme-style/SKILL.md) |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## Examples
- *write the README of this project*
- *a documentation site with search for this project, in English and French*
- *explain how the scheduler works, simple and technical*
- *update all the docs after the release*

[All the categories](../README.md) · [Installation](../../INSTALL.md)
