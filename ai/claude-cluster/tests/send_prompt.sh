#!/usr/bin/env bash
# Regression test: a prompt sent by the global session (MCP session_send) reaches the sub-session whole.
# Isolated core + a fake `claude` (tests/fake_claude.py) logging what it receives.
# Usage: tests/send_prompt.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
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
ID="$("${BIN}" spawn "${WORK}/proj" --name t1 --rtk)"
sleep 2

# the MCP server of the global session: session_send with the argument named "text", then "prompt" (like session_spawn), then none
mcp() {
    printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
        '{"jsonrpc":"2.0","method":"notifications/initialized"}' "$@"
    sleep 3
}
OUT="$(mcp \
    "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"session_send\",\"arguments\":{\"session\":\"${ID}\",\"text\":\"via text\",\"reply\":false}}}" \
    "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"session_send\",\"arguments\":{\"session\":\"${ID}\",\"prompt\":\"via prompt\",\"reply\":false}}}" \
    "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"session_send\",\"arguments\":{\"session\":\"${ID}\",\"reply\":false}}}" \
    | "${BIN}" mcp 2>/dev/null)"

fail=0
check() { # <what> <pattern> <file>
    if grep -qF -- "$2" "$3"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi
}
check "text reaches the session" 'PROMPT "[from the global session] via text"' "${WORK}/log"
check "prompt (alias) reaches the session" 'PROMPT "[from the global session] via prompt"' "${WORK}/log"
if grep -qF 'PROMPT "[from the global session] "' "${WORK}/log"; then echo "FAIL an empty marked prompt was sent"; fail=1; else echo "ok   no empty marked prompt"; fi
if grep -qF 'empty prompt' <<< "${OUT}"; then echo "ok   a send without text is refused"; else echo "FAIL a send without text is refused"; fail=1; fi
exit "${fail}"
