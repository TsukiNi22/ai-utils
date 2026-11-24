"""The audit journal (what ran, which tools, how much went where) and the statistics computed from it.

One JSON line per execute, per single call and per direct MCP call seen by the Claude Code hook. The file is rotated
at `audit.max_bytes` (one older file kept). Nothing leaves the computer.
"""
from __future__ import annotations

import json
import os
import time
from collections.abc import Iterator
from pathlib import Path

from .paths import est_tokens, stamp, sub

def path() -> Path:
    return sub("audit") / "audit.jsonl"

def log(entry: dict, config: dict) -> None:
    """Append one record; rotate the journal when it is too big."""
    file_path = path()
    try:
        if file_path.is_file() and file_path.stat().st_size > int(config.get("audit", {}).get("max_bytes", 10000000)):
            os.replace(file_path, file_path.with_name("audit.1.jsonl"))
        with open(file_path, "a", encoding="utf-8") as file:
            file.write(json.dumps({"t": round(time.time(), 1), **entry}, ensure_ascii=False, default=str) + "\n")
        try:
            file_path.chmod(0o600)
        except OSError:
            pass  # no modes on this filesystem
    except OSError:
        pass  # the journal never fails the work it records

def records() -> Iterator[dict]:
    """Every record kept, oldest first."""
    for file_path in (path().with_name("audit.1.jsonl"), path()):
        try:
            with open(file_path, "r", encoding="utf-8") as file:
                for line in file:
                    try:
                        entry = json.loads(line)
                    except ValueError:
                        continue
                    if isinstance(entry, dict):
                        yield entry
        except OSError:
            continue

def tokens(chars: int) -> str:
    count = est_tokens(chars)
    return f"~{count / 1000:.1f}k" if count >= 1000 else f"~{count}"

def stats(session: str | None = None) -> str:
    """What code mode kept out of the context, for one session or for everything recorded."""
    runs = calls = failed = retried = direct = cut = recipes = 0
    returned = code = raw = sent = single_out = single_raw = direct_chars = cut_saved = 0
    first = 0.0
    by_tool: dict[str, list[int]] = {}
    for entry in records():
        if session and entry.get("session") != session:
            continue
        first = first or float(entry.get("t") or 0)
        kind = entry.get("op")
        if kind == "execute":
            runs += 1
            recipes += 1 if entry.get("recipe") else 0
            returned += int(entry.get("out") or 0)
            code += int(entry.get("code_chars") or 0)
            for call in entry.get("calls") or []:
                calls += 1
                failed += 0 if call[2] else 1
                retried += 1 if call[2] and call[7] > 1 else 0
                sent += call[4]
                raw += call[5]
                slot = by_tool.setdefault(f"{call[0]}.{call[1]}", [0, 0])
                slot[0] += 1
                slot[1] += call[5]
        elif kind == "call":
            single_out += int(entry.get("out") or 0)
            single_raw += int(entry.get("raw") or 0)
            calls += 1
            failed += 0 if entry.get("ok") else 1
        elif kind == "direct":
            direct += 1
            direct_chars += int(entry.get("chars") or 0)
            if entry.get("cut"):
                cut += 1
                cut_saved += int(entry["cut"])
    scope = f"session {session}" if session else "all sessions"
    lines = [f"codemode stats - {scope}" + (f" (since {stamp(first)})" if first else " (nothing recorded yet)")]
    if runs:
        direct_cost, code_cost = raw + sent, returned + code
        saved = direct_cost - code_cost
        share = f" ({100 * saved // direct_cost}%)" if direct_cost > 0 else ""
        lines.append(f"execute: {runs} runs ({recipes} from a recipe), {calls} tool calls ({failed} failed, {retried} ok after a retry)")
        lines.append(f"  in the context: {tokens(code_cost)} tok (code {tokens(code)} + output {tokens(returned)})"
                     f" | the same calls direct: {tokens(direct_cost)} tok | saved: {tokens(saved)} tok{share}")
    if single_raw or single_out:
        lines.append(f"call: returned {tokens(single_out)} tok for {tokens(single_raw)} tok of raw results")
    if direct:
        lines.append(f"direct MCP calls seen by the hook: {direct}, {tokens(direct_chars)} tok"
                     + (f" | cut by the compressor: {cut} results, {tokens(cut_saved)} tok saved" if cut else ""))
    top = sorted(by_tool.items(), key=lambda item: -item[1][1])[:5]
    if top:
        lines.append("most kept out of the context: " + ", ".join(f"{name} {tokens(chars)} tok ({count} calls)" for name, (count, chars) in top))
    lines.append("(tokens are estimated from the character counts)")
    return "\n".join(lines)
