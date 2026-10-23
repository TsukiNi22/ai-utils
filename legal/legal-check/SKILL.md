---
name: legal-check
description: Deep legal compliance check of a project, a data processing or a website - GDPR / RGPD (lawful bases, consent, information, the rights incl. erasure and portability, records, processors and DPAs, DPIA, retention, security, breaches, transfers), cookies and trackers (ePrivacy, CNIL), legal notice (LCEN mentions légales), terms (CGU / CGV), consumer and distance-selling law, marketing emails, accessibility (RGAA, European Accessibility Act), DSA, AI Act, Cyber Resilience Act, NIS2, Data Act, the licenses of the project and of every dependency / font / image / dataset / model with the credits and notices they require (GPL / AGPL source offers, Apache NOTICE), the mandatory mentions (language - loi Toubon, prices, advertising, product safety), UK / Swiss / US state / other laws when the users are there - in two scopes (global: any project handling data; web: a site or web app, on top), every finding with its legal basis, the evidence in the project (file:line, tracker seen, page) and its fix, the dates that move checked against the primary texts, delivered as a report. Use whenever the user asks if a project / site is legal or compliant, a RGPD / GDPR / cookies / CGU / mentions légales / RGAA / accessibility / privacy / right to erasure / AI Act check, "est-ce légal", "audit légal", "conformité".
---

# Legal check (compliance audit)

Find **everything legal that can cause a problem** in a project, prove it from the project itself, and say how to fix
it. Read only: nothing is changed (the fixes: `legal-setup`). Not legal advice: said once in the report; anything
binding (a contract, a sanction risk, a sensitive processing) is flagged "to verify with a lawyer / DPO".

**Request given with the skill:** the text after `/legal-check` if any (a scope, a project, a site URL).

## 1. Scope (skill `interactive-questions`, one round, only what the project does not tell)
- **Scope**: global (any project handling data: `references/global.md`) · **web** (a site / web app: `global.md` +
  `references/web.md`) · both. A project with a web front: both, recommended.
- The facts the checklist needs and the code cannot give: the countries of the users (EU / France, UK, Switzerland,
  US states, others), B2C or B2B, the organisation (company / individual / public body, its size: micro, < 250 staff,
  turnover > €250 M), the role (controller / processor), sector (health, finance, minors, public, AI), a live URL to test.
- Unknown answers become `to verify` items, never guesses.

## 2. Inventory (read the project, the live site when given)
- **Personal data**: models / schemas / migrations, forms, API payloads, logs, analytics events, files uploaded, emails,
  backups, caches, search indexes, AI prompts. A table: data · where stored · why · who receives it · kept how long.
- **Third parties / recipients**: every SDK, script, API key, env var, infra file (docker-compose, Terraform, CI
  secrets), DNS / CDN, email provider, payment, error tracking, AI API, fonts, embeds - with their country.
- **Web**: load the site with an empty browser profile (or `curl -I` + the HTML when no browser): cookies / storage set
  before any consent, third-party requests, the banner and its buttons, the footer links (legal notice, privacy,
  cookies, terms, accessibility), the forms and their notices, HTTPS / headers.
- **Documents present**: privacy policy, legal notice, terms, cookie policy, accessibility statement, records, DPAs,
  retention policy, breach procedure, security.txt / SECURITY.md, LICENSE / NOTICE.

## 3. Check (every applicable item of the references, none skipped)
- Go through `references/global.md` (and `web.md` for the web scope) **in order**; each item gets a status:
  `ok` · `missing` · `partial` · `n/a` (+ why) · `to verify`.
- Each non-ok item: the **legal basis** (article), the **evidence** (file:line, URL, cookie name, screenshot / request
  seen), the **risk** (who can sanction, the order of the fines: GDPR up to 4 % / €20 M, LCEN, DGCCRF, CNIL decisions),
  the **fix** (what to add / change / remove) and its **effort** (S / M / L).
- **Dates and statuses marked ⏱**: check them against `references/sources.md` (EUR-Lex, Légifrance, CNIL, the
  Legislative Observatory...) with a web search / fetch before writing them; a blog alone never settles a date;
  conflicting sources: say both and mark `to verify`. Update `sources.md` when a status changed (said in the report).
- Licenses of the dependencies and of the content: use `audit-deps` / `license` and include their result.
- Look past the checklist: anything else in this project that a regulator, a user or a rights holder could object to
  (a dark pattern, a misleading claim, scraping, data sold, staff monitoring, a sensitive field) is added.

## 4. Report (skill `report`, format asked: Markdown and / or PDF)
- Summary: the scope, the laws that apply and why, the counts by severity, the 5 most urgent fixes.
- Findings table sorted by severity: **blocking** (illegal now: trackers before consent, no legal notice, no lawful
  basis, data never deleted, plaintext passwords...) · **high** · **medium** · **low** · then `to verify` and `ok`.
- The inventory tables (data, recipients, documents present / missing), the `n/a` items with their reason, the sources
  checked with their date, the "not legal advice" line.
- Saved in `audit/` at the repository root (`legal-check-<date>.md` / `.pdf`).
- Last: offer `legal-setup` for the fixes (through the popup: all / the blocking ones / a choice / later).
