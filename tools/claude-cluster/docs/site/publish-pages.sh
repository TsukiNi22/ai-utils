#!/usr/bin/env bash
# Builds the static copy of the claude-cluster documentation and puts it in the claude-cluster/ folder of a checkout of the gh-pages branch
# (nothing else of that branch is touched: its graph explorer and its index stay as they are), with a LOCAL commit only. Nothing is pushed:
# the last line tells the command to run when the result looks right (or the workflow claude-cluster-docs.yml does all of it on GitHub).
# Usage: publish-pages.sh <checkout of gh-pages> [--code-root DIR]
set -euo pipefail

TARGET="${1:?checkout of the gh-pages branch}"
shift || true
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "${OUT}"' EXIT

if [[ "$(command git -C "${TARGET}" rev-parse --abbrev-ref HEAD)" != "gh-pages" ]]; then
    echo "${TARGET} is not a checkout of the gh-pages branch" >&2
    exit 1
fi

python3 "${HERE}/build.py" --out "${OUT}" "$@"
python3 "${HERE}/tools/check.py" "${OUT}"
if command -v node > /dev/null; then
    node "${HERE}/tools/test_search.js" "${OUT}"
fi

command mkdir -p "${TARGET}/claude-cluster"
find "${TARGET}/claude-cluster" -mindepth 1 -delete
cp -r "${OUT}/." "${TARGET}/claude-cluster/"
touch "${TARGET}/.nojekyll"

command git -C "${TARGET}" add claude-cluster .nojekyll
if command git -C "${TARGET}" diff --cached --quiet; then
    echo "claude-cluster/ is already up to date"
    exit 0
fi
version="$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['version'])" "${OUT}/search-index.json")"
command git -C "${TARGET}" commit -q -m "docs(claude-cluster): documentation site ${version}"
echo "committed locally in ${TARGET}: review it, then push it yourself (git -C ${TARGET} push origin gh-pages)"
