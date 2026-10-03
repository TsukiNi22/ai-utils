#!/usr/bin/env python3
"""
List what the unit tests of a C++ project don't reference yet (helps to "test the whole project").

Usage:
    untested.py [<project root>] [--methods] [--tests tests] [--include include] [--all]

For every header of include/ (or --include), extracts the classes / structs / enums and the free functions
(+ the public methods with --methods), then looks for each name in the files of tests/ (word match).
Prints a coverage table per module (include/<root>/<module>/) and the names never referenced.
A referenced name is not a tested behavior: use it as an inventory, not as a coverage metric.
--all: also list the referenced names.
"""

import argparse
import os
import re
import sys

HEADER = (".hpp", ".hh", ".hxx", ".h", ".tpp", ".inl")
SOURCE = HEADER + (".cpp", ".cc", ".cxx", ".c")
SKIP = {"if", "for", "while", "switch", "return", "sizeof", "decltype", "alignas", "requires", "static_assert",
        "noexcept", "throw", "operator", "static_cast", "reinterpret_cast", "const_cast", "dynamic_cast"}


def strip(text: str) -> str:
    """Remove comments, strings and preprocessor lines (structure kept)."""
    text = re.sub(r"//[^\n]*|/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r'R"([^(]*)\(.*?\)\1"', '""', text, flags=re.S)
    text = re.sub(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', '""', text)
    return re.sub(r"^[ \t]*#(?:[^\n]*\\\n)*[^\n]*", "", text, flags=re.M)


def func_name(head: str) -> str:
    head = re.sub(r"^template\s*<.*?>\s*", "", head.strip(), flags=re.S)
    head = re.sub(r"\[\[.*?\]\]", " ", head) # attributes ([[deprecated("...")]])
    for m in re.finditer(r"(~?[A-Za-z_][\w:]*)\s*\(", head):
        name = m.group(1).split("::")[-1]
        if name in SKIP or re.fullmatch(r"_[a-z]+", name): continue # attribute macros (_hot, _nodiscard...)
        return m.group(1)
    return ""


def declarations(text: str, methods: bool) -> list[tuple[str, str]]:
    """[(kind, name)] declared in a header."""
    code, out = strip(text), []
    stack, head, skip = [], "", 0 # stack of (type, name, access)
    for c in code:
        if skip:
            skip += c == "{"
            skip -= c == "}"
            if not skip: head = ""
            continue
        if c == "{":
            h = re.sub(r"\s+", " ", head).strip()
            h = re.sub(r"^(?:(?:public|private|protected)\s*:\s*)+", "", h)
            head = ""
            t = re.sub(r"^template\s*<.*?>\s*", "", h)
            if re.match(r"^(inline\s+)?namespace\b", t) or t in ("", 'extern ""'):
                stack.append(("ns", "", "public"))
                continue
            m = re.match(r"^(class|struct|union)\s+(?:(?:\[\[.*?\]\]|alignas\([^)]*\)|_[a-z]+)\s+)*([A-Za-z_]\w*)", t)
            if m and "(" not in t.split(":")[0]:
                if all(s[2] == "public" for s in stack): out.append(("class", m.group(2)))
                stack.append(("class", m.group(2), "private" if m.group(1) == "class" else "public"))
                continue
            m = re.match(r"^enum\s+(?:class\s+|struct\s+)?([A-Za-z_]\w*)", t)
            if m:
                if all(s[2] == "public" for s in stack): out.append(("enum", m.group(1)))
                skip = 1
                continue
            if "(" in t: declared(t, stack, out, methods)
            skip = 1
            continue
        if c == "}":
            if stack: stack.pop()
            head = ""
            continue
        if c == ";":
            h = re.sub(r"\s+", " ", head).strip()
            head = ""
            m = re.match(r"^((?:(?:public|private|protected)\s*:\s*)+)", h)
            if m and stack and stack[-1][0] == "class":
                stack[-1] = (stack[-1][0], stack[-1][1], m.group(1).split(":")[-2].strip())
                h = h[m.end():]
            if "(" in h and not re.match(r"^(using|typedef|friend|static_assert|return)\b", h) and not re.search(r"=\s*delete$", h):
                declared(h, stack, out, methods)
            continue
        head += c
        m = re.search(r"(public|private|protected)\s*:(?!:)\s*$", head)
        if m and stack and stack[-1][0] == "class":
            stack[-1] = (stack[-1][0], stack[-1][1], m.group(1))
            head = ""
    return out


def declared(h: str, stack: list, out: list, methods: bool) -> None:
    full = func_name(h)
    if not full: return
    name = full.split("::")[-1]
    in_class = stack and stack[-1][0] == "class"
    if in_class:
        cls, access = stack[-1][1], stack[-1][2]
        if methods and access == "public" and name not in (cls, "~" + cls) and not name.startswith("~"):
            out.append(("method", f"{cls}::{name}"))
    elif all(s[0] == "ns" for s in stack) and "::" not in full and name != "main":
        out.append(("function", name))


def module_of(rel: str) -> str:
    parts = rel.split("/")
    return parts[1] if len(parts) >= 3 else (parts[0] if len(parts) == 2 else "root")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("root", nargs="?", default=".")
    ap.add_argument("--methods", action="store_true")
    ap.add_argument("--tests", default="tests")
    ap.add_argument("--include", default="include")
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    root = os.path.abspath(a.root)
    inc, tst = os.path.join(root, a.include), os.path.join(root, a.tests)
    if not os.path.isdir(inc):
        print(f"Error: no {a.include}/ in {root}", file=sys.stderr); return 1

    words = set()
    if os.path.isdir(tst):
        for d, _, files in os.walk(tst):
            for f in files:
                if f.endswith(SOURCE):
                    words |= set(re.findall(r"[A-Za-z_]\w*", strip(open(os.path.join(d, f), errors="replace").read())))
    else:
        print(f"warning: no {a.tests}/ directory: nothing is tested yet", file=sys.stderr)

    modules: dict[str, dict] = {}
    for d, _, files in os.walk(inc):
        for f in sorted(files):
            if not f.endswith(HEADER) or f.startswith("generated_"): continue
            path = os.path.join(d, f)
            rel = os.path.relpath(path, inc)
            mod = modules.setdefault(module_of(rel), {"done": [], "todo": []})
            for kind, name in dict.fromkeys(declarations(open(path, errors="replace").read(), a.methods)):
                hit = all(p in words for p in name.split("::"))
                mod["done" if hit else "todo"].append((kind, name, rel))

    total_done = sum(len(m["done"]) for m in modules.values())
    total = total_done + sum(len(m["todo"]) for m in modules.values())
    print(f"{'Module':<20} {'Referenced':>11} {'Total':>6}  Not referenced")
    for name in sorted(modules):
        m = modules[name]
        n = len(m["done"]) + len(m["todo"])
        if not n: continue
        todo = ", ".join(sorted({x[1] for x in m["todo"]}))
        print(f"{name:<20} {len(m['done']):>11} {n:>6}  {todo[:150] + ('...' if len(todo) > 150 else '')}")
    print(f"{'TOTAL':<20} {total_done:>11} {total:>6}  ({(100 * total_done // total) if total else 0}% of the public names referenced by {a.tests}/)")
    print()
    for name in sorted(modules):
        for kind, n, rel in sorted(modules[name]["todo"], key=lambda x: (x[2], x[1])):
            print(f"untested  {kind:<8} {n:<40} include/{rel}" if a.include == "include" else f"untested  {kind:<8} {n:<40} {rel}")
        if a.all:
            for kind, n, rel in sorted(modules[name]["done"], key=lambda x: (x[2], x[1])):
                print(f"tested    {kind:<8} {n:<40} {rel}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
