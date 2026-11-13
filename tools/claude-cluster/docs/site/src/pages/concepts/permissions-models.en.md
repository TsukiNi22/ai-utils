+++
title = "Permissions and models policy"
summary = "Permission modes, who may approve what, and the model and thinking policy the global session follows."
section = "concepts"
order = 110
keywords = "permissions mode auto manual acceptEdits plan dontAsk bypassPermissions model policy effort thinking costly routing advise force allowed"
+++

## Permission modes {#modes}

| Mode | Meaning |
|---|---|
| `auto` | the classifier of Claude Code allows the safe actions (default for new sessions: `[agent] permission_mode`) |
| `manual` (= `default`) | every tool asks |
| `acceptEdits` | file edits are accepted, the rest asks |
| `plan` | read only, a plan first |
| `dontAsk` | what is not allowed is denied without asking |
| `bypassPermissions` | never asks (default of the global session: `[global] permission_mode`) |

`Shift+Tab` cycles auto, acceptEdits, plan, bypassPermissions, manual; `/mode <mode>`, `claude-cluster mode <s> <mode>` or `session_set_mode` set one. Sessions are launched with `--allow-dangerously-skip-permissions` (`[agent] allow_bypass`) so bypass can be chosen later; the mode shown is the one Claude Code confirms. The other drivers start one process per turn, so their permissions follow the mode and cannot be asked live.

## Answering {#answering}

A pending permission is a popup on the session, a notification (with *Allow* / *Deny*) and a line in `session_status`. You answer with `Alt+Y`, `Alt+A` (always, keeps the rule) or `Alt+D`; the global session answers with `session_allow`. The reason of every permission is shown with it. In `auto` mode, when the classifier gives no verdict (a failure on its side) one counted line is shown, and after two in a row the session goes manual for the rest of the turn.

## Model and thinking policy {#policy}

The global session runs on `[global] model` / `effort` and **sets the model and thinking of the sub-sessions** by the difficulty of each task, within **Settings > Model policy** (`[policy]`, protected: it never changes them).

| Setting | What it does |
|---|---|
| default model / thinking | what sub-sessions start from |
| *How the default applies* (`model_mode`, `effort_mode`) | `free`: none, by the task; `advise`: the start, deviate for a reason; `force`: nothing else, other choices refused with the reason |
| allowed models / efforts | the only ones it may choose (a name or a part: `haiku, sonnet`) |
| ask before a costly choice | the **costly** models (`opus`, `fable`) and efforts (`xhigh`, `max`, and above): a question on every interface (Allow / Deny, a notification) |

Its system prompt always carries a **routing guide**: cheap model and low thinking for lookups and mechanical work, balanced for ordinary coding, premium and high thinking only for hard problems; start low and escalate on a real failure; never move a large session to a stronger model for one step (its whole context is re-read at the new price) but open a new sub-session with a short brief. The tool `models_policy` gives it your rules live: the models of its provider with tier, price, whether they are costly or allowed, and what a turn of each really cost you.

A costly choice returns at once as `asked`: the session keeps its model until you answer; Allow applies it and tells the global session, Deny keeps it and says why. These questions are yours alone. A change you make yourself (palette, `/model`, `claude-cluster model`) is never gated by the policy.

## Changing the model of a loaded session {#model-change}

The prompt cache belongs to a model: the next turn on another model reads the **whole context again**, written to its cache at full price. So a model changed while the context is still loaded (its cache alive, or a turn running) is **asked first**, in every interface: the popup tells how much (*84k tokens used by this session*, or the total of the *N sessions* of *every session*), **Cancel** is focused (a stray Enter changes nothing), **Change anyway** applies it. A cache already expired changes at once: everything is read again anyway.

## Backends {#backends}

`--backend` and `[agent] provider` take `<provider>[/<model>]`: `claude` (default), `anthropic-api`, `ollama`, `openrouter`, `deepseek`, `kimi`, `zai`, `minimax` (Claude Code on an Anthropic-compatible endpoint), `openai` and `ollama-openai` (Qwen Code), `opencode/<provider>/<model>`, `codex`, or your own `[providers.<name>]`. The **claude** driver is the complete one: one process per session, live permissions, the MCP server, Remote Control. `claude-cluster auth list` shows what is ready. The cost shown for a non-Anthropic provider through Claude Code is a guess of claude and shown as `local / unknown`.
