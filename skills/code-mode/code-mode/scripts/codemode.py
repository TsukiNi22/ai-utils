#!/usr/bin/env python3
"""Code mode for the local MCP servers: drive them with code, so only what the code prints enters the context.

    codemode.py search [QUERY] [--server S] [--detail] [--refresh]    tools (and recipes): words or a regex; empty = the servers
    codemode.py call SERVER.TOOL [name=value ...] [--json ARGS] [--pick PATHS] [--write]
    codemode.py run [FILE | -] [--write] [--fresh] [--wait N] [--args JSON] [--recipe NAME] [--save NAME --desc TEXT] [--standalone]
    codemode.py job [ID] [--wait N] [--kill]           what a background run printed since, wait for it, kill it; no id = the list
    codemode.py recipe [list | show NAME | rm NAME | prune]
    codemode.py stats [--all]                          what code mode kept out of the context (this session / everything)
    codemode.py status | doctor | off                  what is installed and running / check every server / stop this session's engine
    codemode.py priority [tokens | cost] [--global]    what to save first: tokens (subscription) or $ (API); this session or all
    codemode.py setup --scope session|project|user [--shared] [--move a,b] [--hooks remind,compress|none] [--sandbox on|off]
                      [--priority tokens|cost] [--desktop] [--desktop-move a,b] [--dry-run]
    codemode.py uninstall [--scope project|user|all] [--purge]
    codemode.py serve                                  the MCP server (stdio) a client starts: tools search, call, execute, job
    codemode.py hook [remind] [compress]               the Claude Code PostToolUse hook (event JSON on stdin)

The servers are the ones already declared: Claude Code (user, local, approved .mcp.json), Claude Desktop, and the ones
`setup --move` took behind codemode. In a script: call(server, tool, **args), pmap(server, tool, items, key=None),
ToolError, ok(results), failed(results), args; variables persist between runs of one session (--fresh: a clean separate
process, --standalone: no shared engine at all, for cron and CI). A tool that changes things needs --write.
The session is --session, else $CODEMODE_SESSION, else $CLAUDE_CODE_SESSION_ID; data in $CODEMODE_HOME (default: the user
data folder of the system). Standard library only, Python 3.9+, Linux / macOS / Windows.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

def parse_value(text: str) -> object:
    """A command line value: JSON when it is JSON (numbers, true, lists), the text itself otherwise."""
    try:
        return json.loads(text)
    except ValueError:
        return text

def read_code(source: str | None) -> str:
    if source in (None, "-"):
        return sys.stdin.read()
    with open(source, "r", encoding="utf-8") as file:
        return file.read()

def parser() -> argparse.ArgumentParser:
    top = argparse.ArgumentParser(prog="codemode.py", description="Code mode for the local MCP servers", epilog="Full help: the top of this file")
    top.add_argument("--session", help="the session this command belongs to (default: the Claude Code session)")
    sub = top.add_subparsers(dest="command", required=True)
    search = sub.add_parser("search", help="find tools and recipes")
    search.add_argument("query", nargs="?", default="")
    search.add_argument("--server", default="")
    search.add_argument("--detail", action="store_true")
    search.add_argument("--refresh", action="store_true", help="ask every server for its tools again")
    call = sub.add_parser("call", help="one tool call, its result compact")
    call.add_argument("target", help="server.tool")
    call.add_argument("values", nargs="*", help="name=value arguments")
    call.add_argument("--json", dest="json_args", default="", help="the arguments as one JSON object")
    call.add_argument("--pick", default="")
    call.add_argument("--write", action="store_true")
    run = sub.add_parser("run", help="run a script against the servers")
    run.add_argument("file", nargs="?", help="a file, or - for stdin (default)")
    run.add_argument("--write", action="store_true")
    run.add_argument("--fresh", action="store_true")
    run.add_argument("--standalone", action="store_true", help="no shared engine: servers started and stopped by this command")
    run.add_argument("--wait", type=int)
    run.add_argument("--args", default="", help="JSON object the code reads as args")
    run.add_argument("--recipe", default="")
    run.add_argument("--save", default="", help="keep the script as a recipe under this name")
    run.add_argument("--desc", default="")
    job = sub.add_parser("job", help="follow a background run")
    job.add_argument("id", nargs="?", default="")
    job.add_argument("--wait", type=int)
    job.add_argument("--kill", action="store_true")
    recipe = sub.add_parser("recipe", help="the saved recipes")
    recipe.add_argument("action", nargs="?", default="list", choices=["list", "show", "rm", "prune"])
    recipe.add_argument("name", nargs="?", default="")
    stats = sub.add_parser("stats", help="what code mode saved")
    stats.add_argument("--all", action="store_true")
    for name, text in (("status", "what is installed and running"), ("doctor", "check every server"), ("off", "stop the engine of this session")):
        sub.add_parser(name, help=text)
    setup = sub.add_parser("setup", help="install for a project or the whole computer")
    setup.add_argument("--scope", default="session", choices=["session", "project", "user"])
    setup.add_argument("--shared", action="store_true", help="project scope: write .mcp.json (shared) instead of the private local scope")
    setup.add_argument("--move", default="", help="user scope servers to put behind codemode (a,b or all)")
    setup.add_argument("--hooks", default=None, help="remind,compress | remind | compress | none")
    setup.add_argument("--sandbox", default=None, choices=["on", "off"])
    setup.add_argument("--priority", default=None, choices=["tokens", "cost"], help="tokens (subscription) or cost ($, API)")
    priority = sub.add_parser("priority", help="tokens (subscription) or cost ($, API) first")
    priority.add_argument("value", nargs="?", default="", choices=["", "tokens", "cost"])
    priority.add_argument("--global", dest="everywhere", action="store_true", help="every session (config.json), not only this one")
    setup.add_argument("--desktop", action="store_true")
    setup.add_argument("--desktop-move", default="")
    setup.add_argument("--dry-run", action="store_true")
    uninstall = sub.add_parser("uninstall", help="remove what setup installed")
    uninstall.add_argument("--scope", default="all", choices=["project", "user", "all"])
    uninstall.add_argument("--purge", action="store_true", help="also delete the codemode data folder")
    sub.add_parser("serve", help="the MCP server (stdio)")
    daemon = sub.add_parser("daemon", help=argparse.SUPPRESS)
    daemon.add_argument("--cwd", default="")
    daemon.add_argument("--owner", type=int, default=0)
    return top

def names(text: str) -> list[str]:
    return [name.strip() for name in text.split(",") if name.strip()]

def install(args: argparse.Namespace, cwd: str, key: str) -> str:
    from cm import setup  # only this command needs it
    if args.command == "uninstall":
        return setup.uninstall(args.scope, cwd, args.purge)
    move = sorted(setup.movable()) if args.move == "all" else names(args.move)
    hooks = None if args.hooks is None else [flag for flag in names(args.hooks) if flag in setup.HOOK_FLAGS]
    sandbox_on = None if args.sandbox is None else args.sandbox == "on"
    return setup.setup(args.scope, cwd, move, hooks, sandbox_on, args.desktop, names(args.desktop_move), args.shared, args.dry_run,
                       args.priority, key)

def request_of(args: argparse.Namespace) -> tuple[str, dict]:
    """The engine request a command line means."""
    if args.command == "search":
        return "search", {"query": args.query, "server": args.server, "detail": args.detail, "refresh": args.refresh}
    if args.command == "call":
        server, _, tool = args.target.partition(".")
        values = json.loads(args.json_args) if args.json_args else {}
        for pair in args.values:
            name, _, value = pair.partition("=")
            values[name] = parse_value(value)
        return "call", {"server": server, "tool": tool, "args": values, "pick": args.pick, "allow_write": args.write}
    if args.command == "run":
        code = "" if args.recipe else read_code(args.file)
        return "execute", {"code": code, "recipe": args.recipe, "args": json.loads(args.args) if args.args else {}, "allow_write": args.write,
                           "fresh": args.fresh, "wait": args.wait, "save_as": args.save, "description": args.desc}
    if args.command == "job":
        return "job", {"id": args.id, "action": "kill" if args.kill else "wait" if args.wait else "status", "wait": args.wait}
    if args.command == "recipe":
        return "recipe", {"action": args.action, "name": args.name}
    return "stats", {"all": args.all}

def main(argv: list[str]) -> int:
    if argv[1:2] == ["hook"]:
        from cm import hooks  # the fast path: called at every direct MCP tool call
        return hooks.run(argv[2:])
    if argv[1:2] == ["worker"]:
        from cm import worker
        return worker.main()
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    args = parser().parse_args(argv[1:])
    from cm import daemon, paths
    from cm.engine import Engine
    key = args.session or paths.session_key()
    cwd = os.environ.get("CLAUDE_PROJECT_DIR") if args.command == "serve" and os.environ.get("CLAUDE_PROJECT_DIR") else os.getcwd()
    if args.command == "serve":
        from cm.server import Server
        engine = Engine(cwd, key if key != "default" else f"mcp-{os.getpid()}")
        control = daemon.Control(engine, engine.session, "mcp")
        control.start()
        try:
            return Server(engine).serve()
        finally:
            control.close()
    if args.command == "daemon":
        return daemon.run_daemon(key, args.cwd or cwd, args.owner)
    try:
        if args.command in ("setup", "uninstall"):
            sys.stdout.write(install(args, cwd, key) + "\n")
            return 0
        if args.command == "priority":
            from cm import setup
            if args.value:
                sys.stdout.write(setup.priority(args.value, key, args.everywhere) + "\n")
            else:
                value, origin = paths.priority_of(key)
                sys.stdout.write(f"priority: {value} ({origin}) - {paths.RULES[value]}\n")
            return 0
        if args.command == "doctor":
            from cm import setup
            sys.stdout.write(setup.doctor(cwd) + "\n")
            return 0
        if args.command == "status":
            from cm import setup
            sys.stdout.write(setup.status(cwd, key) + "\n")
            if daemon.locate(key) is not None:
                sys.stdout.write(daemon.request(key, "status", {}, cwd, autostart=False)["text"] + "\n")
            return 0
        if args.command == "off":
            if daemon.locate(key) is None:
                sys.stdout.write("no codemode engine runs for this session\n")
                return 0
            sys.stdout.write(daemon.request(key, "stop", {}, cwd, autostart=False)["text"] + "\n")
            return 0
        op, payload = request_of(args)
        if args.command == "run" and args.standalone:
            engine = Engine(cwd, f"standalone-{os.getpid()}")
            try:
                payload["wait"] = int(engine.config["job_timeout_s"])
                answer = engine.handle(op, payload)
            finally:
                engine.close()
        else:
            answer = daemon.request(key, op, payload, cwd)
    except (daemon.NoEngine, OSError, ValueError) as error:
        sys.stderr.write(f"codemode: {error}\n")
        return 2
    except Exception as error:  # setup errors and the like: one clear line, no traceback for the model to read
        if type(error).__name__ != "SetupError":
            raise
        sys.stderr.write(f"codemode: {error}\n")
        return 2
    sys.stdout.write(answer["text"] + "\n")
    return 1 if answer.get("is_error") else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
