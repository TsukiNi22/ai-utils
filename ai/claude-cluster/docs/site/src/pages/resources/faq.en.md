+++
title = "FAQ"
summary = "Short answers: data and privacy, agents and models, access from outside, platforms, and common questions."
section = "resources"
order = 30
keywords = "faq questions privacy data license mit ollama windows mac iphone free outside bypass tokens cost"
+++

## Data and privacy {#privacy}

**Does claude-cluster send my data anywhere?** No telemetry. The only network traffic is what you start: your agents talk to their provider (Anthropic, OpenAI, Ollama on your machine...), the optional voice APIs (`groq`, `deepgram`, `elevenlabs`, `cartesia`) you configured, the usage endpoint of claude.ai read with your Claude Code token, and the web access you opened. This documentation is served by your own computer and uses no external resource.

**Where are my conversations?** On your computer, in the agent's own store and in `~/.local/share/claude-cluster/logs/`. See [Data formats](page:data-formats).

**What does the global session read?** The map of your computer (`machine.md`) is read by it and therefore sent to its API provider; the prompt history keeps what you typed in clear in your data folder. See [Security](page:security#privacy).

**Is it GDPR compliant?** The GDPR governs the processing of personal data by a controller. claude-cluster is software you run yourself: its data stays on your computer, there is no service of ours that receives it, so it does not apply to claude-cluster itself. It still applies to what you do with the providers you connect.

## Agents and models {#agents}

**Do I need Claude?** No: Claude Code is the default and the complete driver, but Ollama (local), Anthropic-compatible endpoints, OpenAI-compatible ones (Qwen Code), opencode and the OpenAI Codex CLI work too. The global session needs a claude-driver provider.

**Why does the global session run in `bypassPermissions`?** It orchestrates: asking before each tool would defeat the purpose. The tools it holds are bounded and the protected settings are out of its reach; you can set `[global] permission_mode = "auto"` to have it ask. See [Permissions](page:permissions-models).

**How much does it cost?** claude-cluster is free (MIT licence); the models cost what their provider charges. The *Tokens*, *Cost* and *Usage* panels and `/usage` show it, and `[limits]` caps it per day, week and month.

## Using it {#using}

**Category or tag?** One category per session (its primary label), several tags. Both live in the [taxonomy](page:taxonomy).

**How do I start over?** Palette *Start over*: every sub-session goes to the trash, queued tasks stop and the global session gets a new conversation.

**A session is stuck.** `Ctrl+C` stops the turn, F4 or `/retry` asks again after an API error, `/continue` goes on after a stop. A session that exited is started again on its conversation.

**Can two sessions share a folder?** Yes; they are independent processes, so they can step on each other's files like two terminals would. Use a fork, a worktree or one session per folder.

## Access from outside {#outside}

**Can I use it from my phone away from home?** Yes: `[web] remote = tailscale` (recommended: nothing is opened to the Internet) or `router`. The Android app follows by itself. See [Interfaces](page:interfaces#web) and [Security](page:security).

**Is there an iPhone app?** No: the web page works in the iPhone browser, with the certificate steps shown on the login page.

## Platforms {#platforms}

**Which systems?** Linux: Fedora and Debian / Ubuntu are documented. The window needs Qt6, the voice its local tools, the phone app the Android SDK at build time.

**Where do I report a bug?** On the repository, with the version and commit shown at the bottom of this page and, if useful, the debug trace (`[debug] enabled = true`).
