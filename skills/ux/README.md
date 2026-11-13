# UX skills

The interface of a project or a website: several languages, and the colour modes (light / dark / system, high
contrast). The `-website` skills are the web part of the `-project` ones.

## Skills
<!-- generated:skills -->
| Skill | What it does | Installs with it |
|---|---|---|
| [`color-modes-project`](color-modes-project/SKILL.md) | Light / dark / system (and high contrast, sepia, palettes, an accent) for any project: the modes asked, semantic colour tokens, WCAG contrast checked in each mode, the system scheme followed live (Qt, GTK portal, Windows, macOS, Android, iOS, terminals), persisted and switched without restart, icons / charts / highlighting adapted. | [`interactive-questions`](../ai/interactive-questions/SKILL.md) |
| [`color-modes-website`](color-modes-website/SKILL.md) | The web part of `color-modes-project`: CSS variables per mode, `prefers-color-scheme` + a system / light / dark switch on `data-theme`, no flash (inline head script), `color-scheme` / `theme-color`, `light-dark()`, `prefers-contrast` / `forced-colors` / reduced motion, images and charts per mode, Tailwind / component libraries, an accessible sun / moon toggle. | [`color-modes-project`](../ux/color-modes-project/SKILL.md), [`html-style`](../docs/html-style/SKILL.md) |
| [`multilingual-project`](multilingual-project/SKILL.md) | Any project in several languages: the languages asked (a few, Europe, the most spoken, as many as possible, RTL), every user string in standard catalogs (Qt `.ts`, gettext `.po`, Fluent, Android / iOS, ICU JSON) with named placeholders, plurals, locale formats, RTL and fonts, the language setting + system locale + fallbacks, AI-drafted translations marked for review, missing / unused keys checked in the CI. | [`interactive-questions`](../ai/interactive-questions/SKILL.md), [`legal-setup`](../legal/legal-setup/SKILL.md), [`ci-cd`](../git/ci-cd/SKILL.md) |
| [`multilingual-website`](multilingual-website/SKILL.md) | The web part of `multilingual-project`: the framework's library, `/fr/` URLs and translated slugs, hreflang + x-default, sitemaps, `lang` / `dir`, a switcher with the languages written in themselves (no flags), Accept-Language suggested never forced, SEO meta, emails in the user's language, legal pages per language. | [`multilingual-project`](../ux/multilingual-project/SKILL.md), [`html-style`](../docs/html-style/SKILL.md) |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## Examples
- *make the app available in French, German and Arabic*
- *add a dark mode that follows the system to the website*

[All the categories](../README.md) · [Installation](../../INSTALL.md)
