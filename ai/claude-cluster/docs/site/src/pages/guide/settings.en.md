+++
title = "Settings"
summary = "The settings page, config.toml, dangerous values, protected settings, the Services tab and searching a setting."
section = "guide"
order = 60
keywords = "profiles profils sync shared partagés settings config.toml f9 dangerous protected services service systemd search profiles providers styles context window compact keep warm wake-up"
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

## Profiles {#profiles}

Every claude-cluster of the computer shares `config.toml` and reads it again when it changes: a setting changed anywhere is changed everywhere. **Settings > Profiles** keeps named copies of every setting: *Save the settings of now as…* (any name), then on each profile **Apply** (the settings before kept: *Undo the last apply*), **Save the settings of now into it**, **Rename…**, **Note…**, **Archive** / **Unarchive**, **Delete** (the trash for 7 days: *Restore* or *Delete for good*, then gone). The same cards in the terminal, the window, the page and the app.

## Accounts and limits {#accounts}

*Accounts & limits* lists every provider with its state (a key stored, a login done, or what is missing). `claude-cluster auth login <provider>` asks a key without echo and stores it in the keyring (`secret-tool`) or `credentials.json` (`0600`), never in `config.toml`. The **limits** cap what sessions may spend together per day, week and month (`[limits]`), counted for API keys only (`scope = keys`) or everything, with *Block at the limit* and *Warn at* a share.

## Plugins and skills {#plugins}

The **Plugins** page (`/plugins`, the palette *Plugins*, in every interface) lists the **sources** first - a repository, a folder, a `.zip` - each with what it offers, has installed and has on, its last sync and its last commit (or its date). A source opened shows its offer as a **checklist**: tick several (or all), then install, remove, turn on or off the ticked ones at once. Each source can be synced now, installed or removed whole, turned off (every item of it off, kept installed) and on again, have its **auto sync** turned off (pulled by hand only), or be dropped. *Sync all* pulls every source; `[plugins] sync_hours` (24) is how often the auto ones are pulled. The settings page keeps only the sources and that period. A source's code runs as you: add only sources you trust.

## Context and keep warm {#context}

- **Context at most** (`[global] context` for the global session, `[sessions] context` for the others; `auto` by default, `100k` to `1M`): the conversation is compacted once it reaches this size (Claude Code `CLAUDE_CODE_AUTO_COMPACT_WINDOW`, given at every start). Every turn costs less, and a wake-up after its cache expired reloads that much, not 1M tokens. The global session may give a sub-session its own when it starts it (`session_spawn` `context`); a value out of 100k..1M is refused.
- **Keep warm** (`[sessions] keep_warm`: `off`, `chosen`, `all`, `background`): an idle session woken up a little before its prompt cache expires (`keep_warm_ttl`, `keep_warm_margin`), counted from the **last request** of its agent, so a session taken back (Esc) stays warm. A wake-up never piles up: when the last prompt is still a wake-up, it is taken back first and the same prompt sent again on the same cached prefix (one in the conversation at most). Kept warm `keep_warm_hours` after the last real prompt or take-back.

## Debug and the showcase {#debug}

*Debug* has a **showcase** that shows for real everything claude-cluster can draw (display blocks, question and permission popups, session states, progress and failure popups, notifications) in a session of its own whose agent is never asked anything: nothing is spent.
