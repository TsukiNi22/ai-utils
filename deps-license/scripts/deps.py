#!/usr/bin/env python3
"""
Inventory the dependencies of a project with their version, origin and license, then classify the licenses.

Usage:
    deps.py [<project root>] [--transitive] [--json out.json] [--md out.md] [--notices THIRD_PARTY_NOTICES.md]

Sources:
    C/C++   : CMake find_package / pkg_check_modules / FetchContent / ExternalProject, git submodules,
              vendored folders (third_party, vendor, external, libs, cmake/libs), non standard #include <...>;
              resolved to the system package that owns the file (rpm -qf / dpkg -S) for version + license,
              or the LICENSE file of the vendored / fetched code
    Python  : requirements*.txt, pyproject.toml (pip metadata when installed, else PyPI)
    JS/TS   : package.json (+ package-lock.json for the transitive ones), npm registry
    Rust    : Cargo.toml (+ Cargo.lock), crates.io
--transitive: also the library packages required by the system packages (rpm --requires, 2 levels) and the
              lockfile entries.
Classification (category per dependency): public-domain, permissive (attribution), permissive-notice
(Apache NOTICE), weak-copyleft, strong-copyleft, network-copyleft, non-commercial, source-available,
proprietary, unknown/none.
"""

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request

STD = set("""algorithm any array atomic barrier bit bitset cassert cctype cerrno cfenv cfloat charconv chrono cinttypes
climits clocale cmath codecvt compare complex concepts condition_variable coroutine csetjmp csignal cstdarg cstddef
cstdint cstdio cstdlib cstring ctime cuchar cwchar cwctype deque exception execution expected filesystem format
forward_list fstream functional future initializer_list iomanip ios iosfwd iostream istream iterator latch limits
list locale map memory memory_resource mutex new numbers numeric optional ostream print queue random ranges ratio
regex scoped_allocator semaphore set shared_mutex source_location span spanstream sstream stack stacktrace
stdexcept stop_token streambuf string string_view strstream syncstream system_error thread tuple type_traits
typeindex typeinfo unordered_map unordered_set utility valarray variant vector version""".split())
SYS_DIRS = ("sys", "netinet", "arpa", "linux", "bits", "net", "asm", "gnu")
POSIX = set("""unistd fcntl poll dlfcn signal errno termios pthread sched semaphore dirent netdb pwd grp glob spawn
syslog time string strings stdio stdlib math assert limits stdint stddef ctype locale wchar wctype setjmp stdarg
float iso646 fenv inttypes stdbool complex tgmath uchar execinfo cxxabi elf link malloc alloca getopt libgen regex
fnmatch ifaddrs utime ucontext""".split())
VENDOR_DIRS = ("third_party", "thirdparty", "3rdparty", "vendor", "external", "extern", "deps", "libs", "cmake/libs")


def run(cmd):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=60).stdout.strip()
    except Exception:
        return ""


def http_json(url):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "deps-license"}), timeout=15) as r:
            return json.load(r)
    except Exception:
        return None


# ---------------------------------------------------------------- #
# License classification

SYSTEM_RUNTIME = re.compile(r"^(glibc|libgcc|libstdc\+\+|libatomic|libgomp|kernel|linux-libc|musl|libxcrypt)(-|$)")


def classify(lic: str, name: str = "") -> str:
    if SYSTEM_RUNTIME.match(name):
        return "system-runtime"
    if not lic or lic.strip().lower() in ("", "unknown", "none", "noassertion", "unlicensed"):
        return "unknown"
    l = lic.lower()
    if re.search(r"proprietary|commercial|redistributable,? no modification|freeware|eula|nvidia|all rights reserved", l):
        return "proprietary"
    if re.search(r"\bnc\b|non-?commercial|polyform-noncommercial|cc-by-nc", l):
        return "non-commercial"
    if re.search(r"busl|bsl-1\.1|business source|sspl|elastic-2|commons clause|confluent|polyform", l):
        return "source-available"
    if "agpl" in l or "affero" in l:
        return "network-copyleft"
    if re.search(r"(?<!l)gpl|gnu general public", l) and not re.search(r"with (gcc|classpath|llvm|bison|autoconf|libtool|font|linking)[- ]?exception", l):
        return "strong-copyleft"
    if re.search(r"lgpl|mpl|epl|cddl|eupl|osl|cecill-c|ms-rl|gpl.*exception", l):
        return "weak-copyleft"
    if re.search(r"apache", l):
        return "permissive-notice"
    if re.search(r"\bmit\b|bsd|isc|zlib|libpng|bsl-1\.0|boost|x11|curl|openssl|ssleay|python|psf|unicode|ofl|cc-by(?!-nc|-sa)|w3c|ncsa|0bsd", l):
        return "permissive"
    if re.search(r"cc0|unlicense|public domain|wtfpl", l):
        return "public-domain"
    if "cc-by-sa" in l:
        return "weak-copyleft"
    return "unknown"


OBLIGATIONS = {
    "system-runtime": "system C/C++ runtime: its exceptions (GCC runtime / LGPL dynamic) allow any use, nothing to credit",
    "public-domain": "nothing required",
    "permissive": "keep the copyright + license text in the distributed sources/binaries (THIRD_PARTY_NOTICES)",
    "permissive-notice": "license text + the NOTICE file of the project, state the changes",
    "weak-copyleft": "file/library level copyleft: changes to the library stay open; LGPL: allow relinking (dynamic link or provide objects)",
    "strong-copyleft": "the whole distributed program must be GPL-compatible and its sources provided",
    "network-copyleft": "AGPL: sources must be offered to network users too (SaaS included)",
    "non-commercial": "NO commercial use",
    "source-available": "not open source: usage limits (production/SaaS/competition), check the terms",
    "proprietary": "proprietary / paid terms: check the license (redistribution, fees, EULA)",
    "unknown": "NO license found = all rights reserved: do not use/distribute until clarified",
}


# ---------------------------------------------------------------- #
# System packages (rpm / dpkg)

def pkg_of_file(path):
    if shutil.which("rpm"):
        out = run(["rpm", "-qf", "--qf", "%{NAME}\n", path])
        if out and "not owned" not in out and "n'appartient" not in out:
            return out.split("\n")[0]
    if shutil.which("dpkg"):
        out = run(["dpkg", "-S", path])
        if out and ":" in out:
            return out.split(":")[0]
    return None


def pkg_info(name):
    if shutil.which("rpm"):
        out = run(["rpm", "-q", "--qf", "%{NAME}|%{VERSION}-%{RELEASE}|%{LICENSE}|%{URL}\n", name])
        if out and "|" in out:
            n, v, l, u = out.split("\n")[0].split("|", 3)
            return {"name": n, "version": v, "license": l, "url": u, "ecosystem": "rpm"}
    if shutil.which("dpkg-query"):
        out = run(["dpkg-query", "-W", "-f", "${Package}|${Version}|${Homepage}", name])
        if out and "|" in out:
            n, v, u = out.split("|", 2)
            lic = ""
            cp = f"/usr/share/doc/{n}/copyright"
            if os.path.exists(cp):
                lic = ", ".join(sorted(set(re.findall(r"^License:\s*(.+)$", open(cp, errors="replace").read(), re.M))))
            return {"name": n, "version": v, "license": lic, "url": u, "ecosystem": "deb"}
    return None


def requires_of(pkg, depth):
    """Library packages required by a system package (rpm only)."""
    if not shutil.which("rpm") or depth <= 0:
        return set()
    found = set()
    for req in run(["rpm", "-q", "--requires", pkg]).split("\n"):
        req = req.strip().split(" ")[0]
        if not req or req.startswith(("rpmlib(", "/")) or "(" in req and not req.startswith("lib"):
            continue
        prov = run(["rpm", "-q", "--whatprovides", "--qf", "%{NAME}\n", req]).split("\n")[0]
        if prov and "no package" not in prov and "aucun paquet" not in prov and prov != pkg and \
                re.match(r"^(lib|glibc|openssl|zlib|gcc|libstdc|boost|json|fmt|spdlog|gtest|cpp-|libconfig)", prov):
            found.add(prov)
    deeper = set()
    for p in found:
        deeper |= requires_of(p, depth - 1)
    return found | deeper


# ---------------------------------------------------------------- #
# Inventory

def license_file_of(path):
    for f in sorted(glob.glob(os.path.join(path, "*"))):
        if re.match(r"(?i)(license|licence|copying|unlicense)(\.|$|-)", os.path.basename(f)) and os.path.isfile(f):
            return f
    return None


def guess_license_text(path):
    try:
        t = open(path, errors="replace").read()[:4000]
    except Exception:
        return ""
    checks = [("Apache-2.0", r"Apache License,?\s+Version 2\.0"), ("GPL-3.0", r"GNU GENERAL PUBLIC LICENSE\s+Version 3"),
              ("GPL-2.0", r"GNU GENERAL PUBLIC LICENSE\s+Version 2"), ("LGPL", r"GNU LESSER GENERAL PUBLIC"),
              ("AGPL-3.0", r"GNU AFFERO"), ("MPL-2.0", r"Mozilla Public License,? v(ersion)? ?2\.0"),
              ("BSL-1.0", r"Boost Software License"), ("MIT", r"Permission is hereby granted, free of charge"),
              ("BSD-3-Clause", r"Neither the name"), ("BSD-2-Clause", r"Redistribution and use in source and binary forms"),
              ("ISC", r"Permission to use, copy, modify, and/or distribute"), ("Zlib", r"This software is provided 'as-is'"),
              ("Unlicense", r"This is free and unencumbered software"), ("CC0-1.0", r"CC0 1\.0")]
    for spdx, rx in checks:
        if re.search(rx, t, re.I):
            return spdx
    return ""


def cmake_commands(text):
    text = re.sub(r"#[^\n]*", "", text)
    for m in re.finditer(r"([A-Za-z_]\w*)\s*\(", text):
        depth, i = 1, m.end()
        while i < len(text) and depth:
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
        yield m.group(1).lower(), re.findall(r'"[^"]*"|[^\s"]+', text[m.end():i - 1])


def find_cmake_config(name):
    pats = [f"/usr/lib*/cmake/{name}*/{name}Config.cmake", f"/usr/lib*/cmake/{name}*/{name.lower()}-config.cmake",
            f"/usr/share/cmake/{name}*/{name}Config.cmake", f"/usr/share/{name.lower()}*/cmake/{name}Config.cmake",
            f"/usr/local/lib*/cmake/{name}*/{name}Config.cmake", f"/usr/lib*/cmake/{name.lower()}*/*onfig.cmake"]
    for p in pats:
        hits = glob.glob(p)
        if hits:
            return hits[0]
    return None


def inventory(root, transitive):
    deps = {}

    def add(key, **kw):
        d = deps.setdefault(key, {"name": key, "version": "", "license": "", "url": "", "ecosystem": "", "source": [], "direct": True})
        for k, v in kw.items():
            if k == "source":
                if v not in d["source"]: d["source"].append(v)
            elif v and not d.get(k): d[k] = v
        return d

    # CMake
    for cm in glob.glob(os.path.join(root, "**", "CMakeLists.txt"), recursive=True):
        if "/build" in cm or "/_deps/" in cm:
            continue
        rel = os.path.relpath(cm, root)
        for name, args in cmake_commands(open(cm, errors="replace").read()):
            if name == "find_package" and args:
                pkg = args[0]
                if pkg in ("PkgConfig", "Python3", "Python", "Threads", "GTest") and pkg != "GTest":
                    continue
                cfg = find_cmake_config(pkg)
                sysp = pkg_of_file(cfg) if cfg else None
                info = pkg_info(sysp) if sysp else None
                if info:
                    add(info["name"], **{k: v for k, v in info.items() if k != "name"}, source=f"{rel}: find_package({pkg})")
                else:
                    add(pkg, source=f"{rel}: find_package({pkg})" + (f" -> {cfg} (not owned by a package: source install?)" if cfg else " (not found on this system)"))
            elif name == "pkg_check_modules" and len(args) > 1:
                for mod in [a for a in args[1:] if a not in ("REQUIRED", "QUIET", "IMPORTED_TARGET", "GLOBAL", "NO_CMAKE_PATH", "NO_CMAKE_ENVIRONMENT_PATH")]:
                    mod = re.split(r"[<>=]", mod)[0]
                    pc = run(["pkg-config", "--variable=pcfiledir", mod])
                    sysp = pkg_of_file(os.path.join(pc, mod + ".pc")) if pc else None
                    info = pkg_info(sysp) if sysp else None
                    if info:
                        add(info["name"], **{k: v for k, v in info.items() if k != "name"}, source=f"{rel}: pkg_check_modules({mod})")
                    else:
                        add(mod, source=f"{rel}: pkg_check_modules({mod})")
            elif name in ("fetchcontent_declare", "externalproject_add") and args:
                url = next((args[i + 1] for i, a in enumerate(args[:-1]) if a in ("GIT_REPOSITORY", "URL")), "")
                tag = next((args[i + 1] for i, a in enumerate(args[:-1]) if a == "GIT_TAG"), "")
                add(args[0], version=tag, url=url, ecosystem="git", source=f"{rel}: {name}")
    # Git submodules
    gm = os.path.join(root, ".gitmodules")
    if os.path.exists(gm):
        for path, url in re.findall(r"path\s*=\s*(\S+)\s*\n\s*url\s*=\s*(\S+)", open(gm).read()):
            sha = run(["git", "-C", root, "ls-tree", "HEAD", path]).split()
            lf = license_file_of(os.path.join(root, path))
            add(os.path.basename(path), version=sha[2] if len(sha) > 2 else "", url=url, ecosystem="git",
                license=guess_license_text(lf) if lf else "", source=f".gitmodules: {path}")
    # Vendored code
    for vd in VENDOR_DIRS:
        base = os.path.join(root, vd)
        if not os.path.isdir(base):
            continue
        for entry in sorted(os.listdir(base)):
            p = os.path.join(base, entry)
            if os.path.isdir(p):
                lf = license_file_of(p)
                add(entry, ecosystem="vendored", license=guess_license_text(lf) if lf else "", source=f"{vd}/{entry}" + ("" if lf else " (no license file)"))
            elif entry.endswith((".a", ".so", ".lib", ".dll")):
                add(entry, ecosystem="prebuilt", source=f"{vd}/{entry} (prebuilt binary: origin and license to check)")
    # Non standard includes
    seen_inc = set()
    for f in glob.glob(os.path.join(root, "**", "*.[ch]*"), recursive=True):
        if "/build" in f or not re.search(r"\.(c|cc|cpp|cxx|h|hh|hpp|hxx|tpp|inl)$", f):
            continue
        for inc in re.findall(r'^\s*#\s*include\s*<([^>]+)>', open(f, errors="replace").read(), re.M):
            first = inc.split("/")[0]
            base = re.sub(r"\.(h|hpp|hh|hxx|h\+\+)$", "", first)
            if inc in seen_inc or inc in STD or first in SYS_DIRS or base in POSIX or (not "/" in inc and "." not in inc):
                continue
            seen_inc.add(inc)
            for d in ("/usr/include", "/usr/local/include"):
                if os.path.exists(os.path.join(d, inc)):
                    sysp = pkg_of_file(os.path.join(d, inc))
                    info = pkg_info(sysp) if sysp else None
                    if info and not info["name"].startswith(("glibc", "kernel-headers", "libstdc++")):
                        add(info["name"], **{k: v for k, v in info.items() if k != "name"}, source=f"#include <{inc}>")
                    elif not info:
                        add(first, source=f"#include <{inc}> -> {d} (not owned by a package: source install?)")
                    break
    # Python
    reqs = glob.glob(os.path.join(root, "requirements*.txt")) + glob.glob(os.path.join(root, "**", "requirements*.txt"), recursive=True)
    for rf in sorted(set(reqs)):
        if "/build" in rf: continue
        for line in open(rf, errors="replace"):
            m = re.match(r"^\s*([A-Za-z0-9_.-]+)\s*(?:[=<>~!]=?\s*([\w.]+))?", line)
            if not m or line.strip().startswith(("#", "-")): continue
            name, ver = m.group(1), m.group(2) or ""
            meta = http_json(f"https://pypi.org/pypi/{name}/{ver}/json" if ver else f"https://pypi.org/pypi/{name}/json")
            lic = ""
            if meta:
                info = meta.get("info", {})
                lic = info.get("license_expression") or info.get("license") or ""
                if not lic or len(lic) > 80:
                    cl = [c.split(" :: ")[-1] for c in info.get("classifiers", []) if c.startswith("License ::")]
                    lic = ", ".join(cl) or lic[:80]
                ver = ver or info.get("version", "")
            add(f"pypi:{name}", version=ver, license=lic, ecosystem="PyPI", source=os.path.relpath(rf, root))
    # npm
    pj = os.path.join(root, "package.json")
    if os.path.exists(pj):
        data = json.load(open(pj))
        lock = os.path.join(root, "package-lock.json")
        locked = json.load(open(lock)).get("packages", {}) if os.path.exists(lock) else {}
        direct = {**data.get("dependencies", {}), **data.get("devDependencies", {})}
        for name, spec in direct.items():
            e = locked.get(f"node_modules/{name}", {})
            add(f"npm:{name}", version=e.get("version", spec), license=e.get("license", ""), ecosystem="npm", source="package.json")
        if transitive:
            for key, e in locked.items():
                if key.startswith("node_modules/") and key[13:] not in direct:
                    d = add(f"npm:{key.split('node_modules/')[-1]}", version=e.get("version", ""), license=e.get("license", ""), ecosystem="npm", source="package-lock.json")
                    d["direct"] = False
    # Rust
    cl = os.path.join(root, "Cargo.lock")
    if os.path.exists(cl) and (transitive or True):
        direct = set(re.findall(r'^\s*([A-Za-z0-9_-]+)\s*=', open(os.path.join(root, "Cargo.toml")).read().split("[dependencies]")[-1], re.M)) if os.path.exists(os.path.join(root, "Cargo.toml")) else set()
        for name, ver in re.findall(r'\[\[package\]\]\s*name = "([^"]+)"\s*version = "([^"]+)"', open(cl).read()):
            if name not in direct and not transitive: continue
            meta = http_json(f"https://crates.io/api/v1/crates/{name}/{ver}")
            d = add(f"crates:{name}", version=ver, license=(meta or {}).get("version", {}).get("license", ""), ecosystem="crates.io", source="Cargo.lock")
            d["direct"] = name in direct

    # Transitive system libraries
    if transitive:
        for d in list(deps.values()):
            if d["ecosystem"] in ("rpm",):
                for sub in requires_of(d["name"], 2):
                    if sub not in deps:
                        info = pkg_info(sub)
                        if info:
                            n = add(sub, **{k: v for k, v in info.items() if k != "name"}, source=f"required by {d['name']}")
                            n["direct"] = False
    for d in deps.values():
        d["category"] = classify(d["license"], d["name"])
        d["obligation"] = OBLIGATIONS[d["category"]]
        # Only used by the tests (not distributed with the product)
        d["scope"] = "test" if d["source"] and all(re.match(r"^(tests?/|#include <gtest|#include <gmock|.*find_package\(GTest)", s) or "tests/" in s.split(":")[0] for s in d["source"]) else "product"
        if d["scope"] == "test" and d["category"] not in ("non-commercial", "proprietary", "unknown", "source-available"):
            d["obligation"] = "test only (not distributed): no obligation for the product"
    # Transitive dependencies inherit the scope of what requires them
    for d in deps.values():
        parents = [s[len("required by "):] for s in d["source"] if s.startswith("required by ")]
        if parents and all(deps.get(p, {}).get("scope") == "test" for p in parents):
            d["scope"] = "test"
            if d["category"] not in ("non-commercial", "proprietary", "unknown", "source-available", "system-runtime"):
                d["obligation"] = "test only (not distributed): no obligation for the product"
    return sorted(deps.values(), key=lambda d: (not d["direct"], d["name"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root", nargs="?", default=".")
    ap.add_argument("--transitive", action="store_true")
    ap.add_argument("--json")
    ap.add_argument("--md")
    ap.add_argument("--notices")
    a = ap.parse_args()
    root = os.path.abspath(a.root)
    deps = inventory(root, a.transitive)
    if a.json:
        json.dump(deps, open(a.json, "w"), indent=1)
    md = ["| Dependency | Version | License | Category | Scope | Direct | Found in |", "|---|---|---|---|---|---|---|"]
    for d in deps:
        lic = d['license'] if len(d['license']) < 70 else d['license'][:67] + "..."
        md.append(f"| `{d['name']}` | {d['version'] or '?'} | {lic or '**none found**'} | {d['category']} | {d['scope']} | "
                  f"{'yes' if d['direct'] else 'no'} | {'; '.join(d['source'])[:90]} |")
    if a.md:
        open(a.md, "w").write("\n".join(md) + "\n")
    if a.notices:
        out = ["# Third-party notices", "", "This project uses the following third-party software.", ""]
        for d in deps:
            if d["category"] in ("public-domain",):
                continue
            out += [f"## {d['name']} {d['version']}", "", f"- License: {d['license'] or 'unknown'}", f"- Home: {d['url'] or '-'}", ""]
            texts = glob.glob(f"/usr/share/licenses/{d['name']}/*") + glob.glob(f"/usr/share/doc/{d['name']}/copyright")
            for t in texts[:3]:
                out += ["```", open(t, errors="replace").read().strip()[:6000], "```", ""]
        open(a.notices, "w").write("\n".join(out))
    print("\n".join(md))
    cats = {}
    for d in deps:
        cats[d["category"]] = cats.get(d["category"], 0) + 1
    print("\n" + ", ".join(f"{k}: {v}" for k, v in sorted(cats.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
