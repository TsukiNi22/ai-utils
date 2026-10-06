#!/bin/bash
# Coverage of the unit tests of a CMake project (cpp-tests layout): clang source based coverage in a /tmp copy.
#
# Usage: coverage.sh [<project>] [--out <dir>] [--html] [--run "<command>"] [--ignore "<regex>"]
#
#   <project>   root of the project (default: .), tests/ + BUILD_TESTS as set up by cpp-tests
#   --out       output directory (default: /tmp/coverage-<date>)
#   --html      also the HTML pages (llvm-cov show), <out>/html/index.html
#   --run       command that runs the tests from the copy (default: ./unit_tests, else ctest)
#   --ignore    files left out of the numbers (default: tests/, the system and generated headers)
#
# Outputs: build.log, tests.log, summary.txt (llvm-cov report), coverage.json (llvm-cov export), html/.
# Then: python3 uncovered.py <out> --md <out>/uncovered.md
# Exit code: 0 measured (even with failing tests: see tests.log), 2 build failure.

set -uo pipefail

PROJECT="."; OUT=""; HTML=false; RUN=""; IGNORE='(^|/)(tests|_deps|build)/|^/usr/|generated_'
while [[ $# -gt 0 ]]; do
    case "$1" in
        --out) OUT="$2"; shift ;;
        --html) HTML=true ;;
        --run) RUN="$2"; shift ;;
        --ignore) IGNORE="$2"; shift ;;
        -h|--help) sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*) echo "Error: unknown argument '$1'" >&2; exit 2 ;;
        *) PROJECT="$1" ;;
    esac
    shift
done
for tool in clang++ llvm-profdata llvm-cov cmake; do
    command -v "$tool" > /dev/null 2>&1 || { echo "Error: $tool is required (clang / llvm package)" >&2; exit 2; }
done
PROJECT="$(cd "$PROJECT" && pwd)"
OUT="${OUT:-/tmp/coverage-$(date +%Y%m%d-%H%M%S)}"
command mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

# =========================
# Instrumented build (copy of the working tree)
# =========================
COPY="$(mktemp -d /tmp/coverage-build-XXXXXX)"
if command -v rsync > /dev/null 2>&1; then
    rsync -a --exclude .git --exclude 'build*' --exclude 'cmake-build-*' "$PROJECT/" "$COPY/"
else
    cp -r "$PROJECT/." "$COPY/" && rm -rf "$COPY/.git"
fi
FLAGS="-fprofile-instr-generate -fcoverage-mapping -O0 -g"
echo "build: $COPY"
touch "$OUT/.start" # binaries built after it = instrumented ones
if ! cmake -S "$COPY" -B "$COPY/build" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_FLAGS="$FLAGS" -DCMAKE_C_FLAGS="$FLAGS" -DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate" \
        -DCMAKE_SHARED_LINKER_FLAGS="-fprofile-instr-generate" > "$OUT/build.log" 2>&1 \
    || ! cmake --build "$COPY/build" --parallel "$(nproc)" >> "$OUT/build.log" 2>&1; then
    echo "Error: build failed, see $OUT/build.log" >&2
    exit 2
fi

# =========================
# Run the tests (one profile per process: forks / sub-processes included)
# =========================
cd "$COPY" || exit 2
export LLVM_PROFILE_FILE="$OUT/profiles/%p-%m.profraw"
if [[ -z "$RUN" ]]; then
    BINARY="$(find "$COPY" -maxdepth 3 -type f -name unit_tests -perm -u+x | head -1)"
    if [[ -n "$BINARY" ]]; then RUN="$BINARY"; else RUN="ctest --test-dir $COPY/build --output-on-failure"; fi
fi
echo "tests: $RUN"
bash -c "$RUN" > "$OUT/tests.log" 2>&1
echo "tests exit code: $?" >> "$OUT/tests.log"

# =========================
# Report
# =========================
if ! ls "$OUT"/profiles/*.profraw > /dev/null 2>&1; then
    echo "Error: no profile written (were the tests run? see $OUT/tests.log)" >&2
    exit 2
fi
llvm-profdata merge -sparse "$OUT"/profiles/*.profraw -o "$OUT/merged.profdata"
# Every instrumented binary of the build (tests + the project's own executables / shared libraries)
OBJECTS=()
while IFS= read -r object; do
    if llvm-cov report "$object" -instr-profile="$OUT/merged.profdata" > /dev/null 2>&1; then
        [[ ${#OBJECTS[@]} -eq 0 ]] && OBJECTS+=("$object") || OBJECTS+=("-object" "$object")
    fi
done < <(find "$COPY" -type f \( -perm -u+x -o -name '*.so' \) -newer "$OUT/.start" ! -name '*.sh' ! -name '*.py' 2> /dev/null)
if [[ ${#OBJECTS[@]} -eq 0 ]]; then
    echo "Error: no instrumented binary found in $COPY" >&2
    exit 2
fi
llvm-cov report "${OBJECTS[@]}" -instr-profile="$OUT/merged.profdata" -ignore-filename-regex="$IGNORE" > "$OUT/summary.txt" 2>&1
llvm-cov export "${OBJECTS[@]}" -instr-profile="$OUT/merged.profdata" -ignore-filename-regex="$IGNORE" > "$OUT/coverage.json" 2> /dev/null
if $HTML; then
    llvm-cov show "${OBJECTS[@]}" -instr-profile="$OUT/merged.profdata" -ignore-filename-regex="$IGNORE" -format=html \
        -show-branches=count -output-dir="$OUT/html" > /dev/null 2>&1
fi
echo "$COPY" > "$OUT/source.txt" # paths of the report are the ones of the copy
tail -3 "$OUT/summary.txt"
echo "out: $OUT"
