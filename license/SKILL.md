---
name: license
description: Choose, add, change, compare or check the license of a project - matches the user's needs (commercial use, copyleft, patents, attribution, SaaS, no-sale...) against the known SPDX licenses with a comparison matrix, proposes the closest existing one, discusses the 2-3 differing points before any custom text, fetches the official text (never written from memory), identifies the current LICENSE and asks before replacing it, updates the README/CPack/package files and checks the dependencies' licenses. Use whenever the user asks about a license, a LICENSE/COPYING file, licensing terms or license compatibility.
---

# License

Not legal advice: say it once in the answer when the choice has consequences (custom terms, commercial
restrictions, relicensing, incompatible dependencies) and recommend a lawyer for anything custom.
`SKILL_DIR` = the directory of this file. Matrix: `reference/matrix.md` (read it before proposing anything).

## 1. Current state
```bash
python3 SKILL_DIR/scripts/license.py identify <LICENSE|COPYING|LICENSE.md>   # SPDX id + similarity + copyright notice
```
Also look at where the license is declared: README (badge, `## License`), `CMakeLists.txt`
(`CPACK_RPM_PACKAGE_LICENSE`, `CPACK_RESOURCE_FILE_LICENSE`, `install(FILES LICENSE ...)`), `package.json`,
`pyproject.toml`, `Cargo.toml`, `debian/copyright`, `SPDX-License-Identifier:` headers in the sources.
Dependencies: `find_package`/`pkg_check_modules`/`FetchContent` of the CMake, vendored folders, package manifests
(their LICENSE files when present locally).

## 2. Needs
From the request, then one AskUserQuestion call (French) for what is still unknown, max 4 questions among:
commercial use by others, must modifications/derived works stay open (none / modified files / library / whole work /
also SaaS), patent grant, attribution required, forbid selling or hosting, closed-source linking allowed,
documentation/assets (CC) vs code, compatibility with a given dependency or ecosystem. The user's own projects use
`MIT` (`Copyright (c) <year> Tsukini`): a good default when nothing is required.

## 3. Match (matrix)
- **Exact match** -> propose it with 1-2 alternatives and why (one line each).
- **Close match** (an existing license differs on 2-3 points) -> show a table "besoin / licence proposée / écart",
  then AskUserQuestion: accept the existing license as it is (Recommandé), pick another close one, or only if the
  points are really not negotiable, an addition. **Never** go straight to a custom license.
- **Not negotiable** -> prefer, in this order: another known license (incl. source-available: `BUSL-1.1`,
  `Elastic-2.0`, `PolyForm-*`), a known license + an official exception/clause (`license.py exception <ID>`,
  Commons Clause), dual licensing. A text written from scratch is the very last resort: warn clearly (unknown
  license = legal risk, not OSI/FSF approved, incompatible with most ecosystems, packagers and companies will avoid
  it, have it reviewed by a lawyer) and keep it short, as additional terms on top of a known base.
- Source-available licenses are **not open source**: say it when proposing them.
- Check the dependencies against the chosen license (matrix "Compatibility"); report blocking ones (ex: GPL
  dependency in an MIT project, Apache-2.0 with GPL-2.0-only).

## 4. Write it
```bash
python3 SKILL_DIR/scripts/license.py info <SPDX-ID>                             # permissions / conditions / limitations
python3 SKILL_DIR/scripts/license.py fetch <SPDX-ID> --holder "<name>" [--year YYYY] --out LICENSE [--force]
```
- Texts only from choosealicense.com / the SPDX list (cached in `~/.cache/license-skill`); fills `[year]`,
  `[fullname]`, `<year>`, `<copyright holders>`; holder = existing notice, else `git config user.name`, else ask.
- **Existing LICENSE**: show its identification and ask (AskUserQuestion) before replacing it, with the option to
  keep a backup (`LICENSE.old`); `--force` only after the confirmation. Relicensing code that has other contributors
  needs their agreement: say it.
- Licenses with parameters: `BUSL-1.1` (Licensor, Licensed Work, Additional Use Grant, Change Date, Change License),
  `Apache-2.0` (`NOTICE` file when redistributing), GPL/LGPL/AGPL (the copyright notice goes in the README / file
  headers, the LICENSE file is the plain text; `-or-later` vs `-only` is a notice choice, same text).
- Exceptions / clauses go in the same LICENSE after the base text (or `LICENSE.exception`), with a sentence saying so.

## 5. Update the references
README (`readme-style`: badge `![License](https://img.shields.io/github/license/<owner>/<repo>)`, `## License` section
naming the SPDX id), `CPACK_RPM_PACKAGE_LICENSE "<SPDX>"`, manifests `license` fields, `SPDX-License-Identifier`
headers when the project already uses them. Report: chosen license (SPDX), why, files changed, open points.

An analysis delivered as a summary (comparison of licenses, compatibility of the dependencies, state of a
project) goes in a **PDF** through `pdf-report`; a simple LICENSE change is just reported in the chat.
