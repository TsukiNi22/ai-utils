#!/usr/bin/env python3
# =============================================================================
# build.py - generator of the claude-cluster documentation site (deterministic)
#
#   build.py --out DIR [--code-root DIR] [--version X] [--commit H] [--embed] [--artifacts DIR]
#
#   src/pages/**/<slug>.<en|fr>.md   the pages (TOML front matter: title, summary, section, order, keywords)
#   gen/*.py                         the generated pages (MCP tools, configuration, keys, changelog, downloads...)
#   src/assets/                      extra.css / extra.js / logo / actions
#   -> DIR/<slug>.html (self-contained, EN + FR in one file), index.html, search-index.json / .js, sitemap.xml,
#      docs-map.json (contextual help), logo.svg, downloads/
# html-style scripts (new_page.py, check_style.py) are looked up in ../../../../docs/html-style/scripts
# =============================================================================

import argparse
import html
import importlib.util
import json
import re
import subprocess
import sys
import tomllib
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE / "gen"))
import _data  # noqa: E402
import mdlib  # noqa: E402

REPO_ROOT = HERE.parents[3]
LANGS = ("en", "fr")
VERSION_TOKEN = "@@CC_VERSION@@"
COMMIT_TOKEN = "@@CC_COMMIT@@"

# ----- context -----
class Context:
    def __init__(self, args: argparse.Namespace):
        self.config = tomllib.loads((HERE / "config.toml").read_text())
        self.code_root = Path(args.code_root).resolve()
        self.artifacts = Path(args.artifacts).resolve() if args.artifacts else None
        self.embed = args.embed
        self.cpp = Path(args.cpp).resolve() if args.cpp else None
        self.version = args.version or self.cmake_version()
        self.commit = args.commit or self.git("rev-parse", "--short", "HEAD") or "unknown"
        self.date = self.git("log", "-1", "--format=%cs") or ""
        self.out = Path(args.out).resolve()

    def cmake_version(self) -> str:
        m = re.search(r"project\(\S+\s+VERSION\s+([\d.]+)", (self.code_root / "CMakeLists.txt").read_text())
        return m.group(1) if m else "0"

    def git(self, *cmd: str) -> str:
        try:
            return subprocess.run(["git", "-C", str(self.code_root), *cmd], capture_output=True, text=True, check=True).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            return ""

    def shown_version(self) -> tuple[str, str]:
        return (VERSION_TOKEN, COMMIT_TOKEN) if self.embed else (self.version, self.commit)

# ----- pages -----
def load_static_pages() -> dict[str, dict]:
    pages: dict[str, dict] = {}
    for path in sorted((HERE / "src" / "pages").rglob("*.md")):
        slug, lang = path.name[:-3].rsplit(".", 1)
        if lang not in LANGS:
            raise SystemExit("%s: the name must be <slug>.<en|fr>.md" % path)
        front, body = mdlib.split_front(path.read_text())
        page = pages.setdefault(slug, {"slug": slug, "section": "", "order": 0, "keywords": ""})
        page[lang] = {"title": front["title"], "summary": front.get("summary", ""), "md": body, "keywords": front.get("keywords", "")}
        if lang == "en":
            page["section"] = front["section"]
            page["order"] = front.get("order", 100)
    return pages

def load_generated_pages(ctx: Context) -> tuple[dict[str, dict], list[dict]]:
    pages: dict[str, dict] = {}
    extra: list[dict] = []
    for path in sorted((HERE / "gen").glob("*.py")):
        if path.name.startswith("_"):
            continue
        spec = importlib.util.spec_from_file_location("gen_" + path.stem, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for page in module.pages(ctx):
            pages[page["slug"]] = page
        if hasattr(module, "search_entries"):
            extra += module.search_entries(ctx)
    return pages, extra

def split_sections(blocks: list[dict]) -> tuple[list[str], list[dict]]:
    """The blocks before the first h2 (intro) and the h2 sections (id, title, raw, html)."""
    intro: list[str] = []
    sections: list[dict] = []
    for b in blocks:
        if b.get("h") == 2:
            sections.append({"id": b["id"], "title": b["title"], "raw": b["raw"], "html": [], "md": ""})
        elif b.get("h"):
            html_b = '<h%d id="%s">%s</h%d>' % (b["h"], b["id"], b["title"], b["h"])
            (sections[-1]["html"] if sections else intro).append(html_b)
        else:
            (sections[-1]["html"] if sections else intro).append(b["html"])
    return intro, sections

def parse_page(page: dict) -> None:
    for lang in LANGS:
        if lang not in page:
            raise SystemExit("page %s: the %s version is missing" % (page["slug"], lang))
        blocks = mdlib.Renderer(lang).render(page[lang]["md"])
        page[lang]["intro"], page[lang]["sections"] = split_sections(blocks)
        # the text of each section for the search index
        parts = re.split(r"^## .*$", page[lang]["md"], flags=re.M)
        for n, s in enumerate(page[lang]["sections"]):
            s["text"] = mdlib.plain_text(parts[n + 1] if n + 1 < len(parts) else "")
        page[lang]["lead_text"] = mdlib.plain_text(parts[0])
    ids_en = [s["id"] for s in page["en"]["sections"]]
    ids_fr = [s["id"] for s in page["fr"]["sections"]]
    if ids_en != ids_fr:
        raise SystemExit("page %s: the sections differ between EN %s and FR %s" % (page["slug"], ids_en, ids_fr))

def ordered(pages: dict[str, dict], sections: list[dict]) -> list[dict]:
    rank = {s["id"]: n for n, s in enumerate(sections)}
    for p in pages.values():
        if p["section"] not in rank:
            raise SystemExit("page %s: unknown section %r" % (p["slug"], p["section"]))
    return sorted(pages.values(), key=lambda p: (rank[p["section"]], p["order"], p["slug"]))

# ----- html -----
def both(en: str, fr: str) -> str:
    return '<span data-lang="en">%s</span><span data-lang="fr">%s</span>' % (en, fr)

def esc(text: str) -> str:
    return html.escape(text, quote=True)

def sidebar(ctx: Context, page: dict | None, all_pages: list[dict]) -> str:
    out = ['<aside class="sidebar" id="sidebar"><p data-i18n>Contents</p>']
    if page:
        for n, s in enumerate(page["en"]["sections"], 1):
            fr = page["fr"]["sections"][n - 1]
            out.append('<a href="#%s">%d. %s</a>' % (s["id"], n, both(esc(s["raw"]), esc(fr["raw"]))))
        out.append("<hr>")
    for sec in ctx.config["sections"]:
        out.append('<div class="grp">%s</div>' % both(esc(sec["en"]), esc(sec["fr"])))
        for p in all_pages:
            if p["section"] == sec["id"]:
                cls = ' class="cur"' if page and p["slug"] == page["slug"] else ""
                out.append('<a href="%s.html"%s>%s</a>' % (p["slug"], cls, both(esc(p["en"]["title"]), esc(p["fr"]["title"]))))
    out.append("</aside>")
    return "".join(out)

def page_body(ctx: Context, page: dict, all_pages: list[dict]) -> str:
    sec = next(s for s in ctx.config["sections"] if s["id"] == page["section"])
    idx = all_pages.index(page)
    out = [sidebar(ctx, page, all_pages), "<main>"]
    out.append('<header><div class="page-eyebrow">%s</div><h1>%s</h1><p class="lead">%s</p></header>' % (
        both(esc(sec["en"]), esc(sec["fr"])),
        both(esc(page["en"]["title"]), esc(page["fr"]["title"])),
        both(esc(page["en"]["summary"]), esc(page["fr"]["summary"]))))
    if page["en"]["intro"] or page["fr"]["intro"]:
        out.append('<div data-lang="en">%s</div><div data-lang="fr">%s</div>' % ("".join(page["en"]["intro"]), "".join(page["fr"]["intro"])))
    for n, s in enumerate(page["en"]["sections"], 1):
        fr = page["fr"]["sections"][n - 1]
        out.append('<section id="%s"><h2>%d. %s</h2><div data-lang="en">%s</div><div data-lang="fr">%s</div></section>' % (
            s["id"], n, both(s["title"], fr["title"]), "".join(s["html"]), "".join(fr["html"])))
    pager = []
    if idx > 0:
        p = all_pages[idx - 1]
        pager.append('<a href="%s.html"><span data-i18n>Previous</span>%s</a>' % (p["slug"], both(esc(p["en"]["title"]), esc(p["fr"]["title"]))))
    else:
        pager.append('<a href="index.html"><span data-i18n>Previous</span>%s</a>' % both("Home", "Accueil"))
    if idx + 1 < len(all_pages):
        p = all_pages[idx + 1]
        pager.append('<a class="next" href="%s.html"><span data-i18n>Next</span>%s</a>' % (p["slug"], both(esc(p["en"]["title"]), esc(p["fr"]["title"]))))
    out.append('<nav class="pager">%s</nav></main>' % "".join(pager))
    return "".join(out)

TILE_SVG = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">%s</svg>'
SEARCH_SVG = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><circle cx="11" cy="11" r="7"/><path d="M20 20l-4-4"/></svg>'

def home_body(ctx: Context, all_pages: list[dict], changes: list[dict], downloads: list[dict]) -> str:
    home = ctx.config["home"]
    ver, commit = ctx.shown_version()
    out = ['<main><header class="hero"><div class="page-eyebrow">claude-cluster <span class="badge">v%s · %s</span></div>' % (ver, commit)]
    out.append("<h1>%s</h1>" % both("Documentation", "Documentation"))
    out.append('<p class="lead">%s</p>' % both(esc(home["tagline_en"]), esc(home["tagline_fr"])))
    out.append('<div class="sbox">%s<input type="search" autocomplete="off" aria-label="Search" placeholder="Search the docs, settings, tools…  ( / )" data-ph-fr="Rechercher dans les docs, réglages, outils…  ( / )"><ul class="results" hidden></ul></div></header>' % SEARCH_SVG)
    out.append('<div class="grid">')
    for sec in ctx.config["sections"]:
        first = next((p for p in all_pages if p["section"] == sec["id"]), None)
        if not first:
            continue
        out.append('<a class="tile" href="%s.html">%s<h3>%s</h3><p>%s</p></a>' % (
            first["slug"], TILE_SVG % sec["icon"], both(esc(sec["en"]), esc(sec["fr"])), both(esc(sec["desc_en"]), esc(sec["desc_fr"]))))
    out.append("</div>")
    out.append('<div class="two"><section id="downloads"><h2>%s</h2>' % both("Downloads", "Téléchargements"))
    out.append('<div class="table-wrap"><table><thead><tr><th>%s</th><th>Version</th><th>%s</th></tr></thead><tbody>' % (both("File", "Fichier"), both("Size", "Taille")))
    for d in downloads:
        out.append('<tr><td><a href="%s">%s</a></td><td>%s</td><td>%s</td></tr>' % (esc(d["href"]), esc(d["name"]), esc(d["version"]), esc(d["size"])))
    out.append('</tbody></table></div><p><a href="downloads.html">%s</a></p></section>' % both("All downloads", "Tous les téléchargements"))
    out.append('<section id="changelog"><h2>Changelog</h2><ul class="cl">')
    for c in changes[:5]:
        out.append("<li><b>%s</b><span>%s</span></li>" % (esc(c["version"]), both(esc(c["en"]), esc(c["fr"]))))
    out.append('</ul><p><a href="changelog.html">%s</a></p></section></div>' % both("Full changelog", "Historique complet"))
    out.append('<div class="links"><a href="downloads.html">%s</a><a href="changelog.html">Changelog</a><a href="faq.html">FAQ</a><a href="glossary.html">%s</a></div></main>' % (both("Downloads", "Téléchargements"), both("Glossary", "Glossaire")))
    return "".join(out)

def make_page(ctx: Context, body: str, out: Path, title: str, wide: bool = False) -> None:
    ver, commit = ctx.shown_version()
    tmp = ctx.out / ".body.html"
    tmp.write_text(body)
    script = REPO_ROOT / "docs" / "html-style" / "scripts" / "new_page.py"
    footer = "claude-cluster v%s · %s · %s" % (ver, commit, "docs")
    cmd = [sys.executable, str(script), str(tmp), "-o", str(out), "--force", "--lang-switch",
           "--css", str(HERE / "src/assets/extra.css"), "--js", str(HERE / "src/assets/extra.js"),
           "--actions", str(HERE / "src/assets/actions.html"),
           "--set", "PROJECT=claude-cluster docs", "TITLE=%s" % title, "STORAGE_KEY=claude-cluster-docs", "FOOTER=%s" % footer, "VERSION=%s" % ver]
    subprocess.run(cmd, check=True, capture_output=True, text=True)
    tmp.unlink()
    text = out.read_text()
    logo = (HERE / "src/assets/logo-inline.svg").read_text().strip()
    text = re.sub(r'<svg class="logo".*?</svg>', lambda m: logo, text, count=1, flags=re.S)
    if wide:
        text = text.replace('<div class="layout" id="top">', '<div class="layout wide" id="top">', 1)
    text = text.replace('<a class="brand" href="#top">', '<a class="brand" href="index.html">', 1)
    head = '<link rel="icon" type="image/svg+xml" href="logo.svg">'
    if ctx.embed:
        head += '<meta name="docs-search" content="/docs/search">'
    text = text.replace("</title>", "</title>" + head, 1)
    text = text.replace("</body>", '<script src="search-index.js" defer></script>\n</body>', 1)
    out.write_text(text)

# ----- search index -----
def build_index(ctx: Context, all_pages: list[dict], extra: list[dict]) -> list[dict]:
    entries: list[dict] = []
    for p in all_pages:
        for lang in LANGS:
            d = p[lang]
            entries.append({"k": "docs", "l": lang, "t": d["title"], "d": d["summary"], "u": "%s.html" % p["slug"], "x": (d["keywords"] + " " + d["lead_text"])[:300]})
            for s in d["sections"]:
                entries.append({"k": "docs", "l": lang, "t": "%s › %s" % (d["title"], s["raw"]), "d": s["text"][:110], "u": "%s.html#%s" % (p["slug"], s["id"]), "x": s["text"][:600]})
    entries += extra
    return entries

def split_assets(ctx: Context) -> None:
    """--embed: the style and the scripts every page repeats (about 40 KB) move to site.css / site.js (the pages stay valid on file://)."""
    style = script = ""
    for path in sorted(ctx.out.glob("*.html")):
        text = path.read_text()
        m_style = re.search(r"<style>\n(.*?)\n</style>", text, re.S)
        scripts = re.findall(r"<script>\n(.*?)\n</script>", text, re.S)
        if not m_style or len(scripts) != 2:
            raise SystemExit("%s: the page layout changed, cannot split its assets" % path.name)
        if style and (style != m_style.group(1) or script != "\n".join(scripts)):
            raise SystemExit("%s: its style / script differ from the other pages" % path.name)
        style, script = m_style.group(1), "\n".join(scripts)
        text = text.replace(m_style.group(0), '<link rel="stylesheet" href="site.css">', 1)
        text = re.sub(r"<script>\n.*?\n</script>\n", "", text, flags=re.S)
        text = text.replace('<script src="search-index.js" defer></script>', '<script src="site.js"></script>\n<script src="search-index.js" defer></script>', 1)
        path.write_text(text)
    (ctx.out / "site.css").write_text(style)
    (ctx.out / "site.js").write_text(script)

MIMES = {".html": "text/html; charset=utf-8", ".css": "text/css; charset=utf-8", ".js": "application/javascript; charset=utf-8", ".json": "application/json; charset=utf-8",
         ".svg": "image/svg+xml", ".pdf": "application/pdf", ".xml": "application/xml"}

def write_cpp(ctx: Context, target: Path) -> None:
    """--cpp: the files of the site as byte arrays of a C++ source (no string literal over the 64 KB a compiler must accept)."""
    files = [p for p in sorted(ctx.out.rglob("*")) if p.is_file() and not p.name.startswith(".") and p.name not in ("sitemap.xml", "search-index.json")]  # the index travels as search-index.js
    lines = ["// Generated by docs/site/build.py (--cpp) for claude-cluster %s, do not edit" % ctx.version, '#include "cluster/Docs.hpp"', "namespace {"]
    table = []
    for n, path in enumerate(files):
        data = path.read_bytes()
        rows = [",".join(str(b) for b in data[i:i + 32]) for i in range(0, len(data), 32)]
        lines.append("const unsigned char f%d[] = {%s};" % (n, ",\n".join(rows) if rows else "0"))
        table.append('    {"%s", "%s", f%d, %d},' % (path.relative_to(ctx.out).as_posix(), MIMES.get(path.suffix, "application/octet-stream"), n, len(data)))
    lines += ["}", "const cluster::DocEntry cluster::docs_table[] = {", *table, '    {nullptr, nullptr, nullptr, 0},', "};", "const std::size_t cluster::docs_count = %d;" % len(files)]
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(lines) + "\n")

def copy_downloads(ctx: Context) -> None:
    """The files of downloads/ (the audit report PDF) next to the pages."""
    target = ctx.out / "downloads"
    for path in sorted((HERE / "downloads").glob("*")):
        target.mkdir(parents=True, exist_ok=True)
        (target / path.name).write_bytes(path.read_bytes())

def write_site(ctx: Context) -> None:
    ctx.out.mkdir(parents=True, exist_ok=True)
    copy_downloads(ctx)
    pages, extra = load_generated_pages(ctx)
    static = load_static_pages()
    for slug in static:
        if slug in pages:
            raise SystemExit("page %s exists as source and as generated page" % slug)
    pages.update(static)
    for p in pages.values():
        parse_page(p)
    all_pages = ordered(pages, ctx.config["sections"])
    changes = _data.changes(ctx)
    downloads = _data.downloads(ctx)
    for p in all_pages:
        make_page(ctx, page_body(ctx, p, all_pages), ctx.out / ("%s.html" % p["slug"]), "%s - claude-cluster docs" % p["en"]["title"])
    make_page(ctx, home_body(ctx, all_pages, changes, downloads), ctx.out / "index.html", "claude-cluster documentation", wide=True)
    index = build_index(ctx, all_pages, extra)
    payload = {"version": ctx.version, "commit": ctx.commit, "entries": index}
    (ctx.out / "search-index.json").write_text(json.dumps(payload, ensure_ascii=False, separators=(",", ":"), sort_keys=True))
    (ctx.out / "search-index.js").write_text("window.DOCS_INDEX=" + json.dumps(payload, ensure_ascii=False, separators=(",", ":"), sort_keys=True) + ";")
    (ctx.out / "logo.svg").write_text((HERE / "src/assets/logo.svg").read_text())
    (ctx.out / "docs-map.json").write_text(json.dumps(ctx.config["contexts"], indent=1, sort_keys=True))
    urls = ["index.html"] + ["%s.html" % p["slug"] for p in all_pages]
    base = ctx.config["site"]["pages_url"]
    (ctx.out / "sitemap.xml").write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n' + "".join("<url><loc>%s%s</loc></url>\n" % (base, u) for u in urls) + "</urlset>\n")
    if ctx.embed:
        split_assets(ctx)
    if ctx.cpp:
        write_cpp(ctx, ctx.cpp)
    print("built %d pages + index in %s (v%s %s)" % (len(all_pages), ctx.out, ctx.version, ctx.commit))

def main() -> None:
    ap = argparse.ArgumentParser(description="Generator of the claude-cluster documentation site")
    ap.add_argument("--out", required=True)
    ap.add_argument("--code-root", default=str(REPO_ROOT / "ai" / "claude-cluster"), help="the claude-cluster sources read by the generators")
    ap.add_argument("--version", default="")
    ap.add_argument("--commit", default="")
    ap.add_argument("--embed", action="store_true", help="the copy served by the binary: runtime version tokens, /docs/search endpoint")
    ap.add_argument("--cpp", default="", help="also write the site as a C++ source (byte arrays) for the binary")
    ap.add_argument("--artifacts", default="", help="folder of the built packages (RPM / DEB / APK / PDF) for the downloads sizes")
    ctx = Context(ap.parse_args())
    write_site(ctx)

if __name__ == "__main__":
    main()
