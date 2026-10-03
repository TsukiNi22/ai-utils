#!/usr/bin/env python3
"""
Known vulnerabilities / compromised versions of the dependencies of a project (direct + transitive).

Usage:
    vulns.py [<project root>] [--no-transitive] [--json out.json] [--md out.md] [--recent-days 90]

Uses the inventory of deps.py, then for each dependency:
    PyPI / npm / crates.io  -> OSV (api.osv.dev), malicious packages (MAL-*) included
    git (commit pinned)     -> OSV commit query
    rpm (Fedora / RHEL)     -> pending security advisories of the installed package (dnf updateinfo)
    GitHub upstream (url)   -> repository security advisories (gh api), compared with the installed version
An advisory published less than --recent-days ago is flagged "recent".
"""

import argparse
import datetime
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("deps", os.path.join(HERE, "deps.py"))
deps_mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deps_mod)


def run(cmd, timeout=120):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout).stdout
    except Exception:
        return ""


def osv(payload):
    try:
        req = urllib.request.Request("https://api.osv.dev/v1/query", data=json.dumps(payload).encode(),
                                     headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=20) as r:
            return json.load(r).get("vulns", [])
    except Exception:
        return None


def vkey(v):
    return [int(x) if x.isdigit() else 0 for x in re.findall(r"\d+", v or "")][:4] or [0]


def in_range(version, rng):
    """GitHub ranges like '< 0.26.0', '>= 1.0, < 1.2', '<= 2.3', '= 1.0'."""
    if not rng or not version:
        return True
    v = vkey(version)
    for cond in rng.split(","):
        m = re.match(r"\s*(<=|>=|<|>|=)\s*(\S+)", cond)
        if not m:
            continue
        op, ref = m.group(1), vkey(m.group(2))
        if not {"<": v < ref, "<=": v <= ref, ">": v > ref, ">=": v >= ref, "=": v == ref}[op]:
            return False
    return True


def severity_of(v):
    s = (v.get("database_specific") or {}).get("severity") or ""
    for sv in v.get("severity", []) or []:
        if sv.get("type", "").startswith("CVSS") and not s:
            s = sv.get("score", "")
    return s or "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root", nargs="?", default=".")
    ap.add_argument("--no-transitive", action="store_true")
    ap.add_argument("--json")
    ap.add_argument("--md")
    ap.add_argument("--recent-days", type=int, default=90)
    a = ap.parse_args()
    root = os.path.abspath(a.root)
    deps = deps_mod.inventory(root, not a.no_transitive)
    now = datetime.datetime.now(datetime.timezone.utc)
    findings = []

    def add(dep, vid, sev, summary, published, fixed, source):
        recent = False
        try:
            recent = (now - datetime.datetime.fromisoformat(published.replace("Z", "+00:00"))).days <= a.recent_days
        except Exception:
            pass
        findings.append({"dependency": dep["name"], "version": dep["version"], "scope": dep.get("scope", ""),
                         "direct": dep["direct"], "id": vid, "severity": sev, "summary": summary[:160],
                         "published": (published or "")[:10], "fixed": fixed, "recent": recent, "source": source})

    # Pending Fedora security advisories for the installed packages
    advisories = {}
    if shutil.which("dnf"):
        for line in run(["dnf", "-q", "updateinfo", "list", "--security"]).split("\n"):
            m = re.match(r"^(\S+)\s+security\s+(\S+)\s+(\S+)\s+(\S+ \S+)$", line.strip())
            if m:
                name = re.sub(r"-[^-]+-[^-]+$", "", re.sub(r"\.(x86_64|noarch|i686|aarch64)$", "", m.group(3)))
                name = re.sub(r"^\d+:", "", name)
                advisories.setdefault(name, []).append((m.group(1), m.group(2), m.group(4)))

    gh = shutil.which("gh")
    checked = {"osv": 0, "dnf": 0, "github": 0}
    for d in deps:
        eco = d.get("ecosystem")
        if eco in ("PyPI", "npm", "crates.io") and d["version"]:
            name = d["name"].split(":", 1)[-1]
            vulns = osv({"package": {"name": name, "ecosystem": eco}, "version": d["version"]})
            checked["osv"] += 1
            for v in vulns or []:
                fixed = ", ".join(e.get("fixed") for r in (v.get("affected") or [{}])[0].get("ranges", []) for e in r.get("events", []) if e.get("fixed"))
                add(d, v["id"], "MALICIOUS" if v["id"].startswith("MAL-") else severity_of(v), v.get("summary") or v.get("details", "")[:160],
                    v.get("published", ""), fixed, "OSV")
        if eco == "git" and re.fullmatch(r"[0-9a-f]{40}", d.get("version") or ""):
            checked["osv"] += 1
            for v in osv({"commit": d["version"]}) or []:
                add(d, v["id"], severity_of(v), v.get("summary", ""), v.get("published", ""), "", "OSV (commit)")
        if eco == "rpm":
            checked["dnf"] += 1
            for aid, sev, issued in advisories.get(d["name"], []):
                add(d, aid, sev, "security update available for the installed package", issued, "dnf upgrade " + d["name"], "dnf updateinfo")
        m = re.search(r"github\.com[/:]([^/]+)/([^/#?.]+)", d.get("url") or "")
        if gh and m and d["version"]:
            checked["github"] += 1
            out = run([gh, "api", f"repos/{m.group(1)}/{m.group(2)}/security-advisories", "--paginate"], timeout=60)
            try:
                items = json.loads(out) if out.strip() else []
            except Exception:
                items = []
            upstream = re.sub(r"-[^-]*$", "", d["version"]) if d.get("ecosystem") in ("rpm", "deb") else d["version"]
            for adv in items:
                for vul in adv.get("vulnerabilities") or []:
                    if in_range(upstream, vul.get("vulnerable_version_range")) and not (vul.get("patched_versions") and in_range(upstream, ">= " + vul["patched_versions"].split(",")[0].strip())):
                        add(d, adv.get("ghsa_id") or adv.get("cve_id"), adv.get("severity", "?"), adv.get("summary", ""),
                            adv.get("published_at", ""), vul.get("patched_versions") or "", "GitHub advisory")
                        break

    rank = {"malicious": 0, "critical": 1, "high": 2, "important": 2, "moderate": 3, "medium": 3, "low": 4}
    findings.sort(key=lambda f: (rank.get(str(f["severity"]).lower(), 5), not f["recent"], f["dependency"]))
    md = ["| Dependency | Version | Advisory | Severity | Published | Fixed in | Source |", "|---|---|---|---|---|---|---|"]
    for f in findings:
        md.append(f"| `{f['dependency']}`{'' if f['direct'] else ' (transitive)'}{' [test]' if f['scope'] == 'test' else ''} | {f['version']} | "
                  f"{f['id']}{' **recent**' if f['recent'] else ''} — {f['summary'][:80]} | {f['severity']} | {f['published']} | {f['fixed'] or '—'} | {f['source']} |")
    if a.json:
        json.dump({"dependencies": deps, "findings": findings, "checked": checked}, open(a.json, "w"), indent=1)
    if a.md:
        open(a.md, "w").write("\n".join(md) + "\n")
    print(f"{len(deps)} dependencies, checked: {checked}, {len(findings)} advisory(ies), "
          f"{sum(f['recent'] for f in findings)} recent, {sum(f['severity'] == 'MALICIOUS' for f in findings)} malicious")
    print("\n".join(md[:2 + 25]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
