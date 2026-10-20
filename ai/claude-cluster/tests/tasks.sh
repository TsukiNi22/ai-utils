#!/usr/bin/env bash
# Regression test: the task queue (chains with {{tN.result}}, own session, file event, priority, budget), the schedules (dry run, cron,
# command without model, pause / resume / edit, history) and the scoped MCP of a sub-session.
# Isolated core + a fake `claude` (tests/fake_claude_wake.py); the global session is the fake too, its log shows what it received.
# Usage: tests/tasks.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
export FAKE_CLAUDE_LOG="${WORK}/log"
command mkdir -p "${WORK}/cfg" "${WORK}/data" "${WORK}/proj"
printf '[commands]\nclaude = "%s/fake_claude_wake.py"\n[agent]\nsub_schedules = true\nsub_spawn = false\n' "${HERE}" > "${WORK}/cfg/config.toml"

"${BIN}" serve > "${WORK}/serve.log" 2>&1 &
SERVE=$!
sleep 4

# one MCP call: tool name, JSON arguments -> the text answer (SUB: the id of a sub-session -> its own MCP)
call() {
    {
        printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
            '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
            "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"$1\",\"arguments\":$2}}"
        sleep 2
    } | CLAUDE_CLUSTER_SESSION="${SUB:-}" "${BIN}" mcp 2>/dev/null | grep '"id":2'
}
tools() { # the tool names a sub-session sees
    {
        printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
            '{"jsonrpc":"2.0","id":3,"method":"tools/list"}'
        sleep 2
    } | CLAUDE_CLUSTER_SESSION="$1" "${BIN}" mcp 2>/dev/null | grep '"id":3' | grep -o '"name":"[a-z_]*"' | tr '\n' ' '
}

fail=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; fail=1; }
has() { # <what> <text> <pattern>
    if grep -qF -- "$3" <<< "$2"; then ok "$1"; else bad "$1 (no '$3' in: ${2:0:300})"; fi
}
hasnt() {
    if grep -qF -- "$3" <<< "$2"; then bad "$1 ('$3' found)"; else ok "$1"; fi
}
count() { grep -cF -- "$1" "${WORK}/log" || true; }
idof() { grep -o '\\"id\\":\\"[a-z][0-9]*' | head -1 | grep -o '[a-z][0-9]*$'; } # the id in the answer of a tool

# a session to work in
SPAWNED="$(call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"worker\",\"reply\":false}")"
sleep 2
ID="$(idof <<< "${SPAWNED}")"

# (1) a chain: the result of the first task is in the prompt of the second
A="$(call task_add '{"prompt":"alpha job","target":"worker"}' | idof)"
call task_add "{\"prompt\":\"second needs {{${A}.result}}\",\"target\":\"worker\"}" > /dev/null
sleep 6
LIST="$(call task_list '{}')"
has "task result kept" "${LIST}" '\"result\":\"ok done\"'
if [[ "$(count 'second needs ok done')" == 1 ]]; then ok "placeholder {{tN.result}} replaced"; else bad "placeholder not replaced: $(grep -c 'second needs' "${WORK}/log")"; fi

# (2) a task with its own session
call task_add "{\"prompt\":\"own session job\",\"spawn\":{\"cwd\":\"${WORK}/proj\",\"name\":\"made-by-task\"}}" > /dev/null
sleep 6
LIST="$(call task_list '{}')"
has "spawn task: its session was made" "${LIST}" 'made-by-task'
has "spawn task: done" "${LIST}" '\"spawned\":true'

# (3) an event: a file appears
call task_add "{\"prompt\":\"after the file\",\"target\":\"worker\",\"on_event\":{\"type\":\"file\",\"path\":\"${WORK}/ready\"}}" > /dev/null
sleep 8
if [[ "$(count 'after the file')" == 0 ]]; then ok "event: waits while the file is missing"; else bad "event: ran too early"; fi
touch "${WORK}/ready"
sleep 14
if [[ "$(count 'after the file')" == 1 ]]; then ok "event: runs once the file exists"; else bad "event: did not run ($(count 'after the file'))"; fi

# (4) bad arguments are refused
has "unknown event type refused" "$(call task_add '{"prompt":"x","on_event":{"type":"nope"}}')" 'on_event type'
has "unknown dependency refused" "$(call task_add '{"prompt":"x","after":["t999"]}')" "no task"

# (5) schedules: dry run, cron, rules refused
DRY="$(call schedule_add '{"text":"x","cron":"*/15 9-17 * * mon-fri","tz":"Europe/Paris","dry_run":true}')"
has "dry run: when it would fire" "${DRY}" 'would_fire'
has "bad cron refused" "$(call schedule_add '{"text":"x","cron":"99 * * * *"}')" 'cron'
has "bad timezone refused" "$(call schedule_add '{"text":"x","when":"+1h","tz":"Mars/Base"}')" 'timezone'

# (6) a command without any model: only a non-empty output is sent
call schedule_add '{"action":"command","when":"+1s","command":"echo build-is-red","text":"watch"}' > /dev/null
call schedule_add '{"action":"command","when":"+1s","command":"true","text":"silent"}' > /dev/null
sleep 8
if [[ "$(count 'build-is-red')" == 1 ]]; then ok "command schedule: its output reaches the global session"; else bad "command output ($(count 'build-is-red'))"; fi
if [[ "$(count 'scheduled w2')" == 0 ]]; then ok "command schedule: no output, nothing sent"; else bad "silent command sent something"; fi

# (7) a condition not met skips the run; pause / resume / edit / history
W="$(call schedule_add '{"text":"never","every":1,"if":{"command":"false"}}' | idof)"
has "pause" "$(call schedule_act "{\"id\":\"${W}\",\"action\":\"pause\"}")" 'paused'
has "edit while paused" "$(call schedule_edit "{\"id\":\"${W}\",\"text\":\"still never\",\"every\":5}")" 'still never'
has "resume" "$(call schedule_act "{\"id\":\"${W}\",\"action\":\"resume\"}")" 'resumed'
call schedule_act "{\"id\":\"${W}\",\"action\":\"snooze\",\"minutes\":120}" > /dev/null
has "list: rules and next times" "$(call schedule_list "{\"id\":\"${W}\"}")" '\"then\"'
call schedule_cancel "{\"id\":\"${W}\"}" > /dev/null

# (8) the MCP of a sub-session: its own tools, scoped
TOOLS="$(tools "${ID}")"
has "sub-session: task_add" "${TOOLS}" '"task_add"'
has "sub-session: task_list" "${TOOLS}" '"task_list"'
hasnt "sub-session: nothing of the global session" "${TOOLS}" '"session_spawn"'
SUB="${ID}"
export SUB
has "sub-session: a local command is refused" "$(call schedule_add '{"action":"command","when":"+1h","command":"id"}')" 'not allowed'
has "sub-session: a spawn is refused" "$(call task_add "{\"prompt\":\"x\",\"spawn\":{\"cwd\":\"${WORK}/proj\"}}")" 'not allowed'
has "sub-session: another session refused" "$(call task_add '{"prompt":"x","target":"s999"}')" 'no session'
MINE="$(call task_add '{"prompt":"for myself"}')"
has "sub-session: queues for itself" "${MINE}" '"id"'
SEEN="$(call task_list '{}')"
has "sub-session: sees its own" "${SEEN}" 'for myself'
hasnt "sub-session: does not see the others" "${SEEN}" 'alpha job'
unset SUB

if [[ "${fail}" != 0 ]]; then echo "--- log:"; cut -c1-200 "${WORK}/log" | tail -30; echo "--- serve:"; tail -20 "${WORK}/serve.log"; fi
exit "${fail}"
