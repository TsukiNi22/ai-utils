#!/usr/bin/env bash
# Regression test: the notifications of the desktop are published to the Android app (GET /api/notify): a device logged in gets
# the number, then the ones after it (the notify tool), none replayed for a number of before a restart, none without its cookie,
# and none at all with [ui] notify_app = false. Isolated core (own config / data / socket / port), no global session.
# Usage: tests/notify_app.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
PORT=18791
URL="http://127.0.0.1:${PORT}"
trap 'kill "${SERVE:-0}" 2>/dev/null && wait "${SERVE:-0}" 2>/dev/null; rm -rf "${WORK}"' EXIT # TERM: the watcher stops its core (SIGKILL would leave it)

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
source "$(dirname "${BASH_SOURCE[0]}")/isolate.sh"   # refuses the real config / data / socket of the user, a home of its own
command mkdir -p "${WORK}/cfg" "${WORK}/data"

fail=0
check() { # <what> <condition: 0 = ok>
    if [[ "$2" == 0 ]]; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi
}

notify_tool() { # the MCP tool of the agents
    {
        printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
            '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
            '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"notify","arguments":{"title":"hello phone","text":"from the test","urgent":true}}}'
        sleep 2
    } | "${BIN}" mcp > /dev/null 2>&1
}

run() { # <notify_app>
    printf '[commands]\nclaude = "%s/fake_claude.py"\n[ui]\nnotify_app = %s\n[web]\nenabled = true\nport = %s\nhttps = false\napprove = false\n' "${HERE}" "$1" "${PORT}" > "${WORK}/cfg/config.toml"
    "${BIN}" serve --no-global > "${WORK}/serve.log" 2>&1 < /dev/null &
    SERVE=$!
    sleep 4
    local pin
    pin="$("${BIN}" web pin --json < /dev/null 2> /dev/null | grep -o '"pin": *"[0-9 ]*"' | tr -dc '0-9')"
    command curl -s -c "${WORK}/jar" -H 'Content-Type: application/json' -d "{\"pin\":\"${pin}\",\"name\":\"t\"}" "${URL}/api/login" > /dev/null
}

stop() {
    kill "${SERVE}" 2> /dev/null || true
    for _ in 1 2 3 4 5 6 7 8 9 10; do # its quit waits for the sessions: killed for good when it is slow
        kill -0 "${SERVE}" 2> /dev/null || break
        sleep 1
    done
    kill -9 "${SERVE}" 2> /dev/null || true
    sleep 1
}

run true
base="$(command curl -s -b "${WORK}/jar" "${URL}/api/notify")"
seq="$(grep -o '"seq":[0-9]*' <<< "${base}" | cut -d: -f2)"
grep -q '"events":\[\]' <<< "${base}"; check "no number: only the current one, nothing replayed" "$?"
( command curl -s -b "${WORK}/jar" "${URL}/api/notify?since=${seq}" > "${WORK}/poll.json" ) &
POLL=$!
sleep 1
notify_tool
wait "${POLL}"
grep -q '"title":"hello phone"' "${WORK}/poll.json" && grep -q '"kind":"message"' "${WORK}/poll.json" && grep -q '"urgent":true' "${WORK}/poll.json"; check "the long poll answers with the notification of the notify tool" "$?"
grep -q "\"seq\":$((seq + 1))" "${WORK}/poll.json"; check "its number follows the last one" "$?"
big="$(command curl -s -b "${WORK}/jar" "${URL}/api/notify?since=99999")"
grep -q '"events":\[\]' <<< "${big}"; check "a number from before a restart: nothing replayed" "$?"
[[ "$(command curl -s "${URL}/api/notify")" == *"not connected"* ]]; check "a device that is not connected gets nothing" "$?"
stop

run false
seq="$(command curl -s -b "${WORK}/jar" "${URL}/api/notify" | grep -o '"seq":[0-9]*' | cut -d: -f2)"
notify_tool
after="$(command curl -s -b "${WORK}/jar" "${URL}/api/notify?since=${seq}" --max-time 3 || true)"
[[ -z "${after}" ]] || grep -q '"events":\[\]' <<< "${after}"; check "[ui] notify_app = false publishes nothing" "$?"
exit "${fail}"
