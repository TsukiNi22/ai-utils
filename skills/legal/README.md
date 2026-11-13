# Legal skills

The legal side of a project: its license, a compliance check (GDPR, cookies, legal notice, terms, accessibility,
credits, AI Act, CRA...) and the setup of what is missing, documents and code. Router:
[`/legal`](../routers/legal/SKILL.md).

## Skills
<!-- generated:skills -->
| Skill | What it does | Installs with it |
|---|---|---|
| [`legal-check`](legal-check/SKILL.md) | Deep legal compliance check, global (any project handling data) or web (a site / web app, on top): GDPR (bases, consent, notices, rights incl. erasure, records, DPAs, DPIA, retention, security, breaches, transfers), cookies (ePrivacy / CNIL), legal notice (LCEN), terms, consumer law, accessibility (RGAA, EAA), language (loi Toubon), licenses and credits of every dependency / font / asset, mandatory mentions, DSA, AI Act, CRA, NIS2, Data Act, UK / Swiss / US laws; each finding with its article, the evidence in the project and the fix; dates checked on the primary texts; a report. | [`interactive-questions`](../ai/interactive-questions/SKILL.md), [`report`](../docs/report/SKILL.md), [`license`](../legal/license/SKILL.md), [`audit-deps`](../audit/audit-deps/SKILL.md) |
| [`legal-setup`](legal-setup/SKILL.md) | Makes it compliant: writes the documents from the real data flows (privacy policy, mentions légales, cookie policy, CGU / CGV, accessibility statement, records of processing, retention, rights and breach procedures, credits, security.txt) after asking every fact it cannot know (never invented), and implements the code (consent banner blocking trackers until consent, refuse = accept, export and real erasure, purge jobs, form notices, unsubscribe, AI disclosure, accessibility fixes, footer links). | [`legal-check`](../legal/legal-check/SKILL.md), [`interactive-questions`](../ai/interactive-questions/SKILL.md), [`report`](../docs/report/SKILL.md), [`license`](../legal/license/SKILL.md), [`audit-deps`](../audit/audit-deps/SKILL.md) |
| [`license`](license/SKILL.md) | Finds the license matching the needs (comparison matrix), negotiates the close ones, fetches the official text, replaces the current LICENSE only after confirmation, checks dependencies compatibility. | [`report`](../docs/report/SKILL.md) |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## Examples
- *which license fits this project?*
- *check the legal compliance of the website*
- *make the website compliant (privacy policy, cookie banner, legal notice)*

[All the categories](../README.md) · [Installation](../../INSTALL.md)
