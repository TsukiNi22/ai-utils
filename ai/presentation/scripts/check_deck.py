#!/usr/bin/env python3
"""Check a presentation: the model, then each built file, and write contact sheets to LOOK at.

    python3 check_deck.py deck.json [--dir OUTDIR] [--sheets SHEETDIR]

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
    parser.add_argument("--sheets", default=None, help="folder of the PNG sheets (default: the same folder)")
    args = parser.parse_args()
    source = Path(args.deck).resolve()
    deck = json.loads(source.read_text(encoding="utf-8"))
    folder = Path(args.dir).resolve() if args.dir else source.parent
    sheets = Path(args.sheets).resolve() if args.sheets else folder
    sheets.mkdir(parents=True, exist_ok=True)
    errors = lint(deck, source.parent)
    stem = re.sub(r"[^a-z0-9]+", "-", re.sub(r"\*\*|`", "", deck.get("title", "deck")).lower()).strip("-") or "deck"
    pdf, pptx, page = folder / f"{stem}.pdf", folder / f"{stem}.pptx", folder / f"{stem}.html"
    if pdf.exists():
        pages = int(re.search(r"Pages:\s+(\d+)", subprocess.run(["pdfinfo", str(pdf)], capture_output=True, text=True, check=True).stdout).group(1))
        print(f"pdf: {pages} pages" + ("" if pages == len(deck["slides"]) else f" ERROR expected {len(deck['slides'])}"))
        errors += pages != len(deck["slides"])
        sheet(pdf, sheets / f"{stem}-pdf.png")
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
