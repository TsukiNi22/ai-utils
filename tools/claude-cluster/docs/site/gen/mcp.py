#!/usr/bin/env python3
# =============================================================================
# mcp.py - the page of the MCP tools of the global session, generated from the tool schemas of the sources
#          (Mcp.cpp and Orchestrator-Control.cpp: name, description, parameters, required ones)
# =============================================================================

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import _cpp  # noqa: E402

SCALARS = {"str_": "string", "int_": "integer", "bool_": "boolean", "obj_": "object", "strs_": "string[]",
           "str": "string", "num": "integer", "flag": "boolean"}

GROUPS = [
    ("sessions", "Sessions", "Sessions", ["session_list", "session_status", "session_read", "session_spawn", "session_send", "session_wait", "session_allow", "session_set_mode",
        "session_set_model", "session_interrupt", "session_close", "session_restore", "session_trash", "session_purge", "session_rename", "session_continue", "session_describe",
        "session_cd", "session_fanout", "group_status", "group_cancel", "session_fork", "session_import", "external_conversations", "session_context", "session_transcript",
        "session_messages", "session_edit", "search", "export", "taxonomy"]),
    ("tasks", "Tasks, schedules and notifications", "Tâches, planifications et notifications", ["task_add", "task_list", "task_act", "task_cancel", "schedule_add", "schedule_list",
        "schedule_edit", "schedule_act", "schedule_cancel", "notify"]),
    ("displays", "Displays and pins", "Affichages et épingles", ["session_display", "session_display_update", "session_display_get", "session_pin", "session_unpin", "session_pin_get"]),
    ("orchestrator", "Orchestrator and heavy jobs", "Orchestrateur et travaux lourds", ["heavy_run", "jobs_add", "jobs_list", "job_status", "job_wait", "job_kill", "job_stop", "job_resume",
        "job_prio", "hardware_scan", "orchestrator_config"]),
    ("interface", "Interface", "Interface", ["ui_state", "ui_focus", "ui_layout", "ui_style", "ui_panel"]),
    ("settings", "Settings, policy and the computer", "Réglages, politique et ordinateur", ["settings_get", "settings_set", "config_files", "config_read", "config_write", "cluster_info",
        "models_policy", "work_method", "usage", "providers", "voice_engines", "web_access", "remote_control", "machine_sync", "docs"]),
]
FIRST = {"sessions": ("Sessions", "Sessions")}


def text_of(value) -> str:
    """The text of a description expression: its strings, a call of the sources named for what it is."""
    if isinstance(value, str):
        return value
    if isinstance(value, tuple) and value[0] == "call" and value[1].endswith("display_help"):
        return "(the catalogue of display types: see Rich displays)"
    if isinstance(value, list):
        return "".join(text_of(v) for v in value if isinstance(v, (str, list)) or (isinstance(v, tuple) and v[0] == "call" and v[1].endswith("display_help")))
    return ""


def schema_of(value, defs: dict) -> tuple[str, str]:
    """(type, description) of a property value: str_("...") helpers, a {{"type", ..}, {"description", ..}} object, a shared constant."""
    if isinstance(value, tuple) and value[0] == "id":
        return schema_of(defs[value[1]], defs) if value[1] in defs else ("", "")
    if isinstance(value, tuple) and value[0] == "call":
        fn = value[1].split("::")[-1]
        if fn in SCALARS:
            return SCALARS[fn], text_of(value[2][0]) if value[2] else ""
        return "", ""
    if isinstance(value, list):
        kv: dict = {}
        for pair in value:
            if isinstance(pair, list) and len(pair) == 2 and isinstance(pair[0], str):
                kv[pair[0]] = pair[1]
        kind = text_of(kv.get("type", ""))
        if kind == "array":
            items = kv.get("items")
            inner = ""
            if isinstance(items, list):
                for pair in items:
                    if isinstance(pair, list) and len(pair) == 2 and pair[0] == "type":
                        inner = text_of(pair[1])
            kind = (inner or "any") + "[]"
        return kind, text_of(kv.get("description", ""))
    return "", ""


def properties(value, defs: dict) -> list[dict]:
    if isinstance(value, tuple) and value[0] == "id":
        return properties(defs.get(value[1], []), defs)
    out: list[dict] = []
    if not isinstance(value, list):
        return out
    for pair in value:
        if isinstance(pair, list) and len(pair) == 2 and isinstance(pair[0], str):
            kind, desc = schema_of(pair[1], defs)
            out.append({"name": pair[0], "type": kind, "desc": desc})
    return out


def orchestrator_defs(src: str, defs: dict) -> dict:
    """run / add = common + the keys assigned to them (run["timeout"] = num("...");)."""
    out = dict(defs)
    for name, extra in (("run", "run"), ("add", "add")):
        base = list(out.get("common", []))
        for key, fn, arg in re.findall(r'\b%s\["(\w+)"\]\s*=\s*(\w+)\(("(?:[^"\\]|\\.)*")\)' % extra, src):
            base.append([key, ("call", fn, [_cpp.unescape(arg)])])
        out[name] = base
    return out


def load_tools(ctx) -> list[dict]:
    root = ctx.code_root / "src" / "cluster"
    mcp_src = (root / "Mcp.cpp").read_text()
    orch_src = (root / "Orchestrator-Control.cpp").read_text()
    tools: list[dict] = []
    mcp_defs = _cpp.definitions(mcp_src)
    for call in _cpp.find_calls(mcp_src, "tool_"):
        if len(call) == 4 and isinstance(call[0], str):
            tools.append({"name": call[0], "desc": text_of(call[1]), "props": properties(call[2], mcp_defs), "required": [r for r in call[3] if isinstance(r, str)] if isinstance(call[3], list) else []})
    orch_defs = orchestrator_defs(orch_src, _cpp.definitions(orch_src))
    for call in _cpp.find_calls(orch_src, "tool"):
        if len(call) == 4 and isinstance(call[0], str) and call[0] in dict((n, 1) for g in GROUPS for n in g[3]):
            tools.append({"name": call[0], "desc": text_of(call[1]), "props": properties(call[2], orch_defs), "required": [r for r in call[3] if isinstance(r, str)] if isinstance(call[3], list) else []})
    seen: set[str] = set()
    unique = []
    for t in tools:
        if t["name"] not in seen:
            seen.add(t["name"])
            unique.append(t)
    return unique


def md_cell(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ")


def render(tools: list[dict], lang: str) -> str:
    by_name = {t["name"]: t for t in tools}
    placed: set[str] = set()
    out: list[str] = []
    if lang == "en":
        out.append("The global session drives claude-cluster through the MCP server `claude-cluster mcp`. This page is generated from the tool schemas in the sources (`Mcp.cpp`, `Orchestrator-Control.cpp`): the names, descriptions and parameters below are exactly what the model is told. Each tool can be turned off in `[mcp_tools]` (settings page *MCP*): `<tool> = false`.\n")
        out.append("> [!NOTE]\n> A sub-session of the claude driver has its own small MCP with `task_*`, `schedule_*` and the job tools only, limited to what it created.\n")
    else:
        out.append("La session globale pilote claude-cluster par le serveur MCP `claude-cluster mcp`. Cette page est générée à partir des schémas d'outils des sources (`Mcp.cpp`, `Orchestrator-Control.cpp`) : les noms, descriptions et paramètres ci-dessous sont exactement ce que le modèle reçoit. Chaque outil peut être désactivé dans `[mcp_tools]` (page de réglages *MCP*) : `<outil> = false`.\n")
        out.append("> [!NOTE]\n> Les descriptions viennent du code et sont donc en anglais, la langue dans laquelle le modèle les lit.\n")
        out.append("> [!NOTE]\n> Une sous-session du pilote claude a son propre petit MCP avec seulement `task_*`, `schedule_*` et les outils de travaux, limités à ce qu'elle a créé.\n")
    for gid, en, fr, names in GROUPS:
        members = [by_name[n] for n in names if n in by_name]
        if not members:
            continue
        placed.update(t["name"] for t in members)
        out.append("## %s {#%s}\n" % (en if lang == "en" else fr, gid))
        for t in members:
            out.append("### `%s` {#tool-%s}\n" % (t["name"], t["name"].replace("_", "-")))
            out.append(t["desc"].strip() + "\n")
            if t["props"]:
                out.append("| %s | %s | %s |" % (("Parameter", "Type", "Description") if lang == "en" else ("Paramètre", "Type", "Description")))
                out.append("|---|---|---|")
                for p in t["props"]:
                    mark = " (required)" if p["name"] in t["required"] and lang == "en" else (" (requis)" if p["name"] in t["required"] else "")
                    out.append("| `%s`%s | `%s` | %s |" % (p["name"], mark, p["type"] or "any", md_cell(p["desc"])))
                out.append("")
            else:
                out.append("*No parameter.*\n" if lang == "en" else "*Aucun paramètre.*\n")
    rest = [t for t in tools if t["name"] not in placed]
    if rest:
        out.append("## %s {#other}\n" % ("Other tools" if lang == "en" else "Autres outils"))
        for t in rest:
            out.append("### `%s` {#tool-%s}\n\n%s\n" % (t["name"], t["name"].replace("_", "-"), t["desc"].strip()))
    return "\n".join(out)


def pages(ctx) -> list[dict]:
    tools = load_tools(ctx)
    return [{"slug": "mcp-tools", "section": "reference", "order": 20,
             "en": {"title": "MCP tools", "summary": "Every tool of the global session, generated from the code: %d tools with their parameters." % len(tools), "keywords": "mcp tools session_send session_spawn task_add heavy_run schema", "md": render(tools, "en")},
             "fr": {"title": "Outils MCP", "summary": "Tous les outils de la session globale, générés depuis le code : %d outils avec leurs paramètres." % len(tools), "keywords": "mcp outils session_send session_spawn task_add heavy_run schéma", "md": render(tools, "fr")}}]


def search_entries(ctx) -> list[dict]:
    out = []
    for t in load_tools(ctx):
        first = re.split(r"(?<=[.!?])\s", t["desc"].strip(), maxsplit=1)[0]
        out.append({"k": "tools", "t": t["name"], "d": first[:140], "u": "mcp-tools.html#tool-%s" % t["name"].replace("_", "-"),
                    "x": " ".join(p["name"] for p in t["props"])})
    return out
