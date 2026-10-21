#!/usr/bin/env python3
# =============================================================================
# _data.py - data shared by the pages: the changelog (git log) and the downloads (files and their size)
# =============================================================================

import re
from pathlib import Path

PROMPT_FILES = ("global.md", "sessions.md", "rendering.md")
SUBJECT = re.compile(r"^(?:feat|fix|docs|refactor|perf|test|chore|build)\(claude-cluster\):\s*(.*?)(?:,\s*(\d+\.\d+\.\d+))?\s*$")


def human_size(size: int) -> str:
    if size >= 1 << 20:
        return "%.1f MB" % (size / (1 << 20))
    return "%d KB" % max(1, round(size / 1024))


def short(text: str, limit: int = 110) -> str:
    """First clause of a commit subject (up to ' - ' or the limit)."""
    head = text.split(" - ", 1)[0]
    return head if len(head) <= limit else head[:limit - 1].rstrip() + "…"


def changes(ctx) -> list[dict]:
    """One entry per version, newest first: version, date, the commits (hash, subject) of that version."""
    out = ctx.git("log", "--format=%h|%cs|%s", "--", ".")
    versions: list[dict] = []
    pending: list[dict] = []  # commits without a version: they belong to the next newer version
    for line in reversed(out.splitlines()):
        h, date, subject = line.split("|", 2)
        m = SUBJECT.match(subject)
        if not m:
            continue
        item = {"hash": h, "date": date, "text": m.group(1)}
        pending.append(item)
        if m.group(2):
            versions.append({"version": m.group(2), "date": date, "commits": pending, "en": short(m.group(1)), "fr": short(m.group(1))})
            pending = []
    if pending:
        versions.append({"version": "next", "date": pending[-1]["date"], "commits": pending, "en": short(pending[-1]["text"]), "fr": short(pending[-1]["text"])})
    merged: dict[str, dict] = {}
    for v in versions:  # a version released by several commits: one entry
        if v["version"] in merged:
            merged[v["version"]]["commits"] += v["commits"]
            merged[v["version"]]["date"] = v["date"]
        else:
            merged[v["version"]] = v
    return sorted(merged.values(), key=lambda v: [int(x) if x.isdigit() else 10 ** 6 for x in v["version"].split(".")], reverse=True)


def artifact(ctx, pattern: str) -> Path | None:
    if ctx.artifacts:
        found = sorted(ctx.artifacts.glob(pattern))
        if found:
            return found[-1]
    return None


def downloads(ctx) -> list[dict]:
    rows: list[dict] = []
    setup = ctx.code_root.parents[1] / "setup.sh"
    raw = "https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh"
    rows.append({"name": "setup.sh", "href": raw, "version": "main", "size": human_size(setup.stat().st_size) if setup.exists() else "-"})
    apk = artifact(ctx, "*.apk")
    rows.append({"name": "claude-cluster.apk", "href": "downloads.html#apk", "version": ctx.version, "size": human_size(apk.stat().st_size) if apk else "built at install"})
    pdf = artifact(ctx, "claude-cluster-audit-report.pdf") or (ctx.out / "downloads" / "claude-cluster-audit-report.pdf")
    if pdf.exists():
        rows.append({"name": "audit-report.pdf", "href": "downloads/claude-cluster-audit-report.pdf", "version": ctx.version, "size": human_size(pdf.stat().st_size)})
    for name in PROMPT_FILES:
        source = ctx.code_root / "templates" / name
        if source.exists():
            rows.append({"name": name, "href": "downloads/" + name, "version": "%s (%s)" % (ctx.version, ctx.commit), "size": human_size(source.stat().st_size)})
    return rows
