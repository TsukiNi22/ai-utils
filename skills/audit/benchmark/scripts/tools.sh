#!/bin/bash
# Profiling tools available on this computer, and how to install the missing ones.
#
# Usage: tools.sh
#
# One line per tool: "<tool> ok <version>" | "<tool> missing <install command>", then the perf permission level.

set -uo pipefail # no -e: the probes of the missing tools fail on purpose (xstyle: ignore-file SH-STRICT)

# Package manager of the OS
if command -v dnf > /dev/null 2>&1; then
    PM="dnf"
elif command -v apt-get > /dev/null 2>&1; then
    PM="apt"
else
    PM=""
fi

# <tool> <command> <dnf package> <apt package> <what for>
TOOLS=(
    "perf|perf|perf|linux-tools-common linux-tools-$(uname -r)|sampling profiler: hot functions, call graph, hardware counters"
    "hyperfine|hyperfine|hyperfine|hyperfine|wall time with warmup, mean / stddev over many runs"
    "valgrind|valgrind|valgrind|valgrind|callgrind (exact instruction counts per function / line) and massif (heap)"
    "heaptrack|heaptrack|heaptrack|heaptrack|allocations: hot allocation sites, peak memory, leaks"
    "flamegraph|flamegraph.pl|flamegraph flamegraph-stackcollapse-perf|flamegraph|flame graph SVG from perf (FlameGraph scripts)"
    "time|/usr/bin/time|time|time|peak memory (max RSS) of a run"
    "py-spy|py-spy|-|-|Python sampling profiler (pip install --user py-spy)"
)

for entry in "${TOOLS[@]}"; do
    IFS='|' read -r name command dnf apt why <<< "$entry"
    if command -v "$command" > /dev/null 2>&1 || [[ -x "$command" ]]; then
        version="$("$command" --version 2> /dev/null | head -1 || true)"
        printf "%-10s ok       %s\n" "$name" "${version:-installed}"
        continue
    fi
    case "$PM" in
        dnf) install="sudo dnf install -y $dnf" ;;
        apt) install="sudo apt-get install -y $apt" ;;
        *) install="see the documentation of $name" ;;
    esac
    [[ "$dnf" == "-" ]] && install="pip install --user $name"
    printf "%-10s missing  %s   # %s\n" "$name" "$install" "$why"
done

# perf needs perf_event_paranoid <= 2 for the user space events of our own processes
paranoid="$(cat /proc/sys/kernel/perf_event_paranoid 2> /dev/null || echo "?")"
if [[ "$paranoid" == "?" ]]; then
    echo "perf_event_paranoid: unknown"
elif (( paranoid > 2 )); then
    echo "perf_event_paranoid: $paranoid (too high: sudo sysctl kernel.perf_event_paranoid=2, until the next reboot)"
else
    echo "perf_event_paranoid: $paranoid (ok: user space events, kernel symbols hidden when 2)"
fi
