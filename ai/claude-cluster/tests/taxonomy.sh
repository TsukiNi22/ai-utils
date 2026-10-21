#!/usr/bin/env bash
# Regression test: the taxonomy of the categories and tags (parent of a node, cycles refused, descendants in the list filters, rename / merge /
# delete on the sessions, kept in taxonomy.json), through the CLI of an isolated core + a fake `claude` (tests/fake_claude.py).
# Usage: tests/taxonomy.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
source "$(dirname "${BASH_SOURCE[0]}")/isolate.sh"   # refuses the real config / data / socket of the user, a home of its own
export FAKE_CLAUDE_LOG="${WORK}/log"
command mkdir -p "${WORK}/cfg" "${WORK}/data" "${WORK}/proj"
printf '[commands]\nclaude = "%s/fake_claude.py"\n' "${HERE}" > "${WORK}/cfg/config.toml"

"${BIN}" serve --no-global > "${WORK}/serve.log" 2>&1 &
SERVE=$!
sleep 3
A="$("${BIN}" spawn "${WORK}/proj" --name alpha --rtk)"
B="$("${BIN}" spawn "${WORK}/proj" --name beta --rtk)"
C="$("${BIN}" spawn "${WORK}/proj" --name gamma --rtk)"
sleep 2

fail=0
check() { # <what> <pattern> <text>
    if grep -qF -- "$2" <<< "$3"; then echo "ok   $1"; else echo "FAIL $1 (wanted: $2)"; fail=1; fi
}
absent() { # <what> <pattern> <text>
    if grep -qF -- "$2" <<< "$3"; then echo "FAIL $1 (found: $2)"; fail=1; else echo "ok   $1"; fi
}

"${BIN}" label "${A}" --category bugfix --tag api > /dev/null
"${BIN}" label "${B}" --tag ui > /dev/null
"${BIN}" label "${C}" --tag docs > /dev/null

# the parents: api under backend, ui under frontend
"${BIN}" taxonomy set_parent api backend > /dev/null
"${BIN}" taxonomy set_parent ui frontend > /dev/null
TREE="$("${BIN}" taxonomy --rtk)"
check "api under backend" "api|backend|" "${TREE}"
check "ui under frontend" "ui|frontend|" "${TREE}"
check "backend under global" "backend|global|" "${TREE}"
check "kept on disk" '"api"' "$(cat "${WORK}/data/taxonomy.json")"

# a cycle is refused (backend under api), the tree unchanged
if "${BIN}" taxonomy set_parent backend api > /dev/null 2>&1; then echo "FAIL a cycle was accepted"; fail=1; else echo "ok   a cycle is refused"; fi
check "backend still under global" "backend|global|" "$("${BIN}" taxonomy --rtk)"

# the filters work on the descendants: tag backend finds alpha (tag api, below backend), not beta
LIST="$("${BIN}" list --tag backend --rtk)"
check "tag backend includes the sessions under api" "alpha|" "${LIST}"
absent "tag backend leaves ui alone" "beta|" "${LIST}"

# rename: the sessions follow
"${BIN}" taxonomy rename api rest-api > /dev/null
check "renamed in the tree" "rest-api|backend|" "$("${BIN}" taxonomy --rtk)"
check "renamed on the session" "rest-api" "$("${BIN}" list --rtk)"

# merge ui into frontend: beta now has the tag frontend
"${BIN}" taxonomy merge ui frontend > /dev/null
check "merged on the session" "|frontend" "$("${BIN}" list --rtk)"

# delete backend: rest-api goes to its parent (global); the sessions keep their labels
"${BIN}" taxonomy delete backend > /dev/null
check "children moved up" "rest-api|global|" "$("${BIN}" taxonomy --rtk)"
absent "backend gone" "backend|" "$("${BIN}" taxonomy --rtk)"

# the setting: the global session cannot edit it when it is off (the CLI always can)
"${BIN}" taxonomy set_parent docs frontend > /dev/null
check "the CLI edits it anyway" "docs|frontend|" "$("${BIN}" taxonomy --rtk)"

exit "${fail}"
