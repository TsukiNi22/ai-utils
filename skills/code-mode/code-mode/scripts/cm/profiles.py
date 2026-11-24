"""Notes per kind of server: how to use it well through code (shipped: Blender, Roblox Studio; the user adds his own).

A profile is a small text file: header lines `tools:` (it applies when the server has one of these tools) and / or
`server:` (a name pattern), then `---`, then the notes shown by search the first time the server comes up in a session.
The profiles of the skill are in ../../profiles/, the ones of the user in <data folder>/profiles/ (same name wins).
"""
from __future__ import annotations

import fnmatch
from pathlib import Path

from .paths import home

SHIPPED = Path(__file__).resolve().parent.parent.parent / "profiles"

def load() -> list[dict]:
    """Every profile: {"name", "tools", "server", "text"}, the user's own replacing a shipped one of the same name."""
    found: dict[str, dict] = {}
    for folder in (SHIPPED, home() / "profiles"):
        for path in sorted(folder.glob("*.md")) if folder.is_dir() else []:
            try:
                with open(path, "r", encoding="utf-8") as file:
                    head, _, text = file.read().partition("\n---\n")
            except OSError:
                continue
            fields = dict(line.split(":", 1) for line in head.splitlines() if ":" in line)
            found[path.stem] = {"name": path.stem, "tools": [name.strip() for name in fields.get("tools", "").split(",") if name.strip()],
                                "server": [name.strip().lower() for name in fields.get("server", "").split(",") if name.strip()],
                                "text": " ".join(text.split())}
    return list(found.values())

def notes(server: str, tools: list[str], known: list[dict]) -> str:
    """The notes that apply to a server (its tools first, then its name), or nothing."""
    for profile in known:
        if set(profile["tools"]) & set(tools) or any(fnmatch.fnmatchcase(server.lower(), pattern) for pattern in profile["server"]):
            return profile["text"]
    return ""
