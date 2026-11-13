+++
title = "Architecture"
summary = "One core, many interfaces: the processes, the sessions, the MCP server and how the parts talk."
section = "reference"
order = 10
keywords = "architecture core manager control socket interfaces ftxui qt web mcp agents processes drivers orchestrator voice source layout"
+++

## Overview {#overview}

claude-cluster is one C++20 program. A single **core** per user owns the sessions; every terminal, window, web page and phone is an **interface** of that core.

:::html
<figure><svg viewBox="0 0 760 330" role="img" aria-label="Interfaces talk to the core, which runs the agents; the MCP server and cluster-run call back into the core">
<defs><marker id="arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0L10 5L0 10z" class="arrow"/></marker></defs>
<g id="i1"><rect class="box" x="20" y="14" width="150" height="40" rx="8"/><text class="small" x="95" y="39" text-anchor="middle">terminal (FTXUI)</text></g>
<g id="i2"><rect class="box" x="195" y="14" width="150" height="40" rx="8"/><text class="small" x="270" y="39" text-anchor="middle">window (Qt6)</text></g>
<g id="i3"><rect class="box" x="370" y="14" width="190" height="40" rx="8"/><text class="small" x="465" y="39" text-anchor="middle">web page, phone app</text></g>
<g id="i4"><rect class="box" x="585" y="14" width="155" height="40" rx="8"/><text class="small" x="662" y="39" text-anchor="middle">CLI (headless)</text></g>
<g id="core"><rect class="box-alt" x="120" y="120" width="520" height="86" rx="10"/><text class="lbl" x="380" y="150" text-anchor="middle">core (claude-cluster serve)</text><text class="small" x="380" y="176" text-anchor="middle">Manager · Control socket · Web server · Orchestrator · Voice</text></g>
<g id="a1"><rect class="box" x="20" y="268" width="170" height="40" rx="8"/><text class="small" x="105" y="293" text-anchor="middle">global session (claude)</text></g>
<g id="a2"><rect class="box" x="215" y="268" width="190" height="40" rx="8"/><text class="small" x="310" y="293" text-anchor="middle">sub-sessions (agents)</text></g>
<g id="a3"><rect class="box" x="430" y="268" width="150" height="40" rx="8"/><text class="small" x="505" y="293" text-anchor="middle">cluster-run</text></g>
<g id="a4"><rect class="box" x="605" y="268" width="135" height="40" rx="8"/><text class="small" x="672" y="293" text-anchor="middle">MCP server</text></g>
<path class="edge-accent" data-from="i1" data-to="core" marker-end="url(#arr)"/><path class="edge-accent" data-from="i2" data-to="core" marker-end="url(#arr)"/><path class="edge-accent" data-from="i3" data-to="core" marker-end="url(#arr)"/><path class="edge-accent" data-from="i4" data-to="core" marker-end="url(#arr)"/>
<path class="edge" data-from="core" data-to="a1" marker-end="url(#arr)"/><path class="edge" data-from="core" data-to="a2" marker-end="url(#arr)"/><path class="edge" data-from="a3" data-to="core" marker-end="url(#arr)"/><path class="edge" data-from="a4" data-to="core" marker-end="url(#arr)"/>
</svg><figcaption>Figure 1: interfaces and agents around the core.</figcaption></figure>
:::

## The core {#core}

The core is started by the first terminal or window (`claude-cluster serve --linger`) or by the systemd service; it stops when the last interface is closed (unless a session is still working and you chose to keep it). It holds:

- **Manager**: the sessions, the trash, tasks, schedules, groups, the taxonomy, usage, statistics.
- **Control**: a unix socket served in a thread per connection; the CLI, the MCP server, the other interfaces and `cluster-run` talk to it (see [the protocol](page:protocol)).
- **Web**: the https server of the web access, the terminal page and the docs.
- **Orchestrator**: the queue of heavy jobs, with its scheduler thread ([Orchestrator](page:orchestrator)).
- **Voice**: `voice-listen` and `voice-say` processes.

Interfaces of other processes sync their state from the core and their actions run there. A build id (its version and the date of its binary) is checked at attach: another build is refused until replaced.

## Sessions and agents {#agents}

Every session is a child process of the core, run through `env -C <folder> VAR=...` with the provider environment; every pipe end is close-on-exec so a session never keeps another one's pipes. The agents run in their own session (`setsid`): the signals of your terminal never reach them.

```text
core
├── global session  claude -p --input-format stream-json ... --mcp-config mcp.json
├── session s1      claude -p stream-json  (permissions: --permission-prompt-tool stdio)
├── session s2      qwen -p ... -o stream-json -r <id>       (one process per turn)
└── session s3      opencode run --format json -s <id>        (one process per turn)
```

The **claude driver** keeps one process per session with live permissions and an MCP server; the others (Qwen Code, opencode, Codex) start one process per turn and resume the conversation by id.

## The MCP server {#mcp}

`claude-cluster mcp` is a small stdio MCP server started by the global session. It turns each tool call into a request on the control socket, with `origin: "mcp"` (the limits you set for the global session), or `origin: "sub"` plus the session id for a sub-session's own small MCP. Results are compact JSON without empty fields.

## Source layout {#sources}

| Part | Files |
|---|---|
| core | `Core.cpp`, `Manager*.cpp`, `Session.cpp`, `Store.cpp`, `Config.cpp`, `Auth.cpp` |
| control and tools | `Control.cpp`, `Mcp.cpp`, `Actions.cpp`, `Settings.cpp`, `Tools.cpp` |
| web | `Web.cpp`, `WebTty.cpp`, `WebVoice.cpp`, `templates/web.html`, `templates/tty.html` |
| front-ends | `ui/Tty.cpp`, `ui/TtyGraph.cpp`, `ui/Gui.cpp`, `ui/LineEditor.cpp`, `ui/Popup.cpp` |
| orchestrator | `Orchestrator*.cpp`, `Hardware.cpp`, `RunProfile.cpp`, `run/cluster-run.cpp` |
| voice | `Voice.cpp`, `VoiceEngines.cpp`, `Vocabulary.cpp`, `templates/wake.py` |
| tasks | `Manager-Tasks.cpp`, `Manager-Schedules.cpp`, `Cron.cpp` |
| taxonomy, usage, stats | `Taxonomy.cpp`, `Manager-Usage.cpp`, `Usage.cpp`, `Stats.cpp` |

Dependencies: [libutils](https://github.com/TsukiNi22/libutils), nlohmann/json, toml++, FTXUI, Qt6 Widgets (optional) and OpenSSL (optional). The default prompts and the config template are embedded in the binary from `templates/` at build time.
