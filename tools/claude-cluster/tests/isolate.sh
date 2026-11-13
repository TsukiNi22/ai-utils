#!/usr/bin/env bash
# Sourced by every test right after its CLAUDE_CLUSTER_* exports (WORK = its temporary folder): refuses the real files of the
# user (config, data, socket) and gives the test a home of its own, so that nothing a core under test resolves from HOME
# (the default prompts, ~/.claude.json, the tools' caches...) can reach the real ones.
REAL_HOME="$(getent passwd "$(id -u)" | cut -d: -f6)"
for var in CLAUDE_CLUSTER_CONFIG_DIR CLAUDE_CLUSTER_DATA_DIR CLAUDE_CLUSTER_SOCKET; do
    value="${!var:-}"
    case "${value}" in
        "" | "${REAL_HOME}/.config/claude-cluster"* | "${REAL_HOME}/.local/share/claude-cluster"* | "${XDG_RUNTIME_DIR:-/nonexistent}/claude-cluster"*)
            echo "isolate: ${var}='${value}' is not isolated (the real files of the user): refused" >&2
            exit 2
            ;;
    esac
done
if [[ -z "${WORK:-}" || "${WORK}" == "${REAL_HOME}" || "${WORK}" == "/" ]]; then
    echo "isolate: WORK='${WORK:-}' is not a temporary folder: refused" >&2
    exit 2
fi
command mkdir -p "${WORK}/home"
export HOME="${WORK}/home"
unset XDG_CONFIG_HOME XDG_DATA_HOME
