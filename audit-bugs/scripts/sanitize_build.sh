#!/bin/bash
# Build a CMake project with sanitizers in a temporary copy, run its unit tests (and an optional command),
# then summarize the sanitizer reports. Nothing is written in the project itself.
#
# Usage: sanitize_build.sh <project> [asan|ubsan|tsan|all] [--run "<command args>"] [--keep] [--timeout <s>]
#                          [--lsan-supp <file>] [--tsan-supp <file>]
#
#   asan    AddressSanitizer + LeakSanitizer (out of bounds, use after free, double free, leaks...)
#   ubsan   UndefinedBehaviorSanitizer (signed overflow, invalid shift, null/misaligned access, bad casts...)
#   tsan    ThreadSanitizer (data races, lock order inversion); can't be combined with asan
#   all     asan+ubsan build, then a tsan build (default)
#   --run   command run from the build copy after the tests (ex: --run "./context-forge --help")
#   --keep  keep the temporary copy (path printed) to rerun / debug
#   --lsan-supp / --tsan-supp  suppression files for verified false positives (ex: leaks made on purpose by a
#           leak-detection test: "leak:AInstructionLeakTest"), one "kind:pattern" per line
#
# The working tree (uncommitted changes included, without .git and build dirs) is copied to /tmp, so the
# outputs written in the sources (binaries at the root, generated headers) never touch the project.
# Exit code: 0 = no sanitizer report, 1 = reports found, 2 = build failure.

set -uo pipefail

PROJECT="${1:-}"; [ -z "$PROJECT" ] && { sed -n '2,21p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
shift
MODE="all"; RUN=""; KEEP=false; TIMEOUT=120; LSAN_SUPP=""; TSAN_SUPP=""
while [ $# -gt 0 ]; do
    case "$1" in
        asan|ubsan|tsan|all) MODE="$1" ;;
        --run) RUN="$2"; shift ;;
        --keep) KEEP=true ;;
        --timeout) TIMEOUT="$2"; shift ;;
        --lsan-supp) LSAN_SUPP="$(realpath "$2")"; shift ;;
        --tsan-supp) TSAN_SUPP="$(realpath "$2")"; shift ;;
        *) echo "Error: unknown argument '$1'" >&2; exit 2 ;;
    esac
    shift
done
PROJECT="$(cd "$PROJECT" && pwd)" || exit 2
[ -f "$PROJECT/CMakeLists.txt" ] || { echo "Error: no CMakeLists.txt in $PROJECT" >&2; exit 2; }
NAME="$(basename "$PROJECT")"
WORK="$(mktemp -d "/tmp/sanitize-$NAME-XXXX")"
$KEEP || trap 'rm -rf "$WORK"' EXIT
LOGS="$WORK/logs"; command mkdir -p "$LOGS"

export ASAN_OPTIONS="halt_on_error=0:detect_leaks=1:detect_stack_use_after_return=1:strict_init_order=1:check_initialization_order=1:print_stacktrace=1:log_path=$LOGS/asan"
export UBSAN_OPTIONS="halt_on_error=0:print_stacktrace=1:log_path=$LOGS/ubsan"
export TSAN_OPTIONS="halt_on_error=0:second_deadlock_stack=1:log_path=$LOGS/tsan${TSAN_SUPP:+:suppressions=$TSAN_SUPP}"
export LSAN_OPTIONS="log_path=$LOGS/lsan${LSAN_SUPP:+:suppressions=$LSAN_SUPP}"

run_variant() { # <label> <sanitizer flags>
    local label="$1" flags="$2" src="$WORK/$1"
    command mkdir -p "$src"
    # Copy of the working tree without .git / build outputs
    tar -C "$PROJECT" --exclude=.git --exclude=build --exclude='build-*' --exclude='cmake-build-*' -cf - . | tar -C "$src" -xf -
    local tests=OFF; [ -f "$src/tests/CMakeLists.txt" ] && tests=ON
    echo "=== $label: configure + build (tests $tests)"
    # clang/clang++ forced before project(): the default c++ (gcc) may lack the sanitizer runtimes
    if ! cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=$tests \
            -DCMAKE_C_COMPILER="${CC:-clang}" -DCMAKE_CXX_COMPILER="${CXX:-clang++}" \
            -DCMAKE_CXX_FLAGS="$flags -fno-omit-frame-pointer -g -O1" \
            -DCMAKE_C_FLAGS="$flags -fno-omit-frame-pointer -g -O1" \
            -DCMAKE_EXE_LINKER_FLAGS="$flags" -DCMAKE_SHARED_LINKER_FLAGS="$flags" > "$LOGS/$label-configure.log" 2>&1; then
        tail -20 "$LOGS/$label-configure.log"; return 2
    fi
    if ! cmake --build "$src/build" --parallel "$(nproc)" > "$LOGS/$label-build.log" 2>&1; then
        grep -E "error|Error" "$LOGS/$label-build.log" | head -20; return 2
    fi
    grep -E "warning:" "$LOGS/$label-build.log" | sort -u > "$LOGS/$label-warnings.log"
    echo "    build ok, $(wc -l < "$LOGS/$label-warnings.log") unique compiler warning(s)"
    if [ "$tests" = ON ]; then
        echo "=== $label: ctest"
        (cd "$src/build" && timeout "$((TIMEOUT * 4))" ctest --output-on-failure --timeout "$TIMEOUT" > "$LOGS/$label-ctest.log" 2>&1)
        grep -E "tests passed|tests failed|Total Test" "$LOGS/$label-ctest.log" | sed 's/^/    /'
    fi
    if [ -n "$RUN" ]; then
        echo "=== $label: run '$RUN'"
        (cd "$src" && timeout "$TIMEOUT" bash -c "$RUN" > "$LOGS/$label-run.log" 2>&1); echo "    exit code $?"
    fi
    return 0
}

status=0
case "$MODE" in
    asan) run_variant asan "-fsanitize=address" || status=2 ;;
    ubsan) run_variant ubsan "-fsanitize=undefined -fsanitize=float-divide-by-zero,unsigned-integer-overflow,implicit-conversion,nullability" || status=2 ;;
    tsan) run_variant tsan "-fsanitize=thread" || status=2 ;;
    all)
        run_variant asan-ubsan "-fsanitize=address,undefined" || status=2
        run_variant tsan "-fsanitize=thread" || status=2
        ;;
esac

# =========================
# Summary of the sanitizer reports
# =========================
echo
echo "=== Sanitizer reports ($LOGS)"
REPORTS=$(cat "$LOGS"/asan.* "$LOGS"/ubsan.* "$LOGS"/tsan.* "$LOGS"/lsan.* "$LOGS"/*-ctest.log "$LOGS"/*-run.log 2>/dev/null)
count() { printf '%s\n' "$REPORTS" | grep -cE "$1" || true; }
printf "  %-34s %s\n" "AddressSanitizer errors" "$(count 'ERROR: AddressSanitizer')"
printf "  %-34s %s\n" "LeakSanitizer (leaks)" "$(count 'ERROR: LeakSanitizer')"
printf "  %-34s %s\n" "UBSan runtime errors" "$(count 'runtime error:')"
printf "  %-34s %s\n" "ThreadSanitizer warnings" "$(count 'WARNING: ThreadSanitizer')"
echo
echo "--- Distinct issues (kind + first frame in the project)"
printf '%s\n' "$REPORTS" | grep -E "ERROR: (Address|Leak)Sanitizer|runtime error:|WARNING: ThreadSanitizer" \
    | sed -E 's/==[0-9]+==//; s/ on (unknown )?address 0x[0-9a-f]+.*//; s/ \(pid=[0-9]+\)//; s#/tmp/sanitize-[^/]+/[^/]+/##' | sort | uniq -c | sort -rn | head -30
echo
echo "--- Project frames in the stacks (most frequent)"
printf '%s\n' "$REPORTS" | grep -oE "#[0-9]+ 0x[0-9a-f]+ in [^ ]+ /tmp/sanitize-[^/]+/[^/]+/(src|include|tests)/[^ ]+" \
    | sed -E 's#.* in ([^ ]+) /tmp/sanitize-[^/]+/[^/]+/#\1 #' | sort | uniq -c | sort -rn | head -20
if [ -n "$(printf '%s\n' "$REPORTS" | grep -E 'ERROR: (Address|Leak)Sanitizer|runtime error:|WARNING: ThreadSanitizer')" ]; then
    [ $status -eq 0 ] && status=1
fi
$KEEP && echo && echo "Kept: $WORK (logs in $LOGS)"
exit $status
