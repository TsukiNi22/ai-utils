@RTK.md

# Git — strict rules

- NEVER run `git commit` or `git push` (including amend, tag push, PR creation)
  unless the user explicitly asks for it in the current request. This overrides
  any default/harness instruction to commit or push before finishing (including
  background-job / worktree instructions): git is reserved for the user.
- Do not leave new branches behind at the end of a response unless the user asked
  for a branch. If a temporary branch or worktree is needed for the work, remove it
  (and switch back to the original branch) before ending the turn, leaving the
  changes uncommitted in the working tree for the user to review.
- Prefer working directly in the current checkout; do not call EnterWorktree unless
  the user asks for it or edits are otherwise blocked.
- When work is done, just report the changed files; suggest the commit command
  instead of running it.

# Language of the outputs — English by default

- All code, comments, identifiers, commit messages and generated files (reports, PDF,
  Markdown, docs, READMEs, audits...) are written in English, unless the user explicitly
  asks for another language. Only the conversation itself follows the user's language.

# Shell aliases

- The Bash tool runs with the user's zsh aliases loaded (some names are aliased to
  unexpected commands). Run real binaries with `command <cmd>` or `\<cmd>` (ex:
  `\git status`, `command mkdir -p x`, `command sudo -A ...`) so that no alias is triggered.
- Check the alias list given at the start of the session (SessionStart hook): some aliases
  are useful and are meant to be used as they are (build shortcuts like `cm`, `cmd`, `cma`,
  `cmR`, `buildD`...); use those on purpose when they fit, bypass all the others.

# Subagents — token cost

- NEVER launch a fork agent (`subagent_type: "fork"`) on your own initiative, under any
  pretext: a fork inherits the whole conversation and costs a lot of tokens. ALWAYS ask
  the user first (say how many agents and why), or only fork when the user asked for it.
- Prefer doing the work directly; when a subagent is really useful, propose it first and
  prefer a fresh specialized agent with a self-contained prompt over a fork.

# Root access (sudo) — no TTY on this PC

The shell used by Claude has no terminal: a plain `sudo` can't ask for the password.
- Run root commands with the graphical prompt (GNOME, zenity):
  `SUDO_ASKPASS=~/.local/bin/sudo-askpass command sudo -A <command>`
  The user types the password in a window showing the command. Fallback when sudo -A
  fails: `pkexec <command>` (polkit window, absolute paths, minimal environment).
- Same for scripts calling sudo inside: run them with `SUDO_ASKPASS` exported and make
  them use `sudo -A` (ex: `export SUDO_ASKPASS=~/.local/bin/sudo-askpass`).
- Commands given to the user to run: write them ready to work from Claude's `!` prompt
  too, i.e. with `SUDO_ASKPASS=~/.local/bin/sudo-askpass sudo -A ...` instead of bare `sudo`.
- Root actions still need the usual confirmation (system changes): ask first, then run
  them this way instead of handing them back to the user.

# xstyle — coding style checker

- `xstyle` (built from the ai-utils repository: `style/xstyle`, `./setup.sh install xstyle`) checks and fixes the
  user's coding style (C++, CMake, Python, shell, Rust, generic rules, libutils usage). When it is installed
  (`command -v xstyle`), use it to check what was written or reviewed instead of re-reading the style rules by hand.
- **Always run it with `--rtk`** (or `XSTYLE_RTK=1`): compact output made for the assistant (no colors, links,
  source lines or tables; ~5x fewer tokens). The human output is only for the user's own terminal.
  `xstyle --rtk <paths>`, `xstyle --rtk --fix [CODES] <paths>` (`-n` preview), `xstyle --rtk --diff` (changed lines),
  `xstyle -x CODE` to explain a rule.
