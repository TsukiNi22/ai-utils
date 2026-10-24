+++
title = "Security overview"
summary = "Threat model for local use, the LAN, the web access and the phone app; permissions, secrets and privacy."
section = "security"
order = 10
keywords = "security threat model permissions secrets authentication certificate tailscale protected settings privacy gdpr bypass web access lan"
+++

claude-cluster drives agents that run commands with your rights, so its security is about **who can talk to the core** and **what a manipulated model can do**. The detailed findings of the code audit are in the [security audit](page:audit-report).

## Who can reach what {#model}

| Actor | Reaches | Trust |
|---|---|---|
| you, at the keyboard | everything | full |
| any program running as you | the control socket and your config / data folders | the same as your account |
| a device on your LAN | the https page; nothing else until the code **and** your approval | none, then approved |
| a device from outside (Tailscale, router) | the same, https only, approval always asked | same |
| the global session (a model) | the MCP tools, bounded by protected settings | low: it obeys text it reads |
| a sub-session | its own small MCP | depends on its permission mode |

The design assumes that the models can be **manipulated by text they read** (a web page, a file, a reply): the guards are meant to bound what that text can make them do.

## Local use {#local}

Everything goes through a unix socket in your runtime folder (`0700`, the socket `0600`): only your user (and root) reaches it. Any program running as you, a script or an agent in `bypassPermissions`, may drive claude-cluster through it: the same trust as your account. API keys live in the keyring (`secret-tool`) or in `credentials.json` (`0600`), never in `config.toml`, logs or transcripts; the agents of the sessions receive them on a pipe, not on their command line. The config and data folders are `0700`. Programs run with your rights (`commands.*`, editor, viewer, voice commands) are confirmed before they change.

## Web access {#web}

- **https by default** with an authority of your own computer (key `0600`), TLS 1.2 or newer; a plain request is redirected.
- **Two factors per device**: the 6-digit code (new at each opening; a wrong code is answered 1 s later, 5 wrong ones block the address 15 minutes and rotate the code) **and** your approval on the computer. A trusted device (its own 32-byte secret) comes back by itself.
- **Cookies** are `HttpOnly`, `SameSite=Strict` and `Secure`, and the session is **bound to the address** of the device.
- **Scope**: only private addresses of a subnet of your computer, the tailnet range, or (with `remote = router`) any address over https; the `Host` is checked (no DNS rebinding) and a `POST` needs the right `Origin`.
- **Found by the app** (`[web] discovery`, on by default): while the access is open, the core answers a broadcast on UDP 8789 from a private address of a subnet of the computer only, once per address every 0.5 s, with its name, its port, https, the fingerprint of its authority and its version - never the code; the app connects only to the authority it pinned. Turn it off to stay silent on the network.
- **The page**: a per-response CSP nonce, `X-Frame-Options: DENY`, `nosniff`, HSTS; everything shown is escaped; attachments are sandboxed. 8 connections per address and 64 at once at most.
- **What a device can do**: everything the keyboard can, except a shell (`!command`) and the settings you did not open to it (`[web] settings`). A trusted phone lost or unlocked is your claude-cluster: *Web devices > trust no more / ban*, and keep the shell off unless you need it.
- **From outside**: `tailscale` opens nothing to the Internet; `router` makes the login page reachable by anyone, so the code, your approval and the bans are what stand. The code, the conversation and the cookies are encrypted end to end and the key never leaves your computer.

> [!WARN]
> The shell switch (`[web] shell`) does not by itself stop an approved device from asking an agent to run a command: treat every approved device as a person with your keyboard. See the audit, findings F7 and F8.

## The global session and its tools {#global}

The global session runs in `bypassPermissions` by default, so it obeys what reaches it with your rights: that is the point of an orchestrator and its risk, because a prompt injected in a sub-session returns to it as a reply. Its MCP tools run without a prompt, so what they may do is bounded:

- the **protected settings** are never changed by it (`settings_set` and `config_write` refuse them: it asks you): the web opened wider, the permissions of the agents, the programs run as you, the MCP servers and tools, the limits and the model policy, the plugin sources, and the switches `rewrite_*`;
- it writes only `.md` / `.txt` files outside hidden folders (`export`), never gives a hidden file (`~/.ssh`, `~/.config`, claude-cluster's folders) to a session as a context, and rewrites the system prompts only when you allowed it;
- **the voice keys and the palette actions** (log in / out, quit everything, start over) are the user's: never run by its tools nor by the web page.

> [!NOTE]
> These limits bound the **tools** of the global session, not the session process itself, which has its own shell and file tools in `bypassPermissions`.

> [!WARN]
> Found by the audit in this version: the switches `global_answers`, `global_fork` and `global_edit` are meant to be protected but a naming error leaves them changeable by the global session (F2), the hidden-file rule misses the `path` of a displayed file (F3), and a sub-session can run a command through `heavy_run` without a prompt (F1). Fixes are recommended in the [audit](page:audit-report).

## Secrets {#secrets}

API keys: keyring or `0600` file, given to agents on a pipe. The token of Claude Code is read only to ask the usage endpoint of claude.ai, passed by a header file readable by you alone, removed at once, never logged. The Android app pins the authority of your computer the first time (its fingerprint shown), its updates come from the computer and keep its signing key. `/ca.crt`, `/app.apk`, `/api/version` and `/docs` are served to anyone who reaches the page: nothing secret is in them.

## Privacy and the GDPR {#privacy}

Everything stays on your computer: conversations, the prompt history (in clear, in your data folder), attachments, keys, the map of your computer. There is no server run by the project, no telemetry and no account. The **GDPR does not apply to claude-cluster itself** since no personal data is collected or processed by the project: data stays local. It still applies to what you choose to send to a provider: the map of your computer (`machine.md`), the prompts and the files a session sends are read by the API provider you connected.

## Reporting a problem {#report}

Open an issue on [the repository](https://github.com/TsukiNi22/ai-utils) with the version and commit shown at the bottom of this page. For a vulnerability, do not publish the details before the author had time to fix it.
