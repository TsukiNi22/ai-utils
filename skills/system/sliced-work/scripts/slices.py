#!/usr/bin/env python3
"""
Run a long job (render, generation, conversion, batch) slice by slice, resumable after a crash or a stop, then merge
the slices into the final result.

Usage:
    slices.py init DIR --cmd CMD (--range A-B --size N | --items FILE | --tiles CxR --image WxH)
                       [--merge frames|cat|concat|tiles|none|"<command>"] [--final PATH] [--max SIZE]
    slices.py run DIR [--retries N] [--only ID...]
    slices.py status DIR
    slices.py merge DIR
    slices.py reset DIR [ID...]

- init: writes DIR/slices.json, the plan; nothing runs. One slice per range of N items (--range 1-240 --size 10:
  frames 1-10, 11-20...), per line of a file (--items prompts.txt), or per tile of an image (--tiles 4x4 --image
  1920x1080). The command is a template: {id} {start} {end} {item} {out} {dir}, and for tiles {col} {row} {cols}
  {rows} {x} {y} {w} {h} (pixels, from the top left).
- run: the pending slices in order, each one writing into DIR/parts/<id>.partial/ ({out}), renamed DIR/parts/<id>/
  only when the command exits 0 and wrote something: a slice is done or not, never half done. A .partial left by a
  crash is deleted and its slice runs again. --max runs each slice through memory-guard (memguard.py run --max).
  Ctrl+C / a kill / a crash -> run again: the done slices are skipped.
- status: done / pending / failed, the time per slice and what is left.
- merge: once every slice is done - frames: the files of every part into --final (a folder), names kept, a name met
  twice is an error; cat: the files of the parts concatenated in order into --final; concat: the videos of the
  parts joined without re-encoding (ffmpeg concat, -c copy); tiles: the images placed at their x / y into --final
  (ImageMagick, the depth and format of the tiles kept); a command: run with {parts} (the part folders in order)
  and {final}.
- reset: slices back to pending (all, or the ids given), their parts deleted.
"""

import argparse
import json
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path

STATE = "slices.json"
MEMGUARD = Path(__file__).resolve().parents[2] / "memory-guard" / "scripts" / "memguard.py"

def load(folder: Path) -> dict:
    path = folder / STATE
    if not path.is_file():
        raise SystemExit(f"slices: no {STATE} in {folder} (slices.py init first)")
    return json.loads(path.read_text(encoding="utf-8"))

def save(folder: Path, state: dict) -> None:
    temp = folder / (STATE + ".tmp")
    temp.write_text(json.dumps(state, indent=1), encoding="utf-8")
    temp.replace(folder / STATE)

def plan(args: argparse.Namespace) -> list[dict]:
    if args.range:
        start, _, end = args.range.partition("-")
        first, last = int(start), int(end or start)
        return [{"start": a, "end": min(a + args.size - 1, last)} for a in range(first, last + 1, args.size)]
    if args.items:
        lines = [line for line in Path(args.items).read_text(encoding="utf-8").splitlines() if line.strip()]
        return [{"start": n, "end": n, "item": line} for n, line in enumerate(lines, 1)]
    cols, rows = (int(v) for v in args.tiles.lower().split("x"))
    width, height = (int(v) for v in args.image.lower().split("x"))
    slices = []
    for row in range(rows):
        for col in range(cols):
            x, y = width * col // cols, height * row // rows
            slices.append({"col": col, "row": row, "cols": cols, "rows": rows, "x": x, "y": y,
                           "w": width * (col + 1) // cols - x, "h": height * (row + 1) // rows - y,
                           "start": 0, "end": 0})
    return slices

def cmd_init(args: argparse.Namespace) -> int:
    folder = Path(args.dir).resolve()
    if (folder / STATE).exists() and not args.force:
        raise SystemExit(f"slices: {folder / STATE} exists (run / status, or init --force to start over)")
    if sum(bool(v) for v in (args.range, args.items, args.tiles)) != 1 or (args.tiles and not args.image):
        raise SystemExit("slices: one of --range A-B, --items FILE or --tiles CxR --image WxH")
    folder.mkdir(parents=True, exist_ok=True)
    slices = plan(args)
    for number, entry in enumerate(slices, 1):
        entry.update(id=f"{number:04d}", state="pending", seconds=0.0, tries=0)
    state = {"cmd": args.cmd, "merge": args.merge, "final": args.final, "max": args.max, "slices": slices}
    save(folder, state)
    print(f"slices: {len(slices)} slices planned in {folder / STATE}")
    return 0

def fill(template: str, entry: dict, folder: Path, out: Path) -> str:
    values = {key: value for key, value in entry.items() if key not in ("state", "seconds", "tries")}
    values.update(out=str(out), dir=str(folder))
    values = {key: shlex.quote(str(value)) if key in ("item", "out", "dir") else value for key, value in values.items()}
    return template.format(**values)

def cmd_run(args: argparse.Namespace) -> int:
    folder = Path(args.dir).resolve()
    state = load(folder)
    parts = folder / "parts"
    parts.mkdir(exist_ok=True)
    for stale in parts.glob("*.partial"):
        shutil.rmtree(stale)
    todo = [e for e in state["slices"] if e["state"] != "done" and (not args.only or e["id"] in args.only)]
    for entry in todo:
        out = parts / f"{entry['id']}.partial"
        out.mkdir()
        command = fill(state["cmd"], entry, folder, out)
        wrapped = ["bash", "-c", command]
        if state.get("max") and MEMGUARD.is_file():
            wrapped = [sys.executable, str(MEMGUARD), "run", "--max", state["max"], "--"] + wrapped
        for attempt in range(args.retries + 1):
            began = time.monotonic()
            code = subprocess.run(wrapped, cwd=folder, check=False).returncode
            entry["tries"] += 1
            entry["seconds"] = round(time.monotonic() - began, 1)
            if code == 0 and any(out.iterdir()):
                out.rename(parts / entry["id"])
                entry["state"] = "done"
                break
            entry["state"] = "failed"
            reason = f"exit {code}" if code else "nothing written in {out}"
            sys.stderr.write(f"slices: {entry['id']} failed ({reason}, try {attempt + 1})\n")
            shutil.rmtree(out)
            out.mkdir()
        save(folder, state)
        if entry["state"] != "done":
            shutil.rmtree(out, ignore_errors=True)
            sys.stderr.write(f"slices: stopped at {entry['id']}: fix it, then run again (done slices are kept)\n")
            return 1
        print(f"slices: {entry['id']} done in {entry['seconds']}s ({summary(state)})", flush=True)
    return cmd_status(args)

def summary(state: dict) -> str:
    done = [e for e in state["slices"] if e["state"] == "done"]
    left = len(state["slices"]) - len(done)
    average = sum(e["seconds"] for e in done) / len(done) if done else 0
    rest = average * left
    return f"{len(done)}/{len(state['slices'])} done, ~" + (f"{rest / 60:.0f} min left" if rest >= 60 else f"{rest:.0f}s left")

def cmd_status(args: argparse.Namespace) -> int:
    state = load(Path(args.dir).resolve())
    failed = [e["id"] for e in state["slices"] if e["state"] == "failed"]
    print(f"slices: {summary(state)}" + (f", failed: {' '.join(failed)}" if failed else ""))
    return 0

def files_in(part: Path) -> list[Path]:
    return sorted(p for p in part.rglob("*") if p.is_file())

def cmd_merge(args: argparse.Namespace) -> int:
    folder = Path(args.dir).resolve()
    state = load(folder)
    missing = [e["id"] for e in state["slices"] if e["state"] != "done"]
    if missing:
        raise SystemExit(f"slices: {len(missing)} slices not done ({' '.join(missing[:8])}...): run first")
    parts = [folder / "parts" / e["id"] for e in state["slices"]]
    final = Path(state["final"] or folder / "final").expanduser()
    if not final.is_absolute():
        final = folder / final
    kind = state["merge"]
    if kind == "frames":
        final.mkdir(parents=True, exist_ok=True)
        for part in parts:
            for path in files_in(part):
                target = final / path.name
                if target.exists():
                    raise SystemExit(f"slices: {path.name} is in two slices (names must be unique)")
                shutil.copy2(path, target)
    elif kind == "cat":
        with final.open("wb") as output:
            for part in parts:
                for path in files_in(part):
                    output.write(path.read_bytes())
    elif kind == "concat":
        listing = folder / "concat.txt"
        listing.write_text("".join(f"file {shlex.quote(str(p))}\n" for part in parts for p in files_in(part)))
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", str(listing),
                        "-c", "copy", str(final)], check=True)
    elif kind == "tiles":
        width = sum(e["w"] for e in state["slices"] if e["row"] == 0)
        height = sum(e["h"] for e in state["slices"] if e["col"] == 0)
        command = ["magick", "-size", f"{width}x{height}", "xc:none"]
        for entry, part in zip(state["slices"], parts):
            tile = files_in(part)[0]
            command += [str(tile), "-geometry", f"+{entry['x']}+{entry['y']}", "-composite"]
        depth = subprocess.run(["magick", "identify", "-format", "%z", str(files_in(parts[0])[0])],
                               capture_output=True, text=True, check=True).stdout.strip()
        command += ["-depth", depth or "8", str(final)]
        subprocess.run(command, check=True)
    elif kind == "none":
        print(f"slices: nothing to merge, the parts are in {folder / 'parts'}")
        return 0
    else:
        command = kind.format(parts=" ".join(shlex.quote(str(p)) for p in parts), final=shlex.quote(str(final)))
        subprocess.run(["bash", "-c", command], cwd=folder, check=True)
    print(f"slices: merged into {final}")
    return 0

def cmd_reset(args: argparse.Namespace) -> int:
    folder = Path(args.dir).resolve()
    state = load(folder)
    for entry in state["slices"]:
        if not args.ids or entry["id"] in args.ids:
            entry.update(state="pending", seconds=0.0, tries=0)
            shutil.rmtree(folder / "parts" / entry["id"], ignore_errors=True)
    save(folder, state)
    return cmd_status(args)

def main() -> int:
    parser = argparse.ArgumentParser(description="Long jobs in resumable slices, merged at the end.")
    sub = parser.add_subparsers(dest="action", required=True)
    init = sub.add_parser("init")
    init.add_argument("dir")
    init.add_argument("--cmd", required=True, help="command template ({start} {end} {item} {out} {x} {y}...)")
    init.add_argument("--range", help="items A-B (ex: frames 1-240)")
    init.add_argument("--size", type=int, default=10, help="items per slice with --range")
    init.add_argument("--items", help="a file, one slice per non-empty line ({item})")
    init.add_argument("--tiles", help="CxR tiles of an image (with --image WxH)")
    init.add_argument("--image", help="WxH of the full image (with --tiles)")
    init.add_argument("--merge", default="none", help="frames, cat, concat, tiles, none or a command")
    init.add_argument("--final", help="the merged result (default: DIR/final)")
    init.add_argument("--max", help="memory limit per slice (memory-guard), ex: 4G")
    init.add_argument("--force", action="store_true", help="replace an existing plan")
    run = sub.add_parser("run")
    run.add_argument("dir")
    run.add_argument("--retries", type=int, default=1)
    run.add_argument("--only", nargs="*", help="these slice ids only")
    for name in ("status", "merge"):
        sub.add_parser(name).add_argument("dir")
    reset = sub.add_parser("reset")
    reset.add_argument("dir")
    reset.add_argument("ids", nargs="*")
    args = parser.parse_args()
    return {"init": cmd_init, "run": cmd_run, "status": cmd_status, "merge": cmd_merge, "reset": cmd_reset}[args.action](args)

if __name__ == "__main__":
    sys.exit(main())
