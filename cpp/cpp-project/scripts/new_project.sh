#!/bin/bash
# Create a new C++ project from cpp_project_template (same renaming as its setup.sh, without git actions).
#
# Usage: new_project.sh <target_dir> <project_name> <core_name> [--source <path|url>] [--no-libutils]
#
#   <target_dir>    directory of the new project (created, must be empty if it exists)
#   <project_name>  replaces 'template' (target, namespace, folders), ex: virtual-os
#   <core_name>     replaces 'Core' (main class), ex: Os
#   --source        template repository (default: ~/personal_delivery/cpp/cpp_project_template if present,
#                   otherwise https://github.com/TsukiNi22/cpp_project_template.git)
#   --no-libutils   remove the libutils parts (find_package, exception generation, ArgParser init)

set -euo pipefail

usage() { sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

[[ $# -lt 3 ]] && usage 1
TARGET="$1"; NAME="$2"; CORE="$3"; shift 3
SOURCE=""
LIBUTILS=true
while [[ $# -gt 0 ]]; do
    case "$1" in
        --source) SOURCE="$2"; shift ;;
        --no-libutils) LIBUTILS=false ;;
        -h|--help) usage 0 ;;
        *) echo "Error: unknown argument '$1'" >&2; usage 1 ;;
    esac
    shift
done

[[ "$NAME" =~ ^[a-z][a-z0-9_-]*$ ]] || { echo "Error: project name must be lower case (a-z, 0-9, '-', '_')" >&2; exit 1; }
[[ "$CORE" =~ ^[A-Z][A-Za-z0-9]*$ ]] || { echo "Error: core name must be PascalCase" >&2; exit 1; }
if [[ -d "$TARGET" ]] && [[ -n "$(ls -A "$TARGET" 2>/dev/null | grep -v '^\.git$' || true)" ]]; then
    echo "Error: $TARGET is not empty" >&2; exit 1
fi

# =========================
# Copy the template (committed files only)
# =========================
LOCAL="$HOME/personal_delivery/cpp/cpp_project_template"
if [[ -z "$SOURCE" ]]; then
    [[ -d "$LOCAL/.git" ]] && SOURCE="$LOCAL" || SOURCE="https://github.com/TsukiNi22/cpp_project_template.git"
fi
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
if [[ -d "$SOURCE/.git" ]]; then
    git -C "$SOURCE" archive --format=tar HEAD | tar -x -C "$TMP"
    FROM="$SOURCE @ $(git -C "$SOURCE" rev-parse --short HEAD)"
else
    git clone -q --depth 1 "$SOURCE" "$TMP/clone"
    FROM="$SOURCE @ $(git -C "$TMP/clone" rev-parse --short HEAD)"
    rm -rf "$TMP/clone/.git"
    mv "$TMP/clone"/* "$TMP/clone"/.[!.]* "$TMP"/ 2>/dev/null || true
    rmdir "$TMP/clone"
fi

# Template only files
rm -f "$TMP/setup.sh" "$TMP/.github/workflows/setup.yml" "$TMP/scripts.zip"
sed -i "/# Don't run on template repository/{N;d;}" "$TMP/.github/workflows/build.yml"

# =========================
# Rename 'template' -> <project_name> and 'Core' -> <core_name>
# =========================
rename() { # <from> <to>
    find "$TMP" -depth -name "*$1*" -not -path "*/.git/*" | while read -r path; do
        new="$(dirname "$path")/$(basename "$path" | sed "s/$1/$2/g")"
        [[ "$path" != "$new" ]] && mv "$path" "$new"
    done
}
replace() { # <from> <to> <include patterns...>
    local from="$1" to="$2"; shift 2
    local args=()
    for p in "$@"; do args+=(--include="$p"); done
    grep -rlZE "${args[@]}" "\\b$from\\b" "$TMP" 2>/dev/null | xargs -0 -r sed -i "s/\\b$from\\b/$to/g"
}
rename template "$NAME"
replace template "$NAME" "*.cpp" "*.hpp" "CMakeLists.txt" ".gitignore" "*.yml"
rename Core "$CORE"
replace Core "$CORE" "*.cpp" "*.hpp" "CMakeLists.txt"
GUARD="$(echo "$CORE" | tr '[:lower:]' '[:upper:]')"
grep -rlZ "CORE_H" "$TMP" 2>/dev/null | xargs -0 -r sed -i "s/\\bCORE_H\\b/${GUARD}_H/g"

# A namespace can't contain '-': use the project name without it
NS="$(echo "$NAME" | tr -d '-')"
if [[ "$NS" != "$NAME" ]]; then
    grep -rlZE --include="*.cpp" --include="*.hpp" "namespace $NAME\\b|\\b$NAME::" "$TMP" 2>/dev/null \
        | xargs -0 -r sed -i "s/namespace $NAME\\b/namespace $NS/g; s/\\b$NAME::/$NS::/g"
fi

# =========================
# Without libutils
# =========================
if ! $LIBUTILS; then
    rm -rf "$TMP/cmake/config" "$TMP/cmake/scripts/const.py" "$TMP/cmake/scripts/generate_exception_header.py" "$TMP/cmake/scripts/requirements.txt"
    echo "warning: --no-libutils: rewrite main.cpp / ${CORE}-init.cpp and the CMakeLists.txt (cmake-style / cpp-class std mode)" >&2
fi

# Doxygen project name
[[ -f "$TMP/Doxyfile" ]] && sed -i "s|^PROJECT_NAME *=.*|PROJECT_NAME           = \"$NAME\"|" "$TMP/Doxyfile"

# Edition date of the headers = today (like the nvim header update)
TODAY="$(date +%d/%m/%Y)"
grep -rlZ --include="*.cpp" --include="*.hpp" "@date .* by @author" "$TMP" 2>/dev/null \
    | xargs -0 -r sed -i "s|##  @date [0-9/]* by @author|##  @date $TODAY by @author|"

# =========================
# Basic README
# =========================
cat > "$TMP/README.md" <<EOF
# ${NAME}

Project ${NAME}, look for future update...
EOF

command mkdir -p "$TARGET"
cp -r "$TMP"/. "$TARGET"/
python3 "$(dirname "$0")/update_gitignore.py" "$TARGET" > /dev/null || echo "warning: .gitignore not updated" >&2
echo "Project '$NAME' (core '$CORE', namespace '$NS') created in '$TARGET' from $FROM"
