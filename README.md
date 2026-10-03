# skills

AI skills based on my work.

This repository holds [Claude Code skills](https://docs.claude.com/en/docs/claude-code/skills)
built from my own projects and habits. Each skill teaches the assistant how I work (code style,
file layout, tooling), so that what it generates looks like something I wrote myself.

## Skills

| Skill | Description |
|---|---|
| [`cpp-class`](cpp-class/SKILL.md) | Generates C++20 `.hpp`/`.cpp` files and class architectures (interface `I*`, abstract `A*`, class, template, `Type`/`Define` headers) in my style, based on [libutils](https://github.com/TsukiNi22/libutils) and my nvim config. Adds the Xartania header, picks libutils attributes or standard `[[...]]` ones, and registers the new sources in the `CMakeLists.txt`. |

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
(ex: *"create a `Timer` class in `utils::system`"*), or explicitly with `/cpp-class`.

## Layout

```
<skill>/
├── SKILL.md       # instructions loaded by the assistant
├── reference/     # detailed rules, read on demand
├── templates/     # file templates
├── examples/      # real files used as ground truth
└── scripts/       # helpers called by the skill
```

## Credits

`cpp-class/scripts/fonts/ansi_shadow.flf`: ANSI Shadow FIGlet font, as distributed with
[pyfiglet](https://github.com/pwaller/pyfiglet) (MIT).
