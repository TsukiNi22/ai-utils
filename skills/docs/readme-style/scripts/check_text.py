#!/usr/bin/env python3
"""Flag the text of a Markdown file that is too long to read at a glance.

    python3 check_text.py README.md [readme/*.md ...] [--sub]

Checks the prose only (code blocks, HTML, comments, front matter, headings and link targets are skipped), with limits
by place: tight in a README (tagline, cells, items), looser in a sub-doc (a file under readme/ or docs/, or --sub).
- LONG    a sentence too long for its place (intro 15 words, item 15, callout 20, paragraph 20; sub-doc 25)
- PARA    a paragraph over 3 sentences / 50 words (intro: 2 sentences; sub-doc: 4 / 80): split, list or table it
- NOTE    a callout over 2 lines of text (its tables not counted): one idea per callout
- CELL    a table cell over 8 words (sub-doc: 15): the detail goes under the table or in a sub-doc
- ITEM    a list item over 20 words (sub-doc: 30)
- FILLER  a word or phrase that adds nothing ("in order to", "allows you to", "note that", "simply"...)
- SIZE    a README over 600 words of prose: move the detail to readme/<subject>.md
Exit 1 when something is flagged. One line per finding: file:line CODE text.
"""
import argparse
import re
import sys
from collections.abc import Iterator
from pathlib import Path

ROOT = {"intro": 15, "item": 15, "note": 20, "para": 20, "intro_sentences": 2, "para_sentences": 3, "para_words": 50,
        "note_lines": 2, "cell": 8, "item_words": 20, "size": 600}
SUB = {"intro": 25, "item": 25, "note": 25, "para": 25, "intro_sentences": 4, "para_sentences": 4, "para_words": 80,
       "note_lines": 3, "cell": 15, "item_words": 30, "size": 0}
FILLERS = (
    r"in order to", r"allows? (?:you |us |the user )?to", r"(?:makes?|made) it possible to", r"it is (?:important|possible|recommended) to",
    r"(?:please )?note that", r"keep in mind", r"make sure (?:to|that)", r"be aware", r"as (?:mentioned|said|described) (?:above|before|earlier)",
    r"the following", r"this (?:section|page|document|file) (?:describes|explains|contains|lists|shows)", r"basically", r"simply",
    r"just", r"very", r"really", r"actually", r"in fact", r"of course", r"obviously", r"easily", r"feel free", r"a (?:lot|number) of",
    r"(?:is|are) able to", r"in a way that", r"at the moment", r"currently", r"various", r"several different",
)
FILLER = re.compile(r"\b(?:" + "|".join(FILLERS) + r")\b", re.IGNORECASE)
SENTENCE_END = re.compile(r"(?<=[.!?])\s+(?=[A-Z`*(\[])")

def plain(text: str) -> str:
    """The text a reader reads: links, images, badges and emphasis reduced to their words."""
    text = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", text)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"<[^>]+>", "", text)
    return re.sub(r"\*\*|__", "", text).strip()

def words(text: str) -> int:
    """Words read; an inline code span counts as one."""
    return len(re.findall(r"[\w'’-]+", re.sub(r"`[^`]*`", "x", text)))

def sentences(text: str) -> list[str]:
    return [part for part in SENTENCE_END.split(text) if words(part)]

def blocks(lines: list[str]) -> Iterator[tuple[str, int, list[str], bool, bool]]:
    """(kind, first line, lines, generated, intro) per prose block: para, item, note, table; code and HTML skipped."""
    fence, comment, generated, intro, current, kind, start = None, False, False, True, [], "", 0
    for number, line in enumerate(lines, 1):
        stripped = line.strip()
        if fence:
            fence = None if stripped.startswith(fence) else fence
            continue
        if comment:
            comment = "-->" not in stripped
            continue
        match = re.match(r"(`{3,}|~{3,})", stripped)
        html = stripped.startswith("<") and not stripped.startswith("<!--")
        if match or html or stripped.startswith("<!--") or not stripped or stripped.startswith("#"):
            if current:
                yield kind, start, current, generated, intro
            current, kind = [], ""
            if match:
                fence = match.group(1)
            elif stripped.startswith("<!--"):
                comment = "-->" not in stripped
                generated = stripped.startswith("<!-- generated") or generated and not stripped.startswith("<!-- /generated")
            elif re.match(r"#{2,}\s", stripped):
                intro = False
            continue
        if stripped.startswith(">"):
            line_kind = "note"
        elif stripped.startswith("|"):
            line_kind = "table"
        elif re.match(r"([-*+]|\d+[.)])\s", stripped):
            line_kind = "item"
        else:
            line_kind = "more" if kind == "item" and line.startswith((" ", "\t")) else "para"
        if line_kind == "more":
            current.append(stripped)
            continue
        if current and (line_kind != kind or line_kind == "item"):
            yield kind, start, current, generated, intro
            current = []
        if not current:
            kind, start = line_kind, number
        current.append(stripped)
    if current:
        yield kind, start, current, generated, intro

def check_prose(where: str, text: str, limit: int, findings: list[str]) -> None:
    for sentence in sentences(text):
        if words(sentence) > limit:
            findings.append(f"{where} LONG {words(sentence)} words (max {limit}): {sentence[:70]}...")
    for match in FILLER.finditer(text):
        findings.append(f"{where} FILLER \"{match.group(0)}\"")

def check(path: Path, sub: bool) -> list[str]:
    limits = SUB if sub or {"readme", "docs"} & set(path.parent.parts) else ROOT
    findings, prose = [], 0
    text = path.read_text(encoding="utf-8").splitlines()
    if text and text[0].strip() in ("---", "+++"):  # front matter (YAML / TOML): not read as prose
        end = next((n for n in range(1, len(text)) if text[n].strip() == text[0].strip()), 0)
        text[:end + 1] = [""] * (end + 1)
    for kind, number, lines, generated, intro in blocks(text):
        where = f"{path}:{number}" + (" (generated: shorten its source)" if generated else "")
        if kind == "table":
            for row in lines:
                if re.fullmatch(r"\|[\s:|-]+\|?", row):
                    continue
                for cell in row.strip("|").split("|"):
                    if words(plain(cell)) > limits["cell"]:
                        findings.append(f"{where} CELL {words(plain(cell))} words (max {limits['cell']}): {plain(cell)[:60]}...")
            continue
        if kind == "note":
            body = [plain(re.sub(r"^>\s?", "", row)) for row in lines]
            body = [row for row in body if row and not row.startswith("|") and not re.fullmatch(r"\[!\w+\]", row)]
            if len(body) > limits["note_lines"]:
                findings.append(f"{where} NOTE {len(body)} lines of text: keep one idea")
            prose += words(" ".join(body))
            check_prose(where, " ".join(body), limits["note"], findings)
            continue
        body = plain(" ".join(lines))
        prose += words(body)
        if kind == "item":
            if words(body) > limits["item_words"]:
                findings.append(f"{where} ITEM {words(body)} words (max {limits['item_words']}): {body[:60]}...")
            check_prose(where, body, limits["item"], findings)
            continue
        most = limits["intro_sentences"] if intro else limits["para_sentences"]
        if len(sentences(body)) > most or words(body) > limits["para_words"]:
            findings.append(f"{where} PARA {len(sentences(body))} sentences, {words(body)} words (max {most} / {limits['para_words']})")
        check_prose(where, body, limits["intro"] if intro else limits["para"], findings)
    if limits["size"] and path.name.lower() == "readme.md" and prose > limits["size"]:
        findings.append(f"{path}:1 SIZE {prose} words of prose (max {limits['size']}): move the detail to readme/<subject>.md")
    return findings

def main() -> int:
    parser = argparse.ArgumentParser(description="Flag Markdown text too long to read at a glance.")
    parser.add_argument("files", nargs="+", type=Path)
    parser.add_argument("--sub", action="store_true", help="sub-doc limits for every file (default: by its folder)")
    args = parser.parse_args()
    findings = []
    for path in args.files:
        findings += check(path, args.sub)
    print("\n".join(findings) if findings else "ok: short enough")
    return 1 if findings else 0

if __name__ == "__main__":
    sys.exit(main())
