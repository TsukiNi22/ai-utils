# License comparison matrix

Legend: `Y` yes / allowed, `N` no, `C` with conditions, `-` not covered. Copyleft: `none`, `file` (modified files only),
`library` (the library, not the program linking it), `strong` (whole derived work), `network` (strong + SaaS use).
Source: choosealicense.com metadata (`license.py info <ID>`) and the license texts; check the text for any edge case.

## Permissive
| SPDX | Commercial | Modify | Distribute | Private | Patent grant | Copyleft | Notice to keep | State changes | Trademark | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| `MIT` | Y | Y | Y | Y | - | none | copyright + license | N | - | shortest, the user's default (libutils, context-forge) |
| `MIT-0` | Y | Y | Y | Y | - | none | **none** | N | - | MIT without attribution |
| `ISC` | Y | Y | Y | Y | - | none | copyright + license | N | - | MIT equivalent, simpler wording |
| `BSD-2-Clause` | Y | Y | Y | Y | - | none | copyright + license (+ in binary docs) | N | - | |
| `BSD-3-Clause` | Y | Y | Y | Y | - | none | same as BSD-2 | N | **no endorsement** with the author's name | |
| `0BSD` | Y | Y | Y | Y | - | none | none | N | - | public-domain like, OSI approved |
| `Apache-2.0` | Y | Y | Y | Y | **Y** | none | copyright + license + NOTICE file | **Y** | **N** (no trademark grant) | patent retaliation clause; GPL-2.0 incompatible, GPL-3.0 compatible |
| `BSL-1.0` | Y | Y | Y | Y | - | none | only for source (not binaries) | N | - | Boost |
| `Zlib` | Y | Y | Y | Y | - | none | must not misrepresent origin, altered source marked | Y | - | games / libraries |
| `Unlicense` | Y | Y | Y | Y | - | none | none | N | - | public domain dedication (unclear in some countries) |
| `CC0-1.0` | Y | Y | Y | Y | **N** (patents excluded) | none | none | N | N | data / assets, not recommended for code |

## Copyleft
| SPDX | Commercial | Copyleft | Source to provide | Patent grant | Linking from proprietary code | Network (SaaS) clause | Notes |
|---|---|---|---|---|---|---|---|
| `MPL-2.0` | Y | file | modified MPL files | Y | Y | N | mixes well with proprietary files; compatible with GPL by default |
| `EPL-2.0` | Y | file / module | modified files | Y | Y | N | optional GPL secondary license |
| `LGPL-2.1-only` / `LGPL-3.0-only` | Y | library | the library + modifications, allow relinking | 3.0: Y | Y (dynamic, or allow relinking) | N | `-or-later` variants accept newer versions |
| `GPL-2.0-only` | Y | strong | whole derived work | - | N | N | incompatible with Apache-2.0 and GPL-3.0 |
| `GPL-3.0-only` | Y | strong | whole derived work | Y | N | N | anti-tivoization; `GPL-3.0-or-later` = "or any later version" (same text, other notice) |
| `AGPL-3.0-only` | Y | network | whole work, **also to network users** | Y | N | **Y** | SaaS must publish the source |
| `EUPL-1.2` | Y | strong (+ network) | whole work | Y | N | Y | 23 official EU languages, compatible list (GPL, MPL, EPL...) |
| `CC-BY-SA-4.0` | Y | strong (ShareAlike) | - | N | - | - | documentation / assets only, not code |

## Documentation / assets
| SPDX | Commercial | Modify | Attribution | Share-alike | Notes |
|---|---|---|---|---|---|
| `CC-BY-4.0` | Y | Y | Y | N | docs, images, data |
| `CC-BY-SA-4.0` | Y | Y | Y | Y | |
| `CC0-1.0` | Y | Y | N | N | |
| `OFL-1.1` | Y | Y | Y | Y (fonts) | fonts only |

## Source-available (NOT open source, not OSI approved)
| SPDX | Commercial use | Main restriction | Becomes open source | Notes |
|---|---|---|---|---|
| `BUSL-1.1` | C (Additional Use Grant) | production use restricted by the licensor's parameters | Y: after the **Change Date** (max 4 years) -> Change License (ex: Apache-2.0) | parameters to fill: Licensor, Licensed Work, Additional Use Grant, Change Date, Change License |
| `Elastic-2.0` | Y | no managed/hosted service, no circumventing license keys | N | |
| `PolyForm-Noncommercial-1.0.0` | **N** | noncommercial purposes only | N | personal / research / nonprofit |
| `PolyForm-Small-Business-1.0.0` | C | free under 100 people / 1M USD revenue | N | |
| `SSPL-1.0` | Y | offering it as a service = publish the whole service stack | N | rejected by OSI |
| Commons Clause (+ base license) | **N** for selling the software | forbids "selling" (incl. paid hosting/support whose value is the software) | N | not SPDX; added on top of Apache-2.0/MIT: https://commonsclause.com |

## Additions instead of a new license
| Need | Use |
|---|---|
| GPL library usable by proprietary programs | GPL + `Classpath-exception-2.0` (or LGPL) |
| GPL compiler / runtime library | `GCC-exception-3.1`, `LLVM-exception` (with Apache-2.0) |
| permissive but no selling | base license + Commons Clause, or PolyForm Noncommercial |
| open source later, protected now | `BUSL-1.1` with a Change Date |
| dual licensing (open + commercial) | `AGPL-3.0-only` / `GPL-3.0-only` + separate commercial license (needs all contributors' rights: CLA) |

## Compatibility (can code under A be used in a project under B?)
| Dependency \ Project | MIT/BSD/ISC | Apache-2.0 | MPL-2.0 | LGPL | GPL-2.0-only | GPL-3.0 | AGPL-3.0 | proprietary |
|---|---|---|---|---|---|---|---|---|
| MIT / BSD / ISC / Zlib | Y | Y | Y | Y | Y | Y | Y | Y |
| Apache-2.0 | Y (that part stays under Apache-2.0: keep its license + NOTICE) | Y | Y | Y (3.0) | **N** | Y | Y | Y |
| MPL-2.0 | file-level | file-level | Y | Y | Y (secondary) | Y | Y | Y (keep MPL files open) |
| LGPL | dynamic link | dynamic link | dynamic link | Y | Y | Y | Y | dynamic link + relinking |
| GPL-2.0-only | **N** | **N** | N | N | Y | **N** | N | **N** |
| GPL-3.0 | **N** (whole project must be GPL-3.0) | N | N | N | N | Y | Y | **N** |
| AGPL-3.0 | N | N | N | N | N | Y (combined work: AGPL terms for that part) | Y | **N** |
