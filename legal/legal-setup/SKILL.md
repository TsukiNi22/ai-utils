---
name: legal-setup
description: Set up the legal compliance of a project or a website (global or web scope): writes the documents from the real data flows (privacy policy, mentions légales, terms CGU / CGV, cookie policy, accessibility statement, records of processing, retention policy, rights and breach procedures, credits, security.txt) after asking every fact it cannot know (identity, host, DPO, countries, languages - never invented), and implements the code (a CNIL-compliant consent banner blocking trackers until consent with refuse as easy as accept, data export and real erasure, purge jobs, form notices, unsubscribe, AI disclosure, accessibility fixes, footer links). Use whenever the user wants a project / site made legal or compliant, a privacy policy, mentions légales, CGU / CGV, a cookie banner, RGPD / GDPR setup, data deletion, an accessibility statement, or the fixes of a legal-check.
---

# Legal setup (make it compliant)

Turn the obligations into **documents and code** for this project. The checklist is the one of `legal-check`
(`../legal-check/references/global.md`, `web.md`, `sources.md`): read it first. Not legal advice: binding texts (terms,
DPAs, a sensitive processing) are drafts to have reviewed by a lawyer, said once.

**Request given with the skill:** the text after `/legal-setup` if any (what to set up, a legal-check report).

## 1. Start from the facts
- A recent `legal-check` report: take its findings (the fixes to do). None: run its inventory steps (§2 of
  `legal-check`) first - documents describe the real data flows, never a generic template.
- Ask what only the user knows (skill `interactive-questions`, rounds of 1-4 questions): scope (global / web / both);
  the publisher's identity (company name, legal form, address, RCS / SIREN, VAT, share capital, publication director,
  contact email / phone) or an individual; the host (name, address, phone); DPO / privacy contact; the countries and the
  **languages** of the documents (the language of the users: French for a French site); B2C / B2B; what is sold; the
  mediator; the retention wishes; the analytics / marketing tools kept or dropped.
- **Never invent** an identity, a number, a host, a mediator or a retention period: a `{{PLACEHOLDER}}` stays, listed at
  the end. Confirm before overwriting an existing legal text (show the diff).

## 2. Documents (templates in `templates/`, filled and adapted, written in the language chosen)
| Document | When | Template |
|---|---|---|
| Privacy policy (art. 13-14) | personal data | `privacy-policy.md` |
| Legal notice / mentions légales (LCEN art. 6-III) | a website | `legal-notice.md` |
| Cookie policy + the list of the trackers | a site with any tracker | `cookie-policy.md` |
| Terms of use (CGU) / of sale (CGV) | accounts, user content / selling | `terms.md` |
| Accessibility statement (RGAA / EAA) | public sector, > €250 M, EAA services | `accessibility-statement.md` |
| Records of processing (art. 30) | personal data (always recommended) | `records-of-processing.md` |
| Retention policy + purge schedule | personal data | `retention-policy.md` |
| Data subject requests procedure (art. 12-22) | personal data | `rights-procedure.md` |
| Breach procedure + register (art. 33-34) | personal data | `breach-procedure.md` |
| Processors / sub-processors list + DPA checklist (art. 28) | any external service | in `records-of-processing.md` |
| security.txt (RFC 9116) / SECURITY.md (CRA vulnerability handling) | a site / a product | `security.txt` |
| Credits / open-source licenses (THIRD_PARTY_NOTICES, NOTICE contents, fonts, icons, images, maps) | anything distributed | `audit-deps` output |
| The project's LICENSE, SPDX ids, copyright headers | always | `license` |
Placed where the project keeps them (site pages / routes, `docs/legal/`, `legal/`), linked from the footer / the app
menu / the README; each page dated with a version, the old versions kept (terms).

## 3. Code (the stack of the project, its style skills)
- **Consent banner**: nothing non-essential runs before consent (scripts blocked: `type="text/plain"` + loaded on
  consent, or a consent-mode loader); Accept all / **Refuse all** / Customize on the first layer, the same weight;
  purposes granular; choice stored ≤ 6 months then asked again; proof logged (time, version, choices); a "Cookies"
  link everywhere to change it; embeds behind click-to-load; fonts self-hosted. Prefer a maintained open-source CMP
  (tarteaucitron, Klaro, Orejime) configured for this, or a small own one when the trackers are few.
- **Audience measurement** without consent only in the CNIL exempt configuration (first-party, IP truncated,
  13 months / 25 months, opt-out offered) - else behind the banner.
- **Rights**: data export (JSON / CSV, art. 20) and **deletion** (art. 17) from the account and by request: cascade to
  every table, file, cache, index, processor (their deletion APIs), mailing list; backups' expiry documented; a
  confirmation step; a log of the requests (without the deleted data).
- **Retention**: purge / anonymisation jobs per data category (cron, scheduled task), log rotation, IP truncation.
- **Forms**: a short notice under each (who, why, basis, link), only needed fields, no pre-ticked opt-in, double opt-in
  for newsletters, unsubscribe link and `List-Unsubscribe` in marketing emails.
- **Security basics** (art. 32): password hashing (argon2 / bcrypt), HTTPS + HSTS, secure cookies, CSP, secrets out of
  the repository, rate limiting, admin 2FA.
- **AI** (AI Act art. 50, from 2 Aug 2026): a visible "you are talking to an AI" for chatbots, AI-generated content
  labelled (+ machine-readable marking when generating media).
- **Accessibility**: the fixes found (labels, alts, contrast, focus, keyboard, headings, lang), an axe / pa11y check in
  the CI when there is one (`ci-cd`), the statement linked from the footer.
- **E-commerce**: withdrawal information + form, "order with obligation to pay" button, the ODR link removed, the
  mediator shown, easy subscription termination.
- **Credits**: an "About / Open-source licenses" screen or page generated from the dependencies (license comments kept in
  the bundles), the NOTICE contents of Apache-2.0 parts, attributions (CC-BY, OpenStreetMap...), the AGPL / GPL source
  offer when required.
- **Language and mandatory mentions** (`global.md` §15): the documents and consumer information in the language of
  the market (French in France), `lang` on pages and passages, prices with taxes, ads marked, product safety
  information on product listings.
- **Footer / menu**: Legal notice · Privacy · Cookies (manage) · Terms · Accessibility · Credits, on every page.

## 4. Check and hand over
- Run the project's tests / build; for a site, load it again with an empty profile: no tracker before consent, refuse
  works, the links work, the pages render on a phone.
- Recap: what was written / changed (paths), the placeholders still to fill, what needs a lawyer / DPO review (terms,
  DPAs, DPIA), the dates to watch (`sources.md` ⏱), and the next re-check (`legal-check`) recommended in 6-12 months.
- Commits: the user's git rules and `git-conventions` (nothing committed unless asked / allowed).
