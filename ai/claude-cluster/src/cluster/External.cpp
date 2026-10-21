/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file External.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/External.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <fstream>
#include <chrono>
#include <map>

/* tools */
_cold static std::int64_t mtime_(const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::file_time_type time = std::filesystem::last_write_time(path, error);
    if (error) return 0;
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::clock_cast<std::chrono::system_clock>(time).time_since_epoch()).count();
}

_cold static std::optional<cluster::Json> parse_(const std::string& line)
{
    cluster::Json json = cluster::Json::parse(line, nullptr, false);
    if (json.is_discarded() || !json.is_object()) return std::nullopt;
    return json;
}

_cold static std::string text_of_(const cluster::Json& content)
{
    // a message content: a string, or blocks of text ({type: text|input_text|output_text, text} or {text} of Qwen)
    if (content.is_string()) return content.get<std::string>();
    std::string text;
    if (!content.is_array()) return text;
    for (const cluster::Json& block: content) {
        if (!block.is_object() || !block.contains("text") || !block["text"].is_string()) continue;
        const std::string type = block.value("type", std::string("text"));
        if (type == "text" || type == "input_text" || type == "output_text") text += (text.empty() ? "" : "\n") + block["text"].get<std::string>();
    }
    return text;
}

_cold static bool human_(const std::string& text)
{
    // a prompt typed by the user: not a command of the agent nor its context (<local-command...>, <environment_context>...)
    const std::string t = cluster::trim(text);
    return !t.empty() && !t.starts_with("<") && !t.starts_with("Caveat:");
}

_cold static std::string tool_summary_(const cluster::Json& input)
{
    if (!input.is_object()) return input.is_string() ? cluster::one_line(input.get<std::string>(), 160) : "";
    for (const char* key: {"command", "file_path", "path", "pattern", "url", "query", "description", "prompt"})
        if (input.contains(key) && input[key].is_string()) return cluster::one_line(input[key].get<std::string>(), 160);
    return cluster::one_line(input.dump(-1, ' ', false, cluster::JSON_LENIENT), 160);
}

_cold static std::vector<std::filesystem::path> files_(const std::filesystem::path& root, const std::string& pattern, const int depth)
{
    // the .jsonl files under root (depth levels of folders), pattern: the start of their name
    std::vector<std::filesystem::path> list;
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) return list;
    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, error), end; it != end; it.increment(error)) {
        if (error) break;
        if (it.depth() > depth) {
            it.disable_recursion_pending();
            continue;
        }
        const std::filesystem::path& path = it->path();
        if (it->is_regular_file(error) && path.extension() == ".jsonl" && path.filename().string().starts_with(pattern)) list.push_back(path);
    }
    return list;
}

/* claude */
_cold static cluster::Conversation claude_(const std::filesystem::path& path)
{
    // its folder and first prompt in its first lines, its title (Claude Code names it, renamed over time) in its end
    cluster::Conversation c{"claude", path.stem().string(), "", "", "", mtime_(path), 0, path};
    std::error_code error;
    c.size = std::filesystem::file_size(path, error);
    std::ifstream in(path);
    std::string line;
    for (int n = 0; n < 3000 && std::getline(in, line) && (c.cwd.empty() || c.first.empty()); ++n) {
        const std::optional<cluster::Json> json = parse_(line);
        if (!json || json->value("type", std::string()) != "user" || json->value("isMeta", false) || json->value("isSidechain", false)) continue;
        if (c.cwd.empty()) c.cwd = json->value("cwd", std::string());
        const std::string text = json->contains("message") ? text_of_((*json)["message"].value("content", cluster::Json())) : "";
        if (c.first.empty() && human_(text)) c.first = cluster::one_line(text, 200);
    }
    in.clear();
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(std::max<std::streamoff>(0, size - 262144));
    while (std::getline(in, line))
        if (line.find("\"ai-title\"") != std::string::npos)
            if (const std::optional<cluster::Json> json = parse_(line)) c.title = json->value("aiTitle", c.title);
    return c;
}

_cold static std::vector<cluster::Entry> claude_entries_(const std::filesystem::path& path)
{
    // the active branch: from the last line up its parents (a rewind leaves other branches in the file); the chain goes
    // through attachments too, only its messages are shown
    std::map<std::string, cluster::Json> messages;
    std::string last;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const std::optional<cluster::Json> json = parse_(line);
        if (!json || !json->contains("uuid") || !(*json)["uuid"].is_string() || json->value("isSidechain", false)) continue;
        last = json->value("uuid", std::string());
        messages[last] = *json;
    }
    std::vector<const cluster::Json*> chain;
    for (std::string at = last; !at.empty() && messages.contains(at) && chain.size() < 100000;) {
        chain.push_back(&messages[at]);
        const cluster::Json& parent = messages[at]["parentUuid"];
        at = parent.is_string() ? parent.get<std::string>() : "";
    }
    std::reverse(chain.begin(), chain.end());
    std::vector<cluster::Entry> entries;
    for (const cluster::Json* json: chain) {
        const std::string kind = json->value("type", std::string());
        if ((kind != "user" && kind != "assistant") || json->value("isMeta", false) || !json->contains("message")) continue;
        const cluster::Json& content = (*json)["message"].value("content", cluster::Json());
        const bool user = json->value("type", std::string()) == "user";
        if (content.is_string()) {
            if (!user || human_(content.get<std::string>())) entries.push_back({user ? cluster::EntryKind::User : cluster::EntryKind::Assistant, content.get<std::string>(), "", 0});
            continue;
        }
        if (!content.is_array()) continue;
        for (const cluster::Json& block: content) {
            const std::string type = block.value("type", std::string());
            if (type == "text" && block.contains("text") && (!user || human_(block.value("text", std::string()))))
                entries.push_back({user ? cluster::EntryKind::User : cluster::EntryKind::Assistant, block.value("text", std::string()), "", 0});
            else if (type == "tool_use")
                entries.push_back({cluster::EntryKind::Tool, tool_summary_(block.value("input", cluster::Json())), block.value("name", std::string()), 0});
            else if (type == "tool_result")
                entries.push_back({cluster::EntryKind::ToolResult, cluster::one_line(text_of_(block.value("content", cluster::Json())), 300), "", 0});
        }
    }
    return entries;
}

/* codex */
_cold static std::filesystem::path codex_home_(void)
{
    const char* home = std::getenv("CODEX_HOME");
    return home != nullptr && *home ? std::filesystem::path(home) : cluster::expand_home("~/.codex");
}

_cold static cluster::Conversation codex_(const std::filesystem::path& path)
{
    cluster::Conversation c{"codex", "", "", "", "", mtime_(path), 0, path};
    std::error_code error;
    c.size = std::filesystem::file_size(path, error);
    std::ifstream in(path);
    std::string line;
    for (int n = 0; n < 400 && std::getline(in, line) && (c.id.empty() || c.first.empty()); ++n) {
        const std::optional<cluster::Json> json = parse_(line);
        if (!json) continue;
        const cluster::Json payload = json->value("payload", cluster::Json::object());
        if (json->value("type", std::string()) == "session_meta") {
            c.id = payload.value("id", std::string());
            c.cwd = payload.value("cwd", std::string());
        } else if (payload.value("type", std::string()) == "message" && payload.value("role", std::string()) == "user") {
            const std::string text = text_of_(payload.value("content", cluster::Json()));
            if (c.first.empty() && human_(text)) c.first = cluster::one_line(text, 200);
        }
    }
    return c;
}

_cold static std::vector<cluster::Entry> codex_entries_(const std::filesystem::path& path)
{
    std::vector<cluster::Entry> entries;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const std::optional<cluster::Json> json = parse_(line);
        if (!json || json->value("type", std::string()) != "response_item") continue;
        const cluster::Json payload = json->value("payload", cluster::Json::object());
        const std::string type = payload.value("type", std::string());
        if (type == "message") {
            const std::string text = text_of_(payload.value("content", cluster::Json()));
            const bool user = payload.value("role", std::string()) == "user";
            if (user ? human_(text) : !text.empty()) entries.push_back({user ? cluster::EntryKind::User : cluster::EntryKind::Assistant, text, "", 0});
        } else if (type == "function_call") {
            const cluster::Json arguments = cluster::Json::parse(payload.value("arguments", std::string("{}")), nullptr, false);
            entries.push_back({cluster::EntryKind::Tool, tool_summary_(arguments.is_discarded() ? cluster::Json(payload.value("arguments", std::string())) : arguments),
                payload.value("name", std::string()), 0});
        }
    }
    return entries;
}

/* qwen */
_cold static cluster::Conversation qwen_(const std::filesystem::path& path)
{
    cluster::Conversation c{"qwen", path.stem().string(), "", "", "", mtime_(path), 0, path};
    std::error_code error;
    c.size = std::filesystem::file_size(path, error);
    std::ifstream in(path);
    std::string line;
    for (int n = 0; n < 400 && std::getline(in, line) && c.first.empty(); ++n) {
        const std::optional<cluster::Json> json = parse_(line);
        if (!json) continue;
        if (json->contains("sessionId")) c.id = json->value("sessionId", c.id);
        if (c.cwd.empty()) c.cwd = json->value("cwd", std::string());
        if (json->value("type", std::string()) == "user" && json->contains("message")) {
            const std::string text = text_of_((*json)["message"].value("parts", cluster::Json()));
            if (human_(text)) c.first = cluster::one_line(text, 200);
        }
    }
    return c;
}

_cold static std::vector<cluster::Entry> qwen_entries_(const std::filesystem::path& path)
{
    std::vector<cluster::Entry> entries;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const std::optional<cluster::Json> json = parse_(line);
        if (!json || !json->contains("message")) continue;
        const std::string type = json->value("type", std::string());
        const std::string text = text_of_((*json)["message"].value("parts", cluster::Json()));
        if (type == "user" && human_(text)) entries.push_back({cluster::EntryKind::User, text, "", 0});
        else if ((type == "assistant" || type == "model" || type == "gemini") && !text.empty()) entries.push_back({cluster::EntryKind::Assistant, text, "", 0});
    }
    return entries;
}

/* opencode */
_cold static std::vector<cluster::Conversation> opencode_(const std::size_t limit)
{
    // in its own database: asked to it (opencode session list)
    std::vector<cluster::Conversation> list;
    if (!cluster::has_command("opencode")) return list;
    const cluster::Captured out = cluster::capture({"opencode", "session", "list", "--format", "json", "-n", std::to_string(limit)}, "", 30);
    const cluster::Json json = cluster::Json::parse(out.out, nullptr, false);
    if (out.code != 0 || !json.is_array()) return list;
    for (const cluster::Json& s: json) {
        if (!s.is_object()) continue;
        const std::string title = s.value("title", std::string());
        list.push_back({"opencode", s.value("id", std::string()), title, s.value("directory", std::string()), title,
            s.value("updated", std::int64_t{0}) / 1000, 0, {}});
    }
    return list;
}

/* functions */
_cold std::vector<cluster::Conversation> cluster::external_conversations(const std::string& agent, const std::size_t limit, const std::vector<std::string>& known)
{
    // the newest files first: only them are read (a long history stays fast)
    std::vector<cluster::Conversation> list;
    using File = std::pair<std::int64_t, std::pair<std::string, std::filesystem::path>>;    // <its time, <agent, path>>
    std::vector<File> files;
    if (agent.empty() || agent == "claude")
        for (const std::filesystem::path& path: files_(cluster::expand_home("~/.claude/projects"), "", 1))
            files.push_back({mtime_(path), {"claude", path}});
    if (agent.empty() || agent == "codex")
        for (const std::filesystem::path& path: files_(codex_home_() / "sessions", "rollout-", 4))
            files.push_back({mtime_(path), {"codex", path}});
    if (agent.empty() || agent == "qwen")
        for (const std::filesystem::path& path: files_(cluster::expand_home("~/.qwen/projects"), "", 2))
            files.push_back({mtime_(path), {"qwen", path}});
    std::sort(files.begin(), files.end(), [](const File& a, const File& b) {return a.first > b.first;});
    for (const auto &[time, file]: files) {
        if (list.size() >= limit) break;
        const cluster::Conversation c = file.first == "claude" ? claude_(file.second) : file.first == "codex" ? codex_(file.second) : qwen_(file.second);
        if (c.id.empty() || c.first.empty() || std::find(known.begin(), known.end(), c.id) != known.end()) continue; // empty, or opened here
        list.push_back(c);
    }
    if (agent.empty() || agent == "opencode")
        for (const cluster::Conversation& c: opencode_(limit))
            if (std::find(known.begin(), known.end(), c.id) == known.end()) list.push_back(c);
    std::stable_sort(list.begin(), list.end(), [](const cluster::Conversation& a, const cluster::Conversation& b) {return a.updated > b.updated;});
    if (list.size() > limit) list.resize(limit);
    for (cluster::Conversation& c: list)
        if (c.title.empty()) c.title = cluster::one_line(c.first, 80);
    return list;
}

_cold std::optional<cluster::Conversation> cluster::external_conversation(const std::string& agent, const std::string& id)
{
    const std::string wanted = cluster::trim(id);
    if (wanted.empty()) return std::nullopt;
    if (agent == "claude" || agent.empty())
        for (const std::filesystem::path& path: files_(cluster::expand_home("~/.claude/projects"), wanted, 1))
            return claude_(path);
    std::optional<cluster::Conversation> found;
    for (const cluster::Conversation& c: cluster::external_conversations(agent, 400)) {
        if (c.id == wanted) return c;
        if (c.id.starts_with(wanted)) {
            if (found) return std::nullopt; // not unique
            found = c;
        }
    }
    return found;
}

_cold std::vector<cluster::Entry> cluster::external_entries(const cluster::Conversation& conversation, const std::size_t max)
{
    std::vector<cluster::Entry> entries;
    if (conversation.path.empty()) return entries;
    if (conversation.agent == "claude") entries = claude_entries_(conversation.path);
    else if (conversation.agent == "codex") entries = codex_entries_(conversation.path);
    else if (conversation.agent == "qwen") entries = qwen_entries_(conversation.path);
    if (entries.size() > max) entries.erase(entries.begin(), entries.end() - static_cast<std::ptrdiff_t>(max));
    for (cluster::Entry& entry: entries)
        entry.time = conversation.updated;
    return entries;
}

_cold std::string cluster::external_backend(const std::string& agent)
{
    if (agent == "claude") return "anthropic";
    if (agent == "qwen") return "openai";
    return agent;
}

_cold cluster::Json cluster::external_json(const cluster::Conversation& c)
{
    return {{"agent", c.agent}, {"id", c.id}, {"title", c.title}, {"cwd", c.cwd}, {"first_prompt", c.first}, {"updated", c.updated},
        {"age", cluster::human_age(c.updated)}, {"size_kb", c.size / 1024}};
}
