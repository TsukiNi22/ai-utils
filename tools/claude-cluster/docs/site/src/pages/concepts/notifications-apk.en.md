+++
title = "Notifications and the phone app"
summary = "Desktop notifications, the face of the icon, and the Android app that follows your sessions from your phone."
section = "concepts"
order = 120
keywords = "notifications icon face android app apk phone service poll notify_app channels battery permission POST_NOTIFICATIONS"
+++

## Desktop notifications {#desktop}

The settings page *Notifications* turns all of them on or off, chooses *when* (`unfocused`: only when claude-cluster is not in front, `always`), each kind (the global session answered, a session finished, a permission, a question, an error, the limits, the web devices, the task queue, reminders, something slow ready) and the sound. A notification clicked (its body or *Open*) shows its session. Never twice: whoever sent the last prompt of a session is notified for it, here or on the web device that sent it. One notification per session, urgent when it waits on you, with buttons answered from the notification (a question's options, *Allow* / *Deny*).

## The face of the icon {#icon}

The icon is a stack of terminals whose front one has a face made of prompt characters; the face follows what claude-cluster does, in the title of the terminal, the window icon, the tab of the web page, the dock (the number of sessions waiting on you, else running) and `claude-cluster icon [face] [badge]` (its SVG, for a status bar).

| Face | The app now |
|---|---|
| `^_^` happy | the global session just answered |
| `>‿<` grin | a session just finished |
| `•_•` attentive | a question or a permission waits, the voice listens |
| `o_o` waiting | an error, the safety check of Claude Code |
| `-_-` idle | nothing running |
| `>_<` squint | something running |

## The Android app {#app}

When the web access is open, the page offers an app at `/app.apk`. It is built by `android/build.sh` (the Android SDK alone, no Gradle) and signed by a key of your own, `~/.config/claude-cluster/android/release.jks`.

- It is the web page in a thin shell: the **computer is trusted once** (its authority pinned after its fingerprint is shown and compared, so no warning), the microphone and the notifications work, it is edge to edge on Android 15.
- Tabs: *Chat* (the last chat opened, the global session first), *Sessions*, *This device* (version, build, this phone; *Check for updates*: installed from your computer, nothing to download elsewhere; per-phone notification kinds).
- An app **older** than the one the computer serves is blocked by a popup until updated (*Install*); a newer one on the computer is offered, never an older one; an app too old for this claude-cluster says so.

## Phone notifications {#phone}

The app shows **every notification of the desktop**. The core logs each one (the last 100, kinds and `[ui] notify_*` as on the desktop) and the app follows that log by a long poll, `GET /api/notify?since=N` (answered at once on a new one, else after 25 s). `[ui] notify_app = false` publishes none. Each phone chooses for itself in *This device*: on / off and which kinds, *Send a test*, *Allow notifications* (Android 13+, `POST_NOTIFICATIONS`) and *Keep running in the background*.

They show while the app is **not in front**, in Android channels per family (*Waiting for you*, *Sessions*, *Errors and limits*, *Reminders and messages*: high; *Information*: low), grouped per session, a tap opens that session. The logo is the status bar icon (a flat white mask generated at every density) and the large icon.

There are **no Google services**, so no push: a foreground service keeps its own connection to the computer (a quiet *following your sessions* notification, started when the app leaves the front and after a boot or an update, reconnecting with a growing delay). It cannot reach a computer that is off or out of the network, cannot survive an Android that kills the service despite the battery exemption (some vendors do: allow autostart), and cannot answer a permission from the notification (it opens the session). Away from home use the web access *Beyond the LAN* with Tailscale ([Security](page:security)).
