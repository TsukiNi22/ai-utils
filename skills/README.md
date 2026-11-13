# Skills

Every skill is `skills/<category>/<skill>/` and is installed **flat** as `~/.claude/skills/<skill>`: the category only
sorts the repository (and makes the plugins of the marketplace). The routers are in [`routers/`](routers/README.md).

## Categories
<!-- generated:categories -->
| Category | What it covers | Skills |
|---|---|---|
| [C++](cpp/README.md) | Project setup, classes / architecture, style, comments, CMake | `cmake-style`, `cpp-class`, `cpp-comments`, `cpp-project`, `cpp-style` |
| [Python](python/README.md) | Project setup, files / classes scaffolding, style and comments | `python-class`, `python-comments`, `python-project`, `python-style` |
| [libutils](libutils/README.md) | API reference, exceptions, install / update, setup in a project | `libutils`, `libutils-exception`, `libutils-install`, `libutils-setup` |
| [Tests](tests/README.md) | GoogleTest (C++), the other frameworks, coverage | `coverage`, `cpp-tests`, `tests` |
| [Style](style/README.md) | Code style and comments for the other languages | `coding-style`, `comments` |
| [Documentation](docs/README.md) | README, GitHub wiki, uniform HTML docs with the project graph, HTML style, explanation pages (simple + technical), protocol / wire format docs, Markdown / PDF reports, update of every doc / site / package / install script after changes, documentation sites with search in the claude-cluster style | `better-explain-doc`, `better-html-doc`, `better-html-style`, `better-protocol-doc`, `docs-update`, `explain-doc`, `html-doc`, `html-style`, `protocol-doc`, `readme-style`, `report`, `wiki-style` |
| [Git](git/README.md) | Git conventions and local commit of the pending diff (commits, branches, pull requests, changelog), the pre-commit hooks and the CI/CD workflows | `ci-cd`, `git-commit`, `git-conventions`, `pre-commit` |
| [Audit](audit/README.md) | Bugs / UB, security (passes fixing every flaw until a clean one), quality, dependencies (licenses, vulnerabilities), benchmark / profiling | `audit-bugs`, `audit-deps`, `audit-quality`, `audit-security`, `benchmark` |
| [Legal](legal/README.md) | Its license, and the compliance check and setup - GDPR / RGPD, cookies, legal notice, terms, consumer law, accessibility (RGAA, EAA), language, licenses and credits, mandatory mentions, DSA, AI Act, CRA, NIS2, Data Act - global or web | `legal-check`, `legal-setup`, `license` |
| [UX](ux/README.md) | Several languages (multilingual-project, multilingual-website) and visual modes light / dark / system / high contrast (color-modes-project, color-modes-website) | `color-modes-project`, `color-modes-website`, `multilingual-project`, `multilingual-website` |
| [System](system/README.md) | Free space on the computer and in the data of Claude Code / claude-cluster, chosen in a QCM | `computer-clean` |
| [AI workflow](ai/README.md) | Token-minimal outputs and --rtk modes, questions through the QCM / Q&A popup, clarification, ideation, a run without interruption, session context setup, writing in the user's voice, handover file, project onboarding, presentations from a plan or an idea (PDF / HTML / PPTX), full git permission from the prompt | `ask-at-end`, `clarify`, `export`, `full-permission`, `ideation`, `interactive-questions`, `onboard`, `presentation`, `rtk`, `setup-context`, `write-like-me` |
<!-- /generated:categories -->

## Anatomy of a skill

```text
skills/<category>/<skill>/
├── SKILL.md       # instructions loaded by the assistant (frontmatter: name, description, metadata.summary)
├── requires.txt   # other skills it needs, installed with it
├── reference/     # detailed rules, read on demand
├── templates/     # file templates
├── examples/      # real files used as ground truth
└── scripts/       # helpers called by the skill
```

A skill names another one's folder as `<name>/...`: `~/.claude/skills/<name>` once installed by `setup.sh`,
`${CLAUDE_SKILL_DIR}/../../*/<name>` in a plugin of the marketplace, `../<name>` in a claude.ai archive.

## Adding a skill
1. `skills/<category>/<skill>/SKILL.md` with `name`, `description` (at most 1024 characters, what triggers it) and
   `metadata.summary` (the line of the README tables), then its `requires.txt`.
2. The routers that should load it (`routers/<router>/SKILL.md` and its `requires.txt`), the plugin of its category
   in [`.claude-plugin/marketplace.json`](../.claude-plugin/marketplace.json).
3. `python3 scripts/readme.py` (the tables of the READMEs), then `python3 scripts/check.py` (run by the CI too).

[Back to the README](../README.md) · [Installation](../INSTALL.md)
