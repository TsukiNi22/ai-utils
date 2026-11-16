#!/usr/bin/env python3
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  15/11/2026 by Tsukini

File Name:
##  inventory.py

File Description:
##  The creative tools of this computer (audio, image, video, 3D, simulation / VFX): programs, flatpaks, Python
##  packages, MCP servers, GPU; compared with the catalog of setup-studio
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

"""
Usage: inventory.py [--json FILE] [--declare ID,...] [--connectors NAME,...] [--compare STATE] [--catalog FILE]

Prints a short summary per domain (found, missing roles) and writes the full result as JSON with --json.
--declare     tools of the catalog the user says they have (claude.ai, or an app the scan cannot see)
--connectors  claude.ai connectors in the tool list of the session (ex: ElevenLabs,Canva)
--compare     reference/state.json of the generated studio skills: what was added / removed since
Never prints the environment, headers or tokens of the MCP servers.
"""

##### Imports #####

import argparse # Used for ArgumentParser
import glob # Used for glob
import importlib.util # Used for find_spec
import json # Used for load, dump
import os # Used for path, cpu_count, statvfs
import platform # Used for node
import shutil # Used for which
import subprocess # Used for run
import sys # Used for exit
import time # Used for strftime
from urllib.parse import urlsplit # Used for urlsplit

##### Constants #####

HERE = os.path.dirname(os.path.abspath(__file__))
CATALOG = os.path.join(HERE, "..", "reference", "catalog.json")
HOME = os.path.expanduser("~")
APP_DIRS = ["~/Applications", "~/Apps", "~/.local/bin", "~/AppImages"]
ALWAYS = {"web", "assets"} # nothing to install: always available
SYSTEM_MANAGERS = ["dnf", "apt", "zypper", "pacman"]
OTHER_INSTALLERS = ["flatpak", "uv", "pipx", "pip", "npx", "docker", "podman"]
VENDORS = {"0x10de": "NVIDIA", "0x1002": "AMD", "0x8086": "Intel"}

##### Functions #####

def run(cmd: list, timeout: int = 5) -> str:
    """Output of a command, empty on any failure"""
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except (OSError, subprocess.SubprocessError):
        return ""
    return res.stdout if res.returncode == 0 else ""

def first_line(text: str) -> str:
    for line in text.splitlines():
        if line.strip():
            return line.strip()[:80]
    return ""

def flatpaks() -> set:
    return set(run(["flatpak", "list", "--app", "--columns=application"]).split()) if shutil.which("flatpak") else set()

def is_appimage(path: str) -> bool:
    """A real AppImage: its extension, or the AppImage magic (AI 1 / AI 2) in the ELF header"""
    if path.lower().endswith(".appimage"):
        return True
    try:
        with open(path, "rb") as file:
            head = file.read(11)
    except OSError:
        return False
    return head[:4] == b"\x7fELF" and head[8:11] in (b"AI\x01", b"AI\x02")

def appimages() -> list:
    """Names of the AppImages of the usual folders (the scripts and links of ~/.local/bin are not AppImages)"""
    found = []
    for folder in APP_DIRS:
        paths = glob.glob(os.path.join(os.path.expanduser(folder), "*"))
        found += [os.path.basename(p).lower() for p in paths if os.path.isfile(p) and is_appimage(p)]
    return found

def appimage_of(tool_id: str, name: str) -> bool:
    """The AppImage is named after the tool (blender-4.2-x86_64.appimage), not just containing its id"""
    return name == tool_id or (name.startswith(tool_id) and name[len(tool_id)] in "-_. ")

def gpus() -> list:
    """GPUs with their VRAM (nvidia-smi first, then the DRM cards)"""
    found = []
    for line in run(["nvidia-smi", "--query-gpu=name,memory.total", "--format=csv,noheader,nounits"]).splitlines():
        name, _, mem = line.partition(",")
        found.append({"vendor": "NVIDIA", "name": name.strip(), "vram_mb": int(mem.strip() or 0)})
    for card in sorted(glob.glob("/sys/class/drm/card[0-9]")):
        try:
            vendor = VENDORS.get(open(f"{card}/device/vendor", encoding="utf-8").read().strip(), "other")
        except OSError:
            continue
        if vendor == "NVIDIA" and found:
            continue
        vram = 0
        try:
            vram = int(open(f"{card}/device/mem_info_vram_total", encoding="utf-8").read()) // (1024 * 1024)
        except (OSError, ValueError):
            pass
        found.append({"vendor": vendor, "name": f"{vendor} GPU", "vram_mb": vram})
    return found

def hardware() -> dict:
    ram = 0
    try:
        for line in open("/proc/meminfo", encoding="utf-8"):
            if line.startswith("MemTotal:"):
                ram = round(int(line.split()[1]) / 1024 / 1024)
    except OSError:
        pass
    st = os.statvfs(HOME)
    return {"gpus": gpus(), "ram_gb": ram, "cpus": os.cpu_count() or 0, "disk_free_gb": st.f_bavail * st.f_frsize // 1024 ** 3}

def os_name() -> str:
    try:
        for line in open("/etc/os-release", encoding="utf-8"):
            if line.startswith("PRETTY_NAME="):
                return line.split("=", 1)[1].strip().strip('"')
    except OSError:
        pass
    return platform.system()

def mcp_target(conf: dict) -> str:
    """What a server runs, without its environment, headers or URL query (tokens)"""
    if conf.get("url"):
        url = urlsplit(conf["url"])
        return f"{url.scheme}://{url.netloc}{url.path}"
    words = [conf.get("command", "")] + [a for a in conf.get("args", []) if not a.startswith("-")][:2]
    return " ".join(os.path.basename(w) for w in words if w)

def mcp_servers() -> list:
    """MCP servers of Claude Code: user (~/.claude.json), local (its project entry), project (.mcp.json)"""
    found = []
    cwd = os.getcwd()
    try:
        data = json.load(open(os.path.join(HOME, ".claude.json"), encoding="utf-8"))
    except (OSError, ValueError):
        data = {}
    sources = [("user", data.get("mcpServers", {})), ("local", data.get("projects", {}).get(cwd, {}).get("mcpServers", {}))]
    try:
        sources.append(("project", json.load(open(os.path.join(cwd, ".mcp.json"), encoding="utf-8")).get("mcpServers", {})))
    except (OSError, ValueError):
        pass
    for scope, servers in sources:
        for name, conf in (servers or {}).items():
            found.append({"name": name, "scope": scope, "type": conf.get("type", "http" if conf.get("url") else "stdio"),
                          "target": mcp_target(conf)})
    return found

def match_mcp(server: dict, tools: list) -> str:
    """Catalog tool driven by a server, from its name or what it runs"""
    text = f"{server['name']} {server['target']}".lower()
    exact = [t["id"] for t in tools if t["id"] == server["name"].lower()]
    if exact:
        return exact[0]
    for tool in tools:
        if tool["id"] in text or tool["name"].split()[0].lower() in text:
            return tool["id"]
    return ""

def detect(tool: dict, flat: set, apps: list, declared: set, connectors: set) -> list:
    """Where a tool of the catalog is on this computer (empty: not found)"""
    det = tool.get("detect", {})
    where = []
    for name in det.get("bin", []):
        path = name if os.path.isabs(name) and os.access(name, os.X_OK) else shutil.which(name)
        if path:
            where.append(f"bin:{path}")
    where += [f"flatpak:{i}" for i in det.get("flatpak", []) if i in flat]
    for mod in det.get("python", []):
        try:
            if importlib.util.find_spec(mod):
                where.append(f"python:{mod}")
        except (ImportError, ValueError):
            pass
    where += [f"dir:{d}" for d in det.get("dirs", []) if os.path.isdir(os.path.expanduser(d))]
    where += [f"appimage:{a}" for a in apps if appimage_of(tool["id"], a)]
    where += [f"connector:{c}" for c in det.get("connector", []) if c.lower() in {n.lower() for n in connectors}]
    if tool["id"] in declared:
        where.append("declared")
    return where

def version(tool: dict, where: list) -> str:
    args = tool.get("version")
    bins = [w[4:] for w in where if w.startswith("bin:")]
    return first_line(run([bins[0]] + args)) if args and bins else ""

def installers() -> dict:
    """The package manager of the system (the first one found: a `pacman` game on Fedora is not one) and the others"""
    native = next((m for m in SYSTEM_MANAGERS if shutil.which(m)), "")
    found = {native: True} if native else {}
    found.update({k: bool(shutil.which(k)) for k in OTHER_INSTALLERS})
    return found

def scan(catalog: dict, declared: set, connectors: set) -> dict:
    flat, apps = flatpaks(), appimages()
    tools = {}
    for tool in catalog["tools"]:
        where = detect(tool, flat, apps, declared, connectors)
        if where:
            tools[tool["id"]] = {"where": where, "version": version(tool, where)}
    servers = mcp_servers()
    for server in servers:
        server["tool"] = match_mcp(server, catalog["tools"])
    return {
        "date": time.strftime("%Y-%m-%d"), "host": platform.node(), "os": os_name(), "hardware": hardware(),
        "installers": installers(),
        "tools": tools, "mcp": servers, "connectors": sorted(connectors), "declared": sorted(declared),
        "missing_roles": missing_roles(catalog, tools, servers),
    }

def missing_roles(catalog: dict, tools: dict, servers: list) -> dict:
    """Roles of each domain that no free available tool covers (a connector spending credits does not count)"""
    driven = {s["tool"] for s in servers if s["tool"]}
    missing = {}
    for domain, info in catalog["domains"].items():
        covered = set()
        for tool in catalog["tools"]:
            free = tool["kind"] != "connector"
            if free and (tool["id"] in tools or tool["id"] in driven or tool["kind"] in ALWAYS):
                covered |= set(tool["domains"].get(domain, []))
        missing[domain] = [r for r in info["roles"] if r not in covered]
    return missing

def summary(inv: dict, catalog: dict) -> None:
    hw = inv["hardware"]
    gpu = ", ".join(f"{g['name']} {g['vram_mb'] // 1024} GB" if g["vram_mb"] else g["name"] for g in hw["gpus"]) or "none"
    print(f"Host: {inv['host']} ({inv['os']}) · GPU: {gpu} · RAM {hw['ram_gb']} GB · {hw['cpus']} CPUs · {hw['disk_free_gb']} GB free")
    print("Installers: " + " ".join(k for k, v in inv["installers"].items() if v))
    for s in inv["mcp"]:
        print(f"MCP {s['name']} ({s['scope']}, {s['target']}) -> {s['tool'] or '? (domain to ask)'}")
    if inv["connectors"]:
        print("Connectors: " + ", ".join(inv["connectors"]))
    names = {t["id"]: t for t in catalog["tools"]}
    for domain in catalog["domains"]:
        found = [i for i in inv["tools"] if domain in names[i]["domains"]]
        print(f"{domain}: found {', '.join(found) or '-'} · missing roles: {', '.join(inv['missing_roles'][domain]) or '-'}")

def compare(inv: dict, state_path: str) -> None:
    try:
        old = json.load(open(state_path, encoding="utf-8"))["inventory"]
    except (OSError, ValueError, KeyError) as err:
        sys.exit(f"Error: cannot read {state_path}: {err}")
    before, now = set(old["tools"]), set(inv["tools"])
    old_mcp, new_mcp = {s["name"] for s in old["mcp"]}, {s["name"] for s in inv["mcp"]}
    print(f"Since {old['date']}: tools added {sorted(now - before) or '-'}, removed {sorted(before - now) or '-'}; "
          f"MCP added {sorted(new_mcp - old_mcp) or '-'}, removed {sorted(old_mcp - new_mcp) or '-'}")

def main() -> int:
    ap = argparse.ArgumentParser(description="Creative tools of this computer, compared with the setup-studio catalog")
    ap.add_argument("--json", help="write the full result to this file")
    ap.add_argument("--declare", default="", help="catalog ids the user has (comma separated)")
    ap.add_argument("--connectors", default="", help="claude.ai connectors of the session (comma separated)")
    ap.add_argument("--compare", help="state.json of the generated skills")
    ap.add_argument("--catalog", default=CATALOG)
    a = ap.parse_args()
    try:
        catalog = json.load(open(a.catalog, encoding="utf-8"))
    except (OSError, ValueError) as err:
        sys.exit(f"Error: cannot read the catalog {a.catalog}: {err}")
    declared = {d.strip() for d in a.declare.split(",") if d.strip()}
    unknown = declared - {t["id"] for t in catalog["tools"]}
    if unknown:
        sys.exit(f"Error: not in the catalog: {', '.join(sorted(unknown))}")
    inv = scan(catalog, declared, {c.strip() for c in a.connectors.split(",") if c.strip()})
    summary(inv, catalog)
    if a.compare:
        compare(inv, a.compare)
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump(inv, f, indent=2)
        print(f"Written: {a.json}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
