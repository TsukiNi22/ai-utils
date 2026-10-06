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
##  uncovered.py

File Description:
##  What the tests don't cover (llvm-cov export of coverage.sh): per file, functions never run, line ranges
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Import #####
# Import that can't be in the try
from sys import argv, exit, stderr

# Import that can be checked
try:
    from pathlib import Path # Used to read the outputs of coverage.sh
    from shutil import which # Used to find c++filt
    import subprocess # Used to demangle the function names
    import argparse # Used for the arguments
    import json # Used to read the llvm-cov export
    import re # Used to shorten the names
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(255)

# Check if the program is call and not imported
if __name__ != "__main__":
    stderr.write(f"The {__file__} can only be executed and not imported!\n")
    exit(255)

##### Const #####
CODE_REGION = 0 # llvm-cov region kind of the executable code (1: expansion, 2: skipped, 3: gap, 4: branch)

##### Tools #####
def demangle(names: list[str]) -> list[str]:
    if not names or not which("c++filt"):
        return names
    result = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True)
    lines = result.stdout.splitlines()
    return lines if len(lines) == len(names) else names

def shorten(symbol: str) -> str:
    # Template arguments collapsed, std::__cxx11::basic_string<...> -> std::string
    symbol = symbol.replace("std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >", "std::string")
    previous = None
    while previous != symbol:
        previous = symbol
        symbol = re.sub(r"<[^<>]*>", "\x01", symbol) # sentinel without < > so that the outer level collapses next
    symbol = symbol.replace("\x01", "<…>").replace("[abi:cxx11]", "")
    return symbol if len(symbol) <= 120 else symbol[:117] + "..."

def ranges(lines: set[int]) -> str:
    # 12, 14-20, 31
    parts = []
    for line in sorted(lines):
        if parts and line == parts[-1][1] + 1:
            parts[-1][1] = line
        else:
            parts.append([line, line])
    return ", ".join(str(a) if a == b else f"{a}-{b}" for a, b in parts)

def percent(summary: dict) -> str:
    return "—" if summary.get("count", 0) == 0 else f"{summary['percent']:.1f} %"

##### Program #####
parser = argparse.ArgumentParser(description="What the tests don't cover (output of coverage.sh)")
parser.add_argument("out", help="output directory of coverage.sh")
parser.add_argument("--md", help="Markdown file to write (tables for the report)")
parser.add_argument("--json", help="JSON file to write")
parser.add_argument("--top", type=int, default=40, help="files shown in the Markdown")
args = parser.parse_args(argv[1:])
out = Path(args.out)
export = out / "coverage.json"
if not export.exists():
    stderr.write(f"Error: {export} not found (run coverage.sh first)\n")
    exit(1)
data = json.loads(export.read_text(encoding="utf-8"))["data"][0]
source = (out / "source.txt").read_text(encoding="utf-8").strip() + "/" if (out / "source.txt").exists() else ""

# Per file: numbers, then the lines of the regions never executed and the functions never called
files = {}
for entry in data["files"]:
    name = entry["filename"].removeprefix(source)
    files[name] = {"summary": entry["summary"], "lines": set(), "functions": []}
uncalled = []
for function in data.get("functions", []):
    for region in function["regions"]:
        line_start, _, line_end, _, count, file_id, _, kind = region[:8]
        name = function["filenames"][file_id].removeprefix(source)
        if kind == CODE_REGION and count == 0 and name in files:
            files[name]["lines"].update(range(line_start, line_end + 1))
    if function["count"] == 0:
        uncalled.append((function["filenames"][0].removeprefix(source), function["name"]))
names = demangle([name for _, name in uncalled])
for (file, _), name in zip(uncalled, names):
    if file in files and name not in files[file]["functions"]:
        files[file]["functions"].append(name)

totals = data["totals"]
order = sorted(files, key=lambda f: files[f]["summary"]["lines"]["count"] - files[f]["summary"]["lines"]["covered"], reverse=True)
md = ["## Coverage", "",
    "| Metric | Covered |", "|---|---|",
    f"| lines | **{percent(totals['lines'])}** ({totals['lines']['covered']} / {totals['lines']['count']}) |",
    f"| functions | **{percent(totals['functions'])}** ({totals['functions']['covered']} / {totals['functions']['count']}) |",
    f"| branches | **{percent(totals['branches'])}** ({totals['branches']['covered']} / {totals['branches']['count']}) |",
    f"| regions | {percent(totals['regions'])} |", "",
    "## Files (most missed lines first)", "",
    "| File | Lines | Functions | Branches | Missed lines |", "|---|---|---|---|---|"]
for name in order[:args.top]:
    summary = files[name]["summary"]
    missed = summary["lines"]["count"] - summary["lines"]["covered"]
    if missed == 0:
        continue
    md.append(f"| `{name}` | {percent(summary['lines'])} | {percent(summary['functions'])} | {percent(summary['branches'])} | {missed} |")
md += ["", "## Never run", ""]
for name in order:
    entry = files[name]
    if not entry["functions"] and not entry["lines"]:
        continue
    md.append(f"### `{name}`")
    if entry["functions"]:
        md.append("Functions never called: " + ", ".join(f"`{shorten(f)}`" for f in entry["functions"][:30]) + (" ..." if len(entry["functions"]) > 30 else ""))
    if entry["lines"]:
        md.append(f"Lines never run: {ranges(entry['lines'])}")
    md.append("")

summary = {"totals": totals, "files": {name: {"summary": files[name]["summary"], "functions": files[name]["functions"],
    "lines": ranges(files[name]["lines"])} for name in order}}
if args.md:
    Path(args.md).write_text("\n".join(md), encoding="utf-8")
if args.json:
    Path(args.json).write_text(json.dumps(summary, indent=4), encoding="utf-8")
if not args.md and not args.json:
    print("\n".join(md))
exit(0)
