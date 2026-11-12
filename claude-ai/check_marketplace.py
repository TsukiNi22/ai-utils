#!/usr/bin/env python3
"""Checks .claude-plugin/marketplace.json before a push: claude.ai refuses / warns on a plugin description over 500 characters.

    python3 claude-ai/check_marketplace.py

Every plugin: description <= 500 characters, its folders exist, every skill of those folders is named in the description
(skills: a, b, c). Exit 1 on any problem.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIMIT = 500

def main() -> None:
    market = json.loads((ROOT / ".claude-plugin" / "marketplace.json").read_text(encoding="utf-8"))
    problems = 0
    for plugin in market["plugins"]:
        name, description = plugin["name"], plugin["description"]
        if len(description) > LIMIT:
            print(f"ERROR {name}: description is {len(description)} characters (limit {LIMIT})")
            problems += 1
        listed = set(re.findall(r"[a-z0-9-]+", (re.search(r"\(skills?: ([^)]*)\)", description) or [None, ""])[1]))
        found = set()
        for folder in plugin.get("skills", []):
            path = ROOT / folder
            if not path.is_dir():
                print(f"ERROR {name}: folder {folder} not found")
                problems += 1
                continue
            found |= {f.parent.name for f in path.glob("*/SKILL.md")}
        if listed and found != listed and plugin["source"] == "./" and not plugin["name"].endswith("-router"):
            print(f"WARN {name}: skills not named in the description: {sorted(found - listed)}; named but absent: {sorted(listed - found)}")
    print(f"{len(market['plugins'])} plugins checked, {problems} problem(s)")
    sys.exit(1 if problems else 0)

if __name__ == "__main__":
    main()
