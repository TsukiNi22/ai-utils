#!/bin/bash
# Install / remove / show the xstyle pre-commit hook of a git repository.
#
# Usage: install_hook.sh <install|remove|status> [options]
#
# Options (install; the defaults are the ones of the hook):
#   --repo <dir>          repository (default: the current directory)
#   --scope <scope>       staged (only the staged lines, default) | files (whole staged files) | all (whole project)
#   --fail-on <level>     refuse the commit from: unforgivable | major (default) | minor | negligible | never
#   --warn-on <level>     warn from: minor (default) | negligible | none
#   --fix <mode>          none (default) | safe (auto fixes of the fully staged files, re-added before the check)
#   --missing <action>    xstyle not installed: warn (default) | fail | ignore
#   --output <mode>       auto (compact under Claude Code, default) | human | rtk
#   --extra "<args>"      more xstyle options (-l cpp,py -e build -i CPP-AUTO ...)
#   --shared              versioned hook: .githooks/pre-commit + core.hooksPath (the whole team gets it)
#   --link                symlink to the template (follows the updates of the skills repository) instead of a copy
#   --force               replace a hook that is not this one without keeping it (default: kept as pre-commit.local)
#   -h, --help            show this help

set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE="$SELF_DIR/../templates/pre-commit"
MARKER="# xstyle pre-commit hook"
REPO="."
SHARED=false
LINK=false
FORCE=false
COMMAND=""
declare -A CONFIG=()

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
    sed -n '2,22p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

error() {
    echo "Error: $*" >&2
    exit 1
}

# Value checked against its allowed list, stored for the config block of the hook
set_config() {
    local name="$1" value="$2" allowed="$3"
    [[ " $allowed " == *" $value "* ]] || error "--${name,,} must be one of: $allowed (got '$value')"
    CONFIG[$name]="$value"
}

# =========================
# Parse arguments
# =========================
while [[ $# -gt 0 ]]; do
    case "$1" in
        install|remove|status)
            [[ -n "$COMMAND" ]] && error "only one command allowed"
            COMMAND="$1"
            ;;
        --repo|--scope|--fail-on|--warn-on|--fix|--missing|--output|--extra)
            [[ $# -lt 2 ]] && error "$1 requires a value"
            case "$1" in
                --repo) REPO="$2" ;;
                --scope) set_config SCOPE "$2" "staged files all" ;;
                --fail-on) set_config FAIL_ON "$2" "unforgivable major minor negligible never" ;;
                --warn-on) set_config WARN_ON "$2" "minor negligible none" ;;
                --fix) set_config FIX "$2" "none safe" ;;
                --missing) set_config MISSING "$2" "warn fail ignore" ;;
                --output) set_config OUTPUT "$2" "auto human rtk" ;;
                --extra) [[ "$2" != *'"'* && "$2" != *'$'* && "$2" != *'`'* ]] || error "--extra can't contain \", \$ or \`"
                    CONFIG[EXTRA_ARGS]="$2" ;;
            esac
            shift
            ;;
        --shared) SHARED=true ;;
        --link) LINK=true ;;
        --force) FORCE=true ;;
        -h|--help) usage 0 ;;
        *) error "unknown argument '$1' (see --help)" ;;
    esac
    shift
done
[[ -z "$COMMAND" ]] && usage 1
command -v git > /dev/null 2>&1 || error "git is required"
ROOT="$(git -C "$REPO" rev-parse --show-toplevel 2> /dev/null)" || error "not a git repository: $REPO"
cd "$ROOT"
if $SHARED; then HOOKS="$ROOT/.githooks"; else HOOKS="$(cd "$(git rev-parse --git-common-dir)" && pwd)/hooks"; fi
CURRENT_PATH="$(git config --get core.hooksPath || true)"
[[ "$COMMAND" != "install" ]] && [[ -n "$CURRENT_PATH" ]] && HOOKS="$(cd "$ROOT" && realpath -m "$CURRENT_PATH")"
HOOK="$HOOKS/pre-commit"

is_ours() {
    [[ -f "$HOOK" ]] && grep -q "^$MARKER" "$HOOK"
}

# Config block of the hook rewritten with the chosen values (the other lines kept)
write_config() {
    local name tmp
    for name in "${!CONFIG[@]}"; do
        tmp="$(mktemp)"
        NAME="$name" VALUE="${CONFIG[$name]}" awk '
            BEGIN { name = ENVIRON["NAME"]; value = ENVIRON["VALUE"] }
            /^# >>> config/ { inside = 1 } /^# <<< config/ { inside = 0 }
            inside && index($0, name "=\"") == 1 && name != "XSTYLE_BIN" { sub(/="[^"]*"/, "=\"" value "\"") }
            { print }' "$HOOK" > "$tmp"
        cat "$tmp" > "$HOOK"
        rm -f "$tmp"
    done
}

# =========================
# Commands
# =========================
case "$COMMAND" in
    status)
        echo "Repository: $ROOT"
        echo "Hooks:      $HOOKS${CURRENT_PATH:+ (core.hooksPath=$CURRENT_PATH)}"
        if is_ours; then
            state="installed ($([ -L "$HOOK" ] && echo "symlink to the template" || echo copy))"
            [[ -x "$HOOKS/pre-commit.local" ]] && state="$state, runs pre-commit.local first"
            echo "Hook:       $state"
            sed -n '/^# >>> config/,/^# <<< config/p' "$HOOK" | grep -v "^#" | sed 's/ *#.*//; s/^/  /'
        elif [[ -e "$HOOK" ]]; then echo "Hook:       another pre-commit hook (not xstyle's)"
        else echo "Hook:       not installed"
        fi
        if command -v xstyle > /dev/null 2>&1; then echo "xstyle:     $(xstyle --version)"
        else echo "xstyle:     not installed (the hook only warns; install: $(remote_setup https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh install xstyle))"
        fi
        ;;

    install)
        command mkdir -p "$HOOKS"
        if [[ -e "$HOOK" ]] && ! is_ours; then
            if $FORCE; then rm -f "$HOOK"
            else
                mv "$HOOK" "$HOOKS/pre-commit.local"
                echo "  existing hook kept: $HOOKS/pre-commit.local (run before the xstyle check)"
            fi
        fi
        rm -f "$HOOK"
        if $LINK; then
            [[ ${#CONFIG[@]} -eq 0 ]] || error "--link uses the template as is: no config option with it (or a copy)"
            ln -s "$(realpath "$TEMPLATE")" "$HOOK"
        else
            cp "$TEMPLATE" "$HOOK"
            write_config
        fi
        chmod +x "$TEMPLATE" "$HOOK"
        if $SHARED; then
            git config core.hooksPath .githooks
            echo "  core.hooksPath=.githooks: commit .githooks/ so the team gets it (each clone runs: git config core.hooksPath .githooks)"
        elif [[ -n "$CURRENT_PATH" ]]; then
            echo "  warning: core.hooksPath=$CURRENT_PATH: git ignores $HOOKS (--shared, or git config --unset core.hooksPath)"
        fi
        echo "  installed $HOOK"
        command -v xstyle > /dev/null 2>&1 || echo "  warning: xstyle is not installed: the hook only warns (install: $(remote_setup https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh install xstyle))"
        ;;

    remove)
        if ! is_ours; then
            echo "  no xstyle hook in $HOOKS"
            exit 0
        fi
        rm -f "$HOOK"
        [[ -e "$HOOKS/pre-commit.local" ]] && mv "$HOOKS/pre-commit.local" "$HOOK" && echo "  previous hook restored: $HOOK"
        if [[ "$CURRENT_PATH" = ".githooks" ]] && [[ -z "$(ls -A "$HOOKS" 2> /dev/null)" ]]; then
            git config --unset core.hooksPath
            rmdir "$HOOKS"
        fi
        echo "  removed the xstyle hook of $ROOT"
        ;;
esac
