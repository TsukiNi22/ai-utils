#!/usr/bin/env python3
"""
Rebuild the navigation of every documentation page of a folder from the pages that really exist.

Usage:
    sync_nav.py [<docs dir>]

Each page declares its kind with <html data-page="guide|technical|graph">. For every page:
- the top tabs (<nav class="tabs">): Guide, Technical, Graph, the current one marked, hidden for a single page;
- the previous / next links (<nav class="pager">);
- the brand link -> first page.
Pages are independent: removing one and re-running this script removes it everywhere.
"""

import glob
import os
import re
import sys

ORDER = [("guide", "Guide", ""), ("technical", "Technical", ""), ("graph", "Graph", "")]


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
    present = [(k, label, group, pages[k]) for k, label, group in ORDER if k in pages]
    for i, (kind, _, _, name) in enumerate(present):
        path = os.path.join(d, name)
        t = open(path, encoding="utf-8").read()
        # Top tabs
        links = "\n".join(f'    <a href="{fn}"' + (' class="current" aria-current="page"' if k == kind else "") + f" data-i18n>{label}</a>"
                          for k, label, _, fn in present)
        hidden = " hidden" if len(present) < 2 else ""
        t = re.sub(r'<nav class="tabs"[^>]*>.*?</nav>', f'<nav class="tabs" aria-label="Pages"{hidden}>\n{links}\n  </nav>', t, count=1, flags=re.S)
        # Previous / next
        prev_, next_ = (present[i - 1] if i > 0 else None), (present[i + 1] if i + 1 < len(present) else None)
        pager = ""
        if prev_:
            pager += f'\n  <a class="prev" href="{prev_[3]}"><span data-i18n>Previous</span><b data-i18n>{prev_[1]}</b></a>'
        if next_:
            pager += f'\n  <a class="next" href="{next_[3]}"><span data-i18n>Next</span><b data-i18n>{next_[1]}</b></a>'
        t = re.sub(r'<nav class="pager"[^>]*>.*?</nav>', f'<nav class="pager" aria-label="Pages">{pager + chr(10) if pager else ""}</nav>', t, count=1, flags=re.S)
        t = re.sub(r'(<a class="brand" href=")[^"]*(")', rf"\g<1>{present[0][3]}\2", t, count=1)
        open(path, "w", encoding="utf-8").write(t)
    print(", ".join(f"{label} -> {fn}" for _, label, _, fn in present))
    return 0


if __name__ == "__main__":
    sys.exit(main())
