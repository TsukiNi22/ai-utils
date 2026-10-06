#!/bin/bash
# Create a throwaway git repository to try the xstyle pre-commit hook (clean / warning / refused / no xstyle).
#
# Usage: demo_repo.sh [dir] [install_hook.sh options...]   (default dir: /tmp/xstyle-hook-demo, recreated)
# The demo files hold their issues on purpose: xstyle: ignore-file G-TODO,G-TRAILING

set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR="${1:-/tmp/xstyle-hook-demo}"
[[ $# -gt 0 ]] && shift
case "$DIR" in
    /tmp/*) ;;
    *) [[ ! -e "$DIR" ]] || [[ -f "$DIR/TRY.md" ]] || { echo "Error: $DIR exists and is not a demo repository" >&2; exit 1; } ;;
esac
[[ -f "$DIR/TRY.md" ]] && rm -rf "$DIR"
mkdir -p "$DIR/src"
cd "$DIR"
git init -q
git config user.name "xstyle demo"
git config user.email "demo@example.com"

# One file per verdict of the hook (default config: fail on major, warn on minor)
cat > src/clean.py << 'EOF'
def add(a: int, b: int) -> int:
    return a + b
EOF
cat > src/minor.py << 'EOF'
def sub(a: int, b: int) -> int:
    return a - b # TODO: check the underflow
EOF
cat > src/major.cpp << 'EOF'
int main(void)
{
    char* p = NULL;
    return p == nullptr ? 0 : 1;
}
EOF
cat > src/fixable.sh << 'EOF'
#!/bin/bash
set -euo pipefail
if [[ -n "${1:-}" ]]; then
    echo "$1"   
fi
EOF
cat > TRY.md << 'EOF'
# xstyle pre-commit hook: try it

Hook: `.git/hooks/pre-commit` (config block at its top). Each scenario is independent.

| Try | Command | Expected |
|---|---|---|
| clean file | `git add src/clean.py && git commit -m clean` | commit accepted, nothing printed |
| minor issue | `git add src/minor.py && git commit -m minor` | issue listed + warning, commit accepted |
| major issue | `git add src/major.cpp && git commit -m major` | issue listed, **commit refused** (exit 1) |
| auto fixable | `git add src/fixable.sh && git commit -m sh` | trailing space: minor warning (or fixed with `--fix safe`) |
| fix then commit | `xstyle --fix --staged && git add -u && git commit -m major` | accepted once fixed |
| xstyle missing | `XSTYLE_BIN=/nonexistent git commit -m major` | warning "xstyle not found", commit accepted |
| skip once | `git commit --no-verify -m major` or `XSTYLE_HOOK=0 git commit ...` | no check |
| only staged lines | edit a committed file, stage one line | only the staged lines are checked |
| other config | `curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh \| bash -s -- hook --fail-on minor` (or `--fix safe`, `--scope files`, `--missing fail`...) | new behavior |
| state | `curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh \| bash -s -- hook status` | config of the hook |

Undo the last commit to retry: `git reset --soft HEAD~1`. Recreate everything: `demo_repo.sh` again (or `curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh \| bash -s -- hook` in a new repository).
EOF
"$SELF_DIR/install_hook.sh" install "$@" | sed 's/^/ /'
echo "Demo repository ready: $DIR (scenarios in $DIR/TRY.md)"
