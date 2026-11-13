#!/usr/bin/env python3
"""
Generate the Xartania-style file header for a .hpp/.cpp file.

Usage:
    header.py --file Timer.hpp [--banner xartania|none|"<NAME>"] [--desc "..."]
              [--author Tsukini] [--date DD/MM/YYYY]
    header.py --banner-only "<NAME>"      # print only the ASCII banner
    header.py --file tool.py --style py   # Python box: 63 double quotes, no @date / @file tags

--banner:
    xartania  -> default XARTANIA banner
    none      -> header box without any banner
    <NAME>    -> custom banner rendered with the ANSI Shadow font
"""

import argparse
import datetime
import os
import sys

FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fonts", "ansi_shadow.flf")
BOX_TOP = "/**************************************************************\\"
BOX_BOTTOM = "\\**************************************************************/"
DEFAULT_DESC = [
    "You know, I don t think there are good or bad descriptions,",
    "for me, life is all about functions...",
]

def load_font(path: str) -> tuple[int, dict[str, list[str]]]:
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    params = lines[0].split()
    hardblank = params[0][-1]
    height = int(params[1])
    comments = int(params[5])
    glyphs: dict[str, list[str]] = {}
    i = 1 + comments
    for code in range(32, 127): # only the required ascii characters
        rows = lines[i:i + height]
        i += height
        clean = []
        for row in rows:
            end = row[-1] if row else ""
            row = row.rstrip(end) if end else row
            clean.append(row.replace(hardblank, " "))
        glyphs[chr(code)] = clean
    return height, glyphs

def render(text: str) -> list[str]:
    height, glyphs = load_font(FONT)
    out = [""] * height
    for c in text.upper():
        glyph = glyphs.get(c, glyphs[" "])
        width = max(len(r) for r in glyph)
        for r in range(height):
            out[r] += glyph[r].ljust(width)
    # No trailing space (the last glyph is padded) and no empty row at the end
    out = [row.rstrip() for row in out]
    while out and not out[-1]:
        out.pop()
    return out

def banner_lines(banner: str) -> list[str]:
    if banner.lower() == "none":
        return []
    name = "XARTANIA" if banner.lower() == "xartania" else banner
    return [""] + [" " + row for row in render(name)] + [""]

def header(file: str, banner: str, desc: str | None, author: str, date: str, style: str = "cpp") -> str:
    description = [d.strip() for d in desc.split("\n")] if desc else DEFAULT_DESC
    python = style == "py"
    lines = ['"' * 63 if python else BOX_TOP]
    lines += banner_lines(banner)
    lines += [
        "Edition:",
        f"##  {date} by {author}" if python else f"##  @date {date} by @author {author}",
        "",
        "File Name:",
        f"##  {file}" if python else f"##  @file {file}",
        "",
        "File Description:",
    ]
    lines += [f"##  {d}" for d in description]
    lines.append('"' * 63 if python else BOX_BOTTOM)
    return "\n".join(lines)

def main() -> int:
    parser = argparse.ArgumentParser(description="Xartania header generator")
    parser.add_argument("--file", help="file name written in the header (ex: Timer.hpp)")
    parser.add_argument("--banner", default="xartania", help="xartania | none | <custom name>")
    parser.add_argument("--desc", default=None, help="file description (\\n for multiple lines)")
    parser.add_argument("--author", default="Tsukini")
    parser.add_argument("--date", default=datetime.date.today().strftime("%d/%m/%Y"))
    parser.add_argument("--banner-only", metavar="NAME", help="only print the ascii banner of NAME")
    parser.add_argument("--style", choices=["cpp", "py"], default=None, help="box of the header (default: from the extension of --file)")
    args = parser.parse_args()

    if args.banner_only:
        print("\n".join(render(args.banner_only)))
        return 0
    if not args.file:
        parser.error("--file is required")
    desc = args.desc.replace("\\n", "\n") if args.desc else None
    style = args.style or ("py" if args.file.endswith(".py") else "cpp")
    print(header(os.path.basename(args.file), args.banner, desc, args.author, args.date, style))
    return 0

if __name__ == "__main__":
    sys.exit(main())
