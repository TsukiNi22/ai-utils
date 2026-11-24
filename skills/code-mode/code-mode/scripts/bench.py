#!/usr/bin/env python3
"""Bench: the same task done with direct MCP calls and through codemode, in tokens added to the context and model turns.

Usage: python3 bench.py [--set generic|blender|roblox|all] [--media DIR] [--write]

No model is involved: each task is scripted twice (the calls a model would send one by one, and the one script of code
mode), run for real on the servers declared on this computer, and what each way puts in the context is counted:
- direct: the definitions of the tools used (as the client loads them), every call and every whole result;
- code mode: the four codemode tool definitions, the search lines, the script and what it prints.
Tokens are estimated the same way on both sides: characters / 3.3 (measured on JSON results with tiktoken o200k).

Sets: generic (--media DIR: a folder of videos and a server with get_media_info, ex: ffmpeg), blender (Blender open,
its MCP addon connected), roblox (Roblox Studio open with its MCP server). The blender and roblox sets call tools that
run code inside the application: they need --write, work in a `codemode_bench` collection / folder they remove at the
end, and compare the naive way (one call per object) with what the profile asks for (one script, a summary printed).
The roblox set was written from the documentation of the servers and not run by its author: a task whose tools or
arguments differ on your version is reported as skipped with the reason.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cm.engine import Engine  # noqa: E402 (the path above comes first)
from cm.hub import CODE_ARGS, ToolFailure  # noqa: E402
from cm.paths import CHARS_PER_TOKEN, load_config  # noqa: E402
from cm.server import TOOLS  # noqa: E402

class Skip(Exception):
    """A task that cannot run here (a missing tool, an application that is closed)."""

def tokens(text: object) -> int:
    text = text if isinstance(text, str) else json.dumps(text, ensure_ascii=False, default=str)
    return int(len(text) / CHARS_PER_TOKEN + 0.999)

class Side:
    """What one way of doing a task puts in the context."""

    def __init__(self) -> None:
        self.tokens = 0
        self.turns = 0
        self.calls = 0

class Context:
    """The servers of this computer and the two ways of using them."""

    def __init__(self, engine: Engine, write: bool, media: str) -> None:
        self.engine = engine
        self.hub = engine.hub
        self.write = write
        self.media = media
        self.tools, self.errors = self.hub.index()
        self.direct = Side()
        self.code = Side()
        self._loaded: set[str] = set()

    def server_with(self, *names: str) -> str:
        """The first server that has one of these tools."""
        for server, tools in sorted(self.tools.items()):
            if any(name in tools for name in names):
                return server
        raise Skip(f"no declared server has {' / '.join(names)}")

    def spec(self, server: str, tool: str) -> dict:
        if tool not in self.tools.get(server, {}):
            raise Skip(f"{server} has no tool {tool}")
        return self.tools[server][tool]

    def reset(self) -> None:
        self.direct, self.code, self._loaded = Side(), Side(), set()

    def call(self, server: str, tool: str, **args: object) -> object:
        """A direct call: its tool definition (once), its input and its whole result enter the context."""
        spec = self.spec(server, tool)
        name = f"mcp__{server}__{tool}"
        if name not in self._loaded:
            self._loaded.add(name)
            self.direct.tokens += tokens({"description": spec.get("description", ""), "name": name, "parameters": spec.get("inputSchema", {})})
        try:
            outcome = self.hub.call(server, tool, args, allow_write=self.write)
        except ToolFailure as failure:
            if failure.kind == "denied":
                raise Skip(f"{server}.{tool} changes things: run the bench with --write") from failure
            raise Skip(f"{server}.{tool} failed: {failure.message[:160]}") from failure
        self.direct.tokens += tokens(name) + tokens(args) + int(outcome.raw_chars / CHARS_PER_TOKEN + 0.999)
        self.direct.calls += 1
        return outcome.data

    def turn(self, count: int = 1) -> None:
        """Model turns of the direct way (a turn may hold several independent calls)."""
        self.direct.turns += count

    def search(self, query: str, server: str) -> None:
        """Code mode: the search that finds the tools (one turn)."""
        text = self.engine.search(query, server)
        self.code.tokens += tokens({"query": query, "server": server}) + tokens(text)
        self.code.turns += 1

    def execute(self, code: str) -> str:
        """Code mode: one script (one turn); its source and what it prints enter the context."""
        answer = self.engine.handle("execute", {"code": code, "allow_write": self.write, "wait": 600})
        if answer["is_error"]:
            raise Skip(f"the script failed: {answer['text'][-300:]}")
        self.code.tokens += tokens(code) + tokens(answer["text"])
        self.code.turns += 1
        return answer["text"]

    def quiet(self, server: str, tool: str, **args: object) -> object:
        """A call that is part of the preparation or the cleanup: not counted."""
        try:
            return self.hub.call(server, tool, args, allow_write=self.write).data
        except ToolFailure as failure:
            raise Skip(f"{server}.{tool}: {failure.message[:160]}") from failure

    def code_args(self, server: str, tool: str, source: str) -> dict:
        """The arguments of a tool that runs code, read from its own schema (the name of the code argument varies)."""
        properties = (self.spec(server, tool).get("inputSchema") or {}).get("properties") or {}
        name = next((candidate for candidate in CODE_ARGS if candidate in properties), None)
        if name is None:
            raise Skip(f"{server}.{tool}: no code argument found in {list(properties)}")
        args: dict = {name: source}
        if "datamodel_type" in properties:
            args["datamodel_type"] = "Edit"
        if "studio_id" in properties:
            args["studio_id"] = self.studio(server)
        return args

    def studio(self, server: str) -> object:
        """The id of the first open Roblox Studio (the built-in server wants it on every call)."""
        if "_studio" not in self.__dict__:
            listed = self.quiet(server, "list_roblox_studios")
            found = re.search(r'"?(?:studio_?id|id)"?\s*[:=]\s*"?([\w-]+)', listed if isinstance(listed, str) else json.dumps(listed), re.I)
            if not found:
                raise Skip(f"no Studio id in the answer of list_roblox_studios: {str(listed)[:120]}")
            self._studio = int(found.group(1)) if found.group(1).isdigit() else found.group(1)
        return self._studio

def generic_inventory(ctx: Context) -> None:
    """Media inventory: one get_media_info per file against one script printing a table."""
    server = ctx.server_with("get_media_info")
    files = sorted(str(path) for path in Path(ctx.media).iterdir() if path.suffix.lower() in (".mp4", ".mkv", ".webm", ".mov", ".avi"))[:40] if ctx.media else []
    if not files:
        raise Skip("give a folder of videos with --media DIR")
    for path in files:
        ctx.call(server, "get_media_info", file_path=path)
    ctx.turn(2)  # the tool definitions, then the calls together
    ctx.search("media_info", server)
    ctx.execute(f"""import os
files = {files!r}
infos = pmap({server!r}, "get_media_info", [{{"file_path": f}} for f in files])
for f, i in zip(files, infos):
    if isinstance(i, ToolError):
        print(os.path.basename(f), "FAILED", i.kind); continue
    v = next((s for s in i["streams"] if s.get("codec_type") == "video"), {{}})
    print(os.path.basename(f), v.get("codec_name"), f"{{v.get('width')}}x{{v.get('height')}}", i["format"]["duration"])
print(len(ok(infos)), "files,", round(sum(float(i["format"]["duration"]) for i in ok(infos)), 1), "s")""")

def generic_single(ctx: Context) -> None:
    """The control case: one small call, where code mode must lose."""
    server = ctx.server_with("get_duration")
    files = sorted(str(path) for path in Path(ctx.media).iterdir() if path.is_file())[:1] if ctx.media else []
    if not files:
        raise Skip("give a folder of videos with --media DIR")
    ctx.call(server, "get_duration", file_path=files[0])
    ctx.turn(2)
    ctx.search("get_duration", server)
    ctx.execute(f'print(call({server!r}, "get_duration", file_path={files[0]!r}))')

BLENDER_SETUP = """import bpy
old = bpy.data.collections.get("codemode_bench")
if old:
    for o in list(old.objects): bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.collections.remove(old)
col = bpy.data.collections.new("codemode_bench"); bpy.context.scene.collection.children.link(col)
for i in range(30):
    mesh = bpy.data.meshes.new(f"cmb_mesh_{i}"); mesh.from_pydata([(0,0,0),(1,0,0),(1,1,0),(0,1,0)], [], [(0,1,2,3)])
    obj = bpy.data.objects.new(f"cmb_{i:02d}", mesh); obj.location = (i % 6 * 2, i // 6 * 2, 0); col.objects.link(obj)
print("ready", len(col.objects))"""
BLENDER_CLEAN = """import bpy
col = bpy.data.collections.get("codemode_bench")
if col:
    for o in list(col.objects): bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.collections.remove(col)
print("cleaned")"""

def blender_scene(ctx: Context) -> None:
    """What is in the scene: the whole listing against one script printing counts and the biggest objects."""
    server = ctx.server_with("execute_blender_code")
    ctx.call(server, "get_scene_info", limit=500)
    ctx.turn(2)
    ctx.search("scene|execute", server)
    script = ("import bpy, collections\\nobjs = bpy.context.scene.objects\\nprint(len(objs), 'objects', dict(collections.Counter(o.type for o in objs)))\\n"
              "big = sorted(objs, key=lambda o: -max(o.dimensions))[:5]\\nprint([(o.name, round(max(o.dimensions), 2)) for o in big])")
    ctx.execute(f'print(call({server!r}, "execute_blender_code", code="{script}"))')

def blender_objects(ctx: Context) -> None:
    """Thirty objects moved and renamed: one execute_blender_code per object against one script for all of them."""
    server = ctx.server_with("execute_blender_code")
    ctx.quiet(server, "execute_blender_code", code=BLENDER_SETUP)
    try:
        ctx.call(server, "get_scene_info", query="cmb_", limit=50)
        ctx.turn(2)
        for number in range(30):
            ctx.call(server, "execute_blender_code", code=f"import bpy\no = bpy.data.objects['cmb_{number:02d}']\no.location.z = 1.0\no.name = 'cmbd_{number:02d}'\nprint(o.name, tuple(round(v, 2) for v in o.location))")
            ctx.turn()
        ctx.quiet(server, "execute_blender_code", code=BLENDER_SETUP)
        ctx.search("execute_blender_code", server)
        inner = ("import bpy\\ncol = bpy.data.collections['codemode_bench']\\nfor i, o in enumerate(sorted(col.objects, key=lambda o: o.name)):\\n"
                 "    o.location.z = 1.0; o.name = f'cmbc_{i:02d}'\\nprint(len(col.objects), 'objects moved to z=1 and renamed cmbc_00..')")
        ctx.execute(f'print(call({server!r}, "execute_blender_code", code="{inner}"))')
    finally:
        ctx.quiet(server, "execute_blender_code", code=BLENDER_CLEAN)

def roblox_code_tool(ctx: Context) -> tuple[str, str]:
    server = ctx.server_with("execute_luau", "run_code")
    return server, "execute_luau" if "execute_luau" in ctx.tools[server] else "run_code"

def roblox_classes(ctx: Context) -> None:
    """Instances by class: the tree (or every descendant) against one Luau script returning counts."""
    server, tool = roblox_code_tool(ctx)
    if "search_game_tree" in ctx.tools[server]:
        properties = (ctx.spec(server, "search_game_tree").get("inputSchema") or {}).get("properties") or {}
        ctx.call(server, "search_game_tree", **({"studio_id": ctx.studio(server)} if "studio_id" in properties else {}))
    else:
        ctx.call(server, tool, **ctx.code_args(server, tool, "for _, d in ipairs(workspace:GetDescendants()) do print(d.ClassName, d:GetFullName()) end"))
    ctx.turn(2)
    ctx.search("luau|run_code|game_tree", server)
    luau = ("local n = {} for _, d in ipairs(workspace:GetDescendants()) do n[d.ClassName] = (n[d.ClassName] or 0) + 1 end "
            "local t = {} for k, v in pairs(n) do table.insert(t, {k, v}) end table.sort(t, function(a, b) return a[2] > b[2] end) "
            "local out = {} for i = 1, math.min(15, #t) do out[i] = t[i][1] .. '=' .. t[i][2] end print(#workspace:GetDescendants(), table.concat(out, ' ')) "
            "return table.concat(out, ' ')")
    ctx.execute(f"print(str(call({server!r}, {tool!r}, **{ctx.code_args(server, tool, luau)!r}))[:600])")

def roblox_console(ctx: Context) -> None:
    """Errors of the output log: the whole log against the few lines that matter."""
    server = ctx.server_with("get_console_output")
    extra = {"studio_id": ctx.studio(server)} if "studio_id" in ((ctx.spec(server, "get_console_output").get("inputSchema") or {}).get("properties") or {}) else {}
    ctx.call(server, "get_console_output", **extra)
    ctx.turn(2)
    ctx.search("console", server)
    ctx.execute(f"""log = call({server!r}, "get_console_output", **{extra!r})
lines = (log if isinstance(log, str) else json.dumps(log)).splitlines()
bad = [l for l in lines if "error" in l.lower() or "warn" in l.lower()]
print(len(lines), "lines,", len(bad), "errors / warnings")
for l in bad[-10:]:
    print(l[:160])""")

def roblox_scripts(ctx: Context) -> None:
    """Scripts that use a service: grep then every whole script against grep and the matching lines only."""
    server = ctx.server_with("script_grep")
    def args_of(tool: str, value: str) -> dict:
        schema = ctx.spec(server, tool).get("inputSchema") or {}
        properties = schema.get("properties") or {}
        name = next((key for key in schema.get("required") or list(properties) if key not in ("studio_id", "datamodel_type")), None)
        if name is None:
            raise Skip(f"{server}.{tool}: no argument found for its value")
        return {name: value, **({"studio_id": ctx.studio(server)} if "studio_id" in properties else {})}
    found = ctx.call(server, "script_grep", **args_of("script_grep", "GetService"))
    paths = sorted(set(re.findall(r"game(?:\.[A-Za-z0-9_]+)+", found if isinstance(found, str) else json.dumps(found))))[:12]
    if not paths:
        raise Skip("script_grep found no script path (an empty place?)")
    ctx.turn(2)
    for path in paths:
        ctx.call(server, "script_read", **args_of("script_read", path))
    ctx.turn()
    ctx.search("script_grep|script_read", server)
    ctx.execute(f"""found = call({server!r}, "script_grep", **{args_of("script_grep", "GetService")!r})
text = found if isinstance(found, str) else json.dumps(found)
import re, collections
paths = re.findall(r"game(?:\\.[A-Za-z0-9_]+)+", text)
print(len(set(paths)), "scripts use GetService:", dict(collections.Counter(paths).most_common(12)))""")

SETS = {"generic": [generic_inventory, generic_single], "blender": [blender_scene, blender_objects],
        "roblox": [roblox_classes, roblox_console, roblox_scripts]}

def main() -> int:
    parser = argparse.ArgumentParser(description="Direct MCP calls against code mode, in tokens and turns")
    parser.add_argument("--set", default="all", choices=[*SETS, "all"])
    parser.add_argument("--media", default="", help="a folder of videos for the generic set")
    parser.add_argument("--write", action="store_true", help="allow the tools that change things (needed by the blender and roblox sets)")
    args = parser.parse_args()
    engine = Engine(os.getcwd(), f"bench-{os.getpid()}", load_config())
    ctx = Context(engine, args.write, args.media)
    fixed = tokens(TOOLS)
    rows = []
    try:
        for name in SETS if args.set == "all" else [args.set]:
            for task in SETS[name]:
                ctx.reset()
                label = f"{name}: {(task.__doc__ or task.__name__).split(':')[0].strip().lower()}"
                try:
                    task(ctx)
                except Skip as skipped:
                    rows.append(f"| {label} | skipped: {str(skipped)[:150]} | | | | |")
                    continue
                code = ctx.code.tokens + fixed
                gain = (code - ctx.direct.tokens) * 100 / max(1, ctx.direct.tokens)
                rows.append(f"| {label} | {ctx.direct.tokens:,} | {ctx.direct.turns} | {code:,} | {ctx.code.turns} | {gain:+.0f} % |")
    finally:
        engine.close()
    sys.stdout.write("| Task | Direct: tokens | turns | Code mode: tokens | turns | Difference |\n|---|---:|---:|---:|---:|---:|\n" + "\n".join(rows) + "\n")
    sys.stdout.write(f"\nTokens: characters / {CHARS_PER_TOKEN} on both sides. Code mode includes its 4 tool definitions ({fixed:,} tokens), "
                     "the search and the script.\n")
    sys.stdout.write("Servers that did not answer: " + (", ".join(f"{name} ({text[:60]})" for name, text in sorted(ctx.errors.items())) or "none") + "\n")
    return 0

if __name__ == "__main__":
    sys.exit(main())
