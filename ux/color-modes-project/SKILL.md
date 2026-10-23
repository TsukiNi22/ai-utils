---
name: color-modes-project
description: Add visual modes to any project, not only websites (desktop, terminal, mobile, CLI) - light / dark / follow the system, and on request high contrast, sepia, a custom accent or several palettes - asked first through the interactive questions (which modes, the default, where the switch lives, a brand palette), built on semantic colour tokens (background, surface, text, muted, accent, border, ok / warn / error, focus...) instead of hard-coded colours, following the system scheme live (Qt 6.5+ colorScheme, GTK / portal, Windows, macOS, Android DayNight, iOS, Electron nativeTheme, terminals: the background detected or chosen), the choice persisted and switched without restart, every pair of colours checked against WCAG contrast in each mode (4.5:1 text, 3:1 UI), icons / images / charts / syntax highlighting / focus rings adapted, screenshots of each mode in the tests. Use whenever the user wants a dark mode, a light / dark switch, themes, "mode sombre / clair", high contrast, palettes; for a website or web app use color-modes-website (it builds on this one).
---

# theming - light / dark and other visual modes

**Request given with the skill:** the text after `/color-modes-project` if any. A website / web app: load `color-modes-website` too.

## 1. Ask (skill `interactive-questions`, one round)
- **Modes** (multi choice + Other): light, dark, **follow the system** (recommended as default), high contrast,
  sepia / warm, dimmed, a custom accent colour, several named palettes (Nord, Solarized...).
- The **default** (system recommended), **where the switch lives** (settings, a toolbar / header button, a shortcut,
  a CLI flag / env var / config key), a brand palette to start from (or derive one: `previews` of palettes in the popup).

## 2. Tokens first
- Replace every hard-coded colour by **semantic tokens**: `bg`, `surface` (cards, panels), `surface-2`, `text`,
  `muted`, `border`, `accent` + `on-accent`, `ok` / `warn` / `error` / `info` (+ their soft backgrounds), `focus`,
  `selection`, `shadow`, `link`, `code-bg`; components use only tokens. One table of values per mode, in one place
  (a style sheet, a theme file, a palette struct, `themes/*.toml`...).
- **Dark mode is not inverted light**: dark grey surfaces (not pure #000 except an OLED "black" mode), lighter
  surfaces for elevation instead of shadows, desaturated accents, text not pure white on large areas (~#e8e8e8).
- **Contrast in every mode** (WCAG 2.1 AA): 4.5:1 for text, 3:1 for large text and UI parts (borders of fields,
  icons, focus ring); a high-contrast mode reaching 7:1. Check every token pair with a script (WCAG relative
  luminance), not by eye; status not by colour alone (an icon / a word too).

## 3. Follow the system, switch live, remember
- The toolkit's signal: Qt 6.5+ `QGuiApplication::styleHints()->colorScheme()` + `colorSchemeChanged` (palette or
  style sheet rebuilt), GTK `gtk-application-prefer-dark-theme` / the XDG portal `org.freedesktop.appearance
  color-scheme`, Windows `AppsUseLightTheme`, macOS `NSAppearance`, Android `Theme.Material3.DayNight` + `values-night`,
  iOS `traitCollection.userInterfaceStyle`, Flutter `ThemeMode.system`, Electron `nativeTheme`.
- **Terminals** (ftxui, curses, CLI colours): ask the background with OSC 11 when the terminal answers, else
  `COLORFGBG`, else a setting; colours from the 16 ANSI ones (the user's terminal theme) or true colour palettes per
  mode; respect `NO_COLOR` / `--color=never` and non-TTY output.
- The setting: `system | light | dark | <others>`, **persisted** (config file / preferences), applied **without
  restart**, the same for every window of the app; a CLI flag / env var for one run when it fits the project.

## 4. Everything follows
- Icons: monochrome SVG drawn with the text / accent token (or two variants); images and logos with a transparent
  background or a variant per mode; screenshots in docs per mode when they matter.
- Charts and graphs: a palette per mode with the same contrast checks; syntax highlighting: a theme per mode;
  terminal / code views, scrollbars, selection, tooltips, menus, dialogs, splash screens, notifications.
- Transitions between modes short or none (reduced motion respected).

## 5. Check
- Screenshots of the main screens in **each mode** (offscreen / headless when possible) looked at, the contrast
  script passing, switching live works, the system change followed, the saved choice restored at start.
- Recap: modes added, where the tokens are, how to add a palette, the setting / flag names.
