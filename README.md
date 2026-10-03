# skills

AI skills based on my work.

This repository holds [Claude Code skills](https://docs.claude.com/en/docs/claude-code/skills)
built from my own projects and habits. Each skill teaches the assistant how I work (code style,
file layout, tooling), so that what it generates looks like something I wrote myself.

## Skills

| Skill | Description |
|---|---|
| [`cpp-class`](cpp-class/SKILL.md) | Sets up a C++20 architecture / new `.hpp` & `.cpp` files (interface `I*`, abstract `A*`, class, template, `Type`/`Define`, family headers) with the namespaces, the Xartania header and **empty bodies only** (never the logic), picks libutils attributes or standard `[[...]]` ones and registers the sources in the `CMakeLists.txt`. Requires `cpp-style` & `cpp-comments`. |
| [`cpp-style`](cpp-style/SKILL.md) | My C++ coding style: naming, indentation, braces, spacing, loops, switch, lambdas, const correctness, attributes. |
| [`cpp-comments`](cpp-comments/SKILL.md) | How I comment C++ code: section separators, `/* group */` labels, aligned trailing comments, no Doxygen. |
| [`git-conventions`](git-conventions/SKILL.md) | Commit messages `type(scope): message`, CI keywords, tags, GitHub releases, CHANGELOG, branches & PRs (to be defined), no AI attribution. |
| [`readme-style`](readme-style/SKILL.md) | README / Markdown docs structure: Table of Contents, Dependencies, Packages, Quick Setup, Usage, GitHub callouts, tables. |
| [`html-doc`](html-doc/SKILL.md) | One uniform style for every HTML documentation page (summary, numbered cards, light/dark theme with a sun/moon button, SVG diagrams). |
| [`cmake-style`](cmake-style/SKILL.md) | `CMakeLists.txt` in my style: section order, explicit sources, Debug/Asan/Optimized modes, release targets, GTest tests, install + `find_package` config, CPack RPM/DEB stable/pre channels, with app/lib/tests templates. |
| [`libutils`](libutils/SKILL.md) | Reference of my library [libutils](https://github.com/TsukiNi22/libutils): every section, header and public API generated from a recorded version/commit (`reference/VERSION.md`), integration and conventions, plus `scripts/update.sh` to see what changed since that commit and regenerate. |

Everything is based on my own work, mainly [libutils](https://github.com/TsukiNi22/libutils).

## Installation

```bash
git clone git@github.com:TsukiNi22/skills.git
cd skills
./setup.sh install            # every skill, in ~/.claude/skills (symlinks)
```

```bash
./setup.sh list                           # available skills
./setup.sh status                         # what is installed
./setup.sh install cpp-class              # only one skill
./setup.sh install --project ~/my/project # in <project>/.claude/skills instead
./setup.sh install --copy                 # copy instead of symlink
./setup.sh remove                         # remove every skill of this repo
./setup.sh remove cpp-class               # remove only one
```

By default the skills are **symlinked**: a `git pull` (or a local edit) is used right away,
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
