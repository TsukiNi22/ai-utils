# skills - context

Global / default context of Claude Code (not skills): what every session loads on my computers.

> [!TIP]
> The skills are on the [`main`](https://github.com/TsukiNi22/skills) branch.

| File | Installed as | Content |
| ---- | ------------ | ------- |
| `claude/CLAUDE.md` | `~/.claude/CLAUDE.md` | Global instructions: git rules, root access without TTY (graphical sudo) |
| `claude/RTK.md` | `~/.claude/RTK.md` | How to read the command output condensed by [rtk](https://github.com/rtk-ai/rtk) (imported by `CLAUDE.md`) |
| `claude/hooks/session-aliases.sh` | `~/.claude/hooks/session-aliases.sh` | `SessionStart` hook: gives the zsh aliases to Claude (and the ones that lock the session) |
| `claude/settings.hooks.json` | merged in `~/.claude/settings.json` | Hooks: `SessionStart` aliases + `PreToolUse` `rtk hook claude` |
| `bin/sudo-askpass` | `~/.local/bin/sudo-askpass` | Graphical password prompt (zenity) for `sudo -A` when there is no terminal |

## Installation

### Quick Setup - 1 (without cloning)
```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/context/setup.sh | bash -s -- install
```

or with `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/TsukiNi22/skills/context/setup.sh | bash -s -- install
```

The branch is cloned (or updated) into `~/.local/share/tsukini-context` (`CONTEXT_HOME` to change it).
From the `main` branch, `./setup.sh context install` does the same.

### Quick Setup - 2 (from a clone)
```bash
git clone --branch context --single-branch git@github.com:TsukiNi22/skills.git skills-context
cd skills-context
./setup.sh install
```

### Commands
```bash
./setup.sh status                 # what is installed (files, hooks, rtk)
./setup.sh install                # symlinks + hooks merge + rtk if missing
./setup.sh install --copy         # copies instead of symlinks
./setup.sh install --no-rtk       # don't install rtk
./setup.sh install --no-hooks     # don't touch ~/.claude/settings.json
./setup.sh update                 # git pull (the symlinks follow)
./setup.sh remove                 # remove, restore the replaced files, unmerge the hooks
./setup.sh remove --purge         # (curl/wget mode) also delete the managed clone
```

> [!NOTE]
> The files replaced by `install` are saved in `~/.claude/backups/context/` and restored by `remove`.
> The hooks are merged in `settings.json` without touching the other settings or hooks.

> [!WARNING]
> `rtk` is installed with its official installer (`rtk-ai/rtk`, into `~/.local/bin`). Without it the
> `PreToolUse` hook fails: use `--no-hooks` if you don't want rtk.
