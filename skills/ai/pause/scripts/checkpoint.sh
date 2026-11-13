#!/bin/bash
# The checkpoint of a paused task: one file per project, outside the project (nothing to commit or ignore).
#
# Usage: checkpoint.sh path [dir]   the file of the project of <dir> (default: the current folder, folder created)
#        checkpoint.sh show [dir]   print it (exit 1 when the project has none)
#        checkpoint.sh list         every checkpoint waiting, newest first (date, project)
#        checkpoint.sh done [dir]   the task is over: move it to done/ (kept, never deleted)
#
# The project is the root of the git repository of <dir>, or <dir> itself outside a repository. The files live in
# ${CLAUDE_PAUSE_DIR:-~/.claude/pauses}/<project path with / -> ->.md.
set -euo pipefail

##### Variables #####
PAUSES="${CLAUDE_PAUSE_DIR:-$HOME/.claude/pauses}"
COMMAND="${1:-}"

usage() {
    sed -n '4,7p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

# Root of the project of a folder: its git repository, else the folder itself
project() {
    local dir="${1:-.}"
    [[ -d "$dir" ]] || { echo "Error: $dir is not a folder" >&2; exit 1; }
    \git -C "$dir" rev-parse --show-toplevel 2> /dev/null || (command cd "$dir" && command pwd)
}

file_of() {
    local root
    root="$(project "${1:-.}")"
    echo "$PAUSES/$(echo "$root" | tr '/' '-').md"
}

##### Commands #####
case "$COMMAND" in
    path)
        command mkdir -p "$PAUSES" # the skill writes the file right after
        file_of "${2:-.}"
        ;;
    show)
        file="$(file_of "${2:-.}")"
        [[ -f "$file" ]] || { echo "no checkpoint for $(project "${2:-.}")" >&2; exit 1; }
        cat "$file"
        ;;
    list)
        found=false
        for file in $(command ls -t "$PAUSES"/*.md 2> /dev/null); do
            found=true
            printf '%s  %s\n' "$(date -r "$file" '+%Y-%m-%d %H:%M')" "$(sed -n 's/^\*\*Project\*\*: *//p' "$file" | head -1 | sed 's/ · .*//')"
        done
        $found || echo "no checkpoint waiting"
        ;;
    done)
        file="$(file_of "${2:-.}")"
        [[ -f "$file" ]] || { echo "no checkpoint for $(project "${2:-.}")" >&2; exit 1; }
        command mkdir -p "$PAUSES/done"
        mv "$file" "$PAUSES/done/$(basename "$file" .md)-$(date '+%Y%m%d-%H%M%S').md"
        echo "checkpoint closed: $PAUSES/done/"
        ;;
    -h|--help) usage 0 ;;
    *) usage 1 ;;
esac
