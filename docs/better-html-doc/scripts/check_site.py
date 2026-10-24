#!/usr/bin/env python3
# =============================================================================
# check_site.py - checks of a built better-html-doc site
#   check_site.py <site dir> [--no-style] [--allow-placeholder PAGE ...]
#   - every page passes check_style.py of html-style (self-contained, contrast, tokens, responsive rules)
#   - every local link (href / src) points to a file of the site, every #fragment to an id of its page
#   - a bilingual site: every page has as many EN as FR blocks; no placeholder text ({{X}}, a to-do mark, lorem ipsum...)
#   - docs-map.json (contextual help): every screen points to a page / id that exists
#   - the search index lists every page
# exit 1 when something is wrong
# =============================================================================

import argparse
import json
import re
import subprocess
import sys
import tempfile
from html.parser import HTMLParser
from pathlib import Path

CHECK_STYLE = Path(__file__).resolve().parents[2] / "html-style" / "scripts" / "check_style.py"
PLACEHOLDER = re.compile(r"\{\{[A-Z_]+\}\}|\b(?:(?i:lorem ipsum)|TODO|FIXME|TBD|XXX)\b")

class Page(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.ids: set[str] = set()
        self.links: list[str] = []
        self.langs: dict[str, int] = {"en": 0, "fr": 0}
        self.text: list[str] = []
        self.skip = 0

    def handle_starttag(self, tag: str, attrs: list) -> None:
        a = dict(attrs)
        if "id" in a:
            self.ids.add(a["id"])
        for key in ("href", "src"):
            if a.get(key):
                self.links.append(a[key])
        if a.get("data-lang") in self.langs:
            self.langs[a["data-lang"]] += 1
        if tag in ("script", "style", "pre", "code"):
            self.skip += 1

    def handle_endtag(self, tag: str) -> None:
        if tag in ("script", "style", "pre", "code"):
            self.skip -= 1

    def handle_data(self, data: str) -> None:
        if not self.skip:
            self.text.append(data)

def main() -> int:
    ap = argparse.ArgumentParser(description="Check a built better-html-doc site")
    ap.add_argument("site")
    ap.add_argument("--no-style", action="store_true")
    ap.add_argument("--allow-placeholder", nargs="*", default=["changelog.html"], help="pages whose text may hold TODO / TBD (a changelog)")
    args = ap.parse_args()
    site = Path(args.site)
    pages = sorted(site.glob("*.html"))
    if not pages:
        print("ERROR no page in %s" % site)
        return 1
    errors: list[str] = []
    parsed: dict[str, Page] = {}
    for path in pages:
        p = Page()
        p.feed(path.read_text(encoding="utf-8"))
        parsed[path.name] = p
    bilingual = any(p.langs["fr"] for p in parsed.values())
    for name, p in parsed.items():
        if bilingual and p.langs["en"] != p.langs["fr"]:
            errors.append("%s: %d EN blocks but %d FR blocks" % (name, p.langs["en"], p.langs["fr"]))
        found = PLACEHOLDER.search(" ".join(p.text)) if name not in args.allow_placeholder else None
        if found:
            errors.append("%s: placeholder text %r" % (name, found.group(0)))
        for link in p.links:
            if re.match(r"^(https?:|mailto:|data:|javascript:)", link) or link.startswith("/"):
                continue
            target, _, frag = link.partition("#")
            target = target.split("?")[0]
            if not target:
                if frag and frag not in p.ids and frag != "top":
                    errors.append("%s: #%s not found" % (name, frag))
                continue
            if not (site / target).exists():
                errors.append("%s: link %s: no such file" % (name, link))
            elif frag and target in parsed and frag not in parsed[target].ids:
                errors.append("%s: link %s: no id %s" % (name, link, frag))
    mapping = site / "docs-map.json"
    if mapping.exists():
        for screen, target in json.loads(mapping.read_text(encoding="utf-8")).items():
            page, _, frag = target.partition("#")
            if page + ".html" not in parsed or (frag and frag not in parsed[page + ".html"].ids):
                errors.append("docs-map.json: %s -> %s does not exist" % (screen, target))
    index = site / "search-index.json"
    if not index.exists():
        errors.append("search-index.json missing")
    else:
        listed = {e["u"].split("#")[0] for e in json.loads(index.read_text(encoding="utf-8"))["entries"]}
        for name in parsed:
            if name != "index.html" and name not in listed:
                errors.append("%s: not in the search index" % name)
    if not args.no_style:
        if not CHECK_STYLE.exists():
            errors.append("html-style/scripts/check_style.py not found next to this skill (--no-style to skip)")
        else:
            shared = site / "site.css"
            with tempfile.TemporaryDirectory() as folder:
                for path in pages:
                    page = path.read_text(encoding="utf-8")
                    if shared.exists():  # a --split site: its style inlined in a copy, check_style reads the page alone
                        page = page.replace('<link rel="stylesheet" href="site.css">', "<style>" + shared.read_text(encoding="utf-8") + "</style>", 1)
                    copy = Path(folder) / path.name
                    copy.write_text(page, encoding="utf-8")
                    run = subprocess.run([sys.executable, str(CHECK_STYLE), str(copy)], capture_output=True, text=True)
                    if run.returncode != 0:
                        lines = [x for x in run.stdout.splitlines() if x.strip()]
                        errors.append("%s: check_style: %s" % (path.name, "; ".join(lines[-6:]) if lines else run.stderr.strip()))
    for e in sorted(set(errors), key=errors.index):
        print("ERROR " + e)
    print("%d pages checked, %d error(s)" % (len(pages), len(set(errors))))
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
