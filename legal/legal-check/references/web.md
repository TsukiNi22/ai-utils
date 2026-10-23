# Legal checklist - websites and web apps (on top of `global.md`)

Same format: **requirement** · *basis* · when · what to look for · fix. ⏱: check the date / status at each audit.

## 1. Cookies and other trackers (ePrivacy directive 2002/58 art. 5(3); France: loi Informatique et Libertés art. 82;
## CNIL guidelines and recommendation of 17 Sept 2020, amended, with a CNIL Q&A of 29 April 2026 ⏱)
- **Consent before** any non-essential cookie / local storage / pixel / fingerprinting / SDK reads or writes on the
  device. Exempt only what is strictly necessary for the service asked (session, cart, load balancing, security,
  the consent choice itself) and **audience measurement configured under the CNIL exemption conditions** (first-party,
  strictly audience measurement, no cross-site tracking or combination, IP truncated, cookie ≤ 13 months without
  automatic extension, data ≤ 25 months, the user informed and able to object; e.g. Matomo set up for it).
- The banner: **"Refuse all" as easy and visible as "Accept all"** (same level, same effort; CNIL sanctions 2021-2022),
  no pre-ticked box, purposes listed and granular, the list of the trackers and their partners, a way to **withdraw**
  at any time as easily (a persistent link "Cookies" / "Gérer mes cookies"), the refusal remembered (CNIL: ask again
  after ~6 months, not at each visit), consent proof kept (date, version, choices).
- Continuing to browse / scrolling is **not** consent. Cookie walls: only under strict conditions (a real alternative).
- Look: open the site with an empty profile and the devtools (Application › Cookies / Storage, Network): what is set
  **before** any click; the scripts in the HTML (Google Analytics / Tag Manager, Meta Pixel, Hotjar, LinkedIn Insight,
  TikTok, YouTube / Vimeo embeds, Google Maps, reCAPTCHA, Intercom / Crisp chat, Stripe.js, Sentry, fonts). In the code:
  `document.cookie`, `localStorage`, `gtag(`, `fbq(`, `_ga`, third-party `<script src>` / `<iframe>`.
- **Embeds** (YouTube, maps, social buttons): load them only after consent or through a click-to-load placeholder
  (use youtube-nocookie + no autoplay; social buttons as plain links).
- **Fonts / CDNs**: Google Fonts loaded from Google send the IP to the US (LG München, 20 Jan 2022) - self-host fonts;
  prefer self-hosted scripts.
- Pending: the Digital Omnibus would move cookie rules into the GDPR (art. 88a / 88b) - **still a proposal** ⏱.

## 2. Legal notice ("mentions légales", France: LCEN 2004-575 art. 6-III and 1-1 ⏱)
- **Professionals**: company name, legal form, address, share capital, RCS / RM number, intra-EU VAT number, phone and
  email, publication director, the **host** (name, address, phone). Regulated professions: their order, rules, title.
- **Individuals (non professional)**: may stay anonymous to the public but must give their identity to the host; the
  host's details are still shown.
- Reachable from every page (footer). Penalty: up to 1 year and €75,000 (individual) / €375,000 (company).
- Other EU countries: the e-commerce directive 2000/31 art. 5 information (Impressum in Germany, TMG/DDG §5).

## 3. Privacy notice and user rights on the site
- A privacy page (`global.md` §2) linked from every page and from **each form** (a short notice under the form:
  who, why, basis, link). Forms collect only what is needed; optional fields marked; no pre-ticked newsletter box.
- **Account deletion from inside the account** (and data export) when accounts exist; the right to erasure works for
  visitors without account through a contact / form (`global.md` §3).
- Contact forms / newsletter: double opt-in recommended; consent proof stored; unsubscribe link in each email.
- Logs (server, CDN, WAF, analytics): IPs are personal data - retention period, truncation, documented.
- Captcha: reCAPTCHA = Google tracker (consent / alternative such as a self-hosted or privacy-friendly captcha).

## 4. Accessibility
- **France - public sector and companies with a turnover > €250 M** (loi 2005-102 art. 47, décret 2019-768 ⏱): RGAA
  (4.1.2, the French reference for WCAG 2.1 AA), an **accessibility statement** ("déclaration d'accessibilité":
  compliance level, non-compliant content, derogations, feedback contact, the Défenseur des droits as remedy), a
  multi-year plan (schéma pluriannuel, 3 years) and yearly action plans, the mention "Accessibilité : non / partiellement
  / totalement conforme" on the home page. Penalty up to €50,000 per service (renewable).
- **European Accessibility Act** (directive 2019/882; France: loi 2023-171 art. 16, décret 2023-931, arrêtés of
  9 Oct 2023), **since 28 June 2025** for consumer services, including **e-commerce**, banking, e-books, electronic
  communications, transport ticketing: the service accessible (EN 301 549 / WCAG 2.1 AA in practice), information on
  how it meets the requirements (in the terms or the site), a feedback channel. **Micro-enterprises providing
  services are exempt** (< 10 staff and ≤ €2 M turnover or balance sheet). Disproportionate burden (art. 14) only with
  a documented assessment, renewed when the service changes. Control: DGCCRF (services) ⏱.
- **Language of the pages**: `<html lang="..">` right on every page (RGAA 8.3 / 8.4), `lang` on the passages in
  another language (8.7), the language switcher accessible and labelled in each language; legal pages and consumer
  information in the language of the market (France: loi Toubon, `global.md` §15).
- Look: run an automatic check (axe-core / Lighthouse / pa11y) **and** a manual pass: keyboard only, focus visible,
  alt texts, contrasts (4.5:1), labels on fields, headings order, language of the page, zoom 200 %, no information by
  colour alone, captions on videos, error messages tied to fields, skip link, ARIA only when needed.

## 5. Terms, e-commerce, platforms
- **Terms of use** (CGU) when there are accounts / user content; **terms of sale** (CGV) when selling (`global.md` §8):
  accepted by an active tick / click, archived version kept, changes notified.
- E-commerce: pre-contract information on the order page, the total price, delivery, withdrawal form, "order with
  obligation to pay" button, confirmation email, invoices; the ODR link removed (platform closed 20 July 2025).
- **User-generated content / hosting** (Digital Services Act, regulation 2022/2065, fully applicable since 17 Feb 2024):
  a single point of contact (art. 11-12), terms that explain moderation (art. 14), **notice-and-action** to report
  illegal content (art. 16), a statement of reasons for each moderation decision (art. 17); online platforms: complaint
  system (art. 20), no dark patterns (art. 25), ads transparency (art. 26), no profiling ads to minors (art. 28).
  Micro / small enterprises are exempt from part of the platform obligations (art. 19). France: LCEN host liability +
  the "loi SREN" 2024 ⏱.
- Reviews shown: how they are checked (Code de la consommation L111-7-2, directive 2019/2161).
- Influencers / sponsored content: the "loi influenceurs" 2023-451 ⏱ (clear "publicité" mention).

## 6. Security of the site (GDPR art. 32 applied)
- HTTPS everywhere, HSTS, secure cookies (`Secure`, `HttpOnly`, `SameSite`), CSP, no mixed content, passwords hashed,
  rate limiting on login, no personal data in URLs / logs / analytics, backups, admin behind 2FA, dependencies updated.
- A `/.well-known/security.txt` (RFC 9116) with a contact - and the CRA reporting duty for products (`global.md` §11).

## 7. Emails sent by the site
- Transactional emails: no marketing inside without consent; sender identified; the processor (SendGrid, Brevo, SES...)
  under a DPA and its transfers covered.
- Marketing: opt-in, unsubscribe in one click (also Gmail / Yahoo bulk sender rules since 2024: List-Unsubscribe,
  SPF / DKIM / DMARC).

## 8. Credits and open-source notices on the site
- A **"Credits / Open-source licenses"** page (or a section of the legal notice) listing the libraries bundled in the
  front (the JS / CSS bundle keeps their license comments or ships a `licenses.txt`: bundlers strip them unless told,
  e.g. `license-webpack-plugin`, `rollup-plugin-license`, Vite `build.rollupOptions` + a generated notices file), the
  fonts (OFL text), icons (CC-BY attribution, Font Awesome free terms), images, maps ("© OpenStreetMap contributors"),
  and the AGPL source link when an AGPL component runs on the server (`global.md` §14).

## 9. Domain, brand, content
- The domain holder's details at the registrar (accurate, privacy proxy allowed), no trademark infringement in the
  domain / name; images, fonts, icons, videos, texts licensed (`global.md` §14); stock photos' licenses kept.
- Links to other sites: no hosting of illegal content; embeds respect the rights holder's terms.
