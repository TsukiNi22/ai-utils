#!/bin/bash
# Inventory of the documentation surfaces of a repository, versions found, dead links.
#
# Usage: inventory.sh [repo_dir]
set -uo pipefail

##### Variables #####
REPO="$(command cd "${1:-.}" && command pwd)"
command cd "$REPO" || exit 1

section() { printf '\n== %s ==\n' "$1"; }

##### Surfaces #####
section "documentation files"
\git ls-files 2>/dev/null | grep -Ei '(^|/)(readme|changelog|contributing|license|notice|third_party|doxyfile|mkdocs\.yml)|\.(1|rst)$|^docs/|completion' \
    | head -80

section "branches with published docs"
\git branch -a 2>/dev/null | grep -Ei 'gh-pages|docs|pages' || echo "(none)"
wiki="../$(basename "$REPO").wiki"
if [ -d "$wiki" ]; then echo "wiki clone: $wiki"; else echo "wiki: no local clone (<repo>.wiki.git)"; fi

section "package / manifest / install files"
\git ls-files 2>/dev/null | grep -Ei '(^|/)(CMakeLists\.txt|pyproject\.toml|setup\.py|package\.json|Cargo\.toml|.*\.spec|control|Dockerfile|marketplace\.json|plugin\.json|setup\.sh|install\.sh)$' | head -40

##### Versions #####
section "versions found"
echo "last tag: $(\git describe --tags --abbrev=0 2>/dev/null || echo none)"
grep -rhoiE 'project\([^)]*VERSION[ ]+[0-9][0-9.]*' --include=CMakeLists.txt . 2>/dev/null | head -5 | sed 's/^/cmake: /'
grep -hE '^(version|__version__)\s*=' pyproject.toml setup.py 2>/dev/null | head -3 | sed 's/^/python: /'
grep -hE '"version"\s*:' package.json 2>/dev/null | head -1 | sed 's/^/npm: /'
grep -hE '^version\s*=' Cargo.toml 2>/dev/null | head -1 | sed 's/^/cargo: /'
grep -hiE '^##? .*v?[0-9]+\.[0-9]+\.[0-9]+' CHANGELOG.md 2>/dev/null | head -2 | sed 's/^/changelog: /'

##### Staleness #####
section "last change of the docs vs the code"
printf 'docs : %s\n' "$(\git log -1 --format='%h %cs %s' -- README.md CHANGELOG.md docs 2>/dev/null | cut -c1-100)"
printf 'code : %s\n' "$(\git log -1 --format='%h %cs %s' -- . ':!README.md' ':!CHANGELOG.md' ':!docs' 2>/dev/null | cut -c1-100)"
base="$(\git log -1 --format=%H -- README.md CHANGELOG.md docs 2>/dev/null)"
[ -n "$base" ] && printf 'commits since the last docs change: %s\n' "$(\git rev-list --count "$base"..HEAD)"

section "links of the Markdown docs to missing files"
for doc in README.md $(\git ls-files 'docs/*.md' 2>/dev/null); do
    [ -f "$doc" ] || continue
    grep -oE '\]\(([^)#: ]+)(#[^)]*)?\)' "$doc" | sed -E 's/^\]\(([^)#]+).*/\1/' | sort -u | while read -r p; do
        [ -e "$(dirname "$doc")/$p" ] || printf '%s -> %s (missing)\n' "$doc" "$p"
    done
done | head -40

section "uncommitted changes (what the docs must follow)"
\git status --short | head -30
exit 0
