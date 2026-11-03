#!/usr/bin/env python3
"""
Check a protocol documentation (Markdown and / or the HTML sections of the technical page).

Usage:
    check_protocol.py <PROTOCOL.md|README.md|docs/technical.html>... [--allow-placeholders] [--fr]

Markdown: every link (#anchor) reaches a heading, every type of the types table (| Byte (hexa) | Type |) has its
section, every frame of the summary block (/* Type */ ENQ ... EOT) uses the byte of its type, the codes of each code
table (| Code ... |) are unique, no {{placeholder}} left, no typo of the reference doc. A table without an empty line before it
(raw pipes in the PDF), a line of a code block wider than the page of the PDF (it wraps, a diagram falls apart).
HTML: every href="#id" reaches an id, no {{placeholder}} left, no typo. A raw <tag> of a frame, also <code> or <data> in a <code> (the
browser swallows it: write &lt;tag&gt;), an HTML element inside a drawing (<span> in an <svg>: the browser leaves the drawing there).
Exit code: 0 = clean, 1 = issues found, 2 = usage error.
"""

import argparse
import re
import sys
from html.parser import HTMLParser

TYPOS = {"steap": "step", "awnser": "answer", "dosen't": "doesn't", "reciever": "receiver", "it's type": "its type",
         "informations": "information", "transfert": "transfer", "reconniction": "reconnection"}
FRENCH_WORDS = {"informations", "transfert"}  # typos of the reference, right in French
CODE_COLUMNS_MAX = 110  # characters of a code block on a page of the PDF: A4, 7.5 pt mono (report.css)
HTML_TAGS = set("""a abbr address area article aside audio b base bdi bdo blockquote body br button canvas caption cite code col colgroup data datalist dd del details dfn dialog
div dl dt em embed fieldset figcaption figure footer form h1 h2 h3 h4 h5 h6 head header hgroup hr html i iframe img input ins kbd label legend li link main map mark menu meta meter nav
noscript object ol optgroup option output p picture pre progress q rp rt ruby s samp script search section select slot small source span strong style sub summary sup table tbody td
template textarea tfoot th thead time title tr track u ul var video wbr acronym big center font nobr strike tt""".split())
SVG_TAGS = set("""svg a animate animatemotion animatetransform circle clippath defs desc ellipse filter foreignobject g image line lineargradient marker mask metadata mpath path pattern
polygon polyline radialgradient rect script set stop style switch symbol text textpath title tspan use view""".split())
MATH_TAGS = set("""math annotation maction menclose merror mfrac mi mmultiscripts mn mo mover mpadded mphantom mprescripts mroot mrow ms mspace msqrt mstyle msub msubsup msup mtable mtd mtext
mtr munder munderover none semantics""".split())
SVG_BREAKOUT = set("""b big blockquote body br center code dd div dl dt em embed h1 h2 h3 h4 h5 h6 head hr i img li listing menu meta nobr ol p pre ruby s small span strong strike sub sup
table tt u ul var""".split())  # what makes the HTML parser of a browser close the <svg> it is in
CODE_INLINE = set("a abbr b br del em i ins kbd mark s samp small span strong sub sup u var wbr".split())  # all that may really be in a <code>: another name is a word of a frame

def anchor(title: str) -> str:
    """The GitHub anchor of a heading: lower case, punctuation removed, spaces as '-'."""
    text = re.sub(r"`|\*\*|\[|\]\([^)]*\)", "", title.strip().lower())
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")

def outside_code(lines: list) -> list:
    """(number, line, in_code) for every line, code fences tracked."""
    out, fence = [], False
    for number, line in enumerate(lines, 1):
        if line.lstrip().startswith(("```", "~~~")):
            fence = not fence
            out.append((number, line, True))
            continue
        out.append((number, line, fence))
    return out

def tables(lines: list) -> list:
    """The Markdown tables: (first line number, header cells, rows of cells)."""
    found, current = [], None
    for number, line, code in outside_code(lines):
        cells = [c.strip() for c in line.strip().strip("|").split("|")] if not code and line.strip().startswith("|") else None
        if cells is None:
            if current:
                found.append(current)
            current = None
            continue
        if current is None:
            current = (number, cells, [])
        elif not all(re.fullmatch(r":?-{2,}:?", c) for c in cells):
            current[2].append((number, cells))
    if current:
        found.append(current)
    return found

def check_layout(path: str, lines: list, issues: list) -> None:
    """What the PDF cannot show: a table glued to the text before it, a code line wider than the page."""
    for number, line, code in outside_code(lines):
        if code:
            if not line.lstrip().startswith(("```", "~~~")) and len(line.rstrip()) > CODE_COLUMNS_MAX:
                issues.append(f"{path}:{number}: code line of {len(line.rstrip())} characters: over the {CODE_COLUMNS_MAX} of a page of the PDF, cut it (one field per line)")
            continue
        before = lines[number - 2].strip() if number >= 2 else ""
        after = lines[number].strip() if number < len(lines) else ""
        if line.strip().startswith("|") and before and not before.startswith(("|", ">", "#")) and re.fullmatch(r"\|?\s*:?-{2,}.*", after):
            issues.append(f"{path}:{number}: a table needs an empty line before it: the PDF shows raw pipes")

def check_markdown(path: str, text: str, issues: list, placeholders: bool, french: bool) -> None:
    lines = text.split("\n")
    rows = outside_code(lines)
    headings = {}
    for number, line, code in rows:
        match = None if code else re.match(r"^(#{1,6})\s+(.*?)\s*#*\s*$", line)
        if match:
            headings.setdefault(anchor(match.group(2)), number)
    # links to the headings
    for number, line, code in rows:
        if code:
            continue
        for target in re.findall(r"\]\(#([^)]+)\)", line):
            if target not in headings and not (placeholders and "{{" in target):
                issues.append(f"{path}:{number}: link to '#{target}': no such heading")
    # the types table: each type has its section; the summary frames use the byte of their type
    types = {}
    for start, header, body in tables(lines):
        if header and header[0].lower().startswith("byte") and len(header) > 1 and header[1].lower().startswith("type"):
            for number, cells in body:
                byte = re.match(r"([A-Z]{2,3})", cells[0])
                name = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", cells[1]).strip() if len(cells) > 1 else ""
                if byte and name:
                    types[name.lower()] = (byte.group(1), number)
                    if anchor(name) not in headings:
                        issues.append(f"{path}:{number}: type '{name}' has no section (### {name})")
        if header and header[0].lower().startswith("code"):
            seen = {}
            for number, cells in body:
                code = re.search(r"`?(-?\d+)`?", cells[0])
                if code and code.group(1) in seen:
                    issues.append(f"{path}:{number}: code {code.group(1)} already used line {seen[code.group(1)]} in this table")
                elif code:
                    seen[code.group(1)] = number
    for number, line, code in rows:
        frame = re.match(r"^\s*/\*\s*(.+?)\s*\*/\s*ENQ\s+\S+\s+ETB\s+([A-Z]{2,3})\b", line) if code else None
        if frame and types:
            name, byte = frame.group(1).lower(), frame.group(2)
            if name not in types:
                issues.append(f"{path}:{number}: frame '{frame.group(1)}' is not in the types table")
            elif types[name][0] != byte:
                issues.append(f"{path}:{number}: frame '{frame.group(1)}' uses {byte}, the types table says {types[name][0]} (line {types[name][1]})")
    check_layout(path, lines, issues)
    common(path, lines, issues, placeholders, french)

class TagChecker(HTMLParser):
    """What a browser does not show as it was written: a word of a frame taken for a tag, an HTML element in a drawing."""

    def __init__(self, path: str, issues: list) -> None:
        super().__init__(convert_charrefs=True)
        self.path, self.issues = path, issues
        self.in_code, self.svg_depth, self.foreign_depth = False, 0, 0

    def tell(self, message: str) -> None:
        self.issues.append(f"{self.path}:{self.getpos()[0]}: {message}")

    def handle_starttag(self, tag: str, attrs: list) -> None:
        known = tag in HTML_TAGS or tag in SVG_TAGS or tag in MATH_TAGS or "-" in tag or (self.svg_depth > 0 and tag.startswith("fe"))
        if not known or (self.in_code and tag not in CODE_INLINE):
            self.tell(f"<{tag}> in the page: the browser hides it as a tag, write &lt;{tag}&gt;")
        if self.svg_depth and not self.foreign_depth and tag in SVG_BREAKOUT:
            self.tell(f"<{tag}> inside a drawing: the browser leaves the drawing there, all that follows falls out of it (a text per language: <text data-lang>)")
        if tag == "code":
            self.in_code = True
        elif tag == "svg":
            self.svg_depth += 1
        elif tag == "foreignobject":
            self.foreign_depth += 1

    def handle_endtag(self, tag: str) -> None:
        if tag == "code":
            self.in_code = False
        elif tag == "svg":
            self.svg_depth = max(0, self.svg_depth - 1)
        elif tag == "foreignobject":
            self.foreign_depth = max(0, self.foreign_depth - 1)

def check_html(path: str, text: str, issues: list, placeholders: bool, french: bool) -> None:
    ids = set(re.findall(r'\bid="([^"]+)"', text))
    lines = text.split("\n")
    for number, line in enumerate(lines, 1):
        for target in re.findall(r'href="#([^"]+)"', line):
            if target not in ids:
                issues.append(f"{path}:{number}: link to '#{target}': no such id")
    TagChecker(path, issues).feed(text)
    common(path, lines, issues, placeholders, french)

def common(path: str, lines: list, issues: list, placeholders: bool, french: bool) -> None:
    for number, line in enumerate(lines, 1):
        if not placeholders and "{{" in line:
            issues.append(f"{path}:{number}: placeholder left: {re.search(r'{{[^}]*}}', line).group(0) if re.search(r'{{[^}]*}}', line) else '{{'}")
        lower = re.sub(r'<(\w+)[^>]*data-lang="fr"[^>]*>.*?</\1>', "", line).lower()  # the French of a bilingual page
        for typo, fix in TYPOS.items():
            if not (french and typo in FRENCH_WORDS) and re.search(r"\b" + re.escape(typo) + r"\b", lower):
                issues.append(f"{path}:{number}: '{typo}' -> '{fix}'")

def main() -> int:
    parser = argparse.ArgumentParser(description="Check a protocol documentation (Markdown / HTML).")
    parser.add_argument("files", nargs="+")
    parser.add_argument("--allow-placeholders", action="store_true", help="templates: {{...}} allowed")
    parser.add_argument("--fr", action="store_true", help="a document in French: informations, transfert are no typos")
    args = parser.parse_args()
    issues = []
    for path in args.files:
        try:
            text = open(path, encoding="utf-8").read()
        except OSError as error:
            sys.stderr.write(f"check_protocol: {path}: {error.strerror}\n")
            return 2
        if path.endswith((".html", ".htm")):
            check_html(path, text, issues, args.allow_placeholders, args.fr)
        else:
            check_markdown(path, text, issues, args.allow_placeholders, args.fr)
    for issue in issues:
        print(issue)
    print(f"{len(issues)} issue(s) in {len(args.files)} file(s)")
    return 1 if issues else 0

if __name__ == "__main__":
    sys.exit(main())
