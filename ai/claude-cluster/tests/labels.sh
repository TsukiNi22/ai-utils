#!/usr/bin/env bash
# Regression test: the category and the tags of a session (set by the CLI and by the MCP session_describe, kept by the trash / a restore,
# filtered by session_list, found by search "#tag").
# Isolated core + a fake `claude` (tests/fake_claude.py).
# Usage: tests/labels.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
export FAKE_CLAUDE_LOG="${WORK}/log"
command mkdir -p "${WORK}/cfg" "${WORK}/data" "${WORK}/proj"
printf '[commands]\nclaude = "%s/fake_claude.py"\n' "${HERE}" > "${WORK}/cfg/config.toml"

"${BIN}" serve --no-global > "${WORK}/serve.log" 2>&1 &
SERVE=$!
sleep 3
A="$("${BIN}" spawn "${WORK}/proj" --name alpha --rtk)"
B="$("${BIN}" spawn "${WORK}/proj" --name beta --rtk)"
sleep 2

fail=0
check() { # <what> <pattern> <text>
    if grep -qF -- "$2" <<< "$3"; then echo "ok   $1"; else echo "FAIL $1 (wanted: $2)"; fail=1; fi
}
absent() { # <what> <pattern> <text>
    if grep -qF -- "$2" <<< "$3"; then echo "FAIL $1 (found: $2)"; fail=1; else echo "ok   $1"; fi
}

# the CLI: a category and tags (cleaned: lower case, # dropped, no duplicate)
"${BIN}" label "${A}" --category BugFix --tag "API, #Urgent api" > /dev/null
"${BIN}" label "${B}" --category docs --tag readme > /dev/null
LIST="$("${BIN}" list --rtk)"
check "alpha has its category and tags" "alpha|" "${LIST}"
check "category lower cased, tags cleaned" "|bugfix|api,urgent" "${LIST}"
check "beta has its labels" "|docs|readme" "${LIST}"

# the filters of session_list
check "filter by category" "alpha" "$("${BIN}" list --rtk --category bugfix)"
absent "filter by category leaves the others out" "beta" "$("${BIN}" list --rtk --category bugfix)"
check "filter by tag" "beta" "$("${BIN}" list --rtk --tag readme)"
absent "filter by tag leaves the others out" "alpha" "$("${BIN}" list --rtk --tag readme)"
absent "two tags must both match" "alpha" "$("${BIN}" list --rtk --tag api,readme)"

# the MCP server of the global session: the description kept when only the labels change, add / remove
mcp() {
    printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
        '{"jsonrpc":"2.0","method":"notifications/initialized"}' "$@"
    sleep 3
}
OUT="$(mcp \
    "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"session_describe\",\"arguments\":{\"session\":\"${A}\",\"text\":\"the api fix\"}}}" \
    "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"session_describe\",\"arguments\":{\"session\":\"${A}\",\"add_tags\":[\"backend\"],\"remove_tags\":[\"urgent\"]}}}" \
    "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"session_list\",\"arguments\":{\"tag\":\"backend\"}}}" \
    | "${BIN}" mcp 2>/dev/null)"
check "session_list filters by tag" '\"tags\":[\"api\",\"backend\"]' "${OUT}"
check "the description is kept when only the labels change" 'the api fix' "${OUT}"

# the search: the labels only with "#", else the text of a label too
check "search #tag" "alpha" "$("${BIN}" search '#backend' --rtk)"
absent "search #tag leaves the others out" "beta" "$("${BIN}" search '#backend' --rtk)"
check "search finds a category by its text" "beta" "$("${BIN}" search docs --rtk)"

# kept by the trash and a restore, listed with state archived
"${BIN}" close "${A}" > /dev/null
sleep 1
check "the trash keeps the labels" "archived" "$("${BIN}" list --rtk --state archived)"
check "the trash is filtered by tag" "alpha" "$("${BIN}" list --rtk --state archived --tag backend)"
absent "the open list no longer has it" "alpha" "$("${BIN}" list --rtk)"
"${BIN}" restore "${A}" > /dev/null
sleep 2
check "a restore keeps the labels" "|bugfix|api,backend" "$("${BIN}" list --rtk)"

exit "${fail}"
