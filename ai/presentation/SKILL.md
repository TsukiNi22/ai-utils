---
name: presentation
description: Make a presentation from a plan or an idea - a time (given, or estimated and confirmed) sets the number of slides; the format (PDF, one-file animated HTML, PPTX, several or all) and the style (several proposals for the subject, "White blueprint" by default, or copied from a PPTX / PDF / HTML the user gives) are asked through interactive-questions. One deck.json gives every format plus speaker notes. Use for "fais une présentation / des slides / un ppt / un pdf de présentation".
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

**Custom look (`Sur mesure`)**: do not start from a catalogue row. Read the subject, then write a complete theme file
`<deck folder>/theme.json` with every key of `themes.json` (`label`, `dark`, `bg`, `paper`, `ink`, `ink2`, `tx`, `dim`,
`code`, `codetx`, `warn`, `ok`, `font`, `head_font`, `mono`, `head_transform`, `head_spacing`, `head_weight`, `radius`,
`bgimg`, `frame`, `inner_frame`, `cartouche`): a palette that belongs to the subject (contrast checked: text on `bg` and
on `ink`), fonts that exist on the machine (`fc-list`), a frame / background only when it means something. Build with
`--theme-file theme.json`, LOOK at the sheets (section 5) and adjust; offer to keep it with the other user themes
(`~/.claude/presentation/themes/`).

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
`theme`, `slides`. A slide: `kind`, `title`, `chapter` (the bar of the web page), `lead` (one line under the title),
`notes` (what is said aloud).

| kind | Fields | Use |
|---|---|---|
| `title` | `title`, `subtitle`, `meta` | first slide |
| `section` | `number`, `title`, `lead` | a chapter starts |
| `bullets` | `points`: text or `{text, sub}` | 3-5 short points, shown one by one |
| `columns` | `columns`: `{head, points}` | compare 2-4 things |
| `table` | `head`, `rows` | real columns of data |
| `code` | `code`, `lang`, `caption` | a command or a few lines (never a whole file) |
| `steps` | `steps`: `{head, text}` | a path in 3-4 stages, revealed one by one |
| `image` | `src` (relative path), `caption`, `alt` | a screenshot, a photo, a QR code |
| `svg` | `svg`, `caption` | a diagram drawn inline (`currentColor`, no fixed colour); HTML / PDF only |
| `quote` | `text`, `by` | the sentence to remember |
| `end` | `title`, `subtitle`, `links`, `src` | last slide: questions, links, QR code |
Text accepts `` `code` `` and `**bold**`.

How a good deck is written (taken from the user's earlier decks and their remarks):
- **Less text, more visual**: a diagram, a table, steps, a command instead of a paragraph; what is said aloud goes in
  `notes`, not on the slide. At most 5-6 points per slide, 60 words, one idea per slide.
- Titles say the idea ("Un Makefile qui lance CMake"), not the topic ("Makefile").
- Examples from the user's real projects and real commands, tested when they can be run.
- A chapter order that answers the questions of someone new, in order; a recap or a quiz slide when the talk is a course.
- The PDF is read without the speaker: it must be understandable alone, the HTML is for presenting.
- A link worth keeping goes with a QR code image on the last slide (`image` / `end` with `src`).
- No emoji, no decoration without meaning, nothing that moves unless it reveals the next point.

## 5. Build and check
```bash
python3 SKILL_DIR/scripts/build_deck.py deck.json -o <folder> --formats html,pdf,pptx,notes [--theme NAME | --theme-file F]
python3 SKILL_DIR/scripts/check_deck.py deck.json --dir <folder> [--sheets <tmpdir>]
```
- `check_deck.py`: the pace (content slides per minute against `duration`), too many points / words, missing notes or
  images, the number of PDF pages; then **contact sheets** (PNG of every page of the PDF and of the PPTX rendered by
  LibreOffice) and the **selftest** of the web page (headless Firefox: text out of the slide, hidden by the bar, clipped,
  the keys). Open the PNGs (Read) and LOOK: a slide that overflows, a table too wide, a title on four lines, black on
  black. Fix the JSON (split the slide, shorten) and build again. Put the PNGs in /tmp, not in the folder delivered.
- The web page keys: arrows / space (next step), Home / End, `O` (outline: chapter cards, the current slide marked, close with the cross or Esc; `?map` in the address opens it), `F` (full screen), `N` (notes), swipe.
- Tools: `weasyprint` (PDF), `python-pptx` (PPTX), `firefox`, `pdftoppm`, `soffice` (checks). A missing one is said with
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
  current folder): `deck.json`, `<name>.html`, `<name>.pdf`, `<name>.pptx`, `<name>-notes.md`, the images used.
- Tell in French: the files, the time and number of slides, the style, what was not verified (a command not run, a
  figure to check), and what the PPTX does not carry (animations, inline drawings: give an `image` for those), and for a Canva import that the
  import itself was not tried.
- A change asked afterwards ("plus court", "autre style"): edit `deck.json`, rebuild, no rewrite.
