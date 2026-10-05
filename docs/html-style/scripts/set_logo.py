#!/usr/bin/env python3
"""
Set the logo of the brand (top bar) of every page of a folder: the real logo of the project when it has one,
otherwise the default documentation logo. Never a pictogram invented from the topic.

Usage:
    set_logo.py <docs dir> [--repo <project root>] [--file <logo.svg|png|jpg|ico>] [--default] [--dry-run]

- --file: this image.
- --repo (default: the parent of the docs dir): search the project logo (logo*.svg / png, icon*, *-logo.*,
  .github/*logo*, assets/ / images/ / resources/...), SVG first; third-party folders are skipped.
- --default: the default documentation logo (also used when no logo is found).
An SVG is inlined (scripts / event attributes / metadata removed, ids prefixed, class="logo", sized by the CSS);
a raster image becomes <img class="logo" src="data:..."> (warned above 100 KB).
"""

import argparse
import base64
import os
import re
import sys
from pathlib import Path

SKILL = Path(__file__).resolve().parent.parent
SKIP = {".git", "node_modules", "vendor", "third_party", "third-party", "external", "extern", "build", "dist",
        "target", ".cache", "venv", ".venv", "__pycache__", "deps", "_deps", "site-packages"}
NAMES = re.compile(r"^(logo|icon|brand|favicon)([-_.][\w-]*)?\.(svg|png|jpe?g|ico|webp)$|^[\w-]+[-_]logo\.(svg|png|jpe?g|webp)$", re.I)
MIME = {".svg": "image/svg+xml", ".png": "image/png", ".jpg": "image/jpeg", ".jpeg": "image/jpeg", ".ico": "image/x-icon",
        ".webp": "image/webp"}


def default_logo() -> str:
    page = (SKILL / "templates" / "page.html").read_text(encoding="utf-8")
    return re.search(r'<svg class="logo".*?</svg>', page, re.S).group(0)


def find_logo(repo: Path, docs: Path):
    found = []
    for root, dirs, files in os.walk(repo):
        dirs[:] = [d for d in dirs if d not in SKIP and not (Path(root) / d).resolve() == docs.resolve()]
        for f in files:
            if NAMES.match(f):
                p = Path(root) / f
                rank = (0 if p.suffix.lower() == ".svg" else 1,              # vector first
                        0 if "logo" in f.lower() else 1 if "brand" in f.lower() else 2,
                        len(p.relative_to(repo).parts),                     # closest to the root
                        len(f))
                found.append((rank, p))
    return [p for _, p in sorted(found)]


def inline_svg(path: Path) -> str:
    s = path.read_text(encoding="utf-8", errors="replace")
    s = re.sub(r"<\?xml.*?\?>|<!DOCTYPE.*?>|<!--.*?-->|<metadata.*?</metadata>|<script.*?</script>|<title>.*?</title>", "", s, flags=re.S | re.I)
    s = re.sub(r"\s+on\w+\s*=\s*(\"[^\"]*\"|'[^']*')", "", s)
    s = re.sub(r"\s+(xmlns:\w+|sodipodi:\w+|inkscape:\w+)\s*=\s*(\"[^\"]*\"|'[^']*')", "", s)
    s = re.sub(r"<(sodipodi|inkscape):[^>]*/>|<(sodipodi|inkscape):.*?</(sodipodi|inkscape):\w+>", "", s, flags=re.S)
    m = re.search(r"<svg\b[^>]*>", s)
    if not m:
        raise ValueError(f"{path}: no <svg> element")
    tag = m.group(0)
    if "viewBox" not in tag:
        w = re.search(r'\swidth="([\d.]+)', tag)
        h = re.search(r'\sheight="([\d.]+)', tag)
        if w and h:
            tag = tag.replace("<svg", f'<svg viewBox="0 0 {w.group(1)} {h.group(1)}"', 1)
    tag = re.sub(r'\s(width|height|class|id|style|aria-hidden)="[^"]*"', "", tag)
    tag = tag.replace("<svg", '<svg class="logo" aria-hidden="true"', 1)
    s = s[:m.start()] + tag + s[m.end():]
    s = s[:s.rfind("</svg>") + 6].strip()
    # ids prefixed: the logo lives in a page with other SVGs (gradients, clip paths...)
    for i in set(re.findall(r'\sid="([^"]+)"', s)):
        s = re.sub(r'(\sid=")' + re.escape(i) + '"', r'\1logo-' + i + '"', s)
        s = s.replace(f"url(#{i})", f"url(#logo-{i})").replace(f'href="#{i}"', f'href="#logo-{i}"')
    return re.sub(r">\s+<", "><", s)


def raster(path: Path) -> str:
    data = path.read_bytes()
    if len(data) > 100 * 1024:
        print(f"warning: {path} is {len(data) // 1024} KB, inlined in every page (prefer an SVG or a smaller PNG)")
    mime = MIME.get(path.suffix.lower(), "image/png")
    return f'<img class="logo" src="data:{mime};base64,{base64.b64encode(data).decode()}" alt="" aria-hidden="true">'


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("docs")
    ap.add_argument("--repo")
    ap.add_argument("--file")
    ap.add_argument("--default", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    docs = Path(a.docs)

    source = "default documentation logo"
    logo = default_logo()
    if a.file:
        candidates = [Path(a.file)]
    elif a.default:
        candidates = []
    else:
        repo = Path(a.repo) if a.repo else docs.resolve().parent
        candidates = find_logo(repo, docs)
        if len(candidates) > 1:
            print("candidates: " + ", ".join(str(c) for c in candidates[:5]))
    if candidates:
        p = candidates[0]
        logo = inline_svg(p) if p.suffix.lower() == ".svg" else raster(p)
        source = str(p)

    pages = 0
    for f in sorted(docs.glob("*.html")):
        t = f.read_text(encoding="utf-8")
        m = re.search(r'(<a class="brand"[^>]*>)(.*?)(</a>)', t, re.S)
        if not m:
            continue
        inner = re.sub(r'<svg class="logo".*?</svg>|<img class="logo"[^>]*>', "", m.group(2), count=1, flags=re.S)
        t = t[:m.start()] + m.group(1) + logo + inner + m.group(3) + t[m.end():]
        if not a.dry_run:
            f.write_text(t, encoding="utf-8")
        pages += 1
    print(f"logo: {source} -> {pages} page(s){' (dry run)' if a.dry_run else ''}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
