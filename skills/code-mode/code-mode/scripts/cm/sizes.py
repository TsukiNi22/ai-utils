"""The real size of the results of each tool, learned call after call (by the engine and by the Claude Code hook).

One short line is appended per call ("server.tool<TAB>chars"); the lines are folded into sizes.json when the log grows.
search shows the average next to each tool, so the choice between a direct call and code rests on measured numbers.
"""
from __future__ import annotations

import os

from .paths import home, read_json, write_json

FOLD_OVER = 200000  # bytes of sizes.log before it is folded into sizes.json

def record(full: str, chars: int) -> None:
    """Note the size of one result of server.tool."""
    try:
        with open(home() / "sizes.log", "a", encoding="utf-8") as file:
            file.write(f"{full}\t{int(chars)}\n")
    except OSError:
        pass  # a statistic is never worth failing a call

def load() -> dict[str, dict]:
    """server.tool -> {"n": calls, "sum": chars, "max": chars}, the log folded in when it is big."""
    path, log = home() / "sizes.json", home() / "sizes.log"
    known = read_json(path, {})
    known = known if isinstance(known, dict) else {}
    try:
        with open(log, "r", encoding="utf-8") as file:
            lines = file.read().splitlines()
    except OSError:
        return known
    for line in lines:
        full, _, chars = line.partition("\t")
        if not full or not chars.isdigit():
            continue
        entry = known.setdefault(full, {"n": 0, "sum": 0, "max": 0})
        entry["n"] += 1
        entry["sum"] += int(chars)
        entry["max"] = max(entry["max"], int(chars))
    try:
        if os.path.getsize(log) > FOLD_OVER:
            write_json(path, known)
            with open(log, "w", encoding="utf-8"):
                pass  # emptied: its lines now live in sizes.json
    except OSError:
        pass  # folded at a next call
    return known

def average(known: dict[str, dict], full: str) -> int:
    """The average size in characters of the results of a tool, 0 when it was never measured."""
    entry = known.get(full)
    return int(entry["sum"] / entry["n"]) if entry and entry.get("n") else 0
