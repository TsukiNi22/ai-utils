#!/usr/bin/env python3
"""Build a presentation from one deck.json: an animated web page (one HTML file), a PDF, a PPTX and the oral notes.

    python3 build_deck.py deck.json [-o OUTDIR] [--formats html,pdf,pptx,notes] [--theme NAME | --theme-file FILE] [--canva]

deck.json (see ../templates/example.json):
    {"title", "subtitle", "lang", "author", "date", "duration": minutes, "theme": "blueprint", "slides": [slide, ...]}
slide = {"kind", "title", "chapter", "notes", "bg", ...}; kinds: title, section, bullets, columns, table, code, image, svg,
steps, grid, big, quote, end. Text accepts `code` and **bold**. The HTML page shows the points one by one (build steps),
the PDF and the PPTX show every step at once. "bg" (one colour or two for a gradient) gives a slide its own background:
the web page fades the colours from one slide to the next. Pictures: "icon" on a point / column / step / grid item (an
SVG takes the theme colour), "src" on bullets (a picture beside the points), "full" on an image (the whole slide),
"credit" under a picture. Themes: ../templates/themes.json, ~/.claude/presentation/themes/*.json, --theme-file.
"""
import argparse
import base64
import html
import itertools
import json
import mimetypes
import re
import shutil
import subprocess
import sys
import tempfile
import unicodedata
from pathlib import Path

HERE = Path(__file__).resolve().parent
TEMPLATES = HERE.parent / "templates"
USER_THEMES = Path.home() / ".claude" / "presentation" / "themes"
KINDS = ("title", "section", "bullets", "columns", "table", "code", "image", "svg", "steps", "grid", "big", "quote", "end")
NEEDS = {"bullets": "points", "columns": "columns", "table": "rows", "code": "code", "image": "src", "svg": "svg", "steps": "steps",
         "grid": "items", "quote": "text"} # what a slide of this kind cannot be built without
LISTS = ("points", "columns", "rows", "steps", "items")
UNTITLED = ("quote", "svg", "end", "big") # kinds whose title is optional
PICTURE_IDS = itertools.count(1) # prefix of the ids of each SVG put inline (two logos may both use #a)
PIC_HEIGHT = {"image": 470, "split": 440} # px of a picture box, minus the lead and the caption when there are some
THEME_VARS = ("bg", "paper", "ink", "ink2", "tx", "dim", "code", "codetx", "warn", "ok", "font", "head_font", "mono",
              "head_transform", "head_spacing", "head_weight", "radius", "bgimg", "frame", "inner_frame") # always written
COLOUR = re.compile(r"^(#[0-9a-fA-F]{3,8}|(rgb|rgba|hsl|hsla)\(.*\))$") # an extra theme key holding a colour
SVG_PAINT = re.compile(r'\s(fill|stroke|stop-color|flood-color|color)="(var\(--[a-z0-9-]+\))"') # var() in an attribute
REVEAL_FIRST, REVEAL_GAP, REVEAL_MAX = 0.25, 0.12, 12 # "reveal": "auto": first delay, gap between steps (s), steps timed
BG_ANGLE, BG_FADE = "115deg", 1.4 # per slide gradients: direction, seconds of the colour fade on the web page

# ======================================================================= model
def load_themes() -> dict:
    themes = json.loads((TEMPLATES / "themes.json").read_text(encoding="utf-8"))
    if USER_THEMES.is_dir():
        for file in sorted(USER_THEMES.glob("*.json")):
            themes[file.stem] = {**json.loads(file.read_text(encoding="utf-8")), "_dir": str(file.parent)}
    return themes

def pick_theme(deck: dict, args: argparse.Namespace) -> tuple[str, dict]:
    if args.theme_file:
        file = Path(args.theme_file).expanduser()
        if not file.is_file():
            sys.exit(f"build_deck: theme file {args.theme_file!r} not found")
        try:
            custom = json.loads(file.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            sys.exit(f"build_deck: {file}: not valid JSON ({error})")
        name = file.stem if file.stem != "theme" else file.resolve().parent.name # <look>/theme.json: the folder names it
        return name, {**load_themes()["blueprint"], **custom, "_dir": str(file.resolve().parent)}
    themes = load_themes()
    name = args.theme or deck.get("theme") or "blueprint"
    if name not in themes:
        sys.exit(f"build_deck: unknown theme {name!r} (known: {', '.join(themes)})")
    return name, {**themes["blueprint"], **themes[name]}

class MissingTool(RuntimeError):
    """A format that cannot be built here (its library is not installed): the other formats are still built."""

def installed(family: str) -> bool:
    import subprocess
    out = subprocess.run(["fc-match", "-f", "%{family}", family], capture_output=True, text=True, check=False).stdout
    return family.lower() in out.lower().split(",")[0].strip().lower() or out.lower().startswith(family.lower())

def canva_theme(theme: dict) -> dict:
    """Theme made for an import into Canva: ONE family per role taken from Canva's library (and installed here, so it is
    embedded in the PDF), no fallback stack (a fallback glyph = another font = a text cut in pieces), no letter spacing,
    no background drawn with gradients (hundreds of vector lines on import)."""
    canva = json.loads((TEMPLATES / "canva.json").read_text(encoding="utf-8"))

    def pick(stack: str, fallback: str) -> str:
        for name in (n.strip().strip("'\"") for n in stack.split(",")):
            if name in canva["fonts"] and installed(name):
                return f"'{name}'"
        return f"'{fallback}'"

    sans = pick(theme["font"], canva["sans"])
    return {**theme, "font": sans, "head_font": pick(theme["head_font"], canva["sans"]) if theme["head_font"] != theme["font"] else sans,
            "mono": pick(theme["mono"], canva["mono"]), "head_spacing": "0", "bgimg": "none"}

def check_model(deck: dict) -> None:
    slides = deck.get("slides")
    if not isinstance(slides, list) or not slides:
        sys.exit("build_deck: deck.json needs a non-empty 'slides' list")
    for index, slide in enumerate(slides, 1):
        if not isinstance(slide, dict):
            sys.exit(f"build_deck: slide {index} must be an object with a 'kind'")
        kind = slide.get("kind")
        if kind not in KINDS:
            sys.exit(f"build_deck: slide {index}: unknown kind {kind!r} (known: {', '.join(KINDS)})")
        if kind not in UNTITLED and not slide.get("title"):
            sys.exit(f"build_deck: slide {index} ({kind}) has no title")
        key = NEEDS.get(kind)
        if key and not slide.get(key):
            sys.exit(f"build_deck: slide {index} ({kind}) has no '{key}'")
        if key in LISTS and not isinstance(slide[key], list):
            sys.exit(f"build_deck: slide {index} ({kind}): '{key}' must be a list")
        if key in ("columns", "steps") and not all(isinstance(item, dict) for item in slide[key]):
            sys.exit(f"build_deck: slide {index} ({kind}): each of '{key}' must be an object (head, {'points' if key == 'columns' else 'text'}, icon)")
        if key == "points" and not all(isinstance(item, str) or (isinstance(item, dict) and item.get("text")) for item in slide[key]):
            sys.exit(f"build_deck: slide {index} ({kind}): a point is a text or an object with a 'text' (and 'sub', 'icon')")
        if key == "items" and not all(isinstance(item, dict) and item.get("icon") for item in slide[key]):
            sys.exit(f"build_deck: slide {index} (grid): each item is an object with an 'icon' (and a 'label')")
        if kind == "big" and slide.get("value") in (None, "") and not (isinstance(slide.get("items"), list) and slide["items"]
                                                                        and all(isinstance(i, dict) and i.get("value") not in (None, "") for i in slide["items"])):
            sys.exit(f"build_deck: slide {index} (big) needs a 'value' (or 'items': [{{value, label}}, ...])")

def inline(text: str) -> str:
    """Escape, then `code` and **bold**."""
    out = html.escape(str(text), quote=False)
    out = re.sub(r"`([^`]+)`", r"<code>\1</code>", out)
    return re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", out)

def slug(text: str) -> str:
    """File name of the outputs: accents dropped before the non letters, never an empty name."""
    ascii_text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"[^a-z0-9]+", "-", ascii_text.lower()).strip("-") or "deck"

def svg_paint(markup: str) -> str:
    """A browser ignores var() in an SVG presentation attribute (fill="var(--ink)" is painted black): move it into the
    style attribute of the same tag, merged with a style already there (two style attributes: the second is dropped)."""
    def fix(match: re.Match) -> str:
        tag = match.group(0)
        styles = [s.strip().rstrip(";") for s in re.findall(r'\sstyle="([^"]*)"', tag)]
        paints = [f"{name}:{value}" for name, value in SVG_PAINT.findall(tag)]
        if not paints and len(styles) < 2:
            return tag
        tag = SVG_PAINT.sub("", re.sub(r'\sstyle="[^"]*"', "", tag))
        merged = "; ".join(s for s in styles + paints if s)
        end = "/>" if tag.endswith("/>") else ">"
        return tag[:-len(end)].rstrip() + f' style="{merged}"' + end
    return re.sub(r"<svg\b.*?</svg>", lambda block: re.sub(r"<[a-zA-Z][^<>]*>", fix, block.group(0)), markup, flags=re.S)

def plain(text: str) -> str:
    return re.sub(r"\*\*([^*]+)\*\*|`([^`]+)`", lambda m: m.group(1) or m.group(2), str(text))

def points(slide: dict, key: str = "points") -> list[tuple[str, list[str], str]]:
    """A list of "text" or {"text", "sub": [...], "icon"} as (text, subs, icon)."""
    out = []
    for item in slide.get(key, []):
        out.append((item, [], "") if isinstance(item, str) else (item["text"], list(item.get("sub", [])), item.get("icon", "")))
    return out

def label_of(slide: dict) -> str:
    """What names a slide in the outline, the notes and the PDF bookmarks."""
    first = (slide.get("items") or [{}])[0] if slide["kind"] == "big" else {}
    return plain(slide.get("title") or slide.get("text") or slide.get("value") or first.get("value") or "")

def data_uri(path: str, base: Path) -> str:
    file = (base / path).resolve()
    if not file.is_file():
        sys.exit(f"build_deck: image not found: {file}")
    kind = mimetypes.guess_type(file.name)[0] or "application/octet-stream"
    return f"data:{kind};base64," + base64.b64encode(file.read_bytes()).decode("ascii")

def svg_inline(file: Path, key: str) -> str | None:
    """An SVG file made fit to sit inline in the page (it then takes the theme colour), or None when it must stay a
    picture (a <style> or an <image> would leak into the page). Nothing that runs is kept (script, foreignObject, on*
    handlers, links out of the file), nor its own size (the CSS sizes it); its ids get the prefix `key`. A one-colour
    icon (no fill anywhere: Simple Icons) is filled with currentColor."""
    text = re.sub(r"<\?xml.*?\?>|<!DOCTYPE[^>]*>|<!--.*?-->", "", file.read_text(encoding="utf-8", errors="replace"), flags=re.S | re.I)
    if re.search(r"<(style|image)\b|@import", text, re.I):
        return None
    text = re.sub(r"<(script|foreignObject|title|metadata)\b.*?</\1\s*>|<(script|foreignObject)\b[^>]*/>", "", text, flags=re.S | re.I)
    text = re.sub(r"\son[a-z]+\s*=\s*(\"[^\"]*\"|'[^']*')", "", text, flags=re.I)
    text = re.sub(r"((?:xlink:)?href\s*=\s*[\"'])(?!#)[^\"']*", r"\1#", text, flags=re.I)
    start, end = text.find("<svg"), text.rfind("</svg>")
    if start < 0 or end < 0:
        sys.exit(f"build_deck: {file}: not an SVG")
    text = text[start:end + 6]
    for ident in set(re.findall(r'\sid="([^"]+)"', text)):
        new = f"{key}-{ident}"
        text = text.replace(f'id="{ident}"', f'id="{new}"').replace(f"url(#{ident})", f"url(#{new})").replace(f'href="#{ident}"', f'href="#{new}"')
    cut = text.find(">")
    root = re.sub(r'\s(width|height|class|style)="[^"]*"', "", text[:cut]).rstrip("/").rstrip()
    if "fill=" not in root and not re.search(r'\sfill="(?!none")|fill:\s*(?!none)', text):
        root += ' fill="currentColor"'
    return root + ' data-ic="1" aria-hidden="true" focusable="false"' + text[cut:]

def picture(path: str, base: Path, cls: str, alt: str = "", inline: bool = True, height: int = 0) -> str:
    """An icon / a picture of the deck folder in a <span class=cls>: an SVG inline (it takes the theme colour) when it
    can be, anything else an <img> data URI. No alt = decorative (the text next to it says it)."""
    file = (base / path).resolve()
    if not file.is_file():
        sys.exit(f"build_deck: picture not found: {file}")
    size = f' style="height:{height}px"' if height else ""
    label = f'role="img" aria-label="{html.escape(alt)}"' if alt else 'aria-hidden="true"'
    markup = svg_inline(file, f"p{next(PICTURE_IDS)}") if inline and file.suffix.lower() == ".svg" else None
    if markup:
        return f'<span class="{cls}" {label}{size}>{markup}</span>'
    return f'<span class="{cls}"{size}><img src="{data_uri(path, base)}" alt="{html.escape(alt)}"></span>'

def visual(slide: dict) -> bool:
    """A slide carried by its pictures (icons, logos, a picture beside the points): laid out bigger and centered."""
    kind = slide["kind"]
    parts = [p for p in slide.get("points", []) + slide.get("columns", []) + slide.get("steps", []) if isinstance(p, dict)]
    return kind == "grid" or (kind == "bullets" and bool(slide.get("src"))) or any(p.get("icon") for p in parts)

def credit(slide: dict) -> str:
    return f'<small class="credit">{inline(slide["credit"])}</small>' if slide.get("credit") else ""

# ======================================================================= html (web page and PDF share it)
def slide_inner(slide: dict, number: int, base: Path) -> str:
    kind, title = slide["kind"], slide.get("title", "")
    head = f"<h2>{inline(title)}</h2>" if title and kind not in ("title", "section", "end") else ""
    lead = f'<p class="lead">{inline(slide["lead"])}</p>' if slide.get("lead") else ""
    mid = visual(slide)

    def body(markup: str) -> str:
        """A visual slide: its body in a box that fills the slide under the title and centers it (what WeasyPrint does
        right; auto margins on a flex child are not centered there)."""
        return f'<div class="mid">{markup}</div>' if mid else markup

    if kind == "title":
        meta = "".join(f"<li>{inline(m)}</li>" for m in slide.get("meta", []))
        return (f"<h1>{inline(title)}</h1><p class=\"sub\">{inline(slide.get('subtitle', ''))}</p>"
                + (f'<ul class="meta">{meta}</ul>' if meta else ""))
    if kind == "section":
        return (f'<div class="num">{slide.get("number", "")}</div><h2>{inline(title)}</h2>'
                + (f'<p class="lead">{inline(slide["lead"])}</p>' if slide.get("lead") else ""))
    if kind == "bullets":
        listed = points(slide)
        icons = any(icon for _, _, icon in listed)
        items = "".join(
            f'<li data-s="{i}"' + (' class="has-ic">' + (picture(icon, base, "ic") if icon else '<span class="ic"></span>') + '<div class="pt">' if icons else ">")
            + inline(text) + (f"<ul>{''.join(f'<li>{inline(s)}</li>' for s in subs)}</ul>" if subs else "") + ("</div>" if icons else "") + "</li>"
            for i, (text, subs, icon) in enumerate(listed, 1))
        listing = f'<ul class="icons">{items}</ul>' if icons else f"<ul>{items}</ul>"
        if not slide.get("src"):
            return f"{head}{lead}{body(listing)}"
        height = PIC_HEIGHT["split"] - (60 if lead else 0) - (26 if slide.get("credit") else 0)
        side = picture(slide["src"], base, "pic", slide.get("alt") or plain(title), height=height)
        return f'{head}{lead}' + body(f'<div class="split"><div class="side">{listing}</div><figure>{side}{credit(slide)}</figure></div>')
    if kind == "columns":
        cols = slide.get("columns", [])
        cells = "".join(
            f'<div class="col" data-s="{i}">' + (picture(c["icon"], base, "ic") if c.get("icon") else "") + f'<h3>{inline(c.get("head", ""))}</h3>'
            + (f"<ul>{''.join(f'<li>{inline(p)}</li>' for p in c['points'])}</ul>" if c.get("points") else "") + "</div>" for i, c in enumerate(cols, 1))
        return f'{head}{lead}' + body(f'<div class="cols" style="--n:{len(cols)}">{cells}</div>')
    if kind == "grid":
        cells = slide.get("items", [])
        per_row = len(cells) if len(cells) <= 4 else 3 if len(cells) in (5, 6, 9) else 4
        tiles = "".join(f'<li data-s="{i}">{picture(c["icon"], base, "ic", "" if c.get("label") else c.get("alt") or Path(c["icon"]).stem)}'
                       + (f'<span class="lbl">{inline(c["label"])}</span>' if c.get("label") else "") + "</li>" for i, c in enumerate(cells, 1))
        return f'{head}{lead}' + body(f'<ul class="grid{" many" if len(cells) > 4 else ""}" style="--n:{per_row}">{tiles}</ul>')
    if kind == "big":
        stats = slide.get("items") or [{"value": slide.get("value"), "label": slide.get("label", "")}]
        figures = "".join(f'<div class="stat"' + (f' data-s="{i}"' if len(stats) > 1 else "") + f'><div class="v">{inline(s["value"])}</div>'
                       + (f'<p class="l">{inline(s["label"])}</p>' if s.get("label") else "") + "</div>" for i, s in enumerate(stats, 1))
        return f'{head}{lead}<div class="stats{" one" if len(stats) == 1 else ""}" style="--n:{len(stats)}">{figures}</div>'
    if kind == "table":
        thead = "".join(f"<th scope=\"col\">{inline(h)}</th>" for h in slide.get("head", []))
        rows = "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in row) + "</tr>" for row in slide.get("rows", []))
        return f"{head}{lead}<table><thead><tr>{thead}</tr></thead><tbody>{rows}</tbody></table>"
    if kind == "code":
        caption = f'<p class="caption">{inline(slide["caption"])}</p>' if slide.get("caption") else ""
        return f'{head}{lead}<pre><code>{html.escape(slide.get("code", ""))}</code></pre>{caption}'
    if kind == "image":
        alt = slide.get("alt") or plain(title)
        caption = f"<figcaption>{inline(slide['caption'])}</figcaption>" if slide.get("caption") else ""
        if slide.get("full"): # the picture covers the slide, the title on a band over it
            band = f'<div class="band"><h2>{inline(title)}</h2>{lead}</div>' if title else ""
            return f'<figure class="bleed" data-nocheck>{picture(slide["src"], base, "pic", alt, inline=False)}</figure>{band}{credit(slide)}'
        height = PIC_HEIGHT["image"] - (60 if lead else 0) - (36 if caption else 0) - (26 if slide.get("credit") else 0)
        return f'{head}{lead}<figure>{picture(slide["src"], base, "pic", alt, height=height)}{caption}{credit(slide)}</figure>'
    if kind == "svg":
        caption = f"<figcaption>{inline(slide['caption'])}</figcaption>" if slide.get("caption") else ""
        return f'{head}{lead}<figure>{svg_paint(slide.get("svg", ""))}{caption}</figure>'
    if kind == "steps":
        steps = slide.get("steps", [])
        items = "".join(f'<li data-s="{i}"' + (f' class="has-ic">{picture(s["icon"], base, "ic")}' if s.get("icon") else ">")
                        + f'<h3>{inline(s.get("head", ""))}</h3>' + (f'<p>{inline(s["text"])}</p>' if s.get("text") else "") + "</li>" for i, s in enumerate(steps, 1))
        return f'{head}{lead}' + body(f'<ol class="flow" style="--n:{min(len(steps), 4)}">{items}</ol>')
    if kind == "quote":
        who = f"<cite>{inline(slide['by'])}</cite>" if slide.get("by") else ""
        return f"<blockquote>{inline(slide.get('text', ''))}</blockquote>{who}"
    links = "".join(f"<li>{inline(link)}</li>" for link in slide.get("links", []))  # end
    image = f'<figure>{picture(slide["src"], base, "pic", slide.get("alt", ""), height=260)}</figure>' if slide.get("src") else ""
    return f'<h1>{inline(title or "Thank you")}</h1><p class="sub">{inline(slide.get("subtitle", ""))}</p><ul>{links}</ul>{image}'

def slides_html(deck: dict, base: Path, with_notes: bool) -> str:
    out = []
    chapter = ""
    for number, slide in enumerate(deck["slides"], 1):
        chapter = slide.get("chapter", chapter) or deck.get("title", "")
        title = label_of(slide) or plain(deck.get("title", ""))
        notes = f'<div class="notes">{inline(slide["notes"])}</div>' if with_notes and slide.get("notes") else ""
        foot = f'<div class="foot"><span>{html.escape(plain(deck.get("title", "")))}</span><span>{html.escape(chapter)} · {number}</span></div>'
        full = " full" if slide["kind"] == "image" and slide.get("full") else ""
        full += " v" if visual(slide) else ""
        out.append(f'<section class="slide k-{slide["kind"]}{full}" id="s{number}" data-ch="{html.escape(chapter)}" '
                   f'data-title="{html.escape(title)}">{slide_inner(slide, number, base)}{foot}{notes}</section>')
    return "\n".join(out)

def custom_css(theme: dict, base: Path) -> str:
    """Free CSS of a 'Sur mesure' theme (`css` text and / or `css_file`): decor, shapes, gradients, @font-face, a layout
    per kind, even its own transitions - the look is not limited to the variables. Not used for a Canva import."""
    if theme.get("canva"):
        return ""
    text = theme.get("css", "")
    if theme.get("css_file"):
        for folder in (Path(theme.get("_dir", ".")), base):
            if (folder / theme["css_file"]).is_file():
                text += "\n" + (folder / theme["css_file"]).read_text(encoding="utf-8")
                break
        else:
            sys.exit(f"build_deck: css_file {theme['css_file']!r} not found")
    return "\n/* theme css */\n" + text if text else ""

def theme_css(theme: dict) -> str:
    """The theme as CSS variables: the fixed keys, then every extra key holding a colour (a custom theme.json may add
    "acc1", "acc2_line"...: --acc1 and --acc2-line exist in the CSS and in the inline SVG of the PDF)."""
    extra = [k for k, v in theme.items() if k not in THEME_VARS and not k.startswith("_") and isinstance(v, str) and COLOUR.match(v.strip())]
    rows = "; ".join(f"--{k.replace('_', '-')}: {theme[k]}" for k in (*THEME_VARS, *extra))
    return f":root {{ {rows}; }}"

def slide_bg(slide: dict, theme: dict) -> tuple[str, str] | None:
    """The two colours of a slide's own background ("bg": "#hex" or ["#hex", "#hex"]), None without one."""
    value = slide.get("bg")
    if not value:
        return None
    colours = [value] if isinstance(value, str) else list(value)
    if not 1 <= len(colours) <= 2 or not all(isinstance(c, str) and COLOUR.match(c.strip()) for c in colours):
        sys.exit(f"build_deck: slide {slide.get('title', '')!r}: bg must be a colour or two colours, not {value!r}")
    return colours[0], colours[-1]

def bg_css(deck: dict, theme: dict, web: bool) -> str:
    """Per slide backgrounds. PDF / no script: each slide paints its gradient under the theme's bgimg. Web page: one layer
    behind the slides whose two colours are registered (@property) so they fade from one slide to the next instead of
    cutting; a slide without "bg" takes the theme background."""
    pairs = [slide_bg(s, theme) for s in deck["slides"]]
    if not any(pairs):
        return ""
    angle = deck.get("bg_angle", BG_ANGLE)
    rows = ["/* per slide backgrounds */"]
    scope = "html:not(.js) " if web else "" # with the script, the fading layer behind the slides paints them
    for number, pair in enumerate(pairs, 1):
        if pair and theme.get("canva"): # a Canva import turns a gradient into a picture: the first colour only
            rows.append(f"{scope}#s{number} {{ background-color: {pair[0]}; }}")
        elif pair:
            rows.append(f"{scope}#s{number} {{ background-color: {pair[0]}; background-image: var(--bgimg), linear-gradient({angle}, {pair[0]}, {pair[1]}); }}")
    if not web:
        return "\n".join(rows) + "\n"
    fade = deck.get("bg_fade", BG_FADE)
    rows += ['@property --c1 { syntax: "<color>"; inherits: true; initial-value: transparent; }',
             '@property --c2 { syntax: "<color>"; inherits: true; initial-value: transparent; }',
             ".js .slide { background-color: transparent; background-image: var(--bgimg); }",
             "#deck { position: absolute; inset: 0; z-index: 1; }",
             f'#stage::before {{ content: ""; position: absolute; inset: 0; z-index: 0; background-color: var(--c1); '
             f"background-image: linear-gradient({angle}, var(--c1), var(--c2)); transition: --c1 {fade}s ease, --c2 {fade * 1.15:.2f}s ease; }}"]
    for number, pair in enumerate(pairs, 1):
        first, last = pair or (theme["bg"], theme["bg"])
        rows.append(f'html[data-slide="{number}"] #stage::before {{ --c1: {first}; --c2: {last}; }}')
    return "\n".join(rows) + "\n"

SKELETON = """<div id="stage"><main id="deck" class="{cls}">
{slides}
</main>
<div class="zone" id="zl" aria-hidden="true"></div><div class="zone" id="zr" aria-hidden="true"></div>
<div id="hud"><nav id="chapters" aria-label="Chapters"></nav><div id="bar"><i></i></div><div id="steps" aria-hidden="true"></div><span id="count"></span><div id="btns"><button type="button" id="prev" aria-label="Previous (left arrow)">&#8249;</button><button type="button" id="ovbtn" aria-label="Chapter map (O)">&#9638;</button><button type="button" id="fsbtn" aria-label="Full screen (F)">&#9974;</button><button type="button" id="next" aria-label="Next (right arrow)">&#8250;</button></div></div>
</div>
<div id="map" hidden></div>"""

def reveal_auto(slides: str) -> tuple[str, str]:
    """"reveal": "auto": no click per point. The build steps (data-s) become an entrance played when the slide comes in,
    one after the other; one click (or arrow) per slide. The PDF is not concerned (it shows every step anyway)."""
    rows = ['@keyframes reveal-in { from { opacity: 0; transform: translateY(12px); } to { opacity: 1; transform: none; } }',
            '.js .slide[data-state="active"] [data-in] { animation: reveal-in .5s cubic-bezier(.16,.84,.3,1) both; }']
    rows += [f'.js .slide[data-state="active"] [data-in="{n}"] {{ animation-delay: {REVEAL_FIRST + (n - 1) * REVEAL_GAP:.2f}s; }}' for n in range(1, REVEAL_MAX + 1)]
    return re.sub(r'\sdata-s="(\d+)"', r' data-in="\1"', slides), "\n".join(rows) + "\n"

def build_web(deck: dict, theme: dict, base: Path, out: Path) -> None:
    css = (TEMPLATES / "deck-base.css").read_text(encoding="utf-8") + "\n" + theme_css(theme) + "\n" + (TEMPLATES / "slides.css").read_text(encoding="utf-8") \
        + "\n" + bg_css(deck, theme, True) + custom_css(theme, base)
    js = (TEMPLATES / "deck.js").read_text(encoding="utf-8")
    slides = slides_html(deck, base, True)
    if deck.get("reveal", "click") == "auto":
        slides, motion = reveal_auto(slides)
        css += "\n" + motion
    elif deck.get("reveal", "click") != "click":
        sys.exit(f"build_deck: reveal must be \"click\" (a click per point) or \"auto\" (everything on arrival), not {deck['reveal']!r}")
    slides = re.sub(r'\sdata-click="(\d+)"', r' data-s="\1"', slides) # always one click each, even with reveal auto
    body = SKELETON.format(slides=slides, cls="cartouche" if theme.get("cartouche") else "")
    page = (f'<!DOCTYPE html>\n<html lang="{deck.get("lang", "en")}">\n<head>\n<meta charset="utf-8">\n'
            f'<meta name="viewport" content="width=device-width, initial-scale=1">\n<title>{html.escape(plain(deck.get("title", "")))}</title>\n'
            f'<style>\n{css}\n</style>\n</head>\n<body>\n{body}\n<script>\n{js}\n</script>\n</body>\n</html>\n')
    out.write_text(page, encoding="utf-8")

# what Canva cuts, merges or flattens when it reads a PDF: letter spacing, justified text, transforms, opacity, shadows
CANVA_CSS = """
.canva, .canva * { letter-spacing: 0 !important; text-shadow: none !important; box-shadow: none !important; text-align-last: auto; }
.canva .slide { text-rendering: auto; font-kerning: none; }
.canva .slide p, .canva .slide li, .canva .slide h1, .canva .slide h2, .canva .slide h3 { text-align: left; hyphens: none; }
.canva .slide code { background: none; padding: 0; }
.canva .slide pre { background: var(--code); }
"""

def build_pdf(deck: dict, theme: dict, base: Path, out: Path) -> None:
    try:
        from weasyprint import HTML
    except ImportError:
        raise MissingTool("the PDF needs weasyprint (sudo dnf install python3-weasyprint, or pip install --user weasyprint)") from None
    css = theme_css(theme) + "\n" + (TEMPLATES / "slides.css").read_text(encoding="utf-8") + "\n" + bg_css(deck, theme, False) + custom_css(theme, base)
    cls = "print cartouche" if theme.get("cartouche") else "print"
    if theme.get("canva"):
        css += CANVA_CSS
        cls += " canva"
    page = (f'<!DOCTYPE html><html lang="{deck.get("lang", "en")}"><head><meta charset="utf-8"><title>{html.escape(plain(deck.get("title", "")))}</title>'
            f'<style>{css}</style></head><body class="{cls}"><div class="{cls}">{pdf_svg(slides_html(deck, base, False), theme)}</div></body></html>')
    HTML(string=page, base_url=str(base)).write_pdf(str(out))

def pdf_svg(page: str, theme: dict) -> str:
    """WeasyPrint does not pass the page colour nor the CSS variables into an inline SVG: currentColor would be black.
    Give each <svg> the text colour of the theme and turn var(--key) into its value (the web page keeps the variables)."""
    def resolve(match: re.Match) -> str:
        block = re.sub(r"var\(--([a-z0-9-]+)\)", lambda v: str(theme.get(v.group(1).replace("-", "_"), v.group(0))), match.group(0))
        opening = block[:block.find(">")]
        if re.search(r"\scolor=", opening):
            return block
        return block.replace("<svg", f'<svg color="{theme["ink"] if "data-ic=" in opening else theme["tx"]}"', 1) # icons: the accent
    return re.sub(r"<svg\b.*?</svg>", resolve, page, flags=re.S)

# ======================================================================= pptx
def rgb(value: str) -> object:
    from pptx.dml.color import RGBColor
    return RGBColor.from_string(value.lstrip("#").upper())

def first_font(stack: str) -> str:
    return stack.split(",")[0].strip().strip("'\"")

def build_pptx(deck: dict, theme: dict, base: Path, out: Path) -> None:
    try:
        from pptx import Presentation
        from pptx.enum.shapes import MSO_SHAPE
        from pptx.enum.text import MSO_ANCHOR, MSO_AUTO_SIZE, PP_ALIGN
        from pptx.util import Emu, Inches, Pt
    except ImportError:
        raise MissingTool("the PPTX needs python-pptx (pip install --user python-pptx, or sudo dnf install python3-python-pptx)") from None
    prs = Presentation()
    prs.slide_width, prs.slide_height = Inches(13.333), Inches(7.5)
    blank = prs.slide_layouts[6]
    sans, mono, head = first_font(theme["font"]), first_font(theme["mono"]), first_font(theme["head_font"])
    W, H, M = 13.333, 7.5, 0.7

    def box(slide: object, x: float, y: float, w: float, h: float, fill: str | None = None, line: str | None = None, shape: object = MSO_SHAPE.RECTANGLE) -> object:
        shp = slide.shapes.add_shape(shape, Inches(x), Inches(y), Inches(w), Inches(h))
        if fill:
            shp.fill.solid()
            shp.fill.fore_color.rgb = rgb(fill)
        else:
            shp.fill.background()
        if line:
            shp.line.color.rgb = rgb(line)
            shp.line.width = Pt(1.5)
        else:
            shp.line.fill.background()
        shp.shadow.inherit = False
        return shp

    def borders(cell: object, color: str, width: int = 12700) -> None:
        """The four borders of a table cell (python-pptx has no API for it): drawn lines, as in the web page and the PDF."""
        from lxml import etree
        props = cell._tc.get_or_add_tcPr()
        for tag in ("a:lnL", "a:lnR", "a:lnT", "a:lnB"):
            for old in props.findall("{http://schemas.openxmlformats.org/drawingml/2006/main}" + tag[2:]):
                props.remove(old)
        ns = "http://schemas.openxmlformats.org/drawingml/2006/main"
        for i, tag in enumerate(("lnL", "lnR", "lnT", "lnB")):
            line = etree.Element(f"{{{ns}}}{tag}", w=str(width), cap="flat", cmpd="sng", algn="ctr")
            fill = etree.SubElement(line, f"{{{ns}}}solidFill")
            etree.SubElement(fill, f"{{{ns}}}srgbClr", val=color.lstrip("#").upper())
            etree.SubElement(line, f"{{{ns}}}prstDash", val="solid")
            props.insert(i, line)  # the lines come before the fill in the schema

    def text(slide: object, x: float, y: float, w: float, h: float, runs: list, size: int, color: str, font: str | None = None, bold: bool = False,
             anchor: object = MSO_ANCHOR.TOP, upper: bool = False) -> object:
        """runs = list of paragraphs; a paragraph is a string (with **bold** / `code`) or (string, level)."""
        frame = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h)).text_frame
        frame.word_wrap = True
        frame.auto_size = MSO_AUTO_SIZE.NONE  # no "shrink on overflow": Canva would import the unscaled size
        frame.vertical_anchor = anchor
        for i, para in enumerate(runs):
            string, level = (para, 0) if isinstance(para, str) else para
            paragraph = frame.paragraphs[0] if i == 0 else frame.add_paragraph()
            paragraph.space_after = Pt(8)
            for part in re.split(r"(\*\*[^*]+\*\*|`[^`]+`)", string):
                if not part:
                    continue
                run = paragraph.add_run()
                code, strong = part.startswith("`"), part.startswith("**")
                run.text = part.strip("`") if code else part.strip("*") if strong else part
                if upper:
                    run.text = run.text.upper()
                run.font.size = Pt(size - (3 if level else 0))
                run.font.name = mono if code else (font or sans)
                run.font.bold = bold or strong
                run.font.color.rgb = rgb(theme["ink"] if strong else (theme["dim"] if level else color))
        return frame

    work = Path(tempfile.mkdtemp(prefix="deck-pptx-"))
    rendered: dict[tuple[str, str], str | None] = {}

    def raster(path: str, colour: str) -> str | None:
        """A file python-pptx can place: PNG / JPEG as they are, an SVG rendered to PNG (currentColor = colour) with
        rsvg-convert, inkscape or magick, WebP / GIF / AVIF through Pillow; None when nothing here renders it."""
        key = (path, colour)
        if key in rendered:
            return rendered[key]
        file = (base / path).resolve()
        target = work / f"p{len(rendered)}.png"
        result: str | None = None
        if file.suffix.lower() in (".png", ".jpg", ".jpeg"):
            result = str(file)
        elif file.suffix.lower() == ".svg":
            markup = svg_inline(file, "r") or file.read_text(encoding="utf-8", errors="replace")
            if "xmlns=" not in markup[:markup.find(">")]:
                markup = markup.replace("<svg", '<svg xmlns="http://www.w3.org/2000/svg"', 1)
            source = work / f"p{len(rendered)}.svg"
            source.write_text(markup.replace("currentColor", colour), encoding="utf-8")
            for tool, command in (("rsvg-convert", ["-w", "600", "-o", str(target), str(source)]),
                                  ("inkscape", [str(source), "--export-type=png", "--export-width=600", f"--export-filename={target}"]),
                                  ("magick", ["-background", "none", "-density", "600", str(source), "-resize", "600x600", str(target)])):
                if shutil.which(tool) and subprocess.run([tool, *command], capture_output=True, check=False).returncode == 0 and target.is_file():
                    result = str(target)
                    break
        else:
            try:
                from PIL import Image
                with Image.open(file) as image:
                    image.convert("RGBA").save(target)
                result = str(target)
            except (ImportError, OSError):
                result = None
        if result is None:
            sys.stderr.write(f"build_deck: pptx: {path} left out (no SVG renderer here: rsvg-convert, inkscape or magick)\n")
        rendered[key] = result
        return result

    def place(slide: object, path: str, x: float, y: float, w: float, h: float, colour: str, cover: bool = False) -> None:
        """A picture inside the box (contain, centered), or covering it (cropped); nothing when it cannot be rendered."""
        file = raster(path, colour)
        if not file:
            return
        pic = slide.shapes.add_picture(file, Inches(x), Inches(y))
        pw, ph = pic.width / 914400, pic.height / 914400
        if cover:
            scale = max(w / pw, h / ph)
            sw, sh = pw * scale, ph * scale
            pic.crop_left = pic.crop_right = (sw - w) / 2 / sw
            pic.crop_top = pic.crop_bottom = (sh - h) / 2 / sh
            pic.left, pic.top, pic.width, pic.height = Inches(x), Inches(y), Inches(w), Inches(h)
            return
        scale = min(w / pw, h / ph)
        pic.width, pic.height = Emu(int(Inches(pw * scale))), Emu(int(Inches(ph * scale)))
        pic.left, pic.top = Emu(int(Inches(x + (w - pw * scale) / 2))), Emu(int(Inches(y + (h - ph * scale) / 2)))

    chapter = ""
    for number, spec in enumerate(deck["slides"], 1):
        kind = spec["kind"]
        chapter = spec.get("chapter", chapter) or deck.get("title", "")
        slide = prs.slides.add_slide(blank)
        slide.background.fill.solid()
        own = slide_bg(spec, theme)
        slide.background.fill.fore_color.rgb = rgb(own[0] if own and re.match(r"^#[0-9a-fA-F]{6}$", own[0]) else theme["bg"])
        upper = theme["head_transform"] == "uppercase"
        title = spec.get("title", "")
        full = kind == "image" and spec.get("full")
        if theme.get("frame", "0") != "0" and not full:
            box(slide, 0.12, 0.12, W - 0.24, H - 0.24, None, theme["ink"])
        if full:
            place(slide, spec["src"], 0, 0, W, H, theme["ink"], cover=True)
            if title:
                tall, wide = 1.05 + (0.5 if spec.get("lead") else 0), min(W - 2 * M, 9.0)
                band = H - 0.7 - tall
                box(slide, M - 0.1, band, wide, tall, theme["paper"], None)
                box(slide, M - 0.1, band, 0.09, tall, theme["ink"])
                text(slide, M + 0.15, band + 0.12, wide - 0.4, 0.8, [title], 34, theme["ink"], head, True, MSO_ANCHOR.TOP, upper)
                if spec.get("lead"):
                    text(slide, M + 0.15, band + 0.95, wide - 0.4, 0.5, [spec["lead"]], 18, theme["dim"])
            if spec.get("credit"):
                text(slide, W - 5.2, H - 0.42, 5.0, 0.3, [spec["credit"]], 10, theme["dim"])
        elif kind in ("title", "end"):
            text(slide, M, 2.0, W - 2 * M, 1.6, [title or "Thank you"], 48, theme["ink"], head, True, MSO_ANCHOR.BOTTOM, upper)
            text(slide, M, 3.8, W - 2 * M, 1.2, [spec.get("subtitle", "")], 24, theme["dim"])
            extra = spec.get("meta", []) if kind == "title" else spec.get("links", [])
            if extra:
                text(slide, M, 5.2, W - 2 * M, 1.6, [plain(e) for e in extra], 16, theme["ink"] if kind == "title" else theme["tx"])
            if kind == "end" and spec.get("src"):
                place(slide, spec["src"], W - 3.2, 4.4, 2.4, 2.4, theme["tx"])
        elif kind == "section":
            text(slide, M, 2.4, 3, 0.6, [str(spec.get("number", ""))], 22, theme["ink2"], mono)
            text(slide, M, 3.0, W - 2 * M, 1.4, [title], 44, theme["ink"], head, True, MSO_ANCHOR.TOP, upper)
            if spec.get("lead"):
                text(slide, M, 4.5, W - 2 * M, 1.0, [spec["lead"]], 22, theme["dim"])
        elif kind == "quote":
            box(slide, M, 2.0, 0.08, 2.6, theme["ink2"])
            text(slide, M + 0.4, 2.0, W - 2 * M - 0.4, 2.6, [spec.get("text", "")], 34, theme["ink"], None, False, MSO_ANCHOR.MIDDLE)
            if spec.get("by"):
                text(slide, M + 0.4, 4.8, W - 2 * M, 0.6, [spec["by"]], 18, theme["dim"])
        else:
            if title:
                text(slide, M, 0.45, W - 2 * M, 1.0, [title], 32, theme["ink"], head, True, MSO_ANCHOR.TOP, upper)
            top = 1.55
            if spec.get("lead"):
                text(slide, M, 1.4, W - 2 * M, 0.7, [spec["lead"]], 18, theme["dim"])
                top = 2.15
            avail = H - top - 0.9
            if kind == "bullets":
                listed = points(spec)
                width = W - 2 * M
                if spec.get("src"): # the picture beside the points
                    width = (W - 2 * M) * 0.55
                    place(slide, spec["src"], M + width + 0.4, top, W - 2 * M - width - 0.4, avail - (0.4 if spec.get("credit") else 0), theme["ink"])
                    if spec.get("credit"):
                        text(slide, M + width + 0.4, top + avail - 0.35, W - 2 * M - width - 0.4, 0.3, [spec["credit"]], 10, theme["dim"])
                if any(icon for _, _, icon in listed): # one row per point: its icon, then its text
                    rows = [1.0 + 0.45 * len(subs) for _, subs, _ in listed]
                    scale = min(1.0, avail / max(0.1, sum(rows)))
                    y = top + max(0.0, (avail - sum(rows) * scale) / 2) # centered under the title, as on the web page
                    for (item, subs, icon), row in zip(listed, rows):
                        if icon:
                            place(slide, icon, M, y + 0.08, 0.72, 0.72, theme["ink"])
                        text(slide, M + 1.05, y + 0.12, width - 1.05, row * scale, [item, *((s, 1) for s in subs)], 26, theme["tx"])
                        y += row * scale
                else:
                    paras: list = []
                    for item, subs, _ in listed:
                        paras.append("• " + item)
                        paras.extend(("– " + s, 1) for s in subs)
                    side = bool(spec.get("src"))
                    text(slide, M, top, width, avail, paras, 26 if side else 22, theme["tx"], None, False, MSO_ANCHOR.MIDDLE if side else MSO_ANCHOR.TOP)
            elif kind == "columns":
                cols = spec.get("columns", [])
                gap = 0.3
                cw = (W - 2 * M - gap * (len(cols) - 1)) / max(1, len(cols))
                lift = 1.1 if any(col.get("icon") for col in cols) else 0
                heights = [min(avail, lift + 0.95 + 0.42 * sum(1 + len(p) // max(1, int(cw * 7.5)) for p in col.get("points", []))) for col in cols]
                y = top + (max(0.0, (avail - max(heights, default=0)) / 2) if lift else 0)
                for i, (col, height) in enumerate(zip(cols, heights)):
                    x = M + i * (cw + gap)
                    box(slide, x, y, cw, height, theme["paper"], theme["ink"])
                    if col.get("icon"):
                        place(slide, col["icon"], x + 0.2, y + 0.2, 0.9, 0.9, theme["ink"])
                    text(slide, x + 0.15, y + 0.1 + lift, cw - 0.3, 0.6, [col.get("head", "")], 20, theme["ink"], head, True)
                    if col.get("points"):
                        text(slide, x + 0.15, y + 0.75 + lift, cw - 0.3, height - 0.85 - lift, ["• " + p for p in col["points"]], 17, theme["tx"])
            elif kind == "grid":
                cells = spec.get("items", [])
                per_row = len(cells) if len(cells) <= 4 else 3 if len(cells) in (5, 6, 9) else 4
                lines = (len(cells) + per_row - 1) // per_row
                cw, ch = (W - 2 * M) / per_row, min(2.5, avail / max(1, lines))
                size = min(1.55, ch - 0.75)
                y0 = top + max(0.0, (avail - lines * ch) / 2)
                for i, cell in enumerate(cells):
                    x, y = M + (i % per_row) * cw, y0 + (i // per_row) * ch
                    place(slide, cell["icon"], x + (cw - size) / 2, y, size, size, theme["ink"])
                    if cell.get("label"):
                        text(slide, x + 0.1, y + size + 0.12, cw - 0.2, 0.6, [cell["label"]], 20, theme["tx"], None, True).paragraphs[0].alignment = PP_ALIGN.CENTER
            elif kind == "big":
                stats = spec.get("items") or [{"value": spec.get("value"), "label": spec.get("label", "")}]
                cw = (W - 2 * M) / len(stats)
                for i, stat in enumerate(stats):
                    frame = text(slide, M + i * cw, top + 0.5, cw, 2.0, [str(stat["value"])], 110 if len(stats) == 1 else 72, theme["ink"], head, True, MSO_ANCHOR.BOTTOM)
                    frame.paragraphs[0].alignment = PP_ALIGN.CENTER
                    if stat.get("label"):
                        text(slide, M + i * cw, top + 2.6, cw, 0.8, [stat["label"]], 22, theme["dim"]).paragraphs[0].alignment = PP_ALIGN.CENTER
            elif kind == "table":
                headers, rows = spec.get("head", []), spec.get("rows", [])
                shape = slide.shapes.add_table(len(rows) + 1, max(1, len(headers)), Inches(M), Inches(top), Inches(W - 2 * M), Inches(min(avail, 0.5 * (len(rows) + 1))))
                for c, name in enumerate(headers):
                    cell = shape.table.cell(0, c)
                    cell.text = plain(name)
                    cell.fill.solid()
                    cell.fill.fore_color.rgb = rgb(theme["ink"])
                    borders(cell, theme["ink"])
                    for run in cell.text_frame.paragraphs[0].runs:
                        run.font.size, run.font.bold, run.font.name = Pt(16), True, sans
                        run.font.color.rgb = rgb(theme["paper"])
                for r, row in enumerate(rows, 1):
                    for c, value in enumerate(row):
                        cell = shape.table.cell(r, c)
                        cell.text = plain(value)
                        cell.fill.solid()
                        cell.fill.fore_color.rgb = rgb(theme["paper"] if r % 2 == 0 else theme["bg"])
                        borders(cell, theme["dim"], 9525)
                        for run in cell.text_frame.paragraphs[0].runs:
                            run.font.size, run.font.name = Pt(15), sans
                            run.font.color.rgb = rgb(theme["tx"])
            elif kind == "code":
                lines = spec.get("code", "").split("\n")
                height = min(avail - 0.5, 0.4 + 0.33 * len(lines))
                box(slide, M, top, W - 2 * M, height, theme["code"], None)
                box(slide, M, top, 0.07, height, theme["ink"])
                text(slide, M + 0.3, top + 0.12, W - 2 * M - 0.5, height - 0.2, lines, 15, theme["codetx"], mono)
                if spec.get("caption"):
                    text(slide, M, top + height + 0.1, W - 2 * M, 0.5, [spec["caption"]], 14, theme["dim"])
            elif kind == "image":
                place(slide, spec["src"], M, top, W - 2 * M, avail - 0.45, theme["ink"])
                under = [s for s in (spec.get("caption"), spec.get("credit")) if s]
                if under:
                    text(slide, M, H - 1.3, W - 2 * M, 0.5, under, 14, theme["dim"]).paragraphs[0].alignment = PP_ALIGN.CENTER
            elif kind == "svg":
                text(slide, M, top, W - 2 * M, 1.0, ["(drawing: see the web page or the PDF)"], 16, theme["dim"])
            elif kind == "steps":
                steps = spec.get("steps", [])
                n = max(1, min(len(steps), 4))
                gap = 0.25
                cw = (W - 2 * M - gap * (n - 1)) / n
                icons = any(step.get("icon") for step in steps)
                y0 = top + (max(0.0, (avail - ((len(steps) + n - 1) // n) * 2.4) / 2) if icons else 0)
                for i, step in enumerate(steps):
                    x, y = M + (i % n) * (cw + gap), y0 + (i // n) * 2.4
                    box(slide, x, y, cw, 2.2, theme["paper"], theme["ink"])
                    if step.get("icon"):
                        place(slide, step["icon"], x + (cw - 0.85) / 2, y + 0.2, 0.85, 0.85, theme["ink"])
                        frame = text(slide, x + 0.12, y + 1.15, cw - 0.24, 0.5, [plain(step.get("head", ""))], 20, theme["ink"], head, True)
                        frame.paragraphs[0].alignment = PP_ALIGN.CENTER
                        if step.get("text"):
                            text(slide, x + 0.12, y + 1.65, cw - 0.24, 0.5, [step["text"]], 14, theme["tx"]).paragraphs[0].alignment = PP_ALIGN.CENTER
                        continue
                    text(slide, x + 0.12, y + 0.08, cw - 0.24, 0.5, [f"{i + 1}. " + plain(step.get("head", ""))], 18, theme["ink"], head, True)
                    if step.get("text"):
                        text(slide, x + 0.12, y + 0.7, cw - 0.24, 1.4, [step["text"]], 15, theme["tx"])
        if kind not in ("title", "section", "end", "quote") and not full:
            text(slide, M, H - 0.55, W - 2 * M, 0.35, [f"{plain(deck.get('title', ''))}  ·  {chapter}  ·  {number}"], 10, theme["dim"])
        if spec.get("notes"):
            slide.notes_slide.notes_text_frame.text = plain(spec["notes"])
    prs.core_properties.title = plain(deck.get("title", ""))
    prs.core_properties.author = deck.get("author", "")
    prs.save(str(out))
    shutil.rmtree(work, ignore_errors=True)

# ======================================================================= notes
def build_notes(deck: dict, out: Path) -> None:
    rows = [f"# {plain(deck.get('title', 'Presentation'))}: speaker notes", "",
            "What is said aloud and is not written on the slides. The numbers are the pages of the PDF and of the web page.", ""]
    for number, slide in enumerate(deck["slides"], 1):
        label = label_of(slide) or slide["kind"]
        rows += [f"## {number}. {label}", "", plain(slide.get("notes", "")) or "—", ""]
    out.write_text("\n".join(rows), encoding="utf-8")

# ======================================================================= main
def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("deck")
    parser.add_argument("-o", "--out", default=None, help="output folder (default: next to deck.json)")
    parser.add_argument("--formats", default="html,pdf,notes", help="html, pdf, pptx, notes (comma separated)")
    parser.add_argument("--theme", default=None)
    parser.add_argument("--theme-file", default=None)
    parser.add_argument("--canva", action="store_true", help="made for an import into Canva: one Canva font per role, no gradient background, nothing a PDF import cuts or merges")
    args = parser.parse_args()
    source = Path(args.deck).expanduser().resolve()
    if not source.is_file():
        sys.exit(f"build_deck: {args.deck!r} not found")
    try:
        deck = json.loads(source.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        sys.exit(f"build_deck: {source.name}: not valid JSON ({error})")
    if not isinstance(deck, dict):
        sys.exit(f"build_deck: {source.name} must be an object with a 'slides' list")
    formats = [f.strip() for f in args.formats.split(",") if f.strip()]
    unknown = [f for f in formats if f not in ("html", "pdf", "pptx", "notes")]
    if unknown or not formats:
        sys.exit(f"build_deck: unknown format {', '.join(unknown) or '(none)'} (known: html, pdf, pptx, notes)")
    check_model(deck)
    name, theme = pick_theme(deck, args)
    if args.canva:
        theme = {**canva_theme(theme), "canva": True}
    out = Path(args.out).resolve() if args.out else source.parent
    out.mkdir(parents=True, exist_ok=True)
    stem = slug(plain(deck.get("title", "deck")))
    failed = []
    for fmt in formats:
        try:
            if fmt == "html":
                build_web(deck, theme, source.parent, out / f"{stem}.html")
            elif fmt == "pdf":
                build_pdf(deck, theme, source.parent, out / f"{stem}.pdf")
            elif fmt == "pptx":
                build_pptx(deck, theme, source.parent, out / f"{stem}.pptx")
            else:
                build_notes(deck, out / f"{stem}-notes.md")
        except MissingTool as error: # the other formats are still built
            failed.append(fmt)
            sys.stderr.write(f"build_deck: {fmt} not built: {error}\n")
            continue
        print(f"{out / (stem + {'notes': '-notes.md'}.get(fmt, '.' + fmt))}  ({len(deck['slides'])} slides, theme {name})")
    if failed:
        sys.exit(1)

if __name__ == "__main__":
    main()
