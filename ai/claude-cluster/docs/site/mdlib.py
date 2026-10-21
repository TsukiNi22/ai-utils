#!/usr/bin/env python3
# =============================================================================
# mdlib.py - the small Markdown subset of the claude-cluster docs (deterministic, no dependency)
#   front matter  +++ TOML +++ (first lines)
#   headings      # .. ####, an explicit id with  ## Title {#id}
#   blocks        paragraphs, - / 1. lists (one nested level, 2 spaces), tables, ``` fences, > quotes,
#                 callouts  > [!NOTE] / [!WARN] / [!OK] / [!PLANNED] / [!SOON], --- rule, :::html raw ::: block
#   inline        `code`, **bold**, *italic*, [text](url), [text](page:slug#id), <kbd> <br> <b> <i> <sub> <sup>
# =============================================================================

import html
import re
import tomllib

ALLOWED_TAGS = ("kbd", "br", "b", "i", "em", "strong", "sub", "sup", "span", "code")
CALLOUTS = {"NOTE": "", "WARN": " warn", "OK": " ok", "PLANNED": " warn", "SOON": " warn"}
CALLOUT_LABEL = {"PLANNED": ("Planned", "Prévu"), "SOON": ("Coming soon", "Bientôt")}
BLOCK_START = r"^(#{1,4}\s|```|:::|>|\||---+\s*$|(\s*)([-*]|\d+\.)\s)"


def split_front(text: str) -> tuple[dict, str]:
    if not text.startswith("+++\n"):
        return {}, text
    end = text.index("\n+++\n", 4)
    return tomllib.loads(text[4:end]), text[end + 5:]


def slugify(text: str) -> str:
    text = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return text or "section"


class Renderer:
    def __init__(self, lang: str):
        self.lang = lang

    # ----- inline -----
    def inline(self, text: str) -> str:
        parts: list[str] = []
        pos = 0
        for m in re.finditer(r"`([^`]+)`", text):
            parts.append(self._inline_plain(text[pos:m.start()]))
            parts.append("<code>" + html.escape(m.group(1), quote=False) + "</code>")
            pos = m.end()
        parts.append(self._inline_plain(text[pos:]))
        return "".join(parts)

    def _inline_plain(self, text: str) -> str:
        tags = "|".join(ALLOWED_TAGS)
        keep: list[str] = []

        def stash(m: re.Match) -> str:
            keep.append(m.group(0))
            return "\x00%d\x00" % (len(keep) - 1)

        text = re.sub(r"</?(?:%s)\s*/?>" % tags, stash, text)
        text = html.escape(text, quote=False)
        text = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", lambda m: '<a href="%s">%s</a>' % (self.url(m.group(2)), m.group(1)), text)
        text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
        text = re.sub(r"(?<![*\w])\*([^*\s][^*]*)\*(?![*\w])", r"<em>\1</em>", text)
        return re.sub(r"\x00(\d+)\x00", lambda m: keep[int(m.group(1))], text)

    @staticmethod
    def url(url: str) -> str:
        if url.startswith("page:"):
            page, _, frag = url[5:].partition("#")
            return page + ".html" + ("#" + frag if frag else "")
        return url

    # ----- blocks -----
    def render(self, text: str) -> list[dict]:
        """Blocks: {"h": level, "id": id, "title": html, "raw": text} or {"html": html}."""
        lines = text.split("\n")
        out: list[dict] = []
        i = 0
        while i < len(lines):
            line = lines[i]
            if not line.strip():
                i += 1
                continue
            m = re.match(r"^(#{1,4})\s+(.*?)(?:\s+\{#([\w-]+)\})?\s*$", line)
            if m:
                level, title = len(m.group(1)), m.group(2)
                out.append({"h": level, "id": m.group(3) or slugify(re.sub(r"[`*]", "", title)), "title": self.inline(title), "raw": re.sub(r"[`*]", "", title)})
                i += 1
            elif line.startswith("```"):
                lang = line[3:].strip()
                title = ""
                if " " in lang:
                    lang, _, title = lang.partition(" ")
                code: list[str] = []
                i += 1
                while i < len(lines) and not lines[i].startswith("```"):
                    code.append(lines[i])
                    i += 1
                i += 1
                attr = ' data-title="%s"' % html.escape(title.strip('"')) if title else ""
                out.append({"html": '<pre%s><code class="language-%s">%s</code></pre>' % (attr, lang or "text", html.escape("\n".join(code), quote=False))})
            elif line.startswith(":::html"):
                raw: list[str] = []
                i += 1
                while i < len(lines) and not lines[i].startswith(":::"):
                    raw.append(lines[i])
                    i += 1
                i += 1
                out.append({"html": "\n".join(raw)})
            elif line.startswith("|") and i + 1 < len(lines) and re.match(r"^\|[\s:|-]+\|\s*$", lines[i + 1]):
                rows: list[str] = []
                while i < len(lines) and lines[i].startswith("|"):
                    rows.append(lines[i])
                    i += 1
                out.append({"html": self.table(rows)})
            elif line.startswith(">"):
                quote: list[str] = []
                while i < len(lines) and lines[i].startswith(">"):
                    quote.append(re.sub(r"^>\s?", "", lines[i]))
                    i += 1
                out.append({"html": self.quote(quote)})
            elif re.match(r"^---+\s*$", line):
                out.append({"html": "<hr>"})
                i += 1
            elif re.match(r"^(\s*)([-*]|\d+\.)\s+", line):
                items: list[str] = []
                while i < len(lines) and (re.match(r"^(\s*)([-*]|\d+\.)\s+", lines[i]) or (lines[i].startswith("  ") and lines[i].strip())):
                    items.append(lines[i])
                    i += 1
                out.append({"html": self.list_(items)})
            else:
                para: list[str] = []
                while i < len(lines) and lines[i].strip() and (not para or not re.match(BLOCK_START, lines[i])):
                    para.append(lines[i].strip())
                    i += 1
                out.append({"html": "<p>" + self.inline(" ".join(para)) + "</p>"})
        return out

    def table(self, rows: list[str]) -> str:
        def cells(row: str) -> list[str]:
            row = row.strip()
            row = row[1:-1] if row.endswith("|") and not row.endswith("\\|") else row[1:]
            return [c.strip().replace("\\|", "|") for c in re.split(r"(?<!\\)\|", row)]

        head = cells(rows[0])
        out = ['<div class="table-wrap"><table><thead><tr>' + "".join("<th>%s</th>" % self.inline(c) for c in head) + "</tr></thead><tbody>"]
        for r in rows[2:]:
            out.append("<tr>" + "".join("<td>%s</td>" % self.inline(c) for c in cells(r)) + "</tr>")
        out.append("</tbody></table></div>")
        return "".join(out)

    def quote(self, quote: list[str]) -> str:
        kind = ""
        label = ""
        m = re.match(r"^\[!(\w+)\]\s*(.*)$", quote[0])
        if m and m.group(1) in CALLOUTS:
            kind = m.group(1)
            quote = ([m.group(2)] if m.group(2) else []) + quote[1:]
            if kind in CALLOUT_LABEL:
                label = "<strong>%s.</strong> " % CALLOUT_LABEL[kind][0 if self.lang == "en" else 1]
        body = "".join(b.get("html", "") for b in self.render("\n".join(quote)))
        if label:
            body = body.replace("<p>", "<p>" + label, 1) if body.startswith("<p>") else "<p>" + label + "</p>" + body
        return '<div class="callout%s">%s</div>' % (CALLOUTS.get(kind, ""), body)

    def list_(self, items: list[str]) -> str:
        def parse(lines: list[str], indent: int) -> str:
            tag = "ol" if re.match(r"^\s*\d+\.", lines[0]) else "ul"
            out = ["<%s>" % tag]
            k = 0
            while k < len(lines):
                m = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", lines[k])
                if not m or len(m.group(1)) != indent:
                    k += 1
                    continue
                text = [m.group(3)]
                sub: list[str] = []
                k += 1
                while k < len(lines):
                    mm = re.match(r"^(\s*)([-*]|\d+\.)\s+", lines[k])
                    if mm and len(mm.group(1)) <= indent:
                        break
                    if mm:
                        sub.append(lines[k])
                    else:
                        text.append(lines[k].strip())
                    k += 1
                nested = parse(sub, len(re.match(r"^\s*", sub[0]).group(0))) if sub else ""
                out.append("<li>" + self.inline(" ".join(text)) + nested + "</li>")
            out.append("</%s>" % tag)
            return "".join(out)

        return parse(items, len(re.match(r"^\s*", items[0]).group(0)))


def plain_text(markdown: str) -> str:
    """Text of a block of Markdown without the markup (search index)."""
    text = re.sub(r"```.*?```", " ", markdown, flags=re.S)
    text = re.sub(r":::html.*?:::", " ", text, flags=re.S)
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"\{#[\w-]+\}|^\s*-{3,}\s*$", " ", text, flags=re.M)
    text = re.sub(r"[`*>#|]", " ", text)
    return re.sub(r"\s+", " ", text).strip()
