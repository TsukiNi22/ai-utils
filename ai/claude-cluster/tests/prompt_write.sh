#!/usr/bin/env bash
# Regression test: the global session (MCP origin) rewrites the system prompts with TWO settings,
# [sessions] rewrite_global_prompt (default off) and rewrite_sessions_prompt (default on); the old key
# global_prompts is the fallback of the first one only. Isolated core (no global session).
# Usage: tests/prompt_write.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT
export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
source "$(dirname "${BASH_SOURCE[0]}")/isolate.sh"   # refuses the real config / data / socket of the user, a home of its own
command mkdir -p "${WORK}/cfg" "${WORK}/data"

fail=0
# <config.toml content> -> "<global write result> <sessions write result>" as the global session (origin mcp)
run_case() {
    printf '%s\n' "$1" > "${WORK}/cfg/config.toml"
    "${BIN}" serve --no-global > "${WORK}/serve.log" 2>&1 &
    SERVE=$!
    sleep 2
    python3 -I - "${WORK}/sock" <<'P'
import json, socket, sys
def ask(cmd, name):
    s = socket.socket(socket.AF_UNIX)
    s.connect(sys.argv[1])
    s.sendall((json.dumps({"cmd": cmd, "name": name, "content": "x\n", "origin": "mcp"}) + "\n").encode())
    out = json.loads(s.makefile().readline())
    s.close()
    return out
res = []
for name in ("global_prompt", "sessions_prompt"):
    out = ask("config_write", name)
    res.append("ok" if out.get("ok", not out.get("error")) and not out.get("error") else "refused:" + str(out.get("error")))
files = {f["name"]: f["editable"] for f in ask("config_files", "")["data"]}
print(res[0].split(":")[0], res[1].split(":")[0], files["global_prompt"], files["sessions_prompt"])
print(res[0] + " | " + res[1], file=sys.stderr)
P
    kill "${SERVE}"
    wait "${SERVE}" 2>/dev/null || true
}
check() { # <what> <expected> <config>
    local got
    got="$(run_case "$3" 2> "${WORK}/err")"
    if [[ "${got}" == "$2" ]]; then echo "ok   $1"; else echo "FAIL $1: got '${got}', want '$2'"; cat "${WORK}/err"; fail=1; fi
}
check "defaults: global refused, sessions allowed" "refused ok False True" "[sessions]"
check "both on" "ok ok True True" $'[sessions]\nrewrite_global_prompt = true'
check "sessions off" "ok refused True False" $'[sessions]\nrewrite_global_prompt = true\nrewrite_sessions_prompt = false'
check "old key: global on, sessions untouched" "ok ok True True" $'[sessions]\nglobal_prompts = true'
check "new key wins over the old one" "refused ok False True" $'[sessions]\nglobal_prompts = true\nrewrite_global_prompt = false'
exit "${fail}"
