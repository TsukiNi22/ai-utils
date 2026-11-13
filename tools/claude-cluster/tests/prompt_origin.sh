#!/usr/bin/env bash
# Regression test: the origin of a prompt (user | mcp | task) is stored with its transcript entry (the colour of the prompt
# in the terminal, the window and the web page): typed by the user: none, session_send of the global session: mcp, a task: task.
# Isolated core + a fake `claude` (tests/fake_claude.py).
# Usage: tests/prompt_origin.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
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
ID="$("${BIN}" spawn "${WORK}/proj" --name t1 --rtk)"
sleep 2

"${BIN}" send "${ID}" "typed by me" --rtk > /dev/null
sleep 2
mcp() {
    printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
        '{"jsonrpc":"2.0","method":"notifications/initialized"}' "$@"
    sleep 3
}
mcp "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"session_send\",\"arguments\":{\"session\":\"${ID}\",\"text\":\"from global\",\"reply\":false}}}" \
    | "${BIN}" mcp > /dev/null 2>&1
sleep 2

LOG="${WORK}/data/logs/${ID}.jsonl"
fail=0
entry() { # <text> -> the origin field of the user entry containing it
    python3 -I -c '
import json, sys
for line in open(sys.argv[1]):
    e = json.loads(line)
    if e.get("kind") == 0 and sys.argv[2] in e.get("text", ""):
        print(e.get("origin", "")); break
else:
    print("MISSING")' "${LOG}" "$1"
}
check() { # <what> <expected> <got>
    if [[ "$2" == "$3" ]]; then echo "ok   $1"; else echo "FAIL $1 (expected '$2', got '$3')"; fail=1; fi
}
check "typed prompt: no origin (the user)" "" "$(entry 'typed by me')"
check "session_send prompt: mcp" "mcp" "$(entry 'from global')"
exit "${fail}"
