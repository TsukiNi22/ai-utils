---
name: ci-workflows
description: Write or edit GitHub Actions CI/CD workflows in Tsukini's way - always asks first what the CI/CD must do (build check, unit tests, style check with xstyle, coverage, dispatch to other repositories, RPM/DEB packages, release, docs on gh-pages...), runs the jobs in the user's Docker images (ghcr.io/tsukini22 ci / unit-tests / package / git from the docker-image repository) or else the smallest official image that has what is needed, follows his conventions (names, vars.RUNNER, ccache, set -euo pipefail, annotations, dispatch keywords), and when one of his images misses a tool, adds it to the docker-image repository (Dockerfile, version, CHANGELOG, README) with a local commit only. Use whenever the user wants a CI, a CD, a GitHub workflow / action / pipeline, or to change one.
---

# CI/CD workflows

`SKILL_DIR` = the directory of this file. Templates: `templates/style.yml` (xstyle), `templates/dispatch-repository.yml`
(repository_dispatch to another repository), `templates/python.yml`; the C/C++ build / tests / packages workflows are
the templates of the `cpp-project` skill (`<cpp-project>/templates/workflows/`, section 6 of its SKILL.md).
Check: `python3 SKILL_DIR/scripts/check_workflows.py [files]` (YAML, actionlint when installed, the conventions below).

## 1. Ask first, always (one AskUserQuestion call, French)
Even when the request looks clear, confirm what the CI/CD must do (multiSelect), with the project's obvious choices
pre-selected in the description:
- **Build** (compile check, executables present) · **Tests unitaires** · **Style (xstyle)** · **Coverage**
- **Dispatch** vers d'autres repos (which repository, which event: `repository_dispatch` type) · **Packages** RPM/DEB
  (+ gh-pages repository) · **Release** GitHub sur tag · **Docs** (html-doc on gh-pages)
Then, in the same call when not obvious: **déclencheurs** (push main, pull requests, tags `v*`, manual
`workflow_dispatch`, commit keywords `[build]` / `[release]` / `[ignore]` of `git-conventions`) and **runner**
(`${{ vars.RUNNER }}`, the self-hosted runner of the user: default; `ubuntu-latest` only if asked).

## 2. Images: the user's first, else the smallest that fits
Read the current list and versions in the docker-image repository (`~/personal_delivery/other/docker-image`, else
`gh repo clone TsukiNi22/docker-image`): `README.md` (tools table), `*.Dockerfile`, `CHANGELOG.md`.
| Need | Image |
|---|---|
| C / C++ build (clang, cmake, ccache, libutils + debug/asan, openssl, python3, xstyle) | `ghcr.io/tsukini22/ci:latest` |
| C / C++ unit tests (ci + GTest / GMock) | `ghcr.io/tsukini22/unit-tests:latest` |
| RPM / DEB packages, signatures, repositories | `ghcr.io/tsukini22/package:latest` |
| git / curl only (dispatch, tags, API calls) | `ghcr.io/tsukini22/git:latest` |
| style check (xstyle) | `ghcr.io/tsukini22/ci:latest` (xstyle from ci v2.1.0) |
| Python | `python:<version>-slim` (pip included); `-alpine` only without compiled wheels |
| Rust | `rust:<version>-slim` |
| Node / web | `node:lts-alpine` |
| anything else | the official `-slim` / `-alpine` image of the tool, or no container at all for pure `gh` / `curl` steps |
Pinned version (`ci:v2.1.0`) instead of `latest` only if the user asks for reproducible builds.

### A tool is missing in one of the user's images
Propose (AskUserQuestion) to add it to the image instead of installing it in every run. If yes, in the docker-image
repository:
1. the `*.Dockerfile`: the package in the matching commented group of `dnf install` (or a build block like xstyle's
   in `ci.Dockerfile`), `dnf clean all` kept last;
2. the version: `VERSION` of `.github/workflows/build-image-<image>.yml` (minor for an added tool, patch for a fix,
   major for a base change);
3. `CHANGELOG.md`: `## [<image>:vX.Y.Z] - <date>` with `### Added` / `### Fixed` / `### Changed` under
   `[<image>:Unreleased]`, and the link line `[<image>:vX.Y.Z]: .../commits/<image>/vX.Y.Z/<image>.Dockerfile`;
4. `README.md`: the tools table;
5. **a local commit only**, like the other commits of that repository (`feat(ci-image): add <tool> ...`,
   `fix(package): ...`), **without co-author / AI attribution and never pushed**: tell the user to push (the push
   builds and publishes the image). Optionally build it locally first: `podman build -f <image>.Dockerfile -t test .`.

## 3. Conventions (from the user's workflows)
- `name:` `<Name> - <Kind>` / `<Name> (CI)` / `(CD)` / `(CI/CD)` (`CI - Executables`, `Unit Tests - Libraries (CI)`,
  `Dispatch (CI/CD)`, `Build Image - CI`); file names `kebab-case.yml`.
- Every job: `runs-on: ${{ vars.RUNNER }}`, `defaults: run: shell: bash`, `container: image: ...`.
- `actions/checkout@v5` with `fetch-depth: 1` (0 when the history / a base branch is needed), `actions/cache@v5` for
  ccache (`/github/home/.cache/ccache`, key on the hashes of the sources + `CMakeLists.txt`) then `ccache -s`.
- Every multi-line `run:` starts with `set -euo pipefail` (`set -e` at least), named steps, `timeout-minutes` on builds
  and tests, errors as annotations `echo "::error title=<Title>,file=<file>::<message>"`, `✓ <thing> ok` on success.
- Dispatch chain: `dispatch.yml` decides (`[build]` -> pre-release packages, tag `vX.Y.Z` -> stable, `[ignore]` ->
  nothing) and triggers the other workflows with `workflow_dispatch` through the API (`curl` + `secrets.GITHUB_TOKEN`);
  another repository: `repository_dispatch` with `secrets.GIT_TOKEN` (`templates/dispatch-repository.yml`, the
  receiving workflow listens with `on: repository_dispatch: types: [<event>]`, like `build-image-ci.yml` with
  `libutils-released`).
- Secrets / variables are never created by the skill: list them for the user (`gh variable set RUNNER --body ...`,
  `gh secret set GIT_TOKEN`, GPG secrets of the packages: see `cpp-project`).

## 4. Style check (xstyle)
`templates/style.yml`: on a pull request only the lines it changes (`--diff-ref origin/<base>`), on a push the whole
project; `--fail-on {{FAIL_ON}}` (`major` by default: ask), the Markdown report appended to the job summary. A project
that is not clean yet: start with `--fail-on unforgivable` or the PR-only trigger, and say so.

## 5. Check and hand over
`python3 SKILL_DIR/scripts/check_workflows.py .github/workflows/*.yml` (+ `xstyle .github` for the generic rules), then
list for the user: the workflows written, the images used, the secrets / variables to create, what triggers what.
Commits follow `git-conventions` (`feat(ci): ...`); nothing is pushed without a request.
