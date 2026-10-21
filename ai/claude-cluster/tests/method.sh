#!/usr/bin/env bash
# Regression test: the work method - injected in the system prompts of the sessions (sub-sessions and global), read / sampled / proposed by the
# global session (MCP work_method), never applied before the user's Allow, protected against settings_set.
# Isolated core + a fake `claude` (tests/fake_claude.py) logging its arguments.
# Usage: tests/method.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
source "$(dirname "${BASH_SOURCE[0]}")/isolate.sh"   # refuses the real config / data / socket of the user, a home of its own
export FAKE_CLAUDE_LOG="${WORK}/log"
command mkdir -p "${WORK}/cfg" "${WORK}/data" "${WORK}/proj"
printf '[commands]\nclaude = "%s/fake_claude.py"\n[agent]\nwork_method = "one-task"\nwork_method_text = "be brief"\nwork_method_detect = true\nwork_method_every = 1\n' "${HERE}" > "${WORK}/cfg/config.toml"

"${BIN}" serve > "${WORK}/serve.log" 2>&1 &
SERVE=$!
sleep 4
"${BIN}" spawn "${WORK}/proj" --name t1 --rtk > /dev/null
sleep 2

fail=0
check() { # <what> <pattern> <text>
    if grep -qF -- "$2" <<< "$3"; then echo "ok   $1"; else echo "FAIL $1 (wanted: $2)"; fail=1; fi
}
absent() { # <what> <pattern> <text>
    if grep -qF -- "$2" <<< "$3"; then echo "FAIL $1 (found: $2)"; fail=1; else echo "ok   $1"; fi
}

# the method reaches the system prompt of the sub-session and of the global session
ARGV="$(grep '^ARGV' "${WORK}/log")"
check "the sub-session is told the method" 'One task at a time: finish and verify' "${ARGV}"
check "the free text is added" 'be brief' "${ARGV}"
check "the global session splits accordingly" 'Split the requests you dispatch accordingly' "${ARGV}"

# the MCP server of the global session
mcp() { # <tool> <arguments json>: the text of the answer
    {
        printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
            '{"jsonrpc":"2.0","method":"notifications/initialized"}'
        printf '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"%s","arguments":%s}}\n' "$1" "$2"
        sleep 2
    } | "${BIN}" mcp 2>/dev/null | grep '"id":2'
}
check "work_method reads the current method" 'one-task' "$(mcp work_method '{}')"
check "the sample counts the prompts" 'prompts_read' "$(mcp work_method '{"action":"sample"}')"
check "the setting is protected" 'by the user only' "$(mcp settings_set '{"section":"agent","key":"work_method","value":"free"}')"
check "the spawn tells the detection is due" 'work_method_due' "$(mcp session_spawn "{\"cwd\":\"${WORK}/proj\",\"name\":\"t2\"}")"
check "an unknown preset is refused" 'unknown preset' "$(mcp work_method '{"action":"propose","preset":"nope"}')"
check "the current method is not proposed again" 'already' "$(mcp work_method '{"action":"propose","preset":"one-task","text":"be brief"}')"

# a proposal: only the user's Allow writes the setting (a Deny keeps it)
answer() { # <label>: the question of the global session answered as the user would (the socket of the core)
    python3 -I - "${CLAUDE_CLUSTER_SOCKET}" "$1" <<'PY'
import json, socket, sys
def call(request):
    s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); s.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"): data += s.recv(65536)
    return json.loads(data)
status = call({"cmd": "status", "session": "g"})["data"]
ask = [p for p in status["permissions"] if p["request_id"].startswith("core-")][0]
call({"cmd": "allow", "session": "g", "request_id": ask["request_id"], "answers": {ask["questions"][0]["question"]: sys.argv[2]}})
PY
}
check "a proposal is asked, not applied" 'asked' "$(mcp work_method '{"action":"propose","preset":"plan-first","reason":"you ask for plans"}')"
absent "the setting is unchanged before the answer" 'plan-first' "$(cat "${WORK}/cfg/config.toml")"
answer Deny
sleep 2
absent "a Deny keeps the method" 'plan-first' "$(cat "${WORK}/cfg/config.toml")"
check "the proposal is cleared" '\"method\":\"one-task\"' "$(mcp work_method '{}')"
check "a second proposal is asked" 'asked' "$(mcp work_method '{"action":"propose","preset":"list-review","text":"review twice"}')"
answer Allow
sleep 3
check "an Allow writes the method" 'work_method = "list-review"' "$(cat "${WORK}/cfg/config.toml")"
check "an Allow writes its text" 'review twice' "$(cat "${WORK}/cfg/config.toml")"

# the detection off: no sample, no proposal
sed -i 's/work_method_detect = true/work_method_detect = false/' "${WORK}/cfg/config.toml"
sleep 3
check "no sample with the detection off" 'detection of the work method is off' "$(mcp work_method '{"action":"sample"}')"
check "no proposal with the detection off" 'detection of the work method is off' "$(mcp work_method '{"action":"propose","preset":"free"}')"
exit "${fail}"
