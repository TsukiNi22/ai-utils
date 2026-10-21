#!/usr/bin/env bash
# Regression test: the documentation embedded in the binary - served at /docs without a login (the version and the commit of the binary filled in,
# one ETag per build and 304, framed by the same origin only, no path out of the table), searched by /docs/search (c2dmp-hsm: a typo found, both
# languages), the PDF of the audit, the command `claude-cluster docs` and the MCP tool `docs`. Isolated core (own config / data / socket / port).
# Needs a build with the docs (python3 3.11+). Usage: tests/docs.sh [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
set -euo pipefail

BIN="${1:-/tmp/cc-build/claude-cluster}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(mktemp -d)"
PORT=18792
URL="http://127.0.0.1:${PORT}"
trap 'kill -9 "${SERVE:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT

export CLAUDE_CLUSTER_CONFIG_DIR="${WORK}/cfg" CLAUDE_CLUSTER_DATA_DIR="${WORK}/data" CLAUDE_CLUSTER_SOCKET="${WORK}/sock"
command mkdir -p "${WORK}/cfg" "${WORK}/data"

fail=0
check() { # <what> <condition: 0 = ok>
    if [[ "$2" == 0 ]]; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi
}

printf '[commands]\nclaude = "%s/fake_claude.py"\n[web]\nenabled = true\nport = %s\nhttps = false\n' "${HERE}" "${PORT}" > "${WORK}/cfg/config.toml"
"${BIN}" serve --no-global > "${WORK}/serve.log" 2>&1 < /dev/null &
SERVE=$!
sleep 4

version="$("${BIN}" --version | awk '{print $2}')"
home="$(command curl -s -D "${WORK}/head" "${URL}/docs/")"
grep -qi '^HTTP/1.1 200' "${WORK}/head"; check "/docs/ answers without a login" "$?"
grep -q "claude-cluster docs" <<< "${home}"; check "the home page is the documentation" "$?"
grep -q "v${version} " <<< "${home}" && ! grep -q '@@CC_' <<< "${home}"; check "its version is the one of the binary, no token left" "$?"
grep -qi '^X-Frame-Options: SAMEORIGIN' "${WORK}/head" && grep -qi '^Cache-Control: public' "${WORK}/head"; check "framed by its origin only, cached by the browser" "$?"
etag="$(grep -i '^ETag:' "${WORK}/head" | cut -d' ' -f2- | tr -d '\r')"
code="$(command curl -s -o /dev/null -w '%{http_code}' -H "If-None-Match: ${etag}" "${URL}/docs/")"
[[ "${code}" == 304 ]]; check "the same ETag: 304" "$?"
command curl -s -D "${WORK}/head2" -o /dev/null "${URL}/ca.crt"
grep -qi '^X-Frame-Options: DENY' "${WORK}/head2"; check "the other pages stay DENY" "$?"
[[ "$(command curl -s -o /dev/null -w '%{http_code}' "${URL}/docs")" == 301 ]]; check "/docs goes to /docs/" "$?"
[[ "$(command curl -s -o /dev/null -w '%{http_code}' --path-as-is "${URL}/docs/../api/state")" != 200 ]]; check "no path out of the table" "$?"
[[ "$(command curl -s -o /dev/null -w '%{http_code}' "${URL}/docs/nothing.html")" == 404 ]]; check "an unknown page: 404" "$?"
[[ "$(command curl -s -o /dev/null -w '%{content_type}' "${URL}/docs/site.css")" == text/css* ]]; check "the style is served as css" "$?"
command curl -s "${URL}/docs/downloads/claude-cluster-audit-report.pdf" | head -c 4 | grep -q '%PDF'; check "the audit report PDF is served" "$?"

en="$(command curl -s "${URL}/docs/search?q=forq&lang=en")"
grep -qi 'fork' <<< "${en}"; check "search: a typo (forq) finds fork" "$?"
fr="$(command curl -s "${URL}/docs/search?q=sous-sesion&lang=fr")"
grep -q 'Sous-sessions' <<< "${fr}"; check "search: French, a typo (sous-sesion)" "$?"
tool="$(command curl -s "${URL}/docs/search?q=session_send&lang=en")"
grep -q '"k":"tools"' <<< "${tool}"; check "search covers the MCP tools" "$?"
set_="$(command curl -s "${URL}/docs/search?q=taxonomy_edit&lang=en")"
grep -q '"k":"settings"' <<< "${set_}"; check "search covers the settings" "$?"
[[ "$(command curl -s "${URL}/docs/search?q=zzzzqqqq&lang=en")" == '{"results":[]}' ]]; check "search: nothing found, an empty list" "$?"

out="$("${BIN}" docs search orchestrator < /dev/null)"
grep -qi 'orchestrator' <<< "${out}"; check "claude-cluster docs search" "$?"
link="$("${BIN}" docs url settings < /dev/null)"
[[ "${link}" == file://*settings.html ]] && [[ -f "${link#file://}" ]]; check "claude-cluster docs url: the copy on disk" "$?"
noshow="$(env -u DISPLAY -u WAYLAND_DISPLAY "${BIN}" docs open < /dev/null)"
grep -q 'no display' <<< "${noshow}"; check "claude-cluster docs open without a display: its address told" "$?"

mcp() {
    {
        printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
            '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
            '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"docs","arguments":{"action":"search","query":"heavy_run"}}}' \
            '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"docs","arguments":{"action":"page","page":"settings"}}}'
        sleep 2
    } | "${BIN}" mcp 2> /dev/null
}
answer="$(mcp)"
grep -q 'heavy_run' <<< "${answer}"; check "MCP tool docs: search" "$?"
grep -q 'settings.html' <<< "${answer}"; check "MCP tool docs: the page of a screen" "$?"

kill "${SERVE}" 2> /dev/null || true
exit "${fail}"
