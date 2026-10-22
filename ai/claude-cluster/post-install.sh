#!/usr/bin/env bash
# claude-cluster installed: its Android app built when the Android SDK is there (served to the phones by its web access)
# The app is optional: a failure of its build is told, never fails the install of claude-cluster itself
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
prefix="${1:-$HOME/.local}"
version="$(sed -n 's/^project([^ ]* VERSION \([0-9.]*\).*/\1/p' "$here/CMakeLists.txt")"
if ! bash "$here/android/build.sh" "$version" "$prefix/bin/claude-cluster"; then
    echo "claude-cluster: the Android app was not built (see above); claude-cluster itself is installed" >&2
fi
