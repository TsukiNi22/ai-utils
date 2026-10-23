---
name: legal
description: Router for legal matters of the user's projects - licenses (choose, compare, add, replace, check compatibility of dependencies) the licenses of the dependencies, and the legal compliance of a project or a website (GDPR / RGPD, cookies, legal notice, terms, accessibility, language, mandatory mentions, AI Act, CRA...): its check and its setup. Invoke manually with /legal.
disable-model-invocation: true
---

# Legal router

**Request given with `/legal`:** $ARGUMENTS

Load the skills with the Skill tool (never the other routers: they are manual-only), then do the task.
With no request: inspect the project (`LICENSE*`, `COPYING`, `NOTICE`, README license section, CPack /
package manifests `license` fields, dependencies) and report its legal state, without changing anything.

## Skills
| Situation | Skills |
|---|---|
| choose / add / change / compare a license, LICENSE file, "which license for...", relicensing | `license` |
| licenses of the dependencies: what to credit, restrictive / paid / unknown ones, THIRD_PARTY_NOTICES | `audit-deps` (+ `license` for the compatibility with the project license) |
| known vulnerabilities / compromised versions of the dependencies | `audit-deps` (vulnerabilities part) |
| is the project / site legal? GDPR / RGPD, cookies, mentions légales, CGU / CGV, RGAA / accessibility, privacy, right to erasure, AI Act, CRA, mandatory mentions, licenses and credits | `legal-check` (global or web scope; uses `license` / `audit-deps`) |
| make it compliant: privacy policy, legal notice, cookie banner, terms, accessibility statement, data export / deletion, retention, records, credits | `legal-setup` (after a `legal-check`, or its inventory first) |
| anything else legal (CLA, trademarks, a contract) | no skill: general information only, a lawyer for anything binding |

## Always
- Reports (legal state, dependencies, comparisons) are delivered through `report` (format asked once, English, `audit/` at the repository root).
- This is not legal advice: say it once when the answer has consequences (relicensing, commercial use).
- Never replace a LICENSE or legal text without the user's explicit confirmation.
- Start the answer with one line listing the loaded skills.
