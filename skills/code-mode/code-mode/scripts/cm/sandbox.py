"""The optional OS sandbox of the code runner: no network, writes limited to the work folders, hidden files out of sight.

Linux: bubblewrap (bwrap). macOS: sandbox-exec. Windows: none. Off by default and asked at setup. When it is on and
the tool is missing, the code does not run at all (never silently outside the sandbox). The runner needs nothing but
its pipes to the engine, so cutting its network and its view of the secrets takes nothing away from the scripts.
Every hidden entry of the home folder (`~/.ssh`, `~/.config`, `~/.claude.json`, tokens of every tool) is out of sight,
not a list of known names: the code only sees the ordinary folders of the user. `sandbox_show` gives one back.
"""
from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

from .paths import home, sub

SECRETS = ("Library/Keychains", "Library/Application Support", "Library/Cookies", "Library/Mail", "Library/Messages",
           "AppData")  # the places that hold secrets without being hidden entries (macOS, a Windows profile)

class SandboxError(Exception):
    """The sandbox is asked for and cannot be set up."""

def available() -> str:
    """The sandbox tool of this system, or nothing."""
    if sys.platform.startswith("linux") and shutil.which("bwrap"):
        return "bwrap"
    if sys.platform == "darwin" and shutil.which("sandbox-exec"):
        return "sandbox-exec"
    return ""

def describe() -> str:
    """One line for setup, status and doctor."""
    tool = available()
    if tool:
        return f"available ({tool}): no network, writes limited to the project folder, the hidden files of the home out of sight"
    need = "install bubblewrap (bwrap)" if sys.platform.startswith("linux") else "nothing equivalent on this system"
    return f"not available ({need})"

def hidden_paths(config: dict) -> list[Path]:
    """What the sandboxed code must not read: every hidden entry of the home, the codemode data, what the user added."""
    try:
        paths = [entry for entry in Path.home().iterdir() if entry.name.startswith(".")]
    except OSError:
        paths = []
    paths += [Path.home() / name for name in SECRETS]
    paths += [Path(str(extra)).expanduser() for extra in config.get("sandbox_hide", [])]
    return [path for path in [*paths, home()] if path.exists()]

def shown_paths(config: dict, argv: list[str]) -> list[Path]:
    """What stays readable inside a hidden folder: this Python, the codemode scripts, what the user gave back."""
    paths = [Path(sys.prefix), Path(sys.base_prefix), Path(sys.executable).parent, Path(sys.executable).resolve().parent,
             Path(__file__).resolve().parent.parent]
    paths += [Path(part).parent for part in argv[:2] if os.path.isabs(part)]
    paths += [Path(str(extra)).expanduser() for extra in config.get("sandbox_show", [])]
    return [path for path in dict.fromkeys(paths) if path.exists()]

def wrap(argv: list[str], cwd: str, writable: list[Path], config: dict) -> list[str]:
    """The command that runs argv inside the sandbox."""
    tool = available()
    if not tool:
        raise SandboxError(f"the sandbox is on in the codemode config but {describe()}: turn it off (setup --sandbox off) or install it")
    # the script is named by its real place: the installed link lives in a hidden folder of the home
    argv = [argv[0]] + [str(Path(part).resolve()) if os.path.isabs(part) and os.path.exists(part) else part for part in argv[1:]]
    work = [Path(cwd).resolve()] + [Path(path).resolve() for path in writable]
    work += [Path(str(extra)).expanduser().resolve() for extra in config.get("sandbox_writable", [])]
    work = [path for path in work if path.is_dir()]
    hidden = [path if path.is_symlink() else path.resolve() for path in hidden_paths(config)]
    hidden = [path for path in hidden if not path.is_symlink()]  # a link is hidden with what it points to, or is not ours to mask
    shown = [path for path in shown_paths(config, argv) if any(str(path).startswith(str(mask) + os.sep) or path == mask for mask in hidden)]
    if tool == "bwrap":
        command = ["bwrap", "--die-with-parent", "--new-session", "--unshare-net", "--unshare-ipc", "--unshare-pid", "--unshare-uts",
                   "--ro-bind", "/", "/", "--dev", "/dev", "--proc", "/proc", "--tmpfs", "/tmp"]
        command += ["--tmpfs", "/run"] if os.path.isdir("/run") else []
        blank = sub("run") / "empty"  # what a hidden file reads as (a device node is refused on a bind mount)
        blank.touch()
        for path in hidden:
            command += ["--tmpfs", str(path)] if path.is_dir() else ["--ro-bind", str(blank), str(path)]
        for path in shown:
            command += ["--ro-bind", str(path), str(path)]
        for path in work:
            command += ["--bind", str(path), str(path)]
        return [*command, "--chdir", str(work[0]) if work else "/", "--", *argv]
    rules = ["(version 1)", "(allow default)", "(deny network*)", "(deny file-write*)"]
    allowed = [str(path) for path in work] + ["/private/tmp", "/private/var/folders", "/dev"]
    rules.append("(allow file-write* " + " ".join(f'(subpath "{quoted(path)}")' for path in allowed) + ")")
    if hidden:
        rules.append("(deny file-read* " + " ".join(f'(subpath "{quoted(str(path))}")' for path in hidden) + ")")
    if shown:
        rules.append("(allow file-read* " + " ".join(f'(subpath "{quoted(str(path))}")' for path in shown) + ")")
    return ["sandbox-exec", "-p", "\n".join(rules), *argv]

def quoted(path: str) -> str:
    """A path inside a sandbox-exec string."""
    return path.replace("\\", "\\\\").replace('"', '\\"')
