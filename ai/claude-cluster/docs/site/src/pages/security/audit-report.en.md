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

Not covered either: dynamic tests (fuzzing, penetration test), the agents themselves, the Android code beyond its trust model, and libutils.
