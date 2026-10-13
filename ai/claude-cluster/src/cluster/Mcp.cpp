/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Mcp.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Control.hpp"
#include "cluster/Mcp.hpp"
#include <iostream>
#include <string>

/* tools */
_cold static cluster::Json tool_(const std::string& name, const std::string& description, const cluster::Json& properties, const std::vector<std::string>& required)
{
    return {{"name", name}, {"description", description},
        {"inputSchema", {{"type", "object"}, {"properties", properties.is_null() ? cluster::Json::object() : properties}, {"required", required}}}};
}

_cold static cluster::Json str_(const std::string& description)
{
    return {{"type", "string"}, {"description", description}};
}

/* mcp */
_cold cluster::Json cluster::mcp_tools(void)
{
    const cluster::Json session = str_("id (s1, g) or name of the session");
    return cluster::Json::array({
        tool_("session_list", "Every session of the cluster: id, name, project folder, backend, state, permission mode, tokens, context, cost, pending permissions.", nullptr, {}),
        tool_("session_status", "Details of one session and its last 20 messages (skills loaded, files modified).", {{"session", session}}, {"session"}),
        tool_("session_read", "Same as session_status: the last messages of a session.", {{"session", session}}, {"session"}),
        tool_("session_spawn", "Start a new session in a project folder. backend: <provider>[/<model>] (claude, ollama/<model>, openai/<model>, opencode/<provider>/<model>, codex...).",
            {{"cwd", str_("project folder (~ allowed)")}, {"name", str_("name of the session (default: the folder name)")}, {"backend", str_("provider[/model], default from the config")},
             {"mode", str_("auto | manual | acceptEdits | plan | dontAsk | bypassPermissions")}, {"profile", str_("profile of the config")}, {"prompt", str_("first prompt")},
             {"effort", str_("thinking effort: default | low | medium | high | xhigh | max")}}, {"cwd"}),
        tool_("session_send", "Send a prompt to a session (queued when it is working). By default (reply: true) its answer comes back to you as a message "
            "\"[reply from <name> (<id>)] ...\" when it is done: go on with something else or end your turn, never poll it with session_read / session_status. "
            "A local command of claude-cluster (/clear, /model <m>, /mode <m>, /rename, /cd, /close, /cost, /context, /export...) runs at once, as typed "
            "in its prompt (no reply): /clear starts a new conversation for the session.",
            {{"session", session}, {"text", str_("the prompt")}, {"reply", {{"type", "boolean"}, {"description", "its answer sent back to you when done (default true)"}}}},
            {"session", "text"}),
        tool_("session_wait", "Wait until a session is done (up to timeout seconds, 110 max) and get only its last answer (not its context): when you need "
            "the answer now to go on. A permission waiting is told.", {{"session", session}, {"timeout", {{"type", "integer"}, {"description", "seconds (default 100)"}}}},
            {"session"}),
        tool_("session_allow", "Answer a pending permission of a session; a question of its agent (AskUserQuestion, its options in "
            "session_status): answers {question text: the label chosen, several joined by \", \"} (behavior deny: declined).",
            {{"session", session}, {"request_id", str_("request id (empty: the oldest one)")}, {"behavior", str_("allow | always | deny")},
             {"answers", {{"type", "object"}, {"description", "AskUserQuestion: {question text: answer}"}}}}, {"session"}),
        tool_("session_set_mode", "Permission mode of a session.", {{"session", session}, {"mode", str_("auto | manual | acceptEdits | plan | dontAsk | bypassPermissions")}}, {"session", "mode"}),
        tool_("session_set_model", "Model and / or thinking effort of a session (the same conversation goes on); none given: its current ones and the "
            "choices of its backend.", {{"session", session}, {"model", str_("a model of its provider (opus, sonnet, claude-opus-5-5, an Ollama model...)")},
            {"effort", str_("default | low | medium | high | xhigh | max (claude); minimal... (codex, opencode)")}}, {"session"}),
        tool_("usage", "Tokens and cost used per vendor (anthropic, ollama, openai...) over every session: today, the last 7 days, lifetime; "
            "the 5 h / 7 d limits left when the vendor tells them.", {{"vendor", str_("one vendor (empty: all)")}}, {}),
        tool_("cluster_info", "Everything about this claude-cluster in one call, instead of searching its files: version, binary, the paths "
            "of its files (config, logs, history, usage, prompts, vocabulary, voice words, map of this computer), the global session, the "
            "agents and their commands, the voice (speech model, models installed, what is missing), the service, the interface, counts.", {}, {}),
        tool_("config_files", "The editable files of claude-cluster: your own system prompt, the one of the sub-sessions, the vocabulary and the "
            "words of the voice, config.toml, the map of this computer (read only); their path, what they hold, when a change is taken.", {}, {}),
        tool_("config_read", "The content of one of them (config_files).", {{"name", str_("global_prompt | sessions_prompt | vocabulary | voice_words | config | machine_map")}}, {"name"}),
        tool_("config_write", "Replace one of them with this content: read it first (config_read) and keep what the user did not ask to change. "
            "config is checked (TOML) before it is written; your own system prompt: you are started again on it after this turn.",
            {{"name", str_("global_prompt | sessions_prompt | vocabulary | voice_words | config")}, {"content", str_("the whole new content")}}, {"name", "content"}),
        tool_("machine_sync", "Ask the user to map this computer again (when asked to, or the map is clearly outdated): its question is shown "
            "in the interface (yes / no), the scan then runs as started by hand.", {}, {}),
        tool_("session_interrupt", "Stop the running turn of a session.", {{"session", session}}, {"session"}),
        tool_("session_close", "Close a session: it goes to the trash, restorable with session_restore.", {{"session", session}}, {"session"}),
        tool_("session_restore", "Restore a closed session from the trash (with its conversation).", {{"session", str_("id or name in the trash")}}, {"session"}),
        tool_("session_trash", "Closed sessions that can still be restored.", nullptr, {}),
        tool_("session_purge", "Delete for good a session of the trash (its conversation and history), \"*\" for the whole trash. Irreversible: only when the user asked it.",
            {{"session", str_("id or name in the trash, or *")}}, {"session"}),
        tool_("session_rename", "Rename a session.", {{"session", session}, {"name", str_("new name")}}, {"session", "name"}),
        tool_("session_cd", "Move a session to another project folder (a new conversation there, same name).", {{"session", session}, {"cwd", str_("folder")}}, {"session", "cwd"}),
        tool_("task_add", "Queue a task: given to a free session (target = session id / name, a project folder, or empty for any idle session), after its dependencies.",
            {{"prompt", str_("the task")}, {"target", str_("session or folder (optional)")}, {"after", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "ids of tasks to finish first"}}}},
            {"prompt"}),
        tool_("task_list", "Tasks of the queue and their status.", nullptr, {}),
        tool_("schedule_add", "Do something at a time, while claude-cluster runs: a prompt to a session (you by default: a wake up / a reminder you get as "
            "\"[scheduled wN] text\") or a desktop notification; repeated every N minutes or once.",
            {{"when", str_("+30m, +2h, +1d, HH:MM (today, else tomorrow), YYYY-MM-DD HH:MM")}, {"text", str_("the prompt / the notification")},
             {"action", str_("prompt (default) | notify")}, {"session", str_("the session of the prompt (default: g, you)")},
             {"every", {{"type", "integer"}, {"description", "minutes between two (0 / missing: once)"}}}}, {"when", "text"}),
        tool_("schedule_list", "The things scheduled: id, time, repeat, action, session, text.", nullptr, {}),
        tool_("notify", "A desktop notification to the user now (something done, something needing them); off when the user turned them off.",
            {{"text", str_("the message")}, {"title", str_("its title (default: claude-cluster)")}, {"urgent", {{"type", "boolean"}, {"description", "stays on screen"}}}}, {"text"}),
        tool_("schedule_cancel", "Cancel a schedule.", {{"id", str_("schedule id (w1...)")}}, {"id"}),
        tool_("task_cancel", "Cancel a task.", {{"id", str_("task id (t1...)")}}, {"id"}),
        tool_("search", "Full-text search in the history of every session.", {{"text", str_("text to find")}}, {"text"}),
        tool_("export", "Export the conversation of a session as Markdown (to a file when path is given).", {{"session", session}, {"path", str_("file (optional)")}}, {"session"}),
        tool_("providers", "Providers / backends and whether they are ready (logged in, key stored, server up).", nullptr, {}),
        tool_("web_access", "The web access of claude-cluster on this LAN (a browser of the same network drives it after a 6-digit code, one at a "
            "time): its address, code and the browser connected; action on / off (this run), kick (the browser out, a new code), pin (a new code). "
            "Give the user the address and the code when they ask to open it.",
            {{"action", {{"type", "string"}, {"enum", {"", "on", "off", "kick", "pin"}}, {"description", "empty: the state only"}}}}, {}),
        tool_("external_conversations", "The conversations of the agents of this computer opened outside claude-cluster (Claude Code, Codex, opencode, "
            "Qwen Code): agent, id, title, folder, first prompt, age; the newest first. To open one here: session_import.",
            {{"agent", str_("claude | codex | opencode | qwen (empty: all)")}, {"limit", {{"type", "integer"}, {"description", "30 by default"}}}}, {}),
        tool_("session_import", "Open a conversation of another agent (external_conversations) as a sub-session resuming it: its transcript shown, the "
            "same conversation goes on here. fork (Claude Code): a copy, the original untouched - use it when it may still be open elsewhere.",
            {{"agent", str_("claude | codex | opencode | qwen")}, {"id", str_("its id (a unique prefix is enough)")}, {"name", str_("name of the session")},
             {"fork", {{"type", "boolean"}, {"description", "a copy (Claude Code)"}}}}, {"agent", "id"}),
        tool_("session_fork", "A copy of a sub-session of Claude Code: a new sub-session with its conversation so far (its context), each one goes on "
            "apart. Only when the user allowed it (settings: Global session can fork).", {{"session", session}, {"name", str_("name of the copy")}}, {"session"}),
        tool_("session_transcript", "The transcript of a session, whole entries (not cut like session_read): index, kind (user, assistant, tool, "
            "tool_result, system, display), text. from: the first index (negative: from the end, -50 by default), count (50, 500 max).",
            {{"session", session}, {"from", {{"type", "integer"}}}, {"count", {{"type", "integer"}}}}, {"session"}),
        tool_("session_context", "Give a context to a session as a prompt it keeps for its next requests (it answers \"ok\"): source = session:<id> "
            "(the conversation of another session), <agent>:<id> (a conversation of external_conversations), or a file path.",
            {{"session", session}, {"source", str_("session:<id> | claude:<id> | codex:<id> | qwen:<id> | a file")}}, {"session", "source"}),
        tool_("session_messages", "The real conversation of a sub-session of Claude Code (what its model sees): its messages with index, role, text, "
            "tools. To rewrite it: session_edit.", {{"session", session}}, {"session"}),
        tool_("session_edit", "Rewrite the conversation of a sub-session of Claude Code (indexes of session_messages): replace texts, delete messages "
            "(a tool call and its result go together), keep only the first N; written as a new conversation (the old one kept), the session "
            "started again on it, its transcript shown again. Only when it is idle and the user allowed it (settings: Global session can edit).",
            {{"session", session}, {"replace", {{"type", "array"}, {"description", "[{index, text}]"}}},
             {"delete", {{"type", "array"}, {"description", "[index, ...]"}}}, {"keep", {{"type", "integer"}, {"description", "keep the first N messages"}}}},
            {"session"}),
        tool_("voice_engines", "The engines of the voice: speech to text ([voice] listen_model) and text to speech ([voice] say_engine), "
            "local or free-tier APIs: what each one handles (languages), its strengths and limits against the others, its free use, its "
            "voices, its links, ready (installed / API key stored), used. To change one: settings_set; a key: the user runs "
            "claude-cluster auth login <provider>.", nullptr, {}),
        tool_("remote_control", "Remote Control of the global session (you): on, the user can go on with you from claude.ai / the Claude app (live both ways); off.",
            {{"on", {{"type", "boolean"}, {"description", "true: on, false: off"}}}}, {"on"}),
        tool_("ui_state", "What the interface of claude-cluster shows: the session shown, the layout, the style, Remote Control, vim; the choices.", nullptr, {}),
        tool_("ui_focus", "Show a session in the interface (a sub-session, or g: you again), while you go on talking.", {{"session", session}}, {"session"}),
        tool_("ui_layout", "Layout of the interface: list (the list + one session), grid (every session at once), tabs.",
            {{"layout", str_("list | grid | tabs")}}, {"layout"}),
        tool_("ui_style", "Colors of the interface (ui_state: the styles).", {{"style", str_("a style name")}}, {"style"}),
        tool_("ui_panel", "Show / hide a panel of a session: skills, tokens, context, cost, git, diff, tools, files.",
            {{"session", session}, {"panel", str_("skills | tokens | context | cost | git | diff | tools | files")}}, {"session", "panel"}),
        tool_("settings_get", "The settings of claude-cluster (the settings page): section, key, value, choices, help. section: one section only.",
            {{"section", str_("ui | agent | commands | global | sessions | voice | service (optional)")}}, {}),
        tool_("settings_set", "Change one setting of claude-cluster (written in its config.toml, applied live except the ones marked at_next_start).",
            {{"section", str_("section of the option")}, {"key", str_("its key (settings_get)")}, {"value", str_("the new value (true / false, a number, a choice, a text)")}},
            {"section", "key", "value"}),
    });
}

_cold cluster::Json cluster::mcp_call(const std::string& name, const cluster::Json& arguments)
{
    static const std::map<std::string, std::string> commands = {
        {"session_list", "list"}, {"session_status", "status"}, {"session_read", "read"}, {"session_spawn", "spawn"}, {"session_send", "send"}, {"session_wait", "wait"},
        {"session_allow", "allow"}, {"session_set_mode", "mode"}, {"session_set_model", "model"}, {"usage", "usage"}, {"cluster_info", "cluster_info"}, {"config_files", "config_files"}, {"config_read", "config_read"}, {"config_write", "config_write"}, {"machine_sync", "machine_sync"}, {"session_interrupt", "interrupt"}, {"session_close", "close"},
        {"session_restore", "restore"}, {"session_trash", "trash"}, {"session_purge", "purge"}, {"session_rename", "rename"}, {"session_cd", "cd"},
        {"task_add", "task_add"}, {"task_list", "task_list"}, {"schedule_add", "schedule_add"}, {"schedule_list", "schedule_list"}, {"schedule_cancel", "schedule_cancel"}, {"notify", "notify"}, {"task_cancel", "task_cancel"}, {"search", "search"}, {"export", "export"},
        {"providers", "providers"}, {"voice_engines", "voice_engines"}, {"web_access", "web"},
        {"external_conversations", "external"}, {"session_import", "import"}, {"session_fork", "fork"}, {"session_transcript", "transcript"},
        {"session_context", "context"}, {"session_messages", "messages"}, {"session_edit", "edit"}, {"remote_control", "remote"}, {"ui_state", "ui_state"}, {"ui_focus", "ui_focus"}, {"ui_layout", "ui_layout"},
        {"ui_style", "ui_style"}, {"ui_panel", "ui_panel"}, {"settings_get", "settings_get"}, {"settings_set", "settings_set"},
    };
    auto it = commands.find(name);
    if (it == commands.end()) return {{"content", {{{"type", "text"}, {"text", "unknown tool " + name}}}}, {"isError", true}};

    cluster::Json request = arguments.is_object() ? arguments : cluster::Json::object();
    request["cmd"] = it->second;
    request["origin"] = "mcp"; // the global session: the limits the user set for it
    if (name == "session_list") request["global"] = false; // the global session lists the others
    if (name == "session_send" && !request.contains("reply")) request["reply"] = true; // its answer comes back (no polling)
    if (name == "session_send") request["from"] = "global"; // marked for the sub-session: a compact answer, for a model
    cluster::Json response;
    try {
        response = cluster::Control::request(request);
    } catch (const utils::exception::IException& e) {
        return {{"content", {{{"type", "text"}, {"text", e.info()}}}}, {"isError", true}};
    }
    const bool ok = response.value("ok", false);
    const std::string text = ok ? (response["data"].is_null() ? "done" : response["data"].dump(1)) : response.value("error", std::string("error"));
    return {{"content", {{{"type", "text"}, {"text", text}}}}, {"isError", !ok}};
}

_cold int cluster::mcp_serve(void)
{
    std::string line;
    while (std::getline(std::cin, line)) {
        cluster::Json message;
        try {
            message = cluster::Json::parse(line);
        } catch (const cluster::Json::exception&) {
            continue;
        }
        if (!message.contains("id")) continue; // notifications (initialized, cancelled...)
        const std::string method = message.value("method", std::string());
        const cluster::Json params = message.value("params", cluster::Json::object());
        cluster::Json response = {{"jsonrpc", "2.0"}, {"id", message["id"]}};

        if (method == "initialize") {
            response["result"] = {
                {"protocolVersion", params.value("protocolVersion", std::string("2025-06-18"))},
                {"capabilities", {{"tools", cluster::Json::object()}}},
                {"serverInfo", {{"name", "claude-cluster"}, {"version", CLUSTER_VERSION}}},
                {"instructions", "Tools of claude-cluster: manage the other Claude Code / agent sessions of the user (list, spawn, send, permissions, modes, tasks), its interface (session shown, layout, style, panels), its settings and the Remote Control of the global session."},
            };
        } else if (method == "tools/list") {
            response["result"] = {{"tools", cluster::mcp_tools()}};
        } else if (method == "tools/call") {
            response["result"] = cluster::mcp_call(params.value("name", std::string()), params.value("arguments", cluster::Json::object()));
        } else if (method == "ping") {
            response["result"] = cluster::Json::object();
        } else {
            response["error"] = {{"code", -32601}, {"message", "method not found: " + method}};
        }
        std::cout << response.dump() << std::endl;
    }
    return 0;
}
