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
    const std::string wanted = cluster::lower(text);
    std::vector<cluster::Hit> hits;
    std::error_code error;

    for (const std::filesystem::directory_entry& file: std::filesystem::directory_iterator(cluster::data_dir() / "logs", error)) {
        const std::string id = file.path().stem().string();
        std::ifstream in(file.path());
        std::string line;
        while (std::getline(in, line)) {
            if (cluster::lower(line).find(wanted) == std::string::npos) continue;
            try {
                const cluster::Json json = cluster::Json::parse(line);
                const std::string body = json.value("text", std::string());
                if (cluster::lower(body).find(wanted) == std::string::npos) continue;
                hits.push_back({id, names.contains(id) ? names[id] : id + " (deleted)", json.value("time", std::int64_t{0}), cluster::one_line(body, 200)});
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
            default: break;
        }
    }
    return md.str();
}

_cold cluster::Json cluster::Manager::toJson(const cluster::Snapshot& snap, const bool full) const
{
    const cluster::Metrics& m = snap.metrics;
    cluster::Json json = {
        {"id", snap.spec.id}, {"name", snap.spec.name}, {"cwd", snap.spec.cwd}, {"backend", snap.spec.backend},
        {"model", snap.modelUsed.empty() ? snap.spec.model : snap.modelUsed}, {"state", cluster::state_name(snap.state)},
        {"mode", snap.spec.mode}, {"queued", snap.queued},
        {"tokens", {{"last", m.last.total()}, {"current", m.current.total()}, {"total", m.total.total()}}},
        {"context", {{"used", m.contextUsed}, {"window", m.contextWindow}}},
        {"cost_usd", m.costKnown ? cluster::Json(m.cost) : cluster::Json("local / unknown")},
    };
    json["permissions"] = cluster::Json::array();
    for (const cluster::Permission& p: snap.permissions)
        json["permissions"].push_back({{"request_id", p.requestId}, {"tool", p.tool}, {"description", cluster::one_line(p.description, 200)}});
    if (!snap.lastError.empty()) json["error"] = snap.lastError;
    if (!snap.remoteId.empty()) json["remote"] = snap.remoteId;
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
