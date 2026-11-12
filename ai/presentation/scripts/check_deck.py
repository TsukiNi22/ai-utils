#!/usr/bin/env python3
"""Check a presentation: the model, then each built file, and write contact sheets to LOOK at.

    python3 check_deck.py deck.json [--dir OUTDIR] [--sheets SHEETDIR] [--canva]

- deck.json: slides per minute (the pace), points per slide, words per slide, missing notes / titles / images.
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
from pathlib import Path

PER_MINUTE = (0.5, 1.0)  # content slides per minute: below = slow, above = rushed
MAX_POINTS, MAX_WORDS = 6, 60


def words(slide: dict) -> int:
    text = json.dumps({k: v for k, v in slide.items() if k not in ("notes", "svg", "kind", "chapter")}, ensure_ascii=False)
    return len(re.findall(r"[\w'-]+", re.sub(r'"[a-z_]+":', " ", text)))


def lint(deck: dict, base: Path) -> int:
    errors = 0
    slides = deck["slides"]
    content = [s for s in slides if s["kind"] not in ("title", "section", "end")]
    print(f"slides: {len(slides)} ({len(content)} of content)")
    minutes = deck.get("duration")
    if minutes:
        rate = len(content) / minutes
        verdict = "ok" if PER_MINUTE[0] <= rate <= PER_MINUTE[1] else ("too few slides for the time" if rate < PER_MINUTE[0] else "too many slides for the time")
        print(f"pace: {len(content)} content slides in {minutes} min = {rate:.2f} per minute -> {verdict}")
    for number, slide in enumerate(slides, 1):
        where = f"slide {number} ({slide['kind']} {slide.get('title', '')[:30]!r})"
        count = len(slide.get("points", [])) + len(slide.get("columns", [])) + len(slide.get("steps", []))
        if count > MAX_POINTS:
            print(f"WARN {where}: {count} points, split it")
        if words(slide) > MAX_WORDS and slide["kind"] in ("bullets", "columns", "steps"):
            print(f"WARN {where}: {words(slide)} words, move the detail to the notes")
        if slide["kind"] not in ("title", "section", "end", "quote") and not slide.get("notes"):
            print(f"WARN {where}: no notes (what is said aloud)")
        if slide["kind"] in ("image", "end") and slide.get("src") and not (base / slide["src"]).is_file():
            print(f"ERROR {where}: image {slide['src']} not found")
            errors += 1
    return errors


def squash(text: str) -> str:
    return re.sub(r"[^\w]+", "", re.sub(r"\*\*|`", "", text)).lower()


def strings_of(slide: dict) -> list[str]:
    """Every visible string of a slide (what must be found again in the built files)."""
    found: list[str] = []

    def walk(value) -> None:
        if isinstance(value, str):
            found.append(value)
        elif isinstance(value, list):
            for item in value:
                walk(item)
        elif isinstance(value, dict):
            for key, item in value.items():
                if key not in ("notes", "svg", "kind", "chapter", "src", "alt", "lang"):
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
        if slide["kind"] not in ("image", "end") and per_page.get(number):
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
    source = Path(args.deck).resolve()
    deck = json.loads(source.read_text(encoding="utf-8"))
    folder = Path(args.dir).resolve() if args.dir else source.parent
    sheets = Path(args.sheets).resolve() if args.sheets else folder
    sheets.mkdir(parents=True, exist_ok=True)
    errors = lint(deck, source.parent)
    allowed = json.loads((Path(__file__).resolve().parent.parent / "templates" / "canva.json").read_text(encoding="utf-8"))["fonts"]
    stem = re.sub(r"[^a-z0-9]+", "-", re.sub(r"\*\*|`", "", deck.get("title", "deck")).lower()).strip("-") or "deck"
    pdf, pptx, page = folder / f"{stem}.pdf", folder / f"{stem}.pptx", folder / f"{stem}.html"
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
