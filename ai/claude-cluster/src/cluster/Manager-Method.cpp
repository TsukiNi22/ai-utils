/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Manager-Method.cpp

File Description:
##  The work method: what the global session reads of it, the sample of the habits of the user it may judge, and its
##  proposal of a method (never applied before the user's Allow)
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Settings.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Method.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <fstream>
#include <regex>

/* tools */
static std::mutex method_mutex_;    // the file of the state (counter, proposal) read and written by several threads

_cold static std::filesystem::path method_file_(void)
{
    return cluster::data_dir() / "method.json";
}

_cold static cluster::Json method_state_(void)
{
    // {"spawned": sessions started since the last proposal, "pending": {preset, text, reason, time}, "declined": time}
    const std::optional<cluster::Json> state = cluster::read_json(method_file_());
    return state && state->is_object() ? *state : cluster::Json::object();
}

_cold static bool has_any_(const std::string& text, const std::vector<std::string>& words)
{
    return std::any_of(words.begin(), words.end(), [&text](const std::string& word) {return text.find(word) != std::string::npos;});
}

/* method */
_cold cluster::Json cluster::Manager::methodJson(void)
{
    std::lock_guard<std::mutex> lock(method_mutex_);
    const cluster::Json state = method_state_();
    const cluster::Config& config = this->_config;
    const cluster::MethodPreset* preset = cluster::method_preset(config.workMethod);
    const bool pending = state.contains("pending");
    cluster::Json json = {{"method", preset != nullptr ? preset->id : std::string("free")}, {"label", preset != nullptr ? preset->label : std::string("Free (no method)")},
        {"text", config.workMethodText}, {"detect", config.workMethodDetect}, {"every_sessions", config.workMethodEvery},
        {"sessions_since", state.value("spawned", 0)}, {"due", config.workMethodDetect && !pending && config.workMethodEvery > 0 && state.value("spawned", 0) >= config.workMethodEvery},
        {"injected", cluster::method_text(config, true)}, {"presets", cluster::Json::array()},
        {"how", "only the user changes the method (settings: Agent > Work method). detect on: work_method action sample, then propose - the user answers Allow / Deny"}};
    for (const cluster::MethodPreset& each: cluster::method_presets())
        json["presets"].push_back({{"id", each.id}, {"label", each.label}});
    if (pending) json["pending"] = state["pending"];
    return json;
}

_cold bool cluster::Manager::methodSpawned(void)
{
    // a session started by the global session: counted; true when the detection is due (the global session is told by the spawn)
    if (!this->_config.workMethodDetect || this->_config.workMethodEvery <= 0) return false;
    std::lock_guard<std::mutex> lock(method_mutex_);
    cluster::Json state = method_state_();
    const int spawned = state.value("spawned", 0) + 1;
    state["spawned"] = spawned;
    cluster::write_json(method_file_(), state);
    return !state.contains("pending") && spawned >= this->_config.workMethodEvery;
}

_cold cluster::Json cluster::Manager::methodSample(const std::size_t max)
{
    // a small sample of how the user phrases requests, read from the transcripts (no model): counts over their last prompts and a few of them
    if (!this->_config.workMethodDetect)
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the detection of the work method is off: the user turns it on in the settings (Agent > Detect the work method)");
    std::vector<std::filesystem::path> files;
    std::error_code error;
    for (const char* folder: {"logs", "logs/archive"})
        for (const std::filesystem::directory_entry& file: std::filesystem::directory_iterator(cluster::data_dir() / folder, error))
            if (file.is_regular_file(error)) files.push_back(file.path());
    std::sort(files.begin(), files.end(), [&error](const std::filesystem::path& a, const std::filesystem::path& b) {
        return std::filesystem::last_write_time(a, error) > std::filesystem::last_write_time(b, error);
    });
    if (files.size() > 12) files.resize(12);
    std::vector<std::pair<std::int64_t, std::string>> prompts;
    for (const std::filesystem::path& path: files) {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("\"kind\":0") == std::string::npos) continue;   // a user prompt
            const cluster::Json json = cluster::Json::parse(line, nullptr, false);
            if (!json.is_object() || json.value("kind", -1) != static_cast<int>(cluster::EntryKind::User)) continue;
            std::string text = cluster::trim(json.value("text", std::string()));
            if (text.starts_with("[voice]")) text = cluster::trim(text.substr(7));
            // not what the user wrote: a command, a message of claude-cluster or of the global session
            if (text.size() < 12 || text.starts_with("[") || text.starts_with("/") || text.starts_with("!") || text.starts_with(":")) continue;
            prompts.emplace_back(json.value("time", std::int64_t{0}), text);
        }
    }
    std::sort(prompts.begin(), prompts.end(), [](const std::pair<std::int64_t, std::string>& a, const std::pair<std::int64_t, std::string>& b) {return a.first > b.first;});
    if (prompts.size() > 300) prompts.resize(300);
    static const std::regex item(R"(^\s*(\d+[.)]|[-*•])\s+\S)");
    int lists = 0;
    int reviews = 0;
    int plans = 0;
    int shorts = 0;
    for (const auto &[time, text]: prompts) {
        int items = 0;
        for (const std::string& line: cluster::split(text, '\n'))
            items += std::regex_search(line, item) ? 1 : 0;
        lists += items >= 3 ? 1 : 0;
        const std::string low = cluster::lower(text);
        reviews += has_any_(low, {"review", "relis", "revue", "re-check", "double check", "double-check", "vérifie", "verifie", "audit", "until good", "jusqu'à ce que"}) ? 1 : 0;
        plans += has_any_(low, {"plan first", "make a plan", "propose a plan", "plan d'abord", "fais un plan", "propose un plan", "before you code", "avant de coder"}) ? 1 : 0;
        shorts += text.size() < 100 ? 1 : 0;
    }
    cluster::Json sample = cluster::Json::array();
    const std::size_t pool = std::min<std::size_t>(prompts.size(), 60);
    const std::size_t wanted = std::min<std::size_t>(std::max<std::size_t>(max, 1), 20);
    for (std::size_t i = 0; i < wanted && pool > 0; ++i)
        sample.push_back(cluster::one_line(prompts[i * pool / wanted].second, 220));
    return {{"prompts_read", prompts.size()}, {"with_task_lists", lists}, {"asking_a_review", reviews}, {"asking_a_plan", plans}, {"short_one_liners", shorts},
        {"sample", sample}, {"current", this->_config.workMethod},
        {"next", "if the habits clearly differ from the current method: work_method action propose (preset, text, reason in one sentence); else do nothing"}};
}

_cold std::string cluster::Manager::methodPropose(const std::string& preset, const std::string& text, const std::string& reason)
{
    // a proposal of the global session: the user answers Allow / Deny in their interface, only then is the setting written
    if (!this->_config.workMethodDetect)
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the detection of the work method is off: the user turns it on in the settings (Agent > Detect the work method)");
    const cluster::MethodPreset* chosen = cluster::method_preset(preset);
    if (chosen == nullptr) {
        std::string ids;
        for (const cluster::MethodPreset& each: cluster::method_presets())
            ids += (ids.empty() ? "" : ", ") + each.id;
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown preset '" + preset + "' (" + ids + ")");
    }
    const std::string own = cluster::one_line(cluster::trim(text), 400);
    const std::string why = cluster::one_line(cluster::trim(reason), 300);
    if (chosen->id == this->_config.workMethod && own == this->_config.workMethodText)
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "this is already the method of the user");
    if (chosen->id == "custom" && own.empty())
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "custom needs a text");
    {
        std::lock_guard<std::mutex> lock(method_mutex_);
        cluster::Json state = method_state_();
        if (state.contains("pending")) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "a proposal is waiting for the answer of the user");
        state["pending"] = {{"preset", chosen->id}, {"text", own}, {"reason", why}, {"time", cluster::now()}};
        state["spawned"] = 0;    // the next check: after another N sessions
        cluster::write_json(method_file_(), state);
    }
    const std::string question = "The global session suggests a work method: \"" + chosen->label + "\"" + (own.empty() ? std::string() : " + \"" + own + "\"")
        + (why.empty() ? std::string() : "\nWhy: " + why) + "\nApply it (settings: Agent > Work method)? The sessions take it at their next start.";
    const std::string label = chosen->label;
    const std::string id = chosen->id;
    this->askUser(GLOBAL_ID, "Work method", question, {{"Allow", "apply \"" + label + "\" to the work method"}, {"Deny", "keep the method as it is"}},
        [this, id, own, label](const std::string& answer) {
            std::string said;
            {
                std::lock_guard<std::mutex> lock(method_mutex_);
                cluster::Json state = method_state_();
                state.erase("pending");
                if (answer != "Allow") state["declined"] = cluster::now();
                cluster::write_json(method_file_(), state);
            }
            if (answer == "Allow") {
                try {
                    (void)cluster::settings_set(this->_config, "agent", "work_method", id);
                    (void)cluster::settings_set(this->_config, "agent", "work_method_text", own);
                    (void)this->_config.reload();
                    said = "[reply] the user allowed the work method \"" + label + "\": applied (the sub-sessions take it at their next start, you at yours); split the requests accordingly";
                } catch (const utils::exception::IException& e) {
                    said = "[reply] the user allowed the work method \"" + label + "\" but it was not written: " + e.info();
                }
            } else {
                said = "[reply] the user refused the work method \"" + label + "\": it stays as it was, do not propose it again";
            }
            if (this->snapshot(GLOBAL_ID)) {
                try {
                    this->send(GLOBAL_ID, said, true, {}, "mcp");
                } catch (const utils::exception::IException&) {} // the global session gone: nobody to tell
            }
        });
    return question;
}
