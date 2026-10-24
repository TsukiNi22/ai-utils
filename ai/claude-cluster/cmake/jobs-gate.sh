#!/usr/bin/env bash
# The compiler launcher of claude-cluster: at most <slots> compilations at once, whatever the -j of the build (a make -j alone starts
# every one: ~0.8 GB each, the memory of the computer gone); a free slot taken (a lock of <dir>, released with the compilation),
# else waited for. Without flock: the command run as it is.
# Usage: jobs-gate.sh <slots> <lock dir> <command...>
set -uo pipefail

slots="$1"
dir="$2"
shift 2
command -v flock > /dev/null || exec "$@"
command mkdir -p "$dir"
while true; do
    for ((i = 0; i < slots; i++)); do
        exec {fd}> "$dir/slot-$i"
        if flock -n "$fd"; then
            "$@"
            exit $?
        fi
        exec {fd}>&-
    done
    sleep 0.2
done
