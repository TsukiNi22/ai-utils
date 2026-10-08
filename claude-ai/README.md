# Skills and preferences on claude.ai

Claude Code reads the skills from `~/.claude/skills` (installed by `setup.sh`) and the rules from `~/.claude/CLAUDE.md`
(`context/`). The claude.ai chat (web, desktop, mobile) has its own copies:

| What | claude.ai chat | Claude Code (terminal) | Claude Code on the web (cloud sessions) |
|---|---|---|---|
| skill uploaded on claude.ai | yes | yes (synced) | no |
| `.claude/skills` committed in a project | no | yes | yes |
| `CLAUDE.md` | no (use the preferences below) | yes | if committed in the project |
| plugin of the marketplace (`/plugin marketplace add TsukiNi22/ai-utils`) | no | yes | yes, added in the session |

## 1. Skills

Needs a Pro / Max / Team / Enterprise plan and **Settings > Capabilities > Code execution** enabled.

```bash
python3 claude-ai/build.py            # every skill -> dist/claude-ai/<skill>.zip (routers in dist/claude-ai/routers/)
python3 claude-ai/build.py html-doc   # only some skills
```

Then on claude.ai: **Customize > Skills > Add > Upload**, one archive at a time (the skill folder is at the top of
each archive, as expected). Re-run the script and upload again after a change of a skill.

- The routers (`dev`, `cpp`, `doc`, `audit`...) are manual `/commands` of Claude Code: upload them only if you want
  them in the chat, where they become normal skills (claude.ai refuses `disable-model-invocation`: the script keeps
  only the frontmatter keys it accepts).
- The paths `~/.claude/skills/<other>/` of a skill become `../<other>/` (the skills side by side); the scripts that
  need your PC (git of a local repository, cmake, sudo...) only work in Claude Code.

## 2. Preferences (the "system prompt" of the chat)

Paste the block of [`preferences.md`](preferences.md) in **Settings > Profile > personal preferences** (applied to
every conversation), or in the instructions of a project to limit it to that project.
