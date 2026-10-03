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

# Reports — always PDF

- Every audit, review, analysis, benchmark or summary delivered to the user is written in
  Markdown and converted to PDF with the `pdf-report` skill (its style, its script
  `~/.claude/skills/pdf-report/scripts/md2pdf.py`). Ask once which outputs are wanted:
  both `.md` + `.pdf` (recommended), PDF only or Markdown only. The chat only gives the
  verdict and the paths.
- Default locations: audits in `audit/` at the root of the repository (or of the current
  folder outside a repository), READMEs at the root of the repository (or the current
  folder); `docs/` only when the user asks for it.

# Subagents — token cost

- NEVER launch a fork agent (`subagent_type: "fork"`) on your own initiative, under any
  pretext: a fork inherits the whole conversation and costs a lot of tokens. ALWAYS ask
  the user first (say how many agents and why), or only fork when the user asked for it.
- Prefer doing the work directly; when a subagent is really useful, propose it first and
  prefer a fresh specialized agent with a self-contained prompt over a fork.

# Root access (sudo) — no TTY on this PC

The shell used by Claude has no terminal: a plain `sudo` can't ask for the password
(and `sudo` is aliased to `lock` in the user's zsh, never call it bare).
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
