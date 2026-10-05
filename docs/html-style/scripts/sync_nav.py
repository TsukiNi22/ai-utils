#!/usr/bin/env python3
"""
Rebuild the navigation of every page of a folder (html-doc and explain-doc pages together) from the pages that
really exist, so that each skill links the pages of the other one.

Usage:
    sync_nav.py [<docs dir>]

Each page declares its kind with <html data-page="guide|technical|graph|explain">; an explain page names its tab
with data-nav-title="AES" (default: the end of its <title>) and can set data-nav-order="2". A page with the level
switch of explain-doc but no data-page (made before this script) is detected and gets the attributes.
For every page:
- the top tabs (<nav class="tabs">): Guide, Technical, Graph, then the explanations; the current one marked,
  hidden for a single page;
- the previous / next links (<nav class="pager">, when the page has one);
- the brand link -> first page;
- the storage key of the explanation pages = the one of the html-doc pages (choices saved once for the site);
  links between the pages also carry ?lang= / ?theme= / ?level= (base.js), for file:// where pages share nothing.
Pages are independent: removing one and re-running this script removes it everywhere.
"""

import glob
import html
import os
import re
import sys

ORDER = [("guide", "Guide"), ("technical", "Technical"), ("graph", "Graph")]


def nav_title(text: str, path: str) -> str:
    m = re.search(r'<html[^>]*\bdata-nav-title="([^"]*)"', text)
    if m:
        return html.unescape(m.group(1))
    m = re.search(r"<title>(.*?)</title>", text, re.S)
    if m:
        return html.unescape(re.split(r"\s[·|:-]\s", m.group(1).strip())[-1])
    return os.path.splitext(os.path.basename(path))[0].replace("-", " ").title()


def main() -> int:
    d = sys.argv[1] if len(sys.argv) > 1 else "docs"
    fixed, explain = {}, []
    for f in sorted(glob.glob(os.path.join(d, "*.html"))):
        text = open(f, encoding="utf-8").read()
        head = text[:3000]
        m = re.search(r'<html[^>]*\bdata-page="(\w+)"', head)
        kind = m.group(1) if m else ("explain" if 'class="level-switch"' in text else None)
        if kind is None:
            continue
        if kind == "explain":
            title = nav_title(text, f)
            if not m:  # page made before the attributes existed: declare it
                text = re.sub(r"<html\b", f'<html data-page="explain" data-nav-title="{html.escape(title)}"', text, count=1)
                open(f, "w", encoding="utf-8").write(text)
            order = re.search(r'<html[^>]*\bdata-nav-order="(\d+)"', head)
            explain.append((int(order.group(1)) if order else 999, os.path.basename(f), title))
        elif kind not in fixed:
            fixed[kind] = os.path.basename(f)
    # One storage key for the whole site (theme / language / level saved once for every page): the key of the
    # html-doc pages, else the one of the first page
    keys = {}
    for fn in list(fixed.values()) + [e[1] for e in explain]:
        k = re.search(r'<html[^>]*\bdata-storage-key="([^"]*)"', open(os.path.join(d, fn), encoding="utf-8").read(3000))
        if k and '{{' not in k.group(1):
            keys.setdefault(fn, k.group(1))
    site_key = next((keys[fn] for fn in fixed.values() if fn in keys), next(iter(keys.values()), None))
    present = [(k, label, fixed[k]) for k, label in ORDER if k in fixed]
    present += [("explain", title, fn) for _, fn, title in sorted(explain)]
    if not present:
        print(f"no page with data-page / level switch in {d}")
        return 0

    for i, (kind, _, name) in enumerate(present):
        path = os.path.join(d, name)
        t = open(path, encoding="utf-8").read()
        # Top tabs
        links = "\n".join(f'    <a href="{fn}"' + (' class="current" aria-current="page"' if fn == name else "")
                          + f" data-i18n>{html.escape(label)}</a>" for _, label, fn in present)
        hidden = " hidden" if len(present) < 2 else ""
        t = re.sub(r'<nav class="tabs"[^>]*>.*?</nav>', f'<nav class="tabs" aria-label="Pages"{hidden}>\n{links}\n  </nav>',
                   t, count=1, flags=re.S)
        # Previous / next
        prev_ = present[i - 1] if i > 0 else None
        next_ = present[i + 1] if i + 1 < len(present) else None
        pager = ""
        if prev_:
            pager += f'\n  <a class="prev" href="{prev_[2]}"><span data-i18n>Previous</span><b data-i18n>{html.escape(prev_[1])}</b></a>'
        if next_:
            pager += f'\n  <a class="next" href="{next_[2]}"><span data-i18n>Next</span><b data-i18n>{html.escape(next_[1])}</b></a>'
        t = re.sub(r'<nav class="pager"[^>]*>.*?</nav>', f'<nav class="pager" aria-label="Pages">{pager + chr(10) if pager else ""}</nav>',
                   t, count=1, flags=re.S)
        t = re.sub(r'(<a class="brand" href=")[^"]*(")', rf"\g<1>{present[0][2]}\2", t, count=1)
        if site_key and kind == "explain":  # html-doc pages keep theirs (also used by the graph config)
            t = re.sub(r'(<html[^>]*\bdata-storage-key=")[^"]*(")', rf"\g<1>{site_key}\2", t, count=1)
        open(path, "w", encoding="utf-8").write(t)
    print(", ".join(f"{label} -> {fn}" for _, label, fn in present))
    return 0


if __name__ == "__main__":
    sys.exit(main())
