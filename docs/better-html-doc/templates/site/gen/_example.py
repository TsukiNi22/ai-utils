"""A generator of pages read from the code (rename it without the leading _ to run it): every gen/*.py with pages(ctx) is called by
build_site.py; ctx.site / ctx.config / ctx.langs / ctx.version / ctx.tr() are there. A page is a dict like a Markdown page: slug, section,
order, kind ("" / "explain" / "protocol"), and one entry per language with title, summary, md, keywords. search_entries(ctx) may add
search results that are not pages (a setting, a command): {"k": kind shown, "l": lang, "t": title, "d": description, "u": url, "x": text}."""
import tomllib


def pages(ctx) -> list[dict]:
    options = tomllib.loads((ctx.site / ".." / ".." / "config.default.toml").read_text())   # read from the project, never typed again
    rows = "\n".join("| `%s` | `%s` |" % (key, value) for key, value in sorted(options.items()))
    page = {"slug": "options", "section": "reference", "order": 50, "kind": ""}
    for lang in ctx.langs:
        title = "Options" if lang == "en" else "Options"
        page[lang] = {"title": title, "summary": "", "keywords": "options",
                      "md": "## %s {#all}\n\n| Key | Default |\n|---|---|\n%s\n" % ("Every option" if lang == "en" else "Toutes les options", rows)}
    return [page]


def search_entries(ctx) -> list[dict]:
    return [{"k": "config", "l": lang, "t": "verbose", "d": "more output", "u": "options.html#all", "x": ""} for lang in ctx.langs]
