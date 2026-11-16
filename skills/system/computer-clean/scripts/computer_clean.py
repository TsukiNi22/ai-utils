#!/usr/bin/env python3
"""
Find what can be cleaned on this computer and in the data of Claude Code / claude-cluster, then delete the chosen ones.

Usage:
    computer_clean.py scan [--days N] [--min-mb N] [--root DIR]... [--top N] [--json]
    computer_clean.py clean ID... [--days N] [--min-mb N] [--root DIR]... [--dry-run]

- scan: a deep pass over every place that fills a disk - every folder of ~/.cache, the caches of the language tools
  (npm, cargo, go, gradle, maven, pnpm...), of the Chromium / Electron apps and of the flatpak apps, build outputs and
  envs of projects untouched for a while, the trash, downloads (big ones, installers, archives already extracted),
  duplicated big files, big old files, core dumps, big logs, containers (images, orphan volumes), flatpak (unused
  runtimes, SDKs no app runs on, data of removed apps), dnf (cache, updates waiting to be installed), journals, system
  crash dumps, old kernels, Claude Code / claude-cluster leftovers. Each candidate: an id, its size (on btrfs, what it
  frees for sure), its age, what it is for, what deleting it does, `safe` or `review`, its command; then the biggest
  folders of the home, for information.
- clean: the candidates of these ids deleted for good (rm -rf, or their command); the scan is made again first
  (same options), an id that is gone or changed is skipped.
- A candidate needing root is run with sudo -A (SUDO_ASKPASS) when set, else its command is told.
- Never touched: a git-tracked file, a worktree with changes, the claude-cluster session running, this job's folder.
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import time
from pathlib import Path

HOME = Path.home()
DAY = 86400.0
MB = 1024 * 1024
UNITS = {"bytes": 1, "b": 1, "kb": 1e3, "mb": 1e6, "gb": 1e9, "tb": 1e12, "kib": 1024, "mib": 1024 ** 2, "gib": 1024 ** 3,
         "tib": 1024 ** 4, "k": 1024, "m": 1024 ** 2, "g": 1024 ** 3, "t": 1024 ** 4}

# =========================
# Tools
# =========================
def run(command: list, timeout: int = 60, stdin: str = "") -> str:
    """The output of a command ("" when it fails or is missing), in the C locale, never waiting on a keyboard."""
    if not shutil.which(command[0]):
        return ""
    try:
        return subprocess.run(command, capture_output=True, text=True, timeout=timeout, input=stdin,
                              env={**os.environ, "LC_ALL": "C.UTF-8"}).stdout
    except (OSError, subprocess.SubprocessError):
        return ""

def size_of(path: Path) -> int:
    """Bytes of a file or a folder (du -sb: fast, links not followed)."""
    if not path.exists():
        return 0
    if path.is_file() or path.is_symlink():
        return path.lstat().st_size
    out = run(["du", "-sb", str(path)], timeout=300)
    try:
        return int(out.split()[0])
    except (IndexError, ValueError):
        return 0

def parse_size(text: str) -> int:
    """Bytes of a size written by a tool ("48.38MB", "1.7 GB", "163.8 kB", "72.0M")."""
    match = re.search(r"([\d.]+)\s*(bytes|[kKMGT]i?B|B|[KMGT])\b", text.replace(",", "."))
    return int(float(match.group(1)) * UNITS[match.group(2).lower()]) if match else 0

def age_days(path: Path) -> float:
    """Days since its last change (the newest of the folder itself and its direct content)."""
    try:
        newest = path.lstat().st_mtime
        if path.is_dir() and not path.is_symlink():
            for child in list(path.iterdir())[:200]:
                newest = max(newest, child.lstat().st_mtime)
        return (time.time() - newest) / DAY
    except OSError:
        return 0.0

def human(size: float) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if size < 1024 or unit == "GB":
            return f"{size:.0f} {unit}" if unit == "B" else f"{size:.1f} {unit}"
        size /= 1024
    return f"{size:.1f} GB"

def tilde(path: Path) -> str:
    return f"~/{path.relative_to(HOME)}" if path.is_relative_to(HOME) else str(path)

def candidate(group: str, kind: str, what: str, size: int, age: float, use: str, after: str, risk: str, path: str = "",
              command: list | None = None, root: bool = False) -> dict:
    """One thing that can go: `use` what it is for, `after` what deleting it does; `command` replaces rm -rf of `path`."""
    key = path or " ".join(command or [])
    return {"id": hashlib.sha1(f"{kind}:{key}".encode()).hexdigest()[:6], "group": group, "kind": kind, "what": what,
            "size": size, "frees": None, "age": round(age, 1), "use": use, "after": after, "risk": risk, "path": path,
            "command": command or [], "root": root}

def tracked(path: Path) -> bool:
    """True when the path (or something in it) is tracked by a git repository."""
    top = run(["git", "-C", str(path.parent), "rev-parse", "--show-toplevel"]).strip()
    if not top:
        return False
    return bool(run(["git", "-C", top, "ls-files", "--", str(path)]).strip())

def frees_for_sure(path: Path) -> int | None:
    """On btrfs, the bytes deleting it frees for sure: its extents no other file shares (reflinks made by uv or
    cp --reflink, snapshots and deduplication keep the shared ones alive); None on other file systems."""
    if not shutil.which("btrfs") or run(["stat", "-f", "-c", "%T", str(path)]).strip() != "btrfs":
        return None
    lines = run(["btrfs", "filesystem", "du", "-s", "--raw", str(path)], timeout=600).splitlines()
    try:
        return int(lines[-1].split()[1])
    except (IndexError, ValueError):
        return None

def user_dir(key: str, default: str) -> Path:
    """A folder of the desktop from user-dirs.dirs (XDG_DOWNLOAD_DIR: ~/Téléchargements in French, ~/Downloads else)."""
    config = Path(os.environ.get("XDG_CONFIG_HOME", HOME / ".config")) / "user-dirs.dirs"
    try:
        for line in config.read_text().splitlines():
            if line.startswith(f"XDG_{key}_DIR="):
                path = Path(line.split("=", 1)[1].strip().strip('"').replace("$HOME", str(HOME)))
                if path != HOME and path.is_dir():
                    return path
    except OSError:
        pass
    return HOME / default

# =========================
# Caches
# =========================
CACHES = {  # ~/.cache/<name>: (what, what it is for, what deleting it does); any other folder is listed as `review`
    "pip": ("pip", "packages downloaded by pip, reused by the next installs", "downloaded again by the next pip install"),
    "pypoetry": ("poetry", "packages downloaded by poetry and the virtual envs of the poetry projects",
                 "downloaded again; the envs of the projects made again by poetry install"),
    "uv": ("uv", "Python packages downloaded and built by uv, reused by every uv install / venv",
           "downloaded again by the next uv install; the venvs keep working (they hold their own files)"),
    "npm": ("npm", "packages downloaded by npm", "downloaded again by the next npm install"),
    "yarn": ("yarn", "packages downloaded by yarn", "downloaded again by the next yarn install"),
    "pnpm": ("pnpm", "metadata of the registry fetched by pnpm", "fetched again by the next pnpm install"),
    "deno": ("Deno", "modules downloaded by deno", "downloaded again when a script needs them"),
    "go-build": ("Go builds", "Go packages already compiled, reused by go build", "the next go build compiles everything again"),
    "ccache": ("ccache", "C / C++ objects already compiled, reused by the next builds", "the next builds compile everything (slower once)"),
    "sccache": ("sccache", "objects already compiled, shared between builds", "the next builds compile everything (slower once)"),
    "bazel": ("Bazel", "build outputs and downloads of Bazel", "built and downloaded again by the next bazel build"),
    "clangd": ("clangd", "index of the C / C++ projects for the editor", "indexed again when a project is opened"),
    "tsukini-skills": ("builds of the tools of ai-utils", "binaries built for the skills of ai-utils", "built again when a skill needs one"),
    "thumbnails": ("thumbnails", "previews of the pictures and videos shown by the file manager", "made again when a folder is opened"),
    "mesa_shader_cache": ("shader cache (Mesa)", "GPU shaders already compiled for games and apps", "compiled again: a few stutters the first time"),
    "mesa_shader_cache_db": ("shader cache (Mesa)", "GPU shaders already compiled for games and apps", "compiled again: a few stutters the first time"),
    "nvidia": ("shader cache (NVIDIA)", "GPU shaders already compiled by the NVIDIA driver", "compiled again: a few stutters the first time"),
    "fontconfig": ("fontconfig", "index of the installed fonts", "made again at the next start of an app"),
    "JetBrains": ("JetBrains IDEs", "indexes and caches of CLion / IntelliJ / PyCharm...", "the IDE indexes the projects again at the next opening"),
    "mozilla": ("Firefox", "pages and pictures cached by Firefox", "pages load a bit slower once; logins and history kept"),
    "google-chrome": ("Chrome", "pages and pictures cached by Chrome", "pages load a bit slower once; logins and history kept"),
    "chromium": ("Chromium", "pages and pictures cached by Chromium", "pages load a bit slower once; logins and history kept"),
    "BraveSoftware": ("Brave", "pages and pictures cached by Brave", "pages load a bit slower once; logins and history kept"),
    "huggingface": ("Hugging Face", "AI models and datasets downloaded by transformers / diffusers / whisper...",
                    "downloaded again (often GBs) the next time a tool uses them"),
    "torch": ("PyTorch hub", "models downloaded by torch.hub", "downloaded again when a script uses them"),
    "whisper": ("Whisper", "speech recognition models", "downloaded again when used"),
    "ms-playwright": ("Playwright", "browsers downloaded by Playwright", "downloaded again by npx playwright install"),
    "puppeteer": ("Puppeteer", "browsers downloaded by Puppeteer", "downloaded again by the next npm install"),
    "Cypress": ("Cypress", "the Cypress app downloaded by npm", "downloaded again by the next npm install"),
    "electron": ("Electron", "Electron runtimes downloaded by the builds", "downloaded again by the next build"),
    "node-gyp": ("node-gyp", "Node headers used to build native modules", "downloaded again when a native module is built"),
    "typescript": ("TypeScript", "type definitions fetched by the TypeScript server", "fetched again by the editor"),
    "pre-commit": ("pre-commit", "the envs of the pre-commit hooks", "made again at the next commit (slower once)"),
    "tracker3": ("GNOME search index", "index of the files for the search of GNOME (Tracker)", "files indexed again in the background"),
    "libdnf5": ("dnf metadata of your user", "repository lists downloaded by dnf run as you", "downloaded again"),
    "gnome-software": ("GNOME Software", "icons and metadata of the software center", "downloaded again"),
    "nvim": ("Neovim", "compiled Lua and the caches of the plugins / LSP servers", "made again at the next start"),
    "vscode-cpptools": ("VS Code C / C++", "IntelliSense database of the C / C++ extension", "made again when a project is opened"),
    "spotify": ("Spotify", "songs kept to play them fast", "downloaded again when played"),
    "pip-tools": ("pip-tools", "resolved dependencies of pip-compile", "resolved again by the next pip-compile"),
    "matplotlib": ("matplotlib", "font list of matplotlib", "made again at the next import"),
    "claude-cli-nodejs": ("Claude Code", "logs of the MCP servers and caches of Claude Code", "made again; the old MCP logs are gone"),
    "gtk-4.0": ("GTK", "shaders and icons compiled by the GTK apps", "made again at the next start of an app"),
    "flatpak": ("flatpak", "metadata of the flatpak remotes fetched as you", "fetched again"),
    "gem": ("Ruby gems", "gems downloaded by gem / bundler", "downloaded again by the next install"),
    "epiphany": ("GNOME Web", "pages and pictures cached by GNOME Web", "pages load a bit slower once"),
}
UNKNOWN_CACHE = ("an application keeps it to go faster (its name says which one)",
                 "made again by that application when needed - check the name: a few tools keep real data in ~/.cache")
TOOL_CACHES = (  # outside ~/.cache: (path from the home, what, what it is for, what deleting it does, risk, command instead of rm)
    (".npm/_cacache", "npm", "packages downloaded by npm, reused by the next installs", "downloaded again by the next npm install", "safe", None),
    (".cargo/registry/cache", "cargo downloads", "crates downloaded by cargo", "downloaded again by the next cargo build", "safe", None),
    (".cargo/registry/src", "cargo sources", "crates extracted from the downloads", "extracted again by the next cargo build", "safe", None),
    (".cargo/git/checkouts", "cargo git checkouts", "git dependencies of the Rust projects", "cloned again by the next cargo build", "safe", None),
    ("go/pkg/mod", "Go modules", "modules downloaded by go (read-only files)", "downloaded again by the next go build", "safe",
     ["go", "clean", "-modcache"]),
    (".gradle/caches", "Gradle", "dependencies and build caches of Gradle / Android Studio",
     "downloaded and built again by the next Gradle build (slower once)", "safe", None),
    (".gradle/wrapper/dists", "Gradle versions", "Gradle versions downloaded by ./gradlew", "downloaded again by the next ./gradlew", "safe", None),
    (".m2/repository", "Maven", "dependencies downloaded by Maven", "downloaded again by the next mvn build", "safe", None),
    (".nuget/packages", "NuGet", "packages downloaded by dotnet", "downloaded again by the next dotnet restore", "safe", None),
    (".conan2/p", "Conan", "C / C++ packages downloaded or built by Conan", "downloaded or built again by the next conan install", "safe", None),
    (".yarn/berry/cache", "yarn", "packages downloaded by yarn", "downloaded again by the next yarn install", "safe", None),
    (".bun/install/cache", "bun", "packages downloaded by bun", "downloaded again by the next bun install", "safe", None),
    (".local/share/pnpm/store", "pnpm store", "the packages of every pnpm project, linked into their node_modules",
     "only the packages no project links any more go (pnpm store prune)", "safe", ["pnpm", "store", "prune"]),
    (".ccache", "ccache (old place)", "C / C++ objects already compiled", "the next builds compile everything (slower once)", "safe", None),
    (".debug", "perf symbols", "copies of the programs profiled with perf, to read old perf.data files",
     "old perf.data files show fewer symbols; made again by the next perf record", "safe", None),
    (".android/cache", "Android SDK downloads", "files downloaded by the Android SDK manager", "downloaded again when needed", "safe", None),
    (".android/avd", "Android emulators", "the disks of the Android emulators (AVD) and what is installed in them",
     "the emulators are lost: created again from the AVD manager", "review", None),
    (".platformio/.cache", "PlatformIO downloads", "files downloaded by PlatformIO", "downloaded again when needed", "safe", None),
    (".node-gyp", "node-gyp headers", "Node headers used to build native modules", "downloaded again when a native module is built", "safe", None),
    (".local/share/virtualenvs", "pipenv envs", "the virtual envs of the pipenv projects", "made again by pipenv install in each project",
     "review", None),
    (".ollama/models", "ollama models", "AI models pulled by ollama", "pulled again by ollama pull (often GBs)", "review", None),
)
WEB_CACHES = {"Cache", "Code Cache", "GPUCache", "DawnCache", "DawnGraphiteCache", "DawnWebGPUCache", "GrShaderCache", "GraphiteDawnCache",
              "ShaderCache", "CacheStorage", "ScriptCache", "component_crx_cache", "CachedData", "CachedExtensionVSIXs", "CachedProfilesData"}

def scan_caches(min_bytes: int) -> list:
    found = []
    cache = Path(os.environ.get("XDG_CACHE_HOME", HOME / ".cache"))
    for path in sorted(cache.iterdir()) if cache.is_dir() else []:
        if path.is_symlink():
            continue
        what, use, after = CACHES.get(path.name, (path.name, *UNKNOWN_CACHE))
        size = size_of(path)
        if size >= min_bytes:
            found.append(candidate("cache", "cache", f"{tilde(path)}: {what}", size, age_days(path), use, after,
                                   "safe" if path.name in CACHES else "review", str(path)))
    for relative, what, use, after, risk, command in TOOL_CACHES:
        path = HOME / relative
        if not path.exists() or (command and not shutil.which(command[0])):
            continue
        size = size_of(path)
        if size >= min_bytes:
            found.append(candidate("cache", "cache", f"{tilde(path)}: {what}", size, age_days(path), use, after, risk, str(path), command))
    # the web caches of the Chromium / Electron apps (Discord, VS Code, Chrome, Slack...) and the caches of the flatpak apps
    apps = {}
    flatpaks = sorted((HOME / ".var" / "app").glob("*")) if (HOME / ".var" / "app").is_dir() else []
    for top, app_of in [(HOME / ".config", lambda parts: HOME / ".config" / parts[0])] + [(app / "config", lambda _parts, app=app: app) for app in flatpaks]:
        for dirpath, dirnames, _ in os.walk(top):
            parts = Path(dirpath).relative_to(top).parts
            for name in [n for n in dirnames if n in WEB_CACHES]:
                dirnames.remove(name)
                apps.setdefault(app_of(parts + (name,)), []).append(Path(dirpath) / name)
            if len(parts) >= 4:
                dirnames[:] = []
    for app in flatpaks:
        if (app / "cache").is_dir():
            apps.setdefault(app, []).append(app / "cache")
    for app, paths in sorted(apps.items()):
        size = sum(size_of(p) for p in paths)
        if size >= min_bytes:
            found.append(candidate("cache", "app-cache", f"{tilde(app)}: caches of the app ({', '.join(sorted({p.name for p in paths}))})",
                                   size, max(age_days(p) for p in paths), "pages, scripts, GPU shaders and files the app keeps to start fast",
                                   "the app downloads / compiles them again (close it first); logins and settings kept", "safe",
                                   command=["rm", "-rf"] + [str(p) for p in paths]))
    return found

# =========================
# Files: trash, downloads, duplicates
# =========================
INSTALLERS = {".iso", ".img", ".rpm", ".deb", ".appimage", ".exe", ".msi", ".dmg", ".run", ".pkg"}
ARCHIVES = (".tar.gz", ".tar.xz", ".tar.bz2", ".tar.zst", ".tgz", ".txz", ".tar", ".zip", ".7z", ".rar")
USER_DIRS = (("DOWNLOAD", "Downloads"), ("DESKTOP", "Desktop"), ("DOCUMENTS", "Documents"), ("PICTURES", "Pictures"),
             ("VIDEOS", "Videos"), ("MUSIC", "Music"))

def archive_stem(name: str) -> str:
    """The name of an archive without its extension ("" when it is no archive)."""
    lower = name.lower()
    return next((name[:-len(suffix)] for suffix in ARCHIVES if lower.endswith(suffix)), "")

def digest(path: Path) -> str:
    """Hash of the content (the path itself when unreadable: never equal to another one)."""
    hasher = hashlib.blake2b()
    try:
        with open(path, "rb") as file:
            for block in iter(lambda: file.read(MB), b""):
                hasher.update(block)
    except OSError:
        return str(path)
    return hasher.hexdigest()

def duplicates(min_bytes: int) -> list:
    """Big files present twice (same content) in the folders of the desktop: every copy but the least deep one."""
    by_size = {}
    for key, default in USER_DIRS:
        top = user_dir(key, default)
        for dirpath, dirnames, filenames in os.walk(top) if top.is_dir() else []:
            dirnames[:] = [d for d in dirnames if not d.startswith(".") and d not in BUILDS]
            if len(Path(dirpath).relative_to(top).parts) >= 4:
                dirnames[:] = []
            for name in filenames:
                path = Path(dirpath) / name
                try:
                    info = path.lstat()
                except OSError:
                    continue
                if stat.S_ISREG(info.st_mode) and info.st_size >= max(min_bytes, 20 * MB):
                    by_size.setdefault(info.st_size, set()).add(path)
    found = []
    for size, paths in by_size.items():
        by_hash = {}
        for path in paths if len(paths) > 1 else []:
            by_hash.setdefault(digest(path), []).append(path)
        for same in by_hash.values():
            same.sort(key=lambda p: (len(p.parts), str(p)))
            for copy in same[1:]:
                found.append(candidate("files", "duplicate", tilde(copy), size, age_days(copy), f"a copy of {tilde(same[0])} (same content)",
                                       f"{tilde(same[0])} stays", "review", str(copy)))
    return found

def scan_files(days: float, min_bytes: int) -> list:
    found = []
    trash = Path(os.environ.get("XDG_DATA_HOME", HOME / ".local" / "share")) / "Trash"
    if size_of(trash) >= min_bytes:
        found.append(candidate("files", "trash", "the trash of the desktop", size_of(trash), age_days(trash),
                               "files already thrown away, kept in case of a mistake", "they can no longer be restored", "safe", str(trash)))
    downloads = user_dir("DOWNLOAD", "Downloads")
    for path in sorted(downloads.iterdir()) if downloads.is_dir() else []:
        if not path.is_file() or path.is_symlink():
            continue
        size, age, stem = path.stat().st_size, age_days(path), archive_stem(path.name)
        if stem and (downloads / stem).is_dir() and size >= min_bytes:
            found.append(candidate("files", "download", tilde(path), size, age, f"an archive, already extracted into {stem}/",
                                   "the extracted folder stays; the archive can be downloaded again", "review", str(path)))
        elif path.suffix.lower() in INSTALLERS and size >= min_bytes and age >= days:
            found.append(candidate("files", "download", tilde(path), size, age, "an installer or a disk image (.iso, .rpm, .deb, .AppImage...)",
                                   "downloaded again from its site if needed (an .AppImage started from here is gone)", "review", str(path)))
        elif size >= max(min_bytes, 200 * MB) and age >= days:
            found.append(candidate("files", "download", tilde(path), size, age, "a big download not changed for a while",
                                   "lost unless it can be downloaded again: check you no longer need it", "review", str(path)))
    return found + duplicates(min_bytes)

# =========================
# Projects: build outputs, envs, big old files, core dumps
# =========================
CODE = ("CMakeLists.txt", "Makefile", "setup.py", "pyproject.toml", "package.json", "meson.build", "build.gradle", "build.gradle.kts",
        "Cargo.toml", "build.zig")
PYTHON = ("pyproject.toml", "requirements*.txt", "setup.py", "setup.cfg", "Pipfile", "poetry.lock", "uv.lock")
JS = ("package.json",)
BUILDS = {  # folder: (project files, one of which sits next to it - () for anywhere, what it holds, made again by, risk)
    "build": (CODE, "build outputs (objects, binaries)", "the next build", "safe"),
    "node_modules": (JS, "the packages of the project", "npm install (or pnpm / yarn install)", "safe"),
    "target": (("Cargo.toml", "pom.xml"), "Rust / Maven build outputs", "the next cargo / mvn build", "safe"),
    ".venv": (PYTHON, "the Python virtual env of the project", "pip install -r / uv sync / poetry install (packages added by hand are lost)", "review"),
    "venv": (PYTHON, "the Python virtual env of the project", "pip install -r / uv sync / poetry install (packages added by hand are lost)", "review"),
    "dist": (JS + ("pyproject.toml", "setup.py"), "packages / bundles built from the project", "the next build (npm run build, python -m build)", "review"),
    ".next": (JS, "the Next.js build and cache", "the next next build / next dev", "safe"),
    ".nuxt": (JS, "the Nuxt build", "the next nuxt build / dev", "safe"),
    ".svelte-kit": (JS, "the SvelteKit build", "the next vite build / dev", "safe"),
    ".turbo": (JS, "the Turborepo cache", "the next turbo run", "safe"),
    ".parcel-cache": (JS, "the Parcel cache", "the next parcel build", "safe"),
    ".angular": (JS, "the Angular cache", "the next ng build", "safe"),
    "__pycache__": ((), "compiled Python files", "Python at the next import", "safe"),
    ".pytest_cache": ((), "the pytest cache (last failures)", "the next pytest run", "safe"),
    ".mypy_cache": ((), "the mypy cache", "the next mypy run", "safe"),
    ".ruff_cache": ((), "the ruff cache", "the next ruff run", "safe"),
    ".tox": ((), "the tox envs", "the next tox run", "safe"),
    ".gradle": (("build.gradle", "build.gradle.kts", "settings.gradle", "settings.gradle.kts"), "the Gradle cache of the project", "the next Gradle build", "safe"),
    ".cache": (("CMakeLists.txt", "compile_commands.json"), "the clangd index of the project", "clangd at the next opening", "safe"),
    ".zig-cache": (("build.zig",), "the Zig build cache", "the next zig build", "safe"),
    "zig-cache": (("build.zig",), "the Zig build cache", "the next zig build", "safe"),
    "zig-out": (("build.zig",), "the Zig build outputs", "the next zig build", "safe"),
    "_build": (("dune-project", "mix.exs", "rebar.config"), "OCaml / Elixir / Erlang build outputs", "the next dune / mix / rebar3 build", "safe"),
    ".stack-work": (("stack.yaml",), "the Haskell stack build", "the next stack build", "safe"),
    "dist-newstyle": (("cabal.project", "*.cabal"), "the Haskell cabal build", "the next cabal build", "safe"),
    "obj": (("*.csproj", "*.fsproj"), ".NET intermediate files", "the next dotnet build", "safe"),
    "bin": (("*.csproj", "*.fsproj"), ".NET build outputs", "the next dotnet build", "safe"),
}
PREFIXES = ("build-", "cmake-build-")
SKIPPED = ("Android", "actions-runner", "externals", "site-packages", "go", "sdk", "Sdk")  # installed programs: never build outputs
CORE = re.compile(r"^(vg)?core(\.\d+)?$")

def is_core(path: Path) -> bool:
    """True for an ELF core dump (e_type 4)."""
    try:
        with open(path, "rb") as file:
            head = file.read(18)
        return len(head) == 18 and head[:4] == b"\x7fELF" and int.from_bytes(head[16:18], "little" if head[5] == 1 else "big") == 4
    except OSError:
        return False

def old_build(found: list, path: Path, kind: tuple, days: float, min_bytes: int) -> None:
    markers, holds, again, risk = kind
    if markers and not any(any(path.parent.glob(marker)) for marker in markers):
        return  # next to no project file: not the output of a build here
    age = age_days(path)
    if age < days or tracked(path):
        return
    size = size_of(path)
    if size >= min_bytes:
        found.append(candidate("build", "build", tilde(path), size, age, f"{holds}, in a project untouched for {age:.0f} days",
                               f"made again by {again}", risk, str(path)))

def old_file(found: list, path: Path, days: float, min_bytes: int) -> None:
    try:
        info = path.lstat()
    except OSError:
        return
    if not stat.S_ISREG(info.st_mode):
        return
    size, age = info.st_size, (time.time() - info.st_mtime) / DAY
    if CORE.match(path.name) and is_core(path):
        found.append(candidate("files", "core", tilde(path), size, age, "a core dump: the memory of a program when it crashed, to debug it with gdb",
                               "that crash can no longer be debugged", "safe", str(path)))
    elif path.suffix == ".log" and size >= max(min_bytes, 50 * MB) and age >= days and not tracked(path):
        found.append(candidate("files", "log", tilde(path), size, age, "a big log file written by a program or a script",
                               "its messages are lost (the program writes a new one)", "review", str(path)))
    elif size >= max(min_bytes, 500 * MB) and age >= max(days, 90) and not tracked(path):
        found.append(candidate("files", "big", tilde(path), size, age, "a big file not changed for months (a video, a disk image, a dataset...)",
                               "lost for good: check you no longer need it", "review", str(path)))

def scan_tree(days: float, min_bytes: int, roots: list) -> list:
    """Build outputs and envs of old projects, big old files, core dumps and big logs under the roots."""
    found = []
    for root in roots:
        for dirpath, dirnames, filenames in os.walk(root) if root.is_dir() else []:
            here = Path(dirpath)
            depth = here.relative_to(root).parts
            if len(depth) > 6 or any(p.startswith(".") for p in depth) or any(p in SKIPPED for p in depth):
                dirnames[:] = []  # hidden folders and installed programs (their own folders, never build outputs)
                continue
            for name in list(dirnames):
                kind = BUILDS.get(name) or (BUILDS["build"] if name.startswith(PREFIXES) else None)
                if kind:
                    dirnames.remove(name)
                    old_build(found, here / name, kind, days, min_bytes)
                elif name in (".git", "proc", "sys"):
                    dirnames.remove(name)
            for name in filenames:
                old_file(found, here / name, days, min_bytes)
    return found

# =========================
# System and containers
# =========================
SYSTEM_PATHS = (  # (path, what, what it is for, what deleting it does, risk, command run as root)
    ("/var/cache/libdnf5", "dnf cache", "packages and repository metadata downloaded by dnf", "downloaded again at the next update", "safe",
     ["dnf", "clean", "all"]),
    ("/var/cache/dnf", "dnf cache (dnf 4)", "packages and repository metadata downloaded by dnf", "downloaded again at the next update", "safe",
     ["dnf", "clean", "all"]),
    ("/var/cache/PackageKit", "GNOME Software cache (PackageKit)", "packages and metadata downloaded by GNOME Software",
     "downloaded again at the next check for updates", "safe", ["find", "/var/cache/PackageKit", "-mindepth", "1", "-delete"]),
    ("/var/lib/dnf/offline", "updates waiting to be installed", "updates downloaded in advance (GNOME Software / dnf offline), installed at the "
     "next 'restart and update'", "downloaded again at the next check; to keep them, install them instead (restart and update): they go by "
     "themselves after", "review", ["dnf5", "offline", "clean"]),
    ("/var/lib/dnf/system-upgrade", "Fedora upgrade leftovers", "packages of a Fedora version upgrade (dnf system-upgrade)",
     "a finished upgrade needs them no more; an upgrade not made yet downloads them again", "review", ["dnf", "system-upgrade", "clean"]),
    ("/var/lib/systemd/coredump", "crash dumps of the system", "memory of the programs that crashed, for coredumpctl debug",
     "those past crashes can no longer be debugged", "safe", ["find", "/var/lib/systemd/coredump", "-type", "f", "-delete"]),
)
ARCHES = ("x86_64", "aarch64", "i386", "arm")

def volumes(engine: str) -> dict:
    """Sizes of the volumes of docker / podman (from system df -v)."""
    sizes, inside = {}, False
    for line in run([engine, "system", "df", "-v"], timeout=120).splitlines():
        if line.startswith("VOLUME NAME"):
            inside = True
        elif inside and not line.strip():
            break
        elif inside:
            sizes[line.split()[0]] = parse_size(line.split()[-1])
    return sizes

def scan_containers(min_bytes: int) -> list:
    found = []
    for engine in ("docker", "podman"):
        if not shutil.which(engine):
            continue
        out = run([engine, "system", "df", "--format", "{{.Type}}\t{{.Reclaimable}}"], timeout=60)
        reclaim = [line.split("\t") for line in out.splitlines() if "\t" in line and "Volume" not in line]
        text = ", ".join(f"{kind}: {size}" for kind, size in reclaim if not size.startswith("0B"))
        if text:
            found.append(candidate("system", engine, f"{engine}: unused images, stopped containers, build cache ({text})", 0, 0,
                                   f"images no container uses, stopped containers, unused networks and the build cache of {engine}",
                                   "pulled / built again when a project starts (slower once); the volumes (data) are not touched", "review",
                                   command=[engine, "system", "prune", "-af"]))
        sizes = volumes(engine)
        for name in run([engine, "volume", "ls", "-q", "-f", "dangling=true"], timeout=60).split():
            if sizes.get(name, 0) >= min_bytes:
                found.append(candidate("system", "volume", f"{engine} volume {name} (no container uses it)", sizes[name], 0,
                                       "the data of a container (a database, uploads...): an old compose project, a renamed one, a test",
                                       "its data is lost for good - check the project it belonged to is over", "review",
                                       command=[engine, "volume", "rm", name]))
    return found

def scan_flatpak(min_bytes: int) -> list:
    if not shutil.which("flatpak"):
        return []
    found, sizes = [], {}
    for line in run(["flatpak", "list", "--runtime", "--columns=ref,size"]).splitlines():
        if "\t" in line:
            sizes[line.split("\t")[0]] = parse_size(line.split("\t")[1])
    # what `flatpak uninstall --unused` would remove: without a terminal it lists them and answers no by itself (and "n" is typed)
    unused = []
    for line in run(["flatpak", "uninstall", "--unused"], timeout=120, stdin="n\n").splitlines():
        tokens = [t for t in line.split() if not re.fullmatch(r"\[.?\]|\[|\]", t)]
        if len(tokens) >= 3 and re.fullmatch(r"\d+\.", tokens[0]) and "." in tokens[1]:
            unused.append((tokens[1], next((t for t in tokens[2:] if t not in ARCHES), "")))
    if unused:
        size = sum(s for ref, s in sizes.items() if (ref.split("/")[0], ref.split("/")[-1]) in unused)
        found.append(candidate("system", "flatpak", f"flatpak runtimes no app uses: {', '.join(f'{i}//{b}' for i, b in unused)}", size, 0,
                               "runtimes and extensions left by apps removed or updated", "nothing uses them: an app needing one installs it again",
                               "safe", command=["flatpak", "uninstall", "--unused", "-y", "--noninteractive"]))
    # SDKs no app runs on (`--unused` keeps them), with their Locale / Debug / Docs; their related extensions are kept (--no-related)
    used = set(run(["flatpak", "list", "--app", "--columns=runtime"]).split())
    apps = set(run(["flatpak", "list", "--app", "--columns=application"]).split())
    left = {i for i, _ in unused}
    for ref in sorted(sizes):
        name, arch, branch = (ref.split("/") + ["", ""])[:3]
        if not name.endswith(".Sdk") or ref in used or name in left:
            continue
        refs = [r for r in sizes if r.split("/")[0] in (name, f"{name}.Locale", f"{name}.Debug", f"{name}.Docs") and r.endswith(f"/{arch}/{branch}")]
        size = sum(sizes[r] for r in refs)
        if size >= min_bytes:
            found.append(candidate("system", "flatpak-sdk", f"flatpak SDK {name}//{branch}", size, 0,
                                   "the tools to build flatpak apps; no installed app runs on it (flatpak uninstall --unused keeps it)",
                                   "the apps keep working (they run on the Platform); installed again when a build needs it", "review",
                                   command=["flatpak", "uninstall", "-y", "--noninteractive", "--no-related"] + refs))
    # data of the flatpak apps no longer installed
    data = HOME / ".var" / "app"
    for path in sorted(data.iterdir()) if apps and data.is_dir() else []:
        if path.is_dir() and path.name not in apps and size_of(path) >= min_bytes:
            found.append(candidate("system", "flatpak-data", tilde(path), size_of(path), age_days(path),
                                   f"settings and files of the flatpak app {path.name}, no longer installed",
                                   "lost for good: reinstalling the app starts from zero", "review", str(path)))
    return found

def old_kernels() -> list:
    """The kernels other than the running one, the newest and one fallback (rpm systems)."""
    lines = [line.split() for line in run(["rpm", "-q", "kernel-core", "--qf", "%{INSTALLTIME} %{VERSION}-%{RELEASE}.%{ARCH}\n"]).splitlines()]
    kernels = sorted(((int(t), v) for t, v in (line for line in lines if len(line) == 2 and line[0].isdigit())), reverse=True)
    keep = {os.uname().release} | {v for _, v in kernels[:1]}
    keep |= {next((v for _, v in kernels if v not in keep), "")}
    found = []
    for installed, version in kernels:
        if version in keep:
            continue
        size = size_of(Path("/usr/lib/modules") / version) + sum(size_of(p) for p in Path("/boot").glob(f"*{version}*"))
        found.append(candidate("system", "kernel", f"kernel {version}", size, (time.time() - installed) / DAY,
                               "an older Linux kernel, kept in the boot menu as a fallback",
                               "it leaves the boot menu (the running kernel and one fallback are never listed; dnf also drops the oldest "
                               "by itself at each kernel update)", "review", command=["dnf", "remove", "-y", f"kernel-core-{version}"], root=True))
    return found

def scan_system(min_bytes: int) -> list:
    found = scan_containers(min_bytes)
    usage = run(["journalctl", "--user", "--disk-usage"])
    if "take up" in usage:
        found.append(candidate("system", "journal", f"the journal of your user ({usage.split('take up')[1].split(' in ')[0].strip()})", 0, 0,
                               "logs of your user session (journalctl --user)", "logs older than 2 weeks are gone", "safe",
                               command=["journalctl", "--user", "--vacuum-time=2weeks"]))
    usage = run(["journalctl", "--disk-usage"])
    if "take up" in usage:
        found.append(candidate("system", "journal", f"the journal of the system ({usage.split('take up')[1].split(' in ')[0].strip()})", 0, 0,
                               "logs of the system and its services (journalctl)", "logs older than 2 weeks are gone", "safe",
                               command=["journalctl", "--vacuum-time=2weeks"], root=True))
    for where, what, use, after, risk, command in SYSTEM_PATHS:
        path = Path(where)
        if path.is_dir() and shutil.which(command[0]):
            size = size_of(path)
            if size >= min_bytes:
                found.append(candidate("system", "system", f"{where}: {what}", size, age_days(path), use, after, risk, command=command, root=True))
    return found + scan_flatpak(min_bytes) + old_kernels()

# =========================
# Claude Code / claude-cluster
# =========================
def running_job() -> str:
    return os.environ.get("CLAUDE_JOB_DIR", "")

def scan_claude(days: float, min_bytes: int) -> list:
    found = []
    claude = HOME / ".claude"
    # transcripts of Claude Code not touched for `days` (a project folder at a time)
    projects = claude / "projects"
    for project in projects.iterdir() if projects.is_dir() else []:
        old = [f for f in project.glob("*.jsonl") if age_days(f) >= days]
        size = sum(f.stat().st_size for f in old)
        if old and size >= min_bytes:
            found.append(candidate("claude", "transcripts", f"~/.claude/projects/{project.name}: {len(old)} conversation(s) older than {days:.0f} days",
                                   size, min(age_days(f) for f in old), "old conversations of Claude Code (claude --resume and the search of "
                                   "earlier sessions read them)", "they can no longer be resumed or searched", "review",
                                   command=["rm", "-f"] + [str(f) for f in old]))
    # finished background jobs
    jobs = claude / "jobs"
    for job in jobs.iterdir() if jobs.is_dir() else []:
        if not job.is_dir() or str(job) == running_job():
            continue
        state = {}
        try:
            state = json.loads((job / "state.json").read_text())
        except (OSError, ValueError):
            pass
        status = str(state.get("status", state.get("state", ""))).lower()
        age = age_days(job)
        if (status in ("done", "completed", "finished", "failed", "stopped") or age >= days) and size_of(job) >= min_bytes:
            found.append(candidate("claude", "job", f"~/.claude/jobs/{job.name} ({status or 'idle'}, {age:.0f} days)", size_of(job), age,
                                   "the temporary files of a background job no longer running", "its outputs and logs are gone (what it "
                                   "committed stays)", "safe" if status else "review", str(job)))
    for name, what in (("shell-snapshots", "shell snapshots"), ("file-history", "file history of edits"), ("todos", "todo lists"), ("debug", "debug logs")):
        path = claude / name
        old = [f for f in path.iterdir() if age_days(f) >= days] if path.is_dir() else []
        size = sum(size_of(f) for f in old)
        if old and size >= min_bytes:
            found.append(candidate("claude", name, f"~/.claude/{name}: {len(old)} {what} older than {days:.0f} days", size, days,
                                   f"{what} kept by Claude Code for its sessions", "the old sessions lose them (undo of edits, shell state, "
                                   "todos, logs)", "safe", command=["rm", "-rf"] + [str(f) for f in old]))
    # worktrees of .claude/worktrees: merged or without changes
    seen = set()
    for top in [HOME / "personal_delivery", HOME / "delivery", HOME]:
        for wt in top.glob("**/.claude/worktrees/*") if top.is_dir() else []:
            if wt in seen or not wt.is_dir() or (running_job() and str(wt) in os.getcwd()):
                continue
            seen.add(wt)
            if run(["git", "-C", str(wt), "status", "--porcelain"]).strip():
                continue
            branch = run(["git", "-C", str(wt), "rev-parse", "--abbrev-ref", "HEAD"]).strip()
            main = wt.parent.parent.parent
            found.append(candidate("claude", "worktree", f"worktree {tilde(wt)} (branch {branch}, no change)", size_of(wt), age_days(wt),
                                   "a git worktree made by Claude Code for an isolated task, with nothing uncommitted",
                                   "the folder goes, its branch and commits stay in the repository", "review",
                                   command=["git", "-C", str(main), "worktree", "remove", "--force", str(wt)]))
    # claude-cluster: its trash, the transcripts of sessions it no longer has, its showcase, its debug log
    data = Path(os.environ.get("CLAUDE_CLUSTER_DATA_DIR", HOME / ".local" / "share" / "claude-cluster"))
    if data.is_dir():
        known = set()
        try:
            state = json.loads((data / "state.json").read_text())
            known = {s.get("id") for s in state.get("sessions", []) + state.get("trash", []) if isinstance(s, dict)}
        except (OSError, ValueError):
            pass
        archive = data / "logs" / "archive"
        if size_of(archive) >= min_bytes:
            found.append(candidate("claude", "cluster", "claude-cluster: archived conversations (logs/archive)", size_of(archive), age_days(archive),
                                   "conversations of sessions started new (/clear), kept for reading only",
                                   "no longer readable in claude-cluster", "review", str(archive)))
        orphans = [f for f in (data / "logs").glob("*.jsonl") if f.stem not in known and not f.stem.startswith("g")] if known else []
        size = sum(f.stat().st_size for f in orphans)
        if orphans and size >= min_bytes:
            found.append(candidate("claude", "cluster", f"claude-cluster: {len(orphans)} transcript(s) of sessions it no longer has", size, 0,
                                   "conversations of sessions deleted since", "nothing shows them any more: nothing changes", "safe",
                                   command=["rm", "-f"] + [str(f) for f in orphans]))
        for name, what in (("showcase", "the showcase session of the Debug page"), ("debug.log", "the debug trace"),
                           ("debug.log.1", "the old debug trace"), ("images", "pictures pasted in prompts")):
            path = data / name
            if path.exists() and size_of(path) >= min_bytes and (name != "images" or age_days(path) >= days):
                found.append(candidate("claude", "cluster", f"claude-cluster: {name}", size_of(path), age_days(path), what,
                                       "made again when needed" if name != "images" else "old prompts show no picture",
                                       "safe" if name != "images" else "review", str(path)))
    return found

# =========================
# Program
# =========================
GROUPS = (("cache", "Caches"), ("build", "Build outputs and envs of old projects"), ("files", "Files: trash, downloads, duplicates, big / core / log files"),
          ("system", "System and containers"), ("claude", "Claude Code / claude-cluster"))

def scan(args: argparse.Namespace, measure: bool) -> list:
    roots = [Path(r).expanduser() for r in (args.root or [str(HOME)])]
    min_bytes = int(args.min_mb * MB)
    found = scan_caches(min_bytes) + scan_files(args.days, min_bytes) + scan_tree(args.days, min_bytes, roots) + scan_system(min_bytes) \
        + scan_claude(args.days, min_bytes)
    unique, seen = [], set()
    for c in found:
        if c["path"] and c["path"] in seen:
            continue
        seen.add(c["path"])
        unique.append(c)
    for c in unique if measure else []:
        if c["path"] and not c["command"] and c["size"] >= min_bytes:  # a command (pnpm store prune...) decides what it frees
            c["frees"] = frees_for_sure(Path(c["path"]))
    order = [group for group, _ in GROUPS]
    return sorted(unique, key=lambda c: (order.index(c["group"]), -c["size"]))

def biggest(count: int) -> list:
    """The biggest folders of the home (3 levels), a folder left out when one of its children holds most of it."""
    sizes = {}
    for line in run(["du", "-x", "-b", "-d", "3", str(HOME)], timeout=900).splitlines():
        size, _, name = line.partition("\t")
        if size.isdigit():
            sizes[Path(name)] = int(size)
    child = {}
    for path, size in sizes.items():
        child[path.parent] = max(child.get(path.parent, 0), size)
    return [(p, s) for s, p in sorted(((s, p) for p, s in sizes.items() if p != HOME and child.get(p, 0) < 0.6 * s), reverse=True)[:count]]

def show(found: list, top: list) -> None:
    for group, title in GROUPS:
        part = [c for c in found if c["group"] == group]
        if not part:
            continue
        print(f"\n{title} ({human(sum(c['size'] for c in part))})")
        for c in part:
            size = human(c["size"]) if c["size"] else "?"
            age = f" ({c['age']:.0f} days)" if c["age"] >= 1 else ""
            print(f"  [{c['id']}] {size:>9}  {c['risk']:6}  {c['what']}{age}")
            print(f"           for:   {c['use']}")
            print(f"           after: {c['after']}")
            if c["frees"] is not None and c["frees"] < 0.9 * c["size"]:
                print(f"           frees at least {human(c['frees'])}: the rest is shared with other files (btrfs reflinks / snapshots)")
            if c["command"]:
                more = " ..." if len(c["command"]) > 7 else ""
                print(f"           run:   {'sudo -A ' if c['root'] else ''}{' '.join(c['command'][:7])}{more}")
    total = sum(c["size"] for c in found)
    sure = sum(c["size"] if c["frees"] is None else c["frees"] for c in found)
    print(f"\n{len(found)} candidate(s), {human(total)} known (Docker, journals: told by their own tool)"
          + (f", at least {human(sure)} freed for sure (btrfs shared data aside)" if sure < 0.95 * total else ""))
    if top:
        print("\nBiggest folders of the home (information only: none of them is a candidate)")
        for path, size in top:
            print(f"  {human(size):>9}  {tilde(path)}")

def clean(args: argparse.Namespace) -> int:
    found = {c["id"]: c for c in scan(args, measure=False)}
    failed = 0
    for wanted in args.ids:
        c = found.get(wanted)
        if not c:
            print(f"  skip {wanted}: no longer found (scan again)")
            continue
        if args.dry_run:
            print(f"  would clean [{wanted}] {c['what']}")
            continue
        if not c["command"]:
            path = Path(c["path"])
            try:
                if path.is_dir() and not path.is_symlink():
                    shutil.rmtree(path)
                else:
                    path.unlink(missing_ok=True)
                print(f"  cleaned [{wanted}] {c['what']} ({human(c['size'])})")
            except OSError as error:
                failed += 1
                print(f"  failed [{wanted}] {c['what']}: {error}")
            continue
        command = c["command"]
        if c["root"]:
            if not os.environ.get("SUDO_ASKPASS"):
                print(f"  root needed [{wanted}]: run  SUDO_ASKPASS=~/.local/bin/sudo-askpass sudo -A {' '.join(command)}")
                continue
            command = ["sudo", "-A"] + command
        result = subprocess.run(command, capture_output=True, text=True, input="")
        if result.returncode == 0:
            print(f"  cleaned [{wanted}] {c['what']}" + (f" ({human(c['size'])})" if c["size"] else ""))
        else:
            failed += 1
            print(f"  failed [{wanted}] {c['what']}: {(result.stderr or result.stdout).strip()[:200]}")
    return 1 if failed else 0

def main() -> int:
    parser = argparse.ArgumentParser(description="What can be cleaned on this computer and in the data of Claude Code / claude-cluster.")
    parser.add_argument("command", choices=["scan", "clean"])
    parser.add_argument("ids", nargs="*", help="clean: the ids given by scan")
    parser.add_argument("--days", type=float, default=30, help="untouched for at least this many days (default 30; 7 for a deep clean)")
    parser.add_argument("--min-mb", type=float, default=5, help="at least this size (default 5 MB)")
    parser.add_argument("--root", action="append", help="where to look for old build outputs and big files (default: the home folder; repeatable)")
    parser.add_argument("--top", type=int, default=12, help="scan: show the N biggest folders of the home for information (default 12, 0: none)")
    parser.add_argument("--json", action="store_true", help="scan: the candidates as JSON")
    parser.add_argument("--dry-run", action="store_true", help="clean: say what would be cleaned")
    args = parser.parse_args()
    if args.command == "scan":
        found = scan(args, measure=True)
        if args.json:
            print(json.dumps(found, indent=1))
        else:
            show(found, biggest(args.top) if args.top > 0 else [])
        return 0
    if not args.ids:
        sys.stderr.write("computer_clean.py clean: give the ids of scan\n")
        return 2
    return clean(args)

if __name__ == "__main__":
    sys.exit(main())
