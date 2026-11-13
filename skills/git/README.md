# Git skills

My git conventions (title-only commits, tags, releases, CHANGELOG, branches, pull requests), the commit of the
pending diff, the xstyle pre-commit hook and the CI / CD workflows on my Docker images. Router:
[`/git`](../routers/git/SKILL.md).

## Skills
<!-- generated:skills -->
| Skill | What it does | Installs with it |
|---|---|---|
| [`ci-cd`](ci-cd/SKILL.md) | CI **and CD** of a GitHub repository: always asks what to run (build, tests, xstyle style check, coverage / GitHub release from the CHANGELOG, RPM/DEB packages, container image, PyPI, docs, dispatch), my Docker images first (`ghcr.io/tsukini22/*`) or the smallest official one, my conventions, a missing tool added to the docker-image repository with a local commit. Used by `cpp-project` / `python-project` on GitHub repositories. | [`git-conventions`](../git/git-conventions/SKILL.md) |
| [`git-commit`](git-commit/SKILL.md) | Commits the diff that exists right now, locally: reads the changes, groups them in logical commits (CHANGELOG last), stages by path, one title-only message each (`type(scope): message`, no body, no co-author, per `git-conventions`), never pushes; the ai-utils repository keeps its own trailer. | [`git-conventions`](../git/git-conventions/SKILL.md) |
| [`git-conventions`](git-conventions/SKILL.md) | Commit messages `type(scope): message`, CI keywords, tags, GitHub releases, CHANGELOG, branches (`main` alone when solo; `main`/`dev`/`sub/`/`feat/`/`fix/` in a team) and PRs (CHANGELOG-style body, `gh` assignee/labels), no AI attribution. | — |
| [`pre-commit`](pre-commit/SKILL.md) | xstyle pre-commit hook of a repository: asks every option (fail on major / warn on minor by default, staged lines, safe fixes, missing xstyle = warning only, local or shared `.githooks`), keeps an existing hook, installs the missing tools on request, demo repository to try it (`scripts/demo_repo.sh`); also `./setup.sh hook` from the repository. | [`git-conventions`](../git/git-conventions/SKILL.md) |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## Examples
- *commit the changes*
- *add a CI with the build, the tests and xstyle, and a release from the CHANGELOG*
- *set up the pre-commit hook, failing on major issues only*

[All the categories](../README.md) · [Installation](../../INSTALL.md)
