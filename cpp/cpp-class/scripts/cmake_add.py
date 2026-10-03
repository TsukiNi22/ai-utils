#!/usr/bin/env python3
"""
Add a source file to the source list of a CMakeLists.txt.

Usage:
    cmake_add.py <CMakeLists.txt> <src/path/File.cpp> [--var SRC] [--group "Name"] [--dry-run]

Behavior:
    - already listed            -> nothing to do (exit 0)
    - sources found with a GLOB -> nothing to do (exit 0)
    - inserted after the last file of the same directory (or of the closest parent),
      otherwise appended at the end of the list inside a new '## <Group>' block
    - no source list found      -> exit 2 (must be done by hand)
"""

import argparse
import os
import re
import sys

LIST_VARS = ("SRC", "SRCS", "SOURCES", "SOURCE", "SRC_FILES", "SOURCE_FILES")


def find_block(lines: list[str], var: str | None) -> tuple[int, int] | None:
    """Return (start, end) line indexes of the source list: start = 'set(VAR', end = ')' line."""
    names = (var,) if var else LIST_VARS
    openers = [re.compile(rf"^\s*set\(\s*{re.escape(n)}\b", re.I) for n in names]
    if not var: # fallback: target declaration with the sources inside
        openers.append(re.compile(r"^\s*add_(executable|library)\(", re.I))
    for opener in openers:
        for start, line in enumerate(lines):
            if not opener.search(line):
                continue
            depth = 0
            for end in range(start, len(lines)):
                code = lines[end].split("#", 1)[0]
                depth += code.count("(") - code.count(")")
                if depth <= 0:
                    block = lines[start:end + 1]
                    if any(".cpp" in l for l in block) or opener.pattern.startswith("^\\s*set"):
                        return start, end
                    break
    return None


def entry(line: str) -> str | None:
    code = line.split("#", 1)[0].strip().strip('"')
    if code.startswith("${CMAKE_SOURCE_DIR}/"):
        code = code[len("${CMAKE_SOURCE_DIR}/"):]
    return code if re.search(r"\.(cpp|cc|cxx|c)$", code) else None


def closeness(a: str, b: str) -> int:
    pa, pb = a.split("/"), b.split("/")
    n = 0
    while n < min(len(pa), len(pb)) and pa[n] == pb[n]:
        n += 1
    return n


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("cmake")
    parser.add_argument("source")
    parser.add_argument("--var", default=None, help="name of the source list variable (default: auto)")
    parser.add_argument("--group", default=None, help="comment of the new block if a new one is needed")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    with open(args.cmake, encoding="utf-8") as f:
        content = f.read()
    lines = content.split("\n")
    source = os.path.normpath(args.source)
    raw = re.sub(r"#.*", "", content)

    # Already present
    if re.search(rf"(^|[\s\"/]){re.escape(source)}([\s\")]|$)", raw, re.M):
        print(f"already listed: {source}")
        return 0

    # Globbing: the file will be found automatically
    if re.search(r"file\(\s*GLOB(_RECURSE)?[^)]*\.(cpp|cc|cxx)", raw, re.I):
        print("sources collected with file(GLOB...): nothing to add")
        return 0

    block = find_block(lines, args.var)
    if block is None:
        print("no source list found in the CMake, add it by hand", file=sys.stderr)
        return 2
    start, end = block

    entries = [(i, entry(lines[i])) for i in range(start, end + 1)]
    entries = [(i, e) for i, e in entries if e]
    folder = os.path.dirname(source)

    if entries:
        indent = re.match(r"^\s*", lines[entries[-1][0]]).group(0)
        best = max(closeness(os.path.dirname(e), folder) for _, e in entries)
        same = [i for i, e in entries if os.path.dirname(e) == folder]
        if same:
            new = [indent + source]
            at = same[-1] + 1
        else:
            # New block with a group comment ('## Module (sub-part: x)' like the existing ones)
            parts = folder.split("/")[1:] if folder.startswith("src/") else folder.split("/")
            parts = [p for p in parts if p] or ["Sources"]
            name = args.group or (parts[-1].capitalize() if len(parts) <= 2 else f"{parts[-2].capitalize()} (sub-part: {parts[-1]})")
            new = ["", indent + f"## {name}", indent + source]
            near = [i for i, e in entries if closeness(os.path.dirname(e), folder) == best and best > 0]
            at = (near[-1] + 1) if near else entries[-1][0] + 1
    else:
        indent = "    "
        new = [indent + source]
        at = end

    lines[at:at] = new
    if args.dry_run:
        print("\n".join(lines[start:end + len(new) + 1]))
        return 0
    with open(args.cmake, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"added {source} in {args.cmake} (line {at + len(new)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
