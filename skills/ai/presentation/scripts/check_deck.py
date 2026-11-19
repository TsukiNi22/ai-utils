#!/usr/bin/env python3
"""Check a presentation: the model, then each built file, and write contact sheets to LOOK at.

    python3 check_deck.py deck.json [--dir OUTDIR] [--sheets SHEETDIR] [--canva]

- deck.json: slides per minute (the pace), then little text: points per slide, words per point / slide / title / lead,
  sub-points, a picture on most slides (never text alone twice in a row), missing notes / pictures.
- <name>.pdf: pages = slides, then a contact sheet PNG (pdftoppm + Pillow) of every page.
- <name>.pptx: rendered to PDF with LibreOffice (soffice) when installed, then a contact sheet too.
- <name>.html: the built-in selftest of the web page (?selftest) in headless Firefox: a screenshot with
  "SELFTEST OK/FAIL" (text out of the slide, hidden by the bar, clipped, keys), to be read.
Exit 1 when the model has an error. The sheets are images: open them (Read) and look; no tool sees a bad layout for you.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unicodedata
from pathlib import Path

PER_MINUTE = (0.5, 1.0)  # content slides per minute: below = slow, above = rushed
MAX_POINTS, MAX_ITEMS, MAX_WORDS = 5, 8, 40  # points / columns / steps of a slide, logos of a grid, words on a content slide
POINT_WORDS, LINE_WORDS, TITLE_WORDS = 7, 12, 9  # a point = a few words; a lead / caption / step text / label = one short line
NOT_TEXT = ("notes", "svg", "kind", "chapter", "bg", "icon", "src", "alt", "full", "credit", "lang", "code")  # not read on the slide
STATEMENT = ("image", "svg", "table", "code", "grid", "big", "quote")  # kinds that are a visual (or a statement) by themselves

def slug(text: str) -> str:
    """The file name build_deck.py gives the outputs (same rule: accents dropped, then non letters as '-')."""
    ascii_text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"[^a-z0-9]+", "-", ascii_text.lower()).strip("-") or "deck"

def count(text: object) -> int:
    return len(re.findall(r"[\w'-]+", re.sub(r"\*\*|`", "", str(text))))

def visible(value: object, skip: tuple = NOT_TEXT) -> list[str]:
    """Every text a slide shows (its notes, pictures and settings left out)."""
    if isinstance(value, (str, int, float)) and not isinstance(value, bool):
        return [str(value)]
    if isinstance(value, list):
        return [text for item in value for text in visible(item, skip)]
    if isinstance(value, dict):
        return [text for key, item in value.items() if key not in skip for text in visible(item, skip)]
    return []

def words(slide: dict) -> int:
    return sum(count(text) for text in visible(slide))

def pictures(slide: dict) -> list[str]:
    """The files a slide uses: its src and the icons of its points, columns, steps and grid items."""
    parts = [p for key in ("points", "columns", "steps", "items") for p in slide.get(key, []) if isinstance(p, dict)]
    return [path for path in [slide.get("src", ""), *(p.get("icon", "") for p in parts)] if path]

def has_visual(slide: dict) -> bool:
    return slide["kind"] in STATEMENT or bool(pictures(slide))

def short(where: str, what: str, text: object, limit: int, advice: str) -> None:
    if count(text) > limit:
        print(f"WARN {where}: {what} {str(text)[:40]!r} has {count(text)} words: {advice}")

def lint(deck: dict, base: Path) -> int:
    """The pace, then the rules of a deck with little text: a few words per point, one short line for a lead or a
    caption, a picture on most slides (never two slides of text alone in a row), notes for what is said."""
    errors = 0
    slides = deck["slides"]
    content = [s for s in slides if s["kind"] not in ("title", "section", "end")]
    print(f"slides: {len(slides)} ({len(content)} of content)")
    minutes = deck.get("duration")
    if minutes:
        rate = len(content) / minutes
        verdict = "ok" if PER_MINUTE[0] <= rate <= PER_MINUTE[1] else ("too few slides for the time" if rate < PER_MINUTE[0] else "too many slides for the time")
        print(f"pace: {len(content)} content slides in {minutes} min = {rate:.2f} per minute -> {verdict}")
    run: list[int] = []  # content slides of text alone, one after the other
    for number, slide in enumerate(slides, 1):
        kind = slide["kind"]
        where = f"slide {number} ({kind} {str(slide.get('title', slide.get('value', '')))[:30]!r})"
        parts = len(slide.get("points", [])) + len(slide.get("columns", [])) + len(slide.get("steps", []))
        if parts > MAX_POINTS:
            print(f"WARN {where}: {parts} points, split it (3 or 4 read best)")
        if kind == "grid" and len(slide.get("items", [])) > MAX_ITEMS:
            print(f"WARN {where}: {len(slide['items'])} logos, keep the {MAX_ITEMS} that matter")
        if kind in ("bullets", "columns", "steps", "grid", "big", "image") and words(slide) > MAX_WORDS:
            print(f"WARN {where}: {words(slide)} words on the slide: keep the key words, the sentences go to the notes")
        if kind not in ("title", "section", "end"):
            short(where, "the title", slide.get("title", ""), TITLE_WORDS, "the idea in a few words")
        for key in ("lead", "caption", "label"):
            short(where, f"the {key}", slide.get(key, ""), LINE_WORDS, "one short line or nothing (the notes say it)")
        subs = 0
        for point in slide.get("points", []):
            text = point if isinstance(point, str) else point.get("text", "")
            short(where, "a point", text, POINT_WORDS, "a few words, the sentence goes to the notes")
            subs += 0 if isinstance(point, str) else len(point.get("sub", []))
        for column in slide.get("columns", []):
            for text in column.get("points", []) if isinstance(column, dict) else []:
                short(where, "a point", text, POINT_WORDS, "a few words, the sentence goes to the notes")
        for item in slide.get("steps", []) + slide.get("items", []):
            if isinstance(item, dict):
                short(where, "a text", item.get("text", item.get("label", "")), LINE_WORDS, "one short line or nothing")
        if subs > 2:
            print(f"WARN {where}: {subs} sub-points: the detail goes to the notes unless the slide is wrong without it")
        if kind not in ("title", "section", "end"):
            run = [] if has_visual(slide) else run + [number]
            if len(run) == 2:
                print(f"WARN slides {run[0]}-{run[1]}: text alone twice in a row: give one an icon per point, a picture (src) or a diagram")
        if kind not in ("title", "section", "end", "quote") and not slide.get("notes"):
            print(f"WARN {where}: no notes (what is said aloud)")
        svg = slide.get("svg", "")
        if re.search(r"<text\b[^>]*\sopacity=", svg):
            print(f"WARN {where}: opacity on an SVG <text>: the PDF engine cuts or shifts the text, give it a dim fill instead")
        if re.search(r"<[a-zA-Z][^<>]*\sstyle=\"[^\"]*\"[^<>]*\sstyle=", svg):
            print(f"WARN {where}: two style attributes on one SVG tag (build_deck merges them, merge them in deck.json)")
        for path in pictures(slide):
            if not (base / path).is_file():
                print(f"ERROR {where}: picture {path} not found")
                errors += 1
    with_visual = sum(1 for s in content if has_visual(s))
    print(f"visuals: {with_visual} of {len(content)} content slides (pictures, icons, logos, diagrams, tables, code, numbers)")
    return errors

def squash(text: str) -> str:
    return re.sub(r"[^\w]+", "", re.sub(r"\*\*|`", "", text)).lower()

def strings_of(slide: dict) -> list[str]:
    """Every visible string of a slide (what must be found again in the built files)."""
    found: list[str] = []

    def walk(value: object) -> None:
        if isinstance(value, str):
            found.append(value)
        elif isinstance(value, list):
            for item in value:
                walk(item)
        elif isinstance(value, dict):
            for key, item in value.items():
                if key not in ("notes", "svg", "kind", "chapter", "src", "alt", "lang", "icon"):
                    walk(item)

    walk({k: v for k, v in slide.items() if k != "notes"})
    return [t for t in found if squash(t)]

def canva_pdf(deck: dict, pdf: Path, allowed: list[str]) -> int:
    """What a PDF import (Canva) needs: real text on each page, every font embedded and in its library, no font made of
    shapes, a page not flattened to a picture. Returns the number of errors."""
    errors = 0
    rows = subprocess.run(["pdffonts", str(pdf)], capture_output=True, text=True, check=True).stdout.splitlines()[2:]
    families = set()
    for row in rows:
        parts = row.split()
        name = re.sub(r"^[A-Z]{6}\+", "", parts[0])
        family = re.sub(r"-(Bold|Italic|Regular|Medium|Light|Semi\w*|Extra\w*|Condensed)+.*$", "", name).replace("-", " ")
        families.add(family)
        if "Type 3" in row:
            print(f"ERROR canva pdf: font {name} is Type 3 (letters drawn as shapes: Canva imports them as pictures)")
            errors += 1
        if parts[-5] == "no":  # the "emb" column
            print(f"ERROR canva pdf: font {name} not embedded (Canva replaces it)")
            errors += 1
    for family in sorted(families):
        if family not in allowed:
            print(f"WARN canva pdf: font {family!r} is not in canva.json (replaced by Canva on import, the lines may change)")
    print(f"canva pdf: {len(rows)} font faces, families {sorted(families)}")
    pages = subprocess.run(["pdftotext", str(pdf), "-"], capture_output=True, text=True, check=True).stdout.split("\f")
    images = subprocess.run(["pdfimages", "-list", str(pdf)], capture_output=True, text=True, check=False).stdout.splitlines()[2:]
    per_page = {}
    for row in images:
        per_page[int(row.split()[0])] = per_page.get(int(row.split()[0]), 0) + 1
    for number, slide in enumerate(deck["slides"], 1):
        page = squash(pages[number - 1]) if number <= len(pages) else ""
        missing = [t for t in strings_of(slide) if squash(t) not in page]
        if missing:
            print(f"ERROR canva pdf: page {number} misses text {missing[0][:50]!r}" + (f" (+{len(missing) - 1})" if len(missing) > 1 else "") + " (not real text, or cut in pieces)")
            errors += 1
        if slide["kind"] not in ("image", "end") and not pictures(slide) and per_page.get(number):
            print(f"WARN canva pdf: page {number} holds {per_page[number]} picture(s) (a background or text turned into an image)")
    return errors

def canva_pptx(deck: dict, pptx: Path, allowed: list[str]) -> int:
    """What a PPTX import needs: every text in a Canva font, no group / placeholder / empty box, no shrink-on-overflow,
    the text of each slide present in its shapes."""
    try:
        from pptx import Presentation
        from pptx.enum.shapes import MSO_SHAPE_TYPE
        from pptx.enum.text import MSO_AUTO_SIZE
    except ImportError:
        print("SKIP canva pptx: python-pptx not installed")
        return 0
    errors = 0
    fonts: set[str] = set()
    for number, (spec, slide) in enumerate(zip(deck["slides"], Presentation(str(pptx)).slides), 1):
        have, body = "", ""
        for shape in slide.shapes:
            if shape.shape_type == MSO_SHAPE_TYPE.GROUP:
                print(f"ERROR canva pptx: slide {number} has a group (Canva imports it as one flat picture)")
                errors += 1
            if shape.is_placeholder:
                print(f"WARN canva pptx: slide {number} has a placeholder ({shape.name})")
            frames = []
            if shape.has_text_frame:
                frames.append(shape.text_frame)
            if getattr(shape, "has_table", False) and shape.has_table:
                frames += [cell.text_frame for row in shape.table.rows for cell in row.cells]
            for frame in frames:
                if shape.has_text_frame and frame.auto_size not in (None, MSO_AUTO_SIZE.NONE):
                    print(f"WARN canva pptx: slide {number} {shape.name}: auto-fit is on (the size Canva reads differs)")
                for paragraph in frame.paragraphs:
                    for run in paragraph.runs:
                        body += run.text
                        if run.font.name:
                            fonts.add(run.font.name)
                        else:
                            print(f"WARN canva pptx: slide {number}: a text run without a font name (Canva takes its default)")
        have = squash(body)
        missing = [t for t in strings_of(spec) if squash(t) not in have and spec["kind"] != "svg"]
        if missing:
            print(f"ERROR canva pptx: slide {number} misses text {missing[0][:50]!r}")
            errors += 1
    for family in sorted(fonts - set(allowed)):
        print(f"WARN canva pptx: font {family!r} is not in canva.json (replaced by Canva on import)")
    print(f"canva pptx: fonts {sorted(fonts)}")
    return errors

def sheet(pdf: Path, out: Path) -> None:
    from PIL import Image
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(["pdftoppm", "-r", "30", "-png", str(pdf), f"{tmp}/p"], check=True)
        pages = [Image.open(f) for f in sorted(Path(tmp).glob("p-*.png"))]
        if not pages:
            print(f"ERROR no page in {pdf}")
            return
        w, h = pages[0].size
        cols = 3
        rows = (len(pages) + cols - 1) // cols
        canvas = Image.new("RGB", (cols * (w + 6) + 6, rows * (h + 6) + 6), "#888888")
        for i, page in enumerate(pages):
            canvas.paste(page.convert("RGB"), (6 + (i % cols) * (w + 6), 6 + (i // cols) * (h + 6)))
        canvas.save(out)
        print(f"sheet: {out} ({len(pages)} pages)")

def selftest(page: Path, out: Path) -> None:
    firefox = shutil.which("firefox")
    if not firefox:
        print("SKIP selftest: firefox not installed")
        return
    with tempfile.TemporaryDirectory() as profile:
        subprocess.run(["timeout", "90", firefox, "--headless", "--no-remote", "--profile", profile, "--window-size", "1280,720",
                        "--screenshot", str(out), f"file://{page}?selftest"], capture_output=True, check=False)
    print(f"selftest screenshot: {out} (read the first line: SELFTEST OK or FAIL and the list)" if out.exists() else "ERROR selftest screenshot not written")

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("deck")
    parser.add_argument("--dir", default=None, help="folder of the built files (default: next to deck.json)")
    parser.add_argument("--canva", action="store_true", help="also check the PDF / PPTX for an import into Canva (text kept, fonts embedded and in its library)")
    parser.add_argument("--sheets", default=None, help="folder of the PNG sheets (default: the same folder)")
    args = parser.parse_args()
    source = Path(args.deck).expanduser().resolve()
    if not source.is_file():
        sys.exit(f"check_deck: {args.deck!r} not found")
    try:
        deck = json.loads(source.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        sys.exit(f"check_deck: {source.name}: not valid JSON ({error})")
    if not isinstance(deck, dict) or not isinstance(deck.get("slides"), list) or not all(isinstance(s, dict) and s.get("kind") for s in deck["slides"]):
        sys.exit(f"check_deck: {source.name} needs a 'slides' list of objects with a 'kind' (build_deck.py says what is wrong)")
    folder = Path(args.dir).resolve() if args.dir else source.parent
    sheets = Path(args.sheets).resolve() if args.sheets else folder
    sheets.mkdir(parents=True, exist_ok=True)
    errors = lint(deck, source.parent)
    allowed = json.loads((Path(__file__).resolve().parent.parent / "templates" / "canva.json").read_text(encoding="utf-8"))["fonts"]
    stem = slug(re.sub(r"\*\*|`", "", deck.get("title", "deck")))
    pdf, pptx, page = folder / f"{stem}.pdf", folder / f"{stem}.pptx", folder / f"{stem}.html"
    if not any(f.exists() for f in (pdf, pptx, page)):
        print(f"WARN no {stem}.pdf / .pptx / .html in {folder}: nothing built there to check (build_deck.py first, same -o)")
    if pdf.exists():
        pages = int(re.search(r"Pages:\s+(\d+)", subprocess.run(["pdfinfo", str(pdf)], capture_output=True, text=True, check=True).stdout).group(1))
        print(f"pdf: {pages} pages" + ("" if pages == len(deck["slides"]) else f" ERROR expected {len(deck['slides'])}"))
        errors += pages != len(deck["slides"])
        sheet(pdf, sheets / f"{stem}-pdf.png")
        if args.canva:
            errors += canva_pdf(deck, pdf, allowed)
    if pptx.exists() and args.canva:
        errors += canva_pptx(deck, pptx, allowed)
    if pptx.exists():
        soffice = shutil.which("soffice")
        if soffice:
            with tempfile.TemporaryDirectory() as tmp:
                subprocess.run([soffice, "--headless", "--convert-to", "pdf", "--outdir", tmp, str(pptx)], capture_output=True, check=False)
                if (Path(tmp) / f"{stem}.pdf").exists():
                    sheet(Path(tmp) / f"{stem}.pdf", sheets / f"{stem}-pptx.png")
        else:
            print("SKIP pptx render: soffice (LibreOffice) not installed")
    if page.exists():
        selftest(page, sheets / f"{stem}-selftest.png")
    sys.exit(1 if errors else 0)

if __name__ == "__main__":
    main()
