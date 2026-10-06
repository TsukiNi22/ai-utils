"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  06/10/2026 by Tsukini

File Name:
##  compact.py

File Description:
##  Compact a text / command output for an AI context (rtk skill)
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Import #####
# Import that can't be in the try
from sys import exit, stderr, stdin, stdout

# Import that can be checked
try:
    from os.path import commonpath # Used to factor the common path prefix
    import argparse # Used to parse the options
    import re # Used to strip the ANSI sequences and the decorations
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(255)

# Check if the program is call and not imported
if __name__ != "__main__":
    stderr.write(f"The {__file__} can only be executed and not imported!\n")
    exit(255)

##### Const #####
ANSI = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]|\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)|\r") # colors, cursor, OSC 8 links
DECORATION = re.compile(r"^[\s\-=_*~#+|.─━═│┃┄┈╌╍]+$") # rulers, box lines, empty table rows
PATH = re.compile(r"(?<![\w.])/(?:[\w.+-]+/)+") # absolute directories inside a line

##### Tools #####
def strip(lines: list[str]) -> list[str]: # ANSI, trailing spaces, decoration lines, runs of spaces
    out = []
    for line in lines:
        line = ANSI.sub("", line).rstrip()
        if not line or DECORATION.match(line):
            continue
        out.append(re.sub(r"(?<=\S) {2,}", " ", line))
    return out

def dedupe(lines: list[str]) -> list[str]: # identical consecutive lines -> one line + xN
    out = []
    for line in lines:
        if out and out[-1][0] == line:
            out[-1][1] += 1
        else:
            out.append([line, 1])
    return [line if count == 1 else f"{line} x{count}" for line, count in out]

def factor_paths(lines: list[str]) -> tuple[list[str], str]: # common directory written once as $P
    dirs = [match.group(0) for line in lines for match in PATH.finditer(line)]
    if len(dirs) < 3:
        return lines, ""
    prefix = commonpath(dirs)
    if len(prefix) < 8:
        return lines, ""
    return [line.replace(prefix + "/", "$P/") for line in lines], prefix

##### Program #####
parser = argparse.ArgumentParser(description="Compact a text for an AI context (stdin or files -> stdout)")
parser.add_argument("files", nargs="*", help="input files (default: stdin)")
parser.add_argument("--max-line", type=int, default=0, help="truncate the lines longer than this (lossy, 0 = never)")
parser.add_argument("--head", type=int, default=0, help="keep only the first N lines, then a '+N lines' marker (lossy)")
parser.add_argument("--stats", action="store_true", help="print the size before / after on stderr")
args = parser.parse_args()

# Read
text = ""
for name in args.files or ["-"]:
    try:
        text += stdin.read() if name == "-" else open(name, "r", encoding="utf-8", errors="replace").read()
    except OSError as e:
        stderr.write(f"compact: error: {name}: {e.strerror}\n")
        exit(1)

# Compact (lossless steps first, then the lossy ones only when asked)
lines, prefix = factor_paths(dedupe(strip(text.splitlines())))
if args.max_line > 0:
    lines = [line if len(line) <= args.max_line else line[:args.max_line] + f"…(+{len(line) - args.max_line})" for line in lines]
if args.head > 0 and len(lines) > args.head:
    lines = lines[:args.head] + [f"…(+{len(lines) - args.head} lines)"]
result = (f"# $P={prefix}\n" if prefix else "") + "\n".join(lines) + "\n"
stdout.write(result)
if args.stats:
    stderr.write(f"compact: {len(text)} -> {len(result)} chars (~{len(text) // 4} -> ~{len(result) // 4} tokens, x{len(text) / max(len(result), 1):.1f})\n")
exit(0)
