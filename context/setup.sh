#!/bin/bash
# Install / remove / update the global Claude Code context (CLAUDE.md, RTK.md, hooks, helpers).
#
# Usage: ./setup.sh <command> [options]
#        curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- context <command> ...
#        ./setup.sh context <command> ...   (from the root of the repository)
#
# Commands:
#   install    link (or copy) the files into ~/.claude and ~/.local/bin, merge the hooks in settings.json,
#              install rtk if it is missing
#   remove     remove what was installed (restore the backups of the replaced files, unmerge the hooks)
#   update     pull the last version (the symlinks follow)
#   status     show what is installed
#
# Options:
#   --copy       copy the files instead of symlinks (default: symlink, edits are live and versioned)
#   --no-rtk     don't install rtk (the RTK.md instructions and the rtk hook need it)
#   --no-hooks   don't touch ~/.claude/settings.json (hooks)
#   -h, --help   show this help
#
# Replaced files are saved in ~/.claude/backups/context/ and restored by remove.

set -euo pipefail

RTK_INSTALL="https://raw.githubusercontent.com/rtk-ai/rtk/refs/heads/master/install.sh"

SELF="${BASH_SOURCE[0]}"
REPO="$(cd "$(dirname "$SELF")" && pwd)"
BACKUP="$HOME/.claude/backups/context"
SETTINGS="$HOME/.claude/settings.json"
MODE="link"; RTK=true; HOOKS=true
COMMAND=""

# repo file -> installed path
FILES=(
    "claude/CLAUDE.md:$HOME/.claude/CLAUDE.md"
    "claude/RTK.md:$HOME/.claude/RTK.md"
    "claude/hooks/session-aliases.sh:$HOME/.claude/hooks/session-aliases.sh"
    "bin/sudo-askpass:$HOME/.local/bin/sudo-askpass"
)

usage() { sed -n '2,22p' "$REPO/setup.sh" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

while [ $# -gt 0 ]; do
    case "$1" in
        install|remove|update|status) COMMAND="$1" ;;
        --copy) MODE="copy" ;;
        --no-rtk) RTK=false ;;
        --no-hooks) HOOKS=false ;;
        -h|--help) usage 0 ;;
        *) echo "Error: unknown argument '$1'" >&2; usage 1 ;;
    esac
    shift
done
[ -z "$COMMAND" ] && usage 1

# =========================
# Helpers
# =========================
is_ours() { # <repo file> <target>
    local src="$REPO/$1" dst="$2"
    if [ -L "$dst" ]; then [ "$(readlink -f "$dst")" = "$(readlink -f "$src")" ]
    else [ -f "$dst" ] && [ -f "$dst.context-installed" ]; fi
}

# Merge (or remove) the hooks of claude/settings.hooks.json in ~/.claude/settings.json
hooks() { # merge|unmerge|check
    python3 - "$1" "$REPO/claude/settings.hooks.json" "$SETTINGS" <<'PY'
import json, os, sys
action, frag_path, path = sys.argv[1:4]
frag = json.load(open(frag_path))["hooks"]
data = json.load(open(path)) if os.path.exists(path) and os.path.getsize(path) else {}
hooks = data.setdefault("hooks", {})
def key(h): return json.dumps(h, sort_keys=True)
changed, missing = False, 0
for event, groups in frag.items():
    current = hooks.setdefault(event, [])
    for g in groups:
        present = any(key(g) == key(c) for c in current)
        if action == "merge" and not present: current.append(g); changed = True
        if action == "unmerge" and present: current[:] = [c for c in current if key(c) != key(g)]; changed = True
        if action == "check" and not present: missing += 1
    if not current: del hooks[event]
if not hooks: data.pop("hooks", None)
if action == "check":
    print("merged" if not missing else f"{missing} hook(s) missing"); sys.exit(0)
if changed:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(data, open(path, "w"), indent=2); open(path, "a").write("\n")
print(("hooks " + ("merged into " if action == "merge" else "removed from ") + path) if changed else "hooks already " + ("merged" if action == "merge" else "removed"))
PY
}

# =========================
# Commands
# =========================
case "$COMMAND" in
    update)
        git -C "$REPO" pull --ff-only
        ;;

    status)
        echo "Repository: $REPO ($(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo '?'))"
        for f in "${FILES[@]}"; do
            src="${f%%:*}"; dst="${f#*:}"
            if is_ours "$src" "$dst"; then state="installed ($([ -L "$dst" ] && echo symlink || echo copy))"
            elif [ -e "$dst" ]; then state="other file (not from this repo)"
            else state="not installed"; fi
            printf "  %-34s %s\n" "${dst/#$HOME/\~}" "$state"
        done
        printf "  %-34s %s\n" "~/.claude/settings.json (hooks)" "$(hooks check)"
        printf "  %-34s %s\n" "rtk" "$(command -v rtk > /dev/null && rtk --version || echo 'not installed')"
        ;;

    install)
        stamp="$(date +%Y%m%d-%H%M%S)"
        for f in "${FILES[@]}"; do
            src="${f%%:*}"; dst="${f#*:}"
            command mkdir -p "$(dirname "$dst")"
            if is_ours "$src" "$dst"; then rm -f "$dst" "$dst.context-installed"
            elif [ -e "$dst" ] || [ -L "$dst" ]; then
                command mkdir -p "$BACKUP"
                mv "$dst" "$BACKUP/$(basename "$dst").$stamp"
                echo "  backup: ${dst/#$HOME/\~} -> ${BACKUP/#$HOME/\~}/$(basename "$dst").$stamp"
            fi
            if [ "$MODE" = "link" ]; then ln -s "$REPO/$src" "$dst"
            else cp "$REPO/$src" "$dst"; touch "$dst.context-installed"; fi
            echo "  installed ${dst/#$HOME/\~} ($MODE)"
        done
        if $HOOKS; then echo "  $(hooks merge)"; fi
        if $RTK && ! command -v rtk > /dev/null 2>&1; then
            echo "  installing rtk ($RTK_INSTALL)"
            curl -fsSL "$RTK_INSTALL" | sh
        fi
        command -v rtk > /dev/null 2>&1 || echo "  warning: rtk is not installed, the rtk hook will fail (install it or use --no-hooks)"
        ;;

    remove)
        for f in "${FILES[@]}"; do
            src="${f%%:*}"; dst="${f#*:}"
            if is_ours "$src" "$dst"; then
                rm -f "$dst" "$dst.context-installed"
                last="$(ls -1 "$BACKUP/$(basename "$dst")."* 2>/dev/null | sort | tail -1 || true)"
                if [ -n "$last" ]; then mv "$last" "$dst"; echo "  restored ${dst/#$HOME/\~} from the backup"
                else echo "  removed ${dst/#$HOME/\~}"; fi
            fi
        done
        if $HOOKS; then echo "  $(hooks unmerge)"; fi
        echo "  rtk is kept (remove it with: rm ~/.local/bin/rtk)"
        ;;
esac
