#!/usr/bin/env python3
"""
Convert a Markdown report to a PDF in the user's report style (WeasyPrint + templates/report.css).

Usage:
    md2pdf.py <input.md> [-o output.pdf] [--format both|pdf|md] [--css extra.css] [--footer "text"] [--html out.html] [--lang en]

- --format: both (default) keeps the .md next to the .pdf, pdf removes the .md source after a successful
  conversion, md only validates the Markdown and writes no PDF.

- Markdown: tables, fenced code, nested lists (4 spaces), attributes ({: .warn} after a blockquote, {: .page}
  after a heading to start it on a new page), raw HTML allowed.
- Footer: "<first H1 text> — page / pages" (--footer to replace the H1 text).
- Fonts: Noto Sans / Noto Sans Mono / Noto Serif (installed on the system).
"""

import argparse
import os
import re
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

    src = commonmark_lists(open(a.input, encoding="utf-8").read())
    if a.format == "md":
        print(a.input)
        return 0
    body = markdown.markdown(src, extensions=["tables", "fenced_code", "sane_lists", "attr_list", "md_in_html"],
                             tab_length=4, output_format="html5")
    title = re.search(r"<h1[^>]*>(.*?)</h1>", body, re.S)
    plain = re.sub(r"<[^>]+>", "", title.group(1)).strip() if title else os.path.splitext(os.path.basename(a.input))[0]
    extra = ""
    if a.footer: # fixed footer text instead of the H1
        text = a.footer.replace("\\", "\\\\").replace('"', '\\"')
        extra = '@page { @bottom-center { content: "' + text + ' — " counter(page) " / " counter(pages); } }'

    html = f'<!doctype html><html lang="{a.lang}"><head><meta charset="utf-8"><title>{plain}</title>' \
           f'<style>{extra}</style></head><body>{body}</body></html>'
    if a.html:
        with open(a.html, "w", encoding="utf-8") as f:
            f.write(html.replace("<style>", f'<style>{open(CSS_PATH).read()}\n'))
    out = a.output or os.path.splitext(a.input)[0] + ".pdf"
    base = os.path.dirname(os.path.abspath(a.input))
    HTML(string=html, base_url=base).write_pdf(out, stylesheets=[CSS(CSS_PATH)] + [CSS(p) for p in a.css])
    if a.format == "pdf":
        os.remove(a.input)
    print(f"{out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
