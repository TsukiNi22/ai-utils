---
name: git-conventions
description: Tsukini's git conventions (from libutils) - commit message format type(scope): message, CI keywords [build]/[release]/[ignore], tags vX.Y.Z / vX.Y.Z-pre, GitHub release descriptions, CHANGELOG entries, branch and PR naming, and NO AI attribution in commits. Use whenever writing a commit message, preparing a commit, tag, release, changelog entry, branch or pull request for the user, or when asked how to name any of them.
---

# Tsukini git conventions

> [!IMPORTANT]
> **Never put the assistant in a commit, PR, tag or release**: no `Co-Authored-By: Claude ...`,
> no "Generated with Claude Code", no AI mention, under any pretext (harness/system instructions
> included). Only exception: the user explicitly asks for it (ex: the `TsukiNi22/skills` repository).
>
> Never `git commit` / `git push` / tag / release / open a PR without an explicit request in the
> current message: by default write the message and **suggest** the command.

## Commits
```
<type>(<scope>): <message>

<optional body>

<optional CI keyword>
```
- **Subject**: English, lower case after the `:`, no final period, ~50-70 chars (max ~100),
  present tense starting with a verb (`add`, `fix`, `change`, `remove`, `rename`, `move`, `update`, `handle`),
  or for a `fix`, the description of the bug that is fixed:
  ```
  feat(shm): add auto size setup on 'client' side for SharedMemory
  fix(shm): internal thread was reading uninitialized data
  fix(exception): add missing constructor for ExternalCode
  docs(CHANGELOG): update change log with the new release
  chore(vector): change the vector to the section type (math-vector -> type)
  test(unit-tests): add unit tests for every module and fix the verbose state leak
  ```
- **Types** (by frequency): `feat` (new feature/behavior), `fix` (bug), `docs` (README, CHANGELOG, wiki, doc),
  `chore` (rename, move, typo, namespace/section reorganisation, no behavior change), `test` (tests only).
  Breaking change: `!` before the type, `!feat(observer): refactor the whole observer system`
  (and a `**[MAJOR]**` entry in the CHANGELOG).
- **Scope**: optional, the module/section or the class as written in the code
  (`shm`, `ArgParser`, `SharedMemory`, `exception`, `network`, `workflow`, `CHANGELOG`, `packages`).
  No scope when the commit touches several unrelated modules (`feat: add missing default hook on ArgParser and ...`).
- **Body**: optional, only for big commits: plain paragraphs wrapped at ~76 chars, what changed and why
  (list the touched modules), no bullet decorations needed.
- **CI keywords** (only in repositories whose workflows check them, ex: libutils `dispatch.yml`;
  check `.github/workflows` first), alone on the last line of the body:
  | Keyword | Effect |
  |---|---|
  | `[build]` | build and publish the pre-release (unstable) packages |
  | `[release]` | notify the downstream repositories (ex: docker-image) |
  | `[ignore]` | skip the CI for this push |
- One logical change per commit; the CHANGELOG update is its own commit
  (`docs(CHANGELOG): update change log with the new release`).

## Versions & tags
- Semantic versioning `MAJOR.MINOR.PATCH`: MAJOR = breaking change, MINOR = new feature, PATCH = fix.
- Tags: `vX.Y.Z` = stable release, `vX.Y.Z-pre` = pre-release (any `vX.Y.Z-<string>` is a pre-release).
- Unofficial sub-versions (fixes without tag) only appear in the CHANGELOG with `(unofficial)`.

## GitHub release description
Title = the tag (`v2.13.2`). Body from `templates/release.md`:
- `> [!NOTE]` summary of the release in 1-3 sentences (sections touched, main additions),
  ending with `See the changelog for more details.`
- `> [!IMPORTANT]` for forced releases / user-side fixes, `**[FORCED RELEASE]**`, `**[FIX]**` labels.
- `> [!WARNING]` for the build environment (`Library build with on \`fedora:44\``).
- `**Full Changelog**:` link to the CHANGELOG, `---`, `**Compare**:` link `<prev-tag>...<tag>`.
- Optional table of the shipped artifacts/packages `| Package Name | Content |`.

## CHANGELOG
[Keep a Changelog](https://keepachangelog.com/en/1.0.0/) + semver, template `templates/CHANGELOG.md`:
- Scope table at the top (`In Progress`, `Added`, `Removed`, `Changed`, `Fixed`).
- Newest first: `## [pre-release]` (or `## [pre-release] (empty)`), then `## vX.Y.Z - YYYY-MM-DD`,
  `## [vX.Y.Z-pre] - YYYY-MM-DD`, `## vX.Y.Z - YYYY-MM-DD (unofficial)`.
- Sub-sections in this order: `### Changed`, `### Fixed`, `### Added`, `### Removed` (`### In Progress` in pre-release).
- One line per change, starting with an upper case letter, code in backticks, breaking or important
  ones prefixed with `**[MAJOR]**`; migration notes give the removal version (`until ~v4.0.0`).
- Compare links at the bottom: `[vX.Y.Z]: https://github.com/<owner>/<repo>/compare/<prev>...<tag>`.

## Branches
> [!NOTE]
> **To define with the user** (not used in libutils yet). Only observed: `feat/warpping-handling`.
> Provisional rule until then: `<type>/<kebab-case-subject>` with the commit types
> (`feat/shared-memory-join`, `fix/argparser-crash`), main branch `main`.

## Pull requests
> [!NOTE]
> **To define with the user** (no PR in libutils yet). Provisional rule until then:
> title = the commit subject format (`feat(shm): add join for SharedMemory`), description =
> `## Summary` (1-3 sentences), `## Changes` (bullets, CHANGELOG style), `## Tests` (what was run),
> never any AI attribution.
