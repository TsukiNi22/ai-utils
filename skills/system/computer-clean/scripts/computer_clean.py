#!/usr/bin/env python3
"""
Find what can be cleaned on this computer (caches, build outputs, Docker, journals, trash) and in the data of Claude
Code / claude-cluster (old transcripts, finished jobs, merged worktrees, archived sessions), then delete the chosen ones.

Usage:
    computer_clean.py scan [--days N] [--min-mb N] [--root DIR]... [--json]
    computer_clean.py clean ID... [--days N] [--min-mb N] [--root DIR]... [--dry-run]

- scan: every candidate with an id, its size, its age, its kind, why it is safe (or what to check) and its command.
- clean: the candidates of these ids deleted for good (rm -rf, or their command: docker prune, journalctl --vacuum...);
  the scan is made again first (same options), an id that is gone or changed is skipped.
- A candidate needing root (dnf cache, system journal) is run with sudo -A (SUDO_ASKPASS) when set, else told.
- Never touched: a git-tracked file, a worktree with changes, the claude-cluster session running, this job's folder.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

HOME = Path.home()
DAY = 86400.0

# =========================
# Tools
# =========================
def run(command: list, timeout: int = 60) -> str:
    """The output of a command ("" when it fails or is missing)."""
    if not shutil.which(command[0]):
        return ""
    try:
        return subprocess.run(command, capture_output=True, text=True, timeout=timeout).stdout
    except (OSError, subprocess.SubprocessError):
        return ""

def size_of(path: Path) -> int:
    """Bytes of a file or a folder (du -sb: fast, links not followed)."""
    if not path.exists():
        return 0
    if path.is_file() or path.is_symlink():
        return path.lstat().st_size
    out = run(["du", "-sb", str(path)], timeout=120)
    try:
        return int(out.split()[0])
    except (IndexError, ValueError):
        return 0

def age_days(path: Path) -> float:
    """Days since its last change (the newest of the folder itself and its direct content)."""
    try:
        newest = path.stat().st_mtime
        if path.is_dir():
            for child in list(path.iterdir())[:200]:
                newest = max(newest, child.lstat().st_mtime)
        return (time.time() - newest) / DAY
    except OSError:
        return 0.0

def human(size: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if size < 1024 or unit == "GB":
            return f"{size:.0f} {unit}" if unit == "B" else f"{size:.1f} {unit}"
        size /= 1024
    return f"{size:.1f} GB"

def candidate(group: str, kind: str, what: str, size: int, age: float, why: str, risk: str, path: str = "", command: list = None, root: bool = False) -> dict:
    key = path or " ".join(command or [])
    return {"id": hashlib.sha1(f"{kind}:{key}".encode()).hexdigest()[:6], "group": group, "kind": kind, "what": what, "size": size,
            "age": round(age, 1), "why": why, "risk": risk, "path": path, "command": command or [], "root": root}

def tracked(path: Path) -> bool:
    """True when the path (or something in it) is tracked by a git repository."""
    top = run(["git", "-C", str(path.parent), "rev-parse", "--show-toplevel"]).strip()
    if not top:
        return False
    return bool(run(["git", "-C", top, "ls-files", "--", str(path)]).strip())

# =========================
# The computer
# =========================
CACHES = {  # ~/.cache/<name>: what it is (rebuilt on demand)
    "pip": "pip downloads", "pypoetry": "poetry", "uv": "uv", "npm": "npm", "yarn": "yarn", "pnpm": "pnpm", "go-build": "Go builds", "ccache": "ccache",
    "tsukini-skills": "builds of the tools of ai-utils", "thumbnails": "thumbnails", "mesa_shader_cache": "shader cache", "fontconfig": "fontconfig",
    "JetBrains": "JetBrains caches", "mozilla": "Firefox cache", "google-chrome": "Chrome cache", "chromium": "Chromium cache", "pre-commit": "pre-commit envs",
    "huggingface": "Hugging Face models (downloaded again when used)", "torch": "PyTorch hub", "electron": "Electron downloads", "typescript": "TypeScript",
}
BUILD_DIRS = {"build", "cmake-build-debug", "cmake-build-release", "node_modules", "__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache",
              ".tox", ".gradle", ".cache"}

def scan_computer(days: float, min_bytes: int, roots: list) -> list:
    found = []
    cache = Path(os.environ.get("XDG_CACHE_HOME", HOME / ".cache"))
    for name, what in CACHES.items():
        path = cache / name
        size = size_of(path)
        if size >= min_bytes:
            found.append(candidate("computer", "cache", f"~/.cache/{name}: {what}", size, age_days(path), "a cache: rebuilt when needed", "safe", str(path)))
    npm = HOME / ".npm" / "_cacache"
    if size_of(npm) >= min_bytes:
        found.append(candidate("computer", "cache", "~/.npm/_cacache: npm", size_of(npm), age_days(npm), "a cache: rebuilt when needed", "safe", str(npm)))
    cargo = HOME / ".cargo" / "registry" / "cache"
    if size_of(cargo) >= min_bytes:
        found.append(candidate("computer", "cache", "~/.cargo/registry/cache: crates downloaded", size_of(cargo), age_days(cargo), "downloaded again when needed",
                               "safe", str(cargo)))
    trash = HOME / ".local" / "share" / "Trash"
    if size_of(trash) >= min_bytes:
        found.append(candidate("computer", "trash", "the trash of the desktop", size_of(trash), age_days(trash), "already thrown away", "safe", str(trash)))
    # build outputs of old projects (not tracked by git, untouched for `days`)
    for root in roots:
        if not root.is_dir():
            continue
        for dirpath, dirnames, _ in os.walk(root):
            depth = Path(dirpath).relative_to(root).parts
            if len(depth) > 6 or any(p.startswith(".") for p in depth) or any(p in ("Android", "actions-runner", "externals", "site-packages", "go", "sdk", "Sdk") for p in depth):
                dirnames[:] = []  # hidden folders and installed programs (their own folders, never build outputs)
                continue
            for name in list(dirnames):
                path = Path(dirpath) / name
                if name in BUILD_DIRS or name.startswith("build-") or name.startswith("cmake-build-"):
                    dirnames.remove(name)
                    if name == ".cache" and not (Path(dirpath) / "compile_commands.json").exists() and not (Path(dirpath) / "CMakeLists.txt").exists():
                        continue  # a .cache of clangd only
                    markers = ("package.json",) if name == "node_modules" else ("CMakeLists.txt", "Makefile", "setup.py", "pyproject.toml", "package.json",
                                                                                  "meson.build", "build.gradle", "Cargo.toml") if name not in ("__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache", ".tox") else ()
                    if markers and not any((Path(dirpath) / f).exists() for f in markers):
                        continue  # next to no project file: not the output of a build here
                    age = age_days(path)
                    if age < days or tracked(path):
                        continue
                    size = size_of(path)
                    if size >= min_bytes:
                        found.append(candidate("computer", "build", f"{path.relative_to(HOME) if path.is_relative_to(HOME) else path}", size, age,
                                               f"a build output untouched for {age:.0f} days, made again by a build", "safe", str(path)))
                elif name in (".git", "proc", "sys"):
                    dirnames.remove(name)
    # big old files in Downloads
    downloads = HOME / "Downloads"
    if downloads.is_dir():
        for path in downloads.iterdir():
            try:
                if path.is_file() and path.stat().st_size >= max(min_bytes, 200 * 1024 * 1024) and age_days(path) >= days:
                    found.append(candidate("computer", "download", f"~/Downloads/{path.name}", path.stat().st_size, age_days(path),
                                           "a big download not opened for a while: check you don't need it", "review", str(path)))
            except OSError:
                pass
    # Docker / Podman: what no container uses
    for engine in ("docker", "podman"):
        if shutil.which(engine):
            out = run([engine, "system", "df", "--format", "{{.Type}}\t{{.Reclaimable}}"], timeout=30)
            reclaim = [line.split("\t") for line in out.splitlines() if "\t" in line]
            text = ", ".join(f"{t}: {r}" for t, r in reclaim if not r.startswith("0B"))
            if text:
                found.append(candidate("computer", engine, f"{engine}: unused images, stopped containers, build cache ({text})", 0, 0,
                                       "nothing a running or stopped-and-kept container needs (prune: dangling and unused)", "review",
                                       command=[engine, "system", "prune", "-af"]))
    # journals, dnf, flatpak
    usage = run(["journalctl", "--user", "--disk-usage"])
    if "take up" in usage:
        found.append(candidate("computer", "journal", f"the journal of your user ({usage.split('take up')[1].split('in')[0].strip()})", 0, 0,
                               "old logs; the last 2 weeks kept", "safe", command=["journalctl", "--user", "--vacuum-time=2weeks"]))
    if Path("/var/cache/dnf").is_dir() and size_of(Path("/var/cache/dnf")) >= min_bytes:
        found.append(candidate("computer", "dnf", "/var/cache/dnf: downloaded packages and metadata", size_of(Path("/var/cache/dnf")), age_days(Path("/var/cache/dnf")),
                               "downloaded again at the next update", "safe", command=["dnf", "clean", "all"], root=True))
    if shutil.which("flatpak") and "Nothing unused" not in run(["flatpak", "uninstall", "--unused", "--assumeyes", "--noninteractive", "--dry-run"]):
        found.append(candidate("computer", "flatpak", "flatpak runtimes no app uses", 0, 0, "unused runtimes", "safe",
                               command=["flatpak", "uninstall", "--unused", "-y"]))
    return found

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
    if projects.is_dir():
        for project in projects.iterdir():
            old = [f for f in project.glob("*.jsonl") if age_days(f) >= days]
            size = sum(f.stat().st_size for f in old)
            if old and size >= min_bytes:
                found.append(candidate("claude", "transcripts", f"~/.claude/projects/{project.name}: {len(old)} conversation(s) older than {days:.0f} days",
                                       size, min(age_days(f) for f in old), "old conversations of Claude Code (--resume no longer finds them)", "review",
                                       command=["rm", "-f"] + [str(f) for f in old]))
    # finished background jobs
    jobs = claude / "jobs"
    if jobs.is_dir():
        for job in jobs.iterdir():
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
                                       "a background job no longer running: its temporary files", "safe" if status else "review", str(job)))
    for name, what in (("shell-snapshots", "shell snapshots"), ("file-history", "file history of edits"), ("todos", "todo lists"), ("debug", "debug logs")):
        path = claude / name
        old = [f for f in path.iterdir() if age_days(f) >= days] if path.is_dir() else []
        size = sum(size_of(f) for f in old)
        if old and size >= min_bytes:
            found.append(candidate("claude", name, f"~/.claude/{name}: {len(old)} {what} older than {days:.0f} days", size, days, "kept by Claude Code for a while only",
                                   "safe", command=["rm", "-rf"] + [str(f) for f in old]))
    # worktrees of .claude/worktrees: merged or without changes
    seen = set()
    for top in [HOME / "personal_delivery", HOME / "delivery", HOME]:
        if not top.is_dir():
            continue
        for wt in top.glob("**/.claude/worktrees/*"):
            if wt in seen or not wt.is_dir() or (running_job() and str(wt) in os.getcwd()):
                continue
            seen.add(wt)
            changes = run(["git", "-C", str(wt), "status", "--porcelain"]).strip()
            if changes:
                continue
            branch = run(["git", "-C", str(wt), "rev-parse", "--abbrev-ref", "HEAD"]).strip()
            main = wt.parent.parent.parent
            found.append(candidate("claude", "worktree", f"worktree {wt.relative_to(HOME)} (branch {branch}, no change)", size_of(wt), age_days(wt),
                                   "a worktree of Claude Code with nothing uncommitted (its branch kept)", "review",
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
                                   "conversations of sessions started new (/clear): kept for reading only", "review", str(archive)))
        orphans = [f for f in (data / "logs").glob("*.jsonl") if f.stem not in known and not f.stem.startswith("g")] if known else []
        size = sum(f.stat().st_size for f in orphans)
        if orphans and size >= min_bytes:
            found.append(candidate("claude", "cluster", f"claude-cluster: {len(orphans)} transcript(s) of sessions it no longer has", size, 0,
                                   "their session deleted: nothing shows them any more", "safe", command=["rm", "-f"] + [str(f) for f in orphans]))
        for name, what in (("showcase", "the showcase session of the Debug page"), ("debug.log", "the debug trace"), ("debug.log.1", "the old debug trace"),
                           ("images", "pictures pasted in prompts")):
            path = data / name
            if path.exists() and size_of(path) >= min_bytes and (name != "images" or age_days(path) >= days):
                found.append(candidate("claude", "cluster", f"claude-cluster: {name} ({what})", size_of(path), age_days(path), "made again when needed",
                                       "safe" if name != "images" else "review", str(path)))
    return found

# =========================
# Program
# =========================
def scan(args: argparse.Namespace) -> list:
    roots = [Path(r).expanduser() for r in (args.root or [str(HOME)])]
    found = scan_computer(args.days, args.min_mb * 1024 * 1024, roots) + scan_claude(args.days, args.min_mb * 1024 * 1024)
    return sorted(found, key=lambda c: (c["group"], -c["size"]))

def show(found: list) -> None:
    total = sum(c["size"] for c in found)
    for group in ("computer", "claude"):
        part = [c for c in found if c["group"] == group]
        if not part:
            continue
        print(f"\n{'This computer' if group == 'computer' else 'Claude Code / claude-cluster'} ({human(sum(c['size'] for c in part))})")
        for c in part:
            size = human(c["size"]) if c["size"] else "?"
            print(f"  [{c['id']}] {size:>9}  {c['risk']:6}  {c['what']}")
            print(f"           {c['why']}" + (" (needs root)" if c["root"] else ""))
    print(f"\n{len(found)} candidate(s), {human(total)} known (Docker, journals: told by their own tool)")

def clean(args: argparse.Namespace) -> int:
    found = {c["id"]: c for c in scan(args)}
    failed = 0
    for wanted in args.ids:
        c = found.get(wanted)
        if not c:
            print(f"  skip {wanted}: no longer found (scan again)")
            continue
        if args.dry_run:
            print(f"  would clean [{wanted}] {c['what']}")
            continue
        if c["path"]:
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
        result = subprocess.run(command, capture_output=True, text=True)
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
    parser.add_argument("--days", type=float, default=30, help="untouched for at least this many days (default 30)")
    parser.add_argument("--min-mb", type=float, default=5, help="at least this size (default 5 MB)")
    parser.add_argument("--root", action="append", help="where to look for old build outputs (default: the home folder; repeatable)")
    parser.add_argument("--json", action="store_true", help="scan: the candidates as JSON")
    parser.add_argument("--dry-run", action="store_true", help="clean: say what would be cleaned")
    args = parser.parse_args()
    if args.command == "scan":
        found = scan(args)
        if args.json:
            print(json.dumps(found, indent=1))
        else:
            show(found)
        return 0
    if not args.ids:
        sys.stderr.write("computer_clean.py clean: give the ids of scan\n")
        return 2
    return clean(args)

if __name__ == "__main__":
    sys.exit(main())
