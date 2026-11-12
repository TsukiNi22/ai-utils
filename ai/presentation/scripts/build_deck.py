#!/usr/bin/env python3
"""Build a presentation from one deck.json: an animated web page (one HTML file), a PDF, a PPTX and the oral notes.

    python3 build_deck.py deck.json [-o OUTDIR] [--formats html,pdf,pptx,notes] [--theme NAME | --theme-file FILE] [--canva]

deck.json (see ../templates/example.json):
    {"title", "subtitle", "lang", "author", "date", "duration": minutes, "theme": "blueprint", "slides": [slide, ...]}
slide = {"kind", "title", "chapter", "notes", ...}; kinds: title, section, bullets, columns, table, code, image, svg,
steps, quote, end. Text accepts `code` and **bold**. The HTML page shows the points one by one (build steps), the PDF
and the PPTX show every step at once. Themes: ../templates/themes.json, ~/.claude/presentation/themes/*.json, --theme-file.
"""
import argparse
import base64
import html
import json
import mimetypes
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TEMPLATES = HERE.parent / "templates"
USER_THEMES = Path.home() / ".claude" / "presentation" / "themes"
KINDS = ("title", "section", "bullets", "columns", "table", "code", "image", "svg", "steps", "quote", "end")


# ======================================================================= model
def load_themes() -> dict:
    themes = json.loads((TEMPLATES / "themes.json").read_text(encoding="utf-8"))
    if USER_THEMES.is_dir():
        for file in sorted(USER_THEMES.glob("*.json")):
            themes[file.stem] = json.loads(file.read_text(encoding="utf-8"))
    return themes


def pick_theme(deck: dict, args: argparse.Namespace) -> tuple[str, dict]:
    if args.theme_file:
        file = Path(args.theme_file)
        return file.stem, {**load_themes()["blueprint"], **json.loads(file.read_text(encoding="utf-8"))}
    themes = load_themes()
    name = args.theme or deck.get("theme") or "blueprint"
    if name not in themes:
        sys.exit(f"build_deck: unknown theme {name!r} (known: {', '.join(themes)})")
    return name, {**themes["blueprint"], **themes[name]}


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
        kind = slide.get("kind")
        if kind not in KINDS:
            sys.exit(f"build_deck: slide {index}: unknown kind {kind!r} (known: {', '.join(KINDS)})")
        if kind not in ("quote", "svg", "end") and not slide.get("title"):
            sys.exit(f"build_deck: slide {index} ({kind}) has no title")


def inline(text: str) -> str:
    """Escape, then `code` and **bold**."""
    out = html.escape(str(text), quote=False)
    out = re.sub(r"`([^`]+)`", r"<code>\1</code>", out)
    return re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", out)


def plain(text: str) -> str:
    return re.sub(r"\*\*([^*]+)\*\*|`([^`]+)`", lambda m: m.group(1) or m.group(2), str(text))


def points(slide: dict, key: str = "points") -> list[tuple[str, list[str]]]:
    """A list of "text" or {"text", "sub": [...]} as (text, subs)."""
    out = []
    for item in slide.get(key, []):
        out.append((item, []) if isinstance(item, str) else (item["text"], list(item.get("sub", []))))
    return out


def data_uri(path: str, base: Path) -> str:
    file = (base / path).resolve()
    if not file.is_file():
        sys.exit(f"build_deck: image not found: {file}")
    kind = mimetypes.guess_type(file.name)[0] or "application/octet-stream"
    return f"data:{kind};base64," + base64.b64encode(file.read_bytes()).decode("ascii")


# ======================================================================= html (web page and PDF share it)
def slide_inner(slide: dict, number: int, base: Path) -> str:
    kind, title = slide["kind"], slide.get("title", "")
    head = f"<h2>{inline(title)}</h2>" if title and kind not in ("title", "section", "end") else ""
    lead = f'<p class="lead">{inline(slide["lead"])}</p>' if slide.get("lead") else ""
    if kind == "title":
        meta = "".join(f"<li>{inline(m)}</li>" for m in slide.get("meta", []))
        return (f"<h1>{inline(title)}</h1><p class=\"sub\">{inline(slide.get('subtitle', ''))}</p>"
                + (f'<ul class="meta">{meta}</ul>' if meta else ""))
    if kind == "section":
        return (f'<div class="num">{slide.get("number", "")}</div><h2>{inline(title)}</h2>'
                + (f'<p class="lead">{inline(slide["lead"])}</p>' if slide.get("lead") else ""))
    if kind == "bullets":
        items = "".join(
            f'<li data-s="{i}">{inline(text)}' + (f"<ul>{''.join(f'<li>{inline(s)}</li>' for s in subs)}</ul>" if subs else "") + "</li>"
            for i, (text, subs) in enumerate(points(slide), 1))
        return f"{head}{lead}<ul>{items}</ul>"
    if kind == "columns":
        cols = slide.get("columns", [])
        body = "".join(
            f'<div class="col" data-s="{i}"><h3>{inline(c.get("head", ""))}</h3><ul>'
            + "".join(f"<li>{inline(p)}</li>" for p in c.get("points", [])) + "</ul></div>" for i, c in enumerate(cols, 1))
        return f'{head}{lead}<div class="cols" style="--n:{len(cols)}">{body}</div>'
    if kind == "table":
        thead = "".join(f"<th scope=\"col\">{inline(h)}</th>" for h in slide.get("head", []))
        rows = "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in row) + "</tr>" for row in slide.get("rows", []))
        return f"{head}{lead}<table><thead><tr>{thead}</tr></thead><tbody>{rows}</tbody></table>"
    if kind == "code":
        caption = f'<p class="caption">{inline(slide["caption"])}</p>' if slide.get("caption") else ""
        return f'{head}{lead}<pre><code>{html.escape(slide.get("code", ""))}</code></pre>{caption}'
    if kind == "image":
        alt = html.escape(slide.get("alt") or plain(title))
        caption = f"<figcaption>{inline(slide['caption'])}</figcaption>" if slide.get("caption") else ""
        return f'{head}{lead}<figure><img src="{data_uri(slide["src"], base)}" alt="{alt}">{caption}</figure>'
    if kind == "svg":
        caption = f"<figcaption>{inline(slide['caption'])}</figcaption>" if slide.get("caption") else ""
        return f'{head}{lead}<figure>{slide.get("svg", "")}{caption}</figure>'
    if kind == "steps":
        steps = slide.get("steps", [])
        items = "".join(f'<li data-s="{i}"><h3>{inline(s.get("head", ""))}</h3><p>{inline(s.get("text", ""))}</p></li>' for i, s in enumerate(steps, 1))
        return f'{head}{lead}<ol class="flow" style="--n:{min(len(steps), 4)}">{items}</ol>'
    if kind == "quote":
        who = f"<cite>{inline(slide['by'])}</cite>" if slide.get("by") else ""
        return f"<blockquote>{inline(slide.get('text', ''))}</blockquote>{who}"
    links = "".join(f"<li>{inline(link)}</li>" for link in slide.get("links", []))  # end
    image = f'<figure><img src="{data_uri(slide["src"], base)}" alt="{html.escape(slide.get("alt", ""))}" style="max-height:260px"></figure>' if slide.get("src") else ""
    return f'<h1>{inline(title or "Thank you")}</h1><p class="sub">{inline(slide.get("subtitle", ""))}</p><ul>{links}</ul>{image}'


def slides_html(deck: dict, base: Path, with_notes: bool) -> str:
    out = []
    chapter = ""
    for number, slide in enumerate(deck["slides"], 1):
        chapter = slide.get("chapter", chapter) or deck.get("title", "")
        title = plain(slide.get("title") or slide.get("text") or deck.get("title", ""))
        notes = f'<div class="notes">{inline(slide["notes"])}</div>' if with_notes and slide.get("notes") else ""
        foot = f'<div class="foot"><span>{html.escape(plain(deck.get("title", "")))}</span><span>{html.escape(chapter)} · {number}</span></div>'
        out.append(f'<section class="slide k-{slide["kind"]}" id="s{number}" data-ch="{html.escape(chapter)}" '
                   f'data-title="{html.escape(title)}">{slide_inner(slide, number, base)}{foot}{notes}</section>')
    return "\n".join(out)


def theme_css(theme: dict) -> str:
    keys = ("bg", "paper", "ink", "ink2", "tx", "dim", "code", "codetx", "warn", "ok", "font", "head_font", "mono",
            "head_transform", "head_spacing", "head_weight", "radius", "bgimg", "frame", "inner_frame")
    rows = "; ".join(f"--{k.replace('_', '-')}: {theme[k]}" for k in keys)
    return f":root {{ {rows}; }}"


SKELETON = """<div id="stage"><main id="deck" class="{cls}">
{slides}
</main>
<div class="zone" id="zl" aria-hidden="true"></div><div class="zone" id="zr" aria-hidden="true"></div>
<div id="hud"><nav id="chapters" aria-label="Chapters"></nav><div id="bar"><i></i></div><div id="steps" aria-hidden="true"></div><span id="count"></span><div id="btns"><button type="button" id="prev" aria-label="Previous (left arrow)">&#8249;</button><button type="button" id="ovbtn" aria-label="Chapter map (O)">&#9638;</button><button type="button" id="fsbtn" aria-label="Full screen (F)">&#9974;</button><button type="button" id="next" aria-label="Next (right arrow)">&#8250;</button></div></div>
</div>
<div id="map" hidden></div>"""


def build_web(deck: dict, theme: dict, base: Path, out: Path) -> None:
    css = (TEMPLATES / "deck-base.css").read_text(encoding="utf-8") + "\n" + theme_css(theme) + "\n" + (TEMPLATES / "slides.css").read_text(encoding="utf-8")
    js = (TEMPLATES / "deck.js").read_text(encoding="utf-8")
    body = SKELETON.format(slides=slides_html(deck, base, True), cls="cartouche" if theme.get("cartouche") else "")
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
        sys.exit("build_deck: the PDF needs weasyprint (sudo dnf install python3-weasyprint, or pip install --user weasyprint)")
    css = theme_css(theme) + "\n" + (TEMPLATES / "slides.css").read_text(encoding="utf-8")
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
        return block if re.match(r"<svg[^>]*\scolor=", block) else block.replace("<svg", f'<svg color="{theme["tx"]}"', 1)
    return re.sub(r"<svg\b.*?</svg>", resolve, page, flags=re.S)


# ======================================================================= pptx
def rgb(value: str):
    from pptx.dml.color import RGBColor
    return RGBColor.from_string(value.lstrip("#").upper())


def first_font(stack: str) -> str:
    return stack.split(",")[0].strip().strip("'\"")


def build_pptx(deck: dict, theme: dict, base: Path, out: Path) -> None:
    try:
        from pptx import Presentation
        from pptx.enum.shapes import MSO_SHAPE
        from pptx.enum.text import MSO_ANCHOR, MSO_AUTO_SIZE
        from pptx.util import Emu, Inches, Pt
    except ImportError:
        sys.exit("build_deck: the PPTX needs python-pptx (pip install --user python-pptx, or sudo dnf install python3-python-pptx)")
    prs = Presentation()
    prs.slide_width, prs.slide_height = Inches(13.333), Inches(7.5)
    blank = prs.slide_layouts[6]
    sans, mono, head = first_font(theme["font"]), first_font(theme["mono"]), first_font(theme["head_font"])
    W, H, M = 13.333, 7.5, 0.7

    def box(slide, x, y, w, h, fill=None, line=None, shape=MSO_SHAPE.RECTANGLE):
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

    def borders(cell, color, width=12700):
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

    def text(slide, x, y, w, h, runs, size, color, font=None, bold=False, anchor=MSO_ANCHOR.TOP, upper=False):
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

    chapter = ""
    for number, spec in enumerate(deck["slides"], 1):
        kind = spec["kind"]
        chapter = spec.get("chapter", chapter) or deck.get("title", "")
        slide = prs.slides.add_slide(blank)
        slide.background.fill.solid()
        slide.background.fill.fore_color.rgb = rgb(theme["bg"])
        upper = theme["head_transform"] == "uppercase"
        title = spec.get("title", "")
        if theme.get("frame", "0") != "0":
            box(slide, 0.12, 0.12, W - 0.24, H - 0.24, None, theme["ink"])
        if kind in ("title", "end"):
            text(slide, M, 2.0, W - 2 * M, 1.6, [title or "Thank you"], 48, theme["ink"], head, True, MSO_ANCHOR.BOTTOM, upper)
            text(slide, M, 3.8, W - 2 * M, 1.2, [spec.get("subtitle", "")], 24, theme["dim"])
            extra = spec.get("meta", []) if kind == "title" else spec.get("links", [])
            if extra:
                text(slide, M, 5.2, W - 2 * M, 1.6, [plain(e) for e in extra], 16, theme["ink"] if kind == "title" else theme["tx"])
            if kind == "end" and spec.get("src"):
                slide.shapes.add_picture(str((base / spec["src"]).resolve()), Inches(W - 3.2), Inches(4.4), height=Inches(2.4))
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
            text(slide, M, 0.45, W - 2 * M, 1.0, [title], 32, theme["ink"], head, True, MSO_ANCHOR.TOP, upper)
            top = 1.55
            if spec.get("lead"):
                text(slide, M, 1.4, W - 2 * M, 0.7, [spec["lead"]], 18, theme["dim"])
                top = 2.15
            avail = H - top - 0.9
            if kind == "bullets":
                paras: list = []
                for item, subs in points(spec):
                    paras.append("• " + item)
                    paras.extend(("– " + s, 1) for s in subs)
                text(slide, M, top, W - 2 * M, avail, paras, 22, theme["tx"])
            elif kind == "columns":
                cols = spec.get("columns", [])
                gap = 0.3
                cw = (W - 2 * M - gap * (len(cols) - 1)) / max(1, len(cols))
                for i, col in enumerate(cols):
                    x = M + i * (cw + gap)
                    height = min(avail, 0.95 + 0.42 * sum(1 + len(p) // max(1, int(cw * 7.5)) for p in col.get("points", [])))
                    box(slide, x, top, cw, height, theme["paper"], theme["ink"])
                    text(slide, x + 0.15, top + 0.1, cw - 0.3, 0.6, [col.get("head", "")], 20, theme["ink"], head, True)
                    text(slide, x + 0.15, top + 0.75, cw - 0.3, height - 0.85, ["• " + p for p in col.get("points", [])], 17, theme["tx"])
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
                if spec.get("src"):
                    pic = slide.shapes.add_picture(str((base / spec["src"]).resolve()), Inches(M), Inches(top))
                    scale = min((W - 2 * M) / (pic.width / 914400), (avail - 0.4) / (pic.height / 914400))
                    pic.width, pic.height = Emu(int(pic.width * scale)), Emu(int(pic.height * scale))
                    pic.left = Emu(int((prs.slide_width - pic.width) / 2))
                if spec.get("caption"):
                    text(slide, M, H - 1.3, W - 2 * M, 0.5, [spec["caption"]], 14, theme["dim"])
            elif kind == "svg":
                text(slide, M, top, W - 2 * M, 1.0, ["(drawing: see the web page or the PDF)"], 16, theme["dim"])
            elif kind == "steps":
                steps = spec.get("steps", [])
                n = max(1, min(len(steps), 4))
                gap = 0.25
                cw = (W - 2 * M - gap * (n - 1)) / n
                for i, step in enumerate(steps):
                    x, y = M + (i % n) * (cw + gap), top + (i // n) * 2.4
                    box(slide, x, y, cw, 2.2, theme["paper"], theme["ink"])
                    text(slide, x + 0.12, y + 0.08, cw - 0.24, 0.5, [f"{i + 1}. " + plain(step.get("head", ""))], 18, theme["ink"], head, True)
                    text(slide, x + 0.12, y + 0.7, cw - 0.24, 1.4, [step.get("text", "")], 15, theme["tx"])
        if kind not in ("title", "section", "end", "quote"):
            text(slide, M, H - 0.55, W - 2 * M, 0.35, [f"{plain(deck.get('title', ''))}  ·  {chapter}  ·  {number}"], 10, theme["dim"])
        if spec.get("notes"):
            slide.notes_slide.notes_text_frame.text = plain(spec["notes"])
    prs.core_properties.title = plain(deck.get("title", ""))
    prs.core_properties.author = deck.get("author", "")
    prs.save(str(out))


# ======================================================================= notes
def build_notes(deck: dict, out: Path) -> None:
    rows = [f"# {plain(deck.get('title', 'Presentation'))}: speaker notes", "",
            "What is said aloud and is not written on the slides. The numbers are the pages of the PDF and of the web page.", ""]
    for number, slide in enumerate(deck["slides"], 1):
        label = plain(slide.get("title") or slide.get("text") or slide["kind"])
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
    source = Path(args.deck).resolve()
    deck = json.loads(source.read_text(encoding="utf-8"))
    check_model(deck)
    name, theme = pick_theme(deck, args)
    if args.canva:
        theme = {**canva_theme(theme), "canva": True}
    out = Path(args.out).resolve() if args.out else source.parent
    out.mkdir(parents=True, exist_ok=True)
    stem = re.sub(r"[^a-z0-9]+", "-", plain(deck.get("title", "deck")).lower()).strip("-") or "deck"
    for fmt in (f.strip() for f in args.formats.split(",") if f.strip()):
        if fmt == "html":
            build_web(deck, theme, source.parent, out / f"{stem}.html")
        elif fmt == "pdf":
            build_pdf(deck, theme, source.parent, out / f"{stem}.pdf")
        elif fmt == "pptx":
            build_pptx(deck, theme, source.parent, out / f"{stem}.pptx")
        elif fmt == "notes":
            build_notes(deck, out / f"{stem}-notes.md")
        else:
            sys.exit(f"build_deck: unknown format {fmt!r}")
        print(f"{out / (stem + {'notes': '-notes.md'}.get(fmt, '.' + fmt))}  ({len(deck['slides'])} slides, theme {name})")


if __name__ == "__main__":
    main()
