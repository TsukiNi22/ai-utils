---
name: docs-update
description: Bring every piece of documentation and distribution text of a project up to date with the changes made (and the latest version) - README, CHANGELOG, docs/ HTML site and project graph, gh-pages branch, GitHub wiki, packages / manifests / version numbers, setup / install scripts, --help and usage texts, completions, man pages, examples, templates, marketplace / plugin lists, CI docs, license year - by finding every place the changes touch, not only the obvious ones. Use whenever the user asks to update / refresh / synchronize the docs, "mets à jour toute la doc / le README / le changelog / le site / le wiki / les packages / le script d'install", after a feature, a fix, a release or a version bump.
---

# Update all the documentation

Goal: after changes, **nothing that describes the project is stale** - docs, site, wiki, packages, install scripts,
version strings. Think of every place the project is described, not only the README.

## 1. Baseline: what changed
Find what the docs must catch up with, in this order:
1. What the user says changed (the request, the conversation).
2. The pending diff (`\git status`, `\git diff HEAD`) and the commits since the **last docs update** or the last
   release: `\git describe --tags --abbrev=0`, `\git log <tag>..HEAD --oneline`,
   `\git log -1 --format=%H -- README.md CHANGELOG.md docs`.
3. The **latest version**: the source of truth is the code (`project(... VERSION x.y.z)`, `pyproject.toml`,
   `package.json`, `Cargo.toml`, `reference/VERSION.md`, the last tag, a `version` constant). Take the newest one
   found, and report any disagreement between them. For a dependency documented from its upstream (ex: libutils),
   take its latest release (`libutils` skill: `scripts/update.sh`).

Run `scripts/inventory.sh [repo]` for the first pass: it lists the documentation surfaces that exist, the
versions found, dead links in the Markdown and what is not committed yet.

## 2. Inventory every surface that exists
Check each one; skip what the project has not. Do not stop at the first hit.

| Surface | What to bring up to date | Skill |
|---|---|---|
| `README.md` (+ sub-READMEs of tools / modules) | features, tables, command lists, install, usage, layout tree, badges, versions, links | `readme-style` |
| `CHANGELOG.md` | new entry for the changes / version (its own commit) | `git-conventions` |
| Release notes, tag text | the next release body (not published without a request) | `git-conventions` |
| `docs/` HTML (user guide, technical doc, project graph) | pages, summaries, numbered cards, graph data regenerated, EN/FR texts, theme | `html-doc` (+ `html-style`) |
| `gh-pages` branch / site / packages repository | the published copy of docs, graph, repo metadata, index of packages (worktree or clone, see below) | `html-doc`, `cpp-project` |
| GitHub wiki (`<repo>.wiki.git`) | Home version stamp, sidebar, section pages, member tables | `wiki-style` |
| Packages and manifests | version in `CMakeLists.txt`, CPack / spec / control, `pyproject.toml`, `package.json`, `Cargo.toml`, plugin / marketplace JSON (names, descriptions, skill lists), Dockerfile labels | `cmake-style`, `python-project` |
| Setup / install / update scripts | usage header, command and option lists, list of installable items, URLs, default versions, uninstall paths | `coding-style` |
| `--help` / usage text, man pages, shell completions | every option and command that exists now | the style skill of the language |
| Examples, templates, `reference/` files, skill and router tables | outdated names, removed options, new items | the skill's own rules |
| CI / CD docs and workflow comments | job names, keywords, images, versions | `ci-cd` |
| `LICENSE` / notices | year, holder, third-party notices after a dependency change | `license`, `audit-deps` |
| Doxygen / docstrings / code comments describing changed behavior | the stale ones only | `comments` |
| claude.ai bundles, generated archives, exported files | rebuilt from the sources (never edited by hand) | the project's build script |

## 3. Update
- For each change, ask "where else is this written?": `grep -rn` the old name, option, path, version and count
  (ex: "9 routers", a list of skills) over the whole repository, the wiki and `gh-pages`.
- **Check every name / command / option / API against the code** before writing it; mark planned vs implemented.
- Same list written in several places (README table, layout tree, plugin table, router tables, marketplace JSON,
  setup script header, claude.ai README): change them all in one pass so they stay identical.
- Generated files are regenerated with their generator (graph, archives, reference from a commit), not edited.
- Follow the style skill of each surface; texts in English unless the project's docs are not.
- `gh-pages` and the wiki are other branches / repositories: use `git worktree add` or a temporary clone, edit
  there, then remove the worktree / clone and come back on the original branch; leave the result **uncommitted**
  unless the user asked for commits (then `git-commit` for each repository).
- No version bump, tag, release or publication on your own: if the version in the docs must move, say which
  files and to what, and do it only when asked.

## 4. Verify
- Re-run `scripts/inventory.sh`: no dead link, versions identical everywhere.
- Anchors and tables (`wiki-style` has `check_wiki.py`; `html-style` has `check_style.py`, which also checks the
  responsive rules of the HTML).
- The commands written in the docs actually run (`--help`, install in a scratch prefix when cheap).
- Scripts / builds that read the docs' lists still work (ex: rebuild the archives, `./setup.sh list`).

## 5. Report
List by surface what was changed (path, one line), what was checked and found already current, what could not be
done (private wiki, no network, version decision) and the commit command to suggest (`git-commit`). A written
summary of a wide pass goes through `report` only when the user wants a file.
