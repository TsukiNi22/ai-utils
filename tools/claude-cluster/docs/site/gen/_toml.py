#!/usr/bin/env python3
# =============================================================================
# _toml.py - reader of templates/config.toml as a commented reference: sections, keys, default values, their comments
# =============================================================================

import re


def split_value(rest: str) -> tuple[str, str]:
    """`value  # comment` -> (value, comment): the # of a quoted string or of a list of strings is not a comment."""
    quote = ""
    depth = 0
    for i, c in enumerate(rest):
        if quote:
            if c == quote and rest[i - 1] != "\\":
                quote = ""
        elif c in "\"'":
            quote = c
        elif c == "[":
            depth += 1
        elif c == "]":
            depth -= 1
        elif c == "#" and depth <= 0:
            return rest[:i].strip(), rest[i + 1:].strip()
    return rest.strip(), ""


def parse(text: str) -> list[dict]:
    """[{section, comment, keys: [{key, default, desc}]}] in file order; commented samples are ignored."""
    sections: list[dict] = []
    current: dict | None = None
    pending: list[str] = []
    last: dict | None = None
    for line in text.split("\n"):
        m = re.match(r"^\[([\w.]+)\]\s*(?:#\s*(.*))?$", line)
        if m:
            current = {"section": m.group(1), "comment": m.group(2) or "", "keys": [], "notes": pending}
            sections.append(current)
            pending, last = [], None
            continue
        if current is None:
            continue
        m = re.match(r"^(\w+)\s*=\s*(.*)$", line)
        if m:
            value, comment = split_value(m.group(2))
            if pending and pending[0].lower().startswith("work method") and not m.group(1).startswith("work_method"):
                carry, pending = pending, []  # the template puts this block before keys that are not about it
            else:
                carry = []
            last = {"key": m.group(1), "default": value, "desc": " ".join(pending + ([comment] if comment else [])).strip()}
            current["keys"].append(last)
            pending = carry if carry else []
            continue
        m = re.match(r"^#\s?(.*)$", line)
        if m and not re.match(r"^#\s*\[?[\w.]+\]?\s*(=.*)?$", line) or (m and line.startswith("# ")):
            text_line = m.group(1).strip()
            if re.match(r"^(\[[\w.]+\]|\w+\s*=)", text_line):  # a commented sample: left out
                continue
            pending.append(text_line)
            continue
        m = re.match(r"^\s+#\s?(.*)$", line)
        if m and last is not None:
            last["desc"] = (last["desc"] + " " + m.group(1).strip()).strip()
            continue
        if not line.strip():
            pending = []
            last = last if m is None else last
    return sections
