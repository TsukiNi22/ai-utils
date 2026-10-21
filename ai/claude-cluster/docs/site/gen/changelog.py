#!/usr/bin/env python3
# =============================================================================
# changelog.py - the changelog (per version, from the git log of the claude-cluster folder), the versions page (build
#                info, requirements, version -> commit table) and the downloads page
# =============================================================================

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import _data  # noqa: E402


def series(version: str) -> str:
    parts = version.split(".")
    return ".".join(parts[:2]) if len(parts) >= 2 and version != "next" else "next"


def ident(version: str) -> str:
    return "v" + re.sub(r"[^0-9a-z]+", "-", version)


def changelog_md(ctx, lang: str) -> str:
    en = lang == "en"
    versions = _data.changes(ctx)
    out = ["Every released version of claude-cluster with its changes, newest first. The entries are the commit messages of the repository, so they are in English; the hash after each one is the commit.\n" if en else
           "Chaque version publiée de claude-cluster avec ses changements, de la plus récente à la plus ancienne. Les entrées sont les messages de commit du dépôt, donc en anglais ; le hash après chacune est le commit.\n"]
    groups: dict[str, list[dict]] = {}
    for v in versions:
        groups.setdefault(series(v["version"]), []).append(v)
    for ser, items in groups.items():
        title = ("Series %s" % ser) if ser != "next" else ("Not released yet" if en else "Pas encore publié")
        if ser != "next" and not en:
            title = "Série %s" % ser
        out.append("## %s {#s-%s}\n" % (title, ser.replace(".", "-")))
        for v in items:
            label = v["version"] if v["version"] != "next" else ("Next" if en else "Prochaine")
            out.append("### %s {#%s}\n" % (label, ident(v["version"])))
            out.append("*%s*\n" % v["date"])
            for c in v["commits"]:
                text = c["text"].replace("|", "\\|")
                out.append("- %s (`%s`)" % (text, c["hash"]))
            out.append("")
    return "\n".join(out)


def versions_md(ctx, lang: str) -> str:
    en = lang == "en"
    versions = _data.changes(ctx)
    deps = [("libutils", "4.0.0"), ("nlohmann/json", "3.10"), ("toml++", "3.0"), ("FTXUI", "6.1.9"), ("Qt6 Widgets", "6.2 (%s)" % ("optional" if en else "optionnel")),
            ("OpenSSL", "%s" % ("optional" if en else "optionnel")), ("CMake", "3.20"), ("C++", "20")]
    out = ["## %s {#build}\n" % ("This documentation" if en else "Cette documentation")]
    out.append("| | |\n|---|---|")
    out.append("| claude-cluster | `%s` |" % ctx.version)
    out.append("| commit | `%s` |" % ctx.commit)
    out.append("| %s | %s |" % ("commit date" if en else "date du commit", ctx.date))
    out.append("")
    out.append("%s\n" % ("The version and commit shown at the bottom of every page are those of the binary serving it (or, on GitHub Pages, of the build that generated it)." if en else
                         "La version et le commit affichés en bas de chaque page sont ceux du binaire qui la sert (ou, sur GitHub Pages, du build qui l'a générée)."))
    out.append("## %s {#requirements}\n" % ("Requirements" if en else "Prérequis"))
    out.append("| %s | %s |\n|---|---|" % ("Component" if en else "Composant", "Version"))
    for name, ver in deps:
        out.append("| %s | %s |" % (name, ver))
    out.append("")
    out.append("## %s {#history}\n" % ("Version to commit" if en else "Version et commit"))
    out.append("| %s | %s | %s |\n|---|---|---|" % ("Version", "Date", "Commits"))
    for v in versions:
        hashes = ", ".join("`%s`" % c["hash"] for c in v["commits"][:6]) + (" ..." if len(v["commits"]) > 6 else "")
        out.append("| %s | %s | %s |" % (v["version"], v["date"], hashes))
    return "\n".join(out)


def downloads_md(ctx, lang: str) -> str:
    en = lang == "en"
    rows = _data.downloads(ctx)
    out = []
    out.append(("What can be downloaded today, with the version and the size read from the build. There is no release page yet: claude-cluster is installed from the repository with its install script, and the phone app is built on your own computer and served by it.\n" if en else
                "Ce qui peut être téléchargé aujourd'hui, avec la version et la taille lues dans le build. Il n'y a pas encore de page de release : claude-cluster s'installe depuis le dépôt avec son script, et l'application téléphone est compilée sur votre propre ordinateur et servie par lui.\n"))
    out.append("| %s | %s | %s |\n|---|---|---|" % ("File" if en else "Fichier", "Version", "Size" if en else "Taille"))
    for r in rows:
        out.append("| [%s](%s) | %s | %s |" % (r["name"], r["href"], r["version"], r["size"]))
    out.append("")
    out.append("## %s {#prompts}\n" % ("System prompts" if en else "Prompts système"))
    out.append("The default prompts, taken from `templates/` of the build named in the Version column: [global.md](downloads/global.md), [sessions.md](downloads/sessions.md), [rendering.md](downloads/rendering.md). See [System prompts](page:system-prompts).\n" if en else
               "Les prompts par défaut, pris dans `templates/` du build nommé dans la colonne Version : [global.md](downloads/global.md), [sessions.md](downloads/sessions.md), [rendering.md](downloads/rendering.md). Voir [Prompts système](page:system-prompts).\n")
    out.append("## %s {#script}\n" % ("Install script" if en else "Script d'installation"))
    out.append("`setup.sh` builds and installs claude-cluster for your user (see [Install](page:install)):\n" if en else "`setup.sh` compile et installe claude-cluster pour votre utilisateur (voir [Installation](page:install)) :\n")
    out.append("```bash\ncurl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install claude-cluster\n```\n")
    out.append("## %s {#apk}\n" % ("Android app" if en else "Application Android"))
    out.append("The APK is **built on your computer** by the install script when the Android SDK is present, signed with your own key, and served by claude-cluster itself at `/app.apk` once the web access is open. It is never downloaded from a third party, so its version is always the one of your claude-cluster. See [Notifications and the phone app](page:notifications-apk).\n" if en else
               "L'APK est **compilé sur votre ordinateur** par le script d'installation quand le SDK Android est présent, signé avec votre propre clé, et servi par claude-cluster lui-même sur `/app.apk` dès que l'accès web est ouvert. Il n'est jamais téléchargé chez un tiers, donc sa version est toujours celle de votre claude-cluster. Voir [Notifications et application téléphone](page:notifications-apk).\n")
    out.append("## %s {#packages}\n" % ("RPM and DEB packages" if en else "Paquets RPM et DEB"))
    out.append("> [!SOON]\n> RPM and DEB packages are not published yet.\n" if en else "> [!SOON]\n> Les paquets RPM et DEB ne sont pas encore publiés.\n")
    out.append("## %s {#audit}\n" % ("Audit report" if en else "Rapport d'audit"))
    out.append("The security audit report is a PDF written from reading the code: [claude-cluster-audit-report.pdf](downloads/claude-cluster-audit-report.pdf). See [Security audit](page:audit-report).\n" if en else
               "Le rapport d'audit de sécurité est un PDF écrit à partir de la lecture du code : [claude-cluster-audit-report.pdf](downloads/claude-cluster-audit-report.pdf). Voir [Audit de sécurité](page:audit-report).\n")
    out.append("## %s {#source}\n" % ("Source" if en else "Sources"))
    out.append("The sources are in the `ai/claude-cluster` folder of [the repository](%s); build them as described in [Install](page:install)." % ctx.config["site"]["repo"] if en else
               "Les sources sont dans le dossier `ai/claude-cluster` du [dépôt](%s) ; compilez-les comme décrit dans [Installation](page:install)." % ctx.config["site"]["repo"])
    return "\n".join(out)


def pages(ctx) -> list[dict]:
    return [
        {"slug": "changelog", "section": "resources", "order": 20,
         "en": {"title": "Changelog", "summary": "What changed in each version, newest first - generated from the git history.", "keywords": "changelog versions history release notes commits", "md": changelog_md(ctx, "en")},
         "fr": {"title": "Historique des versions", "summary": "Ce qui a changé dans chaque version, de la plus récente à la plus ancienne - généré depuis l'historique git.", "keywords": "changelog historique versions notes de version commits", "md": changelog_md(ctx, "fr")}},
        {"slug": "versions", "section": "reference", "order": 70,
         "en": {"title": "Versions and commits", "summary": "The version and commit of this documentation, the requirements, and the commit of each version.", "keywords": "versions commit hash requirements build", "md": versions_md(ctx, "en")},
         "fr": {"title": "Versions et commits", "summary": "La version et le commit de cette documentation, les prérequis, et le commit de chaque version.", "keywords": "versions commit hash prérequis build", "md": versions_md(ctx, "fr")}},
        {"slug": "downloads", "section": "resources", "order": 10,
         "en": {"title": "Downloads", "summary": "The install script, the Android app, the audit report - with versions and sizes.", "keywords": "downloads install script apk rpm deb audit pdf", "md": downloads_md(ctx, "en")},
         "fr": {"title": "Téléchargements", "summary": "Le script d'installation, l'application Android, le rapport d'audit - avec versions et tailles.", "keywords": "téléchargements script installation apk rpm deb audit pdf", "md": downloads_md(ctx, "fr")}},
    ]
