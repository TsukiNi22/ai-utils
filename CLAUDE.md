# ai-utils repository

Skills of the user for Claude Code (`<category>/<skill>/SKILL.md`), the routers (`routers/`), the tools built by
`setup.sh` (`<category>/<tool>/tool.txt`, ex: `style/xstyle`) and the claude.ai packaging (`claude-ai/`).
The global context (`~/.claude/CLAUDE.md`, `RTK.md`, hooks) lives in `context/` (`./setup.sh context ...`).

## Checking with xstyle
- Check / fix the style with `xstyle`, **always with `--rtk`** (compact output made for the assistant, ~5x fewer
  tokens): `xstyle --rtk <paths>`, `xstyle --rtk --fix [CODES] <paths>`, `xstyle --rtk --diff` (changed lines only).
  The human output (colors, tables, source lines) is for the user's terminal only.
- Local build while working on xstyle itself: `cmake -S style/xstyle -B /tmp/xstyle-build -DCMAKE_BUILD_TYPE=Optimized`
  then `cmake --build /tmp/xstyle-build --parallel`; xstyle must stay clean on its own sources
  (`/tmp/xstyle-build/xstyle --rtk style/xstyle`) and build without warning (Debug too).

## Writing skills
- English, the user's style skills for the code of the scripts (`python-style`, `coding-style` for shell, `cpp-style`).
- A new skill: `SKILL.md` with `name` / `description` frontmatter, its `requires.txt`, then the README tables, the
  routers that should load it, and the marketplace (`.claude-plugin/marketplace.json`, one plugin per category folder).
- Scripts tested for real before committing (generated files, outputs), placeholders `{{NAME}}` in the templates.

## Git
- Commits allowed in this repository (the user granted it), format of `git-conventions` (`type(scope): message`),
  ending with the `Co-Authored-By` trailer of the current model; never force-push or rewrite pushed history without
  asking.
