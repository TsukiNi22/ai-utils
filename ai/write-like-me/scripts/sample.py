#!/usr/bin/env python3
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  29/10/2026 by Tsukini

File Name:
##  sample.py

File Description:
##  A sample of what the user typed himself (his prompts, his .info notes): counts of his habits, a random sample; local only
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

"""
Usage: sample.py [count] - his prompts (claude-cluster histories, the newest Claude Code transcripts: only what he typed) and
his .info notes. Local only: never paste the sample anywhere else.
"""

##### Imports #####

import collections # Used for Counter
import glob # Used for glob
import json # Used for loads
import os # Used for path
import random # Used for sample
import re # Used for findall, sub, match
import sys # Used for argv

##### Constants #####

HOME = os.path.expanduser("~")
TRANSCRIPTS = 120          # newest Claude Code transcripts read
TAIL = 3_000_000           # bytes read at the end of each
GENERATED = re.compile(r"\[(reply|from|claude-cluster|scheduled|a turn)|^\(a turn|^(You are session|Fix in|Feature in|Change in) ")
HABITS = ["???", "!!!", "...", "->", "/", "(", " ou autre", "ect", "tj", "genre", "truc", "fait en sorte", "rajoute", "check", "setup", "remove"]

##### Functions #####

def read_cluster() -> list:
    """Every prompt typed in claude-cluster (the main data folder and its instances)"""
    texts = []
    for path in glob.glob(f"{HOME}/.local/share/claude-cluster/**/history.jsonl", recursive=True):
        for line in open(path, encoding="utf-8", errors="replace"):
            try:
                entry = json.loads(line)
            except ValueError:
                continue
            text = entry.get("text") if isinstance(entry, dict) else entry
            if isinstance(text, str):
                texts.append(text)
    return texts


def read_claude_code() -> list:
    """The messages the user typed in the newest Claude Code transcripts (no tool results, no commands)"""
    texts = []
    files = sorted(glob.glob(f"{HOME}/.claude/projects/*/*.jsonl"), key=os.path.getmtime, reverse=True)[:TRANSCRIPTS]
    for path in files:
        try:
            with open(path, "rb") as f:
                f.seek(max(0, os.path.getsize(path) - TAIL))
                lines = f.read().decode("utf-8", "replace").splitlines()[1:]
        except OSError:
            continue
        for line in lines:
            if '"type":"user"' not in line:
                continue
            try:
                content = (json.loads(line).get("message") or {}).get("content")
            except ValueError:
                continue
            if isinstance(content, list):
                content = " ".join(c.get("text", "") for c in content if isinstance(c, dict) and c.get("type") == "text")
            if isinstance(content, str) and not content.startswith("<") and "tool_use_id" not in content:
                texts.append(content)
    return texts


def read_notes() -> list:
    """His .info notes (delivery / personal_delivery)"""
    paths = glob.glob(f"{HOME}/delivery/**/.info", recursive=True) + glob.glob(f"{HOME}/personal_delivery/**/.info", recursive=True)
    return [open(p, encoding="utf-8", errors="replace").read() for p in paths if os.path.isfile(p) and "/build" not in p]


def clean(texts: list) -> list:
    """His own words only: images / pasted blocks dropped, generated prompts and continuations skipped, no duplicate"""
    seen = set()
    out = []
    for text in texts:
        text = re.sub(r"\[(Image|Pasted text)[^\]]*\]", "", text).strip()
        if len(text) < 15 or GENERATED.search(text) or "This session is being continued" in text or "SYSTEM NOTIFICATION" in text:
            continue
        if text[:200] not in seen:
            seen.add(text[:200])
            out.append(text)
    return out

##### Main #####

def main() -> int:
    count = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 40
    messages = clean(read_cluster() + read_claude_code())
    notes = clean(read_notes())
    joined = "\n".join(messages).lower()
    print(f"messages {len(messages)}, notes {len(notes)}, words {len(re.findall(r'[a-zà-ÿ]+', joined))}")
    count_of = lambda habit: len(re.findall(r"\b" + re.escape(habit.strip()) + r"\b", joined)) if habit.strip()[0].isalpha() else joined.count(habit) # words whole
    print("habits:", ", ".join(f"{h.strip()!r} {count_of(h)}" for h in HABITS))
    print("accented letters / 1000 chars:", round(sum(joined.count(c) for c in "éèêàùç") * 1000 / max(1, len(joined)), 1))
    print("first words:", collections.Counter(m.split()[0].lower() for m in messages if m.split()).most_common(15))
    random.seed()
    for text in random.sample(messages, min(count, len(messages))):
        print("---")
        print(text[:400])
    for text in random.sample(notes, min(3, len(notes))):
        print("--- note")
        print(text[:600])
    return 0


if __name__ == "__main__":
    sys.exit(main())
