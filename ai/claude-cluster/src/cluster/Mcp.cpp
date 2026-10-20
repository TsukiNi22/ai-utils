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
#include "cluster/Display.hpp"
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
            {{"cwd", str_("project folder (~ allowed), or the name of a project of machine.md (its git repositories: the closest by its name)")}, {"name", str_("name of the session (default: the folder name)")}, {"backend", str_("provider[/model], default from the config")},
             {"mode", str_("auto | manual | acceptEdits | plan | dontAsk | bypassPermissions")}, {"profile", str_("profile of the config")}, {"prompt", str_("first prompt")}, {"reply", {{"type", "boolean"}, {"description", "the answer to the first prompt (and later wake-up turns) sent back to you as \"[reply from ...]\" (default true)"}}},
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
        tool_("models_policy", "The model / thinking policy of the user: the default model and effort, whether they are advised or forced, the allowed ones, "
            "the costly ones (the user is asked first), and the model of the global session. Read it before choosing the model of a sub-session.", {}, {}),
        tool_("session_set_model", "Model and / or thinking effort of a session (the same conversation goes on); none given: its current ones and the "
            "choices of its backend. Under the policy of the user (models_policy): a forbidden one is refused with the reason; a costly one (opus, xhigh...) "
            "returns {asked}: the user answers Allow / Deny in their interface and the result comes back to you as a message.", {{"session", session}, {"model", str_("a model of its provider (opus, sonnet, claude-opus-5-5, an Ollama model...)")},
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
            "config is checked (TOML) before it is written, its protected settings stay the user's; the system prompts only when the user allowed it "
            "(settings: Global session can rewrite its own system prompt / the system prompt of the sub-sessions, one each), else give them the text; your own system prompt: you are started again on it after this turn.",
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
        tool_("session_continue", "A session goes on where it stopped (interrupted, an error of the API or of the connection): \"continue\" sent in "
            "the same conversation; a turn waiting for a retry of the API: stopped and asked again at once.", {{"session", session}}, {"session"}),
        tool_("session_describe", "Give a session its short description (a few words: what it works on, holds; shown under its name, the trash too, to find it "
            "again). Given: kept; empty: claude-cluster writes it again.", {{"session", session}, {"text", str_("a few words, empty: written again")}}, {"session"}),
        tool_("session_cd", "Move a session to another project folder (a new conversation there, same name).", {{"session", session}, {"cwd", str_("folder")}}, {"session", "cwd"}),
        tool_("task_add", "Queue a task: given to a free session (target = session id / name, a project folder, or empty for any idle session), after its dependencies.",
            {{"prompt", str_("the task")}, {"target", str_("session or folder (optional)")}, {"after", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "ids of tasks to finish first"}}}},
            {"prompt"}),
        tool_("task_list", "Tasks of the queue and their status (a failed one: its reason in \"error\"; the finished ones are archived after 30 minutes: archived: true lists them too).",
            {{"archived", {{"type", "boolean"}, {"description", "also the archived tasks (default false)"}}}}, {}),
        tool_("session_fanout", "A request on several sessions / projects at once (a group): a task each, sent as many at once as the parallel sessions "
            "allow (the others wait in the task queue); you are woken ONCE with their answers as [group fN ...] to merge them - end your turn after it, "
            "never poll. tasks: [{session or cwd, prompt (else the common prompt), after: [indexes of tasks of this group it builds on: their answers "
            "given to it]}]; match: {under: folder, has: a file marking a project (default: a git repository), name: part of its name, max} for every "
            "project found (its session reused, else created: shown to the user first, then confirmed: true); wait: {mode: all | first | count, count, "
            "timeout_minutes (then woken with what came), stop_others (the wait met: the others stopped)}; retry (a failed task tried once more, "
            "default true); merge: here (you) | session (a big merge by a session of its own, its answer to you); table: a comparison table asked too.",
            {{"label", str_("what the group does, a few words")}, {"prompt", str_("the common prompt")},
                {"tasks", {{"type", "array"}, {"items", {{"type", "object"}}}, {"description", "[{session | cwd, prompt, after: [index]}]"}}},
                {"match", {{"type", "object"}, {"description", "{under, has, name, max}"}}},
                {"wait", {{"type", "object"}, {"description", "{mode: all | first | count, count, timeout_minutes, stop_others}"}}},
                {"retry", {{"type", "boolean"}}}, {"merge", str_("here | session")}, {"table", {{"type", "boolean"}}},
                {"confirmed", {{"type", "boolean"}, {"description", "the user agreed to the projects found (match)"}}}}, {"label"}),
        tool_("group_status", "The progress of a group (its tasks, their state and answers so far); none: every group of this run.", {{"group", str_("fN")}}, {}),
        tool_("group_cancel", "Stop a group: its tasks not started are cancelled, the running ones interrupted when interrupt is true.",
            {{"group", str_("fN")}, {"interrupt", {{"type", "boolean"}}}}, {"group"}),
        tool_("task_act", "Act on a task of the queue: cancel | delete | archive | unarchive | force (started now, past the limit of parallel sessions).",
            {{"id", str_("task id (task_list)")}, {"action", str_("cancel | delete | archive | unarchive | force")}, {"reason", str_("why (optional)")}}, {"id", "action"}),
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
        tool_("export", "Export the conversation of a session as Markdown (to a file when path is given: a .md / .txt out of the hidden folders).",
            {{"session", session}, {"path", str_("file (optional)")}}, {"session"}),
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
        tool_("session_display", "Show something in the transcript of a session (any sub-session, or g: yours) without a prompt: its agent "
            "never sees it, the user does (terminal, window, web page). display = {type, title?, ...}: " + cluster::display_help() + ". Or only "
            "text (markdown). For the user: a summary table, a chart of numbers, a diagram of an architecture, an image file, a drawing (SVG), "
            "a callout, a progress bar.", {{"session", session}, {"display", {{"type", "object"}, {"description", "{type, ...}"}}},
            {"text", str_("markdown (when no display)")}, {"title", str_("its title")}}, {"session"}),
        tool_("session_display_update", "Replace a display you put (its id, given back by session_display): the steps of a long work, a "
            "progress, a checklist, a table that changes. The user sees it change in place.",
            {{"session", session}, {"id", str_("its id")}, {"display", {{"type", "object"}, {"description", "{type, ...}"}}}}, {"session", "id", "display"}),
        tool_("session_display_get", "A display of a session as it is now: the items the user ticked (checklist), the buttons clicked "
            "(actions), the values sent (form, they also come to you as a message).", {{"session", session}, {"id", str_("its id")}}, {"session", "id"}),
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
            {{"section", str_("section of the option")}, {"key", str_("its key (settings_get)")}, {"value", str_("the new value (true / false, a number, a choice, a text)")},
                {"confirmed", str_("\"true\" only when the user explicitly asked for this value: a dangerous one is refused without it - ask the user first. "
                    "The protected ones (a shell or the web open wider, the permissions of the agents, the programs run, the MCP servers and tools) are "
                    "refused anyway: the user changes them in the settings page")}},
            {"section", "key", "value"}),
    });
}

_cold cluster::Json cluster::mcp_call(const std::string& name, const cluster::Json& arguments)
{
    static const std::map<std::string, std::string> commands = {
        {"session_list", "list"}, {"session_status", "status"}, {"session_read", "read"}, {"session_spawn", "spawn"}, {"session_send", "send"}, {"session_wait", "wait"},
        {"session_allow", "allow"}, {"session_set_mode", "mode"}, {"session_set_model", "model"}, {"models_policy", "models_policy"}, {"usage", "usage"}, {"cluster_info", "cluster_info"}, {"config_files", "config_files"}, {"config_read", "config_read"}, {"config_write", "config_write"}, {"machine_sync", "machine_sync"}, {"session_interrupt", "interrupt"}, {"session_close", "close"},
        {"session_restore", "restore"}, {"session_trash", "trash"}, {"session_purge", "purge"}, {"session_rename", "rename"}, {"session_describe", "describe"}, {"session_continue", "resume"}, {"session_fanout", "fanout"}, {"group_status", "group_status"}, {"group_cancel", "group_cancel"}, {"session_cd", "cd"},
        {"task_add", "task_add"}, {"task_list", "task_list"}, {"schedule_add", "schedule_add"}, {"schedule_list", "schedule_list"}, {"schedule_cancel", "schedule_cancel"}, {"notify", "notify"}, {"task_cancel", "task_cancel"}, {"task_act", "task_act"}, {"search", "search"}, {"export", "export"},
        {"providers", "providers"}, {"voice_engines", "voice_engines"}, {"web_access", "web"},
        {"session_display", "display"}, {"session_display_update", "display_update"}, {"session_display_get", "display_get"}, {"external_conversations", "external"}, {"session_import", "import"}, {"session_fork", "fork"}, {"session_transcript", "transcript"},
        {"session_context", "context"}, {"session_messages", "messages"}, {"session_edit", "edit"}, {"remote_control", "remote"}, {"ui_state", "ui_state"}, {"ui_focus", "ui_focus"}, {"ui_layout", "ui_layout"},
        {"ui_style", "ui_style"}, {"ui_panel", "ui_panel"}, {"settings_get", "settings_get"}, {"settings_set", "settings_set"},
    };
    auto it = commands.find(name);
    if (it == commands.end()) return {{"content", {{{"type", "text"}, {"text", "unknown tool " + name}}}}, {"isError", true}};

    cluster::Json request = arguments.is_object() ? arguments : cluster::Json::object();
    request["cmd"] = it->second;
    request["origin"] = "mcp"; // the global session: the limits the user set for it
    request["tool"] = name;    // off in the settings (MCP page): refused
    if (name == "session_list") request["global"] = false; // the global session lists the others
    if (name == "session_send" && !request.contains("text")) { // the model names it like the prompt of session_spawn: never sent empty
        for (const char* alias: {"prompt", "message", "content"})
            if (request.contains(alias)) {
                request["text"] = request[alias];
                break;
            }
    }
    if (name == "session_send" && !request.contains("reply")) request["reply"] = true; // its answer comes back (no polling)
    if (name == "session_send") request["from"] = "global"; // marked for the sub-session: a compact answer, for a model
    cluster::Json response;
    try {
        response = cluster::Control::request(request);
    } catch (const utils::exception::IException& e) {
        return {{"content", {{{"type", "text"}, {"text", e.info()}}}}, {"isError", true}};
    }
    const bool ok = response.value("ok", false);
    // read by a model, not a human: compact JSON, its empty fields left out (like rtk: every token says something)
    const std::function<void(cluster::Json&)> prune = [&prune](cluster::Json& value) {
        if (value.is_object()) {
            for (auto it = value.begin(); it != value.end();) {
                prune(*it);
                const bool empty = it->is_null() || (it->is_string() && it->get_ref<const std::string&>().empty()) || ((it->is_array() || it->is_object()) && it->empty());
                it = empty ? value.erase(it) : std::next(it);
            }
        } else if (value.is_array()) {
            for (cluster::Json& item: value)
                prune(item);
        }
    };
    cluster::Json data = response.value("data", cluster::Json());
    prune(data);
    const std::string text = ok ? (data.is_null() || (data.is_object() && data.empty()) ? "done" : data.dump()) : response.value("error", std::string("error"));
    return {{"content", {{{"type", "text"}, {"text", text}}}}, {"isError", !ok}};
}

_cold cluster::Json cluster::mcp_servers(const std::vector<std::string>& servers)
{
    cluster::Json out = cluster::Json::object();
    for (const std::string& entry: servers) {
        const std::size_t colon = entry.find(':');
        if (colon == std::string::npos) continue;
        const std::string name = cluster::trim(entry.substr(0, colon));
        const std::string rest = cluster::trim(entry.substr(colon + 1));
        if (name.empty() || rest.empty() || name == "claude-cluster") continue;
        if (rest.starts_with("http://") || rest.starts_with("https://")) {
            out[name] = {{"type", "http"}, {"url", rest}};
            continue;
        }
        // a command and its arguments (words; "quoted words" kept as one)
        std::vector<std::string> words;
        std::string word;
        char quote = 0;
        for (const char c: rest) {
            if (quote) {
                if (c == quote) quote = 0;
                else word += c;
            } else if (c == '"' || c == '\'') {
                quote = c;
            } else if (c == ' ' || c == '\t') {
                if (!word.empty()) words.push_back(word);
                word.clear();
            } else {
                word += c;
            }
        }
        if (!word.empty()) words.push_back(word);
        cluster::Json server = {{"type", "stdio"}, {"command", cluster::expand_home(words[0]).string()}, {"args", cluster::Json::array()}};
        for (std::size_t i = 1; i < words.size(); ++i)
            server["args"].push_back(words[i]);
        out[name] = server;
    }
    return out;
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
            // the ones off in the settings (MCP page) left out
            cluster::Json tools = cluster::mcp_tools();
            try {
                const cluster::Json off = cluster::Control::request({{"cmd", "mcp_off"}}).value("data", cluster::Json::array());
                cluster::Json kept = cluster::Json::array();
                for (const cluster::Json& tool: tools)
                    if (std::find(off.begin(), off.end(), tool.value("name", std::string())) == off.end()) kept.push_back(tool);
                tools = kept;
            } catch (const utils::exception::IException&) {
                // claude-cluster not reached: every tool listed (each call says it)
            }
            response["result"] = {{"tools", tools}};
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
