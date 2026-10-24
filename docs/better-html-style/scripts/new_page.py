#!/usr/bin/env python3
"""
Build one self-contained page in the better-html-style: the html-style page (style.css + base.js) with the site layer of this
skill (templates/site.css + site.js) and the search box in the top bar - with no site index, it searches the sections of the
page itself (typo tolerant, / or Ctrl+K).

Usage:
    new_page.py <body.html> -o <out.html> [--css extra.css ...] [--js extra.js ...] [--actions actions.html] [--no-search]
                [--lang-switch] [--attr NAME=VALUE ...] [--set KEY=VALUE ...] [--force]

Every option but --no-search goes to html-style/scripts/new_page.py (a sibling skill: ../html-style).
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

SKILL = Path(__file__).resolve().parent.parent
HTML_STYLE = SKILL.parent / "html-style" / "scripts" / "new_page.py"

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("body")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--css", nargs="*", default=[])
    ap.add_argument("--js", nargs="*", default=[])
    ap.add_argument("--actions", default="")
    ap.add_argument("--no-search", action="store_true", help="no search box in the top bar")
    ap.add_argument("--lang-switch", action="store_true")
    ap.add_argument("--attr", nargs="*", default=[], metavar="NAME=VALUE")
    ap.add_argument("--set", nargs="*", default=[], metavar="KEY=VALUE")
    ap.add_argument("--force", action="store_true")
    a = ap.parse_args()
    if not HTML_STYLE.exists():
        sys.exit("Error: the html-style skill is missing (%s): install it next to this one" % HTML_STYLE)
    actions = "" if a.no_search else (SKILL / "templates" / "search.html").read_text(encoding="utf-8")
    if a.actions:
        actions += Path(a.actions).read_text(encoding="utf-8")
    with tempfile.NamedTemporaryFile("w", suffix=".html", delete=False, encoding="utf-8") as tmp:
        tmp.write(actions)
    cmd = [sys.executable, str(HTML_STYLE), a.body, "-o", a.out, "--css", str(SKILL / "templates" / "site.css"), *a.css,
           "--js", str(SKILL / "templates" / "site.js"), *a.js, "--actions", tmp.name]
    if a.lang_switch:
        cmd.append("--lang-switch")
    if a.force:
        cmd.append("--force")
    if a.attr:
        cmd += ["--attr", *a.attr]
    if a.set:
        cmd += ["--set", *a.set]
    try:
        return subprocess.run(cmd).returncode
    finally:
        Path(tmp.name).unlink()

if __name__ == "__main__":
    sys.exit(main())
