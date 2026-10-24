+++
title = "Troubleshooting"
summary = "The usual problems and where to look: build, window, web access, certificates, voice, phone app, logs."
section = "guide"
order = 70
keywords = "troubleshooting problem error firewall port certificate warning qt window log debug reconnecting core claude missing voice apk update"
+++

## It does not build {#build}

- `find_package(utils)` fails: libutils 4.0.0 or newer is missing or too old; install it (see [Install](page:install)).
- FTXUI fails to download: the first configure fetches it (needs network); later builds reuse it.
- Too many compilers at once freeze the computer: use `nice -n 19` and `--parallel 2` at most.

## `--gui` says there is no window {#window}

The binary was built without Qt6 Widgets (or `-DCLUSTER_GUI=OFF`), or there is no display. Install the Qt6 development package and build again; the terminal interface always works.

## The agent is not found {#agent}

claude-cluster launches `claude` (or `qwen`, `opencode`, `codex`) by the names in `[commands]`. Check `claude --version` and `claude-cluster auth list`: it says which providers are ready and why not. A provider with a key needs `claude-cluster auth login <provider>`.

## The web page does not load {#web}

- Is the access open? The bottom right of the interface shows `● web https://<address>:8787 · code ...`; `claude-cluster web` tells the state and why the outside is not reached.
- The firewall may block the port (`[web] port`, 8787): `firewall-cmd --add-port=8787/tcp`.
- The app does not find the computer (*Search this network* empty): the web access must be open, `[web] discovery` on, the phone on the same Wi-Fi, and UDP 8789 let in (`firewall-cmd --add-port=8789/udp`); a guest Wi-Fi often blocks broadcasts: type the address.
- The browser must have a private address of a subnet of the computer; a public name pointed at the LAN is refused by design.
- *Your connection is not private*: the certificate belongs to the authority of this computer. Trust it once per device from the login page (*trust this computer once*), or on this computer with `claude-cluster web trust-ca`.
- The code is rejected: 5 wrong codes block the address for 15 minutes and rotate the code; the new code is on the computer.
- Nothing happens after the code: the device waits for **your approval** on the computer (a popup and a notification with *Allow*).

## The voice does not work {#voice}

Run the palette action *Voice: what is missing*. The microphone needs https in a browser. `claude-cluster engines` tells which engines are ready. See [Voice](page:voice).

## The phone app {#apk}

An app older than the one the computer serves is blocked by a popup until updated (*Install*: it downloads `/app.apk` from the computer). No notification? Allow notifications (Android 13+) and *Keep running in the background* in *This device*; some vendors kill background services: allow the app to autostart. A computer that is off or out of the network cannot be reached; there is no Google push. See [Notifications and the phone app](page:notifications-apk).

## Reconnecting {#core}

A popup *Reconnecting* means the core was lost (killed, crashed, restarting): it retries 1, 2, 4... 10 s apart, starts the core again when it is gone and resumes the sessions. **Retry now** (`r` / Enter) or **Quit** (`q`). If another version runs the core (installed again while it ran) a new interface asks whether to replace it.

## Where the logs are {#logs}

| What | Where |
|---|---|
| core log | `~/.local/share/claude-cluster/core.log` |
| conversations | `~/.local/share/claude-cluster/logs/<id>.jsonl` |
| service | `claude-cluster service logs` (the journal of the user unit) |
| debug trace | `[debug] enabled = true`, then `debug.log` in the data folder |
| what failed | a popup *what you did, where it failed, the error* with **OK** / **Retry** |

A bug report can include the debug trace, the version and commit shown at the bottom of this site.
