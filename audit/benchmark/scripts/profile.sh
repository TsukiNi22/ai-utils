#!/bin/bash
# Profile a program: optimized build with symbols (in a copy), wall time, hardware counters, hot functions,
# flame graph, and optionally exact instruction counts (callgrind) and memory (heaptrack / massif).
#
# Usage: profile.sh --cmd "<command>" [--project <dir>] [--cwd <dir>] [--build-type <type>] [--runs <n>]
#                   [--out <dir>] [--callgrind] [--memory] [--no-build]
#
#   --cmd         command to measure (run with bash from --cwd), ex: --cmd "xstyle -S -l cpp"
#   --project     CMake project rebuilt in a /tmp copy (-g -fno-omit-frame-pointer, uncommitted changes
#                 included); its build outputs come first in the PATH while --cmd runs (default: .)
#   --no-build    measure the command as it is (already built program, Python script...)
#   --cwd         directory of the run, where the inputs are (default: the project)
#   --build-type  CMake build type (default: Optimized, the -O3 mode of the user's projects; Release otherwise)
#   --runs        wall time runs (default: 10)
#   --out         output directory (default: /tmp/benchmark-<date>)
#   --callgrind   exact instruction counts per function (valgrind, 20-50x slower: small inputs)
#   --memory      allocations / peak heap (heaptrack, else valgrind massif)
#
# Outputs in --out: build.log, time.json|time.txt, stat.csv, self.txt, children.txt, flame.svg, callgrind.txt,
# memory.txt, info.txt. Then: python3 hotspots.py <out> --md <out>/hotspots.md
# Exit code: 0 measured, 2 build / run failure.

set -uo pipefail

CMD=""; PROJECT="."; CWD=""; BUILD_TYPE=""; RUNS=10; OUT=""; CALLGRIND=false; MEMORY=false; BUILD=true
while [[ $# -gt 0 ]]; do
    case "$1" in
        --cmd) CMD="$2"; shift ;;
        --project) PROJECT="$2"; shift ;;
        --cwd) CWD="$2"; shift ;;
        --build-type) BUILD_TYPE="$2"; shift ;;
        --runs) RUNS="$2"; shift ;;
        --out) OUT="$2"; shift ;;
        --callgrind) CALLGRIND=true ;;
        --memory) MEMORY=true ;;
        --no-build) BUILD=false ;;
        -h|--help) sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Error: unknown argument '$1'" >&2; exit 2 ;;
    esac
    shift
done
[[ -z "$CMD" ]] && { echo "Error: --cmd is required" >&2; exit 2; }
PROJECT="$(cd "$PROJECT" && pwd)"
CWD="$(cd "${CWD:-$PROJECT}" && pwd)"
OUT="${OUT:-/tmp/benchmark-$(date +%Y%m%d-%H%M%S)}"
command mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

# =========================
# Build (copy of the working tree)
# =========================
if $BUILD && [[ -f "$PROJECT/CMakeLists.txt" ]]; then
    COPY="$(mktemp -d /tmp/benchmark-build-XXXXXX)"
    echo "build: $COPY"
    if command -v rsync > /dev/null 2>&1; then
        rsync -a --exclude .git --exclude 'build*' --exclude 'cmake-build-*' "$PROJECT/" "$COPY/"
    else
        cp -r "$PROJECT/." "$COPY/" && rm -rf "$COPY/.git"
    fi
    if [[ -z "$BUILD_TYPE" ]]; then
        grep -q "CONFIG:Optimized" "$PROJECT/CMakeLists.txt" && BUILD_TYPE="Optimized" || BUILD_TYPE="Release"
    fi
    FLAGS="-g -fno-omit-frame-pointer"
    if ! cmake -S "$COPY" -B "$COPY/build" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_CXX_FLAGS="$FLAGS" -DCMAKE_C_FLAGS="$FLAGS" > "$OUT/build.log" 2>&1 \
        || ! cmake --build "$COPY/build" --parallel "$(nproc)" >> "$OUT/build.log" 2>&1; then
        echo "Error: build failed, see $OUT/build.log" >&2
        exit 2
    fi
    export PATH="$COPY:$COPY/build:$PATH"
fi
cd "$CWD" || exit 2
RUN="exec $CMD"

{
    echo "command: $CMD"
    echo "cwd: $CWD"
    echo "project: $PROJECT"
    echo "build type: ${BUILD_TYPE:-not rebuilt}"
    echo "date: $(date '+%Y-%m-%d %H:%M')"
    echo "cpu: $(sed -n 's/^model name\s*: //p' /proc/cpuinfo | head -1) ($(nproc) threads)"
    echo "kernel: $(uname -r)"
} > "$OUT/info.txt"

# =========================
# Wall time
# =========================
echo "wall time: $RUNS runs"
if command -v hyperfine > /dev/null 2>&1; then
    hyperfine -i --warmup 1 --runs "$RUNS" --export-json "$OUT/time.json" "$CMD" > "$OUT/hyperfine.txt" 2>&1
else
    : > "$OUT/time.txt"
    for _ in $(seq "$RUNS"); do
        /usr/bin/time -f "%e %M" -a -o "$OUT/time.txt" bash -c "$RUN" > /dev/null 2>&1
    done
fi

# =========================
# Hardware counters (user space) & sampling
# =========================
if command -v perf > /dev/null 2>&1; then
    echo "perf stat"
    # LC_NUMERIC=C: decimal point in the CSV (a comma breaks it); cpu-clock: one sample stream on hybrid CPUs
    LC_NUMERIC=C perf stat -r 3 -x, -o "$OUT/stat.csv" \
        -e task-clock,cycles:u,instructions:u,branches:u,branch-misses:u,cache-references:u,cache-misses:u \
        -- bash -c "$RUN" > /dev/null 2>&1
    echo "perf record"
    if ! perf record -q -e cpu-clock -F 999 --call-graph dwarf,16384 -o "$OUT/perf.data" -- bash -c "$RUN" > /dev/null 2>&1 \
        && [[ ! -s "$OUT/perf.data" ]]; then
        perf record -q -e cpu-clock -F 999 --call-graph fp -o "$OUT/perf.data" -- bash -c "$RUN" > /dev/null 2>&1
    fi
    LC_NUMERIC=C perf report -i "$OUT/perf.data" --stdio --no-children --sort symbol,dso -g none --percent-limit 0.3 --field-separator=$'\t' 2> /dev/null > "$OUT/self.txt"
    LC_NUMERIC=C perf report -i "$OUT/perf.data" --stdio --children --sort symbol -g none --percent-limit 1 --field-separator=$'\t' 2> /dev/null > "$OUT/children.txt"
    if command -v stackcollapse-perf.pl > /dev/null 2>&1 && command -v flamegraph.pl > /dev/null 2>&1; then
        perf script -i "$OUT/perf.data" 2> /dev/null | stackcollapse-perf.pl | flamegraph.pl --title "$CMD" > "$OUT/flame.svg"
    fi
fi

# =========================
# Exact counts & memory (optional, slow)
# =========================
if $CALLGRIND && command -v valgrind > /dev/null 2>&1; then
    echo "callgrind"
    valgrind --tool=callgrind --trace-children=yes --callgrind-out-file="$OUT/callgrind.%p" bash -c "$RUN" > /dev/null 2>&1
    biggest="$(ls -S "$OUT"/callgrind.* 2> /dev/null | head -1)"
    [[ -n "$biggest" ]] && callgrind_annotate --inclusive=no "$biggest" 2> /dev/null | head -60 > "$OUT/callgrind.txt"
fi
if $MEMORY; then
    echo "memory"
    /usr/bin/time -v bash -c "$RUN" 2> "$OUT/rss.txt" > /dev/null
    if command -v heaptrack > /dev/null 2>&1; then
        heaptrack -o "$OUT/heaptrack" bash -c "$RUN" > /dev/null 2>&1
        heaptrack_print "$(ls "$OUT"/heaptrack.* | head -1)" 2> /dev/null | head -120 > "$OUT/memory.txt"
    elif command -v valgrind > /dev/null 2>&1; then
        valgrind --tool=massif --trace-children=yes --massif-out-file="$OUT/massif.%p" bash -c "$RUN" > /dev/null 2>&1
        biggest="$(ls -S "$OUT"/massif.* 2> /dev/null | head -1)"
        [[ -n "$biggest" ]] && ms_print "$biggest" 2> /dev/null | head -80 > "$OUT/memory.txt"
    fi
fi

echo "out: $OUT"
