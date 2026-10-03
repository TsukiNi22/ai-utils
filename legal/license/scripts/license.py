#!/usr/bin/env python3
"""
License helper: fetch an official license text, identify the license of a file, list the known ones.

Usage:
    license.py list
    license.py info <SPDX-ID>
    license.py fetch <SPDX-ID> [--holder NAME] [--year YYYY] [--out FILE] [--force]
    license.py identify <FILE>
    license.py exception <SPDX-EXCEPTION-ID> [--out FILE]

Texts come from choosealicense.com (clean [year]/[fullname] placeholders and the permissions/conditions/
limitations metadata) or the SPDX license list (every SPDX id and exception). Never write a license text by
hand. Downloads are cached in ~/.cache/license-skill.
"""

import argparse
import datetime
import difflib
import os
import re
import subprocess
import sys
import urllib.request

CHOOSE = "https://raw.githubusercontent.com/github/choosealicense.com/gh-pages/_licenses/{}.txt"
SPDX = "https://raw.githubusercontent.com/spdx/license-list-data/main/text/{}.txt"
CACHE = os.path.expanduser("~/.cache/license-skill")

# Common licenses (SPDX ids) compared by `identify` and shown by `list`
COMMON = [
    "MIT", "MIT-0", "Apache-2.0", "BSD-2-Clause", "BSD-3-Clause", "0BSD", "ISC", "Zlib", "BSL-1.0",
    "Unlicense", "CC0-1.0", "WTFPL", "MPL-2.0", "EPL-2.0", "LGPL-2.1-only", "LGPL-3.0-only", "GPL-2.0-only",
    "GPL-3.0-only", "AGPL-3.0-only", "EUPL-1.2", "CC-BY-4.0", "CC-BY-SA-4.0", "OFL-1.1", "BUSL-1.1",
    "Elastic-2.0", "PolyForm-Noncommercial-1.0.0", "PolyForm-Small-Business-1.0.0", "SSPL-1.0",
]


def get(url: str) -> str | None:
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, re.sub(r"[^\w.-]", "_", url.split("://", 1)[1]))
    if os.path.exists(path):
        return open(path, encoding="utf-8").read()
    try:
        with urllib.request.urlopen(url, timeout=20) as r:
            text = r.read().decode("utf-8")
    except Exception:
        return None
    open(path, "w", encoding="utf-8").write(text)
    return text


def choose_id(spdx: str) -> str:
    return re.sub(r"-(only|or-later)$", "", spdx).lower()


def from_choosealicense(spdx: str) -> tuple[dict, str] | None:
    raw = get(CHOOSE.format(choose_id(spdx)))
    if not raw or not raw.startswith("---"):
        return None
    _, front, body = raw.split("---", 2)
    meta, key = {}, None
    for line in front.split("\n"):
        m = re.match(r"^(\w[\w-]*):\s*(.*)$", line)
        if m:
            key = m.group(1)
            meta[key] = m.group(2).strip() if m.group(2).strip() else []
        elif key and line.strip().startswith("- ") and isinstance(meta.get(key), list):
            meta[key].append(line.strip()[2:])
    return meta, body.lstrip("\n")


def text_of(spdx: str) -> tuple[str, str]:
    c = from_choosealicense(spdx)
    if c and (c[0].get("spdx-id", "").lower() == spdx.lower() or choose_id(spdx) != spdx.lower()):
        return c[1], "choosealicense.com"
    t = get(SPDX.format(spdx))
    if t:
        return t, "SPDX license list"
    if c:
        return c[1], "choosealicense.com"
    raise SystemExit(f"Error: no text found for '{spdx}' (check the SPDX id: https://spdx.org/licenses/)")


def holder_default() -> str:
    try:
        return subprocess.run(["git", "config", "user.name"], capture_output=True, text=True).stdout.strip()
    except Exception:
        return ""


def fill(text: str, holder: str, year: str) -> str:
    for p in ("[year]", "<year>"):
        text = text.replace(p, year)
    for p in ("[fullname]", "<copyright holders>", "<owner>", "<OWNER>", "<COPYRIGHT HOLDER>", "<name of author>"):
        if holder:
            text = text.replace(p, holder)
    return text


def normalize(text: str) -> list[str]:
    text = re.sub(r"(?im)^.*copyright.*\d{4}.*$", " ", text) # drop the copyright lines
    text = text.lower().replace("&", "and")
    return re.findall(r"[a-z0-9]+", text)


def cmd_list(_: argparse.Namespace) -> int:
    print("Common SPDX ids (any id of https://spdx.org/licenses/ works with fetch):")
    for i in COMMON:
        print("  " + i)
    print("Exceptions (exception command): Classpath-exception-2.0, LLVM-exception, GCC-exception-3.1, "
          "Autoconf-exception-3.0, OpenJDK-assembly-exception-1.0 ...")
    print("Not in SPDX: Commons Clause (https://commonsclause.com, added on top of a license)")
    return 0


def cmd_info(a: argparse.Namespace) -> int:
    c = from_choosealicense(a.id)
    if not c:
        print(f"{a.id}: no choosealicense metadata (see https://spdx.org/licenses/{a.id}.html)")
        return 0
    meta = c[0]
    print(f"{meta.get('title', a.id)} ({meta.get('spdx-id', a.id)})")
    print("  " + str(meta.get("description", "")))
    for k in ("permissions", "conditions", "limitations"):
        print(f"  {k}: {', '.join(meta.get(k) or []) or '-'}")
    return 0


def cmd_fetch(a: argparse.Namespace) -> int:
    text, source = text_of(a.id)
    holder = a.holder if a.holder is not None else holder_default()
    text = fill(text, holder, a.year)
    left = sorted(set(re.findall(r"\[(?:year|fullname)\]|<(?:year|copyright holders|owner)>", text)))
    if a.out:
        if os.path.exists(a.out) and not a.force:
            print(f"Error: {a.out} exists (confirm with the user, then --force)", file=sys.stderr)
            return 1
        open(a.out, "w", encoding="utf-8").write(text if text.endswith("\n") else text + "\n")
        print(f"{a.id} ({source}) -> {a.out}  holder: {holder or '-'}  year: {a.year}")
    else:
        sys.stdout.write(text)
    if left:
        print(f"warning: placeholders left: {', '.join(left)}", file=sys.stderr)
    return 0


def cmd_exception(a: argparse.Namespace) -> int:
    t = get(SPDX.format(a.id))
    if not t:
        print(f"Error: exception '{a.id}' not found", file=sys.stderr)
        return 1
    if a.out:
        open(a.out, "w", encoding="utf-8").write(t)
        print(f"{a.id} -> {a.out}")
    else:
        sys.stdout.write(t)
    return 0


def cmd_identify(a: argparse.Namespace) -> int:
    target = normalize(open(a.file, encoding="utf-8", errors="replace").read())
    if not target:
        print("empty file")
        return 1
    scores = []
    for spdx in COMMON:
        try:
            ref = normalize(text_of(spdx)[0])
        except SystemExit:
            continue
        m = difflib.SequenceMatcher(None, target, ref, autojunk=False)
        if m.real_quick_ratio() < 0.3:
            continue
        scores.append((m.ratio(), spdx))
    scores.sort(reverse=True)
    if not scores:
        print("unknown license (no common license above 30% similarity)")
        return 0
    best, spdx = scores[0]
    verdict = "match" if best > 0.95 else "probably (modified)" if best > 0.80 else "unknown / custom"
    print(f"{a.file}: {spdx} ({best:.0%}, {verdict})")
    for s, i in scores[1:3]:
        print(f"  then {i} ({s:.0%})")
    holders = re.findall(r"(?im)^.*copyright.*\d{4}.*$", open(a.file, encoding="utf-8", errors="replace").read())
    for h in holders[:3]:
        print(f"  notice: {h.strip()}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="License helper")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list")
    p = sub.add_parser("info"); p.add_argument("id")
    p = sub.add_parser("fetch"); p.add_argument("id")
    p.add_argument("--holder", default=None); p.add_argument("--year", default=str(datetime.date.today().year))
    p.add_argument("--out", default=None); p.add_argument("--force", action="store_true")
    p = sub.add_parser("identify"); p.add_argument("file")
    p = sub.add_parser("exception"); p.add_argument("id"); p.add_argument("--out", default=None)
    a = ap.parse_args()
    return {"list": cmd_list, "info": cmd_info, "fetch": cmd_fetch, "identify": cmd_identify, "exception": cmd_exception}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
