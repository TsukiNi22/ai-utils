# ai-utils repository

Skills of the user for Claude Code (`skills/<category>/<skill>/SKILL.md`), the routers (`skills/routers/`), the tools
built by `setup.sh` (`tools/<tool>/tool.txt`, ex: `tools/claude-cluster`) and the claude.ai packaging (`claude-ai/`).
The global context (`~/.claude/CLAUDE.md`, `RTK.md`, hooks) lives in `context/` (`./setup.sh context ...`).
Reports of audits on this repository go in `audit/` at the root; repository scripts (README tables, checks) in `scripts/`.

## Checking with xstyle
- Check / fix the style with `xstyle`, **always with `--rtk`** (compact output made for the assistant, ~5x fewer
  tokens): `xstyle --rtk <paths>`, `xstyle --rtk --fix [CODES] <paths>`, `xstyle --rtk --diff` (changed lines only).
  The human output (colors, tables, source lines) is for the user's terminal only.
- xstyle itself lives in its own repository ([TsukiNi22/xstyle](https://github.com/TsukiNi22/xstyle), `~/personal_delivery/other/xstyle`).
- Local build of the tools (`tools/claude-cluster`: `cmake -S tools/claude-cluster -B /tmp/cc-build`, then
  `xstyle --rtk -r tools/claude-cluster`); test claude-cluster isolated with `CLAUDE_CLUSTER_CONFIG_DIR`,
  `CLAUDE_CLUSTER_DATA_DIR` and `CLAUDE_CLUSTER_SOCKET` (`claude-cluster serve` + the headless commands).

## Writing skills
- English, the user's style skills for the code of the scripts (`python-style`, `coding-style` for shell, `cpp-style`).
- A new skill: `skills/<category>/<skill>/SKILL.md` with `name` / `description` frontmatter and its `requires.txt`,
  the routers that should load it, the marketplace (`.claude-plugin/marketplace.json`, one plugin per category folder),
  then `python3 scripts/readme.py` (writes the tables of `skills/README.md` and `skills/<category>/README.md` from the
  frontmatter, never edited by hand between their markers) and `python3 scripts/check.py` (the CI runs it too).
- Scripts tested for real before committing (generated files, outputs), placeholders `{{NAME}}` in the templates.

## Versions
- The tools (claude-cluster...) follow major.minor.patch by the size of the change: a real feature (a new mode, view,
  subsystem: the supervisor) -> **minor** (1.5.0); fixes, look changes, a few settings -> **patch** (1.5.1); a breaking change (config /
  data format, a command removed) -> **major**. Several features in one release: one minor bump.

## Git
- Commits allowed in this repository (the user granted it), format of `git-conventions` (`type(scope): message`),
  ending with the `Co-Authored-By` trailer of the current model; never force-push or rewrite pushed history without
  asking.
