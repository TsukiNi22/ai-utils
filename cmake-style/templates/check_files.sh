#!/bin/bash

# Colors definition
export CYAN="\033[36m"
export RESET="\033[0m"

# Vars definition
mode=$1
shift
files="$@"

# Execution
if [ "$mode" = "unregistered" ]; then
    echo -e "╔═════ 🔻 [${CYAN}Unregistered Files${RESET}] 🔻 ═════╗"
    find src -name "*.cpp" -printf "src/%P\n" | while read file; do
        echo "$files" | grep -q "$file" || echo "$file"
    done
    echo -e "╚═════ 🔺 [${CYAN}Unregistered Files${RESET}] 🔺 ═════╝"
elif [ "$mode" = "unknown" ]; then
    echo -e "╔════════ 🔻 [${CYAN}Unknown Files${RESET}] 🔻 ════════╗"
    for file in $files; do
        [ -f "$file" ] || echo "$file"
    done
    echo -e "╚════════ 🔺 [${CYAN}Unknown Files${RESET}] 🔺 ════════╝"
fi

# Remove the colors
unset CYAN
unset RESET
