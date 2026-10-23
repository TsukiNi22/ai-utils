---
name: multilingual-project
description: Make any project multilingual, not only websites (desktop / terminal / mobile apps, CLIs, libraries, services: Qt Linguist, gettext, Babel, Fluent, Android / iOS, Electron): the languages asked first through the interactive questions (a few, the main European ones, the most spoken, as many as possible, RTL or not), every user string in standard catalogs with named placeholders, plurals and gender, locale formats (dates, numbers, currencies, sorting), a language setting then the system locale with fallbacks, text expansion, RTL and fonts handled, translations drafted and marked for review, missing / unused keys checked in the CI, docs and legal texts in the languages. Use whenever the user wants a project translated, several languages, i18n / l10n, "porter en plusieurs langues", "traduire l'app", a language setting; for a website or web app use multilingual-website (it builds on this one).
---

# i18n - a project in several languages

**Request given with the skill:** the text after `/multilingual-project` if any. A website / web app: load `multilingual-website` too (its web
part: URLs, hreflang, SEO...); this skill gives the common rules.

## 1. Ask (skill `interactive-questions`, one or two rounds)
- **Languages** (multi choice + Other): the ones already there kept; proposals by sets - a few (English + French),
  the main European ones (en, fr, de, es, it, pt, nl, pl), the most spoken in the world (en, zh-Hans, hi, es, fr, ar,
  bn, pt-BR, ru, ja, de, ko, tr, vi, id), **as many as possible** (30+: the sets above + sv, da, nb, fi, cs, el, he,
  uk, ro, hu, th, ms, fa...). Right-to-left (ar, he, fa, ur) asked explicitly: it changes the layout.
- The **source language** of the strings (English recommended for the keys / code) and the **default** for users.
- **Who translates**: drafted here by the AI and marked "to review" (recommended to start), a translator / a platform
  (Weblate, Crowdin, Transifex, POEditor: the catalog format follows it), the community.
- **Regional variants** (fr-FR / fr-CA, pt-PT / pt-BR, en-US / en-GB, zh-Hans / zh-Hant) and what is localised
  besides the text (dates, numbers, currency, units, sort order, images with text, legal pages).

## 2. Internationalise the code (once, before any translation)
- **Every user-visible string** goes to the catalog: UI, messages, errors shown to users, notifications, emails,
  help, generated files the user reads. **Not**: logs meant for developers, identifiers, config keys, CLI flags,
  protocol values, file formats (say it in the recap).
- The standard tool of the stack: Qt `tr()` / `QT_TR_NOOP` + `.ts` (lupdate / lrelease) · gettext `_()` + `.po`
  (xgettext, msgfmt; C++: libintl or boost.locale) · Python `gettext` / Babel (`pybabel extract / update / compile`) ·
  Rust Fluent (`.ftl`) or `rust-i18n` · Android `res/values-xx/strings.xml` · iOS String Catalogs (`.xcstrings`) ·
  JS / Electron: i18next / ICU MessageFormat JSON · Java `ResourceBundle` · Go `x/text` / go-i18n.
- **No concatenation** of translated pieces: one message with **named placeholders** (`{count} files in {folder}`),
  the order can change per language. Placeholders documented for translators (comments / context: `pgettext`,
  `QCoreApplication::translate` disambiguation, ICU `description`).
- **Plurals** with the plural rules of each language (gettext `ngettext` + `Plural-Forms`, ICU `plural`, Qt `%n`):
  Polish / Russian / Arabic have 3-6 forms; **gender / select** where it changes the words.
- **Formats from the locale**, never by hand: dates and times (and time zones), numbers (decimal / thousands
  separators), currencies (symbol position), percentages, units, list joining ("a, b and c"), sorting / collation
  (ICU, `std::locale`, `Intl.Collator`, `locale.strxfrm`), case changes (Turkish dotted i).
- **Layout**: room for +30-50 % longer texts (German, Finnish), no fixed widths around text, wrapping allowed; RTL:
  mirrored layout and icons with a direction, logical start / end instead of left / right (Qt `setLayoutDirection`,
  Android `start` / `end`, `supportsRtl`), bidirectional text isolated.
- **Fonts** covering every script chosen (Noto families: CJK, Arabic, Devanagari, Thai, Hebrew...), fallbacks set.
- **Encoding**: UTF-8 everywhere (files, terminals, database, APIs); width of CJK / emoji in terminals (wcwidth).
- **Choosing the language**: a setting (persisted) > the system / user locale (`LANG`, `QLocale::system()`, Android
  per-app language, `Accept-Language` on a server) > the default; a **fallback chain** (fr-CA → fr → en) so a missing
  string never shows a key or a blank; switching without restart when the toolkit allows it.
- Pseudo-localisation for tests (accents, +40 % length, brackets: `[Ŝéţţîñĝš ___]`) to find hard-coded / clipped text.

## 3. Translate
- Translations drafted for each language from the source strings, with the context comments, a **glossary** of the
  product terms (kept consistent, product names not translated), the tone of each language (tu / vous, du / Sie),
  and each entry marked fuzzy / "needs review" when drafted by AI (gettext `#, fuzzy`, Qt `type="unfinished"`,
  i18next a review flag in the platform). Say clearly which languages need a native review before release.
- Never machine-translate legal texts silently: the legal pages in the languages of the market are a `legal-setup`
  matter (France: French mandatory for consumers - loi Toubon).

## 4. Check and keep it alive
- A check in the tests / CI (`ci-cd`): no missing key per language, no unused key, placeholders identical in every
  translation, catalogs compiled, no untranslated string left in the code (grep / the extractor's report).
- The docs: the README states the languages and how to add one (where the catalogs are, the extract / compile
  commands, the review flow); a user guide in the languages when asked.
- Recap: languages added (reviewed / drafted), what is not translated and why, how to add a language, the commands.
