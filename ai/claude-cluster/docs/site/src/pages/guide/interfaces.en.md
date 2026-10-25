+++
title = "The interfaces"
summary = "Terminal, window, web page and phone: the same sessions everywhere, in sync."
section = "guide"
order = 30
keywords = "supervisor superviseur instances update terminal tty window gui qt web page phone android app headless serve shared core hub grid board"
+++

Every interface is a view of one **core** (`claude-cluster serve --linger`, started by the first terminal or window). Several terminals, windows, web pages and phones can be open at once, in sync: the same sessions, prompts, questions and notices (`[ui] shared_core`, on). Closing one leaves the others; the last one closed stops the core, unless the [background service](page:settings#service) is the core.

## Terminal {#terminal}

`claude-cluster` in a terminal (or `--tty`). Built with FTXUI: a list of sessions, the conversation, panels, popups, the mouse (click, drag to select, wheel) and the keys of [the keys page](page:keys).

- Layouts (`F2`, `--layout`): `list` (list plus active panel) and `grid` (every session in a cell; arrows move between cells on an empty prompt, `Shift+Enter` / `Alt+O` opens one). See [Layouts](page:layouts).
- `Alt+G` opens the [graph view](page:taxonomy#graph): the global session in the middle, the sessions around it.
- The terminal stays in the foreground: `Ctrl+Z` is undo in the prompt, never a suspend.
- Images in the transcript need `chafa` or ImageMagick (`magick`).

## Window {#window}

`claude-cluster --gui` (built when Qt6 Widgets is found): the same features in a native window with tabs, a header per session, dialogs for settings and a hub graph. `claude-cluster -g --tty-mode` shows the terminal interface inside the window (`[ui] gui_mode = tty` keeps it). The service opens it when you say the wake phrase.

## Web page {#web}

Open the web access (settings *Web*, the palette *Web access: open*, `claude-cluster web on` or `--web`) and a browser on the same network drives claude-cluster: sessions, transcripts, prompts, Stop, permissions and questions, new sessions, settings.

- **Views** (the same five in the terminal, the window, the page and the app; every start on the list): *List*, *Grid* (a tile per session, the Tasks as the last one), *Graph* (global, then the categories / sub-globals of the [taxonomy](page:taxonomy), then the sessions), *Hub* (every session around global) and *Tree* (sessions by folder).
- **Getting in**: https with a certificate of your own authority (trusted once per device from the login page, no browser warning afterwards), a **6-digit code** that changes at each opening, then **your approval** on the computer (*Allow*, *Allow and trust*, *Deny*). Trusted devices come back by themselves.
- **Terminal mode**: the ⌨ button runs a real `claude-cluster --tty` for that device in the browser.
- **Beyond the LAN**: `[web] remote = tailscale` (recommended) or `router`. See [Security](page:security).
- **Docs**: this documentation is served by the core at `/docs` and opens in a panel with `Ctrl+K` then *Docs*. Without the web access, `claude-cluster docs serve [port] [lan]` serves it alone, read only (port 8790, this computer only unless `lan`).

By default only private addresses of the subnets of your computer are served, and from the page you cannot open a shell or change the web settings unless you allowed it (see [Security](page:security)).

## Phone {#phone}

Any phone browser works. On Android the page offers an **app** (`/app.apk`): the same page in a thin shell that trusts your computer once, shows the notifications of the desktop, and can use the microphone. See [Notifications and the phone app](page:notifications-apk). iPhone: the browser page, with the certificate steps shown on the login page.

## Simple mode {#simple}

`claude-cluster --simple` (or `--ipad-kid`) opens a calmer interface in every front end, in English or French (`[ui] language`); `[ui] mode = "simple"` keeps it. The home shows what needs you first (questions and permissions answered there), then one card per helper (session) with what it does in plain words, its state and its time; a card opens its conversation with the same prompt as usual (`/`, `::`, `@`). A *New task* tile asks the assistant, which decides how to split the work. Its settings have six pages: *General* (language, look, notifications, back to the classic interface), *Account & provider*, *Assistant* (how it splits the work, how smart, what to ask first, helpers at once), *Phone* (the web access and its QR code), *Shortcuts*, *About* (version, docs, all settings). `F10` opens the statistics.

## Supervisor {#supervisor}

`claude-cluster --supervisor` (or `s` in the start menu) shows every claude-cluster of this computer at a glance: the main one and each independent instance, running or stopped with sessions kept. Each one shows its version, its sessions counted by state (working, idle, waiting for you, background, queued, errors), its web access and its update; `F2` adds the details (every session with its state, queued prompts, agents, ♨, model).

- `Enter` opens one; `u` / `U` its smooth update / every one behind; `f` / `F` force the switch (asked first: turns and background stopped, then continued); `s` / `S` stop (asked first) / start; `w` its web access; `q` quits.
- It runs a newer binary again by itself as soon as it is installed.
- **Its web access** is a core of its own on `[web] supervisor_port` (8788): the page and the app connected to it show the supervisor (cards, Details, Update / Force / Stop / Start, Update all / Force all) instead of sessions; its actions are accepted from that access only.
- The window (`--supervisor -g`) shows the same list as a tree with its buttons.

## Headless {#headless}

`claude-cluster serve` runs the core without any interface. Everything is then driven by the headless commands (`list`, `send`, `spawn`, `allow`, `task`, `schedule`, `web`... with `--rtk` for a compact output made for an AI, `--json` raw) and by the MCP server. See the [protocol](page:protocol).
