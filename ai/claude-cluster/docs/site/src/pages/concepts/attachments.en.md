+++
title = "Attachments"
summary = "Send files, images and photos with a prompt: where they go, what is checked, and the settings."
section = "concepts"
order = 80
keywords = "attachments files images photos upload paperclip drag drop paste exif link global_max camera"
+++

## Attaching {#attach}

Files, images and photos go with a prompt like in a chat: the **paperclip** next to the prompt (window, web page, app), **drag and drop** onto the prompt, **paste** (`Ctrl+V` of a picture or files). The terminal keeps `@path` and the image paste. On a phone the picker offers *Take a photo / video* (the camera, asked once with its reason). A chip per attachment shows above the prompt with a **✕** to remove it before sending.

An image reaches the agent as the picture itself (`[Image #N]`); any other file as `@path` (text, code, PDF...). The transcript shows them with a thumbnail and a link to open or download.

## What is checked {#checks}

- **Upload**: the file is sent to the core and stored in `<data>/attachments/<session>/<id>-<name>` with mode `0600`, never executable. The name is made safe, the type is **sniffed from the content** (a program, ELF or PE, is refused), images are re-encoded **without EXIF** and downscaled to `max_pixels` (needs ImageMagick).
- Only an authenticated device uploads; a session reads only its own folder, which goes with the session when it is deleted for good.
- **Link instead of copy**: a file of **this** computer attached by its path becomes a symlink, nothing is copied and the original is never touched. A phone or another computer always uploads. `[attachments] link = false` turns it off.

## The global session {#global}

It sees attachments in `session_messages` (path, type, size) and may attach local files with `session_send` (`attachments: [paths]`), only from the project folder of the session and your home (no hidden folder, nothing of claude-cluster itself), at most `global_max` files. `[attachments] global = false` forbids it.

## Settings {#settings}

`[attachments]` (settings page *Attachments*): `enabled`, `max_mb` (20), `max_files` (10), `keep` (200 per session, the oldest go), `max_pixels` (2048), `strip_exif`, `dir`, `link`, `global`, `global_max` (5). In the Android app the system picker gives access to the picked files only, so no media permission is requested.
