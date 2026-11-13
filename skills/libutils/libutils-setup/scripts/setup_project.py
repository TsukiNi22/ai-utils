#!/usr/bin/env python3
"""
Set up libutils in an existing CMake project (idempotent: running it twice changes nothing).

Usage:
    setup_project.py [<project root>] [--exceptions] [--targets a,b] [--version X.Y.Z] [--dry-run]

    --exceptions   also set up the custom exception codes (JSON config, generator scripts, CMake block,
                   include/exception, .gitignore)
    --targets      targets linked to libutils (default: every add_executable/add_library of the root CMake)
    --version      minimal version for find_package (default: the installed one)
    --dry-run      print the new CMakeLists.txt and the files that would be created, change nothing

The CMake is edited in Tsukini's layout (cmake-style): '# Requirement', '# Special Targets' and
'# Dependencies' sections, created when missing.
"""

import argparse
import glob
import os
import re
import shutil
import sys

def skill_dir(name: str) -> str:
    """Directory of another skill: installed flat (~/.claude/skills/<name>) or in this repository (<category>/<name>)."""
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__))) # this skill
    for cand in (os.path.join(os.path.dirname(here), name), os.path.expanduser(f"~/.claude/skills/{name}")):
        if os.path.isfile(os.path.join(cand, "SKILL.md")):
            return cand
    repo = os.path.dirname(os.path.dirname(here)) # repository: <category>/<skill>
    for cand in glob.glob(os.path.join(repo, "*", name)):
        if os.path.isfile(os.path.join(cand, "SKILL.md")):
            return cand
    return os.path.join(os.path.dirname(here), name)

EXC_TEMPLATES = os.path.join(skill_dir("libutils-exception"), "templates")
BANNER = "# ========================="
EXC_BLOCK = '''file(GLOB_RECURSE EXCEPTION_CONFIG_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/cmake/config/exceptions/*.json"
)
set(GENERATED_EXCEPTION_HEADER
    "${CMAKE_SOURCE_DIR}/include/exception/generated_external_exception_header.hpp"
)
add_custom_command(
    OUTPUT ${GENERATED_EXCEPTION_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/scripts/generate_exception_header.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${EXCEPTION_CONFIG_FILES} ${CMAKE_SOURCE_DIR}/cmake/scripts/generate_exception_header.py
    COMMENT "Generating exception header (config changed)"
    VERBATIM
)
add_custom_target(generated_external_exception_header
    DEPENDS ${GENERATED_EXCEPTION_HEADER}
)
'''

def remote_setup(url: str) -> str:
    """Command running a remote setup.sh with the download tool available: curl, else wget, else python3, else curl."""
    if shutil.which("curl") or not (shutil.which("wget") or shutil.which("python3")):
        return f"curl -fsSL {url} | bash"
    if shutil.which("wget"):
        return f"wget -qO- {url} | bash"
    return f"python3 -c 'import sys, urllib.request; sys.stdout.buffer.write(urllib.request.urlopen(sys.argv[1]).read())' {url} | bash"

def installed_version() -> str | None:
    found = []
    for d in ("/usr/local/include", "/usr/include"):
        p = os.path.join(d, "utils", "version.hpp")
        if os.path.isfile(p):
            m = re.search(r'__LIBUTILS_VERSION__\s+"v?([0-9.]+)"', open(p, encoding="utf-8").read())
            if m: found.append((d, m.group(1)))
    if not found: return None
    if len(found) > 1 and found[0][1] != found[1][1]:
        print(f"warning: /usr/local has libutils {found[0][1]} and /usr has {found[1][1]}: /usr/local is used first "
              f"by the compiler (libutils-install: 'libutils.sh remove --local' to drop it)", file=sys.stderr)
    return max((v for _, v in found), key=lambda v: [int(x) for x in v.split(".")])

def section_end(lines: list[str], title: str) -> int | None:
    """Index after the last non empty line of the section `title` (None if the section doesn't exist)."""
    for i, l in enumerate(lines):
        if l.strip() == f"# {title}" or l.strip().startswith(f"# {title} ") or l.strip().startswith(f"# {title}("):
            if i > 0 and lines[i - 1].startswith(BANNER) and i + 1 < len(lines) and lines[i + 1].startswith(BANNER):
                j = i + 2
                while j < len(lines) and not (lines[j].startswith(BANNER) and j + 1 < len(lines) and lines[j + 1].startswith("# ")):
                    j += 1
                while j > i + 2 and not lines[j - 1].strip(): j -= 1
                return j
    return None

def add_section(lines: list[str], title: str, body: list[str], before: tuple[str, ...]) -> None:
    """Create a section before the first existing one of `before` (or at the end)."""
    at = len(lines)
    for b in before:
        for i, l in enumerate(lines):
            if l.strip() == f"# {b}" or l.strip().startswith(f"# {b} "):
                if i > 0 and lines[i - 1].startswith(BANNER):
                    at = min(at, i - 1)
    while at > 0 and not lines[at - 1].strip(): at -= 1
    block = [BANNER, f"# {title}", BANNER] + body + ([""] if at < len(lines) and lines[at].strip() else [])
    lines[at:at] = ([""] if at else []) + block

def insert_in_section(lines: list[str], title: str, body: list[str], before: tuple[str, ...]) -> None:
    end = section_end(lines, title)
    if end is None:
        add_section(lines, title, body, before)
        return
    if title == "Requirement": # find_package lines go before the ccache block
        for i in range(end - 1, -1, -1):
            if lines[i].startswith("find_program(CCACHE_PROGRAM"):
                end = i
                break
            if lines[i].startswith(BANNER): break
    lines[end:end] = body

def targets_of(text: str) -> list[str]:
    names = re.findall(r"^\s*add_(?:executable|library)\(\s*([^\s)]+)", text, re.M)
    return list(dict.fromkeys(names))

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("root", nargs="?", default=".")
    ap.add_argument("--exceptions", action="store_true")
    ap.add_argument("--targets", default="")
    ap.add_argument("--version", default="")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    root = os.path.abspath(a.root)
    cmake = os.path.join(root, "CMakeLists.txt")
    if not os.path.isfile(cmake):
        sys.stderr.write(f"Error: no CMakeLists.txt in {root}\n"); return 1
    version = a.version or installed_version()
    if not version:
        print(f"Error: libutils is not installed: install it ({remote_setup('https://raw.githubusercontent.com/TsukiNi22/libutils/main/setup.sh')}) or give --version",
              file=sys.stderr); return 2
    text = open(cmake, encoding="utf-8").read()
    lines = text.split("\n")
    targets = [t for t in a.targets.split(",") if t] or targets_of(text)
    if not targets:
        sys.stderr.write("Error: no add_executable/add_library found, give --targets\n"); return 1
    done = []

    # Requirement
    if not re.search(r"find_package\(\s*utils\b", text):
        insert_in_section(lines, "Requirement", [f"find_package(utils {version} CONFIG REQUIRED) # Check utils dependencies existance"],
                          ("Warnings", "Sources"))
        done.append(f"find_package(utils {version})")
    if a.exceptions and not re.search(r"find_package\(\s*Python3\b", "\n".join(lines)):
        insert_in_section(lines, "Requirement", ["find_package(Python3 REQUIRED) # Used for the special dependencie"],
                          ("Warnings", "Sources"))
        done.append("find_package(Python3)")

    # Exception header generation
    if a.exceptions and "GENERATED_EXCEPTION_HEADER" not in "\n".join(lines):
        insert_in_section(lines, "Special Targets", ([""] if section_end(lines, "Special Targets") else []) + EXC_BLOCK.rstrip("\n").split("\n"),
                          ("Release", "Dependencies", "Includes", "Modes", "Installation", "Utils", "Package Building"))
        done.append("exception header generation")

    # Dependencies of every target
    body = []
    joined = "\n".join(lines)
    for t in targets:
        tre = re.escape(t)
        m = re.search(rf"^(\s*target_link_libraries\(\s*{tre}\s+(?:PRIVATE|PUBLIC|INTERFACE)\b[^)\n]*)\)", joined, re.M)
        if m and "utils::utils" not in m.group(1):
            joined = joined[:m.end(1)] + " utils::utils" + joined[m.end(1):]
            done.append(f"{t}: + utils::utils")
        elif not m and not re.search(rf"target_link_libraries\(\s*{tre}\b[^)]*utils::utils", joined):
            body.append(f"target_link_libraries({t} PRIVATE utils::utils)")
            done.append(f"{t}: link utils::utils")
        if a.exceptions:
            if not re.search(rf"target_include_directories\(\s*{tre}\b[^)]*include/exception", joined):
                body.append(f"target_include_directories({t} PRIVATE include include/exception)")
                done.append(f"{t}: include/exception")
            if not re.search(rf"add_dependencies\(\s*{tre}\b[^)]*generated_external_exception_header", joined):
                body.append(f"add_dependencies({t} generated_external_exception_header)")
                done.append(f"{t}: depends on the generated header")
    lines = joined.split("\n")
    if body:
        insert_in_section(lines, "Dependencies", body, ("Includes", "Headers configuration", "Modes", "Installation", "Utils", "Package Building"))
    new = "\n".join(lines).rstrip("\n") + "\n"

    # Files for the exceptions
    files = []
    if a.exceptions:
        for rel in ("cmake/scripts/const.py", "cmake/scripts/generate_exception_header.py", "cmake/scripts/requirements.txt"):
            if not os.path.exists(os.path.join(root, rel)): files.append((os.path.join(EXC_TEMPLATES, rel), rel))
        cfg = os.path.join(root, "cmake/config/exceptions")
        if not (os.path.isdir(cfg) and any(f.endswith(".json") for f in os.listdir(cfg))):
            files.append((os.path.join(EXC_TEMPLATES, "cmake/config/exceptions/global.json"), "cmake/config/exceptions/global.json"))

    if a.dry_run:
        print(new)
        print("\n# files:", ", ".join(r for _, r in files) or "none")
        print("# changes:", ", ".join(done) or "none")
        return 0
    if new != text:
        open(cmake, "w", encoding="utf-8").write(new)
    for src, rel in files:
        os.makedirs(os.path.dirname(os.path.join(root, rel)), exist_ok=True)
        shutil.copy(src, os.path.join(root, rel))
        done.append(f"created {rel}")
    if a.exceptions:
        os.makedirs(os.path.join(root, "include/exception"), exist_ok=True)
        gi = os.path.join(root, ".gitignore")
        line = "include/exception/generated_external_exception_header.hpp"
        content = open(gi, encoding="utf-8").read() if os.path.exists(gi) else ""
        if line not in content:
            with open(gi, "a", encoding="utf-8") as f: f.write(("" if not content or content.endswith("\n") else "\n") + line + "\n")
            done.append(".gitignore: generated header")
    print(f"libutils {version} -> targets {', '.join(targets)}")
    print("\n".join("  " + d for d in done) if done else "  already set up, nothing to do")
    return 0

if __name__ == "__main__":
    sys.exit(main())
