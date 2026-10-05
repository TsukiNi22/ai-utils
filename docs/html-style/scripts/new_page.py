#!/usr/bin/env python3
"""
Build a self-contained HTML page in the user's style: templates/page.html with templates/style.css and
templates/base.js inlined, the given body, extra CSS / JS (inlined after the base ones) and the placeholders filled.

Usage:
    new_page.py <body.html> -o <out.html> [--css extra.css ...] [--js extra.js ...] [--actions actions.html]
                [--lang-switch] [--set KEY=VALUE ...] [--force]

- <body.html>: content of the page layout (an optional <aside class="sidebar" id="sidebar"> + <main>), may hold
  {{KEY}} placeholders too.
- --actions: HTML put in the top right actions, before the theme switch (other switches, links).
- --lang-switch: adds the EN | FR pill (bilingual page, [data-lang] / [data-i18n] content).
- --set: placeholders (PROJECT, TITLE, STORAGE_KEY, FOOTER...); STORAGE_KEY defaults to the output file name,
  FOOTER to an empty string. A placeholder left unfilled is reported (exit 1).
"""

import argparse
import re
import sys
from pathlib import Path

SKILL = Path(__file__).resolve().parent.parent
LANG_SWITCH = """<div class="lang-switch" role="group" aria-label="Language" data-i18n-aria-label>
    <button type="button" data-set-lang="en" class="on">EN</button>
    <button type="button" data-set-lang="fr">FR</button>
  </div>
  <span class="divider" aria-hidden="true"></span>"""


def read(paths):
    return "\n".join(Path(p).read_text(encoding="utf-8") for p in paths)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("body")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--css", nargs="*", default=[])
    ap.add_argument("--js", nargs="*", default=[])
    ap.add_argument("--actions", default="")
    ap.add_argument("--lang-switch", action="store_true")
    ap.add_argument("--set", nargs="*", default=[], metavar="KEY=VALUE")
    ap.add_argument("--force", action="store_true", help="overwrite an existing output")
    a = ap.parse_args()

    out = Path(a.out)
    if out.exists() and not a.force:
        sys.exit(f"Error: {out} exists (--force to overwrite)")
    values = {"STORAGE_KEY": out.stem, "FOOTER": ""}
    for kv in a.set:
        if "=" not in kv:
            sys.exit(f"Error: --set expects KEY=VALUE, got '{kv}'")
        k, v = kv.split("=", 1)
        values[k] = v

    actions = (LANG_SWITCH if a.lang_switch else "") + (read([a.actions]) if a.actions else "")
    page = (SKILL / "templates" / "page.html").read_text(encoding="utf-8")
    page = page.replace("/*@STYLE@*/", read([SKILL / "templates" / "style.css", *a.css]).rstrip())
    page = page.replace("/*@SCRIPT@*/", read([SKILL / "templates" / "base.js", *a.js]).rstrip())
    page = page.replace("<!--@ACTIONS@-->", actions)
    page = page.replace("<!--@BODY@-->", Path(a.body).read_text(encoding="utf-8").strip())
    page = re.sub(r"\{\{([A-Z_]+)\}\}", lambda m: values.get(m.group(1), m.group(0)), page)

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(page, encoding="utf-8")
    left = sorted(set(re.findall(r"\{\{[A-Z_]+\}\}", page)))
    print(f"{out} ({len(page) // 1024} KB)")
    if left:
        print("unfilled placeholders: " + ", ".join(left) + " (--set KEY=VALUE or edit the page)")
        sys.exit(1)


if __name__ == "__main__":
    main()
