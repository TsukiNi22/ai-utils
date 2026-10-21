+++
title = "Settings"
summary = "The settings page, config.toml, dangerous values, protected settings, the Services tab and searching a setting."
section = "guide"
order = 60
keywords = "settings config.toml f9 dangerous protected services service systemd search profiles providers styles"
+++

## The settings page {#page}

`F9` (or the palette, *Settings*; the ⚙ of the web page) opens a page like a BIOS setup: one tab per section (Interface, Agent, Commands, Global session, Sessions, Voice, Keys, Web, Attachments, Notifications, Orchestrator, Services...), the help of the selected option on the right.

- Terminal: `↑↓` select, `←→` or `Enter` change a choice, toggle or number, `Enter` edits a text, `Tab` / `Shift+Tab` change section, `Ctrl+S` saves (the page stays), `F10` saves and leaves, `Esc` leaves (unsaved changes: *Save the changes?*). `*` marks the changed ones.
- Changes are written in `config.toml` (only the values, its comments kept) and reloaded live. Options marked *applied at the next start* need a restart. Every option is listed in the [settings reference](page:settings-reference), and its default in the [configuration reference](page:config-reference).
- Profiles (`[profiles.<name>]`), own providers (`[providers.<name>]`) and own styles (`[styles.<name>]`) stay in the file.

## Searching a setting {#search}

`/` or `Ctrl+F` in the page, or `setting:` in the palette. The match is typo tolerant and on the name far before the description, and tells where it matched. The documentation search at the top of every docs page covers the same settings, tools and keys.

## Dangerous values {#danger}

A yellow **⚠** marks each setting that can be dangerous and its name turns **red** while its value is the dangerous one: `bypassPermissions` for new sessions or the global session, `allow_bypass`, `bypass_to_global`, `extra_args` that skip permissions, the web access without approval, open to the Internet (`remote = router`), its shell, plain http, an added MCP server. Changing one asks nothing; the save lists them with their warnings in one popup. `:set` needs `:set!` for them and the control socket / MCP refuse without `confirmed`.

## Protected settings {#protected}

The global session can change most settings (`settings_set`) but never the **protected** ones: the web opened wider, the permissions of the agents, the programs run as you (`commands.*`, editors, viewers), the limits and the model policy, the plugin sources, the MCP servers. It asks you, and you change them here. The list is in [Security](page:security#global).

## What a setting needs {#needs}

What is missing for a setting is told in red on its right (`✗ ...`): a program not found (editor, viewer, agent), `upnpc` for `remote = router`, `tailscale` not connected. When a package installs it, `[i]` / *Install...* does it (the terminal asks `sudo`, the window and the web page use the password window of the desktop).

## The Services tab {#service}

One card per sub service, with its state and buttons:

- **Service (systemd)**: the wake phrase while claude-cluster is closed (`claude-cluster service setup | enable | disable | start | stop | restart | status | logs | remove`). The unit `~/.config/systemd/user/claude-cluster.service` is started with the graphical session; what it opens is *Interface > Front-end* (a window, else a terminal).
- **Web access**: state, address, code, whether the outside is reached; open / close, new code.
- **Core**: pid, version and build, who started it, its interfaces by kind, sessions working, how long it will stay in the background; restart, stop all.
- **Voice**, **MCP servers**, **Outside (router / tailscale)**.

## Accounts and limits {#accounts}

*Accounts & limits* lists every provider with its state (a key stored, a login done, or what is missing). `claude-cluster auth login <provider>` asks a key without echo and stores it in the keyring (`secret-tool`) or `credentials.json` (`0600`), never in `config.toml`. The **limits** cap what sessions may spend together per day, week and month (`[limits]`), counted for API keys only (`scope = keys`) or everything, with *Block at the limit* and *Warn at* a share.

## Debug and the showcase {#debug}

*Debug* has a **showcase** that shows for real everything claude-cluster can draw (display blocks, question and permission popups, session states, progress and failure popups, notifications) in a session of its own whose agent is never asked anything: nothing is spent.
