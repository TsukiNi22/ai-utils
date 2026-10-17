#!/bin/bash
# Create a new C++ project with the setup.sh of cpp_project_template (never commits nor pushes).
#
# Usage: new_project.sh <target_dir> <project_name> <core_name> [--kind binary|library|header-only]
#                       [--gh-pages|--no-gh-pages] [--repo <owner/repo>] [--source <path|url>] [--no-libutils]
#
#   <target_dir>    directory of the new project (created; existing: only a .git, a LICENSE or hidden files in it)
#   <project_name>  replaces 'template' (target, namespace, folders, packages): a C++ identifier, ex: virtual_os
#   <core_name>     replaces 'Core' (main class): a C++ identifier, ex: Os
#   --kind          binary (default), library (.a / .so + headers) or header-only
#   --gh-pages      the gh-pages branch (signed packages repository + docs) and its workflows (default: none)
#   --repo          owner/repo of the GitHub repository (default: the origin remote of <target_dir>, else
#                   TsukiNi22/<project_name>): workflows, docs, packages repository
#   --source        template repository (default: ~/personal_delivery/cpp/cpp_project_template if present,
#                   otherwise https://github.com/TsukiNi22/cpp_project_template.git); a local clone gives its
#                   committed files only (HEAD), never its work in progress
#   --no-libutils   remove the exception generation of libutils (the CMake / main.cpp / init to rewrite: told)

set -euo pipefail

usage() { sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

[[ $# -lt 3 ]] && usage 1
TARGET="$1"
NAME="$2"
CORE="$3"
shift 3
KIND="binary"
PAGES="--no-gh-pages"
REPO=""
SOURCE=""
LIBUTILS=true
while [[ $# -gt 0 ]]; do
    case "$1" in
        --kind) [[ $# -ge 2 ]] || usage 1; KIND="$2"; shift ;;
        --gh-pages|--no-gh-pages) PAGES="$1" ;;
        --repo) [[ $# -ge 2 ]] || usage 1; REPO="$2"; shift ;;
        --source) [[ $# -ge 2 ]] || usage 1; SOURCE="$2"; shift ;;
        --no-libutils) LIBUTILS=false ;;
        -h|--help) usage 0 ;;
        *) echo "error: unknown option '$1'" >&2; usage 1 ;;
    esac
    shift
done

# =========================
# Checks
# =========================
case "$KIND" in
    binary|library|header-only) ;;
    *) echo "error: --kind binary, library or header-only (not '$KIND')" >&2; exit 1 ;;
esac
for name in "$NAME" "$CORE"; do
    if ! [[ "$name" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]]; then
        echo "error: '$name' is not a C++ identifier (namespace / class): use '_' instead of '-', ex: ${name//-/_}" >&2
        exit 1
    fi
done
if [[ -d "$TARGET" ]] && [[ -n "$(find "$TARGET" -mindepth 1 -maxdepth 1 ! -name '.*' ! -name LICENSE -print -quit)" ]]; then
    echo "error: '$TARGET' is not empty (only a .git, a LICENSE or hidden files are kept): the project would be mixed with it" >&2
    exit 1
fi

# =========================
# The template: committed files of a local clone, or its repository
# =========================
LOCAL_DEFAULT="$HOME/personal_delivery/cpp/cpp_project_template"
if [[ -z "$SOURCE" ]]; then
    if [[ -d "$LOCAL_DEFAULT/.git" ]]; then SOURCE="$LOCAL_DEFAULT"; else SOURCE="https://github.com/TsukiNi22/cpp_project_template.git"; fi
fi
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
if [[ -d "$SOURCE/.git" ]]; then
    \git -C "$SOURCE" archive HEAD | tar -x -C "$TMP"
    FROM="$SOURCE (HEAD $(\git -C "$SOURCE" log -1 --format=%h))"
    [[ -n "$(\git -C "$SOURCE" status --porcelain 2> /dev/null)" ]] && echo "note: the clone has uncommitted changes: not used (HEAD only)" >&2
else
    \git clone -q --depth 1 "$SOURCE" "$TMP"
    rm -rf "$TMP/.git"
    FROM="$SOURCE"
fi
[[ -f "$TMP/setup.sh" ]] || { echo "error: no setup.sh in the template ($FROM): too old, update the clone" >&2; exit 1; }

# =========================
# The setup.sh of the template, in the target (no question: every choice given; never --push)
# =========================
command mkdir -p "$TARGET"
TARGET="$(cd "$TARGET" && pwd)"
(
    cd "$TARGET"
    export TEMPLATE_DIR="$TMP"
    [[ -n "$REPO" ]] && export GITHUB_REPO="$REPO"
    bash "$TMP/setup.sh" "$NAME" "$CORE" --kind "$KIND" "$PAGES" < /dev/null
)

# =========================
# Without libutils
# =========================
if ! $LIBUTILS; then
    rm -rf "$TARGET/cmake/config" "$TARGET/cmake/scripts/const.py" "$TARGET/cmake/scripts/generate_exception_header.py" "$TARGET/cmake/scripts/requirements.txt"
    echo "warning: --no-libutils: rewrite main.cpp / ${CORE}-init.cpp, the CMakeLists.txt and tests/CMakeLists.txt (cmake-style, cpp-class std mode)" >&2
fi

python3 "$(dirname "$0")/update_gitignore.py" "$TARGET" > /dev/null || echo "warning: .gitignore not updated" >&2
echo "Project '$NAME' ($KIND, core '$CORE', ${PAGES#--}) created in '$TARGET' from $FROM"
