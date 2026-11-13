#!/usr/bin/env bash
# Regression test: every end of turn started on behalf of the global session calls it back once - the first prompt of a spawn,
# a turn woken up by a Monitor / background task, an API error (weekly limit) - and the task queue fails with a clear reason; a callback
# owed is saved, a session closed before it answered is told.
# Isolated core + a fake `claude` (tests/fake_claude_wake.py); the global session is the fake too, its log shows what it received.
# Usage: tests/callbacks.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
source "$(dirname "${BASH_SOURCE[0]}")/isolate.sh"   # refuses the real config / data / socket of the user, a home of its own
export FAKE_CLAUDE_LOG="${WORK}/log"
command mkdir -p "${WORK}/cfg" "${WORK}/data" "${WORK}/proj"
printf '[commands]\nclaude = "%s/fake_claude_wake.py"\n[agent]\nsub_spawn = true\n' "${HERE}" > "${WORK}/cfg/config.toml"

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
count() { grep -cF -- "$1" "${WORK}/log" || true; }
expect() { # <what> <pattern> <times>
    local n; n="$(count "$2")"
    if [[ "${n}" == "$3" ]]; then echo "ok   $1"; else echo "FAIL $1 (${n} instead of $3)"; fail=1; fi
}

# (1) the answer of the first prompt of a spawned session comes back; (2) so does the turn a Monitor wakes it up for
call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"watcher\",\"prompt\":\"arm a monitor\"}" > /dev/null
sleep 6
expect "spawn: the first answer comes back" "reply from watcher" 2
expect "spawn: the wake-up turn comes back" "woken by the monitor" 1

# (2b) a build in its background: "waiting" comes back, then the turn claude-cluster wakes it for (its <task-notification>) with the
# real result - before, that last one never came back (the global session waited for nothing)
call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"builder\",\"prompt\":\"run the build\"}" > /dev/null
sleep 8
expect "background build: its waiting answer comes back" "waiting for it" 1
expect "background build: the real result after the wake-up comes back" "final result after the build" 1
expect "background build: the waiting answer says its result follows (no polling)" "its background still runs" 1

# (3) an API error comes back once, as an error
call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"failing\",\"prompt\":\"please fail\"}" > /dev/null
sleep 4
expect "error: the global session is told once" "error from failing" 1

# the opt-out: no reply asked
call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"quiet\",\"prompt\":\"hello\",\"reply\":false}" > /dev/null
sleep 3
expect "reply:false: nothing comes back" "from quiet" 0

# (4) tasks: a missing target fails with its reason, its dependent too; a task to a free session is answered
T1="$(call task_add "{\"prompt\":\"lost\",\"target\":\"s99\"}")"
sleep 1
LIST="$(call task_list '{}')"
if grep -qF "does not exist" <<< "${LIST}"; then echo "ok   task to a missing session: its reason is listed"; else echo "FAIL task to a missing session"; fail=1; fi
call task_add "{\"prompt\":\"work\",\"target\":\"quiet\"}" > /dev/null
sleep 4
expect "task: its answer comes back" "reply from quiet" 1

# (5) a callback owed is saved (it survives a restart of the core); the session closed before it answered: the global session is
# told, never left waiting
call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"slowpoke\",\"prompt\":\"a slow job\"}" > /dev/null
sleep 2
owed() { python3 -c 'import json, sys; print(len(json.load(open(sys.argv[1])).get("replies", {})))' "${WORK}/data/state.json"; }
if [[ "$(owed)" == 1 ]]; then echo "ok   the callback owed is in the saved state"; else echo "FAIL the callback owed is not saved ($(owed))"; fail=1; fi
call session_close '{"session":"slowpoke"}' > /dev/null
sleep 3
expect "closed before it answered: the global session is told" "[closed: slowpoke" 1
if [[ "$(owed)" == 0 ]]; then echo "ok   nothing owed left after the close"; else echo "FAIL still owed after the close ($(owed))"; fail=1; fi

# (6) a sub-session (watcher, s1) queues a task with a session of its own: its result comes back to it as [tasks], and the turn its
# Monitor wakes it for as [reply from helper ...] - to the sub-session that asked, not to the global session
WATCHER=s1   # the first session spawned by this test
CLAUDE_CLUSTER_SESSION="${WATCHER}" call task_add "{\"prompt\":\"arm a monitor\",\"spawn\":{\"cwd\":\"${WORK}/proj\",\"name\":\"helper\"}}" > /dev/null
sleep 8
expect "task of a sub-session: its result told to it" "[tasks] task t3 done" 1
expect "task of a sub-session: the woken turn of its session told to it" "reply from helper" 1
if [[ "${fail}" != 0 ]]; then echo "--- what the global session received:"; cut -c1-200 "${WORK}/log"; fi
exit "${fail}"
