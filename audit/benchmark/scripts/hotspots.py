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
##  hotspots.py

File Description:
##  Summarize a profile.sh output: wall time, counters, hot functions (self / inclusive), callgrind, memory
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Import #####
# Import that can't be in the try
from sys import argv, exit, stderr

# Import that can be checked
try:
    from pathlib import Path # Used to read the outputs of profile.sh
    from statistics import mean, stdev # Used for the wall time
    import argparse # Used for the arguments
    import json # Used to read hyperfine and write the summary
    import re # Used to parse the perf / callgrind outputs
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(255)

# Check if the program is call and not imported
if __name__ != "__main__":
    stderr.write(f"The {__file__} can only be executed and not imported!\n")
    exit(255)

##### Const #####
TOP = 25 # Rows of the hot functions tables
SYSTEM = re.compile(r"^(std::|__gnu|_|operator new|operator delete|malloc|free|mem|str[a-z]*$|cfree|\[|0x)")

##### Tools #####
def shorten(symbol: str) -> str:
    # Template arguments collapsed (std::vector<std::string, std::allocator<...>> -> std::vector<…>)
    previous = None
    while previous != symbol:
        previous = symbol
        symbol = re.sub(r"<[^<>]*>", "\x01", symbol) # sentinel without < > so that the outer level collapses next
    symbol = symbol.replace("\x01", "<…>")
    symbol = re.sub(r"\((?:[^()]|\([^()]*\))*\)( const)?$", "()", symbol)
    return symbol if len(symbol) <= 100 else symbol[:97] + "..."

def name_of(symbol: str) -> str:
    # Qualified name without the return type (bool std::regex_search<...>(...) -> std::regex_search<...>)
    head = symbol.split("(")[0]
    depth = 0
    start = 0
    for i, c in enumerate(head):
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == " " and depth == 0:
            start = i + 1
    return head[start:]

def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""

def self_rows(text: str) -> list[dict]:
    # perf report --no-children --sort symbol,dso --field-separator=tab: "49.14%<tab>[.] symbol<tab>libstdc++.so.6"
    rows = []
    for line in text.splitlines():
        fields = line.split("\t")
        if line.startswith("#") or len(fields) < 3 or not fields[0].strip().endswith("%"):
            continue
        rows.append({"self": float(fields[0].strip().rstrip("%")), "symbol": re.sub(r"^\[.\]\s*", "", fields[1].strip()), "dso": fields[2].strip()})
    return rows

def children_rows(text: str) -> list[dict]:
    # perf report --children --sort symbol --field-separator=tab: "94.36%<tab>0.00%<tab>[.] symbol"
    rows = []
    for line in text.splitlines():
        fields = line.split("\t")
        if line.startswith("#") or len(fields) < 3 or not fields[0].strip().endswith("%"):
            continue
        rows.append({"inclusive": float(fields[0].strip().rstrip("%")), "self": float(fields[1].strip().rstrip("%")),
            "symbol": re.sub(r"^\[.\]\s*", "", fields[2].strip())})
    return rows

def counters(text: str) -> dict:
    # perf stat -x, (hybrid CPUs: cpu_core/cycles/u + cpu_atom/cycles/u are summed)
    values = {}
    for line in text.splitlines():
        parts = line.split(",")
        if len(parts) < 3 or not parts[0].replace(".", "", 1).isdigit():
            continue
        event = re.sub(r"^cpu_\w+/|/u$|:u$", "", parts[2])
        values[event] = values.get(event, 0.0) + float(parts[0])
    return values

def wall_time(out: Path) -> dict:
    hyperfine = out / "time.json"
    if hyperfine.exists():
        result = json.loads(read(hyperfine))["results"][0]
        return {"mean": result["mean"], "stddev": result.get("stddev") or 0.0, "min": result["min"], "max": result["max"], "runs": len(result["times"]), "tool": "hyperfine"}
    times = []
    rss = []
    for line in read(out / "time.txt").splitlines():
        parts = line.split()
        if len(parts) == 2:
            times.append(float(parts[0]))
            rss.append(int(parts[1]))
    if not times:
        return {}
    return {"mean": mean(times), "stddev": stdev(times) if len(times) > 1 else 0.0, "min": min(times), "max": max(times), "runs": len(times),
        "max_rss_kb": max(rss), "tool": "/usr/bin/time"}

def table(headers: list[str], rows: list[list[str]]) -> str:
    lines = ["| " + " | ".join(headers) + " |", "|" + "---|" * len(headers)]
    lines += ["| " + " | ".join(row) + " |" for row in rows]
    return "\n".join(lines)

##### Program #####
parser = argparse.ArgumentParser(description="Summarize the outputs of profile.sh")
parser.add_argument("out", help="output directory of profile.sh")
parser.add_argument("--md", help="Markdown summary to write (tables for the report)")
parser.add_argument("--json", help="JSON summary to write")
args = parser.parse_args(argv[1:])
out = Path(args.out)
if not out.is_dir():
    stderr.write(f"Error: {out} is not a directory\n")
    exit(1)

info = read(out / "info.txt").strip()
time = wall_time(out)
stat = counters(read(out / "stat.csv"))
self_hot = self_rows(read(out / "self.txt"))
inclusive = children_rows(read(out / "children.txt"))
# Functions of the program (not std / libc / runtime), once each (several call chains give several rows)
own = []
for row in inclusive:
    if SYSTEM.match(name_of(row["symbol"])) or any(shorten(row["symbol"]) == shorten(o["symbol"]) for o in own):
        continue
    own.append(row)
summary = {"info": info, "time": time, "counters": stat, "self": self_hot[:TOP], "inclusive": own[:TOP]}

md = ["## Measures", "", "```text", info, "```", ""]
if time:
    md += [table(["Wall time", "Value"], [
        ["mean", f"**{time['mean'] * 1000:.1f} ms**"], ["stddev", f"{time['stddev'] * 1000:.1f} ms"],
        ["min / max", f"{time['min'] * 1000:.1f} / {time['max'] * 1000:.1f} ms"], ["runs", f"{time['runs']} ({time['tool']})"],
    ] + ([["max RSS", f"{time['max_rss_kb'] / 1024:.1f} MiB"]] if "max_rss_kb" in time else [])), ""]
if stat:
    cycles = stat.get("cycles", 0)
    instructions = stat.get("instructions", 0)
    rows = [["task-clock", f"{stat.get('task-clock', 0):.0f} ms (3 runs)"], ["instructions / cycle (IPC)", f"**{instructions / cycles:.2f}**" if cycles else "n/a"]]
    if stat.get("branches"):
        rows.append(["branch misses", f"{100 * stat.get('branch-misses', 0) / stat['branches']:.2f} %"])
    if stat.get("cache-references"):
        rows.append(["cache misses", f"{100 * stat.get('cache-misses', 0) / stat['cache-references']:.2f} % of the references"])
    md += [table(["Counter (user space)", "Value"], rows), ""]
if self_hot:
    md += ["## Hot functions (self time: where the CPU is)", "",
        table(["Self", "Function", "Library"], [[f"{r['self']:.2f} %", f"`{shorten(r['symbol'])}`", f"`{r['dso']}`"] for r in self_hot[:TOP]]), ""]
if own:
    md += ["## Functions of the program (inclusive: with what they call)", "",
        table(["Inclusive", "Self", "Function"], [[f"{r['inclusive']:.2f} %", f"{r['self']:.2f} %", f"`{shorten(r['symbol'])}`"] for r in own[:TOP]]), ""]
callgrind = read(out / "callgrind.txt")
if callgrind:
    md += ["## Exact instruction counts (callgrind, self)", "", "```text", "\n".join(callgrind.splitlines()[:40]), "```", ""]
memory = read(out / "memory.txt")
if memory:
    md += ["## Memory", "", "```text", "\n".join(memory.splitlines()[:40]), "```", ""]
if (out / "flame.svg").exists():
    md += [f"Flame graph: `{out / 'flame.svg'}`", ""]

if args.md:
    Path(args.md).write_text("\n".join(md), encoding="utf-8")
if args.json:
    Path(args.json).write_text(json.dumps(summary, indent=4), encoding="utf-8")
if not args.md and not args.json:
    print("\n".join(md))
exit(0)
