+++
title = "Local protocol"
summary = "The control socket (one JSON per line), the cluster-run wrapper messages, the web API endpoints and the MCP bridge."
section = "reference"
order = 50
keywords = "protocol control socket json unix cmd hello sync origin cluster-run wrapper web api endpoints notify mcp"
+++

## Control socket {#control}

The core listens on a unix socket, `$XDG_RUNTIME_DIR/claude-cluster/control.sock` (`CLAUDE_CLUSTER_SOCKET` overrides it; the folder is `0700`, the socket `0600`: only your user reaches it). A connection carries any number of requests; each is **one JSON object on one line** and gets **one JSON line** back:

```json
{"cmd": "list"}
{"ok": true, "data": [ ... ]}
{"ok": false, "error": "no such session: s9"}
```

| Field | Meaning |
|---|---|
| `cmd` | the command: `list`, `status`, `send`, `spawn`, `allow`, `mode`, `model`, `task_add`, `schedule_add`, `web`, `usage`, `taxonomy`, `settings_get`, `settings_set`, `heavy_run`, `job_*`... (the headless commands of the CLI, plus the ones of the interfaces and the MCP tools) |
| `session` | id (`s1`, `g`), name or name prefix |
| `origin` | set by the transport: `mcp` (the global session: its limits and its `[mcp_tools]` switches apply), `sub` (the small MCP of a sub-session, with `owner`: its own tasks and schedules only), `web` (a web device: read only for what the settings do not open) |
| `client`, `v`, `build` | an interface of another process: its id, the version of the state it has, its build id |

Special commands: `hello` returns the version, build id, pid and binary of the core (an interface asks before attaching); `sync` is a long poll: it returns the whole state of the interface once its version `v` changes (up to 10 s), and is refused when the build id differs.

A request never stops the core: anything it throws is answered as `{"ok": false, "error": "bad request: ..."}`.

## cluster-run {#run}

`cluster-run [--class c] [--prio n] [--name x] [--cwd d] [--session id] -- cmd...` talks to the same socket with a stream:

| Direction | Message |
|---|---|
| wrapper to core | `{"cmd":"job_run","session","class","prio","name","cwd","argv","wrapper_pid"}` |
| core to wrapper | `{"ok":true,"event":"queued","id","position","total","waiting_for","eta_s"}` (again when it changes) |
| core to wrapper | `{"ok":true,"event":"go","id","threads","profile","nice","io_class","io_level"}` |
| wrapper to core | `{"event":"started","pid"}`, later `{"event":"exit","code","signal","tail"}` |
| core to wrapper | `{"event":"killed","by","reason"}` |

The slot is released at `exit` or when the connection drops (the wrapper died: the command's process group is terminated). Without a core the wrapper runs the command directly; when the connection drops while queued it retries for 30 s, then runs directly.

## Web API {#web}

The web access serves a single page and a small JSON API over https, only after login (the 6-digit code, then your approval); the session cookie is bound to the address of the device. The endpoints:

| Path | Use |
|---|---|
| `/`, `/index.html`, `/sw.js` | the page and its service worker |
| `/api/login`, `/api/logout`, `/api/ping`, `/api/version` | authentication, round trip, version (version is public) |
| `/api/state` | the state of the cluster (long poll) |
| `/api/cmd` | a control command (`origin: "web"`) |
| `/api/complete` | completions of the prompt |
| `/api/attach`, `/api/attach/file`, `/api/attach/link`, `/api/attach/list`, `/api/attach/remove` | attachments |
| `/api/display`, `/api/file` | a display picture; a file of a display |
| `/api/notify?since=N` | the notification log (long poll, the Android app) |
| `/api/listen`, `/api/say` | the voice of the page |
| `/api/tty/*` (`open`, `read`, `write`, `resize`, `close`, `ws`) | the terminal mode, on its own port (`[web] tty_port`) |
| `/ca.crt`, `/app.apk` | served to anyone: the authority certificate and the Android app |
| `/docs/*` | this documentation, served without login |

## The MCP bridge {#mcp}

`claude-cluster mcp` speaks the Model Context Protocol on stdio. `tools/list` returns the [tools](page:mcp-tools); `tools/call` maps the tool name to a control command (`session_display` to `display`, `jobs_add` to `job_add`, ...) and sets `origin`. The environment variable `CLAUDE_CLUSTER_SESSION` makes it the MCP of a sub-session. A tool turned off in `[mcp_tools]` is refused at once with a message for the user.
