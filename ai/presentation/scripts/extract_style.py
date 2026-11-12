#!/usr/bin/env python3
"""Propose a theme (themes.json format) from a presentation / page the user gives as the model to follow.

    python3 extract_style.py FILE [--name my-style] [--save]

FILE: .pptx (theme colours + fonts of the file, then the colours of what is drawn), .pdf or an image (.png / .jpg:
dominant colours of the pages, fonts of `pdffonts`), .html / .css (the :root variables, the colours and fonts used).
Prints the JSON; --save writes it to ~/.claude/presentation/themes/<name>.json (then `--theme <name>` works in build_deck.py).
The result is a first guess from the colours: build the example deck with it and LOOK at the sheets before keeping it.
"""
import argparse
import colorsys
import json
import re
import subprocess
import sys
import tempfile
import zipfile
from collections import Counter
from pathlib import Path

USER_THEMES = Path.home() / ".claude" / "presentation" / "themes"
BASE = {"font": "Inter, 'Noto Sans', sans-serif", "head_font": "Inter, 'Noto Sans', sans-serif", "mono": "'JetBrains Mono', 'DejaVu Sans Mono', monospace",
        "head_transform": "none", "head_spacing": "0", "head_weight": "700", "radius": "6px", "bgimg": "none", "frame": "0", "inner_frame": "0", "cartouche": False}


def hex_of(rgb: tuple) -> str:
    return "#%02x%02x%02x" % tuple(int(v) for v in rgb[:3])


def lightness(color: str) -> float:
    r, g, b = (int(color[i:i + 2], 16) / 255 for i in (1, 3, 5))
    return colorsys.rgb_to_hls(r, g, b)[1]


def saturation(color: str) -> float:
    r, g, b = (int(color[i:i + 2], 16) / 255 for i in (1, 3, 5))
    return colorsys.rgb_to_hls(r, g, b)[2]


def mix(a: str, b: str, ratio: float) -> str:
    return hex_of([int(a[i:i + 2], 16) * (1 - ratio) + int(b[i:i + 2], 16) * ratio for i in (1, 3, 5)])


def theme_from(colors: list[str], fonts: list[str], source: str, name: str) -> dict:
    """colors: most frequent first (the first is the background)."""
    colors = [c for i, c in enumerate(colors) if c not in colors[:i]] or ["#ffffff", "#222222", "#1f4e9c"]
    bg = colors[0]
    dark = lightness(bg) < 0.5
    texts = sorted(colors, key=lambda c: -abs(lightness(c) - lightness(bg)))
    tx = texts[0] if abs(lightness(texts[0]) - lightness(bg)) > 0.5 else ("#f2f2f2" if dark else "#1c1c1c")
    accents = sorted([c for c in colors if saturation(c) > 0.25 and abs(lightness(c) - lightness(bg)) > 0.2], key=lambda c: -saturation(c) * abs(lightness(c) - lightness(bg)))
    ink = accents[0] if accents else mix(bg, tx, 0.6)
    ink2 = accents[1] if len(accents) > 1 else mix(ink, bg, 0.35)
    theme = dict(BASE)
    theme.update({"label": f"{name} (from {source})", "when": f"copied from {source}", "dark": dark, "bg": bg, "paper": mix(bg, tx, 0.06),
                  "ink": ink, "ink2": ink2, "tx": tx, "dim": mix(tx, bg, 0.45), "code": mix(bg, tx, 0.08 if not dark else 0.18), "codetx": tx,
                  "warn": "#d64545" if not dark else "#ff6b6b", "ok": "#2f9e44" if not dark else "#5af2a6"})
    if fonts:
        theme["font"] = f"'{fonts[0]}', Inter, sans-serif"
        theme["head_font"] = f"'{fonts[1] if len(fonts) > 1 else fonts[0]}', Inter, sans-serif"
    return theme


def from_pptx(path: Path) -> tuple[list[str], list[str]]:
    colors, fonts = [], []
    with zipfile.ZipFile(path) as z:
        names = sorted(n for n in z.namelist() if re.match(r"ppt/theme/theme\d+\.xml", n))
        if names:
            xml = z.read(names[0]).decode("utf-8", "ignore")
            scheme = {k: v for k, v in re.findall(r"<a:(dk1|lt1|dk2|lt2|accent\d)>.*?(?:srgbClr val|lastClr)=\"([0-9A-Fa-f]{6})\"", xml, re.S)}
            colors = ["#" + scheme[k].lower() for k in ("lt1", "dk1", "accent1", "accent2", "lt2", "dk2", "accent3") if k in scheme]
            fonts = re.findall(r"<a:latin typeface=\"([^\"]+)\"", xml)[:2]
        used = Counter()
        for name in z.namelist():
            if re.match(r"ppt/slide(Masters|Layouts|s)/[^/]+\.xml", name):
                used.update("#" + c.lower() for c in re.findall(r"srgbClr val=\"([0-9A-Fa-f]{6})\"", z.read(name).decode("utf-8", "ignore")))
        colors = colors[:1] + [c for c, _ in used.most_common(8)] + colors[1:]
    return colors, [f for f in fonts if f and not f.startswith("+")]


def from_pages(path: Path) -> tuple[list[str], list[str]]:
    from PIL import Image
    fonts: list[str] = []
    with tempfile.TemporaryDirectory() as tmp:
        if path.suffix.lower() == ".pdf":
            subprocess.run(["pdftoppm", "-r", "30", "-png", "-l", "8", str(path), f"{tmp}/p"], check=True)
            images = sorted(Path(tmp).glob("p-*.png"))
            listing = subprocess.run(["pdffonts", str(path)], capture_output=True, text=True, check=False).stdout.splitlines()[2:]
            fonts = [re.sub(r"^[A-Z]{6}\+", "", row.split()[0]).split("-")[0] for row in listing if row.strip()][:2]
        else:
            images = [path]
        counter: Counter = Counter()
        accents: Counter = Counter()
        for image in images:
            im = Image.open(image).convert("RGB")
            im.thumbnail((400, 400), Image.NEAREST)  # nearest: thin coloured lines stay pure colours
            for r, g, b in im.getdata():
                key = hex_of((r // 8 * 8 + 4, g // 8 * 8 + 4, b // 8 * 8 + 4))
                counter[key] += 1
                if saturation(key) > 0.35 and 0.15 < lightness(key) < 0.65:
                    accents[key] += 1
    return [c for c, _ in counter.most_common(4)] + [c for c, _ in accents.most_common(3)], fonts


def from_web(path: Path) -> tuple[list[str], list[str]]:
    text = path.read_text(encoding="utf-8", errors="ignore")
    variables = dict(re.findall(r"--([\w-]+)\s*:\s*(#[0-9a-fA-F]{6})\b", text))
    pick = [variables[k].lower() for k in ("bg", "background", "paper", "surface", "text", "tx", "ink", "accent", "primary", "ink2") if k in variables]
    counter = Counter(c.lower() for c in re.findall(r"#[0-9a-fA-F]{6}\b", text))
    colors = pick + [c for c, _ in counter.most_common(8)]
    fonts = [f.strip().strip("'\"") for f in re.findall(r"font-family\s*:\s*([^;}]+)", text) for f in f.split(",")[:1]]
    return colors, [f for f, _ in Counter(f for f in fonts if f and "var(" not in f and f not in ("inherit", "sans-serif", "monospace", "serif")).most_common(2)]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("file")
    parser.add_argument("--name", default=None)
    parser.add_argument("--save", action="store_true")
    args = parser.parse_args()
    path = Path(args.file).expanduser().resolve()
    if not path.is_file():
        sys.exit(f"extract_style: {path} not found")
    suffix = path.suffix.lower()
    if suffix == ".pptx":
        colors, fonts = from_pptx(path)
    elif suffix in (".pdf", ".png", ".jpg", ".jpeg"):
        colors, fonts = from_pages(path)
    elif suffix in (".html", ".htm", ".css"):
        colors, fonts = from_web(path)
    else:
        sys.exit("extract_style: give a .pptx, .pdf, .png, .jpg, .html or .css")
    name = args.name or re.sub(r"[^a-z0-9]+", "-", path.stem.lower()).strip("-")
    theme = theme_from(colors, fonts, path.name, name)
    print(json.dumps({name: theme}, indent=4, ensure_ascii=False))
    if args.save:
        USER_THEMES.mkdir(parents=True, exist_ok=True)
        (USER_THEMES / f"{name}.json").write_text(json.dumps(theme, indent=4, ensure_ascii=False), encoding="utf-8")
        print(f"saved: {USER_THEMES / (name + '.json')} (use: build_deck.py deck.json --theme {name})", file=sys.stderr)


if __name__ == "__main__":
    main()
