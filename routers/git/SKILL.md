---
name: git
description: Router for git / GitHub tasks of the user - commit messages, branches, pull requests, tags, releases, CHANGELOG, repository setup, CI/CD workflows - loads git-conventions, ci-cd, pre-commit (and the doc skills when a README/CHANGELOG is involved). Invoke manually with /git.
disable-model-invocation: true
---

# Git router

**Request given with `/git`:** $ARGUMENTS

Load the skills with the Skill tool (never the other routers: they are manual-only), then do the task. The request is the text given with `/git` (or the latest
request; if none, run `git status` / `git log --oneline -10` and propose: commit message for the pending changes,
release, PR...).

## Context
- `git status`, `git branch -a`, `git log --oneline -15`, `git remote -v`, tags (`git tag --sort=-creatordate | head`).
- Solo or team project: `git shortlog -sn --all | head` (team = several authors) -> branches/PR rules.
- CI keywords used? `grep -l "\[build\]\|\[release\]\|\[ignore\]" .github/workflows/* 2>/dev/null`.

## Skills
| Situation | Skills |
|---|---|
| commit message, commit, amend, branch name, tag, version bump | `git-conventions` |
| release (tag + GitHub release + CHANGELOG) | `git-conventions` (+ `readme-style` if the README changes) |
| pull request (team project only) | `git-conventions` |
| CHANGELOG only | `git-conventions` |
| new repository, branch protection, gh setup | `git-conventions` (+ `cpp-project` for a new C++ project) |
| CI / CD workflows, GitHub actions, the Docker images of the CI | `ci-cd` |
| pre-commit hook, style checked / commits blocked locally before the commit | `pre-commit` |

## Always
- Never commit / push / tag / release / open a PR without an explicit request in the current message: write the
  message and suggest the command otherwise.
- No AI attribution (Co-Authored-By, "Generated with") except where the user allowed it (the ai-utils repository).
- Start the answer with one line listing the loaded skills.
