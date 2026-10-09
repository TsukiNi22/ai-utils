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
#include "cluster/Tools.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

/* tools */
_cold static cluster::Json spec_to_json_(const cluster::SessionSpec& spec)
{
    return {
        {"id", spec.id}, {"name", spec.name}, {"cwd", spec.cwd}, {"backend", spec.backend}, {"claude_id", spec.claudeId},
        {"model", spec.model}, {"mode", spec.mode}, {"profile", spec.profile}, {"extra_args", spec.extraArgs},
        {"panels", spec.panels}, {"global", spec.global}, {"created", spec.created}, {"deleted", spec.deleted}, {"cost", spec.cost}, {"tokens", spec.tokens},
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
    spec.model = json.value("model", std::string());
    spec.mode = json.value("mode", std::string("default"));
    spec.profile = json.value("profile", std::string());
    spec.extraArgs = json.value("extra_args", std::vector<std::string>());
    spec.panels = json.value("panels", std::set<std::string>());
    spec.global = json.value("global", false);
    spec.created = json.value("created", std::int64_t{0});
    spec.deleted = json.value("deleted", std::int64_t{0});
    spec.cost = json.value("cost", 0.0);
    spec.tokens = json.value("tokens", std::int64_t{0});
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

/* constructor */
_cold cluster::Manager::Manager(cluster::Config& config)
    : _config(config), _auth(config)
{
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
    if (!cluster::has_command(launch.command))
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument,
            "`" + launch.command + "` is not installed (backend " + spec.backend + ")");
    if (launch.backend.provider.driver == "claude") launch.extraArgs = this->_config.extraArgs;
    if (spec.global) {
        const std::vector<std::string> global = this->globalArgs_();
        launch.extraArgs.insert(launch.extraArgs.end(), global.begin(), global.end());
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
    spec.profile = req.profile;
    spec.panels = std::set<std::string>(this->_config.panels.begin(), this->_config.panels.end());
    spec.created = cluster::now();
    if (!req.profile.empty()) spec.extraArgs = this->_config.profiles.at(req.profile).extraArgs;

    const std::shared_ptr<cluster::Session> session = this->make_(spec);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_sessions[spec.id] = session;
        this->_order.push_back(spec.id);
    }
    session->start();
    if (!req.prompt.empty()) session->send(req.prompt);
    this->notice_(spec.id, "new session " + spec.name + " (" + spec.backend + ") in " + cluster::short_path(spec.cwd));
    this->save();
    return spec.id;
}

_cold void cluster::Manager::send(const std::string& id, const std::string& text, const bool force)
{
    const std::shared_ptr<cluster::Session> session = this->get_(id);
    const cluster::Snapshot snap = session->snapshot();

    if (snap.state == cluster::State::Remote)
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the global session is in Remote Control (toggle it off first)");
    if (!force && this->_config.budget > 0 && snap.metrics.costKnown && snap.metrics.cost >= this->_config.budget) {
        this->notice_(id, "budget reached (" + cluster::human_cost(snap.metrics.cost) + "): prompt refused", true);
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction,
            "budget of " + cluster::human_cost(this->_config.budget) + " reached for " + snap.spec.name + " (force it, or raise sessions.budget_usd)");
    }
    session->send(text);
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
            const bool gone = id.empty() ? spec.deleted < limit : (spec.id == id || spec.name == id);
            if (gone) removed.push_back(spec.id);
            return gone;
        });
    }
    if (!id.empty() && removed.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session '" + id + "' in the trash");
    std::error_code error;
    for (const std::string& gone: removed)
        std::filesystem::remove(log_file_(gone), error);
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
    std::ifstream file(cluster::expand_home(this->_config.globalPrompt));
    std::stringstream prompt;
    prompt << file.rdbuf();
    std::vector<std::string> args = {"--mcp-config", (cluster::runtime_dir() / "mcp.json").string(), "--allowedTools", "mcp__claude-cluster"};
    if (!prompt.str().empty()) args.insert(args.end(), {"--append-system-prompt", prompt.str()});
    return args;
}

_cold void cluster::Manager::writeMcpConfig_(void) const
{
    const cluster::Json config = {{"mcpServers", {{"claude-cluster", {
        {"type", "stdio"}, {"command", cluster::self_exe().string()}, {"args", {"mcp"}},
        {"env", {{"CLAUDE_CLUSTER_SOCKET", cluster::socket_path().string()}}},
    }}}}};
    cluster::write_json(cluster::runtime_dir() / "mcp.json", config);
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
    spec.cwd = cluster::expand_home(this->_config.globalCwd).string();
    spec.backend = this->_config.globalBackend + (this->_config.globalModel.empty() ? "" : "/" + this->_config.globalModel);
    if (spec.created == 0) spec.created = cluster::now();
    if (spec.panels.empty()) spec.panels = {"tokens", "context", "cost"};
    if (this->_auth.resolve(spec.backend).provider.driver != "claude") {
        this->notice_(GLOBAL_ID, "global.backend must use the claude driver (MCP control): '" + spec.backend + "' replaced by claude", true);
        spec.backend = "claude";
        spec.claudeId.clear();
    }
    try {
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

_cold void cluster::Manager::setRemote(const bool on)
{
    const std::shared_ptr<cluster::Session> session = this->get_(GLOBAL_ID);
    const cluster::SessionSpec spec = session->spec();
    const cluster::Backend backend = this->_auth.resolve(spec.backend);
    const std::string command = this->_config.command("claude");

    if (on) {
        if (!this->_remoteId.empty()) return;
        // Remote Control needs an interactive session: the conversation goes on in a background claude (claude --bg)
        session->stop();
        std::vector<std::string> args = {command, "--bg", "--remote-control", this->_config.remoteName};
        if (!spec.claudeId.empty()) args.insert(args.end(), {"--resume", spec.claudeId});
        const std::vector<std::string> global = this->globalArgs_();
        args.insert(args.end(), global.begin(), global.end());
        std::vector<std::string> envArgs;
        for (const auto &[name, value]: this->_auth.env(backend))
            envArgs.push_back(name + "=" + value);
        envArgs.insert(envArgs.end(), args.begin(), args.end()); // run by capture() through env: VAR=value then the command
        const cluster::Captured started = cluster::capture(envArgs, spec.cwd, 60);
        if (started.code != 0) {
            session->start();
            throw utils::exception::ErrorException(utils::exception::InternalCode::Process, "claude --bg failed: " + cluster::one_line(started.out, 300));
        }
        // The short id is the last word of the output ("... <id>")
        const std::vector<std::string> words = cluster::split(cluster::trim(started.out), ' ');
        this->_remoteId = words.empty() ? "" : words.back();
        this->notice_(GLOBAL_ID, "Remote Control on (" + this->_config.remoteName + ", background session " + this->_remoteId + ")");
    } else {
        if (this->_remoteId.empty()) return;
        (void)cluster::capture({command, "stop", this->_remoteId}, spec.cwd, 30);
        this->_remoteId.clear();
        session->start();
        this->notice_(GLOBAL_ID, "Remote Control off: the global session is back here");
    }
    ++this->_version;
}

/* events */
_cold void cluster::Manager::event_(const cluster::Event& event)
{
    std::string name = event.session;
    try {
        name = this->get_(event.session)->spec().name;
    } catch (const utils::exception::IException&) {}

    switch (event.kind) {
        case cluster::Event::Kind::Init:
            break;
        case cluster::Event::Kind::Permission:
            this->notice_(event.session, name + " waits for a permission: " + event.text);
            if (this->_config.notify) cluster::detach({"notify-send", "-a", "claude-cluster", "-u", "critical", name + ": permission", event.text});
            break;
        case cluster::Event::Kind::TurnDone:
            if (event.session == GLOBAL_ID) {
                if (this->onGlobalAnswer && !event.error) this->onGlobalAnswer(event.text);
            } else {
                this->notice_(event.session, name + (event.error ? " failed: " : " done: ") + cluster::one_line(event.text, 120), event.error);
                if (this->_config.notify) cluster::detach({"notify-send", "-a", "claude-cluster", name + (event.error ? ": error" : ": done"), cluster::one_line(event.text, 200)});
            }
            this->taskDone_(event.session, event.error);
            break;
        case cluster::Event::Kind::Exited:
            this->notice_(event.session, name + ": " + event.text, true);
            if (this->_config.notify) cluster::detach({"notify-send", "-a", "claude-cluster", "-u", "critical", name + ": stopped", event.text});
            break;
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
    for (const cluster::Json& spec: state->value("sessions", cluster::Json::array()))
        this->_previous.push_back(spec_from_json_(spec));
    for (const cluster::Json& spec: state->value("trash", cluster::Json::array()))
        this->_trash.push_back(spec_from_json_(spec));
    for (const cluster::Json& t: state->value("tasks", cluster::Json::array())) {
        cluster::Task task{t.value("id", std::string()), t.value("prompt", std::string()), t.value("target", std::string()),
            t.value("after", std::vector<std::string>()), t.value("status", std::string("queued")), t.value("session", std::string()), t.value("created", std::int64_t{0})};
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
            state["tasks"].push_back({{"id", t.id}, {"prompt", t.prompt}, {"target", t.target}, {"after", t.after}, {"status", t.status}, {"session", t.session}, {"created", t.created}});
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
    if (++seconds % 300 == 1) this->purge();
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
        if (snapshots.back().spec.global && !this->_remoteId.empty()) {
            snapshots.back().state = cluster::State::Remote;
            snapshots.back().remoteId = this->_remoteId;
        }
    }
    return snapshots;
}

std::optional<cluster::Snapshot> cluster::Manager::snapshot(const std::string& id) const
{
    try {
        cluster::Snapshot snap = this->get_(id)->snapshot();
        if (snap.spec.global && !this->_remoteId.empty()) {
            snap.state = cluster::State::Remote;
            snap.remoteId = this->_remoteId;
        }
        return snap;
    } catch (const utils::exception::IException&) {
        return std::nullopt;
    }
}

std::vector<cluster::SessionSpec> cluster::Manager::trash(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_trash;
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
