/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Manager.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Manager.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Models.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Mcp.hpp"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <regex>

/* tools */
_cold static cluster::Json spec_to_json_(const cluster::SessionSpec& spec)
{
    return {
        {"id", spec.id}, {"name", spec.name}, {"cwd", spec.cwd}, {"backend", spec.backend}, {"claude_id", spec.claudeId}, {"leaf", spec.leaf}, {"resume_at", spec.resumeAt}, {"fork", spec.fork},
        {"model", spec.model}, {"mode", spec.mode}, {"effort", spec.effort}, {"profile", spec.profile}, {"extra_args", spec.extraArgs},
        {"panels", spec.panels}, {"global", spec.global}, {"created", spec.created}, {"deleted", spec.deleted}, {"cost", spec.cost}, {"tokens", spec.tokens},
        {"summary", spec.summary}, {"summary_auto", spec.summaryAuto}, {"summary_turns", spec.summaryTurns}, {"turns", spec.turns},
    };
}

_cold static cluster::SessionSpec spec_from_json_(const cluster::Json& json)
{
    cluster::SessionSpec spec;
    spec.id = json.value("id", std::string());
    spec.name = json.value("name", spec.id);
    spec.cwd = json.value("cwd", std::string("~"));
    spec.backend = json.value("backend", std::string("claude"));
    spec.claudeId = json.value("claude_id", std::string());
    spec.leaf = json.value("leaf", std::string());
    spec.resumeAt = json.value("resume_at", std::string());
    spec.fork = json.value("fork", false);
    spec.model = json.value("model", std::string());
    spec.mode = json.value("mode", std::string("default"));
    spec.effort = json.value("effort", std::string());
    spec.profile = json.value("profile", std::string());
    spec.extraArgs = json.value("extra_args", std::vector<std::string>());
    spec.panels = json.value("panels", std::set<std::string>());
    spec.global = json.value("global", false);
    spec.created = json.value("created", std::int64_t{0});
    spec.deleted = json.value("deleted", std::int64_t{0});
    spec.cost = json.value("cost", 0.0);
    spec.tokens = json.value("tokens", std::int64_t{0});
    spec.summary = json.value("summary", std::string());
    spec.summaryAuto = json.value("summary_auto", true);
    spec.summaryTurns = json.value("summary_turns", std::int64_t{0});
    spec.turns = json.value("turns", std::int64_t{0});
    return spec;
}

_cold static std::filesystem::path state_file_(void)
{
    return cluster::data_dir() / "state.json";
}

_cold static std::filesystem::path log_file_(const std::string& id)
{
    return cluster::data_dir() / "logs" / (id + ".jsonl");
}

_cold static std::vector<std::string> prompt_args_(const std::string& file)
{
    // a system prompt file added to the one of claude (CLAUDE.md kept); missing, empty or blank: none
    std::ifstream in(cluster::expand_home(file));
    std::stringstream prompt;
    prompt << in.rdbuf();
    if (cluster::trim(prompt.str()).empty()) return {};
    return {"--append-system-prompt", prompt.str()};
}

/* constructor */
_cold cluster::Manager::Manager(cluster::Config& config)
    : _config(config), _auth(config), _asking(std::make_unique<cluster::Asking>(*this))
{
    this->loadUsage_();
}

_cold cluster::Manager::~Manager()
{
    this->shutdown();
}

/* sessions */
_cold std::shared_ptr<cluster::Session> cluster::Manager::get_(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    auto it = this->_sessions.find(id);
    if (it == this->_sessions.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session '" + id + "'");
    return it->second;
}

_cold std::shared_ptr<cluster::Session> cluster::Manager::make_(cluster::SessionSpec spec)
{
    cluster::Launch launch;
    launch.backend = this->_auth.resolve(spec.backend);
    launch.env = this->_auth.env(launch.backend);
    launch.command = this->_config.command(launch.backend.provider.driver);
    launch.allowBypass = this->_config.allowBypass;
    if (!cluster::has_command(launch.command))
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument,
            "`" + launch.command + "` is not installed (backend " + spec.backend + ")");
    if (launch.backend.provider.driver == "claude") {
        launch.extraArgs = this->_config.extraArgs;
        if (!spec.global) {
            const std::vector<std::string> prompt = prompt_args_(this->_config.sessionsPrompt);
            launch.extraArgs.insert(launch.extraArgs.end(), prompt.begin(), prompt.end());
        }
    }
    if (spec.global) {
        const std::vector<std::string> global = this->globalArgs_();
        launch.extraArgs.insert(launch.extraArgs.end(), global.begin(), global.end());
        if (this->remote() && launch.backend.provider.driver == "claude") launch.remoteName = this->_config.remoteName; // on again
    }
    spec.model = launch.backend.model;
    launch.spec = spec;
    launch.log = log_file_(spec.id);
    launch.listener = [this](const cluster::Event& event) {this->event_(event);};
    return std::make_shared<cluster::Session>(std::move(launch));
}

_cold std::string cluster::Manager::spawn(const cluster::SpawnRequest& request)
{
    cluster::SpawnRequest req = request;
    if (!req.profile.empty()) {
        auto it = this->_config.profiles.find(req.profile);
        if (it == this->_config.profiles.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no profile '" + req.profile + "'");
        const cluster::Profile& profile = it->second;
        if (req.cwd.empty()) req.cwd = profile.cwd;
        if (req.backend.empty()) req.backend = profile.backend;
        if (req.mode.empty()) req.mode = profile.mode;
        if (req.prompt.empty()) req.prompt = profile.prompt;
    }
    const std::filesystem::path cwd = std::filesystem::weakly_canonical(cluster::expand_home(req.cwd.empty() ? "." : req.cwd));
    if (!std::filesystem::is_directory(cwd)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "not a folder: " + cwd.string());

    cluster::SessionSpec spec;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        spec.id = "s" + std::to_string(this->_next++);
        std::string name = req.name.empty() ? cwd.filename().string() : req.name;
        const std::string base = name;
        for (int n = 2; std::any_of(this->_sessions.begin(), this->_sessions.end(), [&](const std::pair<const std::string, std::shared_ptr<cluster::Session>>& s) {return s.second->spec().name == name;}); ++n)
            name = base + "-" + std::to_string(n);
        spec.name = name;
    }
    spec.cwd = cwd.string();
    spec.backend = req.backend.empty() ? this->_config.backend : req.backend;
    spec.mode = req.mode.empty() ? this->_config.mode : req.mode;
    spec.effort = req.effort.empty() ? this->_config.effort : req.effort;
    spec.profile = req.profile;
    spec.panels = std::set<std::string>(this->_config.panels.begin(), this->_config.panels.end());
    spec.created = cluster::now();
    spec.claudeId = req.resume;     // a conversation resumed (another agent's, a fork of a session)
    spec.fork = req.fork && !req.resume.empty();
    if (!req.profile.empty()) spec.extraArgs = this->_config.profiles.at(req.profile).extraArgs;

    const std::shared_ptr<cluster::Session> session = this->make_(spec);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_sessions[spec.id] = session;
        this->_order.push_back(spec.id);
    }
    session->append(req.history);   // its transcript so far, shown (and in its log)
    if (!req.note.empty()) session->mark(req.note);
    session->start();
    if (!req.prompt.empty()) session->send(req.prompt);
    this->notice_(spec.id, "new session " + spec.name + " (" + spec.backend + ") in " + cluster::short_path(spec.cwd));
    this->save();
    return spec.id;
}

void cluster::Manager::setOrigin(const std::string& id, const std::string& origin)
{
    const std::string resolved = this->resolve(id); // before the lock (it takes it)
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_origin[resolved] = origin;
}

std::string cluster::Manager::origin(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    auto it = this->_origin.find(id);
    return it == this->_origin.end() ? std::string("host") : it->second;
}

_cold void cluster::Manager::send(const std::string& id, const std::string& text, const bool force, const std::vector<std::string>& images)
{
    const std::shared_ptr<cluster::Session> session = this->get_(id);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_origin[session->id()] = "host"; // a web device marks it after (Web: setOrigin)
    }
    const cluster::Snapshot snap = session->snapshot();

    if (!force && this->_config.budget > 0 && snap.metrics.costKnown && snap.metrics.cost >= this->_config.budget) {
        this->notice_(id, "budget reached (" + cluster::human_cost(snap.metrics.cost) + "): prompt refused", true);
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction,
            "budget of " + cluster::human_cost(this->_config.budget) + " reached for " + snap.spec.name + " (force it, or raise sessions.budget_usd)");
    }
    session->send(text, images);
}

_cold void cluster::Manager::clear(const std::string& id)
{
    // a new conversation (like /clear of Claude Code): the session again without --resume, an empty transcript (the old
    // one archived: found by the search, not shown)
    const std::shared_ptr<cluster::Session> old = this->get_(id);
    old->stop();
    cluster::SessionSpec spec = old->spec();
    spec.claudeId.clear();
    spec.leaf.clear();
    spec.resumeAt.clear();
    this->archive_(id);
    const std::shared_ptr<cluster::Session> session = this->make_(spec);
    session->loadHistory(300);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_sessions[id] = session;
    }
    session->start();
    this->notice_(id, spec.name + ": new conversation (the transcript before archived)");
    this->save();
}

_cold void cluster::Manager::setModel(const std::string& id, const std::string& model)
{
    // the same conversation (--resume) on another model: the session started again
    cluster::SessionSpec spec = this->get_(id)->spec();
    spec.backend = spec.backend.substr(0, spec.backend.find('/')) + "/" + model;
    spec.model = model;
    this->relaunch_(id, spec);
    this->notice_(id, spec.name + ": model " + model);
}

_cold void cluster::Manager::setEffort(const std::string& id, const std::string& effort)
{
    // thinking / reasoning effort (default: the one of the agent): the same conversation started again
    cluster::SessionSpec spec = this->get_(id)->spec();
    const std::string driver = this->_auth.resolve(spec.backend).provider.driver;
    const std::vector<std::string>& levels = cluster::effort_levels(driver);
    if (levels.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no effort level for the " + driver + " driver");
    if (effort != "default" && std::find(levels.begin(), levels.end(), effort) == levels.end()) {
        std::string list = "default";
        for (const std::string& level: levels)
            list += ", " + level;
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "effort '" + effort + "': " + list);
    }
    spec.effort = effort;
    this->relaunch_(id, spec);
    this->notice_(id, spec.name + ": effort " + effort);
}

_cold void cluster::Manager::relaunch_(const std::string& id, const cluster::SessionSpec& spec)
{
    // the session started again with this spec (its conversation: --resume), its transcript kept
    this->get_(id)->stop();
    const std::shared_ptr<cluster::Session> session = this->make_(spec);
    session->loadHistory(300);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_sessions[id] = session;
    }
    session->start();
    this->save();
}

_cold void cluster::Manager::close(const std::string& id)
{
    if (id == GLOBAL_ID) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the global session can't be closed (quit claude-cluster)");
    const std::shared_ptr<cluster::Session> session = this->get_(id);
    session->stop();
    cluster::SessionSpec spec = session->spec();
    spec.deleted = cluster::now();
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_sessions.erase(id);
        std::erase(this->_order, id);
        this->_trash.push_back(spec);
        for (cluster::Task& task: this->_tasks)
            if (task.session == id && task.status == "running") task.status = "failed";
    }
    this->notice_(id, spec.name + " closed (restorable " + std::to_string(this->_config.trashDays) + " days)");
    this->save();
}

_cold std::string cluster::Manager::restore(const std::string& id)
{
    cluster::SessionSpec spec;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_trash.begin(), this->_trash.end(), [&](const cluster::SessionSpec& s) {return s.id == id || s.name == id;});
        if (it == this->_trash.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session '" + id + "' in the trash");
        spec = *it;
        this->_trash.erase(it);
    }
    spec.deleted = 0;
    const std::shared_ptr<cluster::Session> session = this->make_(spec);
    session->loadHistory(300);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_sessions[spec.id] = session;
        this->_order.push_back(spec.id);
    }
    session->start();
    this->notice_(spec.id, spec.name + " restored");
    this->save();
    return spec.id;
}

_cold void cluster::Manager::purge(const std::string& id)
{
    std::vector<std::string> removed;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        const std::int64_t limit = cluster::now() - static_cast<std::int64_t>(this->_config.trashDays) * 86400;
        std::erase_if(this->_trash, [&](const cluster::SessionSpec& spec) {
            const bool gone = id == "*" || (id.empty() ? spec.deleted < limit : (spec.id == id || spec.name == id));
            if (gone) removed.push_back(spec.id);
            return gone;
        });
    }
    if (!id.empty() && id != "*" && removed.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session '" + id + "' in the trash");
    std::error_code error;
    for (const std::string& gone: removed) {
        std::filesystem::remove(log_file_(gone), error);
        for (const std::filesystem::directory_entry& file: std::filesystem::directory_iterator(log_file_(gone).parent_path() / "archive", error))
            if (file.path().stem().string().starts_with(gone + "-")) std::filesystem::remove(file.path(), error); // its old conversations
    }
    if (!removed.empty()) this->save();
}

_cold void cluster::Manager::rename(const std::string& id, const std::string& name)
{
    if (cluster::trim(name).empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "empty name");
    this->get_(id)->rename(cluster::trim(name));
    this->save();
}

_cold void cluster::Manager::setMode(const std::string& id, const std::string& mode)
{
    this->get_(id)->setMode(mode);
    this->save();
}

_cold void cluster::Manager::togglePanel(const std::string& id, const std::string& panel)
{
    this->get_(id)->togglePanel(panel);
    this->save();
}

_cold void cluster::Manager::cd(const std::string& id, const std::string& cwd)
{
    if (id == GLOBAL_ID) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the folder of the global session is set by global.cwd");
    const std::filesystem::path path = std::filesystem::weakly_canonical(cluster::expand_home(cwd));
    if (!std::filesystem::is_directory(path)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "not a folder: " + path.string());

    // A conversation belongs to its project: the session starts a new one in the new folder (same id / name / log)
    const std::shared_ptr<cluster::Session> old = this->get_(id);
    old->stop();
    cluster::SessionSpec spec = old->spec();
    spec.cwd = path.string();
    spec.claudeId.clear();
    const std::shared_ptr<cluster::Session> session = this->make_(spec);
    session->loadHistory(300);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_sessions[id] = session;
    }
    session->start();
    this->notice_(id, spec.name + " moved to " + cluster::short_path(spec.cwd));
    this->save();
}

/* global session */
_cold std::vector<std::string> cluster::Manager::globalArgs_(void) const
{
    std::vector<std::string> args = {"--mcp-config", cluster::mcp_config_path().string(), "--allowedTools", "mcp__claude-cluster"};
    const std::vector<std::string> prompt = prompt_args_(this->_config.globalPrompt);
    args.insert(args.end(), prompt.begin(), prompt.end());
    return args;
}

_cold void cluster::Manager::writeMcpConfig_(void) const
{
    cluster::Json config = {{"mcpServers", cluster::mcp_servers(this->_config.mcpServers)}}; // the custom ones ([mcp] servers)
    config["mcpServers"]["claude-cluster"] = {
        {"type", "stdio"}, {"command", cluster::self_exe().string()}, {"args", {"mcp"}},
        {"env", {{"CLAUDE_CLUSTER_SOCKET", cluster::socket_path().string()}}},
    };
    cluster::write_json(cluster::mcp_config_path(), config);
}

_cold void cluster::Manager::startGlobal(void)
{
    if (!this->_config.globalEnabled) return;
    this->writeMcpConfig_();
    cluster::SessionSpec spec;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_previous.begin(), this->_previous.end(), [](const cluster::SessionSpec& s) {return s.global;});
        if (it != this->_previous.end()) {
            spec = *it;
            this->_previous.erase(it);
        }
    }
    spec.id = GLOBAL_ID;
    spec.name = "global";
    spec.global = true;
    const std::string cwd = this->globalDir_().string();
    if (!spec.cwd.empty() && spec.cwd != cwd) spec.claudeId.clear(); // its conversation is kept by claude in the other folder
    spec.cwd = cwd;
    spec.backend = this->_config.globalBackend + (this->_config.globalModel.empty() ? "" : "/" + this->_config.globalModel);
    if (spec.created == 0) spec.created = cluster::now();
    if (spec.panels.empty()) spec.panels = {"tokens", "context", "cost", "usage"};
    spec.mode = !this->_config.globalMode.empty() ? this->_config.globalMode : spec.mode.empty() ? this->_config.mode : spec.mode;
    if (!this->_config.globalEffort.empty()) spec.effort = this->_config.globalEffort; // else its own (/effort), first: [agent]
    else if (spec.effort.empty()) spec.effort = this->_config.effort;
    if (this->_auth.resolve(spec.backend).provider.driver != "claude") {
        this->notice_(GLOBAL_ID, "global.backend must use the claude driver (MCP control): '" + spec.backend + "' replaced by claude", true);
        spec.backend = "claude";
        spec.claudeId.clear();
    }
    // a background claude of the old Remote Control (claude --bg) still has the conversation: stopped (its conversation
    // kept), Remote Control goes on in the session itself
    if (!spec.claudeId.empty() && this->_auth.resolve(spec.backend).provider.driver == "claude") {
        const std::string claude = this->_config.command("claude");
        const cluster::Captured agents = cluster::capture({claude, "agents", "--json"}, "", 15);
        const cluster::Json list = cluster::Json::parse(agents.out, nullptr, false);
        if (list.is_array()) {
            for (const cluster::Json& agent: list) {
                if (!agent.is_object() || agent.value("sessionId", std::string()) != spec.claudeId || agent.value("id", std::string()).empty()) continue;
                (void)cluster::capture({claude, "stop", agent.value("id", std::string())}, "", 30);
                this->setUi("remote", "true");
                this->notice_(GLOBAL_ID, "the background Remote Control session was stopped: Remote Control goes on in the global session itself");
            }
        }
    }
    try {
        if (this->_globalNew) { // "start new": its transcript archived, the new conversation shown alone
            this->_globalNew = false;
            this->archive_(GLOBAL_ID);
        }
        const std::shared_ptr<cluster::Session> session = this->make_(spec);
        session->loadHistory(200);
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_sessions[GLOBAL_ID] = session;
        }
        session->start();
    } catch (const utils::exception::IException& e) {
        this->notice_(GLOBAL_ID, std::string("global session: ") + e.what() + ": " + e.info(), true);
    }
}

_cold std::filesystem::path cluster::Manager::globalDir_(void) const
{
    if (!this->_config.globalCwd.empty()) return cluster::expand_home(this->_config.globalCwd);
    const std::filesystem::path dir = cluster::data_dir() / "global";
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    return dir;
}

namespace {

_cold void trust_folder_(const std::filesystem::path& folder)
{
    // claude --bg needs a trusted folder (the trust prompt of an interactive claude): claude-cluster accepts it for its
    // own folder in ~/.claude.json (written to a temporary file then renamed: never a half-written file)
    const std::filesystem::path file = cluster::expand_home("~/.claude.json");
    std::ifstream in(file);
    if (!in) return;
    cluster::Json data = cluster::Json::parse(in, nullptr, false);
    if (data.is_discarded() || !data.is_object()) return;
    cluster::Json& project = data["projects"][folder.string()];
    if (project.is_object() && project.value("hasTrustDialogAccepted", false)) return;
    project["hasTrustDialogAccepted"] = true;
    const std::filesystem::path temporary = file.string() + ".claude-cluster";
    {
        std::ofstream out(temporary);
        out << data.dump(2);
        if (!out) return;
    }
    std::error_code error;
    std::filesystem::rename(temporary, file, error);
}

} // namespace

_cold void cluster::Manager::setRemote(const bool on)
{
    // the global session itself on claude.ai (live both ways), on again at every start until it is turned off
    const std::shared_ptr<cluster::Session> session = this->get_(GLOBAL_ID);
    if (on && this->_config.globalCwd.empty()) trust_folder_(session->spec().cwd);
    session->remoteControl(on, this->_config.remoteName);
    this->setUi("remote", on ? "true" : "false");
    this->notice_(GLOBAL_ID, on ? "Remote Control: connecting (its claude.ai link in the global session)" : "Remote Control off");
    ++this->_version;
}

/* events */
_cold void cluster::Manager::event_(const cluster::Event& event)
{
    std::string name = event.session;
    try {
        name = this->get_(event.session)->spec().name;
    } catch (const utils::exception::IException&) {}

    if (this->_closing && event.kind == cluster::Event::Kind::Exited) return;
    switch (event.kind) {
        case cluster::Event::Kind::Init:
            // claude-cluster ready: its global session started (the first time of this run), and how long it took
            if (event.session == GLOBAL_ID && !this->_readyTold.exchange(true)) {
                const std::string took = std::to_string(cluster::now() - this->_startedAt) + " s";
                this->notice_(GLOBAL_ID, "claude-cluster ready (" + took + ")");
                std::thread([this, took]() {
                    (void)this->notify({"ready", "claude-cluster is ready", "its global session started (" + took + ")", cluster::Face::Happy, false, "service-login"}, "ready");
                }).detach();
            }
            break;
        case cluster::Event::Kind::Prompt:
            this->addHistory({event.text, {}, {}}, event.session);
            this->notice_(event.session, name + ": prompt from claude.ai");
            break;
        case cluster::Event::Kind::Permission:
            if (event.text.starts_with("AskUserQuestion: ")) { // a question of the agent: its popup in the interfaces
                this->notice_(event.session, name + " asks: " + event.text.substr(17));
                {
                    // its options as buttons (one question, one choice), else open it
                    cluster::Notification n{event.session, name + " · question", event.text.substr(17), cluster::Face::Attentive, true, "message-new-instant"};
                    const std::optional<cluster::Snapshot> asked = this->snapshot(event.session);
                    if (asked && !asked->permissions.empty()) {
                        const cluster::Json questions = asked->permissions.back().input.value("questions", cluster::Json::array());
                        n.requestId = asked->permissions.back().requestId;
                        if (questions.size() == 1 && !questions[0].value("multiSelect", false))
                            for (std::size_t i = 0; i < questions[0].value("options", cluster::Json::array()).size() && i < 3; ++i)
                                n.actions.emplace_back("opt" + std::to_string(i), cluster::one_line(questions[0]["options"][i].value("label", std::string()), 24));
                    }
                    n.actions.emplace_back("open", "Open");
                    (void)this->notify(n, "question");
                }
                const std::optional<cluster::Snapshot> snap = this->snapshot(event.session);
                if (this->_config.globalAnswers && event.session != GLOBAL_ID && snap && !snap->permissions.empty() && this->snapshot(GLOBAL_ID)) {
                    // the global session may choose the answer (the user may answer it first: its popup)
                    const cluster::Permission& p = snap->permissions.back();
                    try {
                        this->send(GLOBAL_ID, "[question from " + name + " (" + event.session + ")] " + p.input.value("questions", cluster::Json::array()).dump()
                            + "\nAnswer it with session_allow {session: \"" + event.session + "\", request_id: \"" + p.requestId + "\", answers: {question: label}} "
                            "when you know what the user wants; else leave it to the user (its popup is shown to them).");
                    } catch (const utils::exception::IException&) {}
                }
                break;
            }
            this->notice_(event.session, name + " waits for a permission: " + event.text);
            {
                // allowed / denied from the notification; the safety check of Claude Code: its own face
                const std::optional<cluster::Snapshot> asked = this->snapshot(event.session);
                const bool safety = asked && !asked->permissions.empty() && asked->permissions.back().safety;
                cluster::Notification n{event.session, name + (safety ? " · safety check" : " · permission"), event.text, safety ? cluster::Face::Waiting : cluster::Face::Attentive,
                    true, safety ? "dialog-warning" : "message-new-instant"};
                if (asked && !asked->permissions.empty()) n.requestId = asked->permissions.back().requestId;
                n.actions = safety ? std::vector<std::pair<std::string, std::string>>{{"allow", "Run it anyway"}, {"deny", "Leave it"}, {"open", "Open"}}
                    : std::vector<std::pair<std::string, std::string>>{{"allow", "Allow"}, {"deny", "Deny"}, {"open", "Open"}};
                (void)this->notify(n, safety ? "error" : "permission");
            }
            if (event.session != GLOBAL_ID && this->_config.bypassToGlobal) {
                // a sub-session in bypassPermissions: the global session decides (the user can still answer it)
                const std::optional<cluster::Snapshot> snap = this->snapshot(event.session);
                if (snap && snap->spec.mode == "bypassPermissions" && !snap->permissions.empty() && this->snapshot(GLOBAL_ID)) {
                    const cluster::Permission& p = snap->permissions.back();
                    try {
                        this->send(GLOBAL_ID, "[permission request from " + name + " (" + event.session + ")] " + p.tool + ": " + cluster::one_line(p.description, 400)
                            + (p.reason.empty() ? "" : "\nwhy it asks: " + cluster::one_line(p.reason, 400))
                            + "\nIts mode is bypassPermissions: you decide with session_allow {session: \"" + event.session + "\", request_id: \"" + p.requestId
                            + "\", behavior: allow | deny}. Ask the user first (AskUserQuestion) when it may destroy data or you are unsure.");
                    } catch (const utils::exception::IException&) {}
                }
            }
            break;
        case cluster::Event::Kind::TurnDone:
            this->recordUsage_(event.session);
            if (event.session != GLOBAL_ID) this->turnCounted_(event.session, event.error || event.interrupted);
            if (event.session == GLOBAL_ID && this->_restartAfterTurn.exchange(false)) {
                // it changed its own system prompt: started again on it, the same conversation, once its turn is over
                std::thread([this]() {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    try {
                        this->restartGlobal();
                        this->notice_(GLOBAL_ID, "global session started again on its new system prompt");
                    } catch (const utils::exception::IException& e) {
                        this->notice_(GLOBAL_ID, std::string("global session not started again: ") + e.info(), true);
                    }
                }).detach();
            }
            if (event.interrupted) {
                // stopped on purpose (Ctrl+C, a prompt sent over it...): no answer to speak or notify, not an error
                this->notice_(event.session, name + ": interrupted");
            } else if (event.session == GLOBAL_ID) {
                if (this->onGlobalAnswer && !event.error) this->onGlobalAnswer(event.text);
                (void)this->notify({GLOBAL_ID, event.error ? "global session · error" : "global session", event.text, event.error ? cluster::Face::Waiting : cluster::Face::Happy,
                    event.error, event.error ? "dialog-warning" : "message", {{"open", "Open"}}}, event.error ? "error" : "global");
            } else {
                // asked by the global session (session_send reply): its answer goes back to it, it wakes up on it
                bool reply = false;
                {
                    std::lock_guard<std::mutex> lock(this->_mutex);
                    auto it = this->_replies.find(event.session);
                    if (it != this->_replies.end() && it->second > 0) {
                        reply = true;
                        if (--it->second == 0) this->_replies.erase(it);
                    }
                }
                if (reply) {
                    const std::string text = std::string(event.error ? "[error from " : "[reply from ") + name + " (" + event.session + ")]\n"
                        + (event.text.size() > 6000 ? event.text.substr(0, 6000) + "\n[...]" : event.text);
                    std::thread([this, text]() {
                        try {
                            this->send(GLOBAL_ID, text, true);
                        } catch (const utils::exception::IException& e) {
                            this->notice_(GLOBAL_ID, std::string("reply not given to the global session: ") + e.info(), true);
                        }
                    }).detach();
                }
                this->notice_(event.session, name + (event.error ? " failed: " : " done: ") + cluster::one_line(event.text, 120), event.error);
                // a limit reached (the usage of the account, the budget): it sleeps until its reset; another error: o_o
                std::string lower = event.text;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});
                const bool limit = event.error && (lower.find("limit") != std::string::npos || lower.find("budget") != std::string::npos);
                std::string took;
                if (const std::optional<cluster::Snapshot> done = this->snapshot(event.session); done && !event.error)
                    took = " (" + cluster::human_tokens(done->metrics.last.total()) + " tokens)";
                (void)this->notify({event.session, name + (limit ? " · limit reached" : event.error ? " · error" : " · finished") + took, event.text,
                    limit ? cluster::Face::Idle : event.error ? cluster::Face::Waiting : cluster::Face::Grin, event.error && !limit,
                    limit ? "message" : event.error ? "dialog-warning" : "complete", {{"open", "Open"}}}, limit ? "limit" : event.error ? "error" : "done");
            }
            this->taskDone_(event.session, event.error, event.interrupted, event.text);
            break;
        case cluster::Event::Kind::Exited: {
            // the global session must stay: its process ended by itself (not a stop) -> started again on its conversation
            const std::int64_t now = cluster::now();
            if (event.session == GLOBAL_ID && now - this->_globalRestart > 60) {
                this->_globalRestart = now;
                this->notice_(GLOBAL_ID, "global session: its process ended (" + event.text + "), started again");
                std::thread([this]() {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    try {
                        this->get_(GLOBAL_ID)->restart();
                    } catch (const utils::exception::IException& e) {
                        this->notice_(GLOBAL_ID, std::string("global session not started again: ") + e.info(), true);
                    }
                    ++this->_version;
                }).detach();
                break;
            }
            this->notice_(event.session, name + ": " + event.text, true);
            (void)this->notify({event.session, name + " · stopped", event.text, cluster::Face::Waiting, true, "dialog-error", {{"open", "Open"}}}, "error");
            break;
        }
    }
    ++this->_version;
}

_cold void cluster::Manager::notice_(const std::string& session, const std::string& text, const bool error)
{
    cluster::Notice notice{cluster::now(), session, text, error};
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_notices.push_back(notice);
        if (this->_notices.size() > 200) this->_notices.pop_front();
    }
    ++this->_version;
    if (this->onNotice) this->onNotice(notice);
}

/* lifecycle */
_cold void cluster::Manager::load(void)
{
    const std::optional<cluster::Json> state = cluster::read_json(state_file_());
    if (!state) return;
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_next = state->value("next", 1);
    this->_nextTask = state->value("next_task", 1);
    this->_nextSchedule = state->value("next_schedule", 1);
    for (const cluster::Json& w: state->value("schedules", cluster::Json::array()))
        this->_schedules.push_back({w.value("id", std::string()), w.value("at", std::int64_t{0}), w.value("every", 0), w.value("action", std::string("prompt")),
            w.value("session", std::string("g")), w.value("text", std::string())});
    for (const cluster::Json& spec: state->value("sessions", cluster::Json::array()))
        this->_previous.push_back(spec_from_json_(spec));
    for (const cluster::Json& spec: state->value("trash", cluster::Json::array()))
        this->_trash.push_back(spec_from_json_(spec));
    for (const cluster::Json& t: state->value("tasks", cluster::Json::array())) {
        cluster::Task task;
        task.id = t.value("id", std::string());
        task.prompt = t.value("prompt", std::string());
        task.target = t.value("target", std::string());
        task.after = t.value("after", std::vector<std::string>());
        task.status = t.value("status", std::string("queued"));
        task.session = t.value("session", std::string());
        task.created = t.value("created", std::int64_t{0});
        task.group = t.value("group", std::string());
        task.label = t.value("label", std::string());
        task.retries = t.value("retries", 0);
        if (task.status == "running") task.status = "queued"; // interrupted by the end of the previous run
        this->_tasks.push_back(task);
    }
    this->_ui = state->value("ui", std::map<std::string, std::string>());
}

_cold void cluster::Manager::save(void) const
{
    cluster::Json state = {{"version", 1}};
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        state["next"] = this->_next;
        state["next_task"] = this->_nextTask;
        state["next_schedule"] = this->_nextSchedule;
        state["schedules"] = cluster::Json::array();
        for (const cluster::Schedule& w: this->_schedules)
            state["schedules"].push_back({{"id", w.id}, {"at", w.at}, {"every", w.every}, {"action", w.action}, {"session", w.session}, {"text", w.text}});
        state["sessions"] = cluster::Json::array();
        auto global = this->_sessions.find(GLOBAL_ID);
        if (global != this->_sessions.end()) state["sessions"].push_back(spec_to_json_(global->second->spec()));
        for (const std::string& id: this->_order)
            state["sessions"].push_back(spec_to_json_(this->_sessions.at(id)->spec()));
        for (const cluster::SessionSpec& spec: this->_previous) // not restored yet: kept for the next run
            state["sessions"].push_back(spec_to_json_(spec));
        state["trash"] = cluster::Json::array();
        for (const cluster::SessionSpec& spec: this->_trash)
            state["trash"].push_back(spec_to_json_(spec));
        state["tasks"] = cluster::Json::array();
        for (const cluster::Task& t: this->_tasks)
            state["tasks"].push_back({{"id", t.id}, {"prompt", t.prompt}, {"target", t.target}, {"after", t.after}, {"status", t.status}, {"session", t.session}, {"created", t.created},
                {"group", t.group}, {"label", t.label}, {"retries", t.retries}});
        state["ui"] = this->_ui;
    }
    try {
        cluster::write_json(state_file_(), state);
    } catch (const utils::exception::IException&) {} // the state is saved again at the next change
}

_cold void cluster::Manager::restorePrevious(const bool restore)
{
    std::vector<cluster::SessionSpec> previous;
    {
        // the sub-sessions only: the global one stays for startGlobal (called before or after)
        std::lock_guard<std::mutex> lock(this->_mutex);
        std::copy_if(this->_previous.begin(), this->_previous.end(), std::back_inserter(previous), [](const cluster::SessionSpec& s) {return !s.global;});
        std::erase_if(this->_previous, [](const cluster::SessionSpec& s) {return !s.global;});
    }
    for (cluster::SessionSpec& spec: previous) {
        if (!restore) {
            spec.deleted = cluster::now();
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_trash.push_back(spec);
            continue;
        }
        try {
            const std::shared_ptr<cluster::Session> session = this->make_(spec);
            session->loadHistory(300);
            {
                std::lock_guard<std::mutex> lock(this->_mutex);
                this->_sessions[spec.id] = session;
                this->_order.push_back(spec.id);
            }
            session->start();
        } catch (const utils::exception::IException& e) {
            this->notice_(spec.id, spec.name + " not restored: " + e.info(), true);
        }
    }
    if (!previous.empty()) this->notice_("", std::to_string(previous.size()) + (restore ? " session(s) restored" : " session(s) moved to the trash"));
    this->save();
}

_cold void cluster::Manager::shutdown(void)
{
    // closing: the agents that end now are not "stopped by themselves" (no notice, no restart)
    this->_closing = true;
    {
        std::lock_guard<std::mutex> lock(this->_describer->mutex); // a description being written: dropped
        this->_describer->alive = false;
    }
    std::vector<std::shared_ptr<cluster::Session>> sessions;
    this->save();
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const auto &[id, session]: this->_sessions)
            sessions.push_back(session);
    }
    for (const std::shared_ptr<cluster::Session>& session: sessions)
        session->stop();
}

_cold void cluster::Manager::tick(void)
{
    static int seconds = 0;
    if (this->_config.reload()) {
        if (this->_config.error().empty()) this->notice_("", "config reloaded");
        else this->notice_("", "config error (previous values kept): " + this->_config.error(), true);
    }
    this->schedule_();
    this->groupsTick_();
    this->fireSchedules_();
    if (++seconds % 300 == 1) this->purge();
    if (seconds == 5) this->describeMissing_();
}

std::vector<cluster::Snapshot> cluster::Manager::list(const bool withGlobal) const
{
    std::vector<std::shared_ptr<cluster::Session>> sessions;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto global = this->_sessions.find(GLOBAL_ID);
        if (withGlobal && global != this->_sessions.end()) sessions.push_back(global->second);
        for (const std::string& id: this->_order)
            sessions.push_back(this->_sessions.at(id));
    }
    std::vector<cluster::Snapshot> snapshots;
    for (const std::shared_ptr<cluster::Session>& session: sessions) {
        snapshots.push_back(session->snapshot());
    }
    return snapshots;
}

std::optional<cluster::Snapshot> cluster::Manager::snapshot(const std::string& id) const
{
    try {
        return this->get_(id)->snapshot();
    } catch (const utils::exception::IException&) {
        return std::nullopt;
    }
}

std::vector<cluster::SessionSpec> cluster::Manager::trash(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_trash;
}

_cold void cluster::Manager::scanMachine(void)
{
    // the home folder takes a few seconds: in the background, a notice when written
    this->setUi("machine", "yes");
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_scan && this->_scanEnd == 0) return; // one at a time
        const std::uint64_t id = this->_scan ? this->_scan->id + 1 : 1;
        this->_scan = cluster::MachineProgress{};
        this->_scan->id = id;
        this->_scanEnd = 0;
    }
    std::thread([this]() {
        const std::function<void(const cluster::MachineProgress&)> report = [this](const cluster::MachineProgress& progress) {
            {
                std::lock_guard<std::mutex> lock(this->_mutex);
                const std::uint64_t id = this->_scan ? this->_scan->id : 1;
                this->_scan = progress;
                this->_scan->id = id;
                if (progress.done) this->_scanEnd = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            }
            ++this->_version;
        };
        try {
            const std::size_t repos = cluster::machine_scan(this->_config, report);
            this->notice_(GLOBAL_ID, "map of this computer written: " + std::to_string(repos) + " repositories (" + cluster::short_path(cluster::machine_file().string()) + ")");
            (void)this->notify({"map", "the map of this computer is written", std::to_string(repos) + " repositories: the global session knows them",
                cluster::Face::Happy, false, "complete"}, "ready");
        } catch (const std::exception& e) {
            this->notice_(GLOBAL_ID, std::string("map of this computer: ") + e.what(), true);
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (this->_scan) this->_scan->done = true;
            this->_scanEnd = 1;
        }
        ++this->_version;
    }).detach();
}

std::optional<std::pair<cluster::MachineProgress, std::int64_t>> cluster::Manager::machineProgress(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_scan) return std::nullopt;
    return std::make_pair(*this->_scan, this->_scanEnd);
}

int cluster::Manager::machineAsk(void) const
{
    const int days = cluster::machine_days_old();
    if (days < 0) return this->ui("machine", "") != "no" ? 1 : 0;
    if (this->_config.machineDays <= 0 || days < this->_config.machineDays) return 0;
    const std::string later = this->ui("machine_later", "0");
    return cluster::now() - std::stoll(later.empty() ? "0" : later) > static_cast<std::int64_t>(this->_config.machineDays) * 86400 ? 2 : 0;
}

_cold void cluster::Manager::restartGlobal(void)
{
    const std::shared_ptr<cluster::Session> old = this->get_(GLOBAL_ID);
    old->stop();
    const std::shared_ptr<cluster::Session> session = this->make_(old->spec());
    session->loadHistory(200);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_sessions[GLOBAL_ID] = session;
    }
    session->start();
    this->notice_(GLOBAL_ID, "global session started again: its system prompt read again (same conversation)");
}

bool cluster::Manager::notifyDesktop(const std::string& title, const std::string& text, const bool urgent)
{
    return this->notify({"", title, text, urgent ? cluster::Face::Attentive : cluster::Face::Happy, urgent, urgent ? "message-new-instant" : "message"});
}

_cold static std::string markup_(const std::string& text)
{
    // the body of a notification: its markup characters escaped
    std::string out;
    for (const char c: text)
        out += c == '&' ? std::string("&amp;") : c == '<' ? std::string("&lt;") : c == '>' ? std::string("&gt;") : std::string(1, c);
    return out;
}

bool cluster::Manager::notify(const cluster::Notification& n, const std::string& kind)
{
    // failsafe: no notify-send, no desktop... nothing said; its buttons answered in a thread of its own (notify-send waits)
    if (n.face == cluster::Face::Grin || n.face == cluster::Face::Happy) this->_moment = {n.face, cluster::now()};
    if (!this->_config.notify) return false;
    const cluster::Config& c = this->_config;
    const std::map<std::string, bool> kinds = {{"global", c.notifyGlobal}, {"done", c.notifyDone}, {"permission", c.notifyPermission}, {"question", c.notifyQuestion},
        {"error", c.notifyError}, {"limit", c.notifyLimits}, {"web", c.notifyWeb}, {"task", c.notifyTasks}, {"reminder", c.notifyReminders},
        {"ready", c.notifyReady}};
    if (kinds.contains(kind) && !kinds.at(kind)) return false;
    // the last prompt of this session came from a web device: its page (or its app) notifies, not the desktop here
    if (kind != "web" && kind != "ready" && !n.session.empty() && this->origin(n.session).starts_with("web:")) return false;
    if (c.notifyWhen != "always" && this->_focused == 1) return false; // in front of the user: its popups there
    const std::string key = n.session.empty() ? "-" : n.session;
    std::vector<std::string> args = {"notify-send", "-a", "claude-cluster", "-i", cluster::face_file(n.face).string(), "-h", "string:desktop-entry:claude-cluster",
        "-u", n.urgent ? "critical" : "normal", "-p"};
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_notifyIds.contains(key)) args.insert(args.end(), {"-r", this->_notifyIds.at(key)});
    }
    args.insert(args.end(), {"-A", "default=Open"}); // its body clicked: its session, the interface to the front
    for (const auto &[action, label]: n.actions)
        args.insert(args.end(), {"-A", action + "=" + label});
    args.push_back(n.title);
    args.push_back(markup_(cluster::one_line(n.body, 400)));
    if (this->_config.notifySound && !n.sound.empty()) {
        if (cluster::has_command("canberra-gtk-play")) {
            cluster::detach({"canberra-gtk-play", "-i", n.sound});
        } else {
            const std::string file = "/usr/share/sounds/freedesktop/stereo/" + n.sound + ".oga";
            if (std::filesystem::exists(file)) cluster::detach({cluster::has_command("pw-play") ? "pw-play" : "paplay", file});
        }
    }
    std::thread([this, args, n, key]() {
        const cluster::Captured out = cluster::capture(args, "", 3600); // its buttons (and its body): waited for
        std::string chosen;
        for (const std::string& raw: cluster::split(out.out, '\n')) {
            const std::string line = cluster::trim(raw);
            if (!line.empty() && std::all_of(line.begin(), line.end(), [](const char c) {return c >= '0' && c <= '9';})) {
                std::lock_guard<std::mutex> lock(this->_mutex);
                this->_notifyIds[key] = line;
            } else if (line == "default" || std::any_of(n.actions.begin(), n.actions.end(), [&line](const std::pair<std::string, std::string>& a) {return a.first == line;})) {
                chosen = line;
            }
        }
        if (chosen.empty()) return;
        // its button: its owner (a device to approve...), else the session shown, its permission or question answered
        try {
            if (n.onAction && chosen != "default") {
                n.onAction(chosen);
                ++this->_version;
                return;
            }
            if (!n.session.empty() && n.session != "ready" && n.session != "map" && n.session != "voice" && this->snapshot(n.session)) this->requestFocus(n.session);
            if (chosen == "default" || chosen == "open") this->requestRaise();
            if (chosen == "allow" || chosen == "deny" || chosen == "always") {
                this->answer(n.session, n.requestId, chosen);
            } else if (chosen.starts_with("opt")) {
                const std::optional<cluster::Ask> ask = this->asking().current();
                if (ask && ask->session == n.session) {
                    this->asking().choose(std::stoi(chosen.substr(3)));
                    (void)this->asking().next();
                }
            }
        } catch (const std::exception& e) {
            this->notice_(n.session.empty() ? GLOBAL_ID : n.session, std::string("notification: ") + e.what(), true);
        }
        ++this->_version;
    }).detach();
    return true;
}

cluster::AppFace cluster::Manager::appFace(const bool listening) const
{
    // what needs the user first: an error or the safety check, a question / permission, a turn just finished, the voice
    // listening, something running, nothing
    std::vector<std::shared_ptr<cluster::Session>> sessions;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const auto &[id, session]: this->_sessions)
            sessions.push_back(session);
    }
    int waiting = 0;
    int working = 0;
    bool alarm = false;
    for (const std::shared_ptr<cluster::Session>& session: sessions) {
        const auto [state, permissions, safety] = session->brief();
        alarm |= state == cluster::State::Error || safety;
        waiting += permissions > 0 ? 1 : 0;
        working += state == cluster::State::Working ? 1 : 0;
    }
    cluster::AppFace face;
    const std::pair<cluster::Face, std::int64_t> moment = this->_moment;
    if (alarm) face.face = cluster::Face::Waiting;
    else if (waiting > 0) face.face = cluster::Face::Attentive;
    else if (cluster::now() - moment.second < 20) face.face = moment.first;
    else if (listening) face.face = cluster::Face::Attentive;
    else if (working > 0) face.face = cluster::Face::Squint;
    else face.face = cluster::Face::Idle;
    face.urgent = waiting > 0 || alarm;
    face.badge = waiting > 0 ? waiting : working;
    return face;
}

void cluster::Manager::archive_(const std::string& id)
{
    // the transcript of a conversation that ends: out of the display, kept for the search (purged with the session)
    const std::filesystem::path log = log_file_(id);
    std::error_code error;
    if (std::filesystem::file_size(log, error) == 0 || error) return;
    const std::filesystem::path archive = log.parent_path() / "archive";
    std::filesystem::create_directories(archive, error);
    std::filesystem::rename(log, archive / (id + "-" + std::to_string(cluster::now()) + ".jsonl"), error);
    if (error) this->notice_(id, "transcript not archived: " + error.message(), true);
}

void cluster::Manager::replyTo(const std::string& id)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    ++this->_replies[id];
}

void cluster::Manager::requestFocus(const std::string& id)
{
    const std::string resolved = this->resolve(id);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_focus = resolved;
    }
    ++this->_focusVersion;
    ++this->_version;
}

_cold cluster::Queued cluster::Manager::rewind(const std::string& id)
{
    const cluster::Queued prompt = this->get_(id)->rewind();
    this->save(); // the cut is kept over a restart (until the next prompt)
    ++this->_version;
    return prompt;
}

bool cluster::Manager::restorable(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return std::any_of(this->_previous.begin(), this->_previous.end(), [](const cluster::SessionSpec& s) {return !s.global || !s.claudeId.empty();});
}

_cold void cluster::Manager::answerRestore(const bool restore)
{
    // "start clean": the sessions to the trash, the global session on a new conversation (its transcript archived)
    this->restorePrevious(restore);
    if (!restore) {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (cluster::SessionSpec& spec: this->_previous) {
            if (!spec.global) continue;
            spec.claudeId.clear();
            spec.leaf.clear();
            spec.resumeAt.clear();
            spec.cost = 0.0;
            spec.tokens = 0;
        }
        this->_globalNew = true;
    }
    this->startGlobal();
}

std::vector<cluster::SessionSpec> cluster::Manager::previous(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    std::vector<cluster::SessionSpec> list;
    std::copy_if(this->_previous.begin(), this->_previous.end(), std::back_inserter(list), [](const cluster::SessionSpec& s) {return !s.global;});
    return list;
}

std::vector<cluster::Notice> cluster::Manager::notices(const std::size_t max) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    const std::size_t start = this->_notices.size() > max ? this->_notices.size() - max : 0;
    return std::vector<cluster::Notice>(this->_notices.begin() + static_cast<long>(start), this->_notices.end());
}

std::string cluster::Manager::resolve(const std::string& idOrName) const
{
    std::vector<std::pair<std::string, std::string>> sessions; // <id, name>
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const auto &[id, session]: this->_sessions)
            sessions.emplace_back(id, session->spec().name);
    }
    const std::string wanted = cluster::lower(cluster::trim(idOrName));
    if (wanted == "global") return GLOBAL_ID;
    for (const auto &[id, name]: sessions)
        if (id == wanted || cluster::lower(name) == wanted) return id;
    std::vector<std::string> matches;
    for (const auto &[id, name]: sessions)
        if (cluster::lower(name).starts_with(wanted)) matches.push_back(id);
    if (matches.size() == 1) return matches[0];
    throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId,
        matches.empty() ? "no session '" + idOrName + "'" : "'" + idOrName + "' matches several sessions");
}

std::uint64_t cluster::Manager::version(void) const
{
    std::uint64_t version = this->_version;
    std::lock_guard<std::mutex> lock(this->_mutex);
    for (const auto &[id, session]: this->_sessions)
        version += session->version();
    return version;
}

std::string cluster::Manager::ui(const std::string& key, const std::string& fallback) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    auto it = this->_ui.find(key);
    return it != this->_ui.end() ? it->second : fallback;
}

void cluster::Manager::setUi(const std::string& key, const std::string& value)
{
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_ui[key] = value;
    }
    ++this->_version;
    this->save();
}
