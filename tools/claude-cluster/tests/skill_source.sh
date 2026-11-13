#!/usr/bin/env bash
# Regression test: the skills / commands offered by the completion of `/` tell where they come from (local, built-in, plugin, imported link),
# an imported skill is "<link>:<skill>" and reaches the agent as "/<skill>".
# Isolated core + a fake `claude` (tests/fake_claude.py). Usage: tests/skill_source.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
trap 'kill "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
source "$(dirname "${BASH_SOURCE[0]}")/isolate.sh"   # refuses the real config / data / socket of the user, a home of its own
export FAKE_CLAUDE_LOG="${WORK}/log" FAKE_CLAUDE_SKILLS="demo,plug:thing,mine" FAKE_CLAUDE_COMMANDS="demo,plug:thing,mine,compact"
command mkdir -p "${WORK}/cfg" "${WORK}/data/plugins" "${WORK}/proj"
printf '[commands]\nclaude = "%s/fake_claude.py"\n' "${HERE}" > "${WORK}/cfg/config.toml"
# an imported link (slug owner-repo) that gave the skill "demo", linked now
printf '[{"url":"owner/repo","slug":"owner-repo","kind":"git","dir":"/x","marketplace":"","error":"","items":[{"id":"owner-repo:demo","name":"demo","kind":"skill","description":"","path":"/x/demo","installed":true,"enabled":true}]}]' \
    > "${WORK}/data/plugins/index.json"

"${BIN}" serve --no-global > "${WORK}/serve.log" 2>&1 &
SERVE=$!
sleep 3
ID="$("${BIN}" spawn "${WORK}/proj" --name t1 --rtk)"
sleep 2

# the control socket: one JSON request per line, the answer printed as one compact line (the entries as "label|source")
ask() { # <request>
    python3 -I -c '
import json, socket, sys
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1])
s.sendall((sys.argv[2] + "\n").encode()); buf = b""
while not buf.endswith(b"\n"): buf += s.recv(65536)
data = json.loads(buf).get("data")
print(" ".join(i["label"] + "|" + i.get("source", "") for i in data) if isinstance(data, list) else json.dumps(data))
' "${CLAUDE_CLUSTER_SOCKET}" "$1"
}
fail=0
check() { # <what> <entry> <text>
    if grep -qwF -- "$2" <<< "$3"; then echo "ok   $1"; else echo "FAIL $1 ($2 not in: $3)"; fail=1; fi
}
ITEMS="$(ask "{\"cmd\":\"complete\",\"session\":\"${ID}\",\"kind\":3,\"query\":\"\"}")"
check "imported skill: <link>:<skill>, its link as source" "owner-repo:demo|owner-repo" "${ITEMS}"
check "plugin skill: its plugin as source" "plug:thing|plug" "${ITEMS}"
check "user skill: local" "mine|local" "${ITEMS}"
check "local command of claude-cluster: local" "cd|local" "${ITEMS}"
check "command of the agent: built-in" "compact|built-in" "${ITEMS}"
if grep -qwF 'demo|' <<< "${ITEMS}"; then echo "FAIL the plain imported skill is listed too"; fail=1; else echo "ok   the imported skill is listed once"; fi

ask "{\"cmd\":\"send\",\"session\":\"${ID}\",\"text\":\"/owner-repo:demo hello\"}" > /dev/null
ask "{\"cmd\":\"send\",\"session\":\"${ID}\",\"text\":\"/plug:thing hello\"}" > /dev/null
sleep 3
if grep -qF 'PROMPT "/demo hello"' "${WORK}/log"; then echo "ok   an imported skill reaches the agent as /<skill>"; else echo "FAIL an imported skill reaches the agent as /<skill>"; fail=1; fi
if grep -qF 'PROMPT "/plug:thing hello"' "${WORK}/log"; then echo "ok   a plugin skill is sent as it is"; else echo "FAIL a plugin skill is sent as it is"; fail=1; fi
exit "${fail}"
