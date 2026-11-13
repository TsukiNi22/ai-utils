#!/usr/bin/env python3
# =============================================================================
# glossary.py - the glossary page and its search entries, from src/glossary.toml
# =============================================================================

import re
import tomllib
from pathlib import Path

SRC = Path(__file__).resolve().parents[1] / "src" / "glossary.toml"


def terms() -> list[dict]:
    return sorted(tomllib.loads(SRC.read_text())["term"], key=lambda t: t["name"].lower())


def slug(name: str) -> str:
    return "t-" + re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def render(lang: str) -> str:
    en = lang == "en"
    out = ["The words of claude-cluster, in alphabetical order.\n" if en else "Les mots de claude-cluster, par ordre alphabétique.\n"]
    letters: dict[str, list[dict]] = {}
    for t in terms():
        letters.setdefault(t["name"][0].upper(), []).append(t)
    for letter, items in letters.items():
        out.append("## %s {#l-%s}\n" % (letter, letter.lower()))
        out.append("| %s | %s |\n|---|---|" % ("Term" if en else "Terme", "Meaning" if en else "Sens"))
        for t in items:
            out.append("| **%s** | %s |" % (t["name"], t[lang]))
        out.append("")
    return "\n".join(out)


def pages(ctx) -> list[dict]:
    return [{"slug": "glossary", "section": "resources", "order": 40,
             "en": {"title": "Glossary", "summary": "The vocabulary of claude-cluster: sessions, taxonomy, orchestrator, web access...", "keywords": "glossary vocabulary terms definitions", "md": render("en")},
             "fr": {"title": "Glossaire", "summary": "Le vocabulaire de claude-cluster : sessions, taxonomie, orchestrateur, accès web...", "keywords": "glossaire vocabulaire termes définitions", "md": render("fr")}}]


def search_entries(ctx) -> list[dict]:
    out = []
    for lang in ("en", "fr"):
        for t in terms():
            out.append({"k": "glossary", "l": lang, "t": t["name"], "d": t[lang][:120], "u": "glossary.html#l-%s" % t["name"][0].lower(), "x": ""})
    return out
