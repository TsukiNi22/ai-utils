#!/usr/bin/env python3
"""
Check a GitHub wiki folder (the clone of <repo>.wiki.git) against the rules of the user's wiki style.

Usage:
    check_wiki.py <wiki-folder> [--strict]

Checks:
- Home.md, _Sidebar.md and _Footer.md exist; Home starts with the version stamp (`> [!WARNING]` up to date as of ...);
- every page is linked from _Sidebar.md;
- links to a page (`[x](Page)`, `[x](Page#anchor)`, `[x](#anchor)`): the page exists (no `.md`), the anchor is a heading
  of the page (GitHub slug: lower case, punctuation removed, spaces -> `-`);
- a page does not start with a `# Title` (GitHub shows the file name);
- callouts are `> [!NOTE|TIP|IMPORTANT|WARNING|CAUTION]`;
- fenced code blocks have a language and are closed;
- tables: the same number of columns on every row, the header separator right below the header;
- a Table of Contents lists every `##` of the page (warning);
- no emoji.
Errors fail (exit 1); the warnings fail only with --strict.
"""

import argparse
import re
import sys
import unicodedata
from pathlib import Path
from urllib.parse import unquote

CALLOUTS = {"NOTE", "TIP", "IMPORTANT", "WARNING", "CAUTION"}
EMOJI = re.compile("[\U0001F300-\U0001FAFF☀-➿⭐⭕]")
HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
LINK = re.compile(r"(?<!!)\[[^\]]*\]\(([^()\s]*(?:\([^()\s]*\)[^()\s]*)*)\)")
CODE_SPAN = re.compile(r"`[^`]*`")


def slug(heading: str) -> str:
    """Anchor GitHub gives to a heading."""
    text = re.sub(r"[`*_]", "", heading.strip().lower())
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = "".join(c for c in text if c in " -" or unicodedata.category(c)[0] in "LN")
    return text.replace(" ", "-")


def split_row(line: str) -> list[str]:
    line = re.sub(r"`[^`]*`", lambda m: m.group(0).replace("|", "\0"), line.strip())
    line = re.sub(r"\\\|", "\0", line)
    return line.strip("|").split("|")


class Page:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.name = path.stem
        self.lines = path.read_text(encoding="utf-8").splitlines()
        self.headings: list[tuple[int, int, str]] = []  # (line, level, text)
        self.anchors: set[str] = set()
        self.code: set[int] = set()  # lines inside a fenced block (fences included)
        self.problems: list[tuple[str, int, str]] = []  # (kind, line, message)
        self.scan()

    def problem(self, kind: str, line: int, message: str) -> None:
        self.problems.append((kind, line, message))

    def scan(self) -> None:
        fence = None
        seen: dict[str, int] = {}
        for i, line in enumerate(self.lines, 1):
            m = re.match(r"^\s*(`{3,}|~{3,})\s*(\S*)", line)
            if m and fence is None:
                fence = (m.group(1)[0], len(m.group(1)), i)
                self.code.add(i)
                if not m.group(2):
                    self.problem("error", i, "code block without a language")
                continue
            if fence is not None:
                self.code.add(i)
                if re.match(r"^\s*" + re.escape(fence[0]) + "{" + str(fence[1]) + r",}\s*$", line):
                    fence = None
                continue
            h = HEADING.match(line)
            if h:
                text = h.group(2)
                self.headings.append((i, len(h.group(1)), text))
                base = slug(text)
                n = seen.get(base, 0)
                seen[base] = n + 1
                self.anchors.add(base if n == 0 else f"{base}-{n}")
        if fence is not None:
            self.problem("error", fence[2], "code block never closed")

    def text_lines(self):
        for i, line in enumerate(self.lines, 1):
            if i not in self.code:
                yield i, line

    def check_style(self) -> None:
        first = next((l for l in self.lines if l.strip()), "")
        if re.match(r"^# ", first) and not self.name.startswith("_"):
            self.problem("warning", 1, "starts with a `# Title` (GitHub shows the file name as the title)")
        for i, line in self.text_lines():
            m = re.match(r"^>\s*\[!(\w+)\]", line)
            if m and m.group(1) not in CALLOUTS:
                self.problem("error", i, f"unknown callout [!{m.group(1)}]")
            if EMOJI.search(line):
                self.problem("error", i, "emoji")
        self.check_tables()
        self.check_toc()

    def check_tables(self) -> None:
        rows: list[tuple[int, str]] = []
        for i, line in list(self.text_lines()) + [(len(self.lines) + 1, "")]:
            if line.lstrip().startswith("|"):
                rows.append((i, line))
                continue
            if rows:
                self.check_table(rows)
                rows = []

    def check_table(self, rows: list[tuple[int, str]]) -> None:
        head = rows[0][0]
        if len(rows) < 2 or not re.match(r"^\s*\|?\s*:?-+", rows[1][1]):
            self.problem("error", head, "table without a header separator right below the header")
            return
        count = len(split_row(rows[0][1]))
        for i, line in rows[1:]:
            if len(split_row(line)) != count:
                self.problem("error", i, f"table row with {len(split_row(line))} columns (header: {count})")

    def check_toc(self) -> None:
        toc = next((i for i, lv, t in self.headings if lv == 2 and t.lower() == "table of contents"), None)
        if toc is None:
            return
        linked = set()
        for i, line in self.text_lines():
            if i > toc and line.startswith(" - ") or (i > toc and re.match(r"^\s+- ", line)):
                linked.update(unquote(a) for a in re.findall(r"\(#([^)]*)\)", line))
            elif i > toc and linked and line.strip() and not line.lstrip().startswith("-"):
                break
        for i, lv, t in self.headings:
            if lv == 2 and i != toc and slug(t) not in linked:
                self.problem("warning", i, f"`## {t}` is not in the Table of Contents")


def main() -> int:
    parser = argparse.ArgumentParser(description="Check a GitHub wiki folder against the user's wiki style")
    parser.add_argument("folder", type=Path)
    parser.add_argument("--strict", action="store_true", help="the warnings fail too")
    args = parser.parse_args()
    folder: Path = args.folder
    if not folder.is_dir():
        print(f"error: {folder} is not a folder", file=sys.stderr)
        return 2
    pages = {p.stem.lower(): Page(p) for p in sorted(folder.glob("*.md"))}
    if not pages:
        print(f"error: no .md page in {folder}", file=sys.stderr)
        return 2
    missing = [f"{need}.md" for need in ("Home", "_Sidebar", "_Footer") if need.lower() not in pages]
    for name in missing:
        print(f"error: {name} is missing")
    for page in pages.values():
        page.check_style()
        for i, line in page.text_lines():
            for target in LINK.findall(CODE_SPAN.sub("", line)):
                if re.match(r"^(https?:|mailto:|//)", target):
                    continue
                name, _, anchor = unquote(target).partition("#")
                if name.endswith(".md"):
                    page.problem("error", i, f"link to `{name}`: no `.md` in a wiki link")
                    name = name[:-3]
                dest = page if not name else pages.get(name.replace("%20", "-").lower())
                if dest is None:
                    page.problem("error", i, f"link to a page that does not exist: `{name}`")
                elif anchor and anchor.lower() not in {a.lower() for a in dest.anchors}:
                    page.problem("error", i, f"link to `{name or page.name}#{anchor}`: no such heading")
    home = pages.get("home")
    if home is not None:
        first = next((l for l in home.lines if l.strip()), "")
        stamp = "\n".join(home.lines[:3])
        if not (first.startswith("> [!WARNING]") and "up to date as of" in stamp):
            home.problem("warning", 1, "Home does not start with the version stamp (`> [!WARNING]` up to date as of ...)")
    side = pages.get("_sidebar")
    if side is not None:
        linked = {unquote(t).partition("#")[0].lower() for _, l in side.text_lines() for t in LINK.findall(CODE_SPAN.sub("", l))}
        for key, page in pages.items():
            if not key.startswith("_") and key != "home" and key not in linked:
                page.problem("warning", 0, "page not linked from _Sidebar.md")
    errors, warnings = len(missing), 0
    for page in pages.values():
        for kind, line, message in sorted(page.problems, key=lambda p: p[1]):
            print(f"{page.path.name}:{line}: {kind}: {message}")
            errors += kind == "error"
            warnings += kind == "warning"
    print(f"== {len(pages)} page(s): {errors} error(s), {warnings} warning(s)")
    return 1 if errors or (args.strict and warnings) else 0


if __name__ == "__main__":
    sys.exit(main())
