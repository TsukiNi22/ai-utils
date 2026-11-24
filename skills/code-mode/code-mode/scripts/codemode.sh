#!/bin/sh
# Run codemode.py with the first Python 3.9+ found (python3, python, py): for the hooks of the skill and for shells
# where "python3" does not exist (Git Bash on Windows). Usage: codemode.sh <the arguments of codemode.py>

dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
for python in python3 python py; do
    if command -v "$python" > /dev/null 2>&1 \
        && "$python" -c 'import sys; sys.exit(sys.version_info < (3, 9))' > /dev/null 2>&1; then
        exec "$python" "$dir/codemode.py" "$@"
    fi
done
echo "codemode: no Python 3.9 or newer found (python3, python, py)" >&2
exit 127
