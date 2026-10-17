#!/usr/bin/env python3
"""
Check a protocol documentation (Markdown and / or the HTML sections of the technical page).

Usage:
    check_protocol.py <PROTOCOL.md|README.md|docs/technical.html>... [--allow-placeholders]

Markdown: every link (#anchor) reaches a heading, every type of the types table (| Byte (hexa) | Type |) has its
section, every frame of the summary block (/* Type */ ENQ ... EOT) uses the byte of its type, the codes of each code
table (| Code ... |) are unique, no {{placeholder}} left, no typo of the reference doc.
HTML: every href="#id" reaches an id, no {{placeholder}} left, no typo.
Exit code: 0 = clean, 1 = issues found, 2 = usage error.
"""

import argparse
import re
import sys

TYPOS = {"steap": "step", "awnser": "answer", "dosen't": "doesn't", "reciever": "receiver", "it's type": "its type",
         "informations": "information", "transfert": "transfer", "reconniction": "reconnection"}

def anchor(title: str) -> str:
    """The GitHub anchor of a heading: lower case, punctuation removed, spaces as '-'."""
    text = re.sub(r"`|\*\*|\[|\]\([^)]*\)", "", title.strip().lower())
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")

def outside_code(lines: list) -> list:
    """(number, line, in_code) for every line, code fences tracked."""
    out, fence = [], False
    for number, line in enumerate(lines, 1):
        if line.lstrip().startswith(("```", "~~~")):
            fence = not fence
            out.append((number, line, True))
            continue
        out.append((number, line, fence))
    return out

def tables(lines: list) -> list:
    """The Markdown tables: (first line number, header cells, rows of cells)."""
    found, current = [], None
    for number, line, code in outside_code(lines):
        cells = [c.strip() for c in line.strip().strip("|").split("|")] if not code and line.strip().startswith("|") else None
        if cells is None:
            if current:
                found.append(current)
            current = None
            continue
        if current is None:
            current = (number, cells, [])
        elif not all(re.fullmatch(r":?-{2,}:?", c) for c in cells):
            current[2].append((number, cells))
    if current:
        found.append(current)
    return found

def check_markdown(path: str, text: str, issues: list, placeholders: bool) -> None:
    lines = text.split("\n")
    rows = outside_code(lines)
    headings = {}
    for number, line, code in rows:
        match = None if code else re.match(r"^(#{1,6})\s+(.*?)\s*#*\s*$", line)
        if match:
            headings.setdefault(anchor(match.group(2)), number)
    # links to the headings
    for number, line, code in rows:
        if code:
            continue
        for target in re.findall(r"\]\(#([^)]+)\)", line):
            if target not in headings and not (placeholders and "{{" in target):
                issues.append(f"{path}:{number}: link to '#{target}': no such heading")
    # the types table: each type has its section; the summary frames use the byte of their type
    types = {}
    for start, header, body in tables(lines):
        if header and header[0].lower().startswith("byte") and len(header) > 1 and header[1].lower().startswith("type"):
            for number, cells in body:
                byte = re.match(r"([A-Z]{2,3})", cells[0])
                name = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", cells[1]).strip() if len(cells) > 1 else ""
                if byte and name:
                    types[name.lower()] = (byte.group(1), number)
                    if anchor(name) not in headings:
                        issues.append(f"{path}:{number}: type '{name}' has no section (### {name})")
        if header and header[0].lower().startswith("code"):
            seen = {}
            for number, cells in body:
                code = re.search(r"`?(-?\d+)`?", cells[0])
                if code and code.group(1) in seen:
                    issues.append(f"{path}:{number}: code {code.group(1)} already used line {seen[code.group(1)]} in this table")
                elif code:
                    seen[code.group(1)] = number
    for number, line, code in rows:
        frame = re.match(r"^\s*/\*\s*(.+?)\s*\*/\s*ENQ\s+\S+\s+ETB\s+([A-Z]{2,3})\b", line) if code else None
        if frame and types:
            name, byte = frame.group(1).lower(), frame.group(2)
            if name not in types:
                issues.append(f"{path}:{number}: frame '{frame.group(1)}' is not in the types table")
            elif types[name][0] != byte:
                issues.append(f"{path}:{number}: frame '{frame.group(1)}' uses {byte}, the types table says {types[name][0]} (line {types[name][1]})")
    common(path, lines, issues, placeholders)

def check_html(path: str, text: str, issues: list, placeholders: bool) -> None:
    ids = set(re.findall(r'\bid="([^"]+)"', text))
    lines = text.split("\n")
    for number, line in enumerate(lines, 1):
        for target in re.findall(r'href="#([^"]+)"', line):
            if target not in ids:
                issues.append(f"{path}:{number}: link to '#{target}': no such id")
    common(path, lines, issues, placeholders)

def common(path: str, lines: list, issues: list, placeholders: bool) -> None:
    for number, line in enumerate(lines, 1):
        if not placeholders and "{{" in line:
            issues.append(f"{path}:{number}: placeholder left: {re.search(r'{{[^}]*}}', line).group(0) if re.search(r'{{[^}]*}}', line) else '{{'}")
        lower = line.lower()
        for typo, fix in TYPOS.items():
            if re.search(r"\b" + re.escape(typo) + r"\b", lower):
                issues.append(f"{path}:{number}: '{typo}' -> '{fix}'")

def main() -> int:
    parser = argparse.ArgumentParser(description="Check a protocol documentation (Markdown / HTML).")
    parser.add_argument("files", nargs="+")
    parser.add_argument("--allow-placeholders", action="store_true", help="templates: {{...}} allowed")
    args = parser.parse_args()
    issues = []
    for path in args.files:
        try:
            text = open(path, encoding="utf-8").read()
        except OSError as error:
            sys.stderr.write(f"check_protocol: {path}: {error.strerror}\n")
            return 2
        if path.endswith((".html", ".htm")):
            check_html(path, text, issues, args.allow_placeholders)
        else:
            check_markdown(path, text, issues, args.allow_placeholders)
    for issue in issues:
        print(issue)
    print(f"{len(issues)} issue(s) in {len(args.files)} file(s)")
    return 1 if issues else 0

if __name__ == "__main__":
    sys.exit(main())
