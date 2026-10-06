#!/bin/bash
# Install / remove the skills (and the tools) of this repository for Claude Code.
#
# Usage: ./setup.sh <command> [name...] [options]
#        curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- <command> ...
#
# Commands:
#   install [name...]    install the given skills / tools and their requirements (default: all)
#   remove  [name...]    remove the given skills / tools (default: all the skills & tools of this repo)
#   update               pull the last version of the repository (the symlinks follow, the tools are rebuilt)
#   list                 list the skills and the tools available in this repo
#   status               show which skills and tools are installed
#   context <command>    global context (CLAUDE.md, RTK.md, hooks, rtk) of the 'context' branch:
#                        install | remove | update | status [options], see its README
#   hook [command]       xstyle pre-commit hook of the git repository of the current directory (not global):
#                        install (default) | remove | status [--repo <dir>] [--fail-on major] [--warn-on minor]
#                        [--scope staged|files|all] [--fix none|safe] [--missing warn|fail|ignore] [--shared] ...
#                        (git/pre-commit/scripts/install_hook.sh --help for every option)
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
#   --purge              with remove: also delete the managed clone (curl/wget mode)
#   -h, --help           show this help
#
# Run without a clone (curl/wget), the repository is cloned/updated into $SKILLS_HOME
# (default: ~/.local/share/tsukini-skills) and the script runs from there.

set -euo pipefail

REMOTE="${SKILLS_REMOTE:-https://github.com/TsukiNi22/skills.git}"
SKILLS_HOME="${SKILLS_HOME:-$HOME/.local/share/tsukini-skills}"

# =========================
# Piped mode (curl/wget): work from a managed clone
# =========================
SELF="${BASH_SOURCE[0]:-}"
if [ -z "$SELF" ] || [ ! -f "$SELF" ] || ! ls "$(dirname "$SELF")"/*/*/SKILL.md > /dev/null 2>&1; then
    command -v git > /dev/null 2>&1 || { echo "Error: git is required" >&2; exit 1; }
    if [ -d "$SKILLS_HOME/.git" ]; then
        git -C "$SKILLS_HOME" pull -q --ff-only || echo "warning: update of $SKILLS_HOME failed, using the local version" >&2
    else
        command mkdir -p "$(dirname "$SKILLS_HOME")"
        git clone -q "$REMOTE" "$SKILLS_HOME"
    fi
    exec bash "$SKILLS_HOME/setup.sh" "$@"
fi

REPO="$(cd "$(dirname "$SELF")" && pwd)"

# =========================
# Global context (branch 'context'): delegate to its own setup.sh
# =========================
if [ "${1:-}" = "context" ]; then
    shift
    CTX="$(mktemp)"
    trap 'rm -f "$CTX"' EXIT
    if git -C "$REPO" fetch -q origin context 2> /dev/null && git -C "$REPO" show origin/context:setup.sh > "$CTX" 2> /dev/null; then :
    else curl -fsSL "https://raw.githubusercontent.com/TsukiNi22/skills/context/setup.sh" -o "$CTX"; fi
    bash "$CTX" "$@"
    exit $?
fi
# =========================
# pre-commit hook of the current repository (skill pre-commit)
# =========================
if [ "${1:-}" = "hook" ]; then
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

# =========================
# Helpers
# =========================
usage() {
    sed -n '2,35p' "$REPO/setup.sh" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

error() {
    echo "Error: $*" >&2
    exit 1
}

# Skills are stored as <category>/<skill>/SKILL.md (routers/, cpp/, docs/...), installed flat by name
available() {
    for file in "$REPO"/*/*/SKILL.md; do
        [ -f "$file" ] && basename "$(dirname "$file")"
    done | sort
}

# Path of a skill in the repository from its name (empty when unknown)
skill_dir() {
    for dir in "$REPO"/*/"$1"; do
        [ -f "$dir/SKILL.md" ] && { echo "$dir"; return; }
    done
}

description() {
    sed -n 's/^description: *//p' "$(skill_dir "$1")/SKILL.md" | head -1 | cut -c1-90
}

# Tools are stored as <category>/<tool>/tool.txt (one line: description), built into <prefix>/bin/<tool>
tools() {
    for file in "$REPO"/*/*/tool.txt; do
        [ -f "$file" ] && basename "$(dirname "$file")"
    done | sort
}

tool_dir() {
    for dir in "$REPO"/*/"$1"; do
        [ -f "$dir/tool.txt" ] && { echo "$dir"; return; }
    done
}

# true if <prefix>/bin/<tool> is this tool (answers "<tool> <version>" to --version)
tool_installed() {
    [ -x "$PREFIX/bin/$1" ] && "$PREFIX/bin/$1" --version 2> /dev/null | grep -q "^$1 "
}

# Build with CMake (Optimized) then install into the prefix, skipped (not fatal) when it can't be built
install_tool() {
    local tool="$1" dir build log
    dir="$(tool_dir "$tool")"
    build="$TOOLS_BUILD/$tool"
    log="$TOOLS_BUILD/$tool.log"
    for cmd in cmake clang++; do
        command -v "$cmd" > /dev/null 2>&1 || { echo "  skip $tool: $cmd is required to build it"; return 0; }
    done
    command mkdir -p "$TOOLS_BUILD"
    echo "  building $tool (log: $log)..."
    if ! cmake -S "$dir" -B "$build" -DCMAKE_BUILD_TYPE=Optimized > "$log" 2>&1; then
        if grep -q "utils" "$log"; then
            echo "  skip $tool: libutils is required (install: curl -fsSL https://raw.githubusercontent.com/TsukiNi22/libutils/main/setup.sh | bash)"
        else
            echo "  skip $tool: CMake configuration failed, see $log"
        fi
        return 0
    fi
    if ! cmake --build "$build" --parallel "$(nproc 2> /dev/null || echo 2)" >> "$log" 2>&1; then
        echo "  skip $tool: build failed, see $log"
        return 0
    fi
    if ! cmake --install "$build" --prefix "$PREFIX" >> "$log" 2>&1; then
        echo "  skip $tool: can't install into $PREFIX (--prefix <dir> to change it), see $log"
        return 0
    fi
    echo "  installed $tool -> $PREFIX/bin/$tool"
    install_completion "$tool"
    TOOLS_CHANGED=true
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
        [ -f "$dir/_$tool" ] && list="$list $tool"
    done
    list="${list# }"
    [ -f "$ZSHRC" ] || [ -n "$list" ] || return 0
    tmp="$(mktemp)"
    # Without the old block and the empty lines at the end
    if [ -f "$ZSHRC" ]; then
        sed '/^# >>> tsukini-skills completion >>>$/,/^# <<< tsukini-skills completion <<<$/d' "$ZSHRC" \
            | sed -e :a -e '/^\n*$/{$d;N;ba' -e '}' > "$tmp"
    fi
    if [ -n "$list" ]; then
        [ -s "$tmp" ] && echo >> "$tmp"
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
    if [ -L "$dest" ]; then
        [ "$(readlink -f "$dest")" = "$(readlink -f "$(skill_dir "$1")")" ]
    else
        [ -f "$dest/.installed-from" ] && [ "$(cat "$dest/.installed-from")" = "$REPO" ]
    fi
}

# =========================
# Parse arguments
# =========================
while [ $# -gt 0 ]; do
    case "$1" in
        install|remove|update|list|status)
            [ -n "$COMMAND" ] && error "only one command allowed"
            COMMAND="$1"
            ;;
        --project)
            [ $# -lt 2 ] && error "--project requires a directory"
            [ -d "$2" ] || error "directory not found: $2"
            TARGET="$(cd "$2" && pwd)/.claude/skills"
            shift
            ;;
        --prefix)
            [ $# -lt 2 ] && error "--prefix requires a directory"
            PREFIX="$(command mkdir -p "$2" && cd "$2" && pwd)"
            shift
            ;;
        --copy) MODE="copy" ;;
        --force) FORCE=true ;;
        --purge) PURGE=true ;;
        -h|--help) usage 0 ;;
        -*) error "unknown option '$1'" ;;
        *) SKILLS+=("$1") ;;
    esac
    shift
done

[ -z "$COMMAND" ] && usage 1

# Names: skills or tools (none = every skill and every tool)
if [ ${#SKILLS[@]} -eq 0 ]; then
    mapfile -t SKILLS < <(available)
    mapfile -t TOOLS < <(tools)
else
    NAMES=("${SKILLS[@]}")
    SKILLS=()
    for name in "${NAMES[@]}"; do
        if [ -n "$(skill_dir "$name")" ]; then SKILLS+=("$name")
        elif [ -n "$(tool_dir "$name")" ]; then TOOLS+=("$name")
        else error "unknown skill or tool '$name' (see: $0 list)"
        fi
    done
fi

# Add the skills required by the selected ones (<skill>/requires.txt), only for install
if [ "$COMMAND" = "install" ]; then
    i=0
    while [ $i -lt ${#SKILLS[@]} ]; do
        req="$(skill_dir "${SKILLS[$i]}")/requires.txt"
        if [ -f "$req" ]; then
            for dep in $(cat "$req"); do
                [ -n "$(skill_dir "$dep")" ] || error "'${SKILLS[$i]}' requires an unknown skill '$dep'"
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
            if [ -L "$dest" ] && is_ours "$skill"; then state="installed (symlink)"
            elif [ -e "$dest" ] && is_ours "$skill"; then state="installed (copy)"
            elif [ -e "$dest" ]; then state="conflict (another skill with this name)"
            else state="not installed"
            fi
            printf "  %-20s %s\n" "$skill" "$state"
        done
        echo "Tools: $PREFIX/bin"
        for tool in $(tools); do
            if tool_installed "$tool"; then
                state="installed ($("$PREFIX/bin/$tool" --version 2> /dev/null))"
                [ -f "$COMPLETION_DATA/bash-completion/completions/$tool" ] && state="$state, completion: bash"
                [ -f "$COMPLETION_DATA/zsh/site-functions/_$tool" ] && state="$state, zsh"
                [ -f "${XDG_CONFIG_HOME:-$HOME/.config}/fish/completions/$tool.fish" ] && state="$state, fish"
            elif [ -e "$PREFIX/bin/$tool" ]; then state="conflict (another program with this name)"
            else state="not installed"
            fi
            printf "  %-20s %s\n" "$tool" "$state"
        done
        ;;

    install)
        command mkdir -p "$TARGET"
        for skill in "${SKILLS[@]}"; do
            dest="$TARGET/$skill"
            if [ -e "$dest" ] || [ -L "$dest" ]; then
                if is_ours "$skill" || $FORCE; then
                    rm -rf "$dest"
                else
                    echo "  skip $skill: $dest already exists (use --force to replace it)"
                    continue
                fi
            fi
            if [ "$MODE" = "link" ]; then
                ln -s "$(skill_dir "$skill")" "$dest"
            else
                cp -r "$(skill_dir "$skill")" "$dest"
                echo "$REPO" > "$dest/.installed-from"
            fi
            find "$(skill_dir "$skill")" -path '*/scripts/*' \( -name '*.sh' -o -name '*.py' \) -exec chmod +x {} +
            echo "  installed $skill -> $dest ($MODE)"
        done
        for tool in "${TOOLS[@]}"; do
            install_tool "$tool"
        done
        ;;

    remove)
        for skill in "${SKILLS[@]}"; do
            dest="$TARGET/$skill"
            if [ ! -e "$dest" ] && [ ! -L "$dest" ]; then
                echo "  $skill: not installed"
            elif is_ours "$skill" || $FORCE; then
                rm -rf "$dest"
                echo "  removed $skill"
            else
                echo "  skip $skill: $dest doesn't come from this repo (use --force to remove it)"
            fi
        done
        for tool in "${TOOLS[@]}"; do
            if tool_installed "$tool" || { $FORCE && [ -e "$PREFIX/bin/$tool" ]; }; then
                rm -f "$PREFIX/bin/$tool"
                remove_completion "$tool"
                rm -rf "${TOOLS_BUILD:?}/$tool" "$TOOLS_BUILD/$tool.log"
                echo "  removed $tool ($PREFIX/bin/$tool)"
            elif [ -e "$PREFIX/bin/$tool" ]; then
                echo "  skip $tool: $PREFIX/bin/$tool is another program (use --force to remove it)"
            else
                echo "  $tool: not installed"
            fi
        done
        ;;
esac

if [ "$COMMAND" = "install" ] || [ "$COMMAND" = "update" ]; then
    reload_hint
fi

if [ "$COMMAND" = "remove" ] && $PURGE; then
    if [ "$REPO" = "$(cd "$SKILLS_HOME" 2>/dev/null && pwd)" ]; then
        rm -rf "$SKILLS_HOME"
        echo "  purged $SKILLS_HOME"
    else
        echo "  --purge ignored: $REPO is not the managed clone ($SKILLS_HOME)"
    fi
fi
