#!/usr/bin/env bash
# Regression test: the pinned item of a session (MCP session_pin / session_unpin / session_pin_get): a display pinned by its id, an
# inline block pinned at once, a message pinned by its index, one pin per session (a new one replaces it), listed by session_list,
# kept by the trash / a restore, refused when the settings say so (global_pin, sub_pin).
# Isolated core + a fake `claude` (tests/fake_claude.py).
# Usage: tests/pin.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
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
sleep 2

fail=0
check() { # <what> <pattern> <text>
    if grep -qF -- "$2" <<< "$3"; then echo "ok   $1"; else echo "FAIL $1 (wanted: $2)"; fail=1; fi
}
absent() { # <what> <pattern> <text>
    if grep -qF -- "$2" <<< "$3"; then echo "FAIL $1 (found: $2)"; fail=1; else echo "ok   $1"; fi
}

# one MCP call: the text of its answer (the server of the global session, or of a sub-session when SUB is set)
n=10
call() { # <tool> <arguments json>
    n=$((n + 1))
    printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
        '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
        "{\"jsonrpc\":\"2.0\",\"id\":${n},\"method\":\"tools/call\",\"params\":{\"name\":\"$1\",\"arguments\":$2}}" \
        | { cat; sleep 2; } | "${BIN}" mcp 2>/dev/null | grep "\"id\":${n}" || true
}

# a checklist put in the transcript, then pinned by its id
SHOWN="$(call session_display "{\"session\":\"${A}\",\"display\":{\"type\":\"checklist\",\"title\":\"Todo\",\"items\":[{\"text\":\"one\"},{\"text\":\"two\"}]}}")"
ID="$(sed -n 's/.*\\"id\\":\\"\([^\\]*\)\\".*/\1/p' <<< "${SHOWN}")"
check "the display has an id" "${ID}" "${SHOWN}"
check "pin a display by its id" 'Todo' "$(call session_pin "{\"session\":\"${A}\",\"display\":\"${ID}\"}")"
check "session_pin_get" "\\\"id\\\":\\\"${ID}\\\"" "$(call session_pin_get "{\"session\":\"${A}\"}")"
check "session_list tells the pin" 'pinned' "$(call session_list '{}')"

# the live block: updated while pinned, still pinned
call session_display_update "{\"session\":\"${A}\",\"id\":\"${ID}\",\"display\":{\"type\":\"checklist\",\"title\":\"Todo\",\"items\":[{\"text\":\"one\",\"done\":true},{\"text\":\"two\"}]}}" > /dev/null
check "still pinned after an update" "${ID}" "$(call session_pin_get "{\"session\":\"${A}\"}")"

# one pin per session: an inline block replaces it
check "an inline block pinned at once" 'note' "$(call session_pin "{\"session\":\"${A}\",\"display\":{\"type\":\"callout\",\"text\":\"hello\"},\"title\":\"note\"}")"
GOT="$(call session_pin_get "{\"session\":\"${A}\"}")"
check "the new pin replaced the old one" 'note' "${GOT}"
absent "the old pin is gone" "\\\"id\\\":\\\"${ID}\\\"" "${GOT}"

# kept by the trash and a restore
"${BIN}" close "${A}" > /dev/null
sleep 1
"${BIN}" restore "${A}" > /dev/null
sleep 2
check "a restore keeps the pin" 'note' "$(call session_pin_get "{\"session\":\"${A}\"}")"

# unpin
call session_unpin "{\"session\":\"${A}\"}" > /dev/null
absent "unpinned" 'note' "$(call session_pin_get "{\"session\":\"${A}\"}")"

# a message pinned by its index (the last entry of the transcript: the answer of the fake agent)
"${BIN}" send "${A}" "hello" > /dev/null 2>&1 || true
sleep 4
check "a message pinned by its index" 'message' "$(call session_pin "{\"session\":\"${A}\",\"index\":-1}")"
call session_unpin "{\"session\":\"${A}\"}" > /dev/null

# the settings: the global session refused when off, a sub-session on itself only
call settings_set '{"section":"agent","key":"global_pin","value":"false"}' > /dev/null
check "the global session refused when off" 'not allowed' "$(call session_pin "{\"session\":\"${A}\",\"display\":{\"type\":\"callout\",\"text\":\"x\"}}")"
call settings_set '{"section":"agent","key":"global_pin","value":"true"}' > /dev/null
SUB() { CLAUDE_CLUSTER_SESSION="${A}" call "$@"; }
check "a sub-session pins on itself" 'sub' "$(SUB session_pin "{\"session\":\"${A}\",\"display\":{\"type\":\"callout\",\"text\":\"mine\"},\"title\":\"sub\"}")"
call settings_set '{"section":"agent","key":"sub_pin","value":"false"}' > /dev/null
check "a sub-session refused when off" 'sub-sessions' "$(SUB session_pin "{\"session\":\"${A}\",\"display\":{\"type\":\"callout\",\"text\":\"x\"}}")"

exit "${fail}"
