#!/usr/bin/env bash
# Regression test: the resource orchestrator - the queue (limits, order, priorities), cluster-run (exit code, output, queued line,
# loss of the wrapper, the low profile), heavy_run / jobs_add (results, callbacks), kill, stop / resume, the cleanup when a session is
# archived, the pressure (a fake /proc: hold, reduce, resume), the scoping of a sub-session, the core restarted.
# Isolated core + a fake `claude`; the heavy commands are `sleep` / `echo`.
# Usage: tests/orchestrator.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster; cluster-run next to it)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
RUN="$(dirname "${BIN}")/cluster-run"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; kill "${BG1:-0}" "${BG2:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
export FAKE_CLAUDE_LOG="${WORK}/log"
command mkdir -p "${WORK}/cfg" "${WORK}/data" "${WORK}/proj" "${WORK}/proc/pressure"

# a fake /proc: 4 cpus, the load and the free memory set by the test
setproc() { # <load1> <mem_available_kb>
    printf '%s 0.00 0.00 1/100 1\n' "$1" > "${WORK}/proc/loadavg"
    printf 'MemTotal: 16000000 kB\nMemFree: 8000000 kB\nMemAvailable: %s kB\nSwapTotal: 2000000 kB\nSwapFree: 2000000 kB\n' "$2" > "${WORK}/proc/meminfo"
}
for i in 0 1 2 3; do printf 'processor\t: %s\nmodel name\t: Fake CPU\n\n' "${i}"; done > "${WORK}/proc/cpuinfo"
setproc 0.10 12000000

write_config() { # <extra [orchestrator] lines>
    {
        printf '[commands]\nclaude = "%s/fake_claude_wake.py"\n' "${HERE}"
        printf '[orchestrator]\nprofile = "low"\nmax_jobs = 2\nmax_per_session = 2\nreserved_ram_mb = 1000\nhold_swap_mb = 100000\nresume_after = 3\nproc_dir = "%s/proc"\n' "${WORK}"
        printf '%s\n' "$1"
    } > "${WORK}/cfg/config.toml"
}
write_config 'kill_on_oom = false'

start_core() {
    "${BIN}" serve > "${WORK}/serve.log" 2>&1 &
    SERVE=$!
    sleep 4
}
start_core

# one MCP call: tool name, JSON arguments -> the text answer (SUB: the id of a sub-session -> its own MCP, scoped)
call() {
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
has() { # <what> <text> <pattern>
    if grep -qF -- "$3" <<< "$2"; then ok "$1"; else bad "$1 (no '$3' in: ${2:0:400})"; fi
}
hasnt() {
    if grep -qF -- "$3" <<< "$2"; then bad "$1 ('$3' found)"; else ok "$1"; fi
}
jid() { grep -o '\\"id\\":\\"j[0-9]*' <<< "$1" | head -1 | grep -o 'j[0-9]*' || true; } # the first job id of an answer (escaped JSON)
status() { call job_status "{\"id\":\"$1\"}"; }
wait_state() { # <job> <text> <seconds>: until its status holds the text
    local i out=""
    for ((i = 0; i < $3; ++i)); do
        out="$(status "$1")"
        if grep -qF -- "$2" <<< "${out}"; then return 0; fi
        sleep 1
    done
    echo "${out:0:300}" >&2
    return 1
}
jid_named() { # the id of the latest job of this name
    call jobs_list '{"state":"all"}' | python3 -c 'import json, sys
text = json.loads(sys.stdin.read())["result"]["content"][0]["text"]
print(next((j["id"] for j in reversed(json.loads(text)["jobs"]) if j.get("name") == sys.argv[1]), ""))' "$1"
}
pid_of() { grep -o '\\"pid\\":[0-9]*' <<< "$1" | head -1 | grep -o '[0-9]*$' || true; }

# (1) the hardware scan and the cluster-run wrapper's own explanation of the share it grants
SCAN="$(call hardware_scan '{}')"
has "hardware scan: its version" "${SCAN}" 'scan_version'
has "hardware scan: the fake cpu count" "${SCAN}" 'threads\":4'
[[ -f "${WORK}/data/hardware.json" ]] && ok "hardware scan: saved" || bad "hardware scan: not saved"
EXPLAIN="$("${RUN}" --explain --threads 2 -- make -j64 all)"
has "wrapper: -jN lowered to the share" "${EXPLAIN}" 'command: make -j2 all'
has "wrapper: MAKEFLAGS limited" "${EXPLAIN}" 'MAKEFLAGS=-j2'
has "wrapper: nproc limited (OMP_THREAD_LIMIT)" "${EXPLAIN}" 'OMP_THREAD_LIMIT=2'
has "wrapper: cmake --parallel lowered" "$("${RUN}" --explain --threads 3 -- cmake --build b --parallel 32)" 'cmake --build b --parallel 3'
has "wrapper: ninja gets -j" "$("${RUN}" --explain --threads 2 -- ninja all)" 'command: ninja -j2 all'
has "wrapper: a small -j is kept" "$("${RUN}" --explain --threads 4 -- make -j2)" 'command: make -j2'
has "wrapper: pytest -n auto" "$("${RUN}" --explain --threads 2 -- pytest -n auto)" 'pytest -n 2'
has "wrapper: a light command is not touched" "$("${RUN}" --explain --threads 2 -- ls -j9)" 'command: ls -j9'

# (2) heavy_run: finished (exit code, duration, tail), failed, the class guessed
OUT="$(call heavy_run '{"command":"echo hello-tail; exit 0","name":"say"}')"
has "heavy_run: finished" "${OUT}" 'finished'
has "heavy_run: exit 0" "${OUT}" 'exit 0'
has "heavy_run: the tail" "${OUT}" 'hello-tail'
OUT="$(call heavy_run '{"command":"echo oops-tail; exit 3"}')"
has "heavy_run: failed" "${OUT}" 'failed'
has "heavy_run: the exit code" "${OUT}" 'exit 3'
has "heavy_run: the tail of a failure" "${OUT}" 'oops-tail'
OUT="$(call jobs_add '{"command":"make -j8 test","callback":false}')"
has "class: make test is tests" "$(status "$(jid "${OUT}")")" 'class\":\"tests\"'
OUT="$(call jobs_add '{"command":"cmake --build build --parallel 8","callback":false}')"
has "class: cmake --build is build" "$(status "$(jid "${OUT}")")" 'class\":\"build\"'
sleep 2

# (3) the limits and the order: max_jobs = 2, four jobs, the priority decides who waits
A="$(jid "$(call jobs_add '{"command":"sleep 6","name":"a","prio":0,"callback":false}')")"
B="$(jid "$(call jobs_add '{"command":"sleep 6","name":"b","prio":0,"callback":false}')")"
C="$(jid "$(call jobs_add '{"command":"sleep 2","name":"c-low","prio":-5,"callback":false}')")"
D="$(jid "$(call jobs_add '{"command":"sleep 2","name":"d-high","prio":5,"callback":false}')")"
LIST="$(call jobs_list '{}')"
has "limits: two run at once" "${LIST}" 'running\":2'
has "limits: two wait" "${LIST}" 'waiting\":2'
has "queue: the high priority one is next" "$(status "${D}")" 'position\":1'
has "queue: the low priority one is last" "$(status "${C}")" 'position\":2'
has "queue: what it waits for" "$(status "${D}")" 'slot (2/2 running)'
has "queue: an ETA" "$(status "${D}")" 'eta_s'
D_STATE="$(call job_wait "{\"id\":\"${D}\",\"timeout\":30}")"
has "queue: the high priority one ran first and finished" "${D_STATE}" 'finished'
has "queue: the low one still waits or runs after it" "$(status "${C}")" '"state'
wait_state "${C}" 'finished' 30 && ok "queue: the low priority one ran at the end" || bad "queue: the low priority one did not finish"
wait_state "${A}" 'finished' 20 && ok "limits: the first ones finished" || bad "limits: the first ones did not finish"

# (4) kill: a waiting job (removed), a running job (its process gone), by whom
K1="$(jid "$(call jobs_add '{"command":"sleep 60","name":"k1","callback":false}')")"
K2="$(jid "$(call jobs_add '{"command":"sleep 60","name":"k2","callback":false}')")"
K3="$(jid "$(call jobs_add '{"command":"sleep 60","name":"k3","callback":false}')")"
sleep 1
has "kill: the third waits" "$(status "${K3}")" 'waiting'
PID1="$(pid_of "$(status "${K1}")")"
KILLED="$(call job_kill "{\"id\":\"${K3}\",\"reason\":\"not needed\"}")"
has "kill: a waiting job is killed by the global session" "${KILLED}" 'killed by global'
has "kill: its reason" "${KILLED}" 'not needed'
call job_kill "{\"id\":\"${K1}\"}" > /dev/null
sleep 2
if [[ -n "${PID1}" ]] && ! kill -0 "${PID1}" 2>/dev/null; then ok "kill: the process of a running job is gone"; else bad "kill: pid ${PID1} still there"; fi
has "kill: the next one took its slot" "$(status "${K2}")" 'running'

# (5) stop / resume: SIGSTOP of the group, partial status, the slot freed
PID2="$(pid_of "$(status "${K2}")")"
STOPPED="$(call job_stop "{\"id\":\"${K2}\"}")"
has "stop: status stopped" "${STOPPED}" 'stopped'
sleep 1
STATE="$(ps -o stat= -p "${PID2}" | tr -d ' ')"
[[ "${STATE}" == T* ]] && ok "stop: the process is stopped (${STATE})" || bad "stop: process state '${STATE}'"
RESUMED="$(call job_resume "{\"id\":\"${K2}\"}")"
has "resume: running again" "${RESUMED}" 'running'
has "resume: the status says it was partial" "$(status "${K2}")" 'partial'
STATE="$(ps -o stat= -p "${PID2}" | tr -d ' ')"
[[ "${STATE}" != T* ]] && ok "resume: the process runs (${STATE})" || bad "resume: still stopped"
call job_kill "{\"id\":\"${K2}\"}" > /dev/null
call job_prio "{\"id\":\"${K2}\",\"prio\":3}" > /dev/null 2>&1 || true

# (6) cluster-run: the exit code, the output, the queued line, the kill message, the loss of the wrapper
"${RUN}" -- sh -c 'echo wrapped-out; echo wrapped-err >&2; exit 7' > "${WORK}/o1" 2> "${WORK}/e1" && CODE=0 || CODE=$?
[[ "${CODE}" == 7 ]] && ok "cluster-run: the exit code is passed" || bad "cluster-run: exit ${CODE} instead of 7"
has "cluster-run: stdout is passed" "$(cat "${WORK}/o1")" 'wrapped-out'
has "cluster-run: stderr is passed" "$(cat "${WORK}/e1")" 'wrapped-err'
has "cluster-run: the low profile in the environment" "$("${RUN}" -- sh -c 'echo "$MAKEFLAGS $CARGO_BUILD_JOBS $CLUSTER_RUN_THREADS"')" '-j1 1 1'
NICE="$("${RUN}" -- sh -c 'ps -o ni= -p $$' | tr -d ' ')"
[[ "${NICE}" == 19 ]] && ok "cluster-run: nice 19 in the low profile" || bad "cluster-run: nice ${NICE}"
S1="$(jid "$(call jobs_add '{"command":"sleep 8","name":"s1","callback":false}')")"
S2="$(jid "$(call jobs_add '{"command":"sleep 8","name":"s2","callback":false}')")"
"${RUN}" --name queued-one -- sh -c 'echo ran-after-the-queue' > "${WORK}/o2" 2> "${WORK}/e2" &
BG1=$!
sleep 2
has "cluster-run: the queued line" "$(cat "${WORK}/e2")" 'queued: position 1'
has "cluster-run: waiting for a slot" "$(cat "${WORK}/e2")" 'slot'
wait "${BG1}" && ok "cluster-run: it ran when a slot freed" || bad "cluster-run: it failed"
has "cluster-run: its output after the queue" "$(cat "${WORK}/o2")" 'ran-after-the-queue'
"${RUN}" --name long -- sleep 60 > /dev/null 2> "${WORK}/e3" &
BG2=$!
sleep 1
LONG="$(jid_named long)"
call job_kill "{\"id\":\"${LONG}\",\"reason\":\"test kill\"}" > /dev/null || true
wait "${BG2}" && CODE=0 || CODE=$?
[[ "${CODE}" == 143 ]] && ok "cluster-run: killed by the core -> 143" || bad "cluster-run: exit ${CODE} after a kill (job '${LONG}')"
has "cluster-run: the message says by whom" "$(cat "${WORK}/e3")" 'killed by global'
# the wrapper killed (SIGKILL) while it waits and while it runs: the slot comes back
call job_kill "{\"id\":\"${S1}\"}" > /dev/null; call job_kill "{\"id\":\"${S2}\"}" > /dev/null
"${RUN}" --name lost -- sleep 60 > /dev/null 2>&1 &
BG1=$!
sleep 2
LOSTPID="$(pid_of "$(status "$(jid_named lost)")")"
kill -9 "${BG1}" 2>/dev/null || true
sleep 3
if [[ -n "${LOSTPID}" ]] && ! kill -0 "${LOSTPID}" 2>/dev/null; then ok "cluster-run: its command is terminated when the wrapper dies"; else bad "cluster-run: the command of a dead wrapper still runs (${LOSTPID})"; fi
has "cluster-run: the slot is free again" "$(call jobs_list '{}')" 'running\":0'

# (7) the callback: a sub-session added a job, it is woken up with its end
SPAWNED="$(call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"worker\",\"reply\":false}")"
sleep 2
SUBID="$(grep -o 's[0-9]*' <<< "${SPAWNED}" | head -1)"
SPAWNED2="$(call session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"other\",\"reply\":false}")"
sleep 2
OTHER="$(grep -o 's[0-9]*' <<< "${SPAWNED2}" | head -1)"
SUBTOOLS="$(tools "${SUBID}")"
has "sub: heavy_run offered" "${SUBTOOLS}" 'heavy_run'
has "sub: jobs_add offered" "${SUBTOOLS}" 'jobs_add'
has "sub: job_status offered" "${SUBTOOLS}" 'job_status'
hasnt "sub: job_stop not offered" "${SUBTOOLS}" 'job_stop'
hasnt "sub: orchestrator_config not offered" "${SUBTOOLS}" 'orchestrator_config'
hasnt "sub: hardware_scan not offered" "${SUBTOOLS}" 'hardware_scan'
SUB="${SUBID}" call jobs_add '{"command":"echo callback-tail","name":"cb"}' > /dev/null
sleep 5
if grep -qF '[job j' "${WORK}/log" && grep -qF 'callback-tail' "${WORK}/log"; then ok "callback: the owner is woken up with the end and the tail"; else bad "callback: not found in the log ($(grep -c 'job j' "${WORK}/log"))"; fi

# (8) scoping: a sub-session sees and kills its own jobs only; the global session sees all
MINE="$(jid "$(SUB="${SUBID}" call jobs_add '{"command":"sleep 60","name":"mine","callback":false}')")"
THEIRS="$(jid "$(SUB="${OTHER}" call jobs_add '{"command":"sleep 60","name":"theirs","callback":false}')")"
SUBLIST="$(SUB="${SUBID}" call jobs_list '{}')"
has "scope: a sub-session sees its job" "${SUBLIST}" "${MINE}"
hasnt "scope: a sub-session does not see the others" "${SUBLIST}" "${THEIRS}"
has "scope: the global session sees both" "$(call jobs_list '{}')" "${THEIRS}"
has "scope: a sub-session cannot status another's job" "$(SUB="${SUBID}" call job_status "{\"id\":\"${THEIRS}\"}")" 'of yours'
has "scope: a sub-session cannot kill another's job" "$(SUB="${SUBID}" call job_kill "{\"id\":\"${THEIRS}\"}")" 'of yours'
has "scope: it kills its own" "$(SUB="${SUBID}" call job_kill "{\"id\":\"${MINE}\"}")" 'killed by session'
has "scope: a sub-session may not change the settings" "$(SUB="${SUBID}" call orchestrator_config '{}')" 'off for the sub-sessions'

# (9) the archive: closing a session kills its running jobs and removes its queued ones
CLEAN1="$(jid "$(SUB="${OTHER}" call jobs_add '{"command":"sleep 60","name":"c1","callback":false}')")"
CLEAN2="$(jid "$(SUB="${OTHER}" call jobs_add '{"command":"sleep 60","name":"c2","callback":false}')")"
FILL="$(jid "$(call jobs_add '{"command":"sleep 60","name":"fill","callback":false}')")"
sleep 1
CLEANPID="$(pid_of "$(status "${THEIRS}")")"
call session_close "{\"session\":\"${OTHER}\"}" > /dev/null
sleep 3
ALL="$(call jobs_list '{"state":"all"}')"
has "archive: its running job was killed (archived)" "$(status "${THEIRS}")" 'archived'
has "archive: its queued job was removed" "$(status "${CLEAN2}")" 'archived'
if [[ -n "${CLEANPID}" ]] && ! kill -0 "${CLEANPID}" 2>/dev/null; then ok "archive: the process is gone"; else bad "archive: pid ${CLEANPID} still there"; fi
call job_kill "{\"id\":\"${FILL}\"}" > /dev/null || true

# (10) the pressure with a fake /proc: hold (nothing new), reduce (the lowest priority one is stopped), then resume with hysteresis
P1="$(jid "$(call jobs_add '{"command":"sleep 90","name":"p1","prio":5,"callback":false}')")"
P2="$(jid "$(call jobs_add '{"command":"sleep 90","name":"p2","prio":-5,"callback":false}')")"
sleep 1
has "pressure: both run when calm" "$(call jobs_list '{}')" 'running\":2'
setproc 8.0 12000000   # 2 per core: hold (nothing new starts)
sleep 3
P3="$(jid "$(call jobs_add '{"command":"sleep 5","name":"p3","callback":false}')")"
sleep 1
has "pressure hold: a new job waits" "$(status "${P3}")" 'waiting'
setproc 20.0 12000000  # 5 per core: reduce
sleep 6
has "pressure reduce: the lowest priority one is stopped by the pressure" "$(status "${P2}")" 'stopped by pressure'
has "pressure reduce: the other one runs" "$(status "${P1}")" 'running'
setproc 0.10 12000000  # calm again
sleep 9
has "pressure over: the stopped job resumed" "$(status "${P2}")" 'running'
call job_kill "{\"id\":\"${P1}\"}" > /dev/null; call job_kill "{\"id\":\"${P2}\"}" > /dev/null; call job_kill "{\"id\":\"${P3}\"}" > /dev/null || true
setproc 0.10 2500000   # 2.4 GB free (hold level): a job needing 2000 MB waits (RAM), unless nothing runs
sleep 2
R1="$(jid "$(call jobs_add '{"command":"sleep 20","name":"r1","callback":false}')")"
R2="$(jid "$(call jobs_add '{"command":"sleep 20","name":"r2","callback":false}')")"
sleep 2
has "RAM: nothing runs, one starts anyway" "$(status "${R1}")" 'running'
has "RAM: the next one waits for memory" "$(status "${R2}")" 'RAM'
call job_kill "{\"id\":\"${R1}\"}" > /dev/null; call job_kill "{\"id\":\"${R2}\"}" > /dev/null
setproc 0.10 12000000
sleep 5

# (11) the settings: the global session reads and changes them; the protected ones stay the user's
has "config: read" "$(call orchestrator_config '{}')" 'max_per_session'
has "config: set" "$(call orchestrator_config '{"key":"max_per_session","value":"3"}')" 'max_per_session'
has "config: a protected key refused" "$(call orchestrator_config '{"key":"sub_see_others","value":"true"}')" 'by the user only'
has "config: disable" "$(call orchestrator_config '{"action":"disable"}')" 'enabled\":false'
has "config: disabled, a wrapper runs at once" "$("${RUN}" -- echo free-run)" 'free-run'
call orchestrator_config '{"action":"enable"}' > /dev/null

# (12) the core restarted: a running job is settled (failed, why), a queued one of the core is queued again
RUN1="$(jid "$(call jobs_add '{"command":"sleep 120","name":"run-a","callback":false}')")"
RUN2="$(jid "$(call jobs_add '{"command":"sleep 120","name":"run-b","callback":false}')")"
QUE="$(jid "$(call jobs_add '{"command":"echo after-restart","name":"queued-c","callback":false}')")"
sleep 1
RUNPID="$(pid_of "$(status "${RUN1}")")"
kill "${SERVE}"; wait "${SERVE}" 2>/dev/null || true
sleep 2
start_core
OLD="$(status "${RUN1}")"
has "restart: a running job of the dead core is failed" "${OLD}" 'failed'
has "restart: with its reason" "${OLD}" 'core restarted'
if [[ -n "${RUNPID}" ]] && ! kill -0 "${RUNPID}" 2>/dev/null; then ok "restart: its process was reaped"; else bad "restart: pid ${RUNPID} still there"; fi
wait_state "${QUE}" 'queued' 1 > /dev/null 2>&1 || true
sleep 3
has "restart: a queued job of the core ran again" "$(status "${QUE}")" 'finished'

echo
if [[ "${fail}" == 0 ]]; then echo "orchestrator: all tests passed"; else echo "orchestrator: FAILURES"; exit 1; fi
