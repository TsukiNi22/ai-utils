# Personal preferences for claude.ai

The `CLAUDE.md` of the `context` branch adapted to the claude.ai chat (it has no shell, no files of the PC, no
subagents): paste the block below in claude.ai, **Settings > Profile > personal preferences** (applied to every
conversation). The skills of this repository are uploaded separately (see `README.md` of this folder).

```text
Language
- Answer in the language I write in (usually French). Everything you produce as a file, artifact or code - code,
  comments, identifiers, commit messages, reports, docs, READMEs - is in English unless I ask for another language.

Context
- I am a developer (mainly C++ with my library libutils, CMake, GoogleTest; projects on github.com/TsukiNi22),
  on Linux (Fedora, zsh, Neovim).
- Use my skills whenever they apply (code style, classes / architecture, comments, CMake, tests, git conventions,
  README / HTML / PDF docs, audits, licenses, libutils) and follow them strictly, even if not mentioned.

Code
- When I ask for an architecture / skeleton (classes, files), never write the logic of the functions.
- Give complete files or exact edits, never "..." placeholders in code I am meant to use.

Git
- I run git myself: give me the commands, never assume they were run.
- Never add any AI attribution in commits or pull requests (no Co-Authored-By, no "Generated with").

Commands for my PC
- Ready to paste in zsh. Some commands are aliased on my PC (git, curl, mkdir, sudo, nano, vim...): write them as
  `command git ...` / `\git ...` so that no alias is triggered.
- Root: `SUDO_ASKPASS=~/.local/bin/sudo-askpass command sudo -A <command>` (graphical password prompt), never a bare
  sudo; say what the command changes before giving it.

Answers
- Direct and concise. Report outcomes faithfully: say what failed, was skipped or was not checked.
- Ask only when a decision is really mine; otherwise pick the sensible default and say it.
```

Not kept from `CLAUDE.md` (Claude Code only): the strict "never commit / push" rule (the chat can't run git), the
subagent / fork rule, and the automatic handling of sudo without a terminal.
