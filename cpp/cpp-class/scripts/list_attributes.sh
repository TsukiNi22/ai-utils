#!/bin/bash
# List the attribute macros really available from libutils for the given project.
#
# Usage: list_attributes.sh [project_dir]
#
# Looks for utils/attribute/Attribute.hpp hard imported in the project first (include/, lib/,
# external/, FetchContent _deps...), then installed on the computer, and prints every
# `#define _x` of Attribute.hpp and of the C++ version file used (c++20.hpp, else c++17/c++14).
# Exit code 1 = no attribute file found (write the standard [[...]] form).

set -uo pipefail

PROJECT="${1:-.}"

# Hard import of the project first, then the system
FOUND=""
while IFS= read -r file; do
    FOUND="$file"
    break
done < <(find "$PROJECT" -path "*/utils/attribute/Attribute.hpp" -not -path "*/.git/*" 2>/dev/null | sort)
if [ -z "$FOUND" ]; then
    for dir in /usr/include /usr/local/include "$HOME/.local/include"; do
        if [ -f "$dir/utils/attribute/Attribute.hpp" ]; then
            FOUND="$dir/utils/attribute/Attribute.hpp"
            break
        fi
    done
fi
if [ -z "$FOUND" ]; then
    echo "no utils/attribute/Attribute.hpp found (project & system)"
    exit 1
fi

DIR="$(dirname "$FOUND")"
echo "# $DIR"
for version in c++20 c++17 c++14; do
    if [ -f "$DIR/$version.hpp" ]; then
        grep -hE "^\s*#define _[a-z]" "$DIR/Attribute.hpp" "$DIR/$version.hpp" | sed -E 's/^\s+//' | sort -u -t' ' -k2,2
        break
    fi
done
grep -q "hardware_destructive_interference_size" "$DIR/Attribute.hpp" && echo "# std::hardware_destructive_interference_size / std::hardware_constructive_interference_size guaranteed (fallback 64)"
exit 0
