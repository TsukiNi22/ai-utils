#!/usr/bin/env python3
"""
Check that the repository holds together: the skills, their requirements, the routers, the marketplace, the READMEs.

Usage: python3 scripts/check.py

Checks (exit 1 on any error):
- every skill: `name` = its folder and unique, a `description` (at most 1024 characters), a `metadata.summary`, its
  requires.txt (optional) naming existing skills (not itself, no duplicate), the files named in its SKILL.md
  (`scripts/...`, `templates/...`, `reference/...`, `examples/...`) present in it or in a skill named on that line or
  the one before, no tab or trailing space;
- every skill other than a router is loaded by at least one router (its requires.txt);
- the marketplace: a plugin for each category and each router, and claude-ai/check_marketplace.py passes;
- the generated tables of the READMEs are up to date (scripts/readme.py --check);
- the relative links of the READMEs and of INSTALL.md lead to a file;
- setup.sh parses (bash -n) and lists every skill.
"""

import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SKILLS = REPO / "skills"
LIMIT = 1024
OWN_PATH = re.compile(r"(?:SKILL_DIR/|`)((?:scripts|templates|reference|examples)/[\w./-]+)(?![\w./-]*[*<{])")
LINK = re.compile(r"\]\((?!https?:|mailto:|#)([^)\s]+)\)")

class Report:
    def __init__(self) -> None:
        self.errors: list[str] = []

    def error(self, where: str, message: str) -> None:
        self.errors.append(f"{where}: {message}")

def front_matter(text: str) -> str:
    match = re.match(r"---\n(.*?)\n---\n", text, re.S)
    return match.group(1) if match else ""

def check_skills(report: Report) -> dict[str, Path]:
    """The checks of each skill on its own; returns name -> folder."""
    found: dict[str, Path] = {}
    for md in sorted(SKILLS.glob("*/*/SKILL.md")):
        folder, name = md.parent, md.parent.name
        where = str(folder.relative_to(REPO))
        if name in found:
            report.error(where, f"a second skill named {name} ({found[name].relative_to(REPO)})")
        found[name] = folder
        text = md.read_text(encoding="utf-8")
        front = front_matter(text)
        if not front:
            report.error(where, "SKILL.md has no frontmatter")
            continue
        fields = dict(re.findall(r"^([\w-]+):[ \t]*(.*)$", front, re.M))
        if fields.get("name") != name:
            report.error(where, f"frontmatter name {fields.get('name')!r} is not the folder name")
        description = fields.get("description", "")
        if not description:
            report.error(where, "no description")
        elif len(description) > LIMIT:
            report.error(where, f"description of {len(description)} characters (at most {LIMIT})")
        if not re.search(r"^  summary: '.+'$", front, re.M):
            report.error(where, "no metadata.summary (the line of the README tables)")
        for number, line in enumerate(text.splitlines(), 1):
            if "\t" in line or line != line.rstrip():
                report.error(f"{where}/SKILL.md:{number}", "tab or trailing space")
        lines = text.splitlines()
        for number, line in enumerate(lines, 1):
            for path in OWN_PATH.findall(line):
                path = path.rstrip(".,:")
                # its own file, or the file of a skill named on that line or the one before (`cpp-class` / **cpp-class**)
                near = lines[number - 2] + " " + line if number > 1 else line
                names = re.findall(r"`([\w-]+)`|\*\*`?([\w-]+)`?\*\*", near)
                owners = [folder] + [other for pair in names for name in pair if name for other in SKILLS.glob(f"*/{name}")]
                if not any((owner / path).exists() for owner in owners):
                    report.error(f"{where}/SKILL.md:{number}", f"names {path}, not in the skill (nor in a skill named next to it)")
    for name, folder in found.items():
        req = folder / "requires.txt"
        where = str(req.relative_to(REPO))
        if not req.is_file():
            continue # needs no other skill
        deps = req.read_text(encoding="utf-8").split()
        for dep in deps:
            if dep not in found:
                report.error(where, f"requires an unknown skill {dep!r}")
            if dep == name:
                report.error(where, "requires itself")
        if len(set(deps)) != len(deps):
            report.error(where, "a skill named twice")
    return found

def check_routers(report: Report, found: dict[str, Path]) -> None:
    routed: set[str] = set()
    for name, folder in found.items():
        if folder.parent.name == "routers" and (folder / "requires.txt").is_file():
            routed |= set((folder / "requires.txt").read_text(encoding="utf-8").split())
    for name, folder in found.items():
        if folder.parent.name != "routers" and name not in routed:
            report.error(str(folder.relative_to(REPO)), "loaded by no router (add it to the requires.txt of one)")

def check_marketplace(report: Report, found: dict[str, Path]) -> None:
    market = json.loads((REPO / ".claude-plugin" / "marketplace.json").read_text(encoding="utf-8"))
    folders = {f for plugin in market["plugins"] for f in plugin.get("skills", [])}
    sources = {plugin["source"] for plugin in market["plugins"]}
    for category in sorted({f.parent.name for f in found.values()} - {"routers"}):
        if f"./skills/{category}/" not in folders:
            report.error(".claude-plugin/marketplace.json", f"no plugin for skills/{category}/")
    for name, folder in found.items():
        if folder.parent.name == "routers" and f"./skills/routers/{name}" not in sources:
            report.error(".claude-plugin/marketplace.json", f"no plugin for the router {name}")
    run = subprocess.run([sys.executable, str(REPO / "claude-ai" / "check_marketplace.py")], capture_output=True, text=True)
    if run.returncode:
        for line in run.stdout.splitlines():
            if line.startswith("ERROR"):
                report.error("claude-ai/check_marketplace.py", line)

def check_readmes(report: Report) -> None:
    run = subprocess.run([sys.executable, str(REPO / "scripts" / "readme.py"), "--check"], capture_output=True, text=True)
    for line in run.stdout.splitlines():
        report.error("scripts/readme.py", line)
    docs = [REPO / "README.md", REPO / "INSTALL.md", *sorted(SKILLS.glob("**/README.md")), REPO / "context" / "README.md",
            REPO / "claude-ai" / "README.md"]
    for doc in docs:
        if not doc.is_file():
            report.error(str(doc.relative_to(REPO)), "missing")
            continue
        for target in LINK.findall(doc.read_text(encoding="utf-8")):
            path = target.split("#", 1)[0]
            if path and not (doc.parent / path).exists():
                report.error(str(doc.relative_to(REPO)), f"link to {target}: not found")

def check_setup(report: Report, found: dict[str, Path]) -> None:
    if subprocess.run(["bash", "-n", str(REPO / "setup.sh")]).returncode:
        report.error("setup.sh", "does not parse (bash -n)")
        return
    run = subprocess.run(["bash", str(REPO / "setup.sh"), "list"], capture_output=True, text=True)
    listed = set(re.findall(r"^  ([\w-]+) ", run.stdout, re.M))
    for name in sorted(set(found) - listed):
        report.error("setup.sh list", f"does not list {name}")

def main() -> int:
    report = Report()
    found = check_skills(report)
    check_routers(report, found)
    check_marketplace(report, found)
    check_readmes(report)
    check_setup(report, found)
    for line in report.errors:
        print(line)
    print(f"check: {len(found)} skills, {len(report.errors)} error(s)")
    return 1 if report.errors else 0

if __name__ == "__main__":
    sys.exit(main())
