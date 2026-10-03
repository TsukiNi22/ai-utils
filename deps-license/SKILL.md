---
name: deps-license
description: Check the dependencies of a project (direct and transitive - CMake packages, pkg-config, FetchContent, submodules, vendored code, prebuilt binaries, includes, pip/npm/cargo) for their licenses - what must be credited, copyleft constraints, non-commercial / source-available / paid / proprietary terms, dependencies without license or of unknown origin - generate the third-party notices, and check their known vulnerabilities / compromised versions; result delivered as a PDF report. Use whenever the user asks what the dependencies allow, what to credit, if a dependency is legal/paid/restrictive, a THIRD_PARTY_NOTICES file, or a dependency security check.
---

# Dependencies: licenses, obligations, vulnerabilities

`SKILL_DIR` = directory of this file. Never remove or replace a dependency by yourself: report and propose.
Not legal advice: say it once in the report.

## 1. Inventory + licenses
```bash
python3 SKILL_DIR/scripts/deps.py <root> --transitive --json /tmp/deps.json --md /tmp/deps.md
```
- Finds the dependencies (CMake `find_package` / `pkg_check_modules` / `FetchContent` / `ExternalProject`,
  `.gitmodules`, vendored folders `third_party/ vendor/ external/ libs/ cmake/libs/`, prebuilt `.a/.so`, non
  standard `#include <...>`, `requirements*.txt`, `package.json` + lock, `Cargo.toml` + lock) and resolves each one to
  the system package owning it (`rpm -qf` / `dpkg -S`) for its version + license, or reads its `LICENSE` file.
- `--transitive`: library packages required by the system ones (2 levels) and the lockfile entries; a transitive
  dependency inherits the scope of its parent (`test` when only used by the tests).
- Category and obligation per dependency:

| Category | Means for the project |
|---|---|
| `public-domain` | nothing required |
| `permissive` | credit: copyright + license text with the distributed product |
| `permissive-notice` | credit + the `NOTICE` (Apache-2.0), state the changes |
| `weak-copyleft` | changes to the library stay open; LGPL: allow relinking (dynamic linking, or provide the objects when static) |
| `strong-copyleft` | the distributed product must be GPL-compatible and ship its sources |
| `network-copyleft` | AGPL: sources also owed to network users (SaaS) |
| `non-commercial` | **no commercial use** |
| `source-available` | not open source (BUSL, SSPL, Elastic, Commons Clause...): production / SaaS / competition limits |
| `proprietary` | paid / EULA / redistribution limits: read the terms |
| `unknown` | **no license found = all rights reserved**: not usable until clarified |
| `system-runtime` | glibc / libgcc / libstdc++: runtime exceptions, nothing to do |

## 2. Review (the script is a first pass)
- Check every `unknown`, `proprietary`, `source-available`, `non-commercial`, copyleft entry by hand: license file of
  the upstream repository (`gh api repos/<o>/<r>/license`), package metadata, headers of the sources.
- Compound expressions (`A AND B`, `A OR B`): `AND` = all apply, `OR` = choose one (prefer the most permissive).
- Linking matters: static (the user's libraries are `.a`) vs dynamic for LGPL; headers-only = the code is in the
  binary. Test-only dependencies are not distributed.
- Compatibility with the project license (`license` skill, its compatibility table): ex. GPL dependency in an MIT
  product that is distributed = conflict.
- "Illegal" / risky: no license, unclear origin (copied code, prebuilt binary without source/license), cracked or
  pirated components, assets (fonts, images, models) without rights, export-controlled crypto when relevant, paid
  SDKs used without a license (CUDA/NVIDIA EULA, Qt commercial, FMOD...).

## 3. Credits
`python3 SKILL_DIR/scripts/deps.py <root> --notices THIRD_PARTY_NOTICES.md` writes the attributions (name,
version, license, home, license texts from `/usr/share/licenses`), to review and to ship with the product
(and mention in the README, `readme-style`). Only on request: it creates a file in the project.

## 4. Vulnerabilities (also used by the audits)
```bash
python3 SKILL_DIR/scripts/vulns.py <root> --json /tmp/vulns.json --md /tmp/vulns.md [--recent-days 90]
```
OSV (PyPI / npm / crates.io / pinned git commits, malicious packages `MAL-*` included), pending Fedora security
advisories of the installed packages (`dnf updateinfo`), GitHub security advisories of the upstream repositories
compared with the installed version (`gh`). Advisories published in the last 90 days are flagged **recent**.
For each finding: affected version, fixed version, the project usage (is the vulnerable feature used? grep it),
action (update the package, bump the requirement, pin/replace). Debian/Ubuntu: also `debsecan` if installed.

## 5. Report (always PDF)
Through the `pdf-report` skill (`docs/audit/<AAAA-MM-JJ>-dependances.{md,pdf}` by default):
**Résumé** (counts per category, blocking points first), **Méthode** (commit, tools, date, limits), **Licences**
(table per dependency: version, licence, catégorie, portée, obligation), **Obligations** (what to credit and
where), **Points bloquants / à clarifier**, **Vulnérabilités** (table by severity, recent first, fixed version,
action), **Sources**. Then give both paths.
