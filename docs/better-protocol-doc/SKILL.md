---
name: better-protocol-doc
description: The reference of a network / application protocol written as pages of a better-html-doc site (the claude-cluster docs style) - framing, one :::message block per message type (name + code, its byte layout drawn with one cell per field sized by its bytes, the fields table, its rules), code tables (errors, events, commands), handshake as a numbered sequence (direction + frame), client / server commands, storage format - every byte read from the code, searchable (each message is an anchor), responsive (the layout wraps on a phone). Use whenever the user wants a protocol / wire format / message format documented inside a documentation site in the claude-cluster style; Markdown / PDF / the older HTML: protocol-doc.
---

# Protocol pages in a better-html-doc site

`SKILL_DIR` = directory of this file. The **reading of the protocol and the structure are those of `protocol-doc`**
(`../protocol-doc/SKILL.md` §2-§3: framing, payload types, one section per message type, code tables, summary of the frames,
handshake step by step, commands, storage; everything read from the code, unclear bytes asked or marked TBD): load it first.
This skill writes it as page(s) of a `better-html-doc` site (`../better-html-doc`), with the built-in `protocol` kind
(`templates/protocol.css`, loaded only on these pages).

## 1. Ask (AskUserQuestion, French) unless known
- **Contenu** (multiSelect) as `protocol-doc` §1.2; **Découpage** : `Une page (Recommandé)` · `Une page par partie`
  (framing + messages / codes / handshake / commands, a big protocol).
- The site and its languages come from its `site.toml` (no site yet: `better-html-doc` first; the pages go in `Référence`).

## 2. Write the page(s)
`src/pages/reference/<slug>.<lang>.md` with `kind = "protocol"` - start from `SKILL_DIR/templates/pages/reference/protocol.*.md`.
- A message:
  ```markdown
  :::message HELLO 0x01
  | field | type | size | description |
  |---|---|---|---|
  | type | u8 | 1 | `0x01` |
  | name | utf8 | var | the client name, 32 bytes at most |
  The rules of this message (when it is sent, what the other side answers, what makes it fail).
  :::
  ```
  → the name and its code, the **byte layout** (one cell per field, its width from its size; `var` = a variable length),
  the table, the text after it. Columns `field | type | size | description` (French headers `champ | type | taille |
  description` work too); `#msg-<name>` is its anchor (link it from the summary table: `[HELLO](#msg-hello)`).
- Handshake / exchange: a numbered sequence in `:::html`: `<ol class="seq"><li><div><span class="dir">client → server</span>
  <br><code>HELLO</code> …</div></li></ol>` (one exchange per item, the exact frames); a timing diagram, when it helps, as an
  inline SVG figure with computed arrows (`html-style`).
- Code tables (errors, events, commands): Markdown tables, one per family, code in hex and decimal when the code uses both.
- Sizes and orders exactly as the parser reads them (endianness stated once in the framing section).

## 3. Build and check
The commands of `better-html-doc`, then `python3 ../protocol-doc/scripts/check_protocol.py` on the Markdown sources when it
applies (codes unique, every message of the summary documented), and the screenshots at 1280 and 390 px (the layout of a
message wraps, never scrolls the page sideways).
