#!/usr/bin/env python3
"""
Build the skills of this repository for claude.ai (Customize > Skills > Add > Upload).

Usage: python3 claude-ai/build.py [--out dist/claude-ai] [skill ...]

One <skill>.zip per skill, the skill folder at the top of the archive (claude.ai format):
- the frontmatter keeps only the keys claude.ai accepts (name, description, license, compatibility, metadata,
  allowed-tools): disable-model-invocation of the routers is removed;
- `<other>/` (folder of another skill) is explained as ../<other>/ (skills side by side);
- the routers (manual /commands of Claude Code) go to <out>/routers/.
"""

import argparse
import re
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SKIP = {"__pycache__", ".DS_Store"}
KEYS = {"name", "description", "license", "compatibility", "metadata", "allowed-tools"}
NOTE = re.compile(r"^`<name>` \(a skill\) = the folder of that skill:.*$", re.M)


def for_claude_ai(text: str) -> str:
    """SKILL.md for claude.ai: accepted frontmatter keys only, other skills side by side."""
    m = re.match(r"---\n(.*?)\n---\n", text, re.S)
    if m:
        keep = [l for l in m.group(1).split("\n") if l.split(":")[0].strip() in KEYS or l.startswith((" ", "\t"))]
        text = "---\n" + "\n".join(keep) + "\n---\n" + text[m.end():]
    return NOTE.sub("`<name>` (a skill) = the folder of that skill, next to this one: `../<name>`.", text)


def skills():
    for md in sorted(REPO.glob("*/*/SKILL.md")):
        yield md.parent


def build(skill: Path, out: Path) -> Path:
    text = (skill / "SKILL.md").read_text()
    router = re.search(r"^disable-model-invocation:\s*true", text, re.M) is not None
    dest = (out / "routers" if router else out) / (skill.name + ".zip")
    dest.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(skill.rglob("*")):
            if f.is_dir() or SKIP & set(f.parts):
                continue
            arc = Path(skill.name) / f.relative_to(skill)
            if f.suffix in {".md", ".txt", ".py", ".sh", ".mjs", ".js", ".json", ".html", ".css", ".cmake", ".yml", ".yaml", ""} or f.name == "CMakeLists.txt":
                try:
                    data = f.read_text().replace("~/.claude/skills/", "../")
                    if f.name == "SKILL.md" and f.parent == skill:
                        data = for_claude_ai(data)
                    z.writestr(str(arc), data)
                    continue
                except UnicodeDecodeError:
                    pass
            z.write(f, str(arc))
    return dest


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(REPO / "dist" / "claude-ai"))
    ap.add_argument("names", nargs="*")
    a = ap.parse_args()
    out = Path(a.out)
    todo = [s for s in skills() if not a.names or s.name in a.names]
    if not todo:
        sys.exit("no skill matches")
    for s in todo:
        d = build(s, out)
        print(f"{d.relative_to(out.parent) if out.parent in d.parents else d}  ({d.stat().st_size // 1024} KB)")
    print(f"\n{len(todo)} archives in {out} - upload them in claude.ai: Customize > Skills > Add > Upload")


if __name__ == "__main__":
    main()
