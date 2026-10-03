# skills

AI skills based on my work.

This repository holds [Claude Code skills](https://docs.claude.com/en/docs/claude-code/skills)
built from my own projects and habits. Each skill teaches the assistant how I work (code style,
file layout, tooling), so that what it generates looks like something I wrote myself.

## Skills

| Skill | Description |
|---|---|
| [`cpp-class`](cpp-class/SKILL.md) | Sets up a C++20 architecture / new `.hpp` & `.cpp` files (interface `I*`, abstract `A*`, class, template, `Type`/`Define`, family headers, `main.cpp` entry point) with the namespaces, the Xartania header and **empty bodies only** (never the logic), picks libutils attributes or standard `[[...]]` ones and registers the sources in the `CMakeLists.txt`. Requires `cpp-style` & `cpp-comments`. |
| [`cpp-style`](cpp-style/SKILL.md) | My C++ coding style: naming, indentation, braces, spacing, loops, switch, lambdas, const correctness, attributes. |
| [`cpp-comments`](cpp-comments/SKILL.md) | How I comment C++ code: section separators, `/* group */` labels, aligned trailing comments, no Doxygen. |
| [`git-conventions`](git-conventions/SKILL.md) | Commit messages `type(scope): message`, CI keywords, tags, GitHub releases, CHANGELOG, branches (`main` alone when solo; `main`/`dev`/`sub/`/`feat/`/`fix/` in a team) and PRs (CHANGELOG-style body, `gh` assignee/labels), no AI attribution. |
| [`readme-style`](readme-style/SKILL.md) | README / Markdown docs structure: Table of Contents, Dependencies, Packages, Quick Setup, Usage, GitHub callouts, tables. |
| [`html-doc`](html-doc/SKILL.md) | One uniform style for the HTML documentation: an optional user guide, the technical documentation and an interactive 2D/3D project graph (filters by category/group/relation, rebuilt from the GitHub repository with an update button), all static self-contained pages with a sun/moon theme button. |
| [`cpp-project`](cpp-project/SKILL.md) | Sets up a new C++ project from [cpp_project_template](https://github.com/TsukiNi22/cpp_project_template): renaming, binary / packaged binary / `.a` / `.so` / header-only, libutils or not, exceptions, GTest, CI/CD workflows (build check or full packages + gh-pages mirror), install script, README/CHANGELOG/docs, using the other skills. |
| [`cmake-style`](cmake-style/SKILL.md) | `CMakeLists.txt` in my style: section order, explicit sources, Debug/Asan/Optimized modes, release targets, GTest tests, install + `find_package` config, CPack RPM/DEB stable/pre channels, with app/lib/tests templates. |
| [`libutils`](libutils/SKILL.md) | Reference of my library [libutils](https://github.com/TsukiNi22/libutils): every section, header and public API generated from a recorded version/commit (`reference/VERSION.md`), integration and conventions, plus `scripts/update.sh` to see what changed since that commit and regenerate. |
| [`libutils-exception`](libutils-exception/SKILL.md) | libutils exceptions: classes, codes, throw/catch patterns, and how to add new exception codes to a project (JSON, generator script, CMake block). Requires `libutils`. |

Everything is based on my own work, mainly [libutils](https://github.com/TsukiNi22/libutils).

## Installation

### Quick Setup - 1 (without cloning)
Run the setup script directly: it clones (or updates) the repository into `~/.local/share/tsukini-skills`
(`SKILLS_HOME` to change it) and links the skills from there, nothing to clone or clean by hand.

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- install
```

or with `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- install
```

Every command works the same way, arguments are given after `bash -s --`:

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- install cpp-class  # only one skill (+ its requirements)
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- update             # pull the last version
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- remove --purge     # remove every skill and the clone
```

> [!NOTE]
> Requires `git` and `bash`. Each run updates the managed clone first (fast-forward only).

### Quick Setup - 2 (from a clone)

```bash
git clone git@github.com:TsukiNi22/skills.git
cd skills
./setup.sh install            # every skill, in ~/.claude/skills (symlinks)
```

### Commands

```bash
./setup.sh list                           # available skills
./setup.sh status                         # what is installed
./setup.sh install cpp-class              # only one skill (and the skills it requires)
./setup.sh install --project ~/my/project # in <project>/.claude/skills instead
./setup.sh install --copy                 # copy instead of symlink
./setup.sh update                         # git pull of the repository
./setup.sh remove                         # remove every skill of this repo
./setup.sh remove cpp-class               # remove only one
./setup.sh remove --purge                 # (curl/wget mode) also delete the managed clone
```

By default the skills are **symlinked**: an update (or a local edit) is used right away,
no need to reinstall. `remove` only deletes skills that come from this repository
(use `--force` otherwise).

## Usage

Once installed, the skill is used automatically when relevant
(ex: *"create a `Timer` class in `utils::system`"*, *"write the commit message"*),
or explicitly with `/<skill>` (`/cpp-class`, `/git-conventions`...).

A skill can require other ones (`<skill>/requires.txt`): `./setup.sh install <skill>` installs them too.

## Layout

```
<skill>/
├── SKILL.md       # instructions loaded by the assistant
├── requires.txt   # other skills needed (optional)
├── reference/     # detailed rules, read on demand
├── templates/     # file templates
├── examples/      # real files used as ground truth
└── scripts/       # helpers called by the skill
```

## Credits

`cpp-class/scripts/fonts/ansi_shadow.flf`: ANSI Shadow FIGlet font, as distributed with
[pyfiglet](https://github.com/pwaller/pyfiglet) (MIT).
