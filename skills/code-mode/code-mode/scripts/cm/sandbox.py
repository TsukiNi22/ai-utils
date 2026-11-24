"""The optional OS sandbox of the code runner: no network, writes limited to the work folders, secrets hidden.

Linux: bubblewrap (bwrap). macOS: sandbox-exec. Windows: none. Off by default and asked at setup. When it is on and
the tool is missing, the code does not run at all (never silently outside the sandbox). The runner needs nothing but
its pipes to the engine, so cutting its network and its view of the secrets takes nothing away from the scripts.
"""
from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

from .paths import home

SECRETS = (".ssh", ".gnupg", ".aws", ".azure", ".kube", ".docker", ".netrc", ".git-credentials", ".npmrc", ".pypirc",
           ".password-store", ".claude.json", ".claude/.credentials.json", ".config/gh", ".config/gcloud", ".config/op",
           ".config/Claude", ".local/share/keyrings", ".mozilla", ".thunderbird", ".config/google-chrome", ".config/chromium",
           ".config/BraveSoftware", "Library/Keychains", "Library/Application Support/Claude", "Library/Cookies")

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
        return f"available ({tool}): no network, writes limited to the project folder and the codemode files, secrets hidden"
    need = "install bubblewrap (bwrap)" if sys.platform.startswith("linux") else "nothing equivalent on this system"
    return f"not available ({need})"

def hidden_paths(config: dict) -> list[Path]:
    """What the sandboxed code must not read: the usual secret places, the codemode data, what the user added."""
    paths = [Path.home() / name for name in SECRETS]
    paths += [Path(str(extra)).expanduser() for extra in config.get("sandbox_hide", [])]
    return [path for path in [*paths, home()] if path.exists()]

def wrap(argv: list[str], cwd: str, writable: list[Path], config: dict) -> list[str]:
    """The command that runs argv inside the sandbox."""
    tool = available()
    if not tool:
        raise SandboxError(f"the sandbox is on in the codemode config but {describe()}: turn it off (setup --sandbox off) or install it")
    work = [Path(cwd).resolve()] + [Path(path).resolve() for path in writable]
    work += [Path(str(extra)).expanduser().resolve() for extra in config.get("sandbox_writable", [])]
    work = [path for path in work if path.is_dir()]
    hidden = [path.resolve() for path in hidden_paths(config)]
    if tool == "bwrap":
        command = ["bwrap", "--die-with-parent", "--new-session", "--unshare-net", "--unshare-ipc", "--unshare-pid", "--unshare-uts",
                   "--ro-bind", "/", "/", "--dev", "/dev", "--proc", "/proc", "--tmpfs", "/tmp"]
        command += ["--tmpfs", "/run"] if os.path.isdir("/run") else []
        for path in hidden:
            command += ["--tmpfs", str(path)] if path.is_dir() else ["--ro-bind", "/dev/null", str(path)]
        for path in work:
            command += ["--bind", str(path), str(path)]
        return [*command, "--chdir", str(work[0]) if work else "/", "--", *argv]
    rules = ["(version 1)", "(allow default)", "(deny network*)", "(deny file-write*)"]
    allowed = [str(path) for path in work] + ["/private/tmp", "/private/var/folders", "/dev"]
    rules.append("(allow file-write* " + " ".join(f'(subpath "{quoted(path)}")' for path in allowed) + ")")
    if hidden:
        rules.append("(deny file-read* " + " ".join(f'(subpath "{quoted(str(path))}")' for path in hidden) + ")")
    return ["sandbox-exec", "-p", "\n".join(rules), *argv]

def quoted(path: str) -> str:
    """A path inside a sandbox-exec string."""
    return path.replace("\\", "\\\\").replace('"', '\\"')
