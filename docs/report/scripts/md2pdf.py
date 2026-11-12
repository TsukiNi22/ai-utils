#!/usr/bin/env python3
"""
Convert a Markdown report to a PDF in the user's report style (WeasyPrint + templates/report.css).

Usage:
    md2pdf.py <input.md> [-o output.pdf] [--format both|pdf|md] [--css extra.css] [--footer "text"] [--html out.html] [--lang en]

- --format: both (default) keeps the .md next to the .pdf, pdf removes the .md source after a successful
  conversion, md only validates the Markdown and writes no PDF.

- Markdown: tables, fenced code, nested lists (4 spaces), attributes ({: .warn} after a blockquote, {: .page}
  after a heading to start it on a new page), GitHub callouts (> [!NOTE], [!TIP], [!WARNING]...), raw HTML allowed.
- Footer: "<first H1 text> — page / pages" (--footer to replace the H1 text), cut when it is too long for one line.
- Fonts: Noto Sans / Noto Sans Mono / Noto Serif (installed on the system).
- Tables fit the page whatever they hold: fit_tables gives each column the width of its words (never one word per line next to
  an empty column), smaller or on a landscape page when they do not fit; an unbreakable text breaks only inside its own cell.
- Checked after the conversion (pdftotext, when it is installed): a word past the edge of a page is told on stderr.
"""

import argparse
import html as htmllib
import math
import os
import re
import shutil
import subprocess
import sys

import markdown
from weasyprint import CSS, HTML

CSS_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "templates", "report.css")

def commonmark_lists(src: str) -> str:
    """Python-Markdown needs an empty line before a list (CommonMark doesn't): add it, outside code fences."""
    out, fence, prev = [], False, ""
    item = re.compile(r"^\s*([-*+]|\d+[.)])\s+")
    for line in src.split("\n"):
        if line.lstrip().startswith(("```", "~~~")):
            fence = not fence
        if not fence and item.match(line) and prev.strip() and not item.match(prev) and not prev.startswith((" ", "\t", ">")):
            out.append("")
        out.append(line)
        prev = line
    return "\n".join(out)

def commonmark_tables(src: str) -> str:
    """A table glued to the line before it (fine for GitHub): an empty line added, else Python-Markdown prints its pipes as text."""
    out, fence = [], False
    lines = src.split("\n")
    separator = re.compile(r"^\s*\|?\s*:?-+:?\s*(\|\s*:?-+:?\s*)*\|?\s*$")
    for at, line in enumerate(lines):
        if line.lstrip().startswith(("```", "~~~")):
            fence = not fence
        head = "|" in line and at + 1 < len(lines) and "-" in lines[at + 1] and separator.match(lines[at + 1]) is not None and "|" in lines[at + 1]
        if not fence and head and at > 0 and lines[at - 1].strip() and "|" not in lines[at - 1]:
            out.append("")
        out.append(line)
    return "\n".join(out)

CALLOUTS = {"NOTE": ("Note", ""), "TIP": ("Tip", "ok"), "IMPORTANT": ("Important", ""), "WARNING": ("Warning", "warn"),
            "CAUTION": ("Caution", "warn")}

def github_callouts(src: str) -> str:
    """GitHub callouts (> [!NOTE], [!TIP], [!IMPORTANT], [!WARNING], [!CAUTION]): a titled callout, coloured by its kind."""
    out, fence, kind = [], False, ""
    marker = re.compile(r"^>\s*\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]\s*$")
    for line in src.split("\n"):
        if line.lstrip().startswith(("```", "~~~")):
            fence = not fence
        if kind and not line.startswith(">"): # the end of the callout: its colour
            if CALLOUTS[kind][1]:
                out.append("{: ." + CALLOUTS[kind][1] + "}")
            kind = ""
        found = None if fence else marker.match(line)
        if found:
            kind = found.group(1)
            out.append("> **" + CALLOUTS[kind][0] + "**")
            out.append(">")
            continue
        out.append(line)
    if kind and CALLOUTS[kind][1]:
        out.append("{: ." + CALLOUTS[kind][1] + "}")
    return "\n".join(out)

def split_callouts(src: str) -> str:
    """Two callouts in a row (only an empty line between them) stay two: Python-Markdown makes one of them, with the colour of the last one."""
    out, fence, quoted, blank = [], False, False, False
    for line in src.split("\n"):
        if line.lstrip().startswith(("```", "~~~")):
            fence = not fence
        if not fence and line.startswith(">") and quoted and blank:
            out.append("<!-- -->")
            out.append("")
        if line.strip():
            if not re.fullmatch(r"\{:[^}]*\}", line.strip()): # its colour line belongs to it
                quoted = line.startswith(">") and not fence
            blank = False
        else:
            blank = True
        out.append(line)
    return "\n".join(out)

def plain_emoji(body: str) -> str:
    """Every emoji in its text form (U+FE0E: one colour, Noto Emoji): the colour font (COLR) crashes WeasyPrint at random (a segfault in hb_vector_paint), Pango takes it whatever the CSS says."""
    return re.sub("([\\U0001F000-\\U0001FAFF\u2600-\u27BF\u2B00-\u2BFF\u231A\u231B\u23E9-\u23FA\u25AA-\u25FE\u2934\u2935\u3030\u303D\u3297\u3299])[\uFE0E\uFE0F]?", "\\1\uFE0E", body)

def task_lists(body: str) -> str:
    """- [ ] and - [x]: a box, no bullet."""
    body = re.sub(r"<li>(\s*<p>)?\s*\[ \]\s", lambda m: '<li class="task">' + (m.group(1) or "") + "\u2610 ", body)
    return re.sub(r"<li>(\s*<p>)?\s*\[[xX]\]\s", lambda m: '<li class="task">' + (m.group(1) or "") + "\u2611 ", body)

# the room of a table, in em of its font: A4 and its margins (521 pt, 768 pt on a landscape page) at the size of each class of report.css
PAGE_FITS = [("", 69.5), ("dense", 80.2), ("wide", 109.8), ("wide dense", 139.7)]
CELL_PADDING = 1.35 # the padding and the borders of a cell
CHIP_PADDING = 0.8  # the padding of an inline code
WORD_KEPT = 16.0    # a longer word (a path, a hash, an address) may break, the others never
LINE_WANTED = 8.5   # a cell with a sentence: at least so much (never a word per line)
SPACE = 0.26

def text_em(text: str, bold: bool, mono: bool) -> float:
    """The width of a text, in em: what Noto Sans gives, a little more to be safe (the width of a column must hold its longest word, whatever it is)."""
    if mono:
        return len(text) * 0.52
    total = 0.0
    for char in text:
        if char in "iljt.,:;'|!fr()[]{}I\"` ":
            total += 0.32
        elif char in "mwMW@%&\u2014":
            total += 0.92
        elif ord(char) >= 0x2E80:
            total += 1.02 # CJK: a square
        elif char.isupper():
            total += 0.7
        elif char.isdigit():
            total += 0.6
        else:
            total += 0.58
    return total * (1.1 if bold else 1.0)

def cell_size(cell: str, head: bool) -> tuple[float, float]:
    """The width of the longest word of a cell and of its whole text, in em."""
    bold = head or "<strong" in cell or "<b>" in cell
    longest, whole = 0.0, 0.0
    for at, piece in enumerate(re.split(r"<code[^>]*>(.*?)</code>", cell, flags=re.S)):
        mono = at % 2 == 1
        words = htmllib.unescape(re.sub(r"<[^>]+>", " ", piece)).split()
        sizes = [text_em(word, bold, mono) + (CHIP_PADDING if mono else 0.0) for word in words]
        longest = max(longest, max(sizes, default=0.0))
        whole += sum(sizes) + SPACE * len(sizes)
    return longest, whole

def fit_tables(body: str) -> str:
    """A width for each column from what it holds (table-layout: fixed: never wider than the page, whatever the table), the size or the page it needs."""
    def fit(match: re.Match) -> str:
        table = match.group(0)
        if "colspan" in table or "rowspan" in table or "<colgroup" in table:
            return table
        rows = [re.findall(r"<(t[hd])[^>]*>(.*?)</t[hd]>", row, re.S) for row in re.findall(r"<tr[^>]*>(.*?)</tr>", table, re.S)]
        columns = max((len(row) for row in rows), default=0)
        if columns == 0:
            return table
        longest, whole = [0.5] * columns, [0.5] * columns
        for row in rows:
            for at, (tag, cell) in enumerate(row):
                word, text = cell_size(cell, tag == "th")
                longest[at] = max(longest[at], word)
                whole[at] = max(whole[at], text)
        want = [min(WORD_KEPT, max(longest[at], min(whole[at], LINE_WANTED))) for at in range(columns)]
        need = sum(want) + columns * CELL_PADDING
        kind, room = next(((name, size) for name, size in PAGE_FITS if need <= size), PAGE_FITS[-1])
        scale = min(1.0, room / need)
        base = [(want[at] + CELL_PADDING) * scale for at in range(columns)]
        left = max(0.0, room - sum(base))
        weight = [math.sqrt(max(0.0, whole[at] - want[at])) for at in range(columns)]
        if sum(weight) == 0:
            weight = base
        widths = [base[at] + left * weight[at] / sum(weight) for at in range(columns)]
        cols = "".join(f'<col style="width:{100 * width / sum(widths):.2f}%">' for width in widths)
        opened = f'<table class="fit {kind}">' if kind else '<table class="fit">'
        table = re.sub(r"<table[^>]*>", opened + "<colgroup>" + cols + "</colgroup>", table, count=1)
        # a head without any text (a table of two columns label / value): none, never an empty band
        return re.sub(r"<thead>\s*<tr>(\s*<th[^>]*>\s*</th>)*\s*</tr>\s*</thead>", "", table)
    body = re.sub(r"<table[^>]*>.*?</table>", fit, body, flags=re.S)
    # the title of a table of a landscape page goes with it: never alone on the page before
    return re.sub(r'(<(h[2-6])[^>]*>(?:(?!</\2>).)*</\2>\s*)(<table class="fit wide[^>]*>.*?</table>)', r'<div class="wide-page">\1\3</div>', body, flags=re.S)

def check_pdf(path: str) -> int:
    """A word past the edge of a page, told: what no CSS rule can be trusted for (an image, a raw HTML table)."""
    if shutil.which("pdftotext") is None:
        return 0
    box = subprocess.run(["pdftotext", "-bbox", path, "-"], capture_output=True, text=True, errors="replace").stdout
    problems = 0
    for number, page in enumerate(re.findall(r'<page width="([\d.]+)" height="[\d.]+">(.*?)</page>', box, re.S), 1):
        edge = float(page[0])
        for x_max, word in re.findall(r'xMax="([\d.]+)"[^>]*>([^<]*)</word>', page[1]):
            if float(x_max) > edge - 5:
                problems += 1
                if problems <= 5:
                    sys.stderr.write(f"warning: page {number}: past the edge of the page: {word[:60]}\n")
    return problems

def callout_classes(body: str) -> str:
    """{: .warn} / {: .ok} after a callout lands on its last paragraph (a lazy continuation line): moved to the blockquote."""
    def move(match: re.Match) -> str:
        inner = match.group(1)
        found = re.search(r'<p class="(warn|ok)">((?:(?!<p[ >]).)*?)</p>\s*$', inner, re.S)
        if not found:
            return match.group(0)
        inner = inner[:found.start()] + "<p>" + found.group(2) + "</p>" + inner[found.end():]
        return f'<blockquote class="{found.group(1)}">{inner}</blockquote>'
    return re.sub(r"<blockquote>(.*?)</blockquote>", move, body, flags=re.S)

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("-o", "--output")
    ap.add_argument("--css", action="append", default=[])
    ap.add_argument("--footer")
    ap.add_argument("--html")
    ap.add_argument("--lang", default="en")
    ap.add_argument("--format", choices=["both", "pdf", "md"], default="both")
    a = ap.parse_args()
    if not os.path.isfile(a.input):
        sys.stderr.write(f"md2pdf: {a.input} not found\n")
        return 1

    src = split_callouts(github_callouts(commonmark_tables(commonmark_lists(open(a.input, encoding="utf-8").read()))))
    if a.format == "md":
        print(a.input)
        return 0
    body = markdown.markdown(src, extensions=["tables", "fenced_code", "sane_lists", "attr_list", "md_in_html"],
                             tab_length=4, output_format="html5")
    body = plain_emoji(fit_tables(task_lists(callout_classes(body))))
    title = re.search(r"<h1[^>]*>(.*?)</h1>", body, re.S)
    plain = re.sub(r"<[^>]+>", "", title.group(1)).strip() if title else os.path.splitext(os.path.basename(a.input))[0]
    extra = ""
    footer = a.footer or (plain[:87].rstrip() + "…" if len(plain) > 90 else "") # fixed footer text instead of the H1; a long H1: never over the width of the page
    if footer:
        text = htmllib.unescape(footer).replace("\\", "\\\\").replace('"', '\\"')
        extra = '@page { @bottom-center { content: "' + text + ' — " counter(page) " / " counter(pages); } }'

    html = f'<!doctype html><html lang="{a.lang}"><head><meta charset="utf-8"><title>{plain}</title>' \
           f'<style>{extra}</style></head><body>{body}</body></html>'
    if a.html:
        with open(a.html, "w", encoding="utf-8") as f:
            f.write(html.replace("<style>", f'<style>{open(CSS_PATH).read()}\n'))
    out = a.output or os.path.splitext(a.input)[0] + ".pdf"
    base = os.path.dirname(os.path.abspath(a.input))
    HTML(string=html, base_url=base).write_pdf(out, stylesheets=[CSS(CSS_PATH)] + [CSS(p) for p in a.css])
    check_pdf(out)
    if a.format == "pdf":
        os.remove(a.input)
    print(f"{out}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
