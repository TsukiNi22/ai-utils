#!/usr/bin/env bash
# Regression test: by default a sub-session has NO heavy_run / jobs_add (the MCP path asks no permission), the global session keeps
# them, the sub-session keeps job_status / jobs_list / job_wait and its shell cluster-run (not an MCP tool).
# Isolated core + a fake `claude`. Usage: tests/orch_sub_default.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
source "$(dirname "${BASH_SOURCE[0]}")/isolate.sh"   # refuses the real config / data / socket of the user, a home of its own
export FAKE_CLAUDE_LOG="${WORK}/log"
command mkdir -p "${WORK}/cfg" "${WORK}/data" "${WORK}/proj"
printf '[commands]\nclaude = "%s/fake_claude_wake.py"\n' "${HERE}" > "${WORK}/cfg/config.toml"

"${BIN}" serve > "${WORK}/serve.log" 2>&1 &
SERVE=$!
sleep 4

call() { # tool, JSON arguments -> the answer (SUB: a sub-session id -> its own MCP)
    {
        printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
            '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
            "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"$1\",\"arguments\":$2}}"
        sleep 1
    } | CLAUDE_CLUSTER_SESSION="${SUB:-}" "${BIN}" mcp 2>/dev/null | grep '"id":2'
}
tools() { # the tool names a sub-session sees
    {
        printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
            '{"jsonrpc":"2.0","id":3,"method":"tools/list"}'
        sleep 1
    } | CLAUDE_CLUSTER_SESSION="$1" "${BIN}" mcp 2>/dev/null | grep '"id":3' | grep -o '"name":"[a-z_]*"' | tr '\n' ' '
}

fail=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; fail=1; }
has() { if grep -qF -- "$3" <<< "$2"; then ok "$1"; else bad "$1 (no '$3' in: ${2:0:300})"; fi; }
hasnt() { if grep -qF -- "$3" <<< "$2"; then bad "$1 ('$3' found)"; else ok "$1"; fi; }

SPAWNED="$(call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"worker\",\"reply\":false}")"
sleep 2
SUBID="$(grep -o 's[0-9][0-9]*' <<< "${SPAWNED}" | head -1)"
SUBTOOLS="$(tools "${SUBID}")"
hasnt "sub: heavy_run is not offered by default" "${SUBTOOLS}" 'heavy_run'
hasnt "sub: jobs_add is not offered by default" "${SUBTOOLS}" 'jobs_add'
has "sub: job_status stays" "${SUBTOOLS}" 'job_status'
has "sub: jobs_list stays" "${SUBTOOLS}" 'jobs_list'
has "sub: job_wait stays" "${SUBTOOLS}" 'job_wait'
# refused core-side too (a client that calls the tool anyway)
has "sub: heavy_run refused by the core" "$(SUB="${SUBID}" call heavy_run '{"command":"echo nope"}')" 'is off for the sub-sessions'
has "sub: jobs_add refused by the core" "$(SUB="${SUBID}" call jobs_add '{"command":"echo nope"}')" 'is off for the sub-sessions'
has "global: heavy_run still works" "$(call heavy_run '{"command":"echo fine-global"}')" 'fine-global'

if [[ "${fail}" == 0 ]]; then echo "orch_sub_default: all tests passed"; fi
exit "${fail}"
