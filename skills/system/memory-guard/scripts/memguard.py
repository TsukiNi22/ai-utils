#!/usr/bin/env python3
"""
Keep the heavy commands of a session inside a memory budget, so that the job is killed, never the session or the device.

Usage:
    memguard.py status [--top N]
    memguard.py budget [--per SIZE] [--reserve SIZE]
    memguard.py run [--max SIZE] [--swap SIZE] [--reserve SIZE] [--no-nice] -- <command> [args...]

- status: RAM and swap (total, available, used), memory pressure (PSI), tmpfs mounts that hold RAM (/tmp), the GPU
  memory (nvidia-smi), the processes using the most memory and the Claude sessions running.
- budget: the memory a new job can take now (available - reserve) and, with --per, how many parallel jobs fit
  (ex: --per 800M for a clang of a big C++ file -> the number for --parallel / -j).
- run: the command in its own systemd scope (systemd-run --user --scope) with MemoryMax / MemoryHigh / MemorySwapMax,
  nice 10 and idle IO priority; over the limit, the kernel kills that scope only. Without systemd-run, a watchdog polls
  the memory of the process tree and kills it over the limit. The exit code of the command is kept (137 = killed).
SIZE: 512M, 4G, 1.5G, or a percentage of the available memory (50%).
"""

import argparse
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

RESERVE = 2 << 30   # left free for the desktop and the sessions
MIN_LIMIT = 256 << 20
WATCH_PERIOD = 0.5

def meminfo() -> dict[str, int]:
    """/proc/meminfo in bytes."""
    values = {}
    for line in Path("/proc/meminfo").read_text().splitlines():
        name, _, rest = line.partition(":")
        values[name] = int(rest.split()[0]) * 1024
    return values

def human(size: int) -> str:
    value = float(size)
    for unit in ("B", "K", "M", "G"):
        if abs(value) < 1024:
            return f"{value:.0f}{unit}" if unit in ("B", "K") else f"{value:.1f}{unit}"
        value /= 1024
    return f"{value:.1f}T"

def parse_size(text: str, available: int) -> int:
    match = re.fullmatch(r"\s*([\d.]+)\s*([%KMGT]?)i?B?\s*", text, re.IGNORECASE)
    if not match:
        raise SystemExit(f"memguard: bad size '{text}' (ex: 512M, 4G, 50%)")
    number, unit = float(match.group(1)), match.group(2).upper()
    if unit == "%":
        return int(available * number / 100)
    return int(number * {"": 1, "K": 1 << 10, "M": 1 << 20, "G": 1 << 30, "T": 1 << 40}[unit])

def pressure() -> str:
    try:
        line = Path("/proc/pressure/memory").read_text().splitlines()[0]
    except OSError:
        return "n/a"
    return " ".join(part for part in line.split()[1:4])

def processes(top: int) -> list[tuple[int, int, str]]:
    """(rss, pid, command) of the processes using the most memory."""
    found = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            rss = int((entry / "statm").read_text().split()[1]) * os.sysconf("SC_PAGE_SIZE")
            command = (entry / "cmdline").read_bytes().replace(b"\0", b" ").decode(errors="replace").strip()
        except (OSError, IndexError, ValueError):
            continue
        if command:
            found.append((rss, int(entry.name), command[:90]))
    return sorted(found, reverse=True)[:top]

def available_budget(reserve: int) -> int:
    info = meminfo()
    return max(0, info["MemAvailable"] - reserve)

def cmd_status(args: argparse.Namespace) -> int:
    info = meminfo()
    swap_used = info["SwapTotal"] - info["SwapFree"]
    print(f"RAM        {human(info['MemAvailable'])} available / {human(info['MemTotal'])}")
    print(f"swap       {human(swap_used)} used / {human(info['SwapTotal'])}")
    print(f"pressure   {pressure()}  (some avg10 > 10 = the machine is already short)")
    for line in Path("/proc/mounts").read_text().splitlines():
        mount, kind = line.split()[1:3]
        if kind == "tmpfs" and mount in ("/tmp", "/dev/shm", "/var/tmp"):
            usage = shutil.disk_usage(mount)
            print(f"tmpfs      {mount}: {human(usage.used)} used (in RAM: big outputs go on disk)")
    if shutil.which("nvidia-smi"):
        gpu = subprocess.run(["nvidia-smi", "--query-gpu=memory.used,memory.total", "--format=csv,noheader"],
                             capture_output=True, text=True, check=False).stdout.strip()
        if gpu:
            print(f"GPU        {gpu}")
    sessions = [p for p in processes(10_000) if re.search(r"(^|/)claude(\s|$)", p[2])]
    print(f"claude     {len(sessions)} session(s), {human(sum(p[0] for p in sessions))}")
    print(f"budget     {human(available_budget(RESERVE))} for a new job (available - {human(RESERVE)} reserve)")
    print("top:")
    for rss, pid, command in processes(args.top):
        print(f"  {human(rss):>7}  {pid:>7}  {command}")
    return 0

def cmd_budget(args: argparse.Namespace) -> int:
    info = meminfo()
    reserve = parse_size(args.reserve, info["MemAvailable"])
    budget = available_budget(reserve)
    print(f"budget {human(budget)}")
    if args.per:
        per = parse_size(args.per, info["MemAvailable"])
        jobs = max(1, min(os.cpu_count() or 1, budget // max(per, 1)))
        print(f"jobs {jobs}  (--parallel {jobs} / -j{jobs}, {human(per)} each)")
    return 0

def tree_rss(root: int) -> int:
    """Memory of a process and all its children."""
    children: dict[int, list[int]] = {}
    rss: dict[int, int] = {}
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            stat = (entry / "stat").read_text()
            parent = int(stat.rsplit(")", 1)[1].split()[1])
            rss[int(entry.name)] = int((entry / "statm").read_text().split()[1]) * os.sysconf("SC_PAGE_SIZE")
        except (OSError, IndexError, ValueError):
            continue
        children.setdefault(parent, []).append(int(entry.name))
    total, stack = 0, [root]
    while stack:
        pid = stack.pop()
        total += rss.get(pid, 0)
        stack += children.get(pid, [])
    return total

def run_watched(command: list[str], limit: int, nice: bool) -> int:
    """Fallback without systemd: a process group killed when its memory goes over the limit."""
    def setup() -> None:
        os.setpgrp()
        if nice:
            os.nice(10)
    process = subprocess.Popen(command, preexec_fn=setup)
    try:
        while process.poll() is None:
            if tree_rss(process.pid) > limit:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                sys.stderr.write(f"memguard: killed, over {human(limit)} (raise --max or make the job smaller)\n")
                return 137
            time.sleep(WATCH_PERIOD)
    except KeyboardInterrupt:
        os.killpg(process.pid, signal.SIGTERM)
        process.wait()
    return process.returncode if process.returncode >= 0 else 128 - process.returncode

def cmd_run(args: argparse.Namespace) -> int:
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        raise SystemExit("memguard: no command (memguard.py run --max 4G -- <command>)")
    info = meminfo()
    reserve = parse_size(args.reserve, info["MemAvailable"])
    budget = available_budget(reserve)
    limit = parse_size(args.max, info["MemAvailable"]) if args.max else budget
    if limit > budget:
        print(f"memguard: --max {human(limit)} > what is free now ({human(budget)}): limited to {human(budget)}",
              file=sys.stderr)
        limit = budget
    if limit < MIN_LIMIT:
        print(f"memguard: only {human(budget)} free (reserve {human(reserve)}): free memory first (status)",
              file=sys.stderr)
        return 75
    swap = parse_size(args.swap, info["MemAvailable"]) if args.swap else limit // 2
    sys.stderr.write(f"memguard: limit {human(limit)} RAM + {human(swap)} swap\n")
    if shutil.which("systemd-run") and os.environ.get("MEMGUARD_NO_SYSTEMD") != "1":
        scope = ["systemd-run", "--user", "--scope", "--quiet", "--collect", f"--unit=memguard-{os.getpid()}",
                 "-p", f"MemoryMax={limit}", "-p", f"MemoryHigh={limit * 9 // 10}", "-p", f"MemorySwapMax={swap}"]
        probe = subprocess.run(scope[:4] + ["true"], capture_output=True, check=False)
        if probe.returncode == 0:
            low = ["nice", "-n", "10", "ionice", "-c", "3"] if not args.no_nice and shutil.which("ionice") else []
            code = subprocess.run(scope + ["--"] + low + command, check=False).returncode
            if code in (137, -9):
                sys.stderr.write(f"memguard: killed, over {human(limit)} (raise --max or make the job smaller)\n")
            return code if code >= 0 else 128 - code
    return run_watched(command, limit + swap, not args.no_nice)

def main() -> int:
    parser = argparse.ArgumentParser(description="Heavy commands inside a memory budget.")
    sub = parser.add_subparsers(dest="action", required=True)
    status = sub.add_parser("status", help="RAM, swap, pressure, tmpfs, GPU, top processes")
    status.add_argument("--top", type=int, default=8)
    budget = sub.add_parser("budget", help="memory free for a job, parallel jobs that fit")
    budget.add_argument("--per", help="memory of one parallel job (ex: 800M)")
    budget.add_argument("--reserve", default="2G")
    run = sub.add_parser("run", help="a command in a memory-limited scope")
    run.add_argument("--max", help="RAM limit (default: available - reserve)")
    run.add_argument("--swap", help="swap limit (default: half the RAM limit)")
    run.add_argument("--reserve", default="2G")
    run.add_argument("--no-nice", action="store_true", help="keep the normal CPU / IO priority")
    run.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    return {"status": cmd_status, "budget": cmd_budget, "run": cmd_run}[args.action](args)

if __name__ == "__main__":
    sys.exit(main())
