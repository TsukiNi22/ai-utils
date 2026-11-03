+++
title = "Security audit report"
summary = "A code-reading audit of the web access, the control socket, attachments, permission gates, prompt-rewriting protection and the orchestrator - with a downloadable PDF."
section = "security"
order = 20
keywords = "audit report pdf security findings web authentication control socket attachments permissions orchestrator gdpr mythos"
+++

> [!OK]
> **Download the report**: [claude-cluster-audit-report.pdf](downloads/claude-cluster-audit-report.pdf), 6 pages, written from reading the code of version 1.4.62 (commit `2139c37`).

## What it covers {#scope}

| Area | Question asked |
|---|---|
| authentication of the web access and devices | can someone get in, or keep a session, without the code and your approval? |
| the control socket | who can reach it, and what protects it? |
| file access of attachments | can a path escape the allowed folders? |
| the shell and MCP permission gates | what stops a model or a device from running a command? |
| protection of the prompts and settings | what can the global session rewrite? |
| the orchestrator | can a session use it to go around its permission mode? |

Method: each entry point was followed from where a request enters to the guard and the action, and each guard was compared with what the README and the settings help promise. Nothing was exploited on a running instance; there was no dynamic testing.

## Findings at a glance {#findings}

| Id | Level | Finding |
|---|---|---|
| F1 | **High** | a sub-session runs arbitrary shell commands through `heavy_run` with no permission prompt |
| F2 | Medium | `sessions.global_fork`, `global_edit`, `global_answers` are meant to be protected but are not (a naming mismatch) |
| F3 | Medium | the hidden-file guard of displays checks `files`, not `path` / `paths` |
| F4 | Medium | the `/tmp/claude-cluster-<uid>` fallback of the runtime folder is predictable and unchecked |
| F5 | Medium | the local certificate authority is valid 20 years with no name constraint |
| F6 | Medium | a describe / chat one-shot run puts an API key on a command line |
| F7 | Low | `[web] settings = all` amounts to code execution |
| F8 | Low | `web.shell = false` stops `!command`, not an agent asked to run it |
| F9 | Low | token compared with `==`; any device can rotate the code |
| F10 | Low | files up to 512 MB are read whole in memory |
| F11-F13 | Info | file creation order, scope of the protections, attachment id generator |

These are findings against the code of the commit read, not against a release; they are reported for fixing. The report details the evidence, the impact and a fix for each, then lists what was checked and holds.

## What holds {#holds}

The web login (a code compared in constant time, 5 wrong codes blocking an address, your approval, cookies bound to the device address), the Host and Origin checks, the headers and CSP nonce, the request limits, the permissions of the socket folder and of the secrets, the attachment path checks, the sniffing of uploads and the secret passing on a pipe for the sessions are all verified in the code. See [Security overview](page:security).

## Privacy and the GDPR {#gdpr}

The data of claude-cluster stays on your computer and the project has no server, so **the GDPR does not apply to claude-cluster itself**. It applies to what you send to the providers you connect.

## What is not in it {#next}

> [!SOON]
> A wider security audit of the repository, run with the Claude Code *mythos* audit, is coming soon and is not part of this report.

Not covered either: the agents themselves, the Android code beyond its trust model, and libutils. Dynamic tests were not part of this report: the second audit below ran them.

## The second audit, version 1.10.0 {#second}

The whole of claude-cluster read again in passes, each one with AddressSanitizer + UBSan and ThreadSanitizer on the test suite, fuzzing of the web access (`tests/web_fuzz.py`) and of the displays (`tests/display_shapes.py`), with every finding reproduced or followed to its exact code path, then fixed with its regression test.

| Id of the first audit | Now |
|---|---|
| F1 | gated by `[orchestrator] sub_add` |
| F2, F3, F4, F6 | **fixed** (the protected names, the hidden paths of a display, the runtime folder, the API key off the command line) |
| F9 | the token compared in constant time; any device can still change the code by wrong codes (the pairing only, nothing else) |
| F10 | **fixed**: the files of the page are streamed |
| F5 | **open**: the local authority is valid 20 years with no name constraint; changing it asks every device to trust a new one (a decision for you) |
| F7, F8 | documented in [Security overview](page:security) |

What the second audit found and fixed, the most serious first:

- **`[web] shell = false` could be walked around** by a device of the page: `:set web.shell true` in a prompt, a schedule or a task running a command, a tag character before `!`. The gate is now in one place, on every way of sending a prompt.
- **What ended the core**: JSON nested very deep (a line of 200 000 `[`: the stack gone), a request using what the quit had destroyed, an exception out of any thread, two pages opening the terminal at once, a save of the state concurrent with another one (`state.json` corrupted, the sessions lost at the next start).
- **What ended a terminal or took the memory of the computer**: a display of an unexpected shape (again at each start: it stays in the log), the numbers of a chart (`[1e300, -1e300, 1]`: a bar without end).
- **Keep warm**: a wake-up taking back its own prompt stopped the shells and agents running in the background, and could erase a real prompt given at the same moment.
- **A paste is never keys**: a letter of a text pasted could allow a device, quit, start an update; in the terminal of the page, an end of paste inside a copied text made the rest of it keys.
- `/api/listen` and `/api/attach` were mixed up since the voice of the page was added: a file uploaded was sent to the speech engine.

An independent reading of each subsystem came after those fixes and confirmed 99 more points, each one reproduced or followed to its code, all fixed:

- **The shell, off, was still reachable**: `Ctrl+E` on the terminal page opened an editor (`:!cmd`, `:w ~/.bashrc`); a `command` that is not a string in a schedule or a task ran through `/bin/sh`; a NUL byte in a path fooled the check of the hidden files, and a device could write over `~/.bashrc`.
- **A file of an agent, opened by a device, ran in the origin of the page**: a form in that page could send any command. It opens as text now, and a plain form can no longer reach the routes taking JSON.
- **Secrets**: a display given with a pin read `~/.ssh/id_rsa` past the check, the global session could attach it when the session sits in the home folder, and `voice.words` could point it at any file.
- **What ended the core or a terminal**: the settings rewritten under the threads reading them, a pattern of deep parentheses in `:s`, two `usage` requests at once, `max_cost: inf` written as a null that stopped every next start, `: ` typed in the prompt, a display or a question of an unexpected size or shape.
- **What lost work**: a keep-warm take-back cutting a real turn or a real prompt, the answer to the restore question replacing the conversation of the global session, a prompt sent during a replacement, a task sent to the session of another project after a restart, the start menu deleting an instance whose state could not be read.

- **What a big log did**: the output file of a background shell was read whole at every frame of its list, in the terminal, the window and the `agents` request (gigabytes of memory for a verbose build), and a fifo named as that file blocked the thread for ever. Only its last bytes are read now, from a regular file.

The full list, with the evidence of each one, is in the report `audit/2026-11-03-security.pdf` of the repository.
