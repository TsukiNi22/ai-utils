#!/usr/bin/env python3
# =============================================================================
# settings.py - the page of the settings (F9), generated from the Setting tables of the sources: section, key, label,
#               kind, choices, description, dangerous / applied at the next start; also feeds the search index
# =============================================================================

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import _cpp  # noqa: E402
from mcp import text_of  # noqa: E402

SECTION_TITLES = {
    "ui": ("Interface", "Interface"), "agent": ("Agent", "Agent"), "commands": ("Commands", "Commandes"), "global": ("Global session", "Session globale"),
    "sessions": ("Sessions", "Sessions"), "voice": ("Voice", "Voix"), "keys": ("Keys", "Touches"), "web": ("Web access", "Accès web"), "attachments": ("Attachments", "Pièces jointes"),
    "mcp": ("MCP", "MCP"), "mcp_tools": ("MCP tools", "Outils MCP"), "user": ("User", "Utilisateur"), "service": ("Service", "Service"), "orchestrator": ("Orchestrator", "Orchestrateur"),
    "plugins": ("Plugins", "Plugins"), "limits": ("Accounts and limits", "Comptes et limites"), "policy": ("Model policy", "Politique des modèles"), "notify": ("Notifications", "Notifications"),
    "services": ("Services", "Services"), "usage": ("Usage", "Usage"), "debug": ("Debug", "Débogage"), "styles": ("Styles", "Styles"), "models": ("Models", "Modèles"),
    "accounts": ("Accounts and limits", "Comptes et limites"), "device": ("This device", "Cet appareil"),
}
HELPERS = {"flag_": "bool", "count_": "int", "list_": "list"}


def entries(ctx) -> list[dict]:
    root = ctx.code_root / "src" / "cluster"
    out: list[dict] = []
    for name in ("Settings.cpp", "Orchestrator-Settings.cpp"):
        text = (root / name).read_text()
        for item in _cpp.brace_lists(text):
            kind = item[3][1].split("::")[-1].lower() if isinstance(item[3], tuple) else ""
            if kind not in ("bool", "int", "float", "text", "choice", "list", "action"):
                continue
            choices = [c for c in item[4] if isinstance(c, str)] if len(item) > 4 and isinstance(item[4], list) else []
            desc = text_of(item[5]) if len(item) > 5 else ""
            danger = len(item) > 7 and item[7] == ("id", "true") or (len(item) > 6 and item[6] == ("id", "true") and False)
            restart = len(item) > 6 and item[6] == ("id", "true")
            out.append({"section": item[0], "key": item[1], "label": item[2], "kind": kind, "choices": choices, "desc": desc, "restart": restart})
        if name == "Orchestrator-Settings.cpp":
            for fn, kind in HELPERS.items():
                for call in _cpp.find_calls(text, fn):
                    if len(call) >= 3 and all(isinstance(c, str) for c in call[:3]):
                        out.append({"section": "orchestrator", "key": call[0], "label": call[1], "kind": kind, "choices": [], "desc": call[2], "restart": False})
    seen: set[tuple[str, str]] = set()
    unique = []
    for e in out:
        k = (e["section"], e["key"])
        if k not in seen and e["key"]:
            seen.add(k)
            unique.append(e)
    return unique


def cell(text: str) -> str:
    return re.sub(r"\s+", " ", text.replace("|", "\\|")).strip()


def render(items: list[dict], lang: str) -> str:
    en = lang == "en"
    out = ["Every option of the settings page (`F9`, the palette *Settings*, the web page ⚙) with its key in `config.toml`, generated from the settings tables of the sources. The default values and the comments of the file are in the [configuration reference](page:config-reference). Settings applied only at the next start are marked ↻.\n" if en else
           "Chaque option de la page de réglages (`F9`, la palette *Settings*, le ⚙ de la page web) avec sa clé dans `config.toml`, générée depuis les tables de réglages du code. Les valeurs par défaut et les commentaires du fichier sont dans la [référence de configuration](page:config-reference). Les réglages pris en compte seulement au prochain démarrage sont marqués ↻.\n"]
    if not en:
        out.append("> [!NOTE]\n> Les libellés et aides viennent du code, en anglais.\n")
    sections: dict[str, list[dict]] = {}
    for e in items:
        sections.setdefault(e["section"], []).append(e)
    for sec, rows in sections.items():
        title = SECTION_TITLES.get(sec, (sec.capitalize(), sec.capitalize()))[0 if en else 1]
        out.append("## %s {#s-%s}\n" % (title, sec.replace("_", "-")))
        out.append("| %s | %s | %s |" % (("Setting", "Type", "What it does") if en else ("Réglage", "Type", "Rôle")))
        out.append("|---|---|---|")
        for e in rows:
            kind = e["kind"] + ((": " + " \\| ".join(e["choices"])) if e["choices"] else "")
            out.append("| **%s**<br>`%s.%s`%s | %s | %s |" % (cell(e["label"]), sec, e["key"], " ↻" if e["restart"] else "", cell(kind), cell(e["desc"])))
        out.append("")
    return "\n".join(out)


def pages(ctx) -> list[dict]:
    items = entries(ctx)
    return [{"slug": "settings-reference", "section": "reference", "order": 35,
             "en": {"title": "Settings reference", "summary": "Every option of the settings page, with its key, kind, choices and help - generated from the code.", "keywords": "settings f9 options keys config.toml", "md": render(items, "en")},
             "fr": {"title": "Référence des réglages", "summary": "Chaque option de la page de réglages, avec sa clé, son type, ses choix et son aide - générée depuis le code.", "keywords": "réglages settings f9 options clés config.toml", "md": render(items, "fr")}}]


def search_entries(ctx) -> list[dict]:
    return [{"k": "settings", "t": "%s.%s" % (e["section"], e["key"]), "d": "%s - %s" % (e["label"], e["desc"][:100]), "u": "settings-reference.html#s-%s" % e["section"].replace("_", "-"),
             "x": e["label"]} for e in entries(ctx)]
