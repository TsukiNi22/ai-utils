# Installation

Every way to install the skills of [ai-utils](README.md): `setup.sh` (from a clone or without one), the Claude Code
plugin marketplace and the claude.ai chat.

### Table of Contents
 - [setup.sh](#setupsh)
 - [Selections](#selections)
 - [Every command](#every-command)
 - [Tools build](#tools-build)
 - [Moved skills](#moved-skills)
 - [Plugin marketplace (Claude Code)](#plugin-marketplace-claude-code)
 - [claude.ai](#claudeai)

## setup.sh
From a clone, `./setup.sh <command>`. Without a clone, the same commands after `bash -s --`: the script clones (or
updates, fast-forward only) the repository into `~/.local/share/tsukini-skills` (`SKILLS_HOME` to change it) and runs
from there.

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install cpp-class  # only one skill (+ its requirements)
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- update             # pull the last version
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- remove --purge     # remove everything and the clone
```

By default the skills are **symlinked** into `~/.claude/skills`: an update (or a local edit) is used right away, no
need to reinstall. `remove` only deletes what comes from this repository (`--force` otherwise). A skill can require
other ones (`skills/<category>/<skill>/requires.txt`): `install` adds them.

## Selections
`install` and `remove` take any mix of these names (none = everything):

| Name | Selects |
|---|---|
| `cpp-class` | one skill (and, for `install`, the skills it requires) |
| `cpp/`, `@cpp` | every skill of a category (`skills/cpp/`) |
| `skills` | every skill, nothing else |
| `tools`, `tools/` | every tool (`claude-cluster`) |
| `claude-cluster` | one tool |
| `context` | the global context (`CLAUDE.md`, `RTK.md`, hooks, `sudo-askpass`) |

## Every command

```bash
./setup.sh list                           # available categories, skills, tools and context
./setup.sh status                         # what is installed (skills, tools, context)
./setup.sh install                        # every skill, every tool and the context
./setup.sh install skills context         # kinds and names mix: every skill + the context
./setup.sh install --no-context           # everything but the context
./setup.sh install --project ~/my/project # in <project>/.claude/skills instead (never the context: it is global)
./setup.sh install --copy                 # copy instead of symlink
./setup.sh install --force                # replace a skill of the same name that does not come from this repository
./setup.sh install context --no-rtk       # the context without rtk (--no-hooks: settings.json untouched)
./setup.sh install claude-cluster --prefix /usr/local
./setup.sh update                         # git pull, repair the links of the moved skills, rebuild the installed tools
./setup.sh doctor                         # what is broken in the installed skills (exit 1 when something is)
./setup.sh doctor --fix                   # repair it: relink, copy again, remove, install the missing requirements
./setup.sh remove cpp-class               # remove only one (`remove context` restores the replaced files)
./setup.sh remove tools                   # a tool: its binary and completion only
./setup.sh remove --purge                 # also the build trees / logs of the tools and (curl / wget mode) the clone
./setup.sh context status                 # only the context, with its own commands (context/README.md)
./setup.sh hook                           # xstyle pre-commit hook in the git repository of the current folder
./setup.sh hook --fail-on minor --shared  # its options: skills/git/pre-commit/scripts/install_hook.sh --help
```

`doctor` checks the target (`~/.claude/skills`, or `--project <dir>`): links to a skill that moved in the repository
or left it, copies (`--copy`) of a skill removed or changed since, and skills required by an installed one but
missing (installed the same way: link or copy).

## Tools build
A tool (`tools/<tool>/tool.txt`) is built with CMake (clang++, libutils) and installed in `~/.local/bin`
(`--prefix <dir>`), with its bash / zsh / fish completion. A tool that can't be built is skipped with the reason,
never fatal.

The progress shows the steps (configure, build, install, completion), the percentage, a bar, the current file, the
elapsed time and an ETA parsed from the build output, which goes to `~/.cache/tsukini-skills/build/<tool>.log`. On a
terminal: one updating line (`NO_COLOR` honoured); in a pipe or CI: a line per 10% and per step. A failed build
shows its step and the last 20 lines of the log.

| Variable / option | Effect |
|---|---|
| `--verbose` | the raw build output instead of the progress |
| `--quiet` | one line per tool (and the failures) |
| `SETUP_PROGRESS=plain\|bar\|off` | force the display (default: `bar` on a terminal, `plain` otherwise) |
| `SETUP_JOBS=N` | parallel jobs of the builds (default: the processors, fewer under ~1.5 GB free per job) |

## Moved skills
> [!IMPORTANT]
> The skills moved from `<category>/` to `skills/<category>/` and claude-cluster from `ai/` to `tools/`. An install
> made before has links to the old places: `./setup.sh update` repairs them. A clone already pulled by an older
> `setup.sh`: run `./setup.sh doctor --fix` once.

## Plugin marketplace (Claude Code)
The repository is also a Claude Code plugin marketplace ([`.claude-plugin/marketplace.json`](.claude-plugin/marketplace.json)),
handy on another machine or in a Claude Code on the web session:

```text
/plugin marketplace add TsukiNi22/ai-utils
/plugin install cpp-skills@tsukini-skills
/plugin install dev@tsukini-skills
```

<!-- generated:plugins -->
| Plugin | Skills |
|---|---|
| `cpp-skills` | cmake-style, cpp-class, cpp-comments, cpp-project, cpp-style |
| `python-skills` | python-class, python-comments, python-project, python-style |
| `libutils-skills` | libutils, libutils-exception, libutils-install, libutils-setup |
| `test-skills` | coverage, cpp-tests, tests |
| `style-skills` | coding-style, comments |
| `doc-skills` | better-explain-doc, better-html-doc, better-html-style, better-protocol-doc, docs-update, explain-doc, html-doc, html-style, protocol-doc, readme-style, report, wiki-style |
| `git-skills` | ci-cd, git-commit, git-conventions, pre-commit |
| `audit-skills` | audit-bugs, audit-deps, audit-quality, audit-security, benchmark |
| `legal-skills` | legal-check, legal-setup, license |
| `ux-skills` | color-modes-project, color-modes-website, multilingual-project, multilingual-website |
| `system-skills` | computer-clean |
| `ai-skills` | ask-at-end, clarify, continue-work, export, full-permission, ideation, interactive-questions, onboard, pause, presentation, rtk, setup-context, write-like-me |
| `dev`, `audit`, `cpp`, `doc`, `git`, `legal`, `python`, `setup-project`, `style` | the routers (`/dev`, `/audit`...) |
<!-- /generated:plugins -->

The skills of a plugin are named `<plugin>:<skill>` (ex: `cpp-skills:cpp-class`); each plugin carries the whole
repository, so a skill finds the scripts of the others. In a cloud session the marketplace has to be added again in
each session; `/plugin marketplace update tsukini-skills` gets the new versions. The context is not a plugin (a plugin
can't write `~/.claude/CLAUDE.md`).

## claude.ai
The same skills in the claude.ai chat (web, desktop, mobile): `python3 claude-ai/build.py` builds one archive per
skill in `dist/claude-ai/` to upload in **Customize > Skills**, and [`claude-ai/preferences.md`](claude-ai/preferences.md)
is the `CLAUDE.md` adapted to the chat, to paste in the personal preferences. Details:
[`claude-ai/README.md`](claude-ai/README.md).
