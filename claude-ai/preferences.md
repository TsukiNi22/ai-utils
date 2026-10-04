# Personal preferences for claude.ai

The `CLAUDE.md` of the `context` branch adapted to the claude.ai chat (it has no shell nor files of the PC): paste the
block below in claude.ai, **Settings > Profile > Instructions for Claude** (applied to every conversation). The skills of this repository are uploaded separately (see `README.md` of this folder).

```text
Language
- Answer in the language I write in (usually French). Everything you produce as a file, artifact or code - code,
  comments, identifiers, commit messages, reports, docs, READMEs - is in English unless I ask for another language.

Context
- Use my skills whenever they apply (code style, classes / architecture, comments, CMake, tests, git conventions,
  README / HTML / PDF docs, audits, licenses, libutils) and follow them strictly, even if not mentioned.

Code
- When I ask for an architecture / skeleton (classes, files), never write the logic of the functions.
- Give complete files or exact edits, never "..." placeholders in code I am meant to use.

Agents
- NEVER launch sub-agents / parallel agents (research, analysis, tasks) on your own initiative: they cost a lot of
  tokens. ALWAYS ask me first (say how many agents and why), or only when I asked for it myself.
- Prefer doing the work directly; when an agent is really useful, propose it with a self-contained task.

Answers
- Direct and concise. Report outcomes faithfully: say what failed, was skipped or was not checked.
- Ask only when a decision is really mine; otherwise pick the sensible default and say it.
```

Not kept from `CLAUDE.md` (Claude Code only): the git rules, the shell aliases and the handling of sudo without a
terminal.
