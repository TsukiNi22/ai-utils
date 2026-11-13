#!/bin/bash
# Is the libutils reference of this skill up to date? (installed version, libutils repository)
#
# Usage: check.sh [--fetch] [--repo <libutils path>]
#
#   --fetch        also compare with origin/main (network)
#   --repo <path>  libutils repository (default: $LIBUTILS or ~/personal_delivery/cpp/libutils)
#
# Last line: "status: up to date" (exit 0) | "status: outdated" (exit 1) | "status: unknown" (exit 2)

set -euo pipefail

SKILL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="${LIBUTILS:-$HOME/personal_delivery/cpp/libutils}"
FETCH=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fetch) FETCH=true ;;
        --repo) REPO="$2"; shift ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Error: unknown argument '$1'" >&2; exit 2 ;;
    esac
    shift
done

# Reference of the skill
VERSION_FILE="$SKILL_DIR/reference/VERSION.md"
REF_VERSION="$(sed -n 's/^| Version (`CMakeLists.txt`) | `\([^`]*\)` |$/\1/p' "$VERSION_FILE" 2> /dev/null || true)"
REF_HASH="$(sed -n 's/^| Commit | `\([0-9a-f]*\)` |$/\1/p' "$VERSION_FILE" 2> /dev/null || true)"
if [[ -z "$REF_HASH" ]]; then
    echo "reference: none ($VERSION_FILE)"
    echo "status: unknown"
    exit 2
fi
echo "reference: $REF_VERSION (${REF_HASH:0:7})"
OUTDATED=false
KNOWN=false

# Installed headers
INSTALLED=""
CPATH_DIRS="${CPATH:-}"
for dir in ${CPATH_DIRS//:/ } /usr/local/include /usr/include "$HOME/.local/include"; do
    if [[ -f "$dir/utils/version.hpp" ]]; then
        INSTALLED="$(sed -n 's/.*#define __LIBUTILS_VERSION__ "\([^"]*\)".*/\1/p' "$dir/utils/version.hpp" | head -1)"
        break
    fi
done
if [[ -n "$INSTALLED" ]]; then
    KNOWN=true
    if [[ "$INSTALLED" != "$REF_VERSION" ]]; then
        OUTDATED=true
        echo "installed: $INSTALLED (differs from the reference)"
    else
        echo "installed: $INSTALLED"
    fi
else
    echo "installed: none"
fi

# Repository (local HEAD, origin/main with --fetch)
if [[ -d "$REPO/.git" ]]; then
    KNOWN=true
    $FETCH && { git -C "$REPO" fetch -q origin 2> /dev/null || echo "warning: fetch failed, local refs only"; }
    for ref in HEAD origin/main; do
        git -C "$REPO" rev-parse -q --verify "$ref" > /dev/null || continue
        AHEAD="$(git -C "$REPO" rev-list --count "$REF_HASH..$ref" -- include src CHANGELOG.md 2> /dev/null || echo "?")"
        echo "repository $ref: $AHEAD commit(s) after the reference"
        [[ "$AHEAD" != "0" ]] && OUTDATED=true
    done
fi

if ! $KNOWN; then
    echo "status: unknown"
    exit 2
fi
if $OUTDATED; then
    echo "status: outdated (update: bash $SKILL_DIR/scripts/update.sh, see the changes: --check)"
    exit 1
fi
echo "status: up to date"
