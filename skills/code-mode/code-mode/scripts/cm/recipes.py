"""Recipes: scripts that worked, kept under a name like a cache.

A recipe not used for `ttl_days` is removed, and when the cache holds more than `max_count` recipes or `max_bytes` of
code the least recently used ones go first. Using a recipe refreshes it.
"""
from __future__ import annotations

import re
import time
from pathlib import Path

from .paths import read_json, sub, write_json

NAME = re.compile(r"^[a-z0-9][a-z0-9_-]{0,39}$")
PARAM = re.compile(r"""args(?:\[\s*["'](\w+)["']\s*\]|\.get\(\s*["'](\w+)["'])""")

def folder() -> Path:
    return sub("recipes")

def index() -> dict[str, dict]:
    """name -> {description, created, last_used, uses, bytes, args}; entries whose file is gone are dropped."""
    known = read_json(folder() / "index.json", {})
    known = known if isinstance(known, dict) else {}
    return {name: entry for name, entry in known.items() if isinstance(entry, dict) and (folder() / f"{name}.py").is_file()}

def prune(config: dict, known: dict[str, dict] | None = None) -> list[str]:
    """Remove the recipes too old, then the least recently used ones over the count / size limits."""
    limits = config.get("recipes", {})
    known = index() if known is None else known
    now = time.time()
    removed = [name for name, entry in known.items()
               if now - float(entry.get("last_used") or 0) > float(limits.get("ttl_days", 30)) * 86400]
    kept = sorted((name for name in known if name not in removed), key=lambda name: float(known[name].get("last_used") or 0))
    while kept and (len(kept) > int(limits.get("max_count", 200))
                    or sum(int(known[name].get("bytes") or 0) for name in kept) > int(limits.get("max_bytes", 5000000))):
        removed.append(kept.pop(0))
    for name in removed:
        known.pop(name, None)
        try:
            (folder() / f"{name}.py").unlink()
        except OSError:
            pass  # already gone
    write_json(folder() / "index.json", known)
    return removed

def save(name: str, code: str, description: str, config: dict) -> str:
    """Keep a script under a name (a new version replaces the old one), then apply the cache limits."""
    if not NAME.match(name):
        raise ValueError(f"recipe name {name!r}: lowercase letters, digits, - and _, 40 characters at most")
    with open(folder() / f"{name}.py", "w", encoding="utf-8") as file:
        file.write(code if code.endswith("\n") else code + "\n")
    known = index()
    old = known.get(name, {})
    now = time.time()
    params = sorted({first or second for first, second in PARAM.findall(code)})
    known[name] = {"description": " ".join(description.split())[:200], "created": old.get("created", now), "last_used": now,
                   "uses": int(old.get("uses") or 0), "bytes": len(code.encode("utf-8")), "args": params}
    removed = prune(config, known)
    return f"recipe {name} saved" + (f" (removed from the cache: {', '.join(removed)})" if removed else "")

def load(name: str) -> str:
    """The code of a recipe; using it counts and refreshes its date."""
    known = index()
    if name not in known:
        raise KeyError(f"no recipe named {name} (search lists them)")
    with open(folder() / f"{name}.py", "r", encoding="utf-8") as file:
        code = file.read()
    known[name]["last_used"] = time.time()
    known[name]["uses"] = int(known[name].get("uses") or 0) + 1
    write_json(folder() / "index.json", known)
    return code

def remove(name: str) -> bool:
    known = index()
    if name not in known:
        return False
    known.pop(name)
    try:
        (folder() / f"{name}.py").unlink()
    except OSError:
        pass  # the index is what counts
    write_json(folder() / "index.json", known)
    return True

def line(name: str, entry: dict) -> str:
    """One line of a recipe in search and in the listing."""
    age = int((time.time() - float(entry.get("last_used") or 0)) / 86400)
    params = ", ".join(entry.get("args") or [])
    return f"recipe:{name}({params}) - {entry.get('description') or 'no description'} [used {int(entry.get('uses') or 0)}x, {age}d ago]"
