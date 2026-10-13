#!/usr/bin/env bash
# claude-cluster installed: its Android app built when the Android SDK is there (served to the phones by its web access)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
prefix="${1:-$HOME/.local}"
version="$(sed -n 's/^project([^ ]* VERSION \([0-9.]*\).*/\1/p' "$here/CMakeLists.txt")"
exec bash "$here/android/build.sh" "$version" "$prefix/bin/claude-cluster"
