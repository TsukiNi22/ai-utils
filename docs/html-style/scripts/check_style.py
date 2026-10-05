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
- diagrams: arrows whose ends touch no shape (rect / circle / ellipse; data-from / data-to arrows are computed by
  base.js and skipped), text out of the drawing or crossing the border of a box;
- self-contained: no external script / stylesheet / @import / url(http...), no web font, no gradient.
--compare: lists the tokens whose value differs from the reference (the style source is html-style/templates/style.css).
Exit 1 when an error is found (warnings don't fail).
"""

import argparse
import re
from html.parser import HTMLParser
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


# ---------- Diagrams (inline SVG of the pages) ----------
FONT = {"lbl": 13, "small": 11.5, "title": 12}


class SvgCollector(HTMLParser):
    """Shapes, arrows and texts of every <svg> with a viewBox, translate() of the groups applied."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.svgs, self.stack, self.text = [], [], None

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "svg" and "viewbox" in a and not self.stack:  # attribute names arrive lower-cased
            vb = [float(v) for v in re.split(r"[\s,]+", a["viewbox"].strip())]
            self.svgs.append({"vb": vb, "shapes": [], "arrows": [], "texts": [], "line": self.getpos()[0]})
            self.stack.append((tag, 0.0, 0.0, False))
            return
        if not self.stack:
            return
        _, ox, oy, skip = self.stack[-1]
        t = a.get("transform", "")
        m = re.fullmatch(r"\s*translate\(\s*(-?[\d.]+)[\s,]*(-?[\d.]+)?\s*\)\s*", t) if t else None
        if t and not m:
            skip = True  # rotate / scale: not checked
        elif m:
            ox, oy = ox + float(m.group(1)), oy + float(m.group(2) or 0)
        skip = skip or "data-at" in a or tag in ("defs", "marker", "clipPath", "pattern", "symbol")
        if tag not in ("rect", "circle", "ellipse", "path", "line", "polyline", "text", "image", "use", "stop", "br"):
            self.stack.append((tag, ox, oy, skip))
        svg = self.svgs[-1]
        num = lambda k: float(re.sub(r"[^\d.\-]", "", a.get(k, "0")) or 0)
        if skip:
            pass
        elif tag == "rect":
            svg["shapes"].append(("rect", num("x") + ox, num("y") + oy, num("width"), num("height")))
        elif tag in ("circle", "ellipse"):
            rx = num("r") if tag == "circle" else num("rx")
            ry = num("r") if tag == "circle" else num("ry")
            svg["shapes"].append(("ellipse", num("cx") + ox, num("cy") + oy, rx, ry))
        elif tag in ("path", "line") and "marker-end" in a and "data-from" not in a and "data-free" not in a:
            pts = path_ends(a.get("d", "")) if tag == "path" else ((num("x1"), num("y1")), (num("x2"), num("y2")))
            if pts:
                svg["arrows"].append(((pts[0][0] + ox, pts[0][1] + oy), (pts[1][0] + ox, pts[1][1] + oy), self.getpos()[0]))
        if tag == "text" and not skip and "data-label-for" not in a:
            size = next((FONT[c] for c in a.get("class", "").split() if c in FONT), None)
            if size is None:
                fs = re.search(r"([\d.]+)", a.get("font-size", "13"))
                size = float(fs.group(1)) if fs else 13
            self.text = {"x": num("x") + ox, "y": num("y") + oy, "anchor": a.get("text-anchor", "start"),
                         "size": size, "s": "", "line": self.getpos()[0]}
        if tag == "text":
            self.stack.append((tag, ox, oy, skip))

    def handle_endtag(self, tag):
        if tag == "text" and self.text is not None:
            if self.text["s"].strip():
                self.svgs[-1]["texts"].append(self.text)
            self.text = None
        if self.stack and self.stack[-1][0] == tag:
            self.stack.pop()

    def handle_data(self, data):
        if self.text is not None:
            self.text["s"] += data


def path_ends(d: str):
    """First and last point of an SVG path (absolute and relative commands)."""
    tok = re.findall(r"[MmLlHhVvCcSsQqTtAaZz]|-?\d*\.?\d+(?:e-?\d+)?", d)
    x = y = sx = sy = 0.0
    first, i, cmd = None, 0, None
    sizes = {"m": 2, "l": 2, "h": 1, "v": 1, "c": 6, "s": 4, "q": 4, "t": 2, "a": 7, "z": 0}
    while i < len(tok):
        if re.fullmatch(r"[A-Za-z]", tok[i]):
            cmd = tok[i]; i += 1
            if cmd in "Zz":
                x, y = sx, sy
                continue
        if cmd is None:
            return None
        n = sizes[cmd.lower()]
        v = [float(t) for t in tok[i:i + n]]
        if len(v) < n:
            break
        i += n
        rel = cmd.islower()
        c = cmd.lower()
        if c == "h":
            x = x + v[0] if rel else v[0]
        elif c == "v":
            y = y + v[0] if rel else v[0]
        else:
            nx, ny = v[-2], v[-1]
            x, y = (x + nx, y + ny) if rel else (nx, ny)
        if c == "m":
            sx, sy = x, y
            cmd = "l" if rel else "L"
        if first is None:
            first = (x, y)
    return (first, (x, y)) if first else None


def border_distance(shape, p):
    """Distance from p to the border of a shape (0 on the border)."""
    if shape[0] == "rect":
        _, x, y, w, h = shape
        inside = x <= p[0] <= x + w and y <= p[1] <= y + h
        if inside:
            return min(p[0] - x, x + w - p[0], p[1] - y, y + h - p[1])
        dx = max(x - p[0], 0, p[0] - x - w)
        dy = max(y - p[1], 0, p[1] - y - h)
        return (dx * dx + dy * dy) ** 0.5
    _, cx, cy, rx, ry = shape
    if rx <= 0 or ry <= 0:
        return float("inf")
    k = ((p[0] - cx) ** 2 / rx ** 2 + (p[1] - cy) ** 2 / ry ** 2) ** 0.5
    return abs(k - 1) * min(rx, ry)


def check_diagrams(html_text: str):
    errors, warnings = [], []
    c = SvgCollector()
    c.feed(html_text)
    for svg in c.svgs:
        vx, vy, vw, vh = svg["vb"]
        if vw <= 40 or vh <= 40:  # icons
            continue
        shapes = [sh for sh in svg["shapes"] if not (sh[0] == "rect" and sh[3] >= vw * 0.95 and sh[4] >= vh * 0.95)]
        for start, end, line in svg["arrows"]:
            for label, p in (("end", end), ("start", start)):
                if not shapes or min(border_distance(sh, p) for sh in shapes) > 8:
                    errors.append(f"line {line}: arrow {label} ({p[0]:.0f}, {p[1]:.0f}) touches no shape "
                                  f"(use data-from / data-to, or data-free for an arrow pointing to nothing)")
        for t in svg["texts"]:
            w = len(t["s"].strip()) * t["size"] * 0.56
            x0 = t["x"] - (w / 2 if t["anchor"] == "middle" else w if t["anchor"] == "end" else 0)
            x1, y0, y1 = x0 + w, t["y"] - t["size"] * 0.8, t["y"] + t["size"] * 0.25
            if x0 < vx - 1 or x1 > vx + vw + 1 or y0 < vy - 1 or y1 > vy + vh + 1:
                errors.append(f"line {t['line']}: text '{t['s'].strip()[:40]}' goes out of the drawing (viewBox {vw:.0f}x{vh:.0f})")
                continue
            for sh in shapes:
                if sh[0] != "rect":
                    continue
                _, x, y, rw, rh = sh
                overlap = x0 < x + rw and x1 > x and y0 < y + rh and y1 > y
                inside = x0 >= x - 1 and x1 <= x + rw + 1 and y0 >= y - 1 and y1 <= y + rh + 1
                if overlap and not inside and rw > w * 0.6:
                    warnings.append(f"line {t['line']}: text '{t['s'].strip()[:40]}' crosses the border of a box")
                    break
    return errors, warnings


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

    if path.suffix.lower() in (".html", ".htm"):
        e, w = check_diagrams(html)
        errors += e
        warnings += w

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
