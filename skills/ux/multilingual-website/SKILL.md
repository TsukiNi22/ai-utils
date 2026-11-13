---
name: multilingual-website
description: Make a website or web app multilingual - on top of the common rules of multilingual-project (languages asked through the interactive questions, catalogs, placeholders, plurals, Intl formats, RTL, review flags) - the web part - the framework's i18n library (i18next / react-i18next, next-intl, vue-i18n, Angular i18n, SvelteKit, Astro, Django / Flask-Babel, Rails, Laravel, plain JS with Intl and JSON catalogs), URLs per language (/fr/ prefix, subdomains or domains), translated slugs, hreflang + x-default, sitemaps, html lang and dir, the language switcher (names in their own language, no flags, accessible), the first choice from Accept-Language without forced redirects, the choice remembered, SEO and social meta per language, emails and notifications in the user's language, server rendering and caching (Vary), lazy-loaded catalogs, and the legal pages in each language. Use whenever a site / web app must exist in several languages, "site multilingue", "traduire le site", hreflang, a language switcher.
metadata:
  summary: 'The web part of `multilingual-project`: the framework''s library, `/fr/` URLs and translated slugs, hreflang + x-default, sitemaps, `lang` / `dir`, a switcher with the languages written in themselves (no flags), Accept-Language suggested never forced, SEO meta, emails in the user''s language, legal pages per language.'
---

# i18n-web - a website / web app in several languages

Load **`multilingual-project` first** (its questions and common rules apply as they are); this skill adds the web.

## 1. Ask (with the questions of `multilingual-project`, same round when possible)
- **URL strategy**: a path prefix per language `/fr/...` (recommended: one domain, easy SEO), subdomains
  `fr.example.com`, or domains per country `example.fr` (when the offer differs per country); the default language
  at the root or prefixed too; **translated slugs** or not.
- The framework already used (detected from the project; asked only if several are possible).

## 2. Build it
- **Library of the stack**: Next.js (`next-intl`, App Router `[locale]` segment + middleware), React (`react-i18next`
  / `react-intl`), Vue / Nuxt (`vue-i18n` / `@nuxtjs/i18n`), Angular (`@angular/localize` or `ngx-translate`),
  SvelteKit (`paraglide` / `svelte-i18n`), Astro (built-in i18n routing), Django (`gettext` + `LocaleMiddleware` +
  `i18n_patterns`), Flask (`Flask-Babel`), Rails (`I18n`), Laravel (`lang/`), static sites (one folder per language or
  a generator's i18n). Plain JS: JSON catalogs + `Intl.MessageFormat`-style formatting + `Intl.DateTimeFormat`,
  `Intl.NumberFormat`, `Intl.PluralRules`, `Intl.RelativeTimeFormat`, `Intl.ListFormat`, `Intl.Collator`.
- **Every page**: `<html lang="fr">` (and `dir="rtl"` for RTL), `lang` on passages in another language (RGAA 8.3 /
  8.7), translated `<title>` / meta description / Open Graph / Twitter tags, `og:locale` + `og:locale:alternate`.
- **SEO**: `<link rel="alternate" hreflang="xx" href="...">` for **every** language of the page, itself included,
  plus `hreflang="x-default"`; the same in the sitemap (`xhtml:link`) or one sitemap per language; canonical URLs
  per language (never all pointing to one language); no content hidden behind a script-only switch.
- **First visit**: suggest the language from `Accept-Language` (a dismissible banner or a one-time redirect from the
  root only) - **never force** by IP / geolocation, never trap a user or a crawler in a language; every URL serves its
  language whatever the browser says.
- **Remember the choice**: a `lang` cookie / localStorage (a preference cookie the user asked for: exempt from consent
  under the CNIL rules) and the account's language on the server for emails.
- **Switcher**: in the header / footer of every page, each language **written in itself** (Deutsch, Français, 日本語),
  `lang` and `hreflang` on each link, keyboard and screen-reader friendly, keeps the same page (its translated URL);
  **no flags** for languages (a flag is a country).
- **Layout**: CSS logical properties (`margin-inline-start`, `padding-inline`, `inset-inline-end`, `text-align: start`)
  so RTL works, `:dir(rtl)` for mirrored icons, fonts per script (`unicode-range` / `font-family` stacks, self-hosted).
- **A longer language stays readable and inside its box** (German, Finnish, French: +30-50 %, a short label up to 3 times): no fixed
  `width` / `height` around a text (`min-height`, padding, `max-width`), a button, a tab, a badge, a table head or a menu grows or wraps; a row of
  them `flex-wrap`; `overflow-wrap: break-word` on the text column, `hyphens: auto` with the right `lang` on running text only (`p`, `li`: in a table or a
  button it squeezes the columns and cuts the words); no `white-space: nowrap` on a sentence, no `overflow: hidden` without `text-overflow`; a top bar that holds its controls at 375px in the longest language (the
  brand gives way first); tables scroll in their own box, never the page.
- **What a translation switch does not reach**: the attributes (`title`, `aria-label`, `placeholder`, `alt`, a button `value`), `<title>`; an `<option>` holds
  text only (one per language, or filled by the script); in an SVG, one `<text lang="...">` per language with the longest one checked inside its box, never
  an HTML element (`<span>` in a `<text>`: the browser leaves the drawing there, all that follows falls out of it).
- **Server and cache**: server-side rendering per language, `Vary: Accept-Language` only where the response depends on
  it (better: the language in the URL), CDN cache keys per language, catalogs split and lazy-loaded per route /
  language, the default language bundled.
- **Forms and data**: validation messages translated, addresses / phone numbers / names in international formats
  (no "first name + last name" assumption), time zones of the user, currencies and prices per market (taxes).
- **Emails, notifications, PDFs**: in the user's saved language, not the browser's of the moment.
- **Legal pages** in each language of the market (`legal-setup`; France: French mandatory for consumers), the cookie
  banner translated.

## 3. Check
- Every route reachable in every language, no key or blank shown (fallback chain), hreflang validated (reciprocal,
  `x-default`), `lang` / `dir` right, RTL screens checked, the switcher keeps the page, Lighthouse SEO + an
  accessibility pass on two languages, the CI check of `multilingual-project` (missing / unused keys, placeholders).
- What is really drawn in each language: `html-style/scripts/check_render.py <page.html> [--shots DIR]`, 0 error (a text past its box, the page
  scrolling sideways, a word per line, at 1280, 768 and 375px, in both themes): one file per language passed together
  (`check_render.py en/page.html de/page.html`; with `--shots` one run and one folder per language, a screenshot takes the name of its file), a
  page with the EN / FR switch of `html-style` measured in both by itself; the longest language looked at on a phone, the pseudo-locale of
  `multilingual-project` when a translation is not there yet.
- Recap as in `multilingual-project` + the URL map per language.
