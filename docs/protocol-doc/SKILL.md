---
name: protocol-doc
description: Document a network / application protocol in Tsukini's way (from the 2ETP protocol doc of his myteams project) - name + version + acronym, what it provides, the framing of a transmission (control bytes ENQ / ETB / EOT...), the payload types table (byte, hex, link), one section per message type (grammar + field bullets), the code tables (errors, events, commands, with the data sent), a summary of every frame, the handshake / key exchange step by step with the exact frames and a sequence diagram, the commands of the client and of the server (purpose, answer, usage), the storage format (separators, layout, parsed type) - always asking first the output (Markdown, PDF, HTML section inside the existing docs/ site). Use whenever the user wants to document, specify, describe or review a protocol, a wire format, a message format, a handshake, the commands of a client / server, or the files format of a networking project.
---

# Protocol documentation

`SKILL_DIR` = the directory of this file. The reference is the **2ETP** doc of the user (the `README.md` of
`~/delivery/year_2/nwp/G-NWP-400-BDX-4-1-myteams-1`): its order, its tables and its notation are the model, improved by
the checks below. Every byte, code, field and command is **read from the code** (the parser, the enums of the control
bytes, the command table, the handshake functions): never invented, planned parts marked as such.

## 1. Ask first, always (one AskUserQuestion call, French)
Even when the request seems to say it, confirm (the answers decide the files):
1. **Format** (multiSelect): `Markdown (Recommandé)` (`PROTOCOL.md`, or a section of the README when the project has
   no other doc) · `PDF` (from the Markdown, style of `report`) · `HTML dans la doc existante` (sections of the
   technical page of `docs/`, `html-doc` style; no `docs/` yet: the technical page is created with `html-doc`).
2. **Contenu** (multiSelect, the ones found in the code pre-selected in the descriptions): `Trame + types de messages` ·
   `Codes (erreurs, événements, commandes)` · `Handshake / échange de clés` · `Commandes client / serveur` ·
   `Format de stockage`.
3. **Langue**: `Anglais (Recommandé)` · `Français` · `Les deux` (HTML: EN / FR switch of `html-doc`, always both).
4. **Emplacement** (only when not obvious): `PROTOCOL.md à la racine` · `docs/PROTOCOL.md` · `Section du README`.
The answers are kept for the rest of the conversation (a later update of the same doc never asks again).

## 2. Read the protocol in the code
- Framing: the send / recv functions, the parser (`parseTransmission`...), the control characters used
  (`utils::write::Char::*`, `#define`, enums), the encoding (Base64, hex, length prefix), the encryption (tag size).
- Message types: the switch on the first byte of the payload, one per type; the code ranges (`code % 128 + 128`).
- Codes: the error / event / command enums or tables, and for each event / answer the data it carries, in order.
- Handshake: the first exchanges of a connection (client and server sides), the keys generated (sizes), what makes
  the server disconnect.
- Commands: the command table of the client (name, arguments, code sent) and its answer on the server; the server
  console commands.
- Storage: the files written, their separators, the order of the fields, the type of the parsed data.
- Anything unclear (a byte whose meaning the code doesn't show): ask in the same AskUserQuestion call or mark it
  **TBD** in the doc, never guess.

## 3. Structure (`templates/PROTOCOL.md`)
1. `# Documentation` (or `# <Project> protocol`), `### Table of Contents` linking every `##`.
2. `## <NAME> Protocol (vX.Y.Z)`, the acronym expanded in a quote with the bold letters
   (`> **E**ncrypted & **E**ncoded, **T**yped **T**ransfer **P**rotocol`), one sentence (layer, transport), then
   `It provides:` and its bullets (encryption, encoding, typed messages, framing).
3. `### Payload`: the grammar of a transmission in a code block (`ENQ <tag> ETB <payload> EOT`), one bullet per element
   (name, hex `` (`0x05`) ``, role), the RFC 2119 words in upper case (**MUST**, **MUST NOT**, **SHOULD**, **MAY**),
   then the **types table** `| Byte (hexa) | Type |`: `` SYN (`0x16`) `` and a link to the section of the type.
4. One `###` per message type: its grammar in a code block (repetition `*(ETB <data>)`, optional `[...]`,
   alternatives `a | b`), one bullet per field (encoding, size, meaning), its code table when it has codes
   (`| Code (0-127) | Error |`, `| Code (0-127) | Event | Informations send |`, `| Code (0-127) | Command |` with links
   to the command sections), the reserved / internal codes (`127`) said.
5. `### Different Payload Type Resume`: every frame of the protocol in one code block, aligned, one comment per line
   (`/* Connection    */ ENQ <tag> ETB SYN EOT`).
6. `## Handshake`: one `###` per step (`### First step (basic, tag = 0 byte)`), who sends what with the **exact frame**
   in backticks, the server behaviour as `If valid → ... / If invalid → ...` bullets, the side code excerpt (short, the
   real code) when it helps, and a sequence diagram of the whole exchange (`templates/PROTOCOL.md`: ASCII in a code
   block, it renders in every format).
7. `## Commands (Client)` / `## Commands (Server)`: one `###` per command: `> [!NOTE]` purpose, `> Server answers
   with \`<frame>\`` (each context on its own line, the events it triggers `& event(N)`), the usage in a code block
   (`login "user_name"`, optional `[ip:port]`, alternatives `(user_uuid|thread_uuid)`); `> [!IMPORTANT]` for an
   internal command.
8. `## <Storage>`: `> [!WARNING]` when not encrypted, the tree of the files (`bash` block), the separators table
   (`| Byte (hexa) | Usage |`), the layers (`file -> record GS record GS ...`), each file layout in its own code block
   (one field per line when it is long: never a single line wider than the page), the parsed C++ type with its comments.
- Notation, everywhere: control bytes by their ASCII name + hex, placeholders `<snake_case>`, literal strings in
  quotes, repetitions `*( ... )`, a field encoded in Base64 said so once in its bullet.

## 4. Writing
- English by default (the answer of question 3), present tense, short sentences; a typo of the reference (`steap`,
  `awnser`, `dosen't`, `reciever`) is never copied.
- Every frame, byte and command checked against the code; a difference between the code and an existing doc is
  reported to the user before choosing which one wins.
- Improvements over the reference, applied by default: the RFC 2119 words, the error answer of each command (`NAK
  <code>` cases), a hex example of one full frame (`templates/PROTOCOL.md`, *Example*), the version of the protocol and
  what a peer does with an unknown type / code, the limits (max size of a frame, timeouts) when the code has them.

## 5. Outputs
- **Markdown**: `templates/PROTOCOL.md` filled (GitHub callouts `> [!NOTE]`, tables, code blocks; `readme-style`
  rules for the rest); the README links to it (`## Documentation` line) when it is a separate file.
- **PDF**: from that Markdown, `python3 <report>/scripts/md2pdf.py PROTOCOL.md --format both -o PROTOCOL.pdf
  --footer "<Project> — <NAME> protocol"` (style of `report`, GitHub callouts rendered: note / tip blue, warning
  orange); `pdftoppm -r 60 -png -f 1 -l 2` to look at it (no line wider than the page, tables whole).
- **HTML in the existing doc** (`html-doc` + `html-style`): the sections of `templates/protocol.html` pasted into
  `docs/technical.html` (or the technical `index.html`) as numbered cards after the existing ones (one `<section>` per
  `##` of the Markdown: Protocol, Handshake, Commands, Storage), their links added to the sidebar, every text in
  both languages; the frames in `<pre data-title="...">`, the types / codes in `table-wrap` tables, the notes in
  `callout` / `callout warn`, the handshake as the SVG sequence diagram of the template (activation boxes on the
  lifelines, arrows computed with `data-from` / `data-to`, never typed). Then
  `python3 <html-style>/scripts/check_style.py docs/*.html` (0 error) and `python3 <html-style>/scripts/sync_nav.py docs`.
  No `docs/` yet: create the technical page with `html-doc` first (ask its questions: logo...).

## 6. Check
```bash
python3 SKILL_DIR/scripts/check_protocol.py PROTOCOL.md [docs/technical.html]
```
It checks the anchors of the contents and of the tables (every link reaches a heading / an `id`), that every row of
the types table and of the command table has its section, the codes unique in each table, the frames of the summary
matching the grammar of their section (same type byte), no `{{...}}` placeholder and no typo of the reference left.
Then report: the files written, the sections, what was marked **TBD** or differs from the code.
