#!/bin/bash
# Detect how attributes must be written in the given project.
#
# Usage: detect_libutils.sh [project_dir]
#
# Output (first line):
#   internal  -> the project IS libutils: use the macros (_hot, _nodiscard...) with relative includes
#   libutils  -> libutils is linked in the CMake or installed: use the macros + <utils/utils.hpp>
#   std       -> no libutils: use the standard attributes ([[gnu::hot]], [[nodiscard]]...)
# Output (second line): the reason of the choice

set -uo pipefail # no -e: a grep without match is an answer, not an error (xstyle: ignore-file SH-STRICT)

PROJECT="${1:-.}"
CMAKE="$PROJECT/CMakeLists.txt"

# The project is libutils itself
if [[ -f "$PROJECT/include/utils/attribute/Attribute.hpp" ]] && [[ -f "$PROJECT/include/utils/utils.hpp" ]]; then
    echo "internal"
    echo "project is libutils ($PROJECT/include/utils)"
    exit 0
fi

# libutils referenced by the CMake (find_package, linked target or embedded lib)
if [[ -f "$CMAKE" ]]; then
    if tr '\n' ' ' < "$CMAKE" | grep -qE 'find_package\(\s*utils\b|utils::|target_link_libraries\([^)]*[[:space:]](utils|utils_debug|\$<[^>]*:utils(_debug)?>)[[:space:])]|libutils'; then
        echo "libutils"
        echo "referenced in $CMAKE"
        exit 0
    fi
fi

# libutils installed on the computer
for dir in /usr/include /usr/local/include "$HOME/.local/include"; do
    if [[ -f "$dir/utils/attribute/Attribute.hpp" ]]; then
        echo "libutils"
        echo "installed in $dir/utils"
        exit 0
    fi
done

echo "std"
echo "libutils not found (CMake & system)"
