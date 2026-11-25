"""Install, show and remove codemode where the user wants it: this project, the whole computer, Claude Desktop.

`setup` registers the MCP server (its tools are then there in every new session of that scope), can move chosen
servers behind codemode (out of the client's own list: their tool names leave the context and they only start when
used), installs the Claude Code hooks and sets the sandbox. `uninstall` puts everything back as it was. The Claude
Code config is changed through the `claude mcp` commands when `claude` is on the PATH (it owns that file).
"""
from __future__ import annotations

import json
import os
import shlex
import shutil
import subprocess
import sys
import time
from collections.abc import Callable
from pathlib import Path

from . import sandbox
from .daemon import engines
from .hub import Hub, claude_json, desktop_config, is_self
from .paths import PRESETS, PRIORITIES, RULES, VERSION, home, load_config, priority_of, read_json, save_config, script_command, set_session_priority, write_json

HOOK_FLAGS = ("remind", "compress")

class SetupError(Exception):
    """A step of the setup that could not be done."""

def entry() -> dict:
    """How a client starts codemode as an MCP server."""
    command = script_command()
    return {"type": "stdio", "command": command[0], "args": [command[1], "serve"], "env": {}}

def run_claude(args: list[str], cwd: str) -> tuple[int, str]:
    """Run a `claude mcp ...` command; 127 when Claude Code is not installed."""
    program = None if os.environ.get("CODEMODE_NO_CLAUDE_CLI") else shutil.which("claude")
    if not program:
        return 127, "claude not found"
    try:
        done = subprocess.run([program, *args], capture_output=True, text=True, cwd=cwd, timeout=120, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        return 1, str(error)
    return done.returncode, (done.stdout + done.stderr).strip()

def edit_json(path: Path, change: Callable[[dict], None]) -> None:
    """Change a JSON config in place, a copy of the original kept next to it the first time."""
    data = read_json(path, {})
    data = data if isinstance(data, dict) else {}
    backup = path.with_name(path.name + ".codemode.bak")
    if not backup.exists():  # the state before codemode first touched the file ({} when there was none)
        if path.is_file():
            shutil.copy2(path, backup)
        else:
            write_json(backup, {})
    change(data)
    mode = (path.stat().st_mode & 0o777) if path.is_file() else 0o600
    write_json(path, data, mode)

def drop_backups(paths: list[Path]) -> None:
    """Remove the copies setup kept of files that are back to what they were (nothing of codemode left in them)."""
    for path in paths:
        backup = path.with_name(path.name + ".codemode.bak")
        if not backup.is_file():
            continue
        if read_json(backup, None) == read_json(path, None) or (not path.exists() and read_json(backup, None) == {}):
            try:
                backup.unlink()
            except OSError:
                pass  # kept: it is only a copy

def settings_path(scope: str, cwd: str) -> Path:
    """The Claude Code settings file that holds the hooks of a scope."""
    if scope == "user":
        return Path(os.environ.get("CLAUDE_CONFIG_DIR") or Path.home() / ".claude") / "settings.json"
    return Path(cwd) / ".claude" / "settings.local.json"

def hook_command(flags: list[str]) -> str:
    """The command of the hook, with this Python and this script (no PATH lookup at each tool call)."""
    parts = [Path(part).as_posix() if sys.platform == "win32" else part for part in script_command()]
    quoted = [f'"{part}"' if sys.platform == "win32" else shlex.quote(part) for part in parts]
    return " ".join([*quoted, "hook", *flags])

def is_ours(item: dict) -> bool:
    return any("codemode.py" in str(hook.get("command") or "") and " hook" in str(hook.get("command") or "") for hook in item.get("hooks") or [])

def set_hooks(path: Path, flags: list[str] | None) -> None:
    """Put (flags given) or remove (None) the codemode hook in a settings file, the other hooks untouched."""
    def change(data: dict) -> None:
        hooks = data.setdefault("hooks", {})
        kept = [item for item in hooks.get("PostToolUse") or [] if not (isinstance(item, dict) and is_ours(item))]
        if flags is not None:
            kept.append({"matcher": "mcp__.*", "hooks": [{"type": "command", "command": hook_command(flags), "timeout": 15}]})
        if kept:
            hooks["PostToolUse"] = kept
        else:
            hooks.pop("PostToolUse", None)
        if not hooks:
            data.pop("hooks", None)
    if flags is None and not path.is_file():
        return
    edit_json(path, change)

def installed_hooks(path: Path) -> str:
    """The flags of the codemode hook found in a settings file ('' when there is none)."""
    data = read_json(path, {})
    for item in ((data.get("hooks") or {}).get("PostToolUse") or []) if isinstance(data, dict) else []:
        if isinstance(item, dict) and is_ours(item):
            command = str(item["hooks"][0].get("command") or "")
            return " ".join(flag for flag in HOOK_FLAGS if f" {flag}" in command.split(" hook", 1)[-1]) or "sizes only"
    return ""

def manifest() -> dict:
    data = read_json(home() / "install.json", {})
    data = data if isinstance(data, dict) else {}
    data.setdefault("scopes", {"user": False, "local": [], "project": []})
    data.setdefault("hooks", [])
    data.setdefault("desktop", False)
    return data

def register(scope: str, cwd: str, report: list[str]) -> None:
    """Declare the codemode server in Claude Code for a scope (user, local or project)."""
    run_claude(["mcp", "remove", "codemode", "--scope", scope], cwd)
    code, out = run_claude(["mcp", "add-json", "--scope", scope, "codemode", json.dumps(entry())], cwd)
    if code == 127:
        if scope == "project":
            edit_json(Path(cwd) / ".mcp.json", lambda data: data.setdefault("mcpServers", {}).update({"codemode": entry()}))
        elif scope == "user":
            edit_json(claude_json(), lambda data: data.setdefault("mcpServers", {}).update({"codemode": entry()}))
        else:
            edit_json(claude_json(), lambda data: data.setdefault("projects", {}).setdefault(cwd, {}).setdefault("mcpServers", {})
                      .update({"codemode": entry()}))
        report.append(f"codemode written in the Claude Code config ({scope} scope; `claude` not on the PATH, file edited directly)")
    elif code != 0:
        raise SetupError(f"claude mcp add-json failed: {out[:300]}")
    else:
        report.append(f"codemode registered in Claude Code ({scope} scope)")

def unregister(scope: str, cwd: str, report: list[str]) -> None:
    code, _ = run_claude(["mcp", "remove", "codemode", "--scope", scope], cwd)
    if code == 127:
        if scope == "project":
            edit_json(Path(cwd) / ".mcp.json", lambda data: data.get("mcpServers", {}).pop("codemode", None))
        elif scope == "user":
            edit_json(claude_json(), lambda data: data.get("mcpServers", {}).pop("codemode", None))
        else:
            edit_json(claude_json(), lambda data: data.get("projects", {}).get(cwd, {}).get("mcpServers", {}).pop("codemode", None))
    report.append(f"codemode removed from Claude Code ({scope} scope{', ' + cwd if scope != 'user' else ''})")

def movable() -> dict[str, dict]:
    """The user scope servers of Claude Code that can go behind codemode."""
    data = read_json(claude_json(), {})
    servers = data.get("mcpServers") if isinstance(data, dict) else {}
    return {name: spec for name, spec in (servers or {}).items() if isinstance(spec, dict) and not is_self(name, spec)}

def move_servers(names: list[str], cwd: str, report: list[str]) -> None:
    """Take servers out of Claude Code's own list and keep them for codemode alone (saved before they are removed)."""
    available = movable()
    moved = read_json(home() / "servers.json", {})
    moved = moved if isinstance(moved, dict) else {}
    for name in names:
        if name not in available:
            report.append(f"{name}: not in the user scope of Claude Code, left as it is")
            continue
        moved[name] = {"config": available[name], "origin": "claude-code user", "moved": time.time()}
        write_json(home() / "servers.json", moved)
        code, out = run_claude(["mcp", "remove", name, "--scope", "user"], cwd)
        if code == 127:
            edit_json(claude_json(), lambda data, key=name: data.get("mcpServers", {}).pop(key, None))
        elif code != 0:
            moved.pop(name)
            write_json(home() / "servers.json", moved)
            report.append(f"{name}: could not be removed from Claude Code ({out[:120]}), left as it is")
            continue
        report.append(f"{name}: now behind codemode only (its tools leave the context of new sessions)")

def restore_servers(names: list[str] | None, cwd: str, report: list[str]) -> None:
    """Give servers back to the client they were taken from."""
    moved = read_json(home() / "servers.json", {})
    moved = moved if isinstance(moved, dict) else {}
    for name in list(moved):
        item = moved[name]
        if (names is not None and name not in names) or item.get("origin") not in ("claude-code user", "claude-desktop"):
            continue  # not asked for, or a server the user declared for codemode alone: it stays
        if item.get("origin") == "claude-desktop":
            edit_json(desktop_config(), lambda data, key=name, spec=item["config"]: data.setdefault("mcpServers", {}).update({key: spec}))
        else:
            code, out = run_claude(["mcp", "add-json", "--scope", "user", name, json.dumps(item["config"])], cwd)
            if code == 127:
                edit_json(claude_json(), lambda data, key=name, spec=item["config"]: data.setdefault("mcpServers", {}).update({key: spec}))
            elif code != 0:
                report.append(f"{name}: could not be given back to Claude Code ({out[:120]}); its config stays in {home() / 'servers.json'}")
                continue
        moved.pop(name)
        write_json(home() / "servers.json", moved)
        report.append(f"{name}: back in {item.get('origin', 'its client')}")

def desktop(enable: bool, move: list[str], report: list[str]) -> None:
    """Declare (or remove) codemode in Claude Desktop, optionally taking some of its servers behind it."""
    path = desktop_config()
    if not path.parent.is_dir():
        report.append(f"Claude Desktop: not found ({path.parent} does not exist), nothing changed")
        return
    moved = read_json(home() / "servers.json", {})
    moved = moved if isinstance(moved, dict) else {}

    def change(data: dict) -> None:
        servers = data.setdefault("mcpServers", {})
        if not enable:
            servers.pop("codemode", None)
            return
        command = script_command()
        servers["codemode"] = {"command": command[0], "args": [command[1], "serve"]}
        for name in move:
            if isinstance(servers.get(name), dict) and name != "codemode":
                moved[name] = {"config": servers.pop(name), "origin": "claude-desktop", "moved": time.time()}
                report.append(f"{name}: now behind codemode only in Claude Desktop")
    edit_json(path, change)
    write_json(home() / "servers.json", moved)
    report.append("Claude Desktop: codemode " + ("declared" if enable else "removed") + " - quit and restart the app to apply")

def priority(value: str, session: str, everywhere: bool) -> str:
    """Set the priority for this session only, or for every session (config.json)."""
    if value not in PRIORITIES:
        raise SetupError(f"priority {value!r}: tokens or cost")
    if everywhere:
        save_config({"priority": value})
        set_session_priority(session, None)
    else:
        set_session_priority(session, value)
    preset = ", ".join(f"{key} {number}" for key, number in PRESETS[value].items()) or "the default thresholds"
    where = "every session (config.json)" if everywhere else f"this session ({session})"
    return f"priority {value} for {where}: {RULES[value]} Hooks: {preset}."

def setup(scope: str, cwd: str, move: list[str], hooks: list[str] | None, sandbox_on: bool | None, with_desktop: bool,
          desktop_move: list[str], shared: bool, dry_run: bool, chosen: str | None = None, session: str = "") -> str:
    """Do (or only list, with dry_run) what the chosen options mean."""
    claude_scope = {"user": "user", "project": "project" if shared else "local"}.get(scope, "")
    plan: list[str] = []
    if scope == "session":
        plan.append("session: nothing to install - the skill drives codemode through its command line, the engine stops with the session")
    if claude_scope:
        where = "every project of this computer" if scope == "user" else f"{cwd} ({'shared in .mcp.json' if shared else 'only for you'})"
        plan.append(f"register the codemode MCP server in Claude Code for {where}")
    if move and scope != "user":
        raise SetupError("moving servers behind codemode needs --scope user: they would disappear from the other projects")
    plan += [f"move behind codemode: {', '.join(move)} (removed from Claude Code's own list, restored by uninstall)"] if move else []
    if hooks is not None and scope in ("user", "project"):
        plan.append(f"install the hook in {settings_path(scope, cwd)}: sizes" + "".join(f" + {flag}" for flag in hooks))
    if sandbox_on is not None:
        plan.append(f"sandbox of the code: {'on' if sandbox_on else 'off'} ({sandbox.describe()})")
    if chosen is not None:
        plan.append(f"priority: {chosen} for {'this session' if scope == 'session' else 'every session'}")
    if with_desktop:
        plan.append("declare codemode in Claude Desktop" + (f", move behind it: {', '.join(desktop_move)}" if desktop_move else ""))
    if dry_run:
        return "would do:\n" + "\n".join(f"- {line}" for line in plan)
    if sandbox_on and not sandbox.available():
        raise SetupError(f"the sandbox is {sandbox.describe()}")
    report: list[str] = []
    state = manifest()
    if claude_scope:
        register(claude_scope, cwd, report)
        if claude_scope == "user":
            state["scopes"]["user"] = True
        elif cwd not in state["scopes"][claude_scope]:
            state["scopes"][claude_scope].append(cwd)
    if move:
        move_servers(move, cwd, report)
    if hooks is not None and scope in ("user", "project"):
        path = settings_path(scope, cwd)
        set_hooks(path, hooks)
        state["hooks"] = sorted(set(state["hooks"]) | {str(path)})
        report.append(f"hook installed in {path} (sizes" + "".join(f", {flag}" for flag in hooks) + ")")
    if sandbox_on is not None:
        save_config({"sandbox": bool(sandbox_on)})
        report.append(f"sandbox of the code: {'on' if sandbox_on else 'off'}")
    if chosen is not None:
        report.append(priority(chosen, session, scope != "session"))
    if with_desktop:
        desktop(True, desktop_move, report)
        state["desktop"] = True
    write_json(home() / "install.json", state)
    if claude_scope or move:
        report.append("Claude Code sessions already open keep their tools until they restart; new sessions get codemode")
    return "\n".join(report) if report else "nothing to do"

def uninstall(scope: str, cwd: str, purge: bool) -> str:
    """Remove codemode from a scope (or everywhere) and give the moved servers back."""
    report: list[str] = []
    state = manifest()
    everything = scope == "all"
    if everything or scope == "user":
        restore_servers(None, cwd, report)
        if state["scopes"]["user"] or "codemode" in (read_json(claude_json(), {}) or {}).get("mcpServers", {}):
            unregister("user", cwd, report)
        state["scopes"]["user"] = False
        user_settings = settings_path("user", cwd)
        set_hooks(user_settings, None)
        state["hooks"] = [path for path in state["hooks"] if path != str(user_settings)]
    for kind in ("local", "project"):
        for folder in list(state["scopes"][kind]):
            if everything or (scope == "project" and folder == cwd):
                unregister(kind, folder, report)
                state["scopes"][kind].remove(folder)
                set_hooks(settings_path("project", folder), None)
                state["hooks"] = [path for path in state["hooks"] if path != str(settings_path("project", folder))]
    if everything:
        for path in list(state["hooks"]):
            set_hooks(Path(path), None)
        state["hooks"] = []
        if state["desktop"]:
            desktop(False, [], report)
            state["desktop"] = False
    write_json(home() / "install.json", state)
    drop_backups([claude_json(), settings_path("user", cwd), settings_path("project", cwd), Path(cwd) / ".mcp.json", desktop_config()])
    if purge and everything:
        left = read_json(home() / "servers.json", {})
        if left:
            report.append(f"data kept: {home()} still holds the config of servers that could not be given back")
        else:
            shutil.rmtree(home(), ignore_errors=True)
            report.append(f"data removed: {home()}")
    return "\n".join(report) if report else "codemode was not installed there"

def status(cwd: str, session: str = "") -> str:
    """What is installed where, and what runs."""
    config = load_config(session)
    claude = read_json(claude_json(), {})
    claude = claude if isinstance(claude, dict) else {}
    local = ((claude.get("projects") or {}).get(cwd) or {}).get("mcpServers") or {}
    shared = read_json(Path(cwd) / ".mcp.json", {})
    shared = (shared.get("mcpServers") if isinstance(shared, dict) else {}) or {}
    moved = read_json(home() / "servers.json", {})
    moved = moved if isinstance(moved, dict) else {}
    desk = read_json(desktop_config(), None)
    places = [name for name, yes in (("this computer (user)", "codemode" in (claude.get("mcpServers") or {})), ("this project (local)", "codemode" in local),
                                     ("this project (.mcp.json)", "codemode" in shared)) if yes]
    hooks = [f"{path}: {flags}" for path in (settings_path("user", cwd), settings_path("project", cwd)) for flags in [installed_hooks(path)] if flags]
    lines = [f"codemode {VERSION} | data: {home()} | python: {sys.executable}",
             f"Claude Code: {', '.join(places) or 'not registered (the skill can still use it for a session)'}",
             f"hooks: {'; '.join(hooks) or 'none installed'}",
             "moved behind codemode: " + (", ".join(f"{name} (from {item.get('origin')})" for name, item in sorted(moved.items())) or "none"),
             "Claude Desktop: " + ("not found" if desk is None else "codemode declared" if "codemode" in ((desk or {}).get("mcpServers") or {}) else "found, codemode not declared"),
             f"sandbox: {'on' if config.get('sandbox') else 'off'} - {sandbox.describe()}",
             "priority: {0} ({1})".format(*priority_of(session))]
    running = engines()
    lines.append("engines running: " + ("; ".join(f"{item['session']} ({item['kind']}, pid {item['pid']})" for item in running) or "none"))
    return "\n".join(lines)

def doctor(cwd: str) -> str:
    """Check what codemode needs and try every declared server once."""
    config = load_config()
    lines = [f"python {sys.version.split()[0]}: {'ok' if sys.version_info >= (3, 9) else 'too old (3.9 or newer needed)'}",
             f"data folder {home()}: {'writable' if os.access(home(), os.W_OK) else 'NOT writable'}",
             f"claude command: {shutil.which('claude') or 'not found (setup edits the config files directly)'}",
             f"sandbox: {'on' if config.get('sandbox') else 'off'} - {sandbox.describe()}"]
    hub = Hub(cwd, config, "doctor")
    started = time.monotonic()
    tools, errors = hub.index(refresh=True)
    for name in sorted(hub.servers()):
        source = hub.servers()[name]["source"]
        lines.append(f"{'ok  ' if name in tools else 'FAIL'} {name} [{source}]: " + (f"{len(tools[name])} tools" if name in tools else errors.get(name, "?")[:200]))
    lines += [f"note: {note}" for note in hub.notes]
    lines.append(f"{len(tools)} of {len(hub.servers())} servers answered in {time.monotonic() - started:.1f}s")
    hub.close()
    return "\n".join(lines)
