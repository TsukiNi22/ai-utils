---
name: wiki-style
description: Tsukini's style for the GitHub wiki of a project (from the libutils wiki) - the wiki is its own git repository (<repo>.wiki.git, flat folder of Page-Name.md files, no H1 title, links without .md), Home with the version stamp / wiki pages table / dependencies / badges, _Sidebar with Getting started / Sections / Miscellaneous groups, _Footer, guide pages (Installing, Usage, Workflow), an overview page of every section and one page per section / class with the opening callout (section, namespace, headers), Table of Contents, tables of members, Example and Errors - with templates and a checker (check_wiki.py: links, anchors, sidebar coverage, code fences, tables). Use whenever creating, completing, updating or reviewing the wiki of one of the user's repositories, or writing a documentation page meant for a GitHub wiki (README: readme-style, HTML docs: html-doc).
---

# Wiki style (GitHub wiki)

`SKILL_DIR` = directory of this file. English (unless asked otherwise), direct, technical, no emoji. Same Markdown
rules as `readme-style` (callouts, tables, bash / cpp blocks, backticks); this skill adds the wiki layout.
The reference is the wiki of libutils (https://github.com/TsukiNi22/libutils/wiki): read a page of it when unsure.
For a libutils based project use the `libutils` skill for the exact names, never write an API from memory.

## 1. The wiki repository
- A wiki is **another git repository**: `https://github.com/<owner>/<repo>.wiki.git` (clone it next to the project
  or in a temporary folder, never inside the project). One flat folder of `.md` files, no sub-folders.
- File = page: `Page-Name.md` (title with spaces -> `-`), `Exception-(usage).md` for "Exception (usage)",
  `Workflow-(CI‐CD).md` for "Workflow (CI/CD)" (GitHub writes `/` as the hyphen `‐` U+2010).
- Special pages: `Home.md` (landing page), `_Sidebar.md`, `_Footer.md`. The wiki must be enabled and have a first
  page created from the GitHub UI before the `.wiki.git` repository exists.
- **No `# Title` at the top**: GitHub shows the file name as the title. A page starts with a callout (section pages)
  or with `## Table of Contents` (guide pages).
- Links: `[Text](Page-Name)` (no `.md`, no path), `[Text](Page-Name#anchor)`, same page `[Text](#anchor)`; the
  anchor is the heading in lower case, punctuation removed, spaces -> `-` (`## Throw & catch` -> `#throw--catch`).
  Links to the code: full URL (`https://github.com/<owner>/<repo>/blob/main/...`).
- Commit in the wiki repository with `git-conventions` (`docs(wiki): ...`); pushing publishes the wiki: only when asked.

## 2. Pages
| Page | Role | Template |
|---|---|---|
| `Home` | landing: version stamp, what it is, doc link, table of the pages, dependencies, badges | `templates/Home.md` |
| `_Sidebar` | navigation on every page | `templates/_Sidebar.md` |
| `_Footer` | one line of links + the license | `templates/_Footer.md` |
| `Installing`, `Usage` | guides (packages, quick setups, CMake / include), the sub-pages of `Usage` are `Xxx-(usage)` | `templates/Guide.md` |
| `Sections` | overview of every section: define, namespace, content, then the conventions common to all | `templates/Sections.md` |
| `Tools-Preview` | every class / tool of the project at a glance, by section, each one linking to its page | `templates/Tools-Preview.md` |
| one per section / class | the reference of that section | `templates/Section.md` |
| `Workflow-(CI‐CD)` | workflows, pre-release, release, publication, notifications (`ci-cd` skill for the content) | `templates/Workflow.md` |

### Home
1. `> [!WARNING]` **version stamp**: `This documentation is only up to date as of \`DD/MM/YYYY\`, using files from version \`vX.Y.Z\` [hash](commit url).`
   Always updated when the wiki is (version from the project, `git rev-parse --short HEAD` of the project).
2. `> [!NOTE]` what the project is (language, build, distribution); `> [!TIP]` the HTML documentation if there is one.
3. `## Wiki Pages`: table `| Page | Content |` of the top-level pages (Installing, Usage, Tools Preview, Sections, Workflow).
4. `## Dependencies`: `> [!CAUTION]` license disclaimer + table `| Name + Link | License | Status | Last Update |`.
5. `---` then the shields badges (License, Status, Language).

### _Sidebar
Three groups as `# ` headings: `# Getting started` (Installing, Usage + numbered sub-pages), `# Sections/Tools`
(`### [Overview](Sections)`, then `### Group` headings with one bullet per page, sub-pages as a numbered list
under their parent), `# Miscellaneous` (Workflow). A top-level page is `- ### [Page](Page)` in Getting started and
`- [Page](Page)` in the sections; **every page of the wiki is in the sidebar**. Ends with the license badge.

### _Footer
`[Home](Home) · [Installing](Installing) · ...` (the top-level pages), a blank line, the license sentence with a link
to the `LICENSE` file (and the other licenses of the repository).

### Section page (the main kind)
````markdown
> [!NOTE]
> Section `_Name` (group `_Group`) · Namespace `ns::name` · Headers `path/to/headers/`

One or two sentences: what it does, the main idea, the limits.

## Table of Contents
 - [Part](#part)
 - [Example](#example)

## Part
| Name | Header | Description |     <- classes
| Method | Description |            <- methods / functions
| Flag | Effect |  |  | Macro | Usage |  |  | Code | Message |
...
## Example
```cpp
#define _Name
#include <utils/utils.hpp>
...
```
## Errors
````
- Opening callout: always `Section \`_X\` (group|from|in \`_Y\`) · Namespace \`...\` · Header(s) \`...\``; a sub-section
  says `from` / `in` its parent (`Section \`_Vector\` (from \`_CustomType\`)`).
- A table for every list of attributes, one row per item, **code in backticks** (types, methods with their
  arguments `spawn(std::size_t n, args...)`, flags, macros, error codes), description in a short sentence without
  final period; `->` for results (`Vector2<int> + Vector2<double>` -> `Vector2<double>`).
- Callouts inside a page: `> [!NOTE]` detail, `> [!IMPORTANT]` required behaviour (ex: `FatalException` aborts),
  `> [!WARNING]` pitfall, `> [!TIP]` shortcut; one idea each.
- `## Example`: a short complete block (the `#define` of the section, the include, a few commented lines); comments
  aligned on the right of the code. Real code, tested or copied from the project tests, never invented.
- `## Errors` (last): table `| Code | Raised when |` of the exceptions of the section, or a link to the codes page.
- The Table of Contents lists the `##` of the page in order; a page with a single `##` can skip it.

### Guide pages
`## Table of Contents` first, then `## ` sections: `Packages` (`> [!NOTE]` about the `-pre` packages + `| File Name | Content |`),
`Quick Setup - 1 (all)` (`> Setup into \`/usr/local\``, clone + build blocks), `Quick Setup - 2 (Limited)`
(`> Setup into \`/usr\``, `> [!WARNING]` restrictions, numbered steps, `wget -qO-` then `or with \`curl\`:`), `Manual Setup`.
`Usage`: `## CMake` (`find_package` + table build type -> library -> package), `## Include`, `## Pages` (table of the sub-pages).

### Overview pages
`Sections`: `## Table of Contents`, one `## Group` per group with `> \`_Group\`` then the table
`| Section | Define | Namespace | Content |`, then `## Errors & conventions` (bullets valid for all sections).
`Tools-Preview`: one `## Section` per section (alphabetical), `| Name | Description |` with each item linking to
`Page#anchor`.

## 3. Workflow
1. Inspect: the project (version, sections / modules, headers, tests, CI) and the existing wiki (clone it, `ls`, read
   `_Sidebar.md` and `Home.md`). Decide the pages to create / update; ask the user only when the scope is unclear.
2. Write each page from its template (`templates/`, placeholders `{{NAME}}`), reading the headers / sources to fill the
   tables; keep the order of the sidebar and of the overview pages consistent with the code layout.
3. Update `Home` (version stamp, table), `_Sidebar`, `_Footer`, `Sections`, `Tools-Preview` for every page added / removed.
4. Check: `python3 SKILL_DIR/scripts/check_wiki.py <wiki-folder>` (links, anchors, sidebar coverage, code fences,
   tables, emoji, callouts, H1; `--strict` makes the warnings fail). Fix everything it reports.
5. Report the pages created / changed; commit in the wiki repository only when asked (`docs(wiki): ...`), push only
   when asked.
- A rename of a page = the file, the sidebar, the footer and every link to it (the checker finds the broken ones).
