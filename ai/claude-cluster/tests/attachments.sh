#!/usr/bin/env bash
# Regression test: files attached by the global session (MCP session_send attachments) - linked in the folder of the session,
# an image given as a picture, a text file as @path; a file outside the project folder / the home is refused.
# Isolated core + a fake `claude` (tests/fake_claude.py) logging what it receives.
# Usage: tests/attachments.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
export FAKE_CLAUDE_LOG="${WORK}/log"
command mkdir -p "${WORK}/cfg" "${WORK}/data" "${WORK}/proj" "${WORK}/outside"
printf '[commands]\nclaude = "%s/fake_claude.py"\n' "${HERE}" > "${WORK}/cfg/config.toml"
echo "hello" > "${WORK}/proj/note.txt"
echo "secret" > "${WORK}/outside/secret.txt"
python3 -c 'import base64,sys; sys.stdout.buffer.write(base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="))' > "${WORK}/proj/pic.bin"

"${BIN}" serve --no-global > "${WORK}/serve.log" 2>&1 &
SERVE=$!
sleep 3
ID="$("${BIN}" spawn "${WORK}/proj" --name t1 --rtk)"
sleep 2

mcp() {
    printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
        '{"jsonrpc":"2.0","method":"notifications/initialized"}' "$@"
    sleep 3
}
OUT="$(mcp \
    "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"session_send\",\"arguments\":{\"session\":\"${ID}\",\"text\":\"look\",\"reply\":false,\"attachments\":[\"${WORK}/proj/note.txt\",\"${WORK}/proj/pic.bin\"]}}}" \
    "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"session_send\",\"arguments\":{\"session\":\"${ID}\",\"text\":\"leak\",\"reply\":false,\"attachments\":[\"${WORK}/outside/secret.txt\"]}}}" \
    | "${BIN}" mcp 2>/dev/null)"

fail=0
check() { # <what> <pattern> <file>
    if grep -qF -- "$2" "$3"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi
}
check "the text file arrives as @path in the folder of the session" "attachments/${ID}/" "${WORK}/log"
check "the text file keeps its name" "-note.txt" "${WORK}/log"
check "the picture arrives as an image block" '"type": "image"' "${WORK}/log"
check "the picture is stored with the extension of its content" ".png" "${WORK}/log"
if grep -qF 'leak' "${WORK}/log"; then echo "FAIL a file outside the project folder / home was attached"; fail=1; else echo "ok   a file outside the project folder / home is refused"; fi
if grep -qF 'home of the user only' <<< "${OUT}"; then echo "ok   the refusal says why"; else echo "FAIL the refusal says why"; fail=1; fi
if [[ "$(find "${WORK}/data/attachments/${ID}" -type l | wc -l)" == 2 ]]; then echo "ok   linked: two symlinks, nothing copied"; else echo "FAIL two symlinks expected"; fail=1; fi
exit "${fail}"
