#!/usr/bin/env python3
"""
Write the generated tables of the READMEs from the skills themselves, so that no list is kept by hand.

Usage: python3 scripts/readme.py [--check]

Sources: skills/<category>/<skill>/SKILL.md (`name`, `metadata.summary`), its requires.txt and
.claude-plugin/marketplace.json (the plugin of each category). Blocks written (between
`<!-- generated:<key> -->` and `<!-- /generated:<key> -->`, the rest of each file is kept as it is):
- README.md: `counts`, `routers`, `categories`;
- skills/README.md: `categories`;
- skills/<category>/README.md: `skills` (the file is created when it is missing);
- INSTALL.md: `plugins`.
--check writes nothing and exits 1 when a file is not up to date (the CI runs it through scripts/check.py).
"""

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SKILLS = REPO / "skills"
ORDER = ["routers", "cpp", "python", "libutils", "tests", "style", "docs", "git", "audit", "legal", "ux", "system", "ai"]
TITLES = {"routers": "Routers", "cpp": "C++", "python": "Python", "libutils": "libutils", "tests": "Tests",
          "style": "Style", "docs": "Documentation", "git": "Git", "audit": "Audit", "legal": "Legal", "ux": "UX",
          "system": "System", "ai": "AI workflow"}
SUMMARY = re.compile(r"^  summary: '((?:[^']|'')*)'$", re.M)
BLOCK = "<!-- generated:{0} -->\n{1}\n<!-- /generated:{0} -->"

@dataclass
class Skill:
    name: str
    category: str
    summary: str
    requires: list[str] = field(default_factory=list)

def load_skills() -> list[Skill]:
    """Every skill of the repository, with its summary and what it installs with it."""
    found: list[Skill] = []
    for md in sorted(SKILLS.glob("*/*/SKILL.md")):
        match = SUMMARY.search(md.read_text(encoding="utf-8").split("\n---\n", 1)[0])
        summary = match.group(1).replace("''", "'") if match else ""
        req = md.parent / "requires.txt"
        requires = req.read_text(encoding="utf-8").split() if req.is_file() else []
        found.append(Skill(md.parent.name, md.parent.parent.name, summary, requires))
    return sorted(found, key=lambda s: (s.name != "dev", s.name)) # /dev first: the entry point

def categories(skills: list[Skill]) -> list[str]:
    """The categories in the reading order (the unknown ones last, by name)."""
    present = {s.category for s in skills}
    return [c for c in ORDER if c in present] + sorted(present - set(ORDER))

def plugins() -> dict[str, tuple[str, str]]:
    """category -> (plugin name, what it covers) from the marketplace."""
    market = json.loads((REPO / ".claude-plugin" / "marketplace.json").read_text(encoding="utf-8"))
    out: dict[str, tuple[str, str]] = {}
    for plugin in market["plugins"]:
        for folder in plugin.get("skills", []):
            match = re.fullmatch(r"\./skills/([\w-]+)/", folder)
            if match:
                covers = re.sub(r"\s*\(skills?:[^)]*\)\s*$", "", plugin["description"])
                covers = covers.split(": ", 1)[-1]
                out[match.group(1)] = (plugin["name"], covers[:1].upper() + covers[1:])
    return out

def cell(text: str) -> str:
    return text.replace("|", "\\|")

def skill_link(skill: Skill, base: str) -> str:
    return f"[`{skill.name}`]({base}{skill.category}/{skill.name}/SKILL.md)"

def routers_table(skills: list[Skill]) -> str:
    rows = ["| Router | What it does |", "|---|---|"]
    rows += [f"| [`/{s.name}`](skills/routers/{s.name}/SKILL.md) | {cell(s.summary)} |" for s in skills if s.category == "routers"]
    return "\n".join(rows)

def categories_table(skills: list[Skill], base: str) -> str:
    """One row per category (not the routers): what it covers and its skills."""
    covers = plugins()
    rows = ["| Category | What it covers | Skills |", "|---|---|---|"]
    for category in categories(skills):
        if category == "routers":
            continue
        names = ", ".join(f"`{s.name}`" for s in skills if s.category == category)
        about = covers.get(category, ("", ""))[1]
        rows.append(f"| [{TITLES.get(category, category)}]({base}{category}/README.md) | {cell(about)} | {names} |")
    return "\n".join(rows)

def skills_table(skills: list[Skill], category: str) -> str:
    """The detailed table of a category README (links relative to skills/<category>/)."""
    by_name = {s.name: s for s in skills}
    own = [s for s in skills if s.category == category]
    if category == "routers":
        rows = ["| Router | What it does | Installs with it |", "|---|---|---|"]
        rows += [f"| [`/{s.name}`]({s.name}/SKILL.md) | {cell(s.summary)} | {len(s.requires)} skills |" for s in own]
        return "\n".join(rows)
    rows = ["| Skill | What it does | Installs with it |", "|---|---|---|"]
    for s in own:
        deps = ", ".join(skill_link(by_name[d], "../") if d in by_name else f"`{d}`" for d in s.requires) or "—"
        rows.append(f"| [`{s.name}`]({s.name}/SKILL.md) | {cell(s.summary)} | {deps} |")
    return "\n".join(rows)

def plugins_table(skills: list[Skill]) -> str:
    covers = plugins()
    rows = ["| Plugin | Skills |", "|---|---|"]
    for category in categories(skills):
        if category in covers:
            rows.append(f"| `{covers[category][0]}` | {', '.join(s.name for s in skills if s.category == category)} |")
    routers = [s.name for s in skills if s.category == "routers"]
    rows.append(f"| {', '.join(f'`{r}`' for r in routers)} | the routers (`/{routers[0]}`, `/{routers[1]}`...) |")
    return "\n".join(rows)

def counts(skills: list[Skill]) -> str:
    routers = sum(s.category == "routers" for s in skills)
    return f"**{len(skills) - routers} skills** in {len(categories(skills)) - 1} categories and **{routers} routers**."

def new_category_readme(category: str) -> str:
    title = TITLES.get(category, category)
    return f"# {title} skills\n\n{BLOCK.format('skills', '')}\n"

def fill(text: str, key: str, content: str, path: Path) -> str:
    pattern = re.compile(r"<!-- generated:%s -->\n.*?<!-- /generated:%s -->" % (key, key), re.S)
    if not pattern.search(text):
        sys.exit(f"readme: {path.relative_to(REPO)} has no block <!-- generated:{key} -->")
    return pattern.sub(lambda _: BLOCK.format(key, content), text)

def targets(skills: list[Skill]) -> dict[Path, dict[str, str]]:
    """Each file -> its blocks -> their content."""
    out: dict[Path, dict[str, str]] = {
        REPO / "README.md": {"counts": counts(skills), "routers": routers_table(skills),
                             "categories": categories_table(skills, "skills/")},
        SKILLS / "README.md": {"categories": categories_table(skills, "")},
        REPO / "INSTALL.md": {"plugins": plugins_table(skills)},
    }
    for category in categories(skills):
        out[SKILLS / category / "README.md"] = {"skills": skills_table(skills, category)}
    return out

def main() -> int:
    parser = argparse.ArgumentParser(description="Write the generated tables of the READMEs")
    parser.add_argument("--check", action="store_true", help="write nothing, exit 1 when a file is not up to date")
    args = parser.parse_args()
    skills = load_skills()
    stale: list[str] = []
    for path, blocks in targets(skills).items():
        old = path.read_text(encoding="utf-8") if path.is_file() else ""
        text = old or (new_category_readme(path.parent.name) if path.parent.parent == SKILLS else "")
        if not text:
            sys.stderr.write(f"readme: {path.relative_to(REPO)} not found\n")
            return 1
        for key, content in blocks.items():
            text = fill(text, key, content, path)
        if text != old:
            stale.append(str(path.relative_to(REPO)))
            if not args.check:
                path.write_text(text, encoding="utf-8")
    if args.check:
        for name in stale:
            print(f"readme: {name} is not up to date (python3 scripts/readme.py)")
        return 1 if stale else 0
    print(f"readme: {len(stale)} file(s) written" + (f": {', '.join(stale)}" if stale else ""))
    return 0

if __name__ == "__main__":
    sys.exit(main())
