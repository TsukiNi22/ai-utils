# ai-utils - context

Global / default context of Claude Code (not skills): what every session loads on my computers.

> [!TIP]
> The skills, routers and tools of the repository are in the other folders, see the [root README](../README.md).

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
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- context install
```

or with `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- context install
```

The repository is cloned (or updated) into `~/.local/share/tsukini-skills` (`SKILLS_HOME` to change it).

> [!TIP]
> The context is also part of the global installer: `./setup.sh install` (everything) or `./setup.sh install context`
> (only this), `--no-context` to leave it out, see the [root README](../README.md#context).

### Quick Setup - 2 (from a clone)
```bash
git clone git@github.com:TsukiNi22/ai-utils.git
cd ai-utils
./setup.sh context install
```

### Commands
```bash
./setup.sh context status                 # what is installed (files, hooks, rtk)
./setup.sh context install                # symlinks + hooks merge + rtk if missing
./setup.sh context install --copy         # copies instead of symlinks
./setup.sh context install --no-rtk       # don't install rtk
./setup.sh context install --no-hooks     # don't touch ~/.claude/settings.json
./setup.sh context update                 # git pull (the symlinks follow)
./setup.sh context remove                 # remove, restore the replaced files, unmerge the hooks
```

> [!NOTE]
> The files replaced by `install` are saved in `~/.claude/backups/context/` and restored by `remove`.
> The hooks are merged in `settings.json` without touching the other settings or hooks.

> [!WARNING]
> `rtk` is installed with its official installer (`rtk-ai/rtk`, into `~/.local/bin`). Without it the
> `PreToolUse` hook fails: use `--no-hooks` if you don't want rtk.
