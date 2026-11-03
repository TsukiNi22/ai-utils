#!/usr/bin/env python3
# =============================================================================
# mdlib.py - the Markdown of the claude-cluster docs (deterministic, no dependency; the same file as better-html-doc/scripts/mdlib.py: keep them equal)
#   front matter  +++ TOML +++ (first lines)
#   headings      # .. ######, an explicit id with  ## Title {#id}
#   blocks        paragraphs, lists (- * + 1. 1)) at any depth with blocks inside an item and task items - [ ] / - [x], tables (:---: aligned,
#                 outer pipes optional), fences ``` ~~~ (```lang "File title"), indented code, > quotes, --- rule, raw HTML (a line opening a
#                 block tag: details, figure, table, svg...; closed by its end tag), <!-- comments -->
#   callouts      > [!NOTE] / [!TIP] / [!IMPORTANT] / [!WARNING] / [!CAUTION] / [!WARN] / [!OK] / [!PLANNED] / [!SOON], {: .warn} / {: .ok} under a quote
#   ::: blocks    :::html (raw HTML) · :::simple / :::technical [tldr] (one level of an explanation page) · :::details Title
#                 (collapsed) · :::message NAME [code] (a protocol message: its byte layout + the table of its fields)
#                 closed by a ::: line, nested blocks allowed
#   inline        `code` (``a`b``), **bold**, *italic*, ~~deleted~~, [text](url "title"), [text](page:slug#id), ![alt](src "title"), <https://autolink>,
#                 \* escapes, &entities;, two spaces at the end of a line (<br>), the tags of ALLOWED_TAGS (with their attributes)
# =============================================================================

import html
import math
import re
import tomllib

ALLOWED_TAGS = ("kbd", "br", "b", "i", "em", "strong", "sub", "sup", "span", "code", "mark", "del", "ins", "s", "u", "abbr", "small", "samp", "var", "q", "cite", "dfn", "wbr", "img", "a")
VOID_TAGS = ("br", "wbr", "img", "hr")
BLOCK_TAGS = ("address", "article", "aside", "audio", "blockquote", "canvas", "center", "details", "dialog", "div", "dl", "fieldset", "figcaption", "figure", "footer", "form",
              "h1", "h2", "h3", "h4", "h5", "h6", "header", "hr", "iframe", "main", "math", "menu", "nav", "noscript", "object", "ol", "p", "picture", "pre", "section", "style",
              "summary", "svg", "table", "ul", "video")
CALLOUTS = {"NOTE": "", "TIP": " ok", "IMPORTANT": "", "WARNING": " warn", "CAUTION": " warn", "WARN": " warn", "OK": " ok", "PLANNED": " warn", "SOON": " warn"}
CALLOUT_LABEL = {"TIP": ("Tip", "Astuce"), "IMPORTANT": ("Important", "Important"), "WARNING": ("Warning", "Attention"), "CAUTION": ("Caution", "Prudence"),
                 "PLANNED": ("Planned", "Prévu"), "SOON": ("Coming soon", "Bientôt")}
TASK_LABEL = {"en": ("to do", "done"), "fr": ("à faire", "fait")}

RULE = r"\s*(?:-\s*){3,}|\s*(?:\*\s*){3,}|\s*(?:_\s*){3,}"
FENCE = r"(`{3,})([^`]*)|(~{3,})(.*)"
LIST_ITEM = re.compile(r"^(\s*)([-*+]|\d{1,9}[.)])\s+(.*)$")
TABLE_RULE = re.compile(r"^\s*\|?\s*:?-+:?\s*(?:\|\s*:?-+:?\s*)*\|?\s*$")
ATTR_LINE = re.compile(r"^\{:\s*([^}]*)\}\s*$")
BLOCK_TAG = re.compile(r"^\s{0,3}<(%s)(?=[\s/>]|$)" % "|".join(BLOCK_TAGS), re.I)
BLOCK_START = r"^(#{1,6}\s|```|~~~|:::|>|\||<!--|(?:%s)\s*$|(\s*)([-*+]|\d{1,9}[.)])\s)" % RULE
ESCAPABLE = r"\\`*_{}\[\]()#+\-.!|<>~"
INLINE_TAG = re.compile(r"</?(%s)(?:\s+[\w:-]+(?:\s*=\s*(?:\"[^\"]*\"|'[^']*'|[^\s\"'<>]+))?)*\s*/?>" % "|".join(ALLOWED_TAGS), re.I)
AUTOLINK = r"<((?:https?|ftp|mailto):[^\s<>]+)>"
WORD_KEPT = 30      # a word up to that many characters stays whole in a column (a longer one may break at a / or a -)
PIECE_KEPT = 48     # what has no place to break (a JSON sample) stays whole up to that: it reads whole, its table scrolls (base.js of html-style lets a longer one break)
BROKEN_FLOOR = 24   # the column of a word too long for that: it breaks anyway
PHRASE_KEPT = 20    # a phrase up to that many characters stays on one line
TEXT_FLOOR = 14     # the narrowest column a longer text is read in
TEXT_ROOMY = 34     # the widest column a long text asks for, in a table that does not fit a phone anyway
PHONE_ROOM = 40     # what a table has on a phone, in ch: under it the table stays on the screen
TABLE_ROOM = 92     # what it has on a tablet: over it the long words of the table may break at a / or a -
CELL_PADDING = 3    # the padding of a cell, in ch (min-width holds it)

def split_front(text: str) -> tuple[dict, str]:
    if not text.startswith("+++\n"):
        return {}, text
    end = text.index("\n+++\n", 4)
    return tomllib.loads(text[4:end]), text[end + 5:]

def slugify(text: str) -> str:
    text = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return text or "section"

def plain_inline(text: str) -> str:
    """A line of Markdown as its reader sees it: a title in the sidebar and the search, the words of a cell."""
    pieces = re.split(r"(?<!`)(`+)(?!`)(.+?)(?<!`)\1(?!`)", text)  # [text, ticks, code, text...]: a code span keeps its characters
    out: list[str] = []
    for n, piece in enumerate(pieces):
        if n % 3 == 1:
            continue
        if n % 3 == 0:
            piece = re.sub(r"!\[([^\]]*)\]\([^)]*\)", r"\1", piece)
            piece = re.sub(r"\[((?:[^\[\]]|\[[^\]]*\])*)\]\([^)]*\)", r"\1", piece)
            piece = re.sub(AUTOLINK, r"\1", piece)
            piece = INLINE_TAG.sub("", piece)
            piece = re.sub(r"\*+|~~", "", piece)
            piece = html.unescape(re.sub(r"\\([%s])" % ESCAPABLE, r"\1", piece))
        out.append(piece)
    return "".join(out).strip()

def without_title(markdown: str, title: str) -> str:
    """A Markdown file brought as it is starts with the title of the page: it is already the h1 of the page."""
    first = re.match(r"\s*#\s+(.*?)\s*(?:\n|$)", markdown)
    return markdown[first.end():] if first and plain_inline(first.group(1)) == plain_inline(title) else markdown

def unnumbered(title: str, number: int) -> str:
    """A section title without the number its author typed: the page numbers its sections."""
    return re.sub(r"^%d[.)]\s+" % number, "", title)

def starts_block(line: str) -> bool:
    """A line that ends the paragraph above it (an ordered item only from its first number: "2026. It was..." is text)."""
    item = LIST_ITEM.match(line)
    if item and item.group(2)[0].isdigit():
        return int(re.sub(r"\D", "", item.group(2))) == 1
    return bool(re.match(BLOCK_START, line) or BLOCK_TAG.match(line))

def opening_fence(line: str) -> str:
    """The fence a line opens (its run of ` or ~), empty if none."""
    m = re.fullmatch(FENCE, line)
    return (m.group(1) or m.group(3)) if m else ""

def closes_fence(fence: str, line: str) -> bool:
    return bool(re.fullmatch(re.escape(fence[0]) + "{%d,}\\s*" % len(fence), line))

def expand_indent(line: str) -> str:
    return re.sub(r"^[ \t]+", lambda m: m.group(0).expandtabs(4), line)

def indent_of(line: str) -> int:
    return len(line) - len(line.lstrip())

def word_floor(word: str, whole: bool) -> int:
    """What a word keeps in a column: itself (whole: up to WORD_KEPT), else its pieces (after a / or a -); no place to break over PIECE_KEPT: it breaks anywhere."""
    if whole and len(word) <= WORD_KEPT:
        return len(word)
    piece = max(len(part) for part in re.split(r"(?<=[/\-?])", word))
    return piece if piece <= PIECE_KEPT else BROKEN_FLOOR

def column_floor(cell: str, head: bool = False, whole: bool = True) -> int:
    """The width (ch) a column must keep for a cell: its words whole, a short phrase on one line, a longer text in a column it can be read in, never a word per line.
    Whatever the other columns ask: the table is then wider than its box and scrolls in it, the words are not squeezed."""
    factor = 1.1 if "`" in cell or "<code" in cell or "<kbd" in cell else 1.0
    if head:
        factor = 1.2  # a head is in capitals
    chip = 3 if "<kbd" in cell else 0
    floor = 0.0
    for part in re.split(r"<br\s*/?>", cell):
        words = plain_inline(part).split()
        if not words:
            continue
        size = max(word_floor(word, whole) + chip for word in words)
        text = sum(len(word) + chip for word in words) + len(words) - 1
        if len(words) >= 2:
            size = max(size, text if text <= PHRASE_KEPT else min(math.ceil(text / 2), TEXT_FLOOR))
        if len(words) >= 3:
            size = max(size, min(len(a) + len(b) + 1 + 2 * chip for a, b in zip(words, words[1:])))
        floor = max(floor, size * factor)
    return math.ceil(floor) + CELL_PADDING if floor else 0

def table_floors(head: list[str], body: list[list[str]]) -> list[int]:
    """What each column of a table keeps. It fits a phone: as narrow as its words let it, it stays on the screen. It cannot: it scrolls there anyway, a long text gets a
    column it reads well in, as long as the table still fits a tablet. Too wide for that: its long words may break at a / or a -."""
    def floors_of(whole: bool) -> list[int]:
        return [max([column_floor(head[n], True, whole), *(column_floor(row[n], False, whole) for row in body)]) for n in range(len(head))]

    floors = floors_of(True)
    if sum(floors) > TABLE_ROOM:
        floors = floors_of(False)
    if sum(floors) <= PHONE_ROOM or sum(floors) >= TABLE_ROOM:
        return floors
    longest = [max(len(plain_inline(cell)) for cell in [head[n], *(row[n] for row in body)]) for n in range(len(head))]
    roomy = [max(floor, min(math.ceil(math.sqrt(size) * 2), TEXT_ROOMY) + CELL_PADDING) for floor, size in zip(floors, longest)]
    extra = sum(roomy) - sum(floors)
    share = min(1.0, (TABLE_ROOM - sum(floors)) / extra) if extra else 0.0
    return [floor + math.floor((wanted - floor) * share) for floor, wanted in zip(floors, roomy)]

class Renderer:
    def __init__(self, lang: str):
        self.lang = lang

    # ----- inline -----
    def inline(self, text: str) -> str:
        keep: list[str] = []

        def stash(value: str) -> str:
            keep.append(value)
            return "\x00%d\x00" % (len(keep) - 1)

        def unstash(value: str) -> str:
            while "\x00" in value:
                value = re.sub(r"\x00(\d+)\x00", lambda m: keep[int(m.group(1))], value)
            return value

        def code_or_escape(m: re.Match) -> str:
            if m.group(1) is not None:
                return stash(html.escape(m.group(1), quote=False))
            code = m.group(3)
            if len(code) > 2 and code[0] == " " and code[-1] == " " and code.strip():
                code = code[1:-1]
            return stash("<code>" + html.escape(code, quote=False) + "</code>")

        def raw_tag(m: re.Match) -> str:
            name = m.group(1).lower()
            whole = m.group(0)
            if name in VOID_TAGS or whole.endswith("/>"):
                return stash(whole)
            if whole.startswith("</"):  # an end tag without its start, or a start tag without its end (a placeholder like <var> in the text): shown as text
                return stash(whole) if re.search(r"<%s[\s>]" % name, text[:m.start()], re.I) else whole
            return stash(whole) if text.find("</%s>" % name, m.end()) >= 0 else whole

        def attr(value: str) -> str:
            return value.replace('"', "&quot;")

        def image(m: re.Match) -> str:
            alt = re.sub(r"<[^>]+>", "", unstash(m.group(1)))
            title = ' title="%s"' % attr(m.group(3)) if m.group(3) else ""
            return stash('<img src="%s" alt="%s"%s loading="lazy">' % (attr(self.url(m.group(2))), attr(alt), title))

        def link(m: re.Match) -> str:
            title = ' title="%s"' % attr(m.group(3)) if m.group(3) else ""
            return stash('<a href="%s"%s>' % (attr(self.url(m.group(2))), title)) + m.group(1) + stash("</a>")

        target = r"\(\s*((?:[^()\s]|\([^()\s]*\))+)(?:\s+\"([^\"]*)\")?\s*\)"
        text = re.sub(r"\\([%s])|(?<!`)(`+)(?!`)(.+?)(?<!`)\2(?!`)" % ESCAPABLE, code_or_escape, text)  # first: nothing in a code span is markup
        text = re.sub(r"<!--.*?-->", "", text)
        text = re.sub(AUTOLINK, lambda m: stash('<a href="%s">%s</a>' % (html.escape(m.group(1)), html.escape(m.group(1), quote=False))), text)
        text = INLINE_TAG.sub(raw_tag, text)
        text = html.escape(text, quote=False)
        text = re.sub(r"&amp;(#\d+|#[xX][0-9a-fA-F]+|[a-zA-Z][a-zA-Z0-9]*);", r"&\1;", text)
        text = re.sub(r"!\[([^\]]*)\]" + target, image, text)
        text = re.sub(r"\[((?:[^\[\]]|\[[^\]]*\])*)\]" + target, link, text)
        text = re.sub(r"\*\*\*(?=\S)(.+?)(?<=\S)\*\*\*", r"<strong><em>\1</em></strong>", text)
        text = re.sub(r"\*\*(?=\S)(.+?)(?<=\S)\*\*", r"<strong>\1</strong>", text)
        text = re.sub(r"(?<![*\w])\*(?=[^\s*])([^*]*?)(?<=\S)\*(?![*\w])", r"<em>\1</em>", text)
        text = re.sub(r"~~(?=\S)(.+?)(?<=\S)~~", r"<del>\1</del>", text)
        return unstash(text)

    @staticmethod
    def url(url: str) -> str:
        if url.startswith("page:"):
            page, _, frag = url[5:].partition("#")
            return page + ".html" + ("#" + frag if frag else "")
        return url

    # ----- blocks -----
    def render(self, text: str) -> list[dict]:
        """Blocks: {"h": level, "id": id, "title": html, "raw": text} or {"html": html}."""
        lines = text.replace("\r\n", "\n").split("\n")
        out: list[dict] = []
        i = 0
        while i < len(lines):
            line = lines[i]
            if not line.strip():
                i += 1
                continue
            m = re.match(r"^(#{1,6})\s+(.*?)(?:\s+#+)?(?:\s+\{#([\w-]+)\})?\s*$", line)
            fence = opening_fence(line)
            if line.lstrip().startswith("<!--"):  # a comment: nothing to show
                while i < len(lines) and "-->" not in lines[i]:
                    i += 1
                i += 1
            elif m:
                level, title = len(m.group(1)), m.group(2)
                raw = plain_inline(title)
                out.append({"h": level, "id": m.group(3) or slugify(raw), "title": self.inline(title), "raw": raw})
                i += 1
            elif fence:
                info = line[len(fence):].strip()
                lang, _, title = info.partition(" ")
                code: list[str] = []
                i += 1
                while i < len(lines) and not closes_fence(fence, lines[i]):
                    code.append(lines[i])
                    i += 1
                i += 1
                if any(row.strip() for row in code):  # an empty fence: no empty box
                    out.append({"html": self.code_block(lang, title, code)})
            elif re.match(r"^:::\w", line):
                kind, _, arg = line[3:].strip().partition(" ")
                inner: list[str] = []
                depth = 1
                i += 1
                while i < len(lines):
                    if re.match(r"^:::\w", lines[i]):
                        depth += 1
                    elif lines[i].strip() == ":::":
                        depth -= 1
                        if depth == 0:
                            break
                    inner.append(lines[i])
                    i += 1
                i += 1
                out.extend(self.block(kind, arg.strip(), inner))
            elif "|" in line and i + 1 < len(lines) and "|" in lines[i + 1] and TABLE_RULE.match(lines[i + 1]):
                rows: list[str] = []
                while i < len(lines) and lines[i].strip() and "|" in lines[i]:
                    rows.append(lines[i])
                    i += 1
                out.append({"html": self.table(rows)})
            elif line.startswith(">"):
                quote: list[str] = []
                while i < len(lines) and lines[i].startswith(">"):
                    content = re.sub(r"^>\s?", "", lines[i])
                    if quote and self.callout_kind(content):  # a second callout glued to the first one: two callouts
                        break
                    quote.append(content)
                    i += 1
                extra = ""
                if i < len(lines) and ATTR_LINE.match(lines[i]):  # {: .warn} under a quote
                    extra = "".join(" " + name for name in re.findall(r"\.([\w-]+)", lines[i]))
                    i += 1
                out.append({"html": self.quote(quote, extra)})
            elif re.fullmatch(RULE, line):
                out.append({"html": "<hr>"})
                i += 1
            elif ATTR_LINE.match(line):  # no element here to give it to
                i += 1
            elif BLOCK_TAG.match(line):
                raw_html, i = self.raw_block(lines, i)
                out.append({"html": raw_html})
            elif LIST_ITEM.match(line):
                items, i = self.collect_list(lines, i)
                out.append({"html": self.list_(items)})
            elif re.match(r"^(?: {4}|\t)", line):
                code = []
                while i < len(lines) and (not lines[i].strip() or re.match(r"^(?: {4}|\t)", lines[i])):
                    code.append(re.sub(r"^(?: {4}|\t)", "", lines[i]))
                    i += 1
                while code and not code[-1].strip():
                    code.pop()
                out.append({"html": self.code_block("", "", code)})
            else:
                para: list[str] = []
                while i < len(lines) and lines[i].strip() and (not para or not starts_block(lines[i])):
                    hard = lines[i].endswith("  ") or lines[i].rstrip().endswith("\\")
                    para.append(lines[i].strip().removesuffix("\\").rstrip() + ("<br>" if hard else ""))
                    i += 1
                out.append({"html": "<p>" + self.inline(" ".join(para).removesuffix("<br>")) + "</p>"})
        return out

    @staticmethod
    def html_of(blocks: list[dict]) -> str:
        """Blocks as HTML, in a place without sections (a callout, a list item, a collapsed block): a heading stays a heading."""
        return "".join(b["html"] if "html" in b else '<h%d id="%s">%s</h%d>' % (b["h"], b["id"], b["title"], b["h"]) for b in blocks)

    @staticmethod
    def code_block(lang: str, title: str, code: list[str]) -> str:
        title = title.strip().strip('"')
        attrs = ' data-title="%s"' % html.escape(title) if title else ""
        label = html.escape(title if title else "Code (%s)" % (lang or "text"))
        attrs += ' tabindex="0" role="region" aria-label="%s"' % label  # RGAA 7.3: a block that scrolls is reachable with the keyboard
        return '<pre%s><code class="language-%s">%s</code></pre>' % (attrs, html.escape(lang or "text"), html.escape("\n".join(code), quote=False))

    @staticmethod
    def raw_block(lines: list[str], start: int) -> tuple[str, int]:
        """Raw HTML: from the line opening a block tag to its end tag (balanced, its blank lines included); never closed: up to the blank line."""
        name = BLOCK_TAG.match(lines[start]).group(1).lower()
        i = start
        while i < len(lines):
            i += 1
            line = lines[i - 1].lower()
            if name in VOID_TAGS:
                if ">" in line:
                    return "\n".join(lines[start:i]), i
                continue
            if "</" + name not in line and "/>" not in line:
                continue
            whole = "\n".join(lines[start:i])
            opened = len(re.findall(r"<%s(?=[\s/>])[^>]*(?<!/)>" % name, whole, re.I))
            if opened <= len(re.findall(r"</%s\s*>" % name, whole, re.I)):
                return whole, i
        i = start
        while i < len(lines) and lines[i].strip():
            i += 1
        return "\n".join(lines[start:i]), i

    def block(self, kind: str, arg: str, inner: list[str]) -> list[dict]:
        """A ::: block: its blocks (a heading inside a level block keeps its place in the sections)."""
        if kind == "html":
            return [{"html": "\n".join(inner)}]
        if kind in ("simple", "technical"):
            blocks = self.render("\n".join(inner))
            opened = {"html": '<div data-level="%s">' % kind}
            if any(b.get("h") for b in blocks):  # headings stay blocks of their own: each run of html between them is wrapped
                out: list[dict] = []
                run: list[str] = []
                for b in blocks:
                    if b.get("h"):
                        if run:
                            out.append({"html": opened["html"] + "".join(run) + "</div>"})
                            run = []
                        b = dict(b, level=kind)
                        out.append(b)
                    else:
                        run.append(b["html"])
                if run:
                    out.append({"html": opened["html"] + "".join(run) + "</div>"})
                return out
            body = "".join(b["html"] for b in blocks)
            if arg == "tldr":  # :::simple tldr - the one-sentence summary of an explanation page, highlighted
                body = body.replace("<p>", '<p class="tldr">')
            return [{"html": opened["html"] + body + "</div>"}]
        if kind == "details":
            body = self.html_of(self.render("\n".join(inner)))
            return [{"html": '<details class="more"><summary>%s</summary><div>%s</div></details>' % (self.inline(arg or "Details"), body)}]
        if kind == "message":
            return [{"html": self.message(arg, inner)}]
        raise SystemExit("unknown block :::%s (html, simple, technical, details, message)" % kind)

    def message(self, arg: str, inner: list[str]) -> str:
        """A protocol message: the name / code, the byte layout (one cell per field, its width from its size), the fields table and
        the text after the table."""
        name, _, code = arg.partition(" ")
        rows = [r for r in inner if r.startswith("|")]
        rest = [r for r in inner if not r.startswith("|")]
        if len(rows) < 3:
            raise SystemExit(":::message %s: a table | field | type | size | description | is expected" % name)
        head = [c.strip().lower() for c in rows[0].strip().strip("|").split("|")]
        cells = []
        for r in rows[2:]:
            values = [c.strip() for c in re.split(r"(?<!\\)\|", r.strip()[1:-1])]
            row = dict(zip(head, values))
            for alias, key in (("champ", "field"), ("nom", "field"), ("name", "field"), ("taille", "size"), ("octets", "size"), ("bytes", "size")):
                if alias in row and key not in row:
                    row[key] = row[alias]
            size = row.get("size", "")
            width = int(size) if size.isdigit() else 4
            cells.append('<span style="flex-grow:%d"><b>%s</b><small>%s · %s</small></span>' % (
                min(max(width, 1), 16), self.inline(row.get("field", "?")), html.escape(row.get("type", "")),
                html.escape(size + (" B" if size.isdigit() else "") if size else "")))
        caption = '<figcaption><b>%s</b>%s</figcaption>' % (html.escape(name), (" <code>%s</code>" % html.escape(code)) if code else "")
        after = self.html_of(self.render("\n".join(rest))) if any(x.strip() for x in rest) else ""
        return '<figure class="message" id="msg-%s">%s<div class="frame" role="img" aria-label="%s">%s</div>%s%s</figure>' % (
            slugify(name), caption, html.escape("layout of " + name), "".join(cells), self.table(rows), after)

    def table(self, rows: list[str]) -> str:
        """A table in its box: every row as wide as the widest one, what each column must keep on its first row (see column_floor)."""
        def cells(row: str) -> list[str]:
            row = row.strip()
            row = row[1:] if row.startswith("|") else row
            row = row[:-1] if row.endswith("|") and not row.endswith("\\|") else row
            return [c.strip().replace("\\|", "|") for c in re.split(r"(?<!\\)\|", row)]

        def align(rule: str) -> str:
            if rule.endswith(":"):
                return "center" if rule.startswith(":") else "right"
            return ""

        head = cells(rows[0])
        aligns = [align(c) for c in cells(rows[1])]
        body = [cells(r) for r in rows[2:]]
        width = max([len(head), *(len(r) for r in body)])
        head += [""] * (width - len(head))
        body = [r + [""] * (width - len(r)) for r in body]
        aligns += [""] * (width - len(aligns))
        floors = table_floors(head, body)

        def attrs(n: int, first: bool) -> str:
            style = []
            if aligns[n]:
                style.append("text-align:%s" % aligns[n])
            if first and floors[n] > 3 + CELL_PADDING:
                style.append("min-width:%dch" % floors[n])
            return ' style="%s"' % ";".join(style) if style else ""

        titled = any(c.strip() for c in head)
        name = html.escape(": ".join(("Table", ", ".join(plain_inline(c) for c in head if c.strip()))) if titled else "Table", quote=True)
        out = ['<div class="table-wrap" tabindex="0" role="region" aria-label="%s"><table>' % name]
        if titled:  # a table without any head (| | |): no empty row on top
            out.append("<thead><tr>" + "".join('<th scope="col"%s>%s</th>' % (attrs(n, True), self.inline(c)) for n, c in enumerate(head)) + "</tr></thead>")
        out.append("<tbody>")
        for k, row in enumerate(body):
            out.append("<tr>" + "".join("<td%s>%s</td>" % (attrs(n, k == 0 and not titled), self.inline(c)) for n, c in enumerate(row)) + "</tr>")
        out.append("</tbody></table></div>")
        return "".join(out)

    @staticmethod
    def callout_kind(line: str) -> str:
        m = re.match(r"^\[!(\w+)\]", line)
        return m.group(1).upper() if m and m.group(1).upper() in CALLOUTS else ""

    def quote(self, quote: list[str], extra: str = "") -> str:
        kind = self.callout_kind(quote[0])
        label = ""
        if kind:
            first = re.sub(r"^\[!\w+\]\s*", "", quote[0])
            quote = ([first] if first else []) + quote[1:]
            if kind in CALLOUT_LABEL:
                label = "<strong>%s.</strong> " % CALLOUT_LABEL[kind][0 if self.lang == "en" else 1]
        body = self.html_of(self.render("\n".join(quote)))
        if not body and not label:  # an empty callout: no empty box
            return ""
        if label:
            body = body.replace("<p>", "<p>" + label, 1) if body.startswith("<p>") else "<p>" + label + "</p>" + body
        return '<div class="callout%s%s">%s</div>' % (CALLOUTS.get(kind, ""), extra, body)

    @staticmethod
    def collect_list(lines: list[str], start: int) -> tuple[list[str], int]:
        """The lines of a list: its items, what is indented under them (a fence too, as it is), the blank lines between them, a line glued to an item."""
        block: list[str] = []
        fence = ""
        i = start
        while i < len(lines):
            line = lines[i]
            if fence:
                block.append(line)
                fence = "" if closes_fence(fence, line.strip()) else fence
                i += 1
                continue
            if not line.strip():
                after = i
                while after < len(lines) and not lines[after].strip():
                    after += 1
                if after >= len(lines) or re.fullmatch(RULE, lines[after]) or not (LIST_ITEM.match(lines[after]) or lines[after].startswith(("  ", "\t"))):
                    break
                block.extend(lines[i:after])
                i = after
                continue
            if re.fullmatch(RULE, line) or (not (LIST_ITEM.match(line) or line.startswith(("  ", "\t"))) and (not block[-1].strip() or starts_block(line))):
                break
            fence = opening_fence(line.strip())
            block.append(line)
            i += 1
        return block, i

    def list_(self, lines: list[str]) -> str:
        """A sibling item at the level of the first one (or under it), what is deeper belongs to the item above: rendered as blocks. A change of marker is a new list."""
        lines = [expand_indent(line) for line in lines]
        base = indent_of(lines[0])
        items: list[dict] = []
        fence = ""
        for line in lines:
            m = LIST_ITEM.match(line) if not fence else None
            if m and len(m.group(1)) <= base:
                items.append({"marker": m.group(2), "lines": [m.group(3)]})
                fence = opening_fence(m.group(3))
                continue
            items[-1]["lines"].append(line)
            if fence:
                fence = "" if closes_fence(fence, line.strip()) else fence
            else:
                fence = opening_fence(line.strip())
        out: list[str] = []
        opened = ""
        for item in items:
            ordered = item["marker"][0].isdigit()
            if opened != ("ol" if ordered else "ul"):
                if opened:
                    out.append("</%s>" % opened)
                opened = "ol" if ordered else "ul"
                start = int(re.sub(r"\D", "", item["marker"])) if ordered else 1
                out.append("<ol start=\"%d\">" % start if ordered and start != 1 else "<%s>" % opened)
            out.append(self.list_item(item["lines"]))
        out.append("</%s>" % opened)
        return "".join(out)

    def list_item(self, lines: list[str]) -> str:
        first, rest = lines[0], lines[1:]
        while rest and not rest[-1].strip():
            rest.pop()
        cut = min((indent_of(line) for line in rest if line.strip()), default=0)
        body = [first] + [line[cut:] for line in rest]
        attrs = box = ""
        task = re.match(r"^\[([ xX])\]\s+(.*)$", first)
        if task:
            done = task.group(1) != " "
            attrs = ' class="task"'
            box = '<input type="checkbox" disabled%s aria-label="%s">' % (" checked" if done else "", TASK_LABEL[self.lang if self.lang in TASK_LABEL else "en"][int(done)])
            body[0] = task.group(2)
        blocks = self.render("\n".join(body))
        parts = [self.html_of([b]) for b in blocks]
        tight = not any("<pre" in part or "table-wrap" in part for part in parts[1:])  # a block that scrolls: under a paragraph of its own
        if tight and parts and parts[0].startswith("<p>") and parts[0].endswith("</p>"):  # the first paragraph is the text of the item
            parts[0] = parts[0][3:-4]
        return "<li%s>%s%s</li>" % (attrs, box, "".join(parts))

def section_texts(markdown: str) -> list[str]:
    """The Markdown before the first section, then of each section: a # or ## line, never one of a fence or of a :::html block."""
    parts: list[list[str]] = [[]]
    fence = ""
    raw = False
    for line in markdown.split("\n"):
        if fence:
            fence = "" if closes_fence(fence, line) else fence
        elif raw:
            raw = line.strip() != ":::"
        elif re.match(r"^:::html\s*$", line):
            raw = True
        elif opening_fence(line):
            fence = opening_fence(line)
        elif re.match(r"^#{1,2}\s", line):
            parts.append([])
            continue
        parts[-1].append(line)
    return ["\n".join(part) for part in parts]

def plain_text(markdown: str) -> str:
    """Text of a block of Markdown without the markup (search index)."""
    text = re.sub(r"^[ \t]*(`{3,}|~{3,})[^\n]*\n.*?^[ \t]*\1[`~]*[ \t]*$", " ", markdown, flags=re.S | re.M)
    text = re.sub(r"^:::html\s*$.*?^:::\s*$", " ", text, flags=re.S | re.M)
    text = re.sub(r"^:::\w*.*$", " ", text, flags=re.M)
    text = re.sub(r"<!--.*?-->", " ", text, flags=re.S)
    text = re.sub(r"!\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"\[((?:[^\[\]]|\[[^\]]*\])*)\]\([^)]*\)", r"\1", text)
    text = re.sub(AUTOLINK, r"\1", text)
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"\{#[\w-]+\}|^\s*-{3,}\s*$", " ", text, flags=re.M)
    text = re.sub(r"^\{:[^}]*\}\s*$", " ", text, flags=re.M)
    text = re.sub(r"\[![A-Za-z]+\]", " ", text)  # the kind of a callout
    text = re.sub(r"^(\s*[-*+]\s+)\[[ xX]\]\s", r"\1", text, flags=re.M)  # the box of a task
    text = re.sub(r"[`*]|~~", "", text)
    text = re.sub(r"\\([%s])" % ESCAPABLE, r"\1", text)
    text = re.sub(r"[>#|]", " ", text)
    text = re.sub(r"(?<![\w-])[-:]{3,}(?![\w-])", " ", text)  # the separator rows of the tables
    return re.sub(r"\s+", " ", text).strip()
