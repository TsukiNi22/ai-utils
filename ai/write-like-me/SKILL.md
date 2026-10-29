---
name: write-like-me
description: Write as Tsukini writes - his own voice, measured on what he typed himself (his prompts in the claude-cluster histories and the Claude Code transcripts, his .info notes): French mixed with English tech words, lowercase, few accents, imperative requests chained with "et", "a/b/c" alternatives, "ou autre", "ect...", asides in parentheses, "->" for a consequence, "???" / "!!!" for emphasis; his notes as short todo lines. Two registers (a message / a request, notes / a todo list), his typos optional; a script refreshes a sample of his real writing from this computer when more fidelity is needed. Use whenever the user asks to write like him / in his style / "comme moi", "écris comme si c'était moi", "avec mes mots", a message, a prompt, a note, a todo or a comment in his voice, or summons /write-like-me.
---

# Write like me (Tsukini's own voice)

Measured on what **he typed himself** - about 600 of his prompts (claude-cluster histories, Claude Code transcripts) and
his `.info` notes - never on texts an AI wrote for him (the long English prompts of sub-sessions, the READMEs and docs:
those are not his voice).

**Text given with the skill:** what to write after `/write-like-me` (a message, a request, a note, a reply), else the
latest request. Not said what kind: a message (register 1).

## Register 1: a message, a request, a reply (his chat voice)
- **French**, with the English words of the trade left in English: check, fix, setup, remove, push, commit, rework,
  display, build, run, popup, setting, workflow, sub session, branch (`check que ca run bien`, `fix ce probleme`,
  `remove le c++20`).
- **Lowercase** start, almost **no accents** (`cela`, `deja`, `meme`, `etre`, `probleme`, `a` for à), few capitals (only
  names, acronyms: APK, CLI, MCP, PDF).
- **Imperative and direct**, the verb first: `fait en sorte que ...`, `rajoute ...`, `regarde ...`, `check que ...`,
  `continue ...`, `essaye de ...`, `remove ...`, `avoir ...` / `pouvoir ...` for a wish (`pouvoir aussi ...`). Questions
  short and open: `pourquoi ... ?`, `ou en est ... ?`, `tu peut ... ?`, `est ce que ... ?`.
- **Long chained sentences** rather than several short ones: ideas added with `et`, `et aussi`, `ou`, `pour que cela soit
  ...`, `en fonction de ...`, `en te basant sur ...`, `quitte a ce que ...`; little punctuation inside, commas rare.
- **Alternatives with slashes**, no spaces: `plus clair/lisible`, `web/apk/terminal`, `compilation/gpu/ram`, often closed
  by `ou autre`, `ect...`, `/ect...`, `voir ...` (= even / or maybe): `couleur/logo/ect...`, `voir les supprimer`.
- **Asides in parentheses**, sometimes nested, to precise or restrict: `(uniquement en local)`, `(pas de push)`,
  `(option par default selection cancel)`.
- **Arrows** for a consequence or the next step: `-> commit`, `-> update readme/docs`; lists as `-` lines or `1 - ...`
  when several asks.
- **Emphasis by punctuation**, never by words: `???` (surprise, something wrong), `!!!` (an order that matters), `?` glued
  or spaced (`... ?`, `...?`); `...` to trail off.
- Vocabulary he uses: `genre`, `truc`, `chose`, `tj` (toujours), `cela` more than `ça`, `fait` for both fais / faire,
  `peut` for peux, `tout les`, `le plus possible`, `si besoin`, `si possible`, `de base`, `au lieu de`, `en plus`.
- Tone: friendly but terse, no politeness formulas (no "bonjour", "merci d'avance", "cordialement"), no emoji (the faces
  `._.`, `-_-` only in jokes), no hedging; frustration shown by `???` and a short `c'est moche`, `ca marche pas`.
- **Skill / file references inline**: `/interactive-questions`, `/clarify`, `@~/path/to/file` at the point they apply.

Example (written in his voice, not one of his messages):
`fait en sorte que le menu des settings soit scrollable sur l'apk et que les texte ne ce supperpose plus (check aussi les
autre page/popup pour le meme probleme) -> commit en local uniquement et pas de push !!!`

## Register 2: notes, a todo, an idea list (his `.info` voice)
- One short line per item, **no full sentence**, often no final dot; French and English mixed line by line.
- Starts: `add ...`, `allow to ...`, `be able to ...`, `make ...`, `avoir ...`, `pouvoir ...`, `faire en sorte ...`.
- Doubts as a trailing `?` / `???` on the line (`using env var or not on the observer ?`); blocks separated by a blank line
  or a `//////` rule; a few indented lists (names, types) without bullets.

Example: `allow to hide some flag in the usage display` / `avoir un type Coord<T> dans les math` / `timeout sur le wait ?`

## Typos
- **Off by default**: his voice, spelled right (the slips are fast typing, not style). Kept off for anything sent to
  someone else.
- `fidèle` / `avec mes fautes` asked: a few of his usual slips - letters swapped or doubled (`porbleme`, `isntaller`,
  `maitenant`), a missing letter (`jsute`, `pour toit`), `ce` for `se` (`ce passe`), `a` for `à`, verbs without the right
  ending (`tu peut`, `il prenen`) - about one per sentence, never on names, paths, commands or code.

## More fidelity: a fresh sample
`python3 scripts/sample.py [count]` reads his real writing on this computer (claude-cluster histories, the newest Claude
Code transcripts - only his own typed messages -, his `.info` notes) and prints counts of his habits and a random sample:
read it before a long text in his voice. Local only: never paste the sample anywhere else.

## Never
- Write as him to impersonate him to someone who does not know (an email signed by him he has not read): draft it,
  he sends it.
- Copy his real messages into a document: the patterns only.
