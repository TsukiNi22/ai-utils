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
             {"mode", str_("auto | manual | acceptEdits | plan | dontAsk | bypassPermissions")}, {"profile", str_("profile of the config")}, {"prompt", str_("first prompt")}}, {"cwd"}),
        tool_("session_send", "Send a prompt to a session (queued when it is working).", {{"session", session}, {"text", str_("the prompt")}}, {"session", "text"}),
        tool_("session_allow", "Answer a pending permission of a session.",
            {{"session", session}, {"request_id", str_("request id (empty: the oldest one)")}, {"behavior", str_("allow | always | deny")}}, {"session", "behavior"}),
        tool_("session_set_mode", "Permission mode of a session.", {{"session", session}, {"mode", str_("auto | manual | acceptEdits | plan | dontAsk | bypassPermissions")}}, {"session", "mode"}),
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
        tool_("task_cancel", "Cancel a task.", {{"id", str_("task id (t1...)")}}, {"id"}),
        tool_("search", "Full-text search in the history of every session.", {{"text", str_("text to find")}}, {"text"}),
        tool_("export", "Export the conversation of a session as Markdown (to a file when path is given).", {{"session", session}, {"path", str_("file (optional)")}}, {"session"}),
        tool_("providers", "Providers / backends and whether they are ready (logged in, key stored, server up).", nullptr, {}),
    });
}

_cold cluster::Json cluster::mcp_call(const std::string& name, const cluster::Json& arguments)
{
    static const std::map<std::string, std::string> commands = {
        {"session_list", "list"}, {"session_status", "status"}, {"session_read", "read"}, {"session_spawn", "spawn"}, {"session_send", "send"},
        {"session_allow", "allow"}, {"session_set_mode", "mode"}, {"session_interrupt", "interrupt"}, {"session_close", "close"},
        {"session_restore", "restore"}, {"session_trash", "trash"}, {"session_purge", "purge"}, {"session_rename", "rename"}, {"session_cd", "cd"},
        {"task_add", "task_add"}, {"task_list", "task_list"}, {"task_cancel", "task_cancel"}, {"search", "search"}, {"export", "export"},
        {"providers", "providers"},
    };
    auto it = commands.find(name);
    if (it == commands.end()) return {{"content", {{{"type", "text"}, {"text", "unknown tool " + name}}}}, {"isError", true}};

    cluster::Json request = arguments.is_object() ? arguments : cluster::Json::object();
    request["cmd"] = it->second;
    if (name == "session_list") request["global"] = false; // the global session lists the others
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
                {"instructions", "Tools of claude-cluster: manage the other Claude Code / agent sessions of the user (list, spawn, send, permissions, modes, tasks)."},
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
