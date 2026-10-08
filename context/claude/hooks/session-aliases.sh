#!/usr/bin/env bash
# SessionStart hook: injects the user's interactive zsh aliases into Claude's context
# so commands that are aliased (e.g. to `lock`) get bypassed with `\cmd` or `command cmd`.
aliases=$(timeout 10 zsh -ic 'print -r -- "@@BEGIN@@"; alias' 2>/dev/null </dev/null \
  | sed -n '/@@BEGIN@@/,$p' | sed 1d)

[ -z "$aliases" ] && exit 0

echo "## Shell aliases active in this environment (zsh)"
echo "Bash commands run with these aliases loaded. If you need the real binary for any"
echo "aliased name, prefix it: \`\\cmd\` or \`command cmd\` (e.g. \`\\git status\`, \`command mkdir -p x\`)."
echo
trap_aliases=$(grep -E "^[^=]+=('?)lock" <<<"$aliases" | cut -d= -f1 | tr '\n' ' ')
[ -n "$trap_aliases" ] && echo "DANGER — aliased to \`lock\` (locks the user's session), NEVER call bare: $trap_aliases" && echo
echo "Full alias list:"
echo "$aliases"
