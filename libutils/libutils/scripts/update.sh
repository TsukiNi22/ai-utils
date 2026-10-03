#!/bin/bash
# Show what changed in libutils since the commit of the reference, then regenerate it.
#
# Usage: update.sh [--ref <git ref>] [--repo <libutils path>] [--check]
#
#   --ref <ref>    commit to generate the reference from (default: HEAD of the repo)
#   --repo <path>  libutils repository (default: ~/personal_delivery/cpp/libutils,
#                  cloned into ~/.cache/libutils if missing)
#   --check        only show the changes, don't regenerate

set -euo pipefail

SKILL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$HOME/personal_delivery/cpp/libutils"
REMOTE="https://github.com/TsukiNi22/libutils.git"
REF="HEAD"
CHECK=false

while [ $# -gt 0 ]; do
    case "$1" in
        --ref) REF="$2"; shift ;;
        --repo) REPO="$2"; shift ;;
        --check) CHECK=true ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Error: unknown argument '$1'" >&2; exit 1 ;;
    esac
    shift
done

# Repository (fallback on a cached clone)
if [ ! -d "$REPO/.git" ]; then
    REPO="$HOME/.cache/libutils"
    if [ -d "$REPO/.git" ]; then
        git -C "$REPO" fetch -q origin && git -C "$REPO" reset -q --hard origin/main
    else
        git clone -q "$REMOTE" "$REPO"
    fi
fi
git -C "$REPO" fetch -q origin 2>/dev/null || echo "warning: fetch failed, using the local refs" >&2

OLD="$(sed -n 's/^| Commit | `\([0-9a-f]*\)` |$/\1/p' "$SKILL_DIR/reference/VERSION.md" 2>/dev/null || true)"
NEW="$(git -C "$REPO" rev-parse "$REF")"

echo "Repository: $REPO"
echo "Reference : ${OLD:0:7} -> ${NEW:0:7} ($REF)"
if [ -n "$(git -C "$REPO" status --porcelain -- include src 2>/dev/null)" ]; then
    echo "warning: uncommitted changes in include/ or src/ are NOT part of the reference (commit them first)"
fi
if [ -z "$OLD" ]; then
    echo "No previous reference."
elif [ "$OLD" = "$NEW" ]; then
    echo "Already up to date."
else
    echo
    echo "=== Commits"
    git -C "$REPO" log --oneline "$OLD..$NEW" -- include src cmake/config CHANGELOG.md || true
    echo
    echo "=== Headers changed"
    git -C "$REPO" diff --stat "$OLD" "$NEW" -- include || true
    echo
    echo "=== CHANGELOG (added lines)"
    git -C "$REPO" diff "$OLD" "$NEW" -- CHANGELOG.md | grep -E '^\+[^+]' | sed 's/^+//' || true
fi

$CHECK && exit 0
echo
python3 "$SKILL_DIR/scripts/gen_api.py" --repo "$REPO" --ref "$NEW"
