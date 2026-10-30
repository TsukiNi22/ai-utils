#!/usr/bin/env python3
# =============================================================================
# build_site.py - generator of a documentation site in the better-html-style (deterministic, no dependency)
#
#   build_site.py [--site DIR] --out DIR [--version X] [--commit H] [--split]
#
#   DIR/site.toml                     the site: name, languages, sections, home, kinds, contextual help map
#   DIR/src/pages/**/<slug>.<lang>.md the pages (+++ TOML front matter +++: title, summary, section, order, keywords, kind, level)
#                                     (<slug>.md on a site of one language)
#   DIR/gen/*.py                      optional generators: pages(ctx) -> pages, search_entries(ctx) -> extra search entries
#   -> OUT/<slug>.html (self-contained, every language in one file), index.html, search-index.json / .js, sitemap.xml,
#      docs-map.json, logo.svg; --split: the style / script every page repeats moved to site.css / site.js
# html-style (new_page.py) and better-html-style (site.css, site.js, search.html) are sibling skills of this one
# =============================================================================

import argparse
import html
import importlib.util
import json
import re
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path

HERE = Path(__file__).resolve().parent
SKILLS = HERE.parent.parent          # the folder holding the skills (docs/ in the repository, ~/.claude/skills installed)
sys.path.insert(0, str(HERE))
import mdlib  # noqa: E402

NEW_PAGE = SKILLS / "html-style" / "scripts" / "new_page.py"
STYLE = SKILLS / "better-html-style" / "templates"
KNOWN_LANGS = ("en", "fr")
# the kinds of page the sibling skills bring (a [kinds.<name>] table of site.toml adds / overrides one)
BUILTIN_KINDS = {
    "explain": {"css": ["@skill/explain-doc/templates/explain.css", "@skill/better-explain-doc/templates/explain-site.css"],
                "js": ["@skill/explain-doc/templates/explain.js"], "actions": "@skill/explain-doc/templates/actions.html",
                "attrs": {"data-page": "explain"}, "label": {"en": "Explanation", "fr": "Explication"}},
    "protocol": {"css": ["@skill/better-protocol-doc/templates/protocol.css"], "js": [], "actions": "", "attrs": {"data-page": "protocol"},
                 "label": {"en": "Protocol", "fr": "Protocole"}},
}
SEARCH_SVG = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><circle cx="11" cy="11" r="7"/><path d="M20 20l-4-4"/></svg>'
TILE_SVG = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">%s</svg>'
DEFAULT_ICON = '<path d="M5 4h14v16H5zM9 8h6M9 12h6M9 16h4"/>'
REPO_LINK = ('<a class="icon-btn" href="%s" aria-label="GitHub repository" data-i18n-aria-label title="GitHub repository" data-i18n-title>'
             '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">'
             '<path d="M8 6l-5 6 5 6M16 6l5 6-5 6"/></svg></a>')

# ----- context -----
class Context:
    """What a generator (gen/*.py) gets: the config, the folders, the languages, the version, tr() and the Markdown renderer."""

    def __init__(self, args: argparse.Namespace):
        self.site = Path(args.site).resolve()
        config = self.site / "site.toml"
        if not config.exists():
            raise SystemExit("%s: no site.toml (start from the template of better-html-doc)" % self.site)
        self.config = tomllib.loads(config.read_text(encoding="utf-8"))
        self.out = Path(args.out).resolve()
        self.split = args.split
        s = self.config.setdefault("site", {})
        self.langs = list(s.get("langs", ["en"]))
        for lang in self.langs:
            if lang not in KNOWN_LANGS:
                raise SystemExit("site.toml: language %r (the switch of html-style knows %s)" % (lang, ", ".join(KNOWN_LANGS)))
        self.name = s.get("name", self.site.name)
        self.version = args.version or s.get("version", "") or self.git("describe", "--tags", "--abbrev=0")
        self.commit = args.commit or self.git("rev-parse", "--short", "HEAD")
        self.renderer = {lang: mdlib.Renderer(lang) for lang in self.langs}

    def git(self, *cmd: str) -> str:
        try:
            return subprocess.run(["git", "-C", str(self.site), *cmd], capture_output=True, text=True, check=True).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            return ""

    def tr(self, value: str | dict, lang: str) -> str:
        """A text of site.toml: a plain string (every language) or {en = "...", fr = "..."}."""
        if isinstance(value, dict):
            return value.get(lang) or value.get("en") or next(iter(value.values()), "")
        return value or ""

def both(ctx: Context, texts: dict) -> str:
    """One text per language: a span per language on a bilingual site, the text alone else."""
    if len(ctx.langs) == 1:
        return texts[ctx.langs[0]]
    return "".join('<span data-lang="%s">%s</span>' % (lang, texts[lang]) for lang in ctx.langs)

def both_tr(ctx: Context, value: str | dict) -> str:
    return both(ctx, {lang: esc(ctx.tr(value, lang)) for lang in ctx.langs})

def blocks(ctx: Context, parts: dict) -> str:
    """Block content per language (divs on a bilingual site)."""
    if len(ctx.langs) == 1:
        return parts[ctx.langs[0]]
    return "".join('<div data-lang="%s">%s</div>' % (lang, parts[lang]) for lang in ctx.langs)

def esc(text: str) -> str:
    return html.escape(text, quote=True)

def resolve(ctx: Context, path: str) -> Path:
    """@skill/<skill>/... = a file of a sibling skill, else a path relative to the site."""
    if path.startswith("@skill/"):
        return SKILLS / path[len("@skill/"):]
    return (ctx.site / path).resolve()

# ----- pages -----
def load_static_pages(ctx: Context) -> dict[str, dict]:
    pages: dict[str, dict] = {}
    root = ctx.site / "src" / "pages"
    for path in sorted(root.rglob("*.md")):
        stem = path.name[:-3]
        slug, _, lang = stem.rpartition(".")
        if not slug or lang not in KNOWN_LANGS:
            if len(ctx.langs) > 1:
                raise SystemExit("%s: on a site of several languages the name is <slug>.<%s>.md" % (path, "|".join(ctx.langs)))
            slug, lang = stem, ctx.langs[0]
        if lang not in ctx.langs:
            raise SystemExit("%s: language %s is not in [site] langs" % (path, lang))
        front, body = mdlib.split_front(path.read_text(encoding="utf-8"))
        for key in ("title", "section"):
            if key not in front and (key == "title" or lang == ctx.langs[0]):
                raise SystemExit("%s: '%s' missing in the front matter" % (path, key))
        page = pages.setdefault(slug, {"slug": slug, "section": "", "order": 100, "kind": "", "level": "", "source": path})
        page[lang] = {"title": front["title"], "summary": front.get("summary", ""), "md": body, "keywords": front.get("keywords", "")}
        if lang == ctx.langs[0]:
            page.update(section=front["section"], order=front.get("order", 100), kind=front.get("kind", ""), level=front.get("level", ""), source=path)
    return pages

def load_generated_pages(ctx: Context) -> tuple[dict[str, dict], list[dict]]:
    pages: dict[str, dict] = {}
    extra: list[dict] = []
    folder = ctx.site / "gen"
    if folder.is_dir():
        sys.path.insert(0, str(folder))
    for path in sorted(folder.glob("*.py")) if folder.is_dir() else []:
        if path.name.startswith("_"):
            continue
        spec = importlib.util.spec_from_file_location("gen_" + path.stem, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for page in module.pages(ctx) if hasattr(module, "pages") else []:
            page.setdefault("order", 100)
            page.setdefault("kind", "")
            page.setdefault("level", "")
            pages[page["slug"]] = page
        if hasattr(module, "search_entries"):
            extra += module.search_entries(ctx)
    return pages, extra

def split_sections(blocks_: list[dict]) -> tuple[list[str], list[dict]]:
    """The blocks before the first h2 (intro) and the h2 sections (id, title, raw, html)."""
    intro: list[str] = []
    sections: list[dict] = []
    for b in blocks_:
        if b.get("h") == 2:
            sections.append({"id": b["id"], "title": b["title"], "raw": b["raw"], "html": [], "level": b.get("level", "")})
        elif b.get("h"):
            level = ' data-level="%s"' % b["level"] if b.get("level") else ""
            html_b = '<h%d id="%s"%s>%s</h%d>' % (b["h"], b["id"], level, b["title"], b["h"])
            (sections[-1]["html"] if sections else intro).append(html_b)
        else:
            (sections[-1]["html"] if sections else intro).append(b["html"])
    return intro, sections

def parse_page(ctx: Context, page: dict) -> None:
    for lang in ctx.langs:
        if lang not in page:
            raise SystemExit("page %s: the %s version is missing" % (page["slug"], lang))
        d = page[lang]
        d["intro"], d["sections"] = split_sections(ctx.renderer[lang].render(d["md"]))
        parts = re.split(r"^## .*$", d["md"], flags=re.M)  # the text of each section, for the search index
        for n, s in enumerate(d["sections"]):
            s["text"] = mdlib.plain_text(parts[n + 1] if n + 1 < len(parts) else "")
        d["lead_text"] = mdlib.plain_text(parts[0])
    ids = {lang: [s["id"] for s in page[lang]["sections"]] for lang in ctx.langs}
    if len({tuple(v) for v in ids.values()}) > 1:
        raise SystemExit("page %s: the sections differ between the languages %s" % (page["slug"], ids))
    if page["kind"] and page["kind"] not in kinds(ctx):
        raise SystemExit("page %s: unknown kind %r (%s)" % (page["slug"], page["kind"], ", ".join(kinds(ctx))))

def kinds(ctx: Context) -> dict[str, dict]:
    out = {k: dict(v) for k, v in BUILTIN_KINDS.items()}
    for name, value in ctx.config.get("kinds", {}).items():
        out[name] = {**out.get(name, {"css": [], "js": [], "actions": "", "attrs": {}, "label": name}), **value}
    return out

def ordered(ctx: Context, pages: dict[str, dict]) -> list[dict]:
    rank = {s["id"]: n for n, s in enumerate(ctx.config.get("sections", []))}
    for p in pages.values():
        if p["section"] not in rank:
            raise SystemExit("page %s: unknown section %r (sections of site.toml: %s)" % (p["slug"], p["section"], ", ".join(rank)))
    return sorted(pages.values(), key=lambda p: (rank[p["section"]], p["order"], p["slug"]))

# ----- html -----
def sidebar(ctx: Context, page: dict | None, all_pages: list[dict]) -> str:
    out = ['<aside class="sidebar" id="sidebar">']
    if page and page[ctx.langs[0]]["sections"]:
        out.append('<p data-i18n>On this page</p>')
        for n, s in enumerate(page[ctx.langs[0]]["sections"], 1):
            texts = {lang: esc(page[lang]["sections"][n - 1]["raw"]) for lang in ctx.langs}
            level = ' data-level="%s"' % s["level"] if s.get("level") else ""
            out.append('<a href="#%s"%s>%d. %s</a>' % (s["id"], level, n, both(ctx, texts)))
        out.append("<hr>")
    for sec in ctx.config.get("sections", []):
        out.append('<div class="grp">%s</div>' % both_tr(ctx, sec["title"]))
        for p in all_pages:
            if p["section"] == sec["id"]:
                cls = ' class="cur" aria-current="page"' if page and p["slug"] == page["slug"] else ""
                out.append('<a href="%s.html"%s>%s</a>' % (p["slug"], cls, both(ctx, {lang: esc(p[lang]["title"]) for lang in ctx.langs})))
    out.append("</aside>")
    return "".join(out)

def page_meta(ctx: Context, page: dict) -> str:
    """Last change (git) and the edit link of a page written by hand."""
    source = page.get("source")
    if not source:
        return ""
    items = []
    date = ctx.git("log", "-1", "--format=%cs", "--", str(source))
    if date:
        items.append('<span><span data-i18n>Updated</span> %s</span>' % esc(date))
    edit = ctx.config["site"].get("edit_url", "")
    if edit:
        rel = Path(source).relative_to(ctx.site).as_posix()
        items.append('<a class="edit" href="%s" data-i18n>Edit this page</a>' % esc(edit.replace("{path}", rel)))
    return '<div class="page-meta">%s</div>' % "".join(items) if items else ""

def page_body(ctx: Context, page: dict, all_pages: list[dict]) -> str:
    sec = next(s for s in ctx.config["sections"] if s["id"] == page["section"])
    idx = all_pages.index(page)
    first = ctx.langs[0]
    out = [sidebar(ctx, page, all_pages), "<main>"]
    out.append('<header><div class="page-eyebrow">%s</div><h1>%s</h1><p class="lead">%s</p>%s</header>' % (
        both_tr(ctx, sec["title"]), both(ctx, {lang: esc(page[lang]["title"]) for lang in ctx.langs}),
        both(ctx, {lang: esc(page[lang]["summary"]) for lang in ctx.langs}), page_meta(ctx, page)))
    if any(page[lang]["intro"] for lang in ctx.langs):
        out.append(blocks(ctx, {lang: "".join(page[lang]["intro"]) for lang in ctx.langs}))
    for n, s in enumerate(page[first]["sections"], 1):
        level = ' data-level="%s"' % s["level"] if s.get("level") else ""
        title = both(ctx, {lang: page[lang]["sections"][n - 1]["title"] for lang in ctx.langs})
        out.append('<section id="%s"%s><h2>%d. %s</h2>%s</section>' % (s["id"], level, n, title,
                   blocks(ctx, {lang: "".join(page[lang]["sections"][n - 1]["html"]) for lang in ctx.langs})))
    pager = []
    if idx > 0:
        p = all_pages[idx - 1]
        pager.append('<a href="%s.html"><span data-i18n>Previous</span>%s</a>' % (p["slug"], both(ctx, {lang: esc(p[lang]["title"]) for lang in ctx.langs})))
    else:
        pager.append('<a href="index.html"><span data-i18n>Previous</span><span data-i18n>Home</span></a>')
    if idx + 1 < len(all_pages):
        p = all_pages[idx + 1]
        pager.append('<a class="next" href="%s.html"><span data-i18n>Next</span>%s</a>' % (p["slug"], both(ctx, {lang: esc(p[lang]["title"]) for lang in ctx.langs})))
    out.append('<nav class="pager">%s</nav></main>' % "".join(pager))
    return "".join(out)

def changelog(ctx: Context) -> list[dict]:
    """The versions of a Keep a Changelog file (## [1.2.0] - date / ## 1.2.0): the first line or bullet of each."""
    path = ctx.config.get("home", {}).get("changelog", "")
    if not path or not resolve(ctx, path).exists():
        return []
    out = []
    for m in re.finditer(r"^##\s+\[?v?([\w.\-]+)\]?[^\n]*\n(.*?)(?=^##\s|\Z)", resolve(ctx, path).read_text(encoding="utf-8"), re.S | re.M):
        if m.group(1).lower() == "unreleased":
            continue
        line = next((re.sub(r"^[-*]\s+|^###\s+", "", x).strip() for x in m.group(2).splitlines() if x.strip() and not x.startswith("###")), "")
        out.append({"version": m.group(1), "text": mdlib.plain_text(line)[:160]})
    return out

def home_body(ctx: Context, all_pages: list[dict]) -> str:
    home = ctx.config.get("home", {})
    badge = ' <span class="badge">%s%s</span>' % (esc(ctx.version), (" · " + esc(ctx.commit)) if ctx.commit and ctx.version else "") if ctx.version else ""
    out = ['<main><header class="hero"><div class="page-eyebrow">%s%s</div>' % (esc(ctx.name), badge)]
    out.append("<h1>%s</h1>" % both_tr(ctx, home.get("title", {"en": "Documentation", "fr": "Documentation"})))
    if home.get("tagline"):
        out.append('<p class="lead">%s</p>' % both_tr(ctx, home["tagline"]))
    out.append('<div class="sbox">%s<input type="search" autocomplete="off" aria-label="Search" data-i18n-aria-label placeholder="Search the documentation…  ( / )" '
               'data-ph-fr="Rechercher dans la documentation…  ( / )"><ul class="results" hidden></ul></div></header>' % SEARCH_SVG)
    out.append('<div class="grid">')
    for sec in ctx.config.get("sections", []):
        mine = [p for p in all_pages if p["section"] == sec["id"]]
        if not mine:
            continue
        count = '<span class="count">%d <span data-i18n>pages</span></span>' % len(mine) if len(mine) > 1 else ""
        out.append('<a class="tile" href="%s.html">%s%s<h2>%s</h2><p>%s</p></a>' % (
            mine[0]["slug"], count, TILE_SVG % sec.get("icon", DEFAULT_ICON), both_tr(ctx, sec["title"]), both_tr(ctx, sec.get("desc", ""))))
    out.append("</div>")
    changes = changelog(ctx)
    if changes:
        out.append('<section id="changelog"><h2 data-i18n>Changelog</h2><ul class="cl">')
        for c in changes[:5]:
            out.append("<li><b>%s</b><span>%s</span></li>" % (esc(c["version"]), esc(c["text"])))
        out.append("</ul></section>")
    links = home.get("links", [])
    if links:
        out.append('<div class="links">%s</div>' % "".join('<a href="%s">%s</a>' % (esc(link["href"]), both_tr(ctx, link["text"])) for link in links))
    out.append("</main>")
    return "".join(out)

def make_page(ctx: Context, body: str, out: Path, title: str, kind: str = "", level: str = "", wide: bool = False) -> None:
    site = ctx.config["site"]
    known = kinds(ctx)
    k = known.get(kind, {}) if kind else {}
    actions = (STYLE / "search.html").read_text(encoding="utf-8")
    if k.get("actions"):
        actions = resolve(ctx, k["actions"]).read_text(encoding="utf-8") + actions
    if site.get("repo"):
        actions += REPO_LINK % esc(site["repo"])
    with tempfile.TemporaryDirectory() as folder:
        tmp_body, tmp_actions = Path(folder) / "body.html", Path(folder) / "actions.html"
        tmp_body.write_text(body, encoding="utf-8")
        tmp_actions.write_text(actions, encoding="utf-8")
        footer = site.get("footer", "") or " · ".join(x for x in (ctx.name + (" " + ctx.version if ctx.version else ""), ctx.commit, "docs") if x)
        cmd = [sys.executable, str(NEW_PAGE), str(tmp_body), "-o", str(out), "--force",
               "--css", str(STYLE / "site.css"), *[str(resolve(ctx, c)) for c in site.get("css", [])],
               "--js", str(STYLE / "site.js"), *[str(resolve(ctx, j)) for j in site.get("js", [])],
               "--actions", str(tmp_actions),
               "--set", "PROJECT=%s" % ctx.name, "TITLE=%s" % title, "STORAGE_KEY=%s" % site.get("storage_key", re.sub(r"\W+", "-", ctx.name.lower()) + "-docs"),
               "FOOTER=%s" % footer, "VERSION=%s" % ctx.version]
        if len(ctx.langs) > 1:
            cmd.append("--lang-switch")
        attrs = ["%s=%s" % (n, v) for n, v in k.get("attrs", {}).items()]
        if level:
            attrs.append("data-default-level=%s" % level)
        if attrs:
            cmd += ["--attr", *attrs]
        run = subprocess.run(cmd, capture_output=True, text=True)
        if run.returncode != 0:
            raise SystemExit("%s: new_page.py failed: %s%s" % (out.name, run.stdout, run.stderr))
    text = out.read_text(encoding="utf-8")
    logo = site.get("logo", "")
    if logo:
        svg = resolve(ctx, logo).read_text(encoding="utf-8")
        svg = re.sub(r"<\?xml.*?\?>|<!--.*?-->", "", svg, flags=re.S).strip()
        svg = svg if 'class="logo"' in svg else svg.replace("<svg", '<svg class="logo" aria-hidden="true"', 1)
        text = re.sub(r'<svg class="logo".*?</svg>', lambda m: svg, text, count=1, flags=re.S)
    if wide:
        text = text.replace('<div class="layout" id="top">', '<div class="layout wide" id="top">', 1)
    text = text.replace('<a class="brand" href="#top">', '<a class="brand" href="index.html">', 1)
    if logo:
        text = text.replace("</title>", '</title><link rel="icon" type="image/svg+xml" href="logo.svg">', 1)
    if site.get("search_endpoint"):
        text = text.replace("</title>", '</title><meta name="docs-search" content="%s">' % esc(site["search_endpoint"]), 1)
    if k:  # the assets of the kind: a style / script block of their own (the shared ones stay identical on every page)
        css = "\n".join(resolve(ctx, c).read_text(encoding="utf-8") for c in k.get("css", []))
        js = "\n".join(resolve(ctx, j).read_text(encoding="utf-8") for j in k.get("js", []))
        if css:
            text = text.replace("</head>", '<style data-kind="%s">\n%s\n</style>\n</head>' % (kind, css), 1)
        if js:
            text = text.replace("</body>", '<script data-kind="%s">\n%s\n</script>\n</body>' % (kind, js), 1)
    text = text.replace("</body>", '<script src="search-index.js" defer></script>\n</body>', 1)
    out.write_text(text, encoding="utf-8")

# ----- search index -----
def build_index(ctx: Context, all_pages: list[dict], extra: list[dict]) -> list[dict]:
    entries: list[dict] = []
    known = kinds(ctx)
    for p in all_pages:
        kind = p["kind"] or "docs"
        for lang in ctx.langs:
            d = p[lang]
            label = ctx.tr(known[p["kind"]]["label"], lang) if p["kind"] else ""
            entries.append({"k": kind, "l": lang, "t": d["title"], "d": d["summary"] or label, "u": "%s.html" % p["slug"], "x": (d["keywords"] + " " + d["lead_text"])[:300]})
            for s in d["sections"]:
                entries.append({"k": kind, "l": lang, "t": "%s › %s" % (d["title"], s["raw"]), "d": s["text"][:110], "u": "%s.html#%s" % (p["slug"], s["id"]), "x": s["text"][:600]})
    entries += extra
    if len(ctx.langs) == 1:
        for e in entries:
            e.pop("l", None)
    return entries

def split_assets(ctx: Context, names: list[str]) -> None:
    """--split: the style and the scripts every page repeats move to site.css / site.js (the pages stay valid on file://).
    Only the pages of this build: an other page of the folder (the graph.html of html-doc, an app) keeps its own style."""
    style = script = ""
    for path in sorted(ctx.out / name for name in names):
        text = path.read_text(encoding="utf-8")
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
        path.write_text(text, encoding="utf-8")
    (ctx.out / "site.css").write_text(style, encoding="utf-8")
    (ctx.out / "site.js").write_text(script, encoding="utf-8")

def write_site(ctx: Context) -> None:
    if not NEW_PAGE.exists() or not (STYLE / "site.css").exists():
        raise SystemExit("the skills html-style and better-html-style must sit next to better-html-doc (%s)" % SKILLS)
    ctx.out.mkdir(parents=True, exist_ok=True)
    pages, extra = load_generated_pages(ctx)
    static = load_static_pages(ctx)
    for slug in static:
        if slug in pages:
            raise SystemExit("page %s exists as source and as generated page" % slug)
    pages.update(static)
    if not pages:
        raise SystemExit("no page: write src/pages/<slug>.<lang>.md")
    for p in pages.values():
        parse_page(ctx, p)
    all_pages = ordered(ctx, pages)
    for p in all_pages:
        title = "%s - %s" % (p[ctx.langs[0]]["title"], ctx.name)
        make_page(ctx, page_body(ctx, p, all_pages), ctx.out / ("%s.html" % p["slug"]), title, p["kind"], p["level"])
    make_page(ctx, home_body(ctx, all_pages), ctx.out / "index.html", "%s documentation" % ctx.name, wide=True)
    payload = {"version": ctx.version, "commit": ctx.commit, "entries": build_index(ctx, all_pages, extra)}
    dumped = json.dumps(payload, ensure_ascii=False, separators=(",", ":"), sort_keys=True)
    (ctx.out / "search-index.json").write_text(dumped, encoding="utf-8")
    (ctx.out / "search-index.js").write_text("window.DOCS_INDEX=" + dumped + ";", encoding="utf-8")
    site = ctx.config["site"]
    if site.get("logo"):
        (ctx.out / "logo.svg").write_text(resolve(ctx, site["logo"]).read_text(encoding="utf-8"), encoding="utf-8")
    if ctx.config.get("contexts"):
        (ctx.out / "docs-map.json").write_text(json.dumps(ctx.config["contexts"], indent=1, sort_keys=True), encoding="utf-8")
    if site.get("pages_url"):
        urls = ["index.html"] + ["%s.html" % p["slug"] for p in all_pages]
        (ctx.out / "sitemap.xml").write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
                                             + "".join("<url><loc>%s%s</loc></url>\n" % (site["pages_url"], u) for u in urls) + "</urlset>\n", encoding="utf-8")
    for name in site.get("copy", []):  # files given as they are (downloads, images)
        source = resolve(ctx, name)
        target = ctx.out / source.name
        target.write_bytes(source.read_bytes())
    if ctx.split:
        split_assets(ctx, ["index.html"] + ["%s.html" % p["slug"] for p in all_pages])
    print("built %d pages + home + index in %s (%s%s)" % (len(all_pages), ctx.out, ctx.name, (" " + ctx.version) if ctx.version else ""))

def main() -> None:
    ap = argparse.ArgumentParser(description="Generator of a documentation site in the better-html-style")
    ap.add_argument("--site", default=".", help="the folder of site.toml (default: the current one)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--version", default="", help="shown in the top bar and on the home (default: [site] version, else the last git tag)")
    ap.add_argument("--commit", default="")
    ap.add_argument("--split", action="store_true", help="the shared style / script in site.css / site.js (smaller pages)")
    write_site(Context(ap.parse_args()))

if __name__ == "__main__":
    main()
