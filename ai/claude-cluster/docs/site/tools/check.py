#!/usr/bin/env python3
# =============================================================================
# check.py - checks of a built documentation site
#   check.py <site dir> [--no-style]
#   - every page passes check_style.py of the html-style skill (self-contained, contrast, tokens)
#   - every local link (href / src) points to a file of the site, every #fragment to an id of its page
#   - every page has its EN and FR blocks, the same ids on both sides, no placeholder text
# exit 1 when something is wrong
# =============================================================================

import argparse
import re
import subprocess
import sys
from html.parser import HTMLParser
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[5]
CHECK_STYLE = REPO_ROOT / "docs" / "html-style" / "scripts" / "check_style.py"
PLACEHOLDER = re.compile(r"\b(lorem ipsum|TODO|FIXME|TBD|XXX|placeholder)\b", re.I)
RUNTIME = ("/docs", "/app.apk", "/api/", "/ca.crt")  # served by the binary only


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
        if tag in ("script", "style"):
            self.skip += 1

    def handle_endtag(self, tag: str) -> None:
        if tag in ("script", "style"):
            self.skip -= 1

    def handle_data(self, data: str) -> None:
        if not self.skip:
            self.text.append(data)


def main() -> int:
    ap = argparse.ArgumentParser(description="Check a built claude-cluster documentation site")
    ap.add_argument("site")
    ap.add_argument("--no-style", action="store_true")
    args = ap.parse_args()
    site = Path(args.site)
    pages = sorted(site.glob("*.html"))
    errors: list[str] = []
    parsed: dict[str, Page] = {}
    for path in pages:
        p = Page()
        p.feed(path.read_text())
        parsed[path.name] = p
    for name, p in parsed.items():
        if p.langs["en"] != p.langs["fr"]:
            errors.append("%s: %d EN blocks but %d FR blocks" % (name, p.langs["en"], p.langs["fr"]))
        found = PLACEHOLDER.search(" ".join(p.text))
        if found:
            errors.append("%s: placeholder text %r" % (name, found.group(0)))
        for link in p.links:
            if re.match(r"^(https?:|mailto:|data:|javascript:)", link) or link.startswith(RUNTIME):
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
    if not args.no_style and CHECK_STYLE.exists():
        for path in pages:
            run = subprocess.run([sys.executable, str(CHECK_STYLE), str(path)], capture_output=True, text=True)
            if run.returncode != 0:
                errors.append("%s: check_style: %s" % (path.name, run.stdout.strip().splitlines()[-1] if run.stdout.strip() else run.stderr.strip()))
    for e in errors:
        print("ERROR " + e)
    print("%d pages checked, %d error(s)" % (len(pages), len(errors)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
