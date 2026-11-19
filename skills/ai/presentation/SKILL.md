---
name: presentation
description: Make a presentation from a plan or an idea, with little text like a real talk - per slide a title saying the idea, one visual and a few key words (icons, logos, a big number, a picture beside the points, a full-slide photo, found on the web with their licence and credit), the sentences in the speaker notes; a time (given, or estimated and confirmed) sets the number of slides; the format (PDF, one-file animated HTML, PPTX, several or all) and the style (several proposals for the subject, "White blueprint" by default, or copied from a PPTX / PDF / HTML the user gives) are asked through interactive-questions. One deck.json gives every format plus speaker notes. Use for "fais une présentation / des slides / un ppt / un pdf de présentation".
metadata:
  summary: 'A presentation from a plan or an idea, with little text like a real talk (per slide a title saying the idea, one visual and a few key words: icons, logos, big numbers, a picture beside the points or over the whole slide, found on the web by `images.py` with their licence and credit; `check_deck.py` warns on sentences, long leads, text alone): the time (given, or estimated and confirmed) sets the number of slides, the format (PDF, one-file animated HTML, PPTX, several or all) and the style (proposals fitting the subject, `White blueprint` by default, or copied from a PPTX / PDF / HTML given) are asked through `interactive-questions`; one `deck.json` builds every format plus the speaker notes; checked with contact sheets and a selftest of the web page; `--canva` builds a PDF / PPTX that survives an import into Canva (one Canva font per role, no gradient background) and `check_deck.py --canva` reads the files back (text kept, fonts embedded)'
---

# Presentation (plan or idea -> slides in the formats asked)

`SKILL_DIR` = the folder of this file. One source, `deck.json`, built into an animated web page (one HTML file), a PDF
(16:9, one page per slide), a PPTX and the speaker notes (`<name>-notes.md`), all in the same style.
Everything written for the user's tools (scripts, JSON keys, file names) is English; the slides are in the language of
the request (French here), code and comments in English.

## 1. What is given
- **A plan**: the user's own outline (a file, a message): keep its order and its words, an idea / line of the plan is
  **not forced to be one slide** (one idea may take two or three slides, three small lines may share one).
- **An idea / a subject**: write the plan first (5-8 chapters, one line each) and show it before the slides.
- **A model to follow** (a PPTX / PDF / HTML / image of theirs): the style comes from it (section 3).
- Ground it: read what the subject is about (the repository, the docs, the files named), and the earlier decks of
  the user when they exist (`~/testing/ppt-tek`, `docs/`, `presentation/` folders): their vocabulary, their level,
  what they already explained. Never invent a number, a command or a source: what cannot be checked is marked
  "to check" in the notes.

## 2. Ask (ONE `interactive-questions` call, French, 3 questions)
1. **Durée** - when the user gave a time, use it. Otherwise estimate it from the plan (about **1 content slide per
   minute**, between 0.5 and 1: a talk of 20 minutes = 15 to 20 content slides, plus the title, the section dividers
   and the end) and ask whether it fits: options "`<N> min, ~<S> slides` (Recommended)", the same shorter, the same
   longer. The number of slides follows the time chosen; say it in the question.
2. **Format** - options: `PDF + HTML (Recommended)` · `PDF + HTML + PPTX (tout)` · `PPTX seulement` · a custom mix
   through *Other*. The HTML is one self-contained file with build animations (the points appear one by one), the PDF is
   the same without motion (every step shown), the PPTX is editable (no animation) and carries the notes.
   Ask in the same question whether the file will be **imported into Canva** (or Google Slides / Keynote): then
   build with `--canva` (section 5b); the user can also say it in *Other*.
3. **Style** - exactly **4 options**, in this order:
   1. `White blueprint (default)` - recommended, always first, the label ends with "(default)".
   2. One other catalogue style that fits the subject (the "when" column below), different in mood.
   3. `Sur mesure (sans template)` - a look designed from scratch for this subject and this plan: its own palette, fonts,
      frame and background drawn from the subject's world, not taken from any catalogue theme (section 3, "Custom
      look"). Say in its description what it would look like (colours, mood) in a few words.
   4. `Autre / custom` - the user gives a file to copy ("copie le style de <fichier>", a PPTX / PDF / HTML / image) or
      describes the look in words; the free-text answer is read as such.
   Use `preview` to show a mini mock-up of each look (title line, colours, font) when it helps to choose.
- Several decks asked in one go: one question call for all, not one per deck.
- Unclear subject / audience / level: ask it in the same call (header "Public").

## 3. Styles
`SKILL_DIR/templates/themes.json` (and `~/.claude/presentation/themes/*.json`, the user's own). Never hard-code a
colour in a slide: the theme is the only place of the look.

| Theme | Label | When |
|---|---|---|
| `blueprint` | White blueprint (default) | technical, code, architecture, repositories, courses: an architect's plan (off-white, fine blue grid, blue ink, title block) |
| `paper` | Clean paper | any subject, reports, school defenses: neutral, readable |
| `night-terminal` | Night terminal | system, shell, security, low level: dark, monospace, green |
| `warm-keynote` | Warm keynote | talks, pitches, ideas: cream, orange, rounded, big words |
| `slate-corporate` | Slate corporate | business case, company, formal defense: slate and deep blue |
| `neon-dark` | Neon dark | games, creative, AI: deep violet, magenta and cyan |

**Custom look (`Sur mesure`)** - the free, bold option: it is NOT limited to colours, a neon or a catalogue mood; the
subject decides everything. Do not start from a catalogue row, start from an idea (a concept taken from the subject:
a poster, a notebook, a terminal, a map, a comic page, a museum label, a blueprint of something else...) and push it:
flashy colours or quiet ones, big type, hard shadows, full-colour section pages, giant numbers, tilted cards, gradients,
patterns, drawn shapes (inline SVG in `bgimg` as `url("data:image/svg+xml,...")`), an unusual layout per kind.
Two files in the deck folder, then `--theme-file theme.json`:
- `theme.json`: every key of `themes.json` (`label`, `dark`, `bg`, `paper`, `ink`, `ink2`, `tx`, `dim`, `code`, `codetx`,
  `warn`, `ok`, `font`, `head_font`, `mono`, `head_transform`, `head_spacing`, `head_weight`, `radius`, `bgimg`, `frame`,
  `inner_frame`, `cartouche`) + `"css_file": "theme.css"` (or `"css": "..."`). **Extra colour keys** are allowed and
  become variables everywhere: `"acc1": "#0d7a55"`, `"acc2_line": "#c4561a"` give `--acc1` / `--acc2-line` in the CSS
  and are resolved in the inline SVG of the PDF (two worlds in one deck: one accent per part, set per slide with `#sN`).
- `theme.css`: free CSS loaded after the layout (HTML and PDF): override anything (`.k-title`, `.k-section`, `.col`,
  `.flow > li`, `.k-quote`, `#hud`, `@font-face`, even transitions). Class names are in `templates/slides.css` (the stage, its scaling and the outline shell: `templates/deck-base.css`).
`templates/custom-example/` is a complete bold example ("Poster pop": cobalt, coral and yellow, hard shadows) to take
the level of boldness from, not to copy. Rules that stay: text readable (contrast on every background, checked on the
sheets), fonts that exist on the machine (`fc-list`), nothing leaving the slide (the selftest says it), no
`::marker { content }` (it crashes the PDF engine: use `list-style-type`). WeasyPrint (the PDF) also: drops a whole rule
that uses `:has()`, makes an absolute `::before` of a grid container a grid cell (use flex there), and the layout rules
`.slide ul` / `.slide ol` / `.slide li` beat a bare class (`.road`): write `.slide ol.road`. Every direct child of a slide
is `position: relative` (above the decor): a block placed with `position: absolute` against the slide needs its figure
set back to `position: static` (`#s9 figure { position: static; }`). The PPTX carries the colours, fonts and
background colour of the theme only (not the CSS), the `--canva` build ignores the CSS: say it. Offer to keep the
result with the user's own themes (`~/.claude/presentation/themes/`, `theme.css` next to it).

**Copy the style of a file** the user gives (or "le style de la présentation sur le repo C++"):
```bash
python3 SKILL_DIR/scripts/extract_style.py <model.pptx|pdf|png|html|css> --name my-style [--save]
```
It prints a theme (colours and fonts read from the file). `--save` keeps it in `~/.claude/presentation/themes/` for the
next decks. It is a first guess: build the example (`templates/example.json`) with `--theme-file`, LOOK at the sheets
(section 5) and adjust the JSON (`ink`, `bg`, `font`, `bgimg`, `frame`...) until it looks like the model. Offer to
**set up** a style once (`--save`) so that it is proposed in the next questions.

## 4. Write `deck.json`
`SKILL_DIR/templates/example.json` shows every kind. Keys: `title`, `subtitle`, `lang`, `author`, `date`, `duration`,
`theme`, `slides` (+ `bg_angle`, `bg_fade` for the per slide backgrounds, `reveal`: `"click"` by default = the
points / columns / steps / `data-s` blocks appear one per click on the web page; `"auto"` = everything appears when the
slide comes in, one after the other in a short entrance, one click per slide - for a pitch or when the user does not want
to click through; the slide's own entrance animations stay; a block of an `svg` slide marked `data-click="1"`, `"2"`...
still waits for its click in both modes: the figures of a problem slide, a punchline). A slide: `kind`, `title`,
`chapter` (the bar of the web page), `lead` (one line under the title), `notes` (what is said aloud), `bg` (optional:
one colour, or two for a gradient: the slide's own background; the web page **fades** the colours from one slide to the
next instead of cutting, the PDF paints each slide's gradient, the PPTX its first colour, `--canva` a solid colour). The
slides are `#s1`, `#s2`... in the HTML / PDF: a `theme.css` can style one slide or a run of slides
(`#s2, #s3 { --ink: var(--acc1); }`).

| kind | Fields | Use |
|---|---|---|
| `title` | `title`, `subtitle`, `meta` | first slide |
| `section` | `number`, `title`, `lead` | a chapter starts |
| `bullets` | `points`: text or `{text, icon, sub}`; `src` (+ `alt`, `credit`): a picture beside the points | 3-4 key words, an icon each, shown one by one |
| `columns` | `columns`: `{head, icon, points}` | compare 2-4 things, a logo / icon each |
| `table` | `head`, `rows` | real columns of data |
| `code` | `code`, `lang`, `caption` | a command or a few lines (never a whole file) |
| `steps` | `steps`: `{head, icon, text}` | a path in 3-4 stages, revealed one by one (an icon each: no text needed) |
| `grid` | `items`: `{icon, label}` (2-8) | the logos of the tools / products / partners, one by one |
| `big` | `value` + `label` (or `items`: `{value, label}`, 2-3); `title` optional | the number to remember |
| `image` | `src` (relative path), `caption`, `alt`, `credit`; `full: true` = the picture covers the slide, the title on a band | a screenshot, a photo, a QR code |
| `svg` | `svg`, `caption` | a diagram drawn inline (`currentColor` = the text colour, `<g style="color:var(--ink)">` for the accent, no fixed colour; the PDF resolves them from the theme); HTML / PDF only. The field is raw markup: an HTML block (a mock-up, a QR card, a custom layout) works too, and its `data-s="1"`, `"2"`... are revealed one by one like the points |
| `quote` | `text`, `by` | the sentence to remember |
| `end` | `title`, `subtitle`, `links`, `src` | last slide: questions, links, QR code |
Text accepts `` `code` `` and `**bold**`. An `icon` / `src` is a file of the deck folder (`img/x.svg`, section 4b): an SVG
is put inline and takes the accent of the theme (one-colour icons and logos; an SVG with its own colours keeps them), a
PNG / JPEG stays as it is; the PPTX gets the SVG rendered to PNG (`rsvg-convert`, `inkscape` or `magick`). A slide with
icons, logos or a picture beside its points is laid out bigger and centered under its title.

Colours in inline SVG (the bugs met, the builder fixes the first, `check_deck.py` warns on the others):
- A theme colour goes in `style`, never in a presentation attribute: `style="fill:var(--paper)"`, not
  `fill="var(--paper)"` (a browser paints it black). `build_deck.py` moves `fill` / `stroke` / `stop-color` / `color`
  attributes holding a `var()` into `style`, and merges two `style` attributes on one tag (the second would be dropped).
- No `opacity` on a `<text>`: WeasyPrint cuts or shifts it in the PDF. Use a dim fill (`style="fill:var(--dim)"`).
- A point on a curve (a station on a line) is computed from the curve (`B(t)` of the cubic, or a segment end), never
  placed by eye.

How a good deck is written - **little text, like a real talk** (the user's rule: the slide shows, the speaker says):
- A content slide = a **title that says the idea** ("Un Makefile qui lance CMake", not "Makefile") + **one visual** + at
  most 3-4 key words. Every sentence (the explanation, the detail, the example told, the transition) goes in `notes`.
- A point is 1-5 words, no sentence, no final dot; 3-4 points (5 at most), one idea per slide; `sub` only when the
  point is wrong without it.
- No description unless it is useful: `lead`, `caption`, a step's `text`, a `label` are left out by default, and
  written only when they say what the title and the picture cannot (a unit, a condition, a source).
- A visual on every content slide, never two slides of text alone in a row; the content picks it:

  | The content | Slide |
  |---|---|
  | a number, a result, a gain | `big` (one figure, or 2-3 `items`) |
  | tools, languages, products, partners | `grid` of their logos |
  | two or three things compared | `columns`, a logo / icon per column |
  | a path, a process | `steps`, an icon per step, no text under |
  | a few ideas | `bullets`, an icon per point, or beside a picture (`src`) |
  | a story, a chapter, a place, a product | `image` with `full`: the photo covers the slide, the title on it |
  | how it works | `svg` (a diagram) or a screenshot (`image`) |
  | data, commands | `table`, `code` |
- The real thing first (a screenshot of the project, its logo, a photo of it), then a diagram, then a photo found
  online, then an icon. One icon set per deck (Lucide; Tabler where Lucide has none).
- Examples from the user's real projects and real commands, tested when they can be run.
- A chapter order that answers the questions of someone new, in order; a recap or a quiz slide when the talk is a course.
- The PDF can be sent without the speaker: the titles carry the message and the notes file goes with it. A document
  to read rather than a talk (a handout, a report): say that the leads come back, or that `report` fits better.
- A link worth keeping goes with a QR code image on the last slide (`image` / `end` with `src`).
- No emoji, no decoration without meaning (a picture that says nothing about the point is decoration), nothing that
  moves unless it reveals the next point.

## 4b. Pictures: icons, logos, photos from the web
`images.py` puts them in `<deck>/img/` (next to `deck.json`), checks each one (a real picture, not too big; WebP / GIF /
AVIF converted) and writes `img/CREDITS.md` (source, author, licence; the licence texts of the icon sets in `img/licenses/`):
```bash
python3 SKILL_DIR/scripts/images.py icon si:cmake si:github lucide:rocket -o <deck>/img   # logos and icons (SVG)
python3 SKILL_DIR/scripts/images.py search "rocket launch" --commercial --sheet /tmp/pick.png
python3 SKILL_DIR/scripts/images.py get openverse:<id> -o <deck>/img --name launch       # + its credit
python3 SKILL_DIR/scripts/images.py get <https address of the file> -o <deck>/img --license "CC BY 4.0" --author X --page <page>
```
- **Logos** of every tool, language, product or company named: Simple Icons first (`si:<slug>`: the name in lower
  case, `+` = `plus`, `.` = `dot`: `cplusplus`, `nodedotjs`; one colour = the theme accent, `--brand` = the official
  colour); missing there: the official logo (press kit, brand page, Wikimedia Commons; WebSearch `<name> logo svg`)
  through `get`. A logo only names what it stands for: never distorted, never shown as the deck's own brand.
- **Icons** for ideas: Lucide (`lucide:<name>`, names on lucide.dev/icons), Tabler (`tabler:<name>`) when Lucide has none.
- **Photos**: `search` (Openverse: Flickr, Wikimedia Commons, museums; English words; `--commercial` leaves out the NC
  licences, for a talk that is sold or made for a client) and LOOK at the `--sheet` (Read) to choose; or a page found
  with WebSearch whose licence is read on the page itself (Wikimedia Commons, Unsplash, Pexels, an official media kit),
  then `get` with that licence. Never a picture whose licence is unknown, never an AI picture of a real person, place
  or product shown as real.
- Read (look at) every picture downloaded before it goes in a slide: the right subject, sharp, no watermark.
- A licence that asks for a credit (CC BY, BY-SA): `"credit": "Photo: NASA, CC BY 2.0"` on the slide (`images.py`
  prints it); `img/CREDITS.md` goes with the deck.
- No network (claude.ai without the web, an offline machine): draw it (`svg`), or ask the user for their files.

## 5. Build and check
```bash
python3 SKILL_DIR/scripts/build_deck.py deck.json -o <folder> --formats html,pdf,pptx,notes [--theme NAME | --theme-file F]
python3 SKILL_DIR/scripts/check_deck.py deck.json --dir <folder> [--sheets <tmpdir>]
```
- `check_deck.py`: the pace (content slides per minute against `duration`), the little-text rules (a point of more
  than 7 words, a title of more than 9, a lead / caption / label of more than 12, more than 5 points or 40 words, sub-points,
  two slides of text alone in a row: each WARN is fixed, or said to the user when it is wanted), missing notes or
  pictures, the number of PDF pages; then **contact sheets** (PNG of every page of the PDF and of the PPTX rendered by
  LibreOffice) and the **selftest** of the web page (headless Firefox: text out of the slide, hidden by the bar, clipped,
  the keys). Open the PNGs (Read) and LOOK: a slide that overflows, a table too wide, a title on four lines, black on
  black. Fix the JSON (split the slide, shorten) and build again. Put the PNGs in /tmp, not in the folder delivered.
- The web page keys: arrows / space (next step), Home / End, `O` (outline: chapter cards, the current slide marked, close with the cross or Esc; `?map` in the address opens it), `F` (full screen), `N` (notes), `H` (hide / show the bottom bar), swipe.
- Tools: `weasyprint` (PDF), `python-pptx` (PPTX), `rsvg-convert` / `inkscape` / `magick` (SVG icons in the PPTX),
  `Pillow` (`images.py`, sheets), `firefox`, `pdftoppm`, `soffice` (checks). A missing one is said with
  its install command; ask before installing (`pip install --user python-pptx`, `sudo dnf install python3-python-pptx`).

## 5b. Import into Canva (PDF or PPTX)
Canva rebuilds a PDF / PPTX as editable elements: it cuts a text where the font changes, drops a font it does not have,
merges lines into one box, turns grouped shapes and gradients into one picture. So for a deck going to Canva:
```bash
python3 SKILL_DIR/scripts/build_deck.py deck.json -o <folder> --formats pdf,pptx --canva
python3 SKILL_DIR/scripts/check_deck.py deck.json --dir <folder> --canva
```
- `--canva` keeps ONE family per role (body, headings, code) chosen in the Canva library (`templates/canva.json`, installed
  here so that it is embedded in the PDF), no fallback fonts, no letter spacing, no gradient grid background, no
  background / padding on inline code, no auto-fit in the PPTX. The look of the theme (colours, frames, layout) stays.
- `check_deck.py --canva` reads the built files back: every string of every slide is found again as real text on its
  PDF page / in its PPTX shapes (a miss = text cut or turned into a picture), every font embedded and in the Canva list,
  no Type 3 font, no group or placeholder in the PPTX, no picture on a page that has none, no run without a font.
  An ERROR is fixed and rebuilt; a WARN is said to the user.
- Prefer the **PPTX** when the text must stay editable in Canva (it keeps one box per text and the table as a table); the PDF
  when the look matters more. `svg` slides become pictures in Canva: use `image` or `columns` / `steps` for what must be edited.
- What no script can check is how Canva itself imports it: tell the user to open it once and compare with the contact
  sheets; a font that Canva replaces anyway goes into `templates/canva.json` / out of it, then build again.

## 6. Deliver
- Folder: the one the user names, otherwise `presentation/<name>/` at the root of the current repository (or of the
  current folder): `deck.json`, `<name>.html`, `<name>.pdf`, `<name>.pptx`, `<name>-notes.md`, `img/` (the pictures and
  `CREDITS.md`).
- Tell in French: the files, the time and number of slides, the style, the pictures taken from the web and their
  licences (the credits put on the slides), what was not verified (a command not run, a figure to check), and what the PPTX does not carry (animations, inline drawings: give an `image` for those), and for a Canva import that the
  import itself was not tried.
- A change asked afterwards ("plus court", "autre style"): edit `deck.json`, rebuild, no rewrite.
