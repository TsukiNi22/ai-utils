/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Manager-Describe.cpp

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
#include <thread>

/* tools */
struct Source {
    std::string first;      // its first prompt
    std::string text;       // its prompts and its last answer, for the model
};

_cold static std::string ai_title_(const std::string& claudeId)
{
    // the title Claude Code wrote in its transcript (its own sessions: "ai-title", a /rename there: "custom-title"), the last one
    if (claudeId.empty()) return "";
    std::error_code error;
    std::filesystem::path file;
    for (const std::filesystem::directory_entry& folder: std::filesystem::directory_iterator(cluster::expand_home("~/.claude/projects"), error))
        if (std::filesystem::exists(folder.path() / (claudeId + ".jsonl"), error)) file = folder.path() / (claudeId + ".jsonl");
    if (file.empty()) return "";
    std::ifstream in(file);
    std::string line;
    std::string title;
    while (std::getline(in, line)) {
        if (line.find("\"type\":\"ai-title\"") == std::string::npos && line.find("\"type\":\"custom-title\"") == std::string::npos) continue;
        const cluster::Json json = cluster::Json::parse(line, nullptr, false);
        if (json.is_discarded()) continue;
        for (const char* key: {"aiTitle", "customTitle"})
            if (json.contains(key) && json[key].is_string() && !json[key].get<std::string>().empty()) title = json[key].get<std::string>();
    }
    return cluster::one_line(title, 120);
}

_cold static Source source_(const std::string& id)
{
    // what it is about: its first prompt, its last two, its last answer (its log: a session of the trash too)
    std::ifstream in(cluster::data_dir() / "logs" / (id + ".jsonl"));
    std::vector<std::string> prompts;
    std::string answer;
    std::string line;
    while (std::getline(in, line)) {
        const cluster::Json json = cluster::Json::parse(line, nullptr, false);
        if (json.is_discarded() || !json.is_object()) continue;
        const cluster::EntryKind kind = static_cast<cluster::EntryKind>(json.value("kind", 0));
        if (kind == cluster::EntryKind::User) prompts.push_back(json.value("text", std::string()));
        else if (kind == cluster::EntryKind::Assistant) answer = json.value("text", std::string());
    }
    if (prompts.empty()) return {};
    std::string text = "user: " + cluster::one_line(prompts.front(), 700);
    const std::size_t last = prompts.size() > 3 ? prompts.size() - 2 : 1; // its last two (after the first)
    for (std::size_t i = last; i < prompts.size(); ++i)
        text += "\nuser: " + cluster::one_line(prompts[i], 400);
    if (!answer.empty()) text += "\nassistant: " + cluster::one_line(answer, 700);
    return {prompts.front(), text};
}

_cold static std::string written_(const std::string& text)
{
    // a few words of a small model, its own process: no history kept, no tool, no MCP server, no setting (hooks) of the user
    const std::string prompt = "Write a short description, 12 words at most, of what this conversation with a coding agent works on or holds, "
        "to find it again in a list of sessions. Write it in the language of the user's messages. Answer with the description only: "
        "no quotes, no final period.\n\n" + text;
    const cluster::Captured out = cluster::capture({"claude", "-p", "--model", "haiku", "--no-session-persistence", "--tools", "", "--strict-mcp-config",
        "--setting-sources", "", prompt}, cluster::data_dir().string(), 60);
    if (out.code != 0) return "";
    std::string last; // its last line (a warning may come before it)
    for (const std::string& line: cluster::split(out.out, '\n'))
        if (!cluster::trim(line).empty()) last = cluster::trim(line);
    while (!last.empty() && (last.front() == '"' || last.front() == '\'' || last.front() == '*'))
        last.erase(0, 1);
    while (!last.empty() && (last.back() == '"' || last.back() == '\'' || last.back() == '*' || last.back() == '.'))
        last.pop_back();
    return cluster::one_line(last, 120);
}

/* descriptions */
_cold void cluster::Manager::describe(const std::string& id, const std::string& text)
{
    // given (the user, the global session): kept, never written again; empty: written again by claude-cluster
    // a live session (its id or name), else one of the trash (its id or name)
    std::string found = id;
    try {
        found = this->resolve(id);
    } catch (const utils::exception::IException&) {
        for (const cluster::SessionSpec& spec: this->trash())
            if (spec.id == id || spec.name == id) found = spec.id;
    }
    const std::string clean = cluster::one_line(cluster::trim(text), 160);
    if (!this->describe_(found, clean, true)) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session " + id);
    if (clean.empty()) this->describeAuto_(found);
}

_cold bool cluster::Manager::describe_(const std::string& id, const std::string& text, const bool given)
{
    // a live session or one of the trash; one written by claude-cluster never replaces one given
    std::shared_ptr<cluster::Session> session;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (auto it = this->_sessions.find(id); it != this->_sessions.end()) {
            session = it->second;
        } else {
            for (cluster::SessionSpec& spec: this->_trash) {
                if (spec.id != id) continue;
                found = true;
                if (!given && !spec.summaryAuto) break;
                spec.summary = text;
                spec.summaryAuto = !given || text.empty();
                spec.summaryTurns = spec.turns;
            }
        }
    }
    if (session) {
        session->describe(text, given);
        found = true;
    }
    if (!found) return false;
    this->save();
    ++this->_version;
    return true;
}

_cold void cluster::Manager::describeAuto_(const std::string& id)
{
    // written in a thread of its own, one at a time (a model process each): the title of Claude Code when it made one, else a few
    // words of a small model ([sessions] describe = "model"), else its first prompt
    if (this->_config.describe == "off") return;
    std::lock_guard<std::mutex> lock(this->_describer->mutex);
    std::deque<std::string>& queue = this->_describer->queue;
    if (!this->_describer->alive || std::find(queue.begin(), queue.end(), id) != queue.end()) return;
    queue.push_back(id);
    if (this->_describer->running) return;
    this->_describer->running = true;
    std::thread([this, describer = this->_describer]() {
        for (;;) {
            std::string id;
            std::string claudeId;
            std::string mode;
            Source source;
            {
                // what it needs read while claude-cluster runs (it quits: nothing more touched)
                std::lock_guard<std::mutex> hold(describer->mutex);
                if (!describer->alive || describer->queue.empty()) {
                    describer->running = false;
                    return;
                }
                id = describer->queue.front();
                describer->queue.pop_front();
                mode = this->_config.describe;
                if (const std::optional<cluster::Snapshot> snap = this->snapshot(id)) claudeId = snap->spec.claudeId;
                else
                    for (const cluster::SessionSpec& spec: this->trash())
                        if (spec.id == id) claudeId = spec.claudeId;
                source = source_(id);
            }
            if (source.first.empty()) continue; // nothing said yet
            std::string text = mode == "prompt" ? std::string() : ai_title_(claudeId);
            if (text.empty() && mode == "model" && cluster::has_command("claude")) text = written_(source.text);
            if (text.empty()) text = cluster::summary_of(source.first, 90);
            std::lock_guard<std::mutex> hold(describer->mutex);
            if (!describer->alive) {
                describer->running = false;
                return;
            }
            (void)this->describe_(id, text, false);
        }
    }).detach();
}

_cold void cluster::Manager::turnCounted_(const std::string& id, const bool failed)
{
    // a turn finished: counted; its description written after its 1st, 3rd, 9th, 27th... turn (it says more of it)
    std::shared_ptr<cluster::Session> session;
    try {
        session = this->get_(id);
    } catch (const utils::exception::IException&) {
        return;
    }
    const std::int64_t turns = session->countTurn();
    if (failed) return;
    const cluster::SessionSpec spec = session->spec();
    if (spec.summaryAuto && (spec.summary.empty() || turns >= std::max<std::int64_t>(3, spec.summaryTurns * 3))) this->describeAuto_(id);
}

_cold void cluster::Manager::describeMissing_(void)
{
    // the sessions without one (from before, the trash): written once, a few a start
    std::vector<std::string> ids;
    for (const cluster::Snapshot& snap: this->list(false))
        if (!snap.spec.global && snap.spec.summary.empty()) ids.push_back(snap.spec.id);
    for (const cluster::SessionSpec& spec: this->trash())
        if (spec.summary.empty()) ids.push_back(spec.id);
    for (std::size_t i = 0; i < ids.size() && i < 12; ++i)
        this->describeAuto_(ids[i]);
}
