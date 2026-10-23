# Legal checklist - any project that handles data (software, SaaS, app, API, tool, data pipeline, AI)

Each item: **what is required** · *legal basis* · when it applies · what to look for in the project · the usual fix.
Status of an item in the report: `ok` · `missing` · `partial` · `n/a` (say why) · `to verify` (fact or lawyer needed).
Dates marked ⏱ move: check them against the primary sources (`sources.md`) at each audit.

## 0. Applicability first (decide which blocks apply)
- Personal data processed at all (any data about an identifiable person: name, email, IP, device id, account id, logs,
  cookies, location, photos, voice, free text)? → GDPR blocks (§1-§6). *GDPR art. 2-4.*
- Where are the users / customers? EU/EEA (GDPR art. 3(2): targeting or monitoring EU people, even from outside),
  UK (UK GDPR + DPA 2018, amended by the Data (Use and Access) Act 2025 ⏱), Switzerland (nFADP / nLPD, since 1 Sept 2023),
  US states (§9), Canada (PIPEDA, Québec Law 25), Brazil (LGPD), others named by the user.
- Who decides the purposes: **controller** (art. 4(7)) or **processor** for a client (art. 4(8), art. 28)? Both is common.
- Consumers (B2C) or businesses only (B2B)? Consumer law (§8) applies to B2C.
- A website / web app? → also `web.md`. AI system or model? → §10. A product with digital elements sold in the EU
  (software, firmware, app, library put on the market)? → §11 CRA. Critical / important sector? → §12 NIS2.
- Purely household / personal use (GDPR art. 2(2)(c)) or no personal data at all: say it and skip the blocks, with the reason.

## 1. Principles and lawful basis (GDPR art. 5-11)
- **A lawful basis per purpose** (art. 6(1): consent, contract, legal obligation, vital interest, public task,
  legitimate interest + its balancing test). Look: every purpose of the code (accounts, analytics, emails, logs, AI
  training, sharing). Fix: a purposes / bases table in the records (§4) and the privacy notice.
- **Consent** when used: freely given, specific, informed, unambiguous, as easy to withdraw as to give, proof kept
  (art. 4(11), 7). No pre-ticked boxes (CJEU Planet49, C-673/17), no consent bundled with the contract.
- **Children**: information society services offered to a child on consent → parental consent under 16
  (art. 8; member states may lower to 13: **France 15**, loi Informatique et Libertés art. 45). Age gate / verification
  when the service targets or attracts minors.
- **Special categories** (health, biometrics, religion, politics, sexual life, union, genetic, ethnic origin; art. 9)
  and criminal data (art. 10): forbidden unless an art. 9(2) exception; DPIA likely. Look: health fields, face / voice
  recognition, free-text fields that collect them.
- **Minimisation, purpose limitation, accuracy, storage limitation, integrity** (art. 5(1)): fields not needed,
  data kept forever, data reused for another purpose. Fix: drop fields, retention periods (§5), purpose list.

## 2. Transparency and information (art. 12-14)
- A **privacy notice** given at collection (art. 13) or within a month when collected elsewhere (art. 14): identity and
  contact of the controller, DPO contact if any, purposes + bases, legitimate interests, recipients, transfers outside
  the EEA + safeguards, retention periods, the rights (§3) + how, right to withdraw consent, right to complain to the
  authority (CNIL in France), whether providing data is required and the consequences, automated decisions / profiling
  (art. 22) and their logic. Concise, plain language, easy to reach (art. 12).
- Look: a privacy page / screen in the app / docs, the forms (a short notice next to each), the emails sent.

## 3. Rights of the people (art. 15-22) - answered within **one month** (art. 12(3), +2 months if complex, told)
- **Access** (art. 15): a copy of their data + the art. 15(1) information. **Rectification** (art. 16).
- **Erasure / right to be forgotten** (art. 17): delete without undue delay when no longer needed, consent withdrawn,
  objection, unlawful processing...; tell the recipients (art. 19); exceptions (legal obligation, claims, archiving).
  Look: an account / data deletion path that really deletes (cascade to related tables, files, caches, search index,
  third-party processors, analytics, emails lists), backups expiry documented, soft-delete never left forever.
- **Restriction** (art. 18), **portability** (art. 20: a structured, machine-readable export of what they provided,
  for consent / contract bases), **objection** (art. 21: always for direct marketing), **automated decisions** (art. 22:
  human review on request when decisions with legal / similar effect).
- Identity check proportionate (art. 12(6)); free of charge; a log of requests and answers kept.
- Look: an export endpoint / button, a delete endpoint / button, an admin procedure, a contact address that works.

## 4. Accountability (art. 24-31)
- **Records of processing activities** (art. 30): purposes, categories of people and data, recipients, transfers,
  retention, security measures; for the controller and (lighter) for the processor. The < 250 employees exemption does
  not cover regular processing - in practice keep one.
- **Privacy by design and by default** (art. 25): least data collected by default, privacy-friendly defaults.
- **Processors** (art. 28): a written contract (DPA) with each processor (host, email, analytics, payment, AI API,
  error tracking, support desk) with the art. 28(3) clauses; sub-processors listed. Look: every external service in the
  code / infra (SDKs, API keys, env vars, Terraform, docker-compose) - each is a recipient to list.
- **DPO** (art. 37): mandatory for public bodies, large-scale regular monitoring or large-scale special data.
- **DPIA** (art. 35): before high-risk processing (CNIL list: large-scale profiling, biometrics, health, location
  tracking, vulnerable people, innovative tech such as AI on personal data, surveillance of employees...).
- **Joint controllers** (art. 26) when purposes are decided together (social plugins, shared analytics).
- **EU representative** (art. 27) for a non-EU controller targeting EU people.

## 5. Retention
- A **period per data category** (art. 5(1)(e), 13(2)(a)), then deletion or real anonymisation (not pseudonymisation).
  Look: TTLs, cron / purge jobs, log rotation, database tables growing forever, old accounts never purged.
- France reference durations (CNIL référentiels, ⏱): prospects 3 years after the last contact; customers: the
  contract + legal limitation periods; accounting 10 years (Code de commerce L123-22); connection logs of a host
  1 year (LCEN art. 6 + décret 2021-1362) - only for those who host / give access; cookies 13 months max (`web.md`).

## 6. Security and breaches (art. 32-34)
- **Appropriate security** (art. 32): encryption in transit (TLS) and at rest where relevant, password hashing (bcrypt /
  scrypt / argon2, never plain / MD5 / SHA1), access control, least privilege, secrets out of the repository, updates,
  backups and restore tests, logging of access to personal data, pseudonymisation where possible.
  France: CNIL recommendation on passwords (2022, ⏱) and its security guide. Look: `grep` secrets, hash functions,
  TLS config, default admin accounts, debug endpoints, CORS, dependencies with CVEs (`audit-deps`).
- **Breaches**: notify the authority within **72 h** (art. 33) unless unlikely to be a risk; tell the people when high
  risk (art. 34); a breach register kept in all cases (art. 33(5)). Fix: a written procedure + who to call.

## 7. Transfers outside the EEA (art. 44-49)
- Each recipient outside the EEA (US cloud, AI API, analytics, email, CDN): an adequacy decision (e.g. EU-US Data Privacy
  Framework since 10 July 2023 - only for **certified** US companies, ⏱ watch challenges), else SCCs (2021/914) + a
  transfer impact assessment, or an art. 49 derogation (narrow). Look: the region of each service, its DPA, its DPF entry.

## 8. Consumers, contracts, marketing (B2C, EU / France)
- **Terms** (CGU / CGV): who provides the service, what it does, price, duration, termination, liability (unfair
  terms are void: directive 93/13, Code de la consommation L212-1), applicable law; accepted explicitly.
- **Distance selling** (directive 2011/83, Code de la consommation L221-1 s.): pre-contract information (L111-1,
  L221-5), **14-day withdrawal** (L221-18) and its form, the exceptions (digital content started with express consent
  and acknowledgement of losing the right, L221-28), order confirmation on a durable medium, "order with obligation to
  pay" button (L221-14).
- **Digital content / services** (directive 2019/770, Code de la consommation L224-25-1 s.): conformity, updates,
  remedies.
- **Mediation**: a consumer mediator named and reachable (L612-1, L616-1). The EU ODR platform **closed on 20 July
  2025** (regulation 2024/3228): its link must be **removed**, not added.
- **Subscriptions**: easy termination ("résiliation en trois clics", L215-1-1, since June 2023), renewal reminders
  (L215-1).
- **Prices**: total price with taxes, price reductions shown against the lowest price of the last 30 days
  (L112-1-1 / directive 2019/2161).
- **Dark patterns / unfair practices**: directive 2005/29 (UCPD), DSA art. 25 for platforms.
- **Marketing emails / SMS**: prior opt-in for consumers (ePrivacy art. 13; Code des postes et communications
  électroniques L34-5), the soft opt-in for similar products of existing customers, an unsubscribe link in each
  message; B2B: information + opt-out. Phone canvassing: Bloctel, and in France the opt-in regime voted in 2025 ⏱.

## 9. Outside the EU (when users are there)
- **UK**: UK GDPR + DPA 2018, amended by the Data (Use and Access) Act 2025 (staged commencement ⏱); PECR for cookies
  and marketing; ICO registration fee for most controllers.
- **Switzerland**: nFADP (in force 1 Sept 2023): information duty, records (unless < 250 staff and low risk), breach
  notification to the FDPIC, transfers.
- **US**: no federal law; ~20 state comprehensive laws in force in 2026 ⏱ (California CCPA/CPRA with its "Do Not
  Sell or Share" link and Global Privacy Control, Virginia, Colorado, Connecticut, Texas, Oregon...), each with its
  thresholds (some as low as 35,000 consumers, Nebraska / Texas with almost none): check each state of the users.
  Children: COPPA (under 13, verifiable parental consent). Health: HIPAA only for covered entities; Washington
  My Health My Data Act for consumer health data.
- **Canada**: PIPEDA + Québec Law 25 (privacy officer, PIA, consent, portability). **Brazil**: LGPD (DPO, bases, ANPD).

## 10. AI (Regulation (EU) 2024/1689, AI Act - amended by the Digital Omnibus on AI, in force 27 July 2026 ⏱)
- **Prohibited practices** (art. 5, since 2 Feb 2025): manipulation, social scoring, emotion recognition at work /
  school, untargeted face scraping, some biometric categorisation and predictive policing.
- **AI literacy** of the staff using AI (art. 4, since 2 Feb 2025).
- **Transparency** (art. 50, from **2 Aug 2026**): tell people they interact with an AI (chatbots), mark AI-generated
  or manipulated content (machine-readable marking; a grace period for marking reported to 2 Dec 2026 ⏱), disclose
  deepfakes and AI text published to inform the public.
- **General-purpose AI models** (art. 53-55, since 2 Aug 2025): only for providers of the model itself.
- **High-risk systems** (Annex III: hiring, credit, education, essential services, law enforcement...): obligations
  **delayed to 2 Dec 2027** (stand-alone) and **2 Aug 2028** (embedded in regulated products) by the Omnibus ⏱.
- Personal data in prompts / training: GDPR applies on top (basis, information, DPIA, transfers to the AI provider).

## 11. Products with digital elements (Regulation (EU) 2024/2847, Cyber Resilience Act)
- Applies to software / hardware **made available on the EU market** in a commercial activity (open source outside any
  commercial activity is out; "open-source stewards" have a light regime).
- **Reporting** of actively exploited vulnerabilities and severe incidents to the CSIRT / ENISA single platform from
  **11 Sept 2026** (early warning 24 h, notification 72 h, final report 14 days / 1 month) - the date for stewards is
  disputed between sources ⏱.
- **Full requirements from 11 Dec 2027**: secure by design / default, vulnerability handling, SBOM, security updates for
  the support period, CE marking, technical documentation.
- Look: a `SECURITY.md` / security.txt contact, a vulnerability handling process, an SBOM, signed releases, update path.

## 12. Cybersecurity of entities (Directive (EU) 2022/2555, NIS2)
- Essential / important entities of the listed sectors (energy, transport, health, digital infrastructure, cloud,
  managed services, digital providers: marketplaces, search engines, social networks...) above the size thresholds.
- **France: not transposed yet in Oct 2026** (the "projet de loi Résilience" still before the Assemblée nationale ⏱;
  the Commission referred France to the CJEU in July 2026). Prepare: risk management, incident reporting, governance.

## 13. Data access and sharing (Regulation (EU) 2023/2854, Data Act - applies since 12 Sept 2025)
- Connected products / related services: users' access to the data they generate, sharing with third parties on
  request, design obligations for products placed on the market from 12 Sept 2026 ⏱.
- **Cloud / data processing services**: switching and portability (contract terms, exit, egress charges ended by
  12 Jan 2027 ⏱), international government access safeguards.

## 14. Licenses, attributions and intellectual property (run `license` and `audit-deps`, include their result)
- **The project's own license**: a `LICENSE` file, the license in the package manifests (CMake / CPack, `package.json`,
  `pyproject.toml`, `Cargo.toml`), SPDX identifiers, copyright headers where the license asks for them, the year and
  holder right. No license = all rights reserved (say it when the project is meant to be open).
- **Each dependency** (direct and transitive, vendored, static / dynamic, bundled in the binary / the JS bundle / the
  container image): its license compatible with the project's license and its distribution mode, and **what it requires
  when distributed**:
  - MIT / BSD / ISC / zlib: keep the copyright notice and the license text with the binary / bundle (an "About" /
    "Open-source licenses" screen or a `THIRD_PARTY_NOTICES` file shipped with it).
  - Apache-2.0: the license + the **NOTICE file** contents (§4(d)), changes marked, no use of their trademarks.
  - GPL-2.0 / 3.0: the whole program under the GPL when combined / distributed, the **source offer** (or the source
    with it), the license text; GPL-3.0 also installation information for user products ("anti-tivoisation").
  - LGPL: dynamic link or the means to relink, the notice, the modified library's source.
  - **AGPL-3.0**: the source offered to the **network users** of a modified version (SaaS included).
  - MPL-2.0 / EPL: file-level copyleft, the modified files' source.
  - "Source-available" / non-commercial / BUSL / SSPL / Commons Clause / custom licenses: what is forbidden (commercial
    use, SaaS, competitors) - often a blocking finding for a commercial project.
  - No license or unknown origin: not usable - replace or get a written permission.
- **Fonts** (OFL: no sale alone, the license with it; commercial fonts: the web / app / desktop license bought),
  **icons and images** (CC-BY: the attribution where shown; CC-BY-SA / ND / NC limits; stock photos' license terms;
  screenshots of others' products), **sounds, videos, texts** quoted (short quotation right, source named).
- **Datasets and AI models**: their license (ODbL share-alike, CC-BY, CC-BY-NC, "research only"), the model licenses
  and acceptable use policies (Llama, Gemma, Mistral, OpenRAIL...), the terms of the AI API used for the outputs;
  scraped data (the sites' terms, the text and data mining opt-out of the DSM directive art. 4).
- **Mandatory credits** shown in the product: an "About / Licenses / Credits" page or screen listing every
  third-party component, its license and copyright (generated: `audit-deps` THIRD_PARTY_NOTICES), map / tile providers'
  attribution (OpenStreetMap "© OpenStreetMap contributors", Mapbox, Google), APIs that require their logo / mention.
- **Trademarks**: the product name / logo not infringing (EUIPO, INPI, USPTO, WIPO search for a commercial name),
  ®/™ used correctly, others' marks only to refer to them (no implied endorsement), app store / platform brand rules.
- **Contributors**: a CLA / DCO when outside contributions are accepted; employees' work owned by the employer
  (software: Code de la propriété intellectuelle L113-9), freelancers: a written assignment of rights.

## 15. Mandatory mentions and language
- **Language** (France: loi Toubon 94-665): French is mandatory for the offer, description, instructions, warranty
  terms, invoices and receipts of goods / services offered to consumers in France, for the terms and for the documents
  given to employees; a translation may come along, never replace it. Québec: Charter of the French language as
  amended by Bill 96 (French first in commerce, contracts, product information, websites; staged dates ⏱).
  Other countries: the consumer information in the language of the market (often required by the consumer law).
- **Accessibility of the language**: the language of each document / page declared (RGAA 8.3, 8.4, 8.7: `lang` of
  the page and of the passages in another language), texts in plain language for public services.
- **Prices**: with all taxes, the unit price when required, the currency; "free" only when really free.
- **Advertising**: identified as such ("publicité", sponsored), influencers (loi 2023-451 ⏱), comparative advertising
  rules; health / financial / alcohol / gambling messages and their legal warnings.
- **Products sold online** (General Product Safety Regulation (EU) 2023/988, since 13 Dec 2024): on each product
  listing the manufacturer (name, address, electronic contact), the EU responsible person when the maker is outside the
  EU, the product identifier, the warnings and safety information in the consumer's language; recalls process.
  Electrical / electronic goods: WEEE take-back and the eco-participation shown (France), the Triman logo, the
  repairability / durability index (France, ⏱).
- **Invoices** (France): the mandatory invoice mentions (Code général des impôts 242 nonies A) and the e-invoicing
  schedule (receiving from Sept 2026, issuing by size from 2026-2027 ⏱).
- **Professional identity** on every commercial document / email: name, legal form, registration, address (and the
  regulated profession's mentions).

## 16. Sector / special cases to ask about
Health data (France: hébergement HDS, Code de la santé publique L1111-8), payments (PSD2 / PCI DSS through the payment
provider), employees monitoring (Code du travail, CNIL référentiel RH), video surveillance, biometrics, geolocation of
staff, public sector (RGAA mandatory, open data, RGS), gambling, financial services (DORA), education (minors),
export control of cryptography (EU dual-use regulation 2021/821) for some encryption products.
