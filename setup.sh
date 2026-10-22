#!/bin/bash
# Install / remove the skills (and the tools) of this repository for Claude Code.
#
# Usage: ./setup.sh <command> [name...] [options]
#        curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- <command> ...
#        wget -qO- https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- <command> ...
#
# Commands:
#   install [name...]    install the given skills / tools / context and their requirements (default: all)
#   remove  [name...]    remove the given skills / tools / context (default: all of this repo)
#                        a name can also be a whole kind or category, nothing to list one by one:
#                          skills       every skill          tools        every tool
#                          context      the context          <category>/  every skill and tool of a category
#                                                                         (ex: cpp/, docs/, style/; also @cpp)
#   update               pull the last version of the repository (the symlinks follow, the tools are rebuilt)
#   list                 list the skills, the tools and the context available in this repo
#   status               show which skills, tools and context are installed
#   context <command>    only the context, with its own options: install | remove | update | status
#   hook [command]       xstyle pre-commit hook of the git repository of the current directory (not global):
#                        install (default) | remove | status [--repo <dir>] [--fail-on major] [--warn-on minor]
#                        [--scope staged|files|all] [--fix none|safe] [--missing warn|fail|ignore] [--shared] ...
#                        (git/pre-commit/scripts/install_hook.sh --help for every option)
#
# Context (context/, name `context`): the global instructions of Claude Code (CLAUDE.md, RTK.md, session hooks,
# rtk, sudo-askpass) installed in ~/.claude and ~/.local/bin (symlinks, the replaced files are saved and restored by
# remove). Part of the default `install` / `remove` / `status` (not with --project: it is global), `--no-context`
# skips it, `--no-rtk` / `--no-hooks` are given to it, see context/README.md.
#
# Tools (<category>/<tool>/tool.txt, ex: style/xstyle): C++ programs built with CMake (clang++, libutils)
# and installed in <prefix>/bin; a tool that can't be built is skipped with the reason, never fatal.
# Their completion is installed for bash (bash-completion), zsh (block in ~/.zshrc) and fish.
#
# Options:
#   --project <dir>      target <dir>/.claude/skills instead of ~/.claude/skills
#   --copy               copy the files instead of a symlink (default: symlink, edits are live)
#   --force              replace an already existing skill that doesn't come from this repo
#   --prefix <dir>       install prefix of the tools (default: ~/.local, binaries in <dir>/bin)
#   --no-context         leave the context aside (default install / remove / status handle it)
#   --no-rtk             context: don't install rtk
#   --no-hooks           context: don't touch ~/.claude/settings.json
#   --purge              with remove: also delete what is kept besides the installed files - the build trees / logs of the tools
#                        (tmp, cache) and the managed clone (curl/wget mode); without it a tool's remove deletes its binary only
#   --verbose            tools: stream the raw build output instead of the progress display
#   --quiet              tools: only the final line of each tool (and the failures)
#   -h, --help           show this help
#
# Build progress (tools, install / update): 4 steps (configure, build, install, completion + post-install), during
# the build the percentage, a bar, the current file, the elapsed time and an ETA (parsed from the output, which goes
# unchanged to the log <cache>/tsukini-skills/build/<tool>.log). On a terminal: one updating line (width-aware,
# colours unless NO_COLOR is set, the cursor is restored on Ctrl-C); otherwise (pipe, CI): a line per 10% and per step.
# A failure shows the step, the last 20 lines of the log and its path (the tool is skipped, never fatal).
#   SETUP_PROGRESS=plain|bar|off   force the display (default: bar on a terminal, plain otherwise; off: no progress)
#   SETUP_JOBS=N                   parallel jobs of the builds (default: the processors, lowered when there is
#                                  less than ~1.5 GB of free memory per job)
#
# Run without a clone (curl/wget), the repository is cloned/updated into $SKILLS_HOME
# (default: ~/.local/share/tsukini-skills) and the script runs from there.
#
# Works with bash 3.2 (the /bin/bash of macOS) and newer, on Linux and macOS (the BSD tools): no mapfile, no namerefs.

set -euo pipefail

REMOTE="${SKILLS_REMOTE:-https://github.com/TsukiNi22/ai-utils.git}"
SKILLS_HOME="${SKILLS_HOME:-$HOME/.local/share/tsukini-skills}"

# =========================
# Piped mode (curl/wget): work from a managed clone
# =========================
SELF="${BASH_SOURCE[0]:-}"
if [[ -z "$SELF" ]] || [[ ! -f "$SELF" ]] || ! ls "$(dirname "$SELF")"/*/*/SKILL.md > /dev/null 2>&1; then
    command -v git > /dev/null 2>&1 || { echo "Error: git is required" >&2; exit 1; }
    if [[ -d "$SKILLS_HOME/.git" ]]; then
        git -C "$SKILLS_HOME" pull -q --ff-only || echo "warning: update of $SKILLS_HOME failed, using the local version" >&2
    else
        command mkdir -p "$(dirname "$SKILLS_HOME")"
        git clone -q "$REMOTE" "$SKILLS_HOME"
    fi
    exec bash "$SKILLS_HOME/setup.sh" "$@"
fi

REPO="$(cd "$(dirname "$SELF")" && pwd)"

# =========================
# Global context (context/): delegate to its own setup.sh
# =========================
if [[ "${1:-}" = "context" ]]; then
    shift
    exec bash "$REPO/context/setup.sh" "$@"
fi
# =========================
# pre-commit hook of the current repository (skill pre-commit)
# =========================
if [[ "${1:-}" = "hook" ]]; then
    shift
    case "${1:-}" in
        install|remove|status|-h|--help) ;;
        *) set -- install "$@" ;;
    esac
    exec bash "$REPO/git/pre-commit/scripts/install_hook.sh" "$@"
fi
TARGET="$HOME/.claude/skills"
PREFIX="${SKILLS_PREFIX:-$HOME/.local}"
TOOLS_BUILD="${XDG_CACHE_HOME:-$HOME/.cache}/tsukini-skills/build"
COMPLETION_DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
ZSHRC="${ZDOTDIR:-$HOME}/.zshrc"
TOOLS_CHANGED=false
MODE="link"
FORCE=false
PURGE=false
COMMAND=""
SKILLS=()
TOOLS=()
CONTEXT=true       # the context is handled by default
CONTEXT_NAMED=false
CONTEXT_OPTS=()
PROJECT=false
PG_FORCE=""        # progress mode forced by --verbose / --quiet
JOBS=1
JOBS_NOTE=""

# =========================
# Helpers
# =========================
# Absolute path with its symlinks resolved (readlink -f is missing on the BSD / macOS tools before 12.3)
real_path() {
    local path="$1" link
    while [[ -L "$path" ]]; do
        link="$(readlink "$path")"
        case "$link" in
            /*) path="$link" ;;
            *) path="$(dirname "$path")/$link" ;;
        esac
    done
    if [[ -d "$path" ]]; then (cd -P "$path" && pwd)
    else echo "$(cd -P "$(dirname "$path")" && pwd)/$(basename "$path")"
    fi
}

# Number of processors (nproc: Linux, sysctl: macOS / BSD)
cpus() {
    nproc 2> /dev/null || sysctl -n hw.ncpu 2> /dev/null || echo 2
}

# Command running a remote setup.sh, with the download tool available: curl, else wget, else python3, else curl
remote_setup() {
    local url="$1" fetch
    shift
    if command -v curl > /dev/null 2>&1; then fetch="curl -fsSL $url"
    elif command -v wget > /dev/null 2>&1; then fetch="wget -qO- $url"
    elif command -v python3 > /dev/null 2>&1; then
        fetch="python3 -c 'import sys, urllib.request; sys.stdout.buffer.write(urllib.request.urlopen(sys.argv[1]).read())' $url"
    else fetch="curl -fsSL $url"
    fi
    echo "$fetch | bash${*:+ -s -- $*}"
}

usage() {
    sed -n '2,/^$/p' "$REPO/setup.sh" | sed '$d' | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

error() {
    echo "Error: $*" >&2
    exit 1
}

# Skills are stored as <category>/<skill>/SKILL.md (routers/, cpp/, docs/...), installed flat by name
available() {
    for file in "$REPO"/*/*/SKILL.md; do
        [[ -f "$file" ]] && basename "$(dirname "$file")"
    done | sort
}

# Path of a skill in the repository from its name (empty when unknown)
skill_dir() {
    for dir in "$REPO"/*/"$1"; do
        [[ -f "$dir/SKILL.md" ]] && { echo "$dir"; return; }
    done
}

description() {
    sed -n 's/^description: *//p' "$(skill_dir "$1")/SKILL.md" | head -1 | cut -c1-90
}

# Tools are stored as <category>/<tool>/tool.txt (one line: description), built into <prefix>/bin/<tool>
tools() {
    for file in "$REPO"/*/*/tool.txt; do
        [[ -f "$file" ]] && basename "$(dirname "$file")"
    done | sort
}

tool_dir() {
    for dir in "$REPO"/*/"$1"; do
        [[ -f "$dir/tool.txt" ]] && { echo "$dir"; return; }
    done
}

# true if <prefix>/bin/<tool> is this tool (answers "<tool> <version>" to --version)
tool_installed() {
    [[ -x "$PREFIX/bin/$1" ]] && "$PREFIX/bin/$1" --version 2> /dev/null | grep -q "^$1 "
}

# =========================
# Progress display of the builds
# =========================
# Modes (PG_MODE): bar = one updating line (interactive terminal), plain = a line per 10% / per step (pipes, CI),
# verbose = the raw output streamed, quiet = the final line of each tool only, off = no progress (messages only)
PG_MODE="plain"
PG_TOTAL=4          # steps of a tool: configure, build, install, completion + post-install
PG_N=0              # current step
PG_NAME=""          # its name
PG_PCT=-1           # percentage parsed from the output of the running command (-1: unknown)
PG_LAST=""          # its last line of output
PG_PID=""           # process running the step
PG_COLS=80
C_HEAD=""
C_BAR=""
C_OK=""
C_ERR=""
C_DIM=""
C_OFF=""

# Pick the mode (--verbose / --quiet, else SETUP_PROGRESS=plain|bar|off, else bar on a terminal) and the colours
progress_init() {
    local colors=0
    if [[ -n "$PG_FORCE" ]]; then PG_MODE="$PG_FORCE"
    else
        case "${SETUP_PROGRESS:-auto}" in
            plain|bar|off) PG_MODE="$SETUP_PROGRESS" ;;
            *) if [[ -t 1 ]]; then PG_MODE="bar"; else PG_MODE="plain"; fi ;;
        esac
    fi
    PG_COLS="${COLUMNS:-}"
    [[ "$PG_COLS" =~ ^[0-9]+$ ]] || PG_COLS="$(tput cols 2> /dev/null || echo 80)"
    [[ "$PG_COLS" =~ ^[0-9]+$ ]] && [[ "$PG_COLS" -ge 20 ]] || PG_COLS=80
    if [[ -t 1 ]] && [[ -z "${NO_COLOR:-}" ]] && [[ "${TERM:-dumb}" != "dumb" ]]; then
        colors="$(tput colors 2> /dev/null || echo 0)"
        [[ "$colors" =~ ^[0-9]+$ ]] || colors=0
        if [[ "$colors" -ge 8 ]]; then
            C_HEAD=$'\033[36m' C_BAR=$'\033[32m' C_OK=$'\033[32m' C_ERR=$'\033[31m' C_DIM=$'\033[2m' C_OFF=$'\033[0m'
        fi
    fi
    trap progress_exit EXIT
    trap progress_abort INT TERM
}

# Restore the cursor whatever the way out
progress_exit() {
    [[ "$PG_MODE" = "bar" ]] && printf '\033[?25h' || true
}

# Ctrl-C / TERM: stop the running step (the processes started by this script only), restore the terminal
progress_abort() {
    trap '' INT TERM
    [[ -n "$PG_PID" ]] && kill_tree "$PG_PID"
    [[ "$PG_MODE" = "bar" ]] && printf '\r\033[K\033[?25h' || true
    echo "  interrupted"
    exit 130
}

# Kill a process and its descendants (the background jobs of a script ignore SIGINT)
kill_tree() {
    local child
    for child in $(pgrep -P "$1" 2> /dev/null); do kill_tree "$child"; done
    kill "$1" 2> /dev/null || true
}

# 75 -> 1m15s
fmt_time() {
    local s="$1"
    if [[ "$s" -ge 3600 ]]; then printf '%dh%02dm' $((s / 3600)) $((s % 3600 / 60))
    elif [[ "$s" -ge 60 ]]; then printf '%dm%02ds' $((s / 60)) $((s % 60))
    else printf '%ds' "$s"
    fi
}

# Percentage + current line from the end of the log: "[ 42%] Building ..." (Makefile), "[12/345] ..." (Ninja),
# "step 2/5" (post-install scripts), else any "NN%"; the percentage stays unknown (-1) when none is printed
progress_parse() {
    local chunk line pct
    local re_make='^\[ *([0-9]+)%\]' re_ninja='^\[([0-9]+)/([0-9]+)\]' re_step='[Ss][Tt][Ee][Pp] ([0-9]+)/([0-9]+)' re_pct='(^|[^0-9.])([0-9]{1,3})%'
    chunk="$(tail -c 3000 "$1" 2> /dev/null | tr '\r' '\n' | tr -d '\000-\010\013-\037' || true)"
    while IFS= read -r line; do
        [[ -z "${line// /}" ]] && continue
        pct=""
        if [[ "$line" =~ $re_make ]]; then pct="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ $re_ninja ]] && [[ "${BASH_REMATCH[2]}" -gt 0 ]]; then pct=$((BASH_REMATCH[1] * 100 / BASH_REMATCH[2]))
        elif [[ "$line" =~ $re_step ]] && [[ "${BASH_REMATCH[2]}" -gt 0 ]]; then pct=$((BASH_REMATCH[1] * 100 / BASH_REMATCH[2]))
        elif [[ "$line" =~ $re_pct ]]; then pct="${BASH_REMATCH[2]}"
        fi
        [[ -n "$pct" ]] && { [[ "$pct" -gt 100 ]] && pct=100; PG_PCT="$pct"; }
        [[ "$line" = "["* ]] && line="${line#*] }"
        if [[ "$line" = *" object "* ]]; then line="${line%% object *} ${line##*/}"; fi
        PG_LAST="$line"
    done <<< "$chunk"
}

# [#####...............]
progress_bar() {
    local filled=$((PG_PCT * 20 / 100)) i out=""
    for ((i = 0; i < 20; i++)); do
        if [[ "$i" -lt "$filled" ]]; then out="$out#"; else out="$out."; fi
    done
    printf '%s' "$out"
}

# Live line (bar mode), cut to the width of the terminal
progress_draw() {
    local elapsed="$1" tick="$2" head mid time eta="" plain colored room last spin
    local frames='|/-\' pct="$PG_PCT"
    head="[$PG_N/$PG_TOTAL] $PG_NAME"
    spin="${frames:$((tick % 4)):1}"
    time="$(fmt_time "$elapsed")"
    if [[ "$pct" -ge 0 ]]; then
        mid="[$(progress_bar)] $(printf '%3d' "$pct")%"
        [[ "$pct" -ge 10 && "$pct" -lt 100 ]] && eta=" eta ~$(fmt_time $((elapsed * (100 - pct) / pct)))"
        colored="  $C_HEAD$head$C_OFF $C_BAR$mid$C_OFF $time$eta"
    else
        mid="$spin"
        colored="  $C_HEAD$head$C_OFF $mid $time"
    fi
    plain="  $head $mid $time$eta"
    if [[ "${#plain}" -ge "$PG_COLS" ]]; then # narrow terminal: no bar, no ETA, the head cut if needed
        [[ "$pct" -ge 0 ]] && mid="${pct}%"
        plain="  $head $mid $time"
        [[ "${#plain}" -ge "$PG_COLS" ]] && head="${head:0:$((PG_COLS - ${#plain} + ${#head} - 2))}~"
        plain="  $head $mid $time"
        colored="  $C_HEAD$head$C_OFF $C_BAR$mid$C_OFF $time"
    fi
    room=$((PG_COLS - 1 - ${#plain} - 2))
    last="$PG_LAST"
    if [[ "$room" -ge 8 && -n "$last" ]]; then
        [[ "${#last}" -gt "$room" ]] && last="${last:0:$((room - 1))}~"
        colored="$colored  $C_DIM$last$C_OFF"
    fi
    printf '\r\033[K\033[?25l%s' "$colored"
}

# Run a command of a step with its output in the log, showing its progress according to the mode;
# PG_N / PG_NAME name the step; returns the status of the command
progress_run() {
    local log="$1" rc=0 start now elapsed tick=0 bucket=0 last_bucket=0 beat=30 mark
    shift
    start="$(date +%s)"
    PG_PCT=-1
    PG_LAST=""
    case "$PG_MODE" in
        verbose)
            echo "  [$PG_N/$PG_TOTAL] $PG_NAME"
            if "$@" 2>&1 | tee -a "$log"; then rc=0; else rc="${PIPESTATUS[0]}"; fi
            ;;
        quiet|off)
            "$@" >> "$log" 2>&1 || rc=$?
            ;;
        *)
            "$@" >> "$log" 2>&1 &
            PG_PID=$!
            while kill -0 "$PG_PID" 2> /dev/null; do
                progress_parse "$log"
                now="$(date +%s)"
                elapsed=$((now - start))
                if [[ "$PG_MODE" = "bar" ]]; then
                    progress_draw "$elapsed" "$tick"
                else
                    bucket=$((PG_PCT / 10))
                    if [[ "$PG_PCT" -ge 0 && "$bucket" -gt "$last_bucket" && "$bucket" -lt 10 ]]; then
                        echo "  [$PG_N/$PG_TOTAL] $PG_NAME $((bucket * 10))% ($(fmt_time "$elapsed"))${PG_LAST:+ $PG_LAST}"
                        last_bucket="$bucket"
                    elif [[ "$PG_PCT" -lt 0 && "$elapsed" -ge "$beat" ]]; then
                        echo "  [$PG_N/$PG_TOTAL] $PG_NAME still running ($(fmt_time "$elapsed"))${PG_LAST:+ $PG_LAST}"
                        beat=$((beat + 30))
                    fi
                fi
                tick=$((tick + 1))
                sleep 0.25 2> /dev/null || sleep 1
            done
            wait "$PG_PID" || rc=$?
            PG_PID=""
            ;;
    esac
    now="$(date +%s)"
    progress_done "$rc" $((now - start))
    return "$rc"
}

# Permanent line of a finished step (replaces the live line)
progress_done() {
    local rc="$1" secs="$2" state="ok" color="$C_OK"
    [[ "$rc" -ne 0 ]] && { state="FAILED"; color="$C_ERR"; }
    case "$PG_MODE" in
        quiet|off|verbose) [[ "$PG_MODE" = "verbose" ]] || return 0 ;;
        bar) printf '\r\033[K\033[?25h' ;;
    esac
    echo "  $C_HEAD[$PG_N/$PG_TOTAL] $PG_NAME$C_OFF $color$state$C_OFF ($(fmt_time "$secs"))"
}

# A step of a tool: pg_step <n> <name> <log> <command...>
pg_step() {
    PG_N="$1"
    PG_NAME="$2"
    shift 2
    progress_run "$@"
}

# Free memory in MB (empty when unknown): MemAvailable on Linux, free + inactive + speculative pages on macOS
free_memory_mb() {
    local mb=""
    if [[ -r /proc/meminfo ]]; then
        mb="$(awk '/^MemAvailable:/ { print int($2 / 1024) }' /proc/meminfo 2> /dev/null || true)"
    elif command -v vm_stat > /dev/null 2>&1; then
        mb="$(vm_stat 2> /dev/null | awk '
            /page size of/ { for (i = 1; i <= NF; i++) if ($i == "of") size = $(i + 1) }
            /^Pages (free|inactive|speculative):/ { gsub(/\./, "", $NF); pages += $NF }
            END { if (size > 0) print int(pages * size / 1048576) }' || true)"
    fi
    echo "$mb"
}

# Parallel jobs of a build (JOBS, with a note in JOBS_NOTE): the processors, lowered when the free memory is
# under ~1.5 GB per job (clang runs out of memory otherwise), SETUP_JOBS=N overrides everything
choose_jobs() {
    local cpu mb max
    cpu="$(cpus)"
    JOBS="$cpu"
    JOBS_NOTE="$cpu cpus"
    if [[ "${SETUP_JOBS:-}" =~ ^[0-9]+$ ]] && [[ "$SETUP_JOBS" -ge 1 ]]; then
        JOBS="$SETUP_JOBS"
        JOBS_NOTE="SETUP_JOBS"
        return 0
    fi
    mb="$(free_memory_mb)"
    [[ "$mb" =~ ^[0-9]+$ ]] || return 0
    max=$((mb * 2 / 3072))
    [[ "$max" -lt 1 ]] && max=1
    JOBS_NOTE="$cpu cpus, $((mb / 1024)).$((mb % 1024 * 10 / 1024)) GB free"
    [[ "$max" -lt "$JOBS" ]] && JOBS="$max"
    return 0
}

# Failure of a step: what failed, the last lines of the log and its path (never fatal: the caller returns 0)
tool_failed() {
    local tool="$1" msg="$2" log="$3" t0="$4"
    echo "  skip $tool: $msg"
    [[ "$PG_MODE" = "quiet" ]] && return 0
    echo "  failed at step $PG_N/$PG_TOTAL ($PG_NAME) after $(fmt_time $(($(date +%s) - t0))), log: $log"
    if [[ "$PG_MODE" != "verbose" ]]; then
        echo "  last 20 lines:"
        tail -n 20 "$log" | sed 's/^/    | /'
    fi
}

# Build with CMake (Optimized) then install into the prefix, skipped (not fatal) when it can't be built;
# the 4 steps (configure, build, install, completion + post-install) show their progress, see "Progress display"
install_tool() {
    local tool="$1" dir build log t0 package hint
    dir="$(tool_dir "$tool")"
    build="$TOOLS_BUILD/$tool"
    log="$TOOLS_BUILD/$tool.log"
    for cmd in cmake clang++; do
        command -v "$cmd" > /dev/null 2>&1 || { echo "  skip $tool: $cmd is required to build it"; return 0; }
    done
    command mkdir -p "$TOOLS_BUILD"
    # Stale cache (the repository was moved / renamed): configure again from scratch
    if [[ -f "$build/CMakeCache.txt" ]] && ! grep -qx "CMAKE_HOME_DIRECTORY:INTERNAL=$dir" "$build/CMakeCache.txt"; then
        rm -rf "$build"
    fi
    : > "$log"
    t0="$(date +%s)"
    [[ "$PG_MODE" = "quiet" ]] || echo "  building $tool (log: $log)..."
    if ! pg_step 1 "configure" "$log" cmake -S "$dir" -B "$build" -DCMAKE_BUILD_TYPE=Optimized; then
        if grep -qE "Could not find a package configuration file.*utils|find_package\(utils|utilsConfig\.cmake" "$log"; then
            tool_failed "$tool" "libutils is required (install: $(remote_setup https://raw.githubusercontent.com/TsukiNi22/libutils/main/setup.sh))" "$log" "$t0"
        elif package="$(grep -oE 'provided by "[^"]+"' "$log" | head -1 | cut -d'"' -f2)" && [[ -n "$package" ]]; then
            case "$package" in
                nlohmann_json) hint="dnf: json-devel, apt: nlohmann-json3-dev" ;;
                tomlplusplus) hint="dnf: tomlplusplus-devel, apt: libtomlplusplus-dev" ;;
                *) hint="its development package" ;;
            esac
            tool_failed "$tool" "$package is required ($hint), see $log" "$log" "$t0"
        else
            tool_failed "$tool" "CMake configuration failed, see $log" "$log" "$t0"
        fi
        return 0
    fi
    choose_jobs
    [[ "$PG_MODE" = "quiet" ]] || echo "  parallel jobs: $JOBS ($JOBS_NOTE; SETUP_JOBS=N to force)"
    if ! pg_step 2 "build" "$log" cmake --build "$build" --parallel "$JOBS"; then
        tool_failed "$tool" "build failed, see $log" "$log" "$t0"
        return 0
    fi
    if ! pg_step 3 "install" "$log" cmake --install "$build" --prefix "$PREFIX"; then
        tool_failed "$tool" "can't install into $PREFIX (--prefix <dir> to change it), see $log" "$log" "$t0"
        return 0
    fi
    TOOLS_CHANGED=true
    # completion for the shells, then its own last step (ex: the Android app of claude-cluster): <tool dir>/post-install.sh <prefix>
    local completion
    completion="$(install_completion "$tool")"
    if [[ -x "$dir/post-install.sh" ]]; then
        if pg_step 4 "completion + post-install" "$log" "$dir/post-install.sh" "$PREFIX"; then
            [[ "$PG_MODE" = "quiet" ]] || { [[ -n "$completion" ]] && echo "$completion"; tail -n 1 "$log" | sed 's/^/  /'; }
        else
            echo "  $tool: its post-install failed, see $log"
            [[ "$PG_MODE" = "quiet" ]] || { tail -n 20 "$log" | sed 's/^/    | /'; }
        fi
    else
        PG_N=4
        PG_NAME="completion"
        progress_done 0 0
        [[ "$PG_MODE" = "quiet" ]] || { [[ -n "$completion" ]] && echo "$completion"; }
    fi
    echo "  installed $tool -> $PREFIX/bin/$tool (total $(fmt_time $(($(date +%s) - t0))))"
    case ":$PATH:" in
        *":$PREFIX/bin:"*) ;;
        *) echo "  warning: $PREFIX/bin is not in the PATH (add: export PATH=\"$PREFIX/bin:\$PATH\")" ;;
    esac
}

# Completion of a tool for the installed shells, generated by "<tool> --completion <shell>"
install_completion() {
    local tool="$1" bin="$PREFIX/bin/$1" dir shells="bash"
    "$bin" --completion bash > /dev/null 2>&1 || return 0
    dir="$COMPLETION_DATA/bash-completion/completions" # loaded on demand by bash-completion (open shells too)
    command mkdir -p "$dir" && "$bin" --completion bash > "$dir/$tool"
    if command -v zsh > /dev/null 2>&1; then
        dir="$COMPLETION_DATA/zsh/site-functions"
        command mkdir -p "$dir" && "$bin" --completion zsh > "$dir/_$tool"
        update_zshrc
        shells="$shells, zsh"
    fi
    if command -v fish > /dev/null 2>&1; then
        dir="${XDG_CONFIG_HOME:-$HOME/.config}/fish/completions" # loaded on demand by fish
        command mkdir -p "$dir" && "$bin" --completion fish > "$dir/$tool.fish"
        shells="$shells, fish"
    fi
    echo "  completion of $tool: $shells"
}

remove_completion() {
    local tool="$1"
    rm -f "$COMPLETION_DATA/bash-completion/completions/$tool" "$COMPLETION_DATA/zsh/site-functions/_$tool" \
        "${XDG_CONFIG_HOME:-$HOME/.config}/fish/completions/$tool.fish"
    update_zshrc
}

# Block of ~/.zshrc registering the completion of the installed tools (rewritten, removed when no tool is left)
update_zshrc() {
    local dir="$COMPLETION_DATA/zsh/site-functions" list="" tool tmp
    for tool in $(tools); do
        [[ -f "$dir/_$tool" ]] && list="$list $tool"
    done
    list="${list# }"
    [[ -f "$ZSHRC" ]] || [[ -n "$list" ]] || return 0
    tmp="$(mktemp)"
    # Without the old block and the empty lines at the end
    if [[ -f "$ZSHRC" ]]; then
        sed '/^# >>> tsukini-skills completion >>>$/,/^# <<< tsukini-skills completion <<<$/d' "$ZSHRC" \
            | awk '{ line[NR] = $0 } END { n = NR; while (n > 0 && line[n] == "") n--; for (i = 1; i <= n; i++) print line[i] }' > "$tmp"
    fi
    if [[ -n "$list" ]]; then
        [[ -s "$tmp" ]] && echo >> "$tmp"
        cat >> "$tmp" << ZSHBLOCK
# >>> tsukini-skills completion >>>
# Added by the skills setup.sh (remove this block to disable it): completion of $list
_ts_dir="\${XDG_DATA_HOME:-\$HOME/.local/share}/zsh/site-functions"
(( \${fpath[(Ie)\$_ts_dir]} )) || fpath=("\$_ts_dir" \$fpath)
if (( \${+functions[compdef]} )); then
    # compinit already done (oh-my-zsh, ...) -> only register the completions
    for _ts_tool in $list; do
        autoload -Uz "_\$_ts_tool" && compdef "_\$_ts_tool" "\$_ts_tool"
    done
else
    autoload -Uz compinit && compinit -i
fi
unset _ts_dir _ts_tool
# <<< tsukini-skills completion <<<
ZSHBLOCK
    fi
    cmp -s "$tmp" "$ZSHRC" 2> /dev/null || cat "$tmp" > "$ZSHRC"
    rm -f "$tmp"
}

# A child process can't reload the shell that started it: print the command for the current shell
reload_hint() {
    local shell list="" tool
    $TOOLS_CHANGED || return 0
    shell="$(ps -o comm= -p "$PPID" 2> /dev/null | tr -d ' -')"
    for tool in $(tools); do
        tool_installed "$tool" && list="$list $tool"
    done
    list="${list# }"
    echo
    case "$shell" in
        zsh)
            echo "Current zsh: run once (the new shells are ready):"
            echo "  rehash; fpath=(\${XDG_DATA_HOME:-~/.local/share}/zsh/site-functions \$fpath); for t in $list; do autoload -Uz _\$t && compdef _\$t \$t; done"
            ;;
        bash) echo "Current bash: run once 'hash -r' (the completion is loaded on demand by bash-completion)" ;;
        fish) ;;
        *) echo "Open a new shell (or rehash) to use: $list" ;;
    esac
}

# true if the installed skill comes from this repo (symlink to it or copy with the marker)
is_ours() {
    local dest="$TARGET/$1"
    if [[ -L "$dest" ]]; then
        [[ "$(real_path "$dest")" = "$(real_path "$(skill_dir "$1")")" ]]
    else
        [[ -f "$dest/.installed-from" ]] && [[ "$(cat "$dest/.installed-from")" = "$REPO" ]]
    fi
}

# =========================
# Parse arguments
# =========================
while [[ $# -gt 0 ]]; do
    case "$1" in
        install|remove|update|list|status)
            [[ -n "$COMMAND" ]] && error "only one command allowed"
            COMMAND="$1"
            ;;
        --project)
            [[ $# -lt 2 ]] && error "--project requires a directory"
            [[ -d "$2" ]] || error "directory not found: $2"
            TARGET="$(cd "$2" && pwd)/.claude/skills"
            PROJECT=true
            shift
            ;;
        --prefix)
            [[ $# -lt 2 ]] && error "--prefix requires a directory"
            PREFIX="$(command mkdir -p "$2" && cd "$2" && pwd)"
            shift
            ;;
        --copy) MODE="copy" ;;
        --force) FORCE=true ;;
        --purge) PURGE=true ;;
        --no-context) CONTEXT=false ;;
        --verbose) PG_FORCE="verbose" ;;
        --quiet) PG_FORCE="quiet" ;;
        --no-rtk|--no-hooks) CONTEXT_OPTS+=("$1") ;;
        -h|--help) usage 0 ;;
        -*) error "unknown option '$1'" ;;
        *) SKILLS+=("$1") ;;
    esac
    shift
done

[[ -z "$COMMAND" ]] && usage 1
progress_init

# Names: skills or tools (none = every skill and every tool)
if [[ ${#SKILLS[@]} -eq 0 ]]; then
    SKILLS=()
    while IFS= read -r name; do SKILLS+=("$name"); done < <(available)
    while IFS= read -r name; do TOOLS+=("$name"); done < <(tools)
    $PROJECT && CONTEXT=false    # the context is global, never in a project folder
else
    NAMES=("${SKILLS[@]}")
    SKILLS=()
    CONTEXT=false
    add_skill() { [[ " ${SKILLS[*]-} " == *" $1 "* ]] || SKILLS+=("$1"); } # added once
    add_tool() { [[ " ${TOOLS[*]-} " == *" $1 "* ]] || TOOLS+=("$1"); }
    for name in "${NAMES[@]}"; do
        if [[ "$name" = "context" ]]; then CONTEXT=true; CONTEXT_NAMED=true
        elif [[ "$name" = "skills" ]]; then
            for skill in $(available); do add_skill "$skill"; done
        elif [[ "$name" = "tools" ]]; then
            for tool in $(tools); do add_tool "$tool"; done
        elif [[ "$name" == */ ]] || [[ "$name" == @* ]]; then
            # a whole category: every skill and tool of <category>/ (cpp/, docs/, style/...)
            category="${name%/}"
            category="${category#@}"
            found=false
            for file in "$REPO/$category"/*/SKILL.md; do
                [[ -f "$file" ]] && { add_skill "$(basename "$(dirname "$file")")"; found=true; }
            done
            for file in "$REPO/$category"/*/tool.txt; do
                [[ -f "$file" ]] && { add_tool "$(basename "$(dirname "$file")")"; found=true; }
            done
            $found || error "unknown category '$category' (see: $0 list)"
        elif [[ -n "$(skill_dir "$name")" ]]; then add_skill "$name"
        elif [[ -n "$(tool_dir "$name")" ]]; then add_tool "$name"
        else error "unknown skill, tool or context '$name' (see: $0 list)"
        fi
    done
fi

# Add the skills required by the selected ones (<skill>/requires.txt), only for install
if [[ "$COMMAND" = "install" ]]; then
    i=0
    while [[ $i -lt ${#SKILLS[@]} ]]; do
        req="$(skill_dir "${SKILLS[$i]}")/requires.txt"
        if [[ -f "$req" ]]; then
            for dep in $(cat "$req"); do
                [[ -n "$(skill_dir "$dep")" ]] || error "'${SKILLS[$i]}' requires an unknown skill '$dep'"
                [[ " ${SKILLS[*]} " == *" $dep "* ]] || SKILLS+=("$dep")
            done
        fi
        i=$((i + 1))
    done
fi

# =========================
# Commands
# =========================
case "$COMMAND" in
    update)
        git -C "$REPO" pull --ff-only
        for tool in $(tools); do
            tool_installed "$tool" && install_tool "$tool"
        done
        ;;

    list)
        echo "Categories (install / remove <category>/ for all of it; skills / tools for every one of a kind):"
        for dir in "$REPO"/*/; do
            category="$(basename "$dir")"
            names=""
            for file in "$dir"*/SKILL.md "$dir"*/tool.txt; do
                [[ -f "$file" ]] && names="$names $(basename "$(dirname "$file")")"
            done
            [[ -n "$names" ]] && printf "  %-20s%s\n" "$category/" "$names"
        done
        echo "Skills:"
        for skill in $(available); do
            printf "  %-20s %s\n" "$skill" "$(description "$skill")"
        done
        echo "Tools:"
        for tool in $(tools); do
            printf "  %-20s %s\n" "$tool" "$(head -1 "$(tool_dir "$tool")/tool.txt" | cut -c1-90)"
        done
        ;;

    status)
        echo "Target: $TARGET"
        for skill in $(available); do
            dest="$TARGET/$skill"
            if [[ -L "$dest" ]] && is_ours "$skill"; then state="installed (symlink)"
            elif [[ -e "$dest" ]] && is_ours "$skill"; then state="installed (copy)"
            elif [[ -e "$dest" ]]; then state="conflict (another skill with this name)"
            else state="not installed"
            fi
            printf "  %-20s %s\n" "$skill" "$state"
        done
        echo "Tools: $PREFIX/bin"
        for tool in $(tools); do
            if tool_installed "$tool"; then
                state="installed ($("$PREFIX/bin/$tool" --version 2> /dev/null))"
                [[ -f "$COMPLETION_DATA/bash-completion/completions/$tool" ]] && state="$state, completion: bash"
                [[ -f "$COMPLETION_DATA/zsh/site-functions/_$tool" ]] && state="$state, zsh"
                [[ -f "${XDG_CONFIG_HOME:-$HOME/.config}/fish/completions/$tool.fish" ]] && state="$state, fish"
            elif [[ -e "$PREFIX/bin/$tool" ]]; then state="conflict (another program with this name)"
            else state="not installed"
            fi
            printf "  %-20s %s\n" "$tool" "$state"
        done
        ;;

    install)
        command mkdir -p "$TARGET"
        for skill in ${SKILLS[@]+"${SKILLS[@]}"}; do
            dest="$TARGET/$skill"
            if [[ -e "$dest" ]] || [[ -L "$dest" ]]; then
                if is_ours "$skill" || $FORCE; then
                    rm -rf "$dest"
                else
                    echo "  skip $skill: $dest already exists (use --force to replace it)"
                    continue
                fi
            fi
            if [[ "$MODE" = "link" ]]; then
                ln -s "$(skill_dir "$skill")" "$dest"
            else
                cp -r "$(skill_dir "$skill")" "$dest"
                echo "$REPO" > "$dest/.installed-from"
            fi
            find "$(skill_dir "$skill")" -path '*/scripts/*' \( -name '*.sh' -o -name '*.py' \) -exec chmod +x {} +
            echo "  installed $skill -> $dest ($MODE)"
        done
        for tool in ${TOOLS[@]+"${TOOLS[@]}"}; do
            install_tool "$tool"
        done
        ;;

    remove)
        for skill in ${SKILLS[@]+"${SKILLS[@]}"}; do
            dest="$TARGET/$skill"
            if [[ ! -e "$dest" ]] && [[ ! -L "$dest" ]]; then
                echo "  $skill: not installed"
            elif is_ours "$skill" || $FORCE; then
                rm -rf "$dest"
                echo "  removed $skill"
            else
                echo "  skip $skill: $dest doesn't come from this repo (use --force to remove it)"
            fi
        done
        for tool in ${TOOLS[@]+"${TOOLS[@]}"}; do
            if tool_installed "$tool" || { $FORCE && [[ -e "$PREFIX/bin/$tool" ]]; }; then
                rm -f "$PREFIX/bin/$tool"
                remove_completion "$tool"
                echo "  removed $tool ($PREFIX/bin/$tool)"
            elif [[ -e "$PREFIX/bin/$tool" ]]; then
                echo "  skip $tool: $PREFIX/bin/$tool is another program (use --force to remove it)"
            else
                echo "  $tool: not installed"
            fi
            # the build tree and its log (the cache of the next install): only with --purge
            if $PURGE && { [[ -e "$TOOLS_BUILD/$tool" ]] || [[ -e "$TOOLS_BUILD/$tool.log" ]]; }; then
                rm -rf "${TOOLS_BUILD:?}/$tool" "$TOOLS_BUILD/$tool.log"
                echo "  purged the build cache of $tool ($TOOLS_BUILD/$tool)"
            fi
        done
        ;;
esac

if [[ "$COMMAND" = "install" ]] || [[ "$COMMAND" = "update" ]]; then
    reload_hint
fi

# =========================
# Context (context/setup.sh): same command, only when it is selected
# =========================
if $CONTEXT && [[ "$COMMAND" != "update" ]]; then
    $PROJECT && $CONTEXT_NAMED && echo "  note: the context is global, --project is ignored for it"
    case "$COMMAND" in
        list)
            echo "Context: (installed in ~/.claude and ~/.local/bin by 'install context')"
            while IFS=: read -r src dst; do printf "  %-34s %s\n" "$src" "$dst"; done < <(bash "$REPO/context/setup.sh" list)
            ;;
        *)
            echo "Context:"
            ctx_opts=()
            [[ "$MODE" = "copy" ]] && ctx_opts+=(--copy)
            [[ "$COMMAND" = "install" ]] && ctx_opts+=(${CONTEXT_OPTS[@]+"${CONTEXT_OPTS[@]}"})
            bash "$REPO/context/setup.sh" "$COMMAND" ${ctx_opts[@]+"${ctx_opts[@]}"}
            ;;
    esac
fi

if [[ "$COMMAND" = "remove" ]] && $PURGE; then
    if [[ "$REPO" = "$(cd "$SKILLS_HOME" 2>/dev/null && pwd)" ]]; then
        rm -rf "$SKILLS_HOME"
        echo "  purged $SKILLS_HOME"
    else
        echo "  --purge ignored: $REPO is not the managed clone ($SKILLS_HOME)"
    fi
fi
