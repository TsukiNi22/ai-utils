---
name: legal
description: Router for legal matters of the user's projects - licenses (choose, compare, add, replace, check compatibility of dependencies) and the future legal skills (notices, third-party attributions, contributor agreements, privacy...). Invoke manually with /legal.
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
| anything else legal (privacy policy, CLA, trademarks, terms of use...) | no skill yet: say it, give general information only and recommend a lawyer for anything binding |

## Always
- Reports (legal state, dependencies, comparisons) are delivered through `pdf-report` (format asked once, English, `audit/` at the repository root).
- This is not legal advice: say it once when the answer has consequences (relicensing, commercial use).
- Never replace a LICENSE or legal text without the user's explicit confirmation.
- Start the answer with one line listing the loaded skills.
