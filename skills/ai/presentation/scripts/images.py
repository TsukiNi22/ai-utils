#!/usr/bin/env python3
"""Pictures for a deck: logos and icons by name, photos found on the web, each one checked and credited.

    python3 images.py icon si:cmake si:github lucide:rocket tabler:git-merge [-o img] [--brand] [--force]
    python3 images.py search "rocket launch" [--commercial] [-n 12] [--sheet /tmp/pick.png]
    python3 images.py get openverse:<id> | <https url> [-o img] [--name NAME] [--license L --author A --page URL]

icon: an SVG of an icon set served by jsDelivr (the version is resolved once and written in the credits):
    si:<slug>      Simple Icons, CC0 (https://simpleicons.org): brand logos in one colour (the theme colour on the slide,
                   --brand: the official colour); the slug is the lower-case name, + = plus, . = dot (cplusplus, nodedotjs)
    lucide:<name>  Lucide, ISC (https://lucide.dev/icons): line icons for ideas (rocket, shield-check, folder-tree...)
    tabler:<name>  Tabler outline, MIT (https://tabler.io/icons): the same when Lucide has none
search: Openverse (openly licensed photos of Flickr, Wikimedia Commons, museums...): id, licence, size, author, title;
    --commercial leaves out the non commercial licences, --sheet writes the thumbnails numbered on one PNG to look at.
get: downloads a picture, checks that it is one (PNG / JPEG / SVG; WebP, GIF, AVIF converted), makes a big one smaller,
    and writes who made it and its licence to <out>/CREDITS.md (read from Openverse, or given: --license, --author,
    --page). Prints the "src" / "icon" to put in deck.json and the "credit" when the licence asks for one.
"""
import argparse
import io
import json
import re
import sys
import unicodedata
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

AGENT = "deck-images/1.0 (presentation skill)"
CDN, RESOLVE = "https://cdn.jsdelivr.net/npm", "https://data.jsdelivr.com/v1/packages/npm"
OPENVERSE = "https://api.openverse.org/v1/images"
ICON_SETS = {  # prefix: package, path in the package, licence, site, licence file to keep next to the icons (or "")
    "si": ("simple-icons", "icons/{name}.svg", "CC0-1.0 (the logo stays a trademark of its owner)", "https://simpleicons.org", ""),
    "lucide": ("lucide-static", "icons/{name}.svg", "ISC", "https://lucide.dev/icons", "LICENSE"),
    "tabler": ("@tabler/icons", "icons/outline/{name}.svg", "MIT", "https://tabler.io/icons", "LICENSE"),
}
ATTRIBUTION_FREE = ("cc0", "pdm")  # Openverse licences that ask for no credit
MAX_ICON, MAX_PICTURE, MAX_SIDE = 2 << 20, 30 << 20, 1920  # bytes of an SVG, bytes of a download, pixels of the long side
VERSIONS: dict[str, str] = {}

# ======================================================================= web
def fetch(url: str, limit: int) -> bytes:
    """GET a URL (https only) and return at most `limit` bytes; a bigger answer is refused, an HTTP error said."""
    if not url.startswith("https://"):
        sys.exit(f"images: only https addresses are downloaded, not {url!r}")
    request = urllib.request.Request(url, headers={"User-Agent": AGENT, "Accept": "*/*"})
    try:
        with urllib.request.urlopen(request, timeout=40) as answer:
            data = answer.read(limit + 1)
    except urllib.error.HTTPError as error:
        raise LookupError(f"{url}: HTTP {error.code}") from None
    except (urllib.error.URLError, TimeoutError) as error:
        sys.exit(f"images: {url}: no answer ({getattr(error, 'reason', error)}): no network here? draw it (svg slide) or ask for the files")
    if len(data) > limit:
        sys.exit(f"images: {url}: bigger than {limit >> 20} MB, refused")
    return data

def resolved(package: str) -> str:
    """The latest version of an npm package on jsDelivr (pinned in the URLs and the credits)."""
    if package not in VERSIONS:
        try:
            VERSIONS[package] = json.loads(fetch(f"{RESOLVE}/{package}/resolved", 1 << 16))["version"]
        except (LookupError, KeyError, json.JSONDecodeError):
            sys.exit(f"images: cannot resolve the version of {package} on jsDelivr")
    return VERSIONS[package]

# ======================================================================= files
def save(out: Path, name: str, data: bytes, force: bool) -> Path:
    """Write out/name; the same bytes already there are kept, other bytes are only replaced with --force."""
    file = out / name
    if file.exists() and file.read_bytes() != data and not force:
        sys.exit(f"images: {file} exists with another picture (--force replaces it, --name gives another name)")
    out.mkdir(parents=True, exist_ok=True)
    file.write_bytes(data)
    return file

def credit(out: Path, name: str, line: str) -> None:
    """One line per picture in out/CREDITS.md (replaced when the picture is downloaded again)."""
    file = out / "CREDITS.md"
    head = "# Credits of the pictures\n\nWritten by `images.py`: where each picture comes from and its licence.\n\n"
    rows = [r for r in (file.read_text(encoding="utf-8").splitlines() if file.exists() else []) if r.startswith("- `")]
    rows = [r for r in rows if not r.startswith(f"- `{name}`")] + [f"- `{name}`: {line}"]
    file.write_text(head + "\n".join(sorted(rows)) + "\n", encoding="utf-8")

def slug(text: str, fallback: str) -> str:
    ascii_text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"[^a-z0-9]+", "-", ascii_text.lower()).strip("-")[:40].strip("-") or fallback

def picture_kind(data: bytes) -> str:
    """svg, png, jpeg, or the Pillow format of another picture; empty when it is not a picture (an HTML page...)."""
    if b"<svg" in data[:4096].lower() and b"<html" not in data[:4096].lower():
        return "svg"
    if data.startswith(b"\x89PNG"):
        return "png"
    if data.startswith(b"\xff\xd8\xff"):
        return "jpeg"
    try:
        from PIL import Image
        with Image.open(io.BytesIO(data)) as image:
            return image.format.lower()
    except ImportError:
        sys.exit("images: Pillow is needed for this picture (sudo dnf install python3-pillow, or pip install --user pillow)")
    except Exception:  # Pillow raises many kinds of errors for a file that is not a picture
        return ""

def normalize(data: bytes, kind: str, max_side: int) -> tuple[bytes, str]:
    """PNG / JPEG out (what the web page, the PDF and the PPTX all read), the long side at most max_side pixels."""
    if kind == "svg":
        return data, "svg"
    from PIL import Image
    with Image.open(io.BytesIO(data)) as image:
        if kind in ("png", "jpeg") and max(image.size) <= max_side:
            return data, kind
        alpha = image.mode in ("RGBA", "LA", "P") and (image.mode != "P" or "transparency" in image.info)
        image = image.convert("RGBA" if alpha else "RGB")
        image.thumbnail((max_side, max_side))
        buffer = io.BytesIO()
        image.save(buffer, "PNG" if alpha else "JPEG", **({"optimize": True} if alpha else {"quality": 86}))
        return buffer.getvalue(), "png" if alpha else "jpeg"

# ======================================================================= icon
def brand_colour(name: str) -> str:
    """The official colour of a Simple Icons logo (its data file), '' when it is not listed."""
    data = json.loads(fetch(f"{CDN}/simple-icons@{resolved('simple-icons')}/data/simple-icons.json", 8 << 20))
    for icon in data if isinstance(data, list) else data.get("icons", []):
        if icon.get("slug") == name or slug(icon.get("title", ""), "") == name:
            return "#" + icon["hex"]
    return ""

def icon(spec: str, out: Path, brand: bool, force: bool) -> None:
    prefix, _, name = spec.partition(":")
    if prefix not in ICON_SETS or not re.fullmatch(r"[a-z0-9][a-z0-9-]*", name):
        sys.exit(f"images: {spec!r}: give set:name with a set among {', '.join(ICON_SETS)} (ex: si:cmake, lucide:rocket)")
    package, path, licence, site, licence_file = ICON_SETS[prefix]
    version = resolved(package)
    try:
        data = fetch(f"{CDN}/{package}@{version}/{path.format(name=name)}", MAX_ICON)
    except LookupError:
        sys.exit(f"images: {spec}: no such icon in {package} {version}: look the name up at {site}")
    if picture_kind(data) != "svg":
        sys.exit(f"images: {spec}: the answer is not an SVG")
    if brand and prefix == "si":
        colour = brand_colour(name)
        if colour:
            data = data.replace(b"<svg ", f'<svg fill="{colour}" '.encode(), 1)
        else:
            sys.stderr.write(f"images: {spec}: no official colour found, kept in one colour\n")
    file = save(out, f"{name}.svg", data, force)
    if licence_file and not (out / "licenses" / f"{prefix}.txt").exists():
        text = fetch(f"{CDN}/{package}@{version}/{licence_file}", 1 << 16)
        save(out / "licenses", f"{prefix}.txt", text, True)
    credit(out, file.name, f"{package} {version}, icon `{name}`, {licence} - {site}"
           + (f" (licence text: `licenses/{prefix}.txt`)" if licence_file else ""))
    print(f'{file}  ->  "icon": "{out.name}/{file.name}"')

# ======================================================================= search / get
def licence_label(meta: dict) -> str:
    code, version = meta.get("license", ""), meta.get("license_version") or ""
    if code == "cc0":
        return "CC0"
    if code == "pdm":
        return "public domain"
    return f"CC {code.upper()} {version}".strip()

def search(query: str, commercial: bool, count: int, sheet: str | None) -> None:
    params = {"q": query, "page_size": str(min(max(count, 1), 20)), "mature": "false"}
    if commercial:
        params["license_type"] = "commercial"
    try:
        results = json.loads(fetch(f"{OPENVERSE}/?{urllib.parse.urlencode(params)}", 4 << 20)).get("results", [])
    except LookupError as error:
        sys.exit(f"images: search failed ({error})")
    if not results:
        print("no picture found: other words (English works best), or without --commercial")
        return
    for number, meta in enumerate(results, 1):
        print(f"{number:2}. openverse:{meta['id']}  {licence_label(meta):14} {meta.get('width') or '?'}x{meta.get('height') or '?'}"
              f"  {meta.get('provider', '')}  {(meta.get('creator') or '?')[:28]}  {(meta.get('title') or '')[:60]}")
    if sheet:
        contact_sheet(results, Path(sheet))

def contact_sheet(results: list, out: Path) -> None:
    """The thumbnails of a search, numbered, on one PNG: Read it to choose."""
    from PIL import Image, ImageDraw
    cell, cols = (320, 240), 4
    rows = (len(results) + cols - 1) // cols
    canvas = Image.new("RGB", (cols * (cell[0] + 8) + 8, rows * (cell[1] + 8) + 8), "#808080")
    draw = ImageDraw.Draw(canvas)
    for index, meta in enumerate(results):
        x, y = 8 + (index % cols) * (cell[0] + 8), 8 + (index // cols) * (cell[1] + 8)
        try:
            with Image.open(io.BytesIO(fetch(meta["thumbnail"], 4 << 20))) as thumb:
                thumb = thumb.convert("RGB")
                thumb.thumbnail(cell)
                canvas.paste(thumb, (x + (cell[0] - thumb.width) // 2, y + (cell[1] - thumb.height) // 2))
        except Exception:  # a missing thumbnail leaves its cell grey, the others are still shown
            pass
        draw.rectangle((x, y, x + 34, y + 24), fill="#000000")
        draw.text((x + 6, y + 5), str(index + 1), fill="#ffffff")
    out.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(out)
    print(f"sheet: {out} (the numbers are the ones of the list)")

def get(source: str, out: Path, args: argparse.Namespace) -> None:
    if source.startswith("openverse:"):
        ident = source.split(":", 1)[1]
        if not re.fullmatch(r"[0-9a-f-]{36}", ident):
            sys.exit(f"images: {source!r}: an Openverse id is the uuid printed by search")
        try:
            meta = json.loads(fetch(f"{OPENVERSE}/{ident}/", 1 << 20))
        except LookupError:
            sys.exit(f"images: {source}: not found on Openverse")
        url, label, page = meta["url"], licence_label(meta), meta.get("foreign_landing_url") or meta["url"]
        author, title = meta.get("creator") or "unknown author", meta.get("title") or ""
        needs = meta.get("license") not in ATTRIBUTION_FREE
        line = f"\"{title}\" by {author}, {label} ({meta.get('license_url', '')}) - {page}"
    else:
        url, label, page, author, title = source, args.license or "", args.page or source, args.author or "", ""
        needs = bool(re.search(r"\bBY\b|attribution", label, re.I))  # CC BY, BY-SA...: the credit goes on the slide
        line = f"{author + ', ' if author else ''}{label or 'licence: to check at the source'} - {page}"
    try:
        data = fetch(url, MAX_PICTURE)
    except LookupError as error:
        sys.exit(f"images: {error}")
    kind = picture_kind(data)
    if not kind:
        sys.exit(f"images: {url}: not a picture (a web page? give the address of the file itself)")
    data, kind = normalize(data, kind, args.max)
    name = f"{args.name or slug(title or Path(urllib.parse.urlparse(url).path).stem, 'picture')}.{'jpg' if kind == 'jpeg' else kind}"
    file = save(out, name, data, args.force)
    credit(out, file.name, line)
    print(f'{file}  ->  "src": "{out.name}/{file.name}"' + (f'  "credit": "{", ".join(p for p in (author, label) if p)}"' if needs else ""))
    if not label:
        sys.stderr.write("images: no --license given: read it on the page of the picture before using it\n")
    if needs:
        print("this licence asks for the credit: put it on the slide (\"credit\") and keep CREDITS.md with the deck")

# ======================================================================= main
def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    one = sub.add_parser("icon", help="logos and icons by name: si:<slug>, lucide:<name>, tabler:<name>")
    one.add_argument("names", nargs="+")
    one.add_argument("--brand", action="store_true", help="Simple Icons in their official colour (else the theme colour)")
    two = sub.add_parser("search", help="openly licensed photos on Openverse")
    two.add_argument("query")
    two.add_argument("--commercial", action="store_true", help="only licences that allow a commercial use")
    two.add_argument("-n", type=int, default=12, help="how many (at most 20)")
    two.add_argument("--sheet", default=None, help="PNG of the numbered thumbnails")
    three = sub.add_parser("get", help="download a picture (openverse:<id> or an https address) and credit it")
    three.add_argument("source")
    three.add_argument("--name", default=None, help="file name without extension")
    three.add_argument("--license", default=None, help="licence read at the source (an address that is not Openverse)")
    three.add_argument("--author", default=None)
    three.add_argument("--page", default=None, help="page of the picture (where its licence is written)")
    three.add_argument("--max", type=int, default=MAX_SIDE, help="long side in pixels (default 1920)")
    for command in (one, three):
        command.add_argument("-o", "--out", default="img", help="folder of the pictures, next to deck.json (default: img)")
        command.add_argument("--force", action="store_true", help="replace a different file of the same name")
    args = parser.parse_args()
    if args.command == "icon":
        out = Path(args.out).expanduser()
        for spec in args.names:
            icon(spec, out, args.brand, args.force)
    elif args.command == "search":
        search(args.query, args.commercial, args.n, args.sheet)
    else:
        get(args.source, Path(args.out).expanduser(), args)

if __name__ == "__main__":
    main()
