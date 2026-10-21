#!/usr/bin/env python3
# =============================================================================
# config.py - the reference of config.toml, generated from templates/config.toml (defaults and comments of the file)
# =============================================================================

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import _toml  # noqa: E402
from settings import SECTION_TITLES  # noqa: E402


def cell(text: str) -> str:
    return re.sub(r"\s+", " ", text.replace("|", "\\|")).strip()


def load(ctx) -> list[dict]:
    return _toml.parse((ctx.code_root / "templates" / "config.toml").read_text())


def render(sections: list[dict], lang: str) -> str:
    en = lang == "en"
    out = ["`~/.config/claude-cluster/config.toml` is written, commented, on the first run and reloaded live when saved. Every key is optional: a missing key keeps its default. This page is generated from `templates/config.toml`: the defaults and the comments are those of the file. The same options with their labels are in the [settings reference](page:settings-reference); the key bindings are in the [keys reference](page:keys-reference).\n" if en else
           "`~/.config/claude-cluster/config.toml` est écrit, commenté, au premier lancement et rechargé à chaud quand il est sauvegardé. Chaque clé est optionnelle : une clé absente garde sa valeur par défaut. Cette page est générée depuis `templates/config.toml` : les valeurs par défaut et les commentaires sont ceux du fichier. Les mêmes options avec leurs libellés sont dans la [référence des réglages](page:settings-reference) ; les touches sont dans la [référence des touches](page:keys-reference).\n"]
    if not en:
        out.append("> [!NOTE]\n> Les commentaires viennent du fichier, en anglais.\n")
    env = ("Environment variables", "Variables d'environnement")
    for sec in sections:
        if sec["section"] == "keys" or not sec["keys"]:
            continue
        title = SECTION_TITLES.get(sec["section"].split(".")[0], (sec["section"], sec["section"]))[0 if en else 1]
        out.append("## [%s] %s {#c-%s}\n" % (sec["section"], "- " + title if title.lower() != sec["section"] else "", sec["section"].replace("_", "-").replace(".", "-")))
        if sec["comment"]:
            out.append(cell(sec["comment"]) + "\n")
        out.append("| %s | %s | %s |" % (("Key", "Default", "Description") if en else ("Clé", "Défaut", "Description")))
        out.append("|---|---|---|")
        for k in sec["keys"]:
            out.append("| `%s` | `%s` | %s |" % (k["key"], cell(k["default"]) or '""', cell(k["desc"])))
        out.append("")
    out.append("## %s {#env}\n" % env[0 if en else 1])
    rows = [("CLAUDE_CLUSTER_CONFIG_DIR", "config folder (default `~/.config/claude-cluster`)", "dossier de configuration (défaut `~/.config/claude-cluster`)"),
            ("CLAUDE_CLUSTER_DATA_DIR", "state, trash, history (default `~/.local/share/claude-cluster`)", "état, corbeille, historique (défaut `~/.local/share/claude-cluster`)"),
            ("CLAUDE_CLUSTER_SOCKET", "control socket (default `$XDG_RUNTIME_DIR/claude-cluster/control.sock`)", "socket de contrôle (défaut `$XDG_RUNTIME_DIR/claude-cluster/control.sock`)"),
            ("CLAUDE_CLUSTER_BACKEND", "same as `--backend`", "comme `--backend`"),
            ("CLAUDE_CLUSTER_RTK", "same as `--rtk`", "comme `--rtk`"),
            ("CLAUDE_CLUSTER_TRANSCRIPTS", "folders of the transcripts read for the usage, separated by `:` (empty: none)", "dossiers des transcriptions lus pour l'usage, séparés par `:` (vide : aucun)")]
    out.append("| %s | %s |" % (("Variable", "Effect") if en else ("Variable", "Effet")))
    out.append("|---|---|")
    for name, e, f in rows:
        out.append("| `%s` | %s |" % (name, e if en else f))
    return "\n".join(out)


def pages(ctx) -> list[dict]:
    sections = load(ctx)
    return [{"slug": "config-reference", "section": "reference", "order": 30,
             "en": {"title": "Configuration reference", "summary": "Every section and key of config.toml with its default - generated from the template shipped in the binary.", "keywords": "config.toml configuration defaults toml section key environment variables", "md": render(sections, "en")},
             "fr": {"title": "Référence de configuration", "summary": "Chaque section et clé de config.toml avec son défaut - générée depuis le modèle embarqué dans le binaire.", "keywords": "config.toml configuration défauts toml section clé variables d'environnement", "md": render(sections, "fr")}}]
