#!/bin/bash
# Install / remove the skills of this repository for Claude Code.
#
# Usage: ./setup.sh <command> [skill...] [options]
#        curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- <command> ...
#
# Commands:
#   install [skill...]   install the given skills and their requirements (default: all)
#   remove  [skill...]   remove the given skills (default: all the skills of this repo)
#   update               pull the last version of the repository (the symlinks follow)
#   list                 list the skills available in this repo
#   status               show which skills are installed
#
# Options:
#   --project <dir>      target <dir>/.claude/skills instead of ~/.claude/skills
#   --copy               copy the files instead of a symlink (default: symlink, edits are live)
#   --force              replace an already existing skill that doesn't come from this repo
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
if [ -z "$SELF" ] || [ ! -f "$SELF" ] || ! ls "$(dirname "$SELF")"/*/SKILL.md > /dev/null 2>&1; then
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
TARGET="$HOME/.claude/skills"
MODE="link"
FORCE=false
PURGE=false
COMMAND=""
SKILLS=()

# =========================
# Helpers
# =========================
usage() {
    sed -n '2,23p' "$REPO/setup.sh" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

error() {
    echo "Error: $*" >&2
    exit 1
}

available() {
    for dir in "$REPO"/*/; do
        [ -f "$dir/SKILL.md" ] && basename "$dir"
    done
}

description() {
    sed -n 's/^description: *//p' "$REPO/$1/SKILL.md" | head -1 | cut -c1-90
}

# true if the installed skill comes from this repo (symlink to it or copy with the marker)
is_ours() {
    local dest="$TARGET/$1"
    if [ -L "$dest" ]; then
        [ "$(readlink -f "$dest")" = "$(readlink -f "$REPO/$1")" ]
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

if [ ${#SKILLS[@]} -eq 0 ]; then
    mapfile -t SKILLS < <(available)
fi
for skill in "${SKILLS[@]}"; do
    [ -f "$REPO/$skill/SKILL.md" ] || error "unknown skill '$skill' (see: $0 list)"
done

# Add the skills required by the selected ones (<skill>/requires.txt), only for install
if [ "$COMMAND" = "install" ]; then
    i=0
    while [ $i -lt ${#SKILLS[@]} ]; do
        req="$REPO/${SKILLS[$i]}/requires.txt"
        if [ -f "$req" ]; then
            for dep in $(cat "$req"); do
                [ -f "$REPO/$dep/SKILL.md" ] || error "'${SKILLS[$i]}' requires an unknown skill '$dep'"
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
        ;;

    list)
        for skill in $(available); do
            printf "  %-20s %s\n" "$skill" "$(description "$skill")"
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
                ln -s "$REPO/$skill" "$dest"
            else
                cp -r "$REPO/$skill" "$dest"
                echo "$REPO" > "$dest/.installed-from"
            fi
            find "$REPO/$skill" -path '*/scripts/*' \( -name '*.sh' -o -name '*.py' \) -exec chmod +x {} +
            echo "  installed $skill -> $dest ($MODE)"
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
        ;;
esac

if [ "$COMMAND" = "remove" ] && $PURGE; then
    if [ "$REPO" = "$(cd "$SKILLS_HOME" 2>/dev/null && pwd)" ]; then
        rm -rf "$SKILLS_HOME"
        echo "  purged $SKILLS_HOME"
    else
        echo "  --purge ignored: $REPO is not the managed clone ($SKILLS_HOME)"
    fi
fi
