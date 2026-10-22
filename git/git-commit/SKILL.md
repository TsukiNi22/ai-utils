---
name: git-commit
description: Make the local commit(s) of the diff that currently exists in the repository, the way Tsukini does it in his repositories - the pending changes grouped in logical commits, each message = its title only (type(scope): message, one short line, no body, no description, no Co-Authored-By), local only (never pushed). Use whenever the user asks to commit the current changes / the diff / "fais le commit", "commit tout", "commit les modifs locales".
---

# Commit the current diff (local)

The request to run this skill **is** the explicit request to commit (the global "never commit unprompted" rule is
satisfied); it is **not** a request to push, tag, release or open a PR.

Load `git-conventions` first (message format, types, scopes, CI keywords): it is the source of truth, and `/git`
routes here.

## 1. Look at the diff
```bash
\git status --short
\git diff --stat            # unstaged
\git diff --cached --stat   # already staged
\git log --oneline -10      # the style of the repository, scopes already used
```
- Read the real diff (`\git diff`, `\git diff --cached`) of what is not obvious; untracked files are read too.
- Never stage what must not be versioned: build trees, logs, caches, `.env` / keys / tokens, big binaries, scratch
  folders (ex: `.claude-cluster*`). If it looks like noise and is not in `.gitignore`, **ask** (or leave it out
  and say so); never add it to `.gitignore` on your own.
- Nothing to commit -> say it and stop.

## 2. Group
- **One logical change per commit** (`git-conventions`): split by purpose, not by file count - a fix, a feature,
  its docs, the tests, a pure rename / reformat are separate commits when they can stand alone.
- The CHANGELOG update is its own commit (`docs(CHANGELOG): ...`) and comes last.
- A change that only makes sense together stays together. When a file mixes two purposes, split it with
  `git add -p` only if the hunks are clean; otherwise commit it with the dominant purpose.
- Order the commits so each one builds (dependencies first, docs / changelog last).
- A very large or ambiguous diff: show the planned commits (title + files) in one short list and go on unless the
  split is really ambiguous (then ask once).

## 3. Commit
For each group, stage **by path** (never `git add -A` / `.`), check what is staged, commit:
```bash
\git add -- <paths>
\git diff --cached --stat
\git commit -m "<type>(<scope>): <message>"
```
- Message = **the title only**: one short line in English, the global recap of the group, in the style of the
  recent log. No body, no description, no bullet list, no second `-m`.
- **No `Co-Authored-By`, no "Generated with", no AI mention.** Only exception: a repository whose own
  instructions require the trailer (the `ai-utils` repository, per its `CLAUDE.md`): then exactly that trailer, nothing else.
- A CI keyword (`[build]` / `[release]` / `[ignore]`) only when the repository's workflows use them and the user
  asked for it.
- A hook that fails (xstyle pre-commit, formatter): fix the cause, restage, commit again (a **new** commit, no
  `--amend` of a commit that failed). Never `--no-verify`.
- Never amend, rebase, reset or rewrite an existing commit unless asked.

## 4. Report
`\git log --oneline -<n>` of the new commits and `\git status --short` (what is left uncommitted and why). Then
the push command as a suggestion only (`\git push`), never run.
