#!/usr/bin/env bash
# Regression test: /model (and /effort) typed while a session works keeps the running prompt and the queued ones
# (they were lost: the session was restarted on the new model with an empty queue), and tells what it did.
# Isolated core + a fake `claude` (tests/fake_claude.py).
# Usage: tests/model_change.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
source "$(dirname "${BASH_SOURCE[0]}")/isolate.sh"   # refuses the real config / data / socket of the user, a home of its own
export FAKE_CLAUDE_LOG="${WORK}/log" FAKE_CLAUDE_DELAY=5
command mkdir -p "${WORK}/cfg" "${WORK}/data" "${WORK}/proj"
printf '[commands]\nclaude = "%s/fake_claude.py"\n' "${HERE}" > "${WORK}/cfg/config.toml"

"${BIN}" serve --no-global > "${WORK}/serve.log" 2>&1 &
SERVE=$!
sleep 3
ID="$("${BIN}" spawn "${WORK}/proj" --name t1 --rtk)"
: > "${WORK}/log"

"${BIN}" send "${ID}" "slow one" --rtk > /dev/null
sleep 1
"${BIN}" send "${ID}" "second queued" --rtk > /dev/null
"${BIN}" send "${ID}" /model sonnet --rtk > /dev/null
"${BIN}" send "${ID}" /effort low --rtk > /dev/null
sleep 14

fail=0
check() { # <what> <condition command...>
    local what="$1"
    shift
    if "$@"; then echo "ok   ${what}"; else echo "FAIL ${what}"; fail=1; fi
}
check "the model changed" bash -c "'${BIN}' list --rtk | grep -q 'claude/sonnet'"
check "the running prompt was sent again" grep -qF 'PROMPT "slow one"' "${WORK}/log"
check "the queued prompt reached the agent" grep -qF 'PROMPT "second queued"' "${WORK}/log"
check "--model sonnet given to the new agent" grep -qF '"--model", "sonnet"' "${WORK}/log"
exit "${fail}"
