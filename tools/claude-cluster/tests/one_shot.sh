#!/usr/bin/env bash
# Regression test: a session spawned with one_shot is closed to the trash once its answer is relayed (setting archive_one_shot,
# on by default); a session without the flag stays; with the setting off the flag is ignored.
# Isolated core + a fake `claude` (tests/fake_claude.py); the global session is the fake too.
# Usage: tests/one_shot.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
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

"${BIN}" serve > "${WORK}/serve.log" 2>&1 &
SERVE=$!
sleep 4

# one MCP call (the global session's tools): tool name, JSON arguments -> the text answer
call() {
    {
        printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
            '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
            "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"$1\",\"arguments\":$2}}"
        sleep 2
    } | "${BIN}" mcp 2>/dev/null | grep '"id":2'
}

fail=0
check() { # <what> <session> <state: open | archived>
    local open archived
    open="$(call session_list '{}')"
    archived="$(call session_list '{"state":"archived"}')"
    local where=open
    if grep -qF "$2" <<< "${archived}"; then where=archived; fi
    if [[ "${where}" == "$3" ]]; then echo "ok   $1"; else echo "FAIL $1 (${2} is ${where})"; fail=1; fi
    : "${open}"
}

call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"oneoff\",\"prompt\":\"do it\",\"one_shot\":true}" > /dev/null
call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"project\",\"prompt\":\"do it\"}" > /dev/null
sleep 8
check "one_shot: archived once its answer is relayed" oneoff archived
check "no flag: the session stays" project open

call settings_set '{"section":"sessions","key":"archive_one_shot","value":"false"}' > /dev/null
call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"ignored\",\"prompt\":\"do it\",\"one_shot\":true}" > /dev/null
sleep 8
check "setting off: the flag is ignored" ignored open

exit "${fail}"
