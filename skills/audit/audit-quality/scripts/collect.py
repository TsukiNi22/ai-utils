#!/usr/bin/env python3
"""
Collect the objective metrics of a cleanliness / convention audit (input of the PDF report).

Usage:
    collect.py [<project root>] [--json out.json] [--md out.md] [--build]

Checks (C/C++ projects in the user's style, the rest is skipped when absent):
    files      : count / lines per module (src, include, tests)
    headers    : Xartania / Epitech file header present in .cpp/.hpp/.c/.h
    guards     : include guard '#ifndef NAME_H' matching the file name (or #pragma once)
    style      : 'auto' (not iterator / structured binding), 'using namespace', tabs, trailing spaces,
                 TODO/FIXME, empty parameter lists without (void), 'NULL', C casts '(int)x'
    cmake      : .cpp not listed in the CMake sources, GLOB used for sources
    gitignore  : project outputs missing from .gitignore (cpp-project/scripts/update_gitignore.py --dry-run)
    git        : commits not following 'type(scope): message' (last 200), AI attribution trailers
    tests      : public names of include/ never referenced by tests (cpp-tests/scripts/untested.py)
    docs       : README / CHANGELOG / LICENSE presence, CHANGELOG last version vs CMake version
    build      : (--build) number of compiler warnings with -Wall -Wextra -Wpedantic -Wshadow -Wconversion
Every finding keeps file:line so the report can cite it.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys
import tempfile
from collections.abc import Iterator

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

CODE = re.compile(r"\.(cpp|hpp|cc|hh|cxx|hxx|tpp|inl|c|h)$")
SKIP_DIRS = {".git", "build", "_deps", "third_party", "vendor", "external", "node_modules", "docs", ".cache"}
COMMIT = re.compile(r"^!?(feat|fix|docs|chore|test|refactor|perf|style|build|ci|release)(\([^)]+\))?!?: \S")

def run(cmd: list[str], cwd: str | None = None, timeout: int = 600) -> subprocess.CompletedProcess:
    try:
        return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    except Exception as e:
        return subprocess.CompletedProcess(cmd, 1, "", str(e))

def files_of(root: str) -> Iterator[str]:
    for d, dirs, files in os.walk(root):
        dirs[:] = [x for x in dirs if x not in SKIP_DIRS and not x.startswith("build")]
        for f in files:
            if CODE.search(f):
                yield os.path.relpath(os.path.join(d, f), root)

def strip_comments_strings(line: str, state: dict) -> str:
    """Very small C/C++ lexer: returns the code of a line without comments/strings (state = in block comment)."""
    out, i = [], 0
    while i < len(line):
        if state["block"]:
            j = line.find("*/", i)
            if j < 0: return "".join(out)
            state["block"], i = False, j + 2
            continue
        c = line[i]
        if line.startswith("//", i): break
        if line.startswith("/*", i): state["block"], i = True, i + 2; continue
        if c in "\"'":
            j = i + 1
            while j < len(line) and line[j] != c:
                j += 2 if line[j] == "\\" else 1
            out.append(c + c); i = j + 1; continue
        out.append(c); i += 1
    return "".join(out)

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("root", nargs="?", default=".")
    ap.add_argument("--json")
    ap.add_argument("--md")
    ap.add_argument("--build", action="store_true")
    a = ap.parse_args()
    root = os.path.abspath(a.root)
    R = {"root": root, "modules": {}, "findings": {}, "summary": {}}
    F = R["findings"]
    add = lambda cat, path, line, msg: F.setdefault(cat, []).append({"file": path, "line": line, "msg": msg})

    paths = sorted(files_of(root))
    for p in paths:
        parts = p.split("/")
        mod = parts[2] if len(parts) > 3 and parts[0] in ("src", "include") else parts[1] if len(parts) > 2 else parts[0]
        text = open(os.path.join(root, p), encoding="utf-8", errors="replace").read()
        lines = text.split("\n")
        m = R["modules"].setdefault(f"{parts[0]}/{mod}" if len(parts) > 1 else "root", {"files": 0, "lines": 0})
        m["files"] += 1; m["lines"] += len(lines)

        # Header
        if not re.search(r"@file|EPITECH PROJECT|File Name:", "\n".join(lines[:25])):
            add("headers", p, 1, "no file header (Xartania / Epitech)")
        # Guard
        if re.search(r"\.(hpp|h|hh|hxx)$", p):
            g = re.search(r"^#ifndef\s+(\w+)\s*\n\s*#\s*define\s+\1\b", text, re.M)
            expected = re.sub(r"[^A-Za-z0-9]", "", os.path.splitext(os.path.basename(p))[0]).upper() + "_H"
            if "#pragma once" in text: pass
            elif not g: add("guards", p, 1, "no include guard")
            elif g.group(1) != expected:
                add("guards", p, text[:g.start()].count("\n") + 1, f"guard {g.group(1)} (expected {expected})")
        # Style
        state = {"block": False}
        for n, raw in enumerate(lines, 1):
            if "\t" in raw: add("tabs", p, n, "tab character")
            if raw.rstrip("\n") != raw.rstrip() and raw.strip(): add("trailing-spaces", p, n, "trailing whitespace")
            if re.search(r"\b(TODO|FIXME|XXX|HACK)\b", raw): add("todo", p, n, raw.strip()[:90])
            code = strip_comments_strings(raw, state)
            if not code.strip() or code.lstrip().startswith("#"): continue
            # auto allowed: iterators, structured bindings, lambdas, defaulted operator<=>
            if re.search(r"\bauto\b", code) and not re.search(r"\bauto\s*&{0,2}\s*\[|\bauto\s+\w*it\w*\s*=|auto\s+\w+\s*=\s*\w[\w:.>-]*\.(c?begin|c?end|find|lower_bound|upper_bound)\(|auto\s+\w+\s*=\s*\[|operator\s*<=>|\[[^\]]*\]\s*\(", code):
                add("auto", p, n, code.strip()[:90])
            if re.search(r"\busing\s+namespace\b", code): add("using-namespace", p, n, code.strip()[:90])
            if re.search(r"\bNULL\b", code): add("null", p, n, "NULL instead of nullptr")
            if re.search(r"\(\s*(int|long|char|float|double|unsigned|size_t|std::size_t)\s*\)\s*[\w(]", code) and not re.search(r"\(void\)", code):
                add("c-cast", p, n, code.strip()[:90])
            # a return type before the name (constructors, destructors and calls are fine without (void))
            if re.search(r"^\s*(?:(?:static|virtual|inline|constexpr|explicit|_\w+)\s+)*[\w:<>,*&]+[\s*&]+\w+\s*\(\s*\)\s*(const\s*)?(noexcept\s*)?(override\s*|final\s*)?[;{]", code) \
                    and not re.search(r"\b(return|if|while|for|switch|sizeof|decltype|new|delete)\b|\boperator\b|= *default|= *delete", code) \
                    and re.search(r"\.(hpp|h|cpp|c)$", p):
                add("void-params", p, n, "empty parameter list without (void)")

    # CMake sources
    cm = os.path.join(root, "CMakeLists.txt")
    if os.path.isfile(cm):
        cmtext = open(cm, encoding="utf-8").read()
        if re.search(r"file\s*\(\s*GLOB[^)]*\.(cpp|c)\b", cmtext): add("cmake", "CMakeLists.txt", 1, "sources collected with file(GLOB)")
        alltext = cmtext + "".join(open(os.path.join(d, f), encoding="utf-8").read() for d, _, fs in os.walk(root) if "/build" not in d for f in fs if f == "CMakeLists.txt" and os.path.join(d, f) != cm)
        for p in paths:
            if re.search(r"\.(cpp|c|cc)$", p) and not p.startswith(("tests/", "test/")) and os.path.basename(p) not in alltext and p not in alltext:
                add("cmake", p, 1, "source not listed in any CMakeLists.txt")
    # .gitignore
    ug = os.path.join(skill_dir("cpp-project"), "scripts", "update_gitignore.py")
    if os.path.isfile(cm) and os.path.isfile(ug):
        out = run([sys.executable, ug, root, "--dry-run"]).stdout.split("\n")
        for l in out:
            if l and not l.startswith("#"): add("gitignore", ".gitignore", 0, f"missing: {l}")
    # Git
    if os.path.isdir(os.path.join(root, ".git")):
        log = run(["git", "-C", root, "log", "-200", "--format=%h%x00%s%x00%b%x1e"]).stdout
        total = bad = 0
        for entry in log.split("\x1e"):
            if "\x00" not in entry: continue
            h, s, b = entry.strip("\n").split("\x00", 2)
            if s.startswith("Merge "): continue
            total += 1
            if not COMMIT.match(s): bad += 1; add("commits", h, 0, s[:90])
            if re.search(r"Co-Authored-By: Claude|Generated with \[?Claude", b): add("ai-attribution", h, 0, s[:90])
        R["summary"]["commits_checked"] = total
        R["summary"]["commits_bad"] = bad
    # Tests
    ut = os.path.join(skill_dir("cpp-tests"), "scripts", "untested.py")
    if os.path.isdir(os.path.join(root, "include")) and os.path.isfile(ut):
        r = run([sys.executable, ut, root])
        R["summary"]["tests"] = r.stdout.strip().split("\n")[:3] if r.returncode == 0 else [r.stderr.strip()[:200]]
        for l in r.stdout.split("\n"): # '<module>  <referenced>  <total>  <names not referenced>'
            m = re.match(r"^(\S+)\s+(\d+)\s+(\d+)\s+(.+)$", l)
            if m and m.group(2) != m.group(3):
                for name in m.group(4).split(", "): add("untested", m.group(1), 0, name.strip())
    # Docs
    for f in ("README.md", "CHANGELOG.md", "LICENSE"):
        if not os.path.exists(os.path.join(root, f)) and not (f == "LICENSE" and any(os.path.exists(os.path.join(root, x)) for x in ("LICENSE.md", "COPYING"))):
            add("docs", f, 0, f"{f} missing")
    if os.path.isfile(cm) and os.path.isfile(os.path.join(root, "CHANGELOG.md")):
        v = re.search(r"project\([^)]*VERSION\s+([0-9.]+)", open(cm, encoding="utf-8").read())
        c = re.search(r"^## \[?v?([0-9][0-9.]*)", open(os.path.join(root, "CHANGELOG.md"), encoding="utf-8").read(), re.M)
        if v and c and v.group(1) != c.group(1): add("docs", "CHANGELOG.md", 0, f"last entry v{c.group(1)} but CMake version v{v.group(1)}")
    # Build warnings
    if a.build and os.path.isfile(cm):
        b = tempfile.mkdtemp(prefix="audit-build-")
        flags = "-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor"
        run(["cmake", "-S", root, "-B", b, f"-DCMAKE_CXX_FLAGS={flags}", "-DBUILD_TESTS=OFF"])
        out = run(["cmake", "--build", b, "--parallel", str(os.cpu_count() or 4)], timeout=1800)
        warns = re.findall(rf"^{re.escape(root)}/([^:\n]+):(\d+):\d+: warning: (.+?) \[(-W[\w-]+)\]", out.stdout + out.stderr, re.M)
        seen = set()
        for f, l, msg, flag in warns:
            if (f, l, flag) in seen: continue
            seen.add((f, l, flag)); add("warnings", f, int(l), f"{flag}: {msg[:80]}")
        R["summary"]["build"] = "ok" if out.returncode == 0 else "failed"
        R["summary"]["build_dir"] = b

    R["summary"]["files"] = len(paths)
    R["summary"]["lines"] = sum(m["lines"] for m in R["modules"].values())
    R["summary"]["counts"] = {k: len(v) for k, v in sorted(F.items())}
    if a.json:
        json.dump(R, open(a.json, "w", encoding="utf-8"), indent=1)
    md = ["| Category | Findings |", "|---|---|"] + [f"| `{k}` | {v} |" for k, v in R["summary"]["counts"].items()]
    md += ["", "| Module | Files | Lines |", "|---|---|---|"] + [f"| `{k}` | {v['files']} | {v['lines']} |" for k, v in sorted(R["modules"].items())]
    if a.md:
        open(a.md, "w", encoding="utf-8").write("\n".join(md) + "\n")
    print(f"{R['summary']['files']} files, {R['summary']['lines']} lines")
    print("\n".join(md[:2 + len(R['summary']['counts'])]))
    return 0

if __name__ == "__main__":
    sys.exit(main())
