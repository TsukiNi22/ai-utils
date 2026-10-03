#!/usr/bin/env python3
"""
Rewrite the page tabs of every documentation page of a folder from the pages that really exist.

Usage:
    sync_nav.py [<docs dir>]

Each page declares its kind with <html data-page="guide|technical|graph">. The tabs (<nav class="tabs">) of every page
are rebuilt in the order Guide, Technique, Graphe with the real file names, the current page marked; with a single
page the tabs are hidden. The brand link points to the first page. Pages are independent: removing one and
re-running this script removes its tab everywhere.
"""

import glob
import os
import re
import sys

ORDER = [("guide", "Guide"), ("technical", "Technique"), ("graph", "Graphe")]


def main() -> int:
    d = sys.argv[1] if len(sys.argv) > 1 else "docs"
    pages = {}
    for f in sorted(glob.glob(os.path.join(d, "*.html"))):
        m = re.search(r'<html[^>]*\bdata-page="(\w+)"', open(f, encoding="utf-8").read(2000))
        if m and m.group(1) not in pages:
            pages[m.group(1)] = os.path.basename(f)
    if not pages:
        print(f"no documentation page (data-page) in {d}")
        return 0
    present = [(k, label, pages[k]) for k, label in ORDER if k in pages]
    for kind, _, name in present:
        path = os.path.join(d, name)
        t = open(path, encoding="utf-8").read()
        links = "\n".join(
            f'    <a href="{fn}"' + (' class="current" aria-current="page"' if k == kind else "") + f">{label}</a>"
            for k, label, fn in present)
        hidden = " hidden" if len(present) < 2 else ""
        t = re.sub(r'<nav class="tabs"[^>]*>.*?</nav>',
                   f'<nav class="tabs" aria-label="Pages"{hidden}>\n{links}\n  </nav>', t, count=1, flags=re.S)
        t = re.sub(r'(<a class="brand" href=")[^"]*(")', rf"\g<1>{present[0][2]}\2", t, count=1)
        open(path, "w", encoding="utf-8").write(t)
    print(", ".join(f"{label} -> {fn}" for _, label, fn in present))
    return 0


if __name__ == "__main__":
    sys.exit(main())
