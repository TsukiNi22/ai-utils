/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file Manager-External.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/External.hpp"
#include "cluster/Display.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <unistd.h>
#include <iterator>
#include <fstream>
#include <random>
#include <cmath>
#include <set>

/* tools */
_cold static std::string uuid_(void)
{
    // a random UUID (version 4): the id of a conversation of claude
    static const char* hex = "0123456789abcdef";
    std::random_device device;
    std::uniform_int_distribution<int> nibble(0, 15);
    std::string out;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) out += '-';
        out += i == 12 ? '4' : i == 16 ? hex[8 + nibble(device) % 4] : hex[nibble(device)];
    }
    return out;
}

_cold static std::string slug_(const std::string& text)
{
    // a session name from a title: lower case, letters and digits, dashes, 24 characters at most
    std::string out;
    for (const char c: cluster::lower(text)) {
        if (std::isalnum(static_cast<unsigned char>(c))) out += c;
        else if (!out.empty() && out.back() != '-') out += '-';
        if (out.size() >= 24) break;
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "imported" : out;
}

_cold static std::string agent_label_(const std::string& agent)
{
    return agent == "claude" ? "Claude Code" : agent == "codex" ? "Codex" : agent == "qwen" ? "Qwen Code" : agent == "opencode" ? "opencode" : agent;
}

_cold static std::string markdown_(const std::vector<cluster::Entry>& entries)
{
    std::string out;
    for (const cluster::Entry& e: entries) {
        if (e.kind == cluster::EntryKind::User) out += "\n## User\n" + e.text + "\n";
        else if (e.kind == cluster::EntryKind::Assistant) out += "\n## Assistant\n" + e.text + "\n";
        else if (e.kind == cluster::EntryKind::Tool) out += "- tool " + e.tool + ": " + cluster::one_line(e.text, 200) + "\n";
    }
    return out;
}

_cold static std::filesystem::path claude_transcript_(const std::string& claudeId)
{
    std::error_code error;
    for (const std::filesystem::directory_entry& folder: std::filesystem::directory_iterator(cluster::expand_home("~/.claude/projects"), error))
        if (std::filesystem::exists(folder.path() / (claudeId + ".jsonl"), error)) return folder.path() / (claudeId + ".jsonl");
    return {};
}

_cold static std::vector<cluster::Json> branch_(const std::filesystem::path& path)
{
    // the lines of the active branch of a conversation of claude, in their order (its messages and the attachments between)
    std::map<std::string, cluster::Json> messages;
    std::string last;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        cluster::Json json = cluster::Json::parse(line, nullptr, false);
        if (json.is_discarded() || !json.is_object() || !json.contains("uuid") || !json["uuid"].is_string() || json.value("isSidechain", false)) continue;
        last = json.value("uuid", std::string());
        messages[last] = std::move(json);
    }
    std::vector<cluster::Json> chain;
    for (std::string at = last; !at.empty() && messages.contains(at) && chain.size() < 100000;) {
        chain.push_back(messages[at]);
        const cluster::Json& parent = messages[at]["parentUuid"];
        at = parent.is_string() ? parent.get<std::string>() : "";
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

_cold static std::vector<std::size_t> message_positions_(const std::vector<cluster::Json>& chain)
{
    // where the messages are in the chain (index i of a message = its i-th position here)
    std::vector<std::size_t> positions;
    for (std::size_t i = 0; i < chain.size(); ++i) {
        const std::string type = chain[i].value("type", std::string());
        if ((type == "user" || type == "assistant") && chain[i].contains("message")) positions.push_back(i);
    }
    return positions;
}

_cold static std::set<std::string> tool_ids_(const cluster::Json& message, const char* type, const char* key)
{
    // the tool_use ids of a message (its tool_use blocks: "id", its tool_result blocks: "tool_use_id")
    std::set<std::string> ids;
    const cluster::Json& content = message["message"].value("content", cluster::Json());
    if (content.is_array())
        for (const cluster::Json& block: content)
            if (block.value("type", std::string()) == type && block.contains(key)) ids.insert(block.value(key, std::string()));
    return ids;
}

/* conversations of the agents */
_cold std::vector<std::string> cluster::Manager::conversationIds(void) const
{
    // the conversations opened here (sessions, trash): not offered again
    std::vector<std::string> ids;
    for (const cluster::Snapshot& snap: this->list(true))
        if (!snap.spec.claudeId.empty()) ids.push_back(snap.spec.claudeId);
    for (const cluster::SessionSpec& spec: this->trash())
        if (!spec.claudeId.empty()) ids.push_back(spec.claudeId);
    return ids;
}

_cold std::string cluster::Manager::importConversation(const std::string& agent, const std::string& id, const std::string& name, const bool fork)
{
    // a conversation of an agent opened outside: a session of claude-cluster resuming it, its transcript shown
    const std::optional<cluster::Conversation> found = cluster::external_conversation(agent, id);
    if (!found) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId,
        "no conversation '" + id + "' of " + (agent.empty() ? "the agents" : agent_label_(agent)) + " (claude-cluster external)");
    std::error_code error;
    if (found->cwd.empty() || !std::filesystem::is_directory(found->cwd, error))
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "its folder is gone: " + found->cwd);
    const bool copy = fork && found->agent == "claude";
    cluster::SpawnRequest request;
    request.cwd = found->cwd;
    request.name = name.empty() ? slug_(found->title) : name;
    request.backend = cluster::external_backend(found->agent);
    if (found->agent == "claude") { // the provider of every session, not the login of the CLI (when it drives claude: else its own)
        try {
            if (this->_auth.resolve(this->_config.provider).provider.driver == "claude") request.backend = this->_config.provider;
        } catch (const utils::exception::IException&) {}
    }
    request.resume = found->id;
    request.fork = copy;
    request.history = cluster::external_entries(*found, 400);
    request.note = "conversation of " + agent_label_(found->agent) + " opened here (" + found->id.substr(0, 8) + ", last used " + cluster::human_age(found->updated)
        + " ago): " + (copy ? "a copy, the original untouched" : "the same conversation, go on with it here only");
    return this->spawn(request);
}

_cold std::string cluster::Manager::forkSession(const std::string& id, const std::string& name)
{
    // a copy of a session of Claude Code: its conversation so far (--fork-session), then each one goes its own way
    const std::optional<cluster::Snapshot> snap = this->snapshot(id);
    if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session " + id);
    if (!this->get_(id)->persistent())
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "fork: only the sessions of Claude Code");
    if (snap->spec.claudeId.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "nothing to fork yet: no conversation");
    cluster::SpawnRequest request;
    request.cwd = snap->spec.cwd;
    request.name = name.empty() ? snap->spec.name + "-fork" : name;
    request.backend = snap->spec.backend;
    request.mode = snap->spec.mode;
    request.effort = snap->spec.effort;
    request.resume = snap->spec.claudeId;
    request.fork = true;
    const std::size_t start = snap->entries.size() > 400 ? snap->entries.size() - 400 : 0;
    request.history.assign(snap->entries.begin() + static_cast<std::ptrdiff_t>(start), snap->entries.end());
    request.note = "fork of " + snap->spec.name + ": its conversation so far copied, each one goes on apart";
    return this->spawn(request);
}

/* transcripts */
_cold cluster::Json cluster::Manager::transcript(const std::string& id, const long from, const std::size_t count) const
{
    // the transcript shown of a session, whole entries (from < 0: from its end)
    const std::optional<cluster::Snapshot> snap = this->snapshot(id);
    if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session " + id);
    const long total = static_cast<long>(snap->entries.size());
    const long start = std::clamp(from < 0 ? total + from : from, 0L, total);
    const long end = std::min(total, start + static_cast<long>(std::max<std::size_t>(1, count)));
    cluster::Json entries = cluster::Json::array();
    static const char* kinds[] = {"user", "assistant", "thinking", "tool", "tool_result", "system", "error", "display"};
    for (long i = start; i < end; ++i) {
        const cluster::Entry& e = snap->entries[static_cast<std::size_t>(i)];
        const std::size_t kind = std::min<std::size_t>(static_cast<std::size_t>(e.kind), std::size(kinds) - 1);
        entries.push_back({{"index", i}, {"kind", kinds[kind]}, {"text", e.text}, {"tool", e.tool}, {"time", e.time}});
    }
    return {{"session", id}, {"total", total}, {"from", start}, {"entries", entries}};
}

_cold void cluster::Manager::loadContext(const std::string& id, const std::string& source)
{
    // a context given to a session as a prompt: another session ("session:<id>"), a conversation of an agent
    // ("<agent>:<id>"), or a file; it keeps it and only answers "ok"
    std::string content;
    std::string label = source;
    if (source.starts_with("session:")) {
        content = this->exportMarkdown(this->resolve(source.substr(8)));
    } else if (const std::size_t colon = source.find(':'); colon != std::string::npos && !source.starts_with("/") && !source.starts_with("~")
        && std::set<std::string>{"claude", "codex", "qwen", "opencode"}.contains(source.substr(0, colon))) {
        const std::optional<cluster::Conversation> found = cluster::external_conversation(source.substr(0, colon), source.substr(colon + 1));
        if (!found) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no conversation " + source);
        content = markdown_(cluster::external_entries(*found, 400));
        label = agent_label_(found->agent) + " conversation \"" + found->title + "\"";
    } else {
        std::ifstream in(cluster::expand_home(source));
        if (!in) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "not readable: " + source);
        content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    if (cluster::trim(content).empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "an empty context: " + source);
    if (content.size() > 60000) content = "[... its start cut]\n" + content.substr(content.size() - 60000);
    this->send(id, "[context given by the global session: " + label + "]\n" + content
        + "\n[end of the context: keep it for the next requests; answer only \"ok\"]");
}

_cold cluster::Json cluster::Manager::conversationMessages(const std::string& id) const
{
    // the real conversation of a session of Claude Code (what the model sees): its messages, to edit them
    const std::optional<cluster::Snapshot> snap = this->snapshot(id);
    if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session " + id);
    const std::filesystem::path path = claude_transcript_(snap->spec.claudeId);
    if (snap->spec.claudeId.empty() || path.empty())
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no conversation of Claude Code for " + id);
    cluster::Json list = cluster::Json::array();
    const std::vector<cluster::Json> chain = branch_(path);
    const std::vector<std::size_t> positions = message_positions_(chain);
    for (std::size_t i = 0; i < positions.size(); ++i) {
        const cluster::Json& message = chain[positions[i]];
        const cluster::Json& content = message["message"].value("content", cluster::Json());
        std::string text;
        std::vector<std::string> tools;
        if (content.is_string()) text = content.get<std::string>();
        else if (content.is_array())
            for (const cluster::Json& block: content) {
                const std::string type = block.value("type", std::string());
                if (type == "text") text += block.value("text", std::string());
                else if (type == "tool_use") tools.push_back(block.value("name", std::string()));
                else if (type == "tool_result") tools.push_back("result");
            }
        list.push_back({{"index", i}, {"role", message.value("type", std::string())}, {"text", text.size() > 4000 ? text.substr(0, 4000) + " [...]" : text},
            {"tools", tools}});
    }
    return {{"session", id}, {"conversation", snap->spec.claudeId}, {"messages", list}};
}

_cold std::string cluster::Manager::editConversation(const std::string& id, const cluster::Json& edits)
{
    // the conversation of a session of Claude Code rewritten: texts replaced, messages deleted (a tool call and its result
    // together), the end cut; written as a new conversation, the session started again on it (the old one kept)
    const std::optional<cluster::Snapshot> snap = this->snapshot(id);
    if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session " + id);
    if (snap->state == cluster::State::Working || snap->state == cluster::State::Waiting || snap->state == cluster::State::Starting)
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is working: edit it when it is idle");
    const std::filesystem::path path = claude_transcript_(snap->spec.claudeId);
    if (snap->spec.claudeId.empty() || path.empty())
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no conversation of Claude Code for " + id);
    std::vector<cluster::Json> chain = branch_(path);
    const std::vector<std::size_t> positions = message_positions_(chain);
    std::size_t changes = 0;

    // replaced texts
    for (const cluster::Json& edit: edits.value("replace", cluster::Json::array())) {
        const std::size_t index = edit.value("index", static_cast<std::size_t>(-1));
        if (index >= positions.size()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no message " + std::to_string(index));
        cluster::Json& content = chain[positions[index]]["message"]["content"];
        const std::string text = edit.value("text", std::string());
        if (content.is_string()) {
            content = text;
        } else {
            bool done = false;
            cluster::Json blocks = cluster::Json::array();
            for (const cluster::Json& block: content) {
                if (block.value("type", std::string()) != "text") blocks.push_back(block);
                else if (!done) blocks.push_back({{"type", "text"}, {"text", text}});
                done = done || block.value("type", std::string()) == "text";
            }
            if (!done) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument,
                "message " + std::to_string(index) + " has no text (a tool call or its result): delete it instead");
            content = blocks;
        }
        ++changes;
    }
    // deleted messages (positions in the chain): a tool call and its result go together; keep N: the end after the N-th cut
    std::set<std::size_t> deleted;
    for (const cluster::Json& index: edits.value("delete", cluster::Json::array()))
        if (index.is_number_unsigned() && index.get<std::size_t>() < positions.size()) deleted.insert(positions[index.get<std::size_t>()]);
    const std::size_t keep = std::min(edits.value("keep", positions.size()), positions.size());
    if (keep < positions.size())
        for (std::size_t i = positions[keep]; i < chain.size(); ++i)
            deleted.insert(i);
    for (bool grew = true; grew;) {
        grew = false;
        std::set<std::string> ids;
        for (const std::size_t i: deleted) {
            for (const std::string& tool: tool_ids_(chain[i], "tool_use", "id")) ids.insert(tool);
            for (const std::string& tool: tool_ids_(chain[i], "tool_result", "tool_use_id")) ids.insert(tool);
        }
        for (const std::size_t i: positions) {
            if (deleted.contains(i)) continue;
            std::set<std::string> own = tool_ids_(chain[i], "tool_use", "id");
            own.merge(tool_ids_(chain[i], "tool_result", "tool_use_id"));
            if (std::any_of(own.begin(), own.end(), [&ids](const std::string& tool) {return ids.contains(tool);})) grew = deleted.insert(i).second || grew;
        }
    }
    changes += static_cast<std::size_t>(std::count_if(positions.begin(), positions.end(), [&deleted](const std::size_t i) {return deleted.contains(i);}));
    std::vector<cluster::Json> kept;
    for (std::size_t i = 0; i < chain.size(); ++i)
        if (!deleted.contains(i)) kept.push_back(chain[i]);
    // a prompt without its answer at the end (and what follows it): its next prompt follows it
    for (bool cut = true; cut && !edits.value("keep_last_prompt", false);) {
        cut = false;
        for (std::size_t i = kept.size(); i-- > 0;) {
            const std::string type = kept[i].value("type", std::string());
            if (type == "assistant") break;
            if (type == "user" && kept[i].contains("message") && tool_ids_(kept[i], "tool_result", "tool_use_id").empty()) {
                kept.resize(i);
                cut = true;
                break;
            }
        }
    }
    const std::size_t left = message_positions_(kept).size();
    if (left == 0) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "nothing left: use /clear instead");
    if (changes == 0) return "nothing changed";

    // the new conversation: its messages chained again, its own id, next to the old one
    const std::string conversation = uuid_();
    std::ofstream out(path.parent_path() / (conversation + ".jsonl"));
    std::string parent;
    for (cluster::Json& message: kept) {
        message["parentUuid"] = parent.empty() ? cluster::Json(nullptr) : cluster::Json(parent);
        message["sessionId"] = conversation;
        parent = message.value("uuid", std::string());
        out << message.dump(-1, ' ', false, cluster::Json::error_handler_t::replace) << "\n";
    }
    out.close();

    // its transcript shown again from it, the session on it
    const cluster::Conversation edited{"claude", conversation, "", snap->spec.cwd, "", cluster::now(), 0, path.parent_path() / (conversation + ".jsonl")};
    std::vector<cluster::Entry> entries = cluster::external_entries(edited, 400);
    entries.push_back({cluster::EntryKind::System, "conversation edited: " + std::to_string(changes) + " change(s), "
        + std::to_string(left) + " message(s) (the previous one kept: " + snap->spec.claudeId.substr(0, 8) + ")", "", cluster::now()});
    {
        std::ofstream log(cluster::data_dir() / "logs" / (id + ".jsonl"), std::ios::trunc);
        for (const cluster::Entry& e: entries)
            log << cluster::Json{{"time", e.time}, {"kind", static_cast<int>(e.kind)}, {"text", e.text}, {"tool", e.tool}}.dump() << "\n";
    }
    cluster::SessionSpec spec = snap->spec;
    spec.claudeId = conversation;
    spec.leaf.clear();
    spec.resumeAt.clear();
    spec.fork = false;
    this->relaunch_(id, spec);
    return std::to_string(changes) + " change(s), " + std::to_string(left) + " message(s) left; conversation " + conversation.substr(0, 8);
}

/* displays */
_cold static std::string spoken_(const cluster::Config& config, const std::string& text)
{
    // an audio from a text: voice-say -o (the engine and the voice of the settings), a wav file of its own
    const cluster::VoiceConf& conf = config.voice;
    const std::filesystem::path path = std::filesystem::temp_directory_path() / ("claude-cluster-say-" + std::to_string(::getpid()) + "-" + std::to_string(cluster::now()) + ".wav");
    const std::string language = cluster::language_of(text);
    std::vector<std::string> args = {cluster::expand_home(conf.sayCommand).string(), "-e", conf.sayEngine.empty() ? "piper" : conf.sayEngine, "-l", language,
        "-f", language == "en" ? conf.voiceEn : conf.voiceFr, "-o", path.string(), text};
    const cluster::Captured out = cluster::capture(args, "", 120);
    std::error_code error;
    if (out.code != 0 || !std::filesystem::exists(path, error))
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "audio: voice-say failed: " + cluster::one_line(out.out, 160));
    return path.string();
}

_cold static std::string dump_(const cluster::Json& display)
{
    return display.dump(-1, ' ', false, cluster::Json::error_handler_t::replace);
}

_cold std::string cluster::Manager::display(const std::string& id, const cluster::Json& display)
{
    // checked and made ready (a file read, a diagram drawn, a text spoken), shown in the transcript; the agent of the session
    // never sees it
    cluster::Json input = display;
    if (input.value("type", std::string()) == "file" && !input.contains("cwd")) input["cwd"] = cluster::expand_home(this->get_(id)->spec().cwd).string();
    std::string spoken;
    if (input.value("type", std::string()) == "audio" && !input.contains("path") && input.contains("text"))
        input["path"] = spoken = spoken_(this->_config, input.value("text", std::string()));
    const cluster::Json ready = cluster::display_prepare(input);
    std::error_code error;
    if (!spoken.empty()) std::filesystem::remove(spoken, error);
    this->get_(id)->append({{cluster::EntryKind::Display, dump_(ready), "", cluster::now()}});
    ++this->_version;
    return ready.value("id", std::string());
}

_cold std::optional<cluster::Json> cluster::Manager::displayOf(const std::string& id, const std::string& displayId) const
{
    if (this->_replica) {
        try {
            return this->call({{"cmd", "display_get"}, {"session", id}, {"id", displayId}});
        } catch (const utils::exception::IException&) {
            return std::nullopt;
        }
    }
    const std::optional<cluster::Snapshot> snap = this->snapshot(id);
    if (!snap) return std::nullopt;
    for (auto it = snap->entries.rbegin(); it != snap->entries.rend(); ++it) {
        if (it->kind != cluster::EntryKind::Display) continue;
        const cluster::Json d = cluster::Json::parse(it->text, nullptr, false);
        if (d.is_object() && d.value("id", std::string()) == displayId) return d;
    }
    return std::nullopt;
}

_cold void cluster::Manager::displayUpdate(const std::string& id, const std::string& displayId, const cluster::Json& display)
{
    if (!this->displayOf(id, displayId))
        throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no display " + displayId + " in " + id);
    cluster::Json input = display;
    input["id"] = displayId;
    if (input.value("type", std::string()) == "audio" && !input.contains("path") && input.contains("text"))
        input["path"] = spoken_(this->_config, input.value("text", std::string()));
    this->get_(id)->updateDisplay(displayId, dump_(cluster::display_prepare(input)));
    ++this->_version;
}

_cold std::string cluster::Manager::displayAct(const std::string& id, const std::string& displayId, const cluster::Json& act)
{
    if (this->_replica) {
        cluster::Json request = act.is_object() ? act : cluster::Json::object();
        request.update({{"cmd", "display_act"}, {"session", id}, {"id", displayId}});
        return this->call(request).value("done", std::string("done"));
    }
    std::optional<cluster::Json> found = this->displayOf(id, displayId);
    if (!found) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no display " + displayId + " in " + id);
    cluster::Json d = *found;
    const std::string type = d.value("type", std::string());
    const std::size_t item = act.value("item", static_cast<std::size_t>(0));
    std::string done;
    if (const std::optional<cluster::Snapshot> owner = this->snapshot(id); owner && owner->spec.name == "showcase" && (type == "actions" || type == "form")) {
        // the showcase: what it would do, said; nothing sent to an agent
        const std::string what = type == "actions" && item < d["buttons"].size() ? "its prompt would be sent: " + d["buttons"][item].value("prompt", std::string())
            : "its values would go to the global session: " + act.value("values", cluster::Json::object()).dump();
        this->notice(id, "showcase: " + what);
        if (type == "form") {
            d["values"] = act.value("values", cluster::Json::object());
            d["submitted"] = true;
            this->displayUpdate(id, displayId, d);
        }
        return "showcase: " + what;
    }
    if (type == "checklist") {
        if (item >= d["items"].size()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no item " + std::to_string(item));
        cluster::Json& entry = d["items"][item];
        entry["done"] = act.contains("done") ? act.value("done", false) : !entry.value("done", false);
        done = std::string(entry.value("done", false) ? "ticked: " : "unticked: ") + entry.value("text", std::string());
    } else if (type == "actions") {
        // its prompt to its session (default: the session of the display), as typed by the user
        if (item >= d["buttons"].size()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no button " + std::to_string(item));
        cluster::Json& button = d["buttons"][item];
        const std::string target = this->resolve(button.value("session", id));
        const std::string prompt = button.value("prompt", button.value("label", std::string()));
        this->send(target, prompt);
        button["clicked"] = cluster::now();
        done = "sent to " + target + ": " + cluster::one_line(prompt, 80);
    } else if (type == "form") {
        // its values checked against their fields, then to the global session (its answer for the one who asked)
        const cluster::Json values = act.value("values", cluster::Json::object());
        cluster::Json kept = cluster::Json::object();
        for (const cluster::Json& field: d["fields"]) {
            const std::string name = field.value("name", std::string());
            const std::string kind = field.value("type", std::string("text"));
            if (!values.contains(name)) {
                if (field.value("required", false)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, field.value("label", name) + ": required");
                continue;
            }
            cluster::Json value = values[name];
            if (kind == "number" && value.is_string()) {
                try {
                    const double number = std::stod(value.get<std::string>());
                    value = number == std::floor(number) && std::abs(number) < 1e15 ? cluster::Json(static_cast<long long>(number)) : cluster::Json(number);
                } catch (const std::exception&) {
                    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, field.value("label", name) + ": a number");
                }
            }
            if (kind == "bool" && value.is_string()) value = value.get<std::string>() == "true" || value.get<std::string>() == "yes" || value.get<std::string>() == "1";
            if (kind == "choice" && field.contains("options") && !value.is_string())
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, field.value("label", name) + ": one of its options");
            kept[name] = value;
        }
        d["values"] = kept;
        d["submitted"] = true;
        if (this->snapshot(GLOBAL_ID)) this->send(GLOBAL_ID, "[form \"" + d.value("title", displayId) + "\" (" + displayId + ") filled by the user] " + kept.dump());
        done = "form sent";
    } else if (type == "audio") {
        // played on this computer (the web page plays it itself)
        const std::filesystem::path path = std::filesystem::temp_directory_path() / ("claude-cluster-play-" + displayId);
        std::ofstream(path, std::ios::binary) << cluster::display_bytes(d);
        if (act.value("stop", false)) { // stopped: its player (its file is unique to the display) ended
            (void)cluster::capture({"pkill", "-f", "claude-cluster-play-" + displayId}, "", 5);
            return "stopped";
        }
        const std::string player = cluster::has_command("pw-play") ? "pw-play" : cluster::has_command("paplay") ? "paplay" : "aplay";
        cluster::detach({"sh", "-c", player + " \"$0\"; rm -f \"$0\"", path.string()});
        return "playing";
    } else {
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, type + ": nothing to do on it");
    }
    this->get_(id)->updateDisplay(displayId, dump_(d));
    ++this->_version;
    return done;
}
