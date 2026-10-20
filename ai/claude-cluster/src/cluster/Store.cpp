/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Store.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Display.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

/* tools */
_cold static const char* kind_name_(const cluster::EntryKind kind)
{
    switch (kind) {
        case cluster::EntryKind::User: return "user";
        case cluster::EntryKind::Assistant: return "assistant";
        case cluster::EntryKind::Thinking: return "thinking";
        case cluster::EntryKind::Tool: return "tool";
        case cluster::EntryKind::ToolResult: return "result";
        case cluster::EntryKind::System: return "system";
        case cluster::EntryKind::Error: return "error";
        case cluster::EntryKind::Display: return "display";
    }
    return "?";
}

/* history */
_cold std::vector<cluster::Hit> cluster::Manager::search(const std::string& text, const std::size_t max) const
{
    // Every log (sessions alive and in the trash), newest first
    std::map<std::string, std::string> names;
    for (const cluster::Snapshot& snap: this->list())
        names[snap.spec.id] = snap.spec.name;
    for (const cluster::SessionSpec& spec: this->trash())
        names[spec.id] = spec.name + " (trash)";
    const bool labelsOnly = cluster::trim(text).starts_with("#");   // "#bugfix": the sessions with that category or tag
    const std::string wanted = cluster::lower(labelsOnly ? cluster::trim(text).substr(1) : text);
    std::vector<cluster::Hit> hits;
    std::error_code error;

    // the category and the tags of the sessions (alive and in the trash)
    const std::function<void(const cluster::SessionSpec&, const std::string&)> byLabel = [&](const cluster::SessionSpec& spec, const std::string& name) {
        std::string tags;
        bool found = !spec.category.empty() && (labelsOnly ? spec.category == wanted : spec.category.find(wanted) != std::string::npos);
        for (const std::string& tag: spec.tags) {
            tags += (tags.empty() ? "" : " ") + std::string("#") + tag;
            if (labelsOnly ? tag == wanted : tag.find(wanted) != std::string::npos) found = true;
        }
        if (found && !wanted.empty()) hits.push_back({spec.id, name, spec.deleted > 0 ? spec.deleted : spec.created, "[label] " + (spec.category.empty() ? std::string() : spec.category + " ") + tags});
    };
    for (const cluster::Snapshot& snap: this->list())
        byLabel(snap.spec, names[snap.spec.id]);
    for (const cluster::SessionSpec& spec: this->trash())
        byLabel(spec, names[spec.id]);
    if (labelsOnly) {
        std::sort(hits.begin(), hits.end(), [](const cluster::Hit& a, const cluster::Hit& b) {return a.time > b.time;});
        if (hits.size() > max) hits.resize(max);
        return hits;
    }

    // the logs of the sessions, then the conversations archived by /clear ("<id>-<time>.jsonl")
    std::vector<std::filesystem::path> files;
    for (const char* folder: {"logs", "logs/archive"})
        for (const std::filesystem::directory_entry& file: std::filesystem::directory_iterator(cluster::data_dir() / folder, error))
            if (file.is_regular_file(error)) files.push_back(file.path());
    for (const std::filesystem::path& path: files) {
        const std::string stem = path.stem().string();
        const bool archived = path.parent_path().filename() == "archive";
        const std::string id = archived ? stem.substr(0, stem.find('-')) : stem;
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            if (cluster::lower(line).find(wanted) == std::string::npos) continue;
            try {
                const cluster::Json json = cluster::Json::parse(line);
                std::string body = json.value("text", std::string());
                if (json.value("kind", 0) == static_cast<int>(cluster::EntryKind::Display)) body = cluster::display_text(cluster::Json::parse(body, nullptr, false));
                if (cluster::lower(body).find(wanted) == std::string::npos) continue;
                const std::string name = names.contains(id) ? names[id] : id + " (deleted)";
                hits.push_back({id, archived ? name + " (archive)" : name, json.value("time", std::int64_t{0}), cluster::one_line(body, 200)});
            } catch (const cluster::Json::exception&) {}
        }
    }
    std::sort(hits.begin(), hits.end(), [](const cluster::Hit& a, const cluster::Hit& b) {return a.time > b.time;});
    if (hits.size() > max) hits.resize(max);
    return hits;
}

_cold std::string cluster::Manager::exportMarkdown(const std::string& id) const
{
    std::optional<cluster::Snapshot> snap = this->snapshot(id);
    std::string name = id;
    std::string cwd;
    if (snap) {
        name = snap->spec.name;
        cwd = snap->spec.cwd;
    } else {
        for (const cluster::SessionSpec& spec: this->trash())
            if (spec.id == id) {
                name = spec.name;
                cwd = spec.cwd;
            }
    }
    std::ifstream in(cluster::data_dir() / "logs" / (id + ".jsonl"));
    if (!in) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no history for '" + id + "'");
    std::ostringstream md;
    md << "# " << name << "\n\n> claude-cluster session `" << id << "`" << (cwd.empty() ? "" : " in `" + cluster::short_path(cwd) + "`") << "\n";

    std::string line;
    while (std::getline(in, line)) {
        cluster::Json json;
        try {
            json = cluster::Json::parse(line);
        } catch (const cluster::Json::exception&) {
            continue;
        }
        const cluster::EntryKind kind = static_cast<cluster::EntryKind>(json.value("kind", 0));
        const std::string text = json.value("text", std::string());
        const std::string time = cluster::clock_time(json.value("time", std::int64_t{0}));
        switch (kind) {
            case cluster::EntryKind::User: md << "\n## User (" << time << ")\n\n" << text << "\n"; break;
            case cluster::EntryKind::Assistant: md << "\n## Assistant (" << time << ")\n\n" << text << "\n"; break;
            case cluster::EntryKind::Tool: md << "\n- `" << json.value("tool", std::string()) << "` " << text << "\n"; break;
            case cluster::EntryKind::Error: md << "\n> [!WARNING]\n> " << cluster::one_line(text, 400) << "\n"; break;
            case cluster::EntryKind::System: md << "\n*" << text << "*\n"; break;
            case cluster::EntryKind::Display: md << "\n" << cluster::display_text(cluster::Json::parse(text, nullptr, false)) << "\n"; break;
            default: break;
        }
    }
    return md.str();
}

/* prompts: history and stash */
_cold void cluster::Manager::addHistory(const cluster::Draft& draft, const std::string& session)
{
    // the [voice] mark is for the global session only: not in the history
    const std::string text = draft.text.starts_with("[voice] ") ? draft.text.substr(8) : draft.text;
    if (cluster::trim(text).empty()) return;
    std::error_code error;
    std::filesystem::create_directories(cluster::data_dir(), error);
    std::lock_guard<std::mutex> lock(this->_mutex);
    std::ofstream(cluster::data_dir() / "history.jsonl", std::ios::app) << cluster::Json{{"time", cluster::now()}, {"session", session}, {"text", text},
        {"images", draft.images}, {"pastes", draft.pastes}}.dump() << "\n";
    if (this->_config.historyLimit > 0 && this->_historyWrites++ % 25 == 0) // its oldest prompts dropped (checked every 25: not rewritten each time)
        (void)cluster::keep_newest(cluster::data_dir() / "history.jsonl", static_cast<std::size_t>(this->_config.historyLimit), 0);
}

_cold std::vector<std::string> cluster::Manager::history(const std::size_t max, const std::string& session) const
{
    std::vector<std::string> texts;
    for (const cluster::Draft& draft: this->drafts(max, session))
        texts.push_back(draft.text);
    return texts;
}

_cold std::vector<cluster::Draft> cluster::Manager::drafts(const std::size_t max, const std::string& session) const
{
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        std::ifstream in(cluster::data_dir() / "history.jsonl");
        std::string line;
        while (std::getline(in, line))
            lines.push_back(line);
    }
    std::vector<cluster::Draft> list;
    for (auto it = lines.rbegin(); it != lines.rend() && list.size() < max; ++it) {
        const cluster::Json json = cluster::Json::parse(*it, nullptr, false);
        if (json.is_discarded() || (!session.empty() && json.value("session", std::string()) != session)) continue;
        const cluster::Draft draft{json.value("text", std::string()), json.value("images", std::vector<std::string>()), json.value("pastes", std::vector<std::string>())};
        if (!draft.text.empty() && std::none_of(list.begin(), list.end(), [&](const cluster::Draft& d) {return d.text == draft.text;})) list.push_back(draft);
    }
    return list;
}

_cold void cluster::Manager::stashPush(const cluster::Draft& draft)
{
    if (cluster::trim(draft.text).empty()) return;
    std::lock_guard<std::mutex> lock(this->_mutex);
    cluster::Json stash = cluster::read_json(cluster::data_dir() / "stash.json").value_or(cluster::Json::array());
    if (!stash.is_array()) stash = cluster::Json::array();
    stash.push_back(cluster::Json::object({{"text", draft.text}, {"images", draft.images}, {"pastes", draft.pastes}}));
    cluster::write_json(cluster::data_dir() / "stash.json", stash);
}

_cold std::optional<cluster::Draft> cluster::Manager::stashPop(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    cluster::Json stash = cluster::read_json(cluster::data_dir() / "stash.json").value_or(cluster::Json::array());
    if (!stash.is_array() || stash.empty()) return std::nullopt;
    const cluster::Json& last = stash.back(); // a text (older stash) or a draft
    const cluster::Draft text = last.is_string() ? cluster::Draft{last.get<std::string>(), {}, {}}
        : cluster::Draft{last.value("text", std::string()), last.value("images", std::vector<std::string>()), last.value("pastes", std::vector<std::string>())};
    stash.erase(stash.size() - 1);
    cluster::write_json(cluster::data_dir() / "stash.json", stash);
    return text;
}

_cold std::size_t cluster::Manager::stashSize(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    const std::optional<cluster::Json> stash = cluster::read_json(cluster::data_dir() / "stash.json");
    return stash && stash->is_array() ? stash->size() : 0;
}

_cold static cluster::Json usage_json_(const cluster::Usage& u)
{
    return {{"input", u.input}, {"cache_write", u.cacheCreation}, {"cache_write_1h", u.cacheCreation1h}, {"cache_read", u.cacheRead}, {"output", u.output}, {"thinking", u.thinking}};
}

_cold cluster::Json cluster::Manager::toJson(const cluster::Snapshot& snap, const bool full) const
{
    const cluster::Metrics& m = snap.metrics;
    cluster::Json json = {
        {"id", snap.spec.id}, {"name", snap.spec.name}, {"cwd", snap.spec.cwd}, {"backend", snap.spec.backend},
        {"model", snap.modelUsed.empty() ? snap.spec.model : snap.modelUsed}, {"state", cluster::state_name(snap.state)},
        {"mode", snap.spec.mode}, {"effort", snap.spec.effort.empty() ? std::string("default") : snap.spec.effort}, {"auth", this->authKind(snap.spec.backend)},
        {"queued", snap.queued}, {"queue_given", snap.queueGiven}, {"summary", snap.spec.summary}, {"pinned", snap.spec.pin.is_object() ? cluster::Json{{"kind", snap.spec.pin.value("kind", std::string())}, {"id", snap.spec.pin.value("id", std::string())}, {"title", snap.spec.pin.value("title", std::string())}} : cluster::Json()}, {"category", snap.spec.category}, {"tags", snap.spec.tags}, {"resumable", cluster::resumable(snap)},
        {"warning", !snap.warning.empty() && cluster::now() - snap.warningAt < 60 ? snap.warning : std::string()},
        {"tokens", {{"last", m.last.total()}, {"current", m.current.total()}, {"total", m.total.total()},
            {"detail", {{"last", usage_json_(m.last)}, {"current", usage_json_(m.current)}, {"total", usage_json_(m.total)}}}}},
        {"context", {{"used", m.contextUsed}, {"window", m.contextWindow}}},
        {"cost_usd", m.costKnown ? cluster::Json(m.cost) : cluster::Json("local / unknown")},
    };
    json["permissions"] = cluster::Json::array();
    for (const cluster::Permission& p: snap.permissions) {
        json["permissions"].push_back({{"request_id", p.requestId}, {"tool", p.tool}, {"description", cluster::one_line(p.description, 200)}});
        if (!p.reason.empty()) json["permissions"].back()["reason"] = cluster::one_line(p.reason, 300);
        if (p.safety) json["permissions"].back()["safety"] = true;
        // a question of the agent: its questions and options (answered with session_allow answers {question: label})
        if (p.tool == "AskUserQuestion" && p.input.contains("questions")) json["permissions"].back()["questions"] = p.input["questions"];
    }
    json["where"] = {{"folder", cluster::expand_home(snap.spec.cwd).string()}, {"host", cluster::host_name()}, {"pid", snap.pid},
        {"branch", cluster::git_branch(cluster::expand_home(snap.spec.cwd))}};
    if (!snap.lastError.empty()) json["error"] = snap.lastError;
    if (!snap.remoteUrl.empty()) json["remote"] = snap.remoteUrl;
    if (!full) return json;
    json["skills_loaded"] = snap.skillsLoaded;
    json["files_modified"] = snap.files;
    json["messages"] = cluster::Json::array();
    const std::size_t start = snap.entries.size() > 20 ? snap.entries.size() - 20 : 0;
    for (std::size_t i = start; i < snap.entries.size(); ++i) {
        const cluster::Entry& e = snap.entries[i];
        if (e.kind == cluster::EntryKind::Thinking) continue;
        json["messages"].push_back({{"kind", kind_name_(e.kind)}, {"text", cluster::one_line(e.text, e.kind == cluster::EntryKind::Assistant ? 2000 : 300)},
            {"tool", e.tool}, {"time", e.time}});
    }
    return json;
}
