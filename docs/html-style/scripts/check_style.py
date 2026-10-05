#!/usr/bin/env python3
"""
Check a page / stylesheet against the rules of the user's HTML style.

Usage:
    check_style.py <file.html|file.css> ... [--compare <reference.css|html>]

Checks (each file):
- tokens: the light (:root) and dark (:root[data-theme="dark"]) themes define the same variables;
- contrast (WCAG): text pairs >= 4.5:1, graphics (lines, borders of meaning) >= 3:1, in both themes;
- inherited text colour: an inline element with its own light background but no colour (code, kbd, mark...) inside
  an element setting a colour that is unreadable on it (ex: white table head text on a light code chip) without an
  override rule `<parent> <element> { color: ... }`;
- self-contained: no external script / stylesheet / @import / url(http...), no web font, no gradient.
--compare: lists the tokens whose value differs from the reference (the style source is html-style/templates/style.css).
Exit 1 when an error is found (warnings don't fail).
"""

import argparse
import re
import sys
from pathlib import Path

TEXT_PAIRS = [  # (foreground, background, minimum ratio)
    ("text", "bg", 4.5), ("text", "surface", 4.5), ("muted", "bg", 4.5), ("muted", "surface", 4.5),
    ("accent", "surface", 4.5), ("accent", "bg", 4.5), ("text", "code-bg", 4.5), ("text", "accent-soft", 4.5),
    ("accent", "accent-soft", 4.5), ("surface", "accent", 4.5), ("text", "warn-soft", 4.5), ("warn", "warn-soft", 3),
    ("text", "ok-soft", 4.5), ("ok", "ok-soft", 3), ("line", "surface", 3), ("text", "node", 4.5),
    ("text", "node-alt", 4.5), ("muted", "code-bg", 4.5),
]
INLINE = r"(code|kbd|mark|samp|var|dfn|abbr|\.badge|\.tag|\.chip|\.pill)"


def css_of(path: Path) -> str:
    text = path.read_text(encoding="utf-8")
    if path.suffix.lower() in (".html", ".htm"):
        return "\n".join(re.findall(r"<style[^>]*>(.*?)</style>", text, re.S))
    return text


def strip_comments(css: str) -> str:
    return re.sub(r"/\*.*?\*/", "", css, flags=re.S)


def block(css: str, selector: str) -> dict:
    """Variables of the first `selector { ... }` block (exact selector)."""
    m = re.search(re.escape(selector) + r"\s*\{([^{}]*)\}", css)
    return dict(re.findall(r"--([\w-]+)\s*:\s*([^;]+);", m.group(1))) if m else {}


def rgb(value: str):
    v = value.strip().lower()
    m = re.fullmatch(r"#([0-9a-f]{3}|[0-9a-f]{6})", v)
    if m:
        h = m.group(1)
        h = "".join(c * 2 for c in h) if len(h) == 3 else h
        return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
    if v in ("white", "#fff"):
        return (255, 255, 255)
    if v == "black":
        return (0, 0, 0)
    return None


def luminance(c):
    def ch(x):
        x /= 255
        return x / 12.92 if x <= 0.03928 else ((x + 0.055) / 1.055) ** 2.4
    r, g, b = (ch(x) for x in c)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    la, lb = sorted((luminance(a), luminance(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


def resolve(value: str, tokens: dict):
    value = value.strip()
    m = re.fullmatch(r"var\(--([\w-]+)\)", value)
    if m:
        value = tokens.get(m.group(1), "")
    return rgb(value)


def rules(css: str):
    """Flat (selector, declarations) list, @media blocks included (their condition is ignored)."""
    out = []
    for sel, body in re.findall(r"([^{}@]+)\{([^{}]*)\}", css):
        decl = dict((k.strip(), v.strip()) for k, v in re.findall(r"([\w-]+)\s*:\s*([^;]+)", body))
        for s in sel.split(","):
            out.append((s.strip(), decl))
    return out


def check(path: Path, reference: dict | None) -> int:
    raw = css_of(path)
    css = strip_comments(raw)
    errors, warnings = [], []
    light = block(css, ":root")
    dark = block(css, ':root[data-theme="dark"]')

    # Tokens of both themes
    if light and dark:
        for k in sorted(set(light) ^ set(dark)):
            errors.append(f"token --{k} defined in only one theme ({'light' if k in light else 'dark'})")
    elif light and not dark:
        warnings.append("no dark theme (:root[data-theme=\"dark\"] block)")

    # Contrast of the token pairs
    for name, tokens in [("light", light)] + ([("dark", {**light, **dark})] if dark else []):
        for fg, bg, minimum in TEXT_PAIRS:
            a, b = rgb(tokens.get(fg, "")), rgb(tokens.get(bg, ""))
            if a and b and contrast(a, b) < minimum:
                errors.append(f"{name}: --{fg} on --{bg} = {contrast(a, b):.2f}:1 (< {minimum})")

    # Inherited colour on an inline element with its own background
    flat = rules(css)
    themes = [("light", light)] + ([("dark", {**light, **dark})] if dark else [])
    for theme, tokens in themes:
        coloured = [(s, resolve(d["color"], tokens)) for s, d in flat if "color" in d and resolve(d["color"], tokens)]
        chips = [(s, resolve(d.get("background", d.get("background-color", "")), tokens)) for s, d in flat
                 if re.search(INLINE + r"$", s) and "color" not in d
                 and resolve(d.get("background", d.get("background-color", "")), tokens)]
        for csel, cval in coloured:
            if (re.search(INLINE + r"$", csel) or csel.startswith(":root") or csel in ("body", "html")
                    or re.search(r"::|button|input|select|option|svg|switch", csel)):  # can't hold inline content
                continue
            for isel, bval in chips:
                if " " in isel and not isel.startswith(csel):  # chip already scoped elsewhere
                    continue
                if contrast(cval, bval) >= 3:
                    continue
                last = isel.split()[-1]
                head = csel.split()[0]  # `th` of `th strong`: an override `th code` also covers it
                fixed = any(re.search(r"(^|\s)" + re.escape(head) + r"\s", s) and s.endswith(last) and "color" in d
                            for s, d in flat)
                if not fixed:
                    msg = f"{theme}: `{last}` inside `{csel}` inherits a colour at {contrast(cval, bval):.2f}:1 on its background (add `{csel} {last} {{ color: ... }}`)"
                    if msg not in errors:
                        errors.append(msg)

    # Self-contained, no gradient / web font
    html = path.read_text(encoding="utf-8")
    for pattern, what, text in ((r"<script[^>]+src=[\"']https?:", "external script", html),
                                (r"<link[^>]+rel=[\"']stylesheet[\"'][^>]+href=[\"']https?:", "external stylesheet", html),
                                (r"@import", "@import", css), (r"url\([\"']?https?:", "external url()", css),
                                (r"@font-face", "web font", css), (r"(linear|radial|conic)-gradient\(", "gradient", css)):
        if re.search(pattern, text):
            errors.append(f"{what} found (pages are self-contained, no gradient / web font)")

    if reference is not None:
        for k, v in sorted(light.items()):
            if k in reference and reference[k].strip().lower() != v.strip().lower():
                warnings.append(f"--{k}: {v} (reference {reference[k]})")

    print(f"== {path}: {len(errors)} error(s), {len(warnings)} warning(s)")
    for e in errors:
        print(f"  error: {e}")
    for w in warnings:
        print(f"  warning: {w}")
    return len(errors)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+")
    ap.add_argument("--compare")
    a = ap.parse_args()
    reference = block(strip_comments(css_of(Path(a.compare))), ":root") if a.compare else None
    total = sum(check(Path(f), reference) for f in a.files)
    sys.exit(1 if total else 0)


if __name__ == "__main__":
    main()
