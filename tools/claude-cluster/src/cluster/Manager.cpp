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
#include "cluster/Orchestrator.hpp"
#include "cluster/Attachments.hpp"
#include "cluster/Templates.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Plugins.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Showcase.hpp"
#include "cluster/Stats.hpp"

#include "cluster/Machine.hpp"
#include "cluster/Models.hpp"
#include "cluster/Method.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Wire.hpp"
#include "cluster/Mcp.hpp"
#include <filesystem>
#include <algorithm>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <regex>
#include <cmath>

/* tools */
_cold static cluster::Json spec_to_json_(const cluster::SessionSpec& spec)
{
    return {
        {"id", spec.id}, {"name", spec.name}, {"cwd", spec.cwd}, {"backend", spec.backend}, {"claude_id", spec.claudeId}, {"leaf", spec.leaf}, {"resume_at", spec.resumeAt}, {"fork", spec.fork},
        {"model", spec.model}, {"mode", spec.mode}, {"effort", spec.effort}, {"profile", spec.profile}, {"extra_args", spec.extraArgs},
        {"panels", spec.panels}, {"global", spec.global}, {"created", spec.created}, {"deleted", spec.deleted}, {"cost", std::isfinite(spec.cost) ? spec.cost : 0.0}, {"tokens", spec.tokens},
        {"summary", spec.summary}, {"summary_auto", spec.summaryAuto}, {"summary_turns", spec.summaryTurns}, {"category", spec.category}, {"tags", spec.tags}, {"turns", spec.turns},
        {"pin", spec.pin}, {"context", spec.context}, {"cached_at", spec.cachedAt}, {"used_at", spec.usedAt},
        {"keep_warm", spec.keepWarm}, // chosen (/keepwarm): kept over a restart or an update
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
    spec.cost = json.contains("cost") && json["cost"].is_number() ? json["cost"].get<double>() : 0.0; // an infinite one was written as null: never a reason to lose the session
    spec.tokens = json.value("tokens", std::int64_t{0});
    spec.summary = json.value("summary", std::string());
    spec.summaryAuto = json.value("summary_auto", true);
    spec.summaryTurns = json.value("summary_turns", std::int64_t{0});
    spec.category = json.value("category", std::string());
    spec.tags = json.value("tags", std::vector<std::string>());
    spec.turns = json.value("turns", std::int64_t{0});
    spec.pin = json.value("pin", cluster::Json());
    spec.context = json.value("context", std::string());
    spec.cachedAt = json.value("cached_at", std::int64_t{0});
    spec.usedAt = std::max(json.value("used_at", std::int64_t{0}), json.value("rewound_at", std::int64_t{0}));
    spec.keepWarm = json.value("keep_warm", false);
    return spec;
}

_cold static std::filesystem::path state_file_(void)
{
    return cluster::data_dir() / "state.json";
}

_cold static std::string why_(const std::exception& e)
{
    const utils::exception::IException* detail = dynamic_cast<const utils::exception::IException*>(&e);
    return detail != nullptr ? detail->info() : e.what();
}

_cold static void next_past_(int& next, const std::string& id, const char kind)
{
    if (id.size() < 2 || id.size() > 9 || id[0] != kind || !std::all_of(id.begin() + 1, id.end(), [](const unsigned char c) {return std::isdigit(c) != 0;})) return;
    next = std::max(next, std::stoi(id.substr(1)) + 1);
}

_cold static std::filesystem::path log_file_(const std::string& id)
{
    return cluster::data_dir() / "logs" / (id + ".jsonl");
}

_cold static std::string plain_(const std::string& markdown)
{
    // an answer in one line for a notice: its Markdown marks out (fences, table rules and pipes, headings, bold, code)
    std::string out;
    for (const std::string& line: cluster::split(markdown, '\n')) {
        std::string t = cluster::trim(line);
        if (t.starts_with("```") || (t.starts_with("|") && t.find_first_not_of("|-: ") == std::string::npos)) continue;
        t = std::regex_replace(t, std::regex(R"(^#+\s*|\*\*|__|`)"), "");
        t = std::regex_replace(t, std::regex(R"(\s*\|\s*)"), " ");
        if (!cluster::trim(t).empty()) out += (out.empty() ? "" : " ") + cluster::trim(t);
    }
    return out;
}

_cold static std::string orchestration_text_(const std::string& mode)
{
    // the mode of orchestration of the user ([agent] orchestration), told to the global session
    if (mode == "compact")
        return "## Orchestration mode: compact\n"
                "The user wants the fewest tokens and the fewest sessions. Put several tasks of the same project in ONE session (one session per "
                "project, reusing the one that already has the context; `task_add` to it). Never spawn a sub-session for a medium task, and never a sub-global. "
                "When splitting would clearly help (very large or independent jobs), ask the user with an `AskUserQuestion` popup first and do not do it yourself.\n";
    if (mode == "ultra")
        return "## Orchestration mode: ultra\n"
                "The user wants maximum parallelism: use a separate sub-session for each different task, and sub-globals to coordinate big pieces of work, "
                "without asking, whenever it makes the work faster or the contexts cleaner.\n";
    return "## Orchestration mode: hybrid\n"
            "Balance tokens and speed: reuse a session for follow-ups and small or medium tasks of the same project; spawn several sub-sessions only when the jobs are "
            "independent and large enough to pay for a new context; use a sub-global rarely (a big multi-session job), and tell the user when you do.\n";
}

_cold static std::string workflow_text_(const bool allowed)
{
    // [agent] workflow: the multi-agent workflows of the model itself, told to every session
    if (allowed)
        return "## Workflows of the model: asked\n"
                "The user allows and asks your own multi-agent workflows when you have them (Claude Code: the Workflow tool, ultracode): this setting is their opt-in. On a big task "
                "that gains from them (many files, independent parts, several passes), use them, without asking; a small task stays a small task.\n";
    return "## Workflows of the model: not allowed\n"
            "The user does not allow your own multi-agent workflows (Claude Code: the Workflow tool, ultracode): never start one, even for a big task. Use your usual tools; "
            "they turn it on in Settings > Agent > Workflows of the model.\n";
}

_cold static std::vector<std::string> prompt_args_(const std::string& file, const bool global, const std::string& method, const std::string& orchestration, const bool workflow)
{
    // a system prompt file added to the one of claude (CLAUDE.md kept), then how the answers are drawn (always, from
    // the binary: the files of the user are copies never refreshed); the display blocks for the global session only
    std::ifstream in(cluster::expand_home(file));
    std::stringstream prompt;
    prompt << in.rdbuf();
    std::string rendering(cluster::templates::RENDERING);
    const std::size_t mark = rendering.find("<!-- global -->");
    if (mark != std::string::npos) rendering = global ? rendering.erase(mark, 15) : rendering.substr(0, mark);
    if (global) rendering += std::string("\n") + std::string(cluster::templates::ROUTING) + "\n" + orchestration_text_(orchestration); // how to choose the model and the thinking of the sub-sessions, how readily to split
    rendering += std::string("\n") + workflow_text_(workflow);
    if (!method.empty()) rendering += std::string("\n") + method; // the work method of the user (short)
    const std::string text = cluster::trim(prompt.str()).empty() ? rendering : prompt.str() + "\n" + rendering;
    return {"--append-system-prompt", text};
}

/* constructor */
_cold cluster::Manager::Manager(cluster::Config& config)
    : _config(config), _auth(config), _asking(std::make_unique<cluster::Asking>(*this)), _transcripts(config.usage.transcripts, cluster::data_dir() / "usage-scan.json")
{
    this->loadUsage_();
}

_cold cluster::Manager::~Manager()
{
    this->shutdown();
    for (int i = 0; i < 100 && (this->_accountsBusy || this->_pluginsBusy); ++i) // the readings of the providers / the plugins end first
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (this->_bgThread.joinable()) this->_bgThread.join(); // the checks of the tasks / schedules end first
    this->_usageStop = true; // the threads of the usage (a scan, a read of the account) end before anything of this manager goes
    if (this->_scanThread.joinable()) this->_scanThread.join();
    if (this->_accountThread.joinable()) this->_accountThread.join();
    // gdbus monitor stopped: a closed pipe would only stop it at its next line, and a signal of the daemon may never come (one process left behind per core);
    // its read ends, its thread with it, before anything it uses goes
    this->_notifyMonitorStop = true;
    if (const pid_t monitor = this->_notifyMonitorPid.load(); monitor > 0) (void)::kill(monitor, SIGKILL);
    if (this->_notifyMonitor.joinable()) this->_notifyMonitor.join();
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
    const cluster::Config& config = this->config();
    cluster::Launch launch;
    launch.backend = this->_auth.resolve(spec.backend);
    launch.env = this->_auth.env(launch.backend);
    for (const auto &[name, value]: this->jobEnv_(spec.id)) // cluster-run: its session, the socket, its folder first in the PATH
        launch.env.push_back({name, value});
    launch.command = config.command(launch.backend.provider.driver);
    launch.allowBypass = config.allowBypass;
    if (!cluster::has_command(launch.command))
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument,
            "`" + launch.command + "` is not installed (backend " + spec.backend + ")");
    if (launch.backend.provider.driver == "claude") {
        launch.extraArgs = config.extraArgs;
        if (!config.workflow) launch.extraArgs.insert(launch.extraArgs.end(), {"--disallowedTools", "Workflow"}); // [agent] workflow off: its multi-agent workflows taken away from it
        // its context at most (compacted past it): the one asked at its start, else the setting of its kind; auto: the window of the model
        const std::string window = cluster::Config::contextWindow(spec.context).value_or("auto") != "auto" ? *cluster::Config::contextWindow(spec.context)
            : spec.global ? config.globalContext : config.sessionsContext;
        if (window != "auto") launch.env.push_back({"CLAUDE_CODE_AUTO_COMPACT_WINDOW", window});
        if (!spec.global) {
            const std::vector<std::string> prompt = prompt_args_(config.sessionsPrompt, false, cluster::method_text(config, false), config.orchestration, config.workflow);
            launch.extraArgs.insert(launch.extraArgs.end(), prompt.begin(), prompt.end());
            const std::vector<std::string> mcp = this->subArgs_(spec.id);
            launch.extraArgs.insert(launch.extraArgs.end(), mcp.begin(), mcp.end());
        }
    }
    if (spec.global) {
        const std::vector<std::string> global = this->globalArgs_();
        launch.extraArgs.insert(launch.extraArgs.end(), global.begin(), global.end());
        if (this->remote() && launch.backend.provider.driver == "claude") launch.remoteName = config.remoteName; // on again
    }
    spec.model = launch.backend.model;
    launch.spec = spec;
    launch.log = log_file_(spec.id);
    launch.transcriptLimitMb = config.transcriptLimitMb;
    launch.memoryBytes = static_cast<std::uintmax_t>(std::max(1, config.memoryMb)) * 1024 * 1024;
    launch.memoryEntries = static_cast<std::size_t>(std::max(0, config.memoryEntries));
    launch.listener = [this](const cluster::Event& event) {this->event_(event);};
    return std::make_shared<cluster::Session>(std::move(launch));
}

_cold std::string cluster::Manager::spawn(const cluster::SpawnRequest& request)
{
    const cluster::Config& config = this->config();
    cluster::SpawnRequest req = request;
    if (!req.profile.empty()) {
        auto it = config.profiles.find(req.profile);
        if (it == config.profiles.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no profile '" + req.profile + "'");
        const cluster::Profile& profile = it->second;
        if (req.cwd.empty()) req.cwd = profile.cwd;
        if (req.backend.empty()) req.backend = profile.backend;
        if (req.mode.empty()) req.mode = profile.mode;
        if (req.prompt.empty()) req.prompt = profile.prompt;
    }
    const std::filesystem::path cwd = cluster::resolve_folder(req.cwd); // a folder, or a project of the map by its name

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
    spec.backend = req.backend.empty() ? config.backend : req.backend;
    spec.mode = req.mode.empty() ? config.mode : req.mode;
    spec.effort = req.effort.empty() ? config.effort : req.effort;
    if (!req.context.empty()) {
        const std::optional<std::string> window = cluster::Config::contextWindow(req.context);
        if (!window) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "context: auto, or 100k to 1M tokens (200k, 500000, 1M)");
        spec.context = *window;
    }
    if (req.cheap) { // nothing of these asks for a big model: the smallest one, no thinking to speak of
        const std::string provider = spec.backend.substr(0, spec.backend.find('/'));
        if (this->vendorOf(spec.backend) == "anthropic") {
            spec.backend = provider + "/haiku";
            spec.model = "haiku";
        }
        const std::vector<std::string>& levels = cluster::effort_levels(this->_auth.resolve(spec.backend).provider.driver);
        if (!levels.empty()) spec.effort = levels.front();
    }
    spec.profile = req.profile;
    spec.panels = std::set<std::string>(config.panels.begin(), config.panels.end());
    spec.created = cluster::now();
    spec.claudeId = req.resume;     // a conversation resumed (another agent's, a fork of a session)
    spec.fork = req.fork && !req.resume.empty();
    if (!req.profile.empty()) spec.extraArgs = config.profiles.at(req.profile).extraArgs;

    const std::shared_ptr<cluster::Session> session = this->make_(spec);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_sessions[spec.id] = session;
        this->_order.push_back(spec.id);
    }
    session->append(req.history);   // its transcript so far, shown (and in its log)
    if (!req.note.empty()) session->mark(req.note);
    session->start();
    if (!req.prompt.empty()) {
        if (req.reply) this->replyTo(spec.id); // before the prompt: a short turn could end before it is known
        if (req.oneShot && req.reply) {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_oneShot.insert(spec.id);
        }
        if (!this->hold_(spec.id, req.prompt, {}, req.origin)) this->deliver_(session, req.prompt, {}, req.origin);
    }
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

_cold void cluster::Manager::send(const std::string& id, const std::string& tagged, const bool force, const std::vector<std::string>& images, const std::string& origin)
{
    const std::string text = cluster::paste_keep(tagged); // the hidden ids of the /entries picked: for the interfaces only; the marks of the pastes:
                                                          // the session gives them to the agent
    if (this->_replica) {
        (void)this->call({{"cmd", "send"}, {"session", id}, {"text", text}, {"force", force}, {"images", images}, {"prompt_origin", origin}, {"raw", true}});
        return;
    }
    if (this->_closing) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "claude-cluster is quitting"); // a session stopped would start again: an agent left behind
    const std::shared_ptr<cluster::Session> session = this->get_(id);
    if (this->hold_(session->id(), text, images, origin)) return; // an update switching: sent by the new version, never a new turn now
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_origin[session->id()] = "host"; // a web device marks it after (Web: setOrigin)
    }
    if (!force) this->refuseOver_(id, session->snapshotLight());
    this->deliver_(session, text, images, origin);
}

_cold void cluster::Manager::refuseOver_(const std::string& id, const cluster::Snapshot& snap)
{
    const cluster::Config& config = this->config();
    if (config.limits.block) {
        if (const std::string why = this->limitReached(); !why.empty()) {
            this->notice_(id, why + ": prompt refused", true);
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, why + " (force it, or raise it in Settings > Accounts & limits)");
        }
    }
    if (config.budget > 0 && snap.metrics.costKnown && snap.metrics.cost >= config.budget) {
        this->notice_(id, "budget reached (" + cluster::human_cost(snap.metrics.cost) + "): prompt refused", true);
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction,
            "budget of " + cluster::human_cost(config.budget) + " reached for " + snap.spec.name + " (force it, or raise sessions.budget_usd)");
    }
}

_cold void cluster::Manager::deliver_(std::shared_ptr<cluster::Session> session, const std::string& text, const std::vector<std::string>& images, const std::string& origin)
{
    // the object of a session refuses what is sent once it is replaced (a model, a folder, /clear, a quit): kept for the new one (never answered ok and lost,
    // never waited for here: the reader of the old object, joined by the replacement, may be the one sending), or the new one once it is there
    const std::string id = session->id();
    for (int tries = 0; tries < 3; ++tries) { // a replacement that has just ended: the object of the moment
        if (session->send(text, images, origin)) return;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (const auto replacing = this->_replacing.find(id); replacing != this->_replacing.end()) {
                cluster::Queued kept{text, images, {}};
                kept.at = cluster::now_ms();
                kept.origin = origin;
                replacing->second.push_back(std::move(kept));
                return;
            }
        }
        session = this->get_(id);
    }
    throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "session " + id + " is closing");
}

_cold void cluster::Manager::giveBack_(const std::shared_ptr<cluster::Session>& session, const cluster::Queued& queued)
{
    try {
        if (!this->hold_(session->id(), queued.text, queued.images, queued.origin) && !session->send(queued.text, queued.images, queued.origin))
            this->notice_(session->id(), "a prompt not given, the session closing: " + cluster::one_line(queued.text, 120), true);
    } catch (const std::exception& e) { // its agent not started again: the others still given, the user told
        this->notice_(session->id(), "a prompt not given (" + why_(e) + "): " + cluster::one_line(queued.text, 120), true);
    }
}

_cold std::shared_ptr<cluster::Session> cluster::Manager::replace_(const std::string& id, const std::function<void(cluster::SessionSpec&)>& change, const bool resend)
{
    // one at a time for a session; from here its old object refuses what is sent (kept in _replacing): before it, its queue or its running turn are read below
    {
        std::unique_lock<std::mutex> lock(this->_mutex);
        this->_replaced.wait(lock, [this, &id]() {return !this->_replacing.contains(id);});
        this->_replacing[id];
    }
    std::shared_ptr<cluster::Session> old;
    std::shared_ptr<cluster::Session> session;
    std::vector<cluster::Queued> unfinished;
    std::exception_ptr failed;
    try {
        old = this->get_(id);
        old->retire(true);
        if (resend) unfinished = old->unfinished();
        old->stop();
        cluster::SessionSpec spec = old->spec();
        change(spec);
        const std::shared_ptr<cluster::Session> made = this->make_(spec);
        made->loadHistory();
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (!this->_sessions.contains(id)) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "session '" + id + "' closed meanwhile");
            this->_sessions[id] = made;
        }
        session = made;
        session->start();
    } catch (...) { // what waits still goes somewhere (not replaced: its old object, its agent started again), the caller told after
        failed = std::current_exception();
    }
    if (!session && old) {
        old->retire(false);
        session = old;
    }
    if (session)
        for (const cluster::Queued& queued: unfinished)
            this->giveBack_(session, queued);
    for (;;) { // in their order; a prompt sent while they are given goes after them
        std::vector<cluster::Queued> kept;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            const auto replacing = this->_replacing.find(id);
            if (replacing->second.empty()) {
                this->_replacing.erase(replacing);
                break;
            }
            kept.swap(replacing->second);
        }
        if (session)
            for (const cluster::Queued& queued: kept)
                this->giveBack_(session, queued);
    }
    this->_replaced.notify_all();
    if (failed) std::rethrow_exception(failed);
    return session;
}

_cold void cluster::Manager::clear(const std::string& id)
{
    // a new conversation (like /clear of Claude Code): the session again without --resume, an empty transcript (the old
    // one archived: found by the search, not shown)
    const std::shared_ptr<cluster::Session> session = this->replace_(id, [this, &id](cluster::SessionSpec& spec) {
        cluster::busy_step("its transcript archived (found by the search)");
        spec.claudeId.clear();
        spec.leaf.clear();
        spec.resumeAt.clear();
        this->archive_(id);
        spec.pin = nullptr; // the pinned item belonged to the old transcript
        spec.cost = 0.0;   // the figures of the list follow the conversation (the usage ledger keeps the spent ones): zero BEFORE the new session reads them
        spec.tokens = 0;
    }, false);
    this->notice_(id, session->spec().name + ": new conversation (the transcript before archived)");
    this->save();
}

_cold void cluster::Manager::setBackend(const std::string& id, const std::string& provider)
{
    // api | auth: the same agent, billed through an API key or through its login - the session started again on it (--resume)
    cluster::SessionSpec spec = this->get_(id)->spec();
    const std::string model = spec.backend.find('/') != std::string::npos ? spec.backend.substr(spec.backend.find('/') + 1) : spec.model;
    (void)this->_auth.resolve(provider + (model.empty() ? "" : "/" + model)); // an unknown provider: refused here
    spec.backend = provider + (model.empty() ? "" : "/" + model);
    (void)this->replace_(id, [&spec](cluster::SessionSpec& now) {now.backend = spec.backend;}, true);
    this->save();
    this->notice_(id, spec.name + ": billing " + (this->authKind(spec.backend) == "api" ? "api (an API key)" : "auth (the login)"));
}

_cold void cluster::Manager::setModel(const std::string& id, const std::string& model)
{
    // the same conversation (--resume) on another model: the session started again
    const std::shared_ptr<cluster::Session> session = this->replace_(id, [&model](cluster::SessionSpec& spec) {
        spec.backend = spec.backend.substr(0, spec.backend.find('/')) + "/" + model;
        spec.model = model;
    }, true);
    this->save();
    this->notice_(id, session->spec().name + ": model " + model);
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
    (void)this->replace_(id, [&effort](cluster::SessionSpec& now) {now.effort = effort;}, true);
    this->save();
    this->notice_(id, spec.name + ": effort " + effort);
}

_cold void cluster::Manager::relaunch_(const std::string& id, const cluster::SessionSpec& spec)
{
    // the session started again with this spec (its conversation: --resume), its transcript kept
    // a turn running or prompts queued: not lost, sent again to the new one (the change applies to them)
    (void)this->replace_(id, [&spec](cluster::SessionSpec& now) {now = spec;}, true);
    this->save();
}

_cold void cluster::Manager::close(const std::string& id)
{
    if (id == GLOBAL_ID) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the global session can't be closed (quit claude-cluster)");
    // out of the lists at once, its agent stopped after (3 s at most, more when its background still holds its output): never a
    // closed session still shown by the interfaces meanwhile
    const std::shared_ptr<cluster::Session> session = this->get_(id);
    cluster::SessionSpec spec = session->spec();
    spec.deleted = cluster::now();
    bool owed = false;              // the global session still waits for its answer
    std::set<std::string> waiting;  // the sub-sessions whose task ran in it
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        owed = this->_replies.contains(id);
        this->_replies.erase(id);
        this->_followed.erase(id);
        this->_oneShot.erase(id);
        this->_sessions.erase(id);
        std::erase(this->_order, id);
        this->_trash.push_back(spec);
        for (cluster::Task& task: this->_tasks)
            if (task.session == id && task.status == "running") {
                task.status = "failed";
                task.reason = "its session was closed before it finished";
                task.ended = cluster::now();
                if (!task.by.empty() && task.by != GLOBAL_ID && task.group.empty()) waiting.insert(task.by);
            }
    }
    // never left waiting for an answer that will not come
    const std::string closed = "[closed: " + spec.name + " (" + id + ")]\nclosed before it answered: no reply will come from it (restorable from the trash)";
    if (owed) cluster::guarded([this, closed]() {
        try {
            this->send(GLOBAL_ID, closed, true, {}, "mcp");
        } catch (const utils::exception::IException&) {}
    }).detach();
    for (const std::string& by: waiting)
        if (by != id) this->tell_(by, closed);
    ++this->_version;
    session->retire(true); // a prompt being sent: refused (no such session), never an agent started again on the object going away
    session->stop();
    this->dropFor_(id, false, spec.name); // its tasks and schedules: cancelled (closed)
    if (this->_orch) this->_orch->sessionGone(id, false); // its heavy jobs: removed (queued) or killed (running)
    this->notice_(id, spec.name + " closed (restorable " + std::to_string(this->config().trashDays) + " days)");
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
    const std::int64_t deleted = spec.deleted;
    spec.deleted = 0;
    std::shared_ptr<cluster::Session> session;
    try {
        session = this->make_(spec);
    } catch (const std::exception&) { // its agent not there any more: still in the trash, never lost
        spec.deleted = deleted;
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_trash.push_back(spec);
        throw;
    }
    session->loadHistory();
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
    std::map<std::string, std::string> names; // <id, name>: its tasks / schedules found by either
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        const std::int64_t limit = cluster::now() - static_cast<std::int64_t>(this->config().trashDays) * 86400;
        std::erase_if(this->_trash, [&](const cluster::SessionSpec& spec) {
            const bool gone = id == "*" || (id.empty() ? spec.deleted < limit : (spec.id == id || spec.name == id));
            if (gone) removed.push_back(spec.id);
            if (gone) names[spec.id] = spec.name;
            return gone;
        });
    }
    if (!id.empty() && id != "*" && removed.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session '" + id + "' in the trash");
    for (const std::string& gone: removed)
        this->dropFor_(gone, true, names[gone]); // deleted for good: its tasks and schedules too
    if (this->_orch)
        for (const std::string& gone: removed)
            this->_orch->sessionGone(gone, true); // its heavy jobs forgotten
    std::error_code error;
    for (const std::string& gone: removed) {
        std::filesystem::remove(log_file_(gone), error);
        cluster::attach_purge(this->config(), gone); // its attachments
        for (const std::filesystem::directory_entry& file: std::filesystem::directory_iterator(log_file_(gone).parent_path() / "archive", error))
            if (file.path().stem().string().starts_with(gone + "-")) std::filesystem::remove(file.path(), error); // its old conversations
    }
    if (!removed.empty()) this->save();
}

_cold std::string cluster::Manager::applyTo(const std::string& kind, const std::string& value, const std::string& scope, const std::string& current)
{
    if (kind != "model" && kind != "mode" && kind != "effort")
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "model | mode | effort, not " + kind);
    if (cluster::trim(value).empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no value");
    if (scope == "new") {
        // the next new sub-sessions: the default of [agent] (a model: on the provider of the session shown)
        std::string key = kind == "mode" ? "permission_mode" : kind == "effort" ? "effort" : "provider";
        std::string setting = value;
        if (kind == "model") {
            const std::optional<cluster::Snapshot> snap = this->snapshot(this->resolve(current));
            const std::string provider = snap ? snap->spec.backend.substr(0, snap->spec.backend.find('/')) : std::string("claude");
            setting = value.find('/') == std::string::npos ? provider + "/" + value : value;
        }
        (void)cluster::settings_set(this->config(), "agent", key, setting); // what it answers: applied at the next start or not, never a failure (that one throws)
        (void)this->config().reload();
        return kind + " " + setting + " for the next new sub-sessions";
    }
    std::vector<std::string> ids;
    if (scope.empty() || scope == "here") ids.push_back(this->resolve(current));
    else if (scope == "global") ids.push_back(GLOBAL_ID);
    else if (scope == "subs" || scope == "all")
        for (const cluster::Snapshot& snap: this->list(scope == "all"))
            ids.push_back(snap.spec.id);
    else ids.push_back(this->resolve(scope));
    std::string failed;
    for (const std::string& id: ids) {
        try {
            if (kind == "model") this->setModel(id, value);
            else if (kind == "mode") this->setMode(id, value);
            else this->setEffort(id, value);
        } catch (const utils::exception::IException& e) {
            failed += (failed.empty() ? "" : ", ") + id + " (" + e.info() + ")";
        }
    }
    return kind + " " + value + ": " + std::to_string(ids.size()) + " session(s)" + (failed.empty() ? std::string() : ", not for " + failed);
}

_cold std::string cluster::Manager::resume(const std::string& id)
{
    // Retry now: the turn waiting for a retry of the API stopped, then "continue" (sent as soon as it stopped: the same
    // conversation, it goes on where it was); Continue: a turn stopped (interrupted, an error): "continue" sent
    const std::shared_ptr<cluster::Session> session = this->get_(this->resolve(id));
    const cluster::Snapshot snap = session->snapshot();
    const bool running = snap.state == cluster::State::Working || snap.state == cluster::State::Starting;
    if (running) this->interrupt(snap.spec.id);
    if (this->hold_(snap.spec.id, "continue", {}, "")) return "held: continued by the new version (an update is switching)";
    this->deliver_(session, "continue", {}, "");
    this->notice_(snap.spec.id, snap.spec.name + (running ? ": retried now" : ": continued"));
    return running ? "retried now" : "continued";
}

_cold void cluster::Manager::askGlobal(const std::string& id, const std::string& requestId, const cluster::Json& questions)
{
    // the user lets the global session answer the questions of an agent for them (its options, a text of its own allowed)
    if (!this->snapshot(GLOBAL_ID)) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "no global session");
    const std::optional<cluster::Snapshot> asking = this->snapshot(id);
    const std::string name = asking ? asking->spec.name : id;
    this->send(GLOBAL_ID, "[questions from " + name + " (" + id + "), the user asks you to answer them for them] " + questions.dump(-1, ' ', false, cluster::JSON_LENIENT)
        + "\nAnswer all of them at once with session_allow {session: \"" + id + "\", request_id: \"" + requestId + "\", answers: {<question>: <label>}} "
        "(several labels: \"a, b\"; none fits: a short text of your own): choose what the user would want from this conversation, the project and "
        "machine.md; ask the user first (AskUserQuestion) only when it may destroy data. Then tell them in one line what you answered.", false, {}, "mcp");
    this->notice_(id, name + ": its questions given to the global session");
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

_cold cluster::Json cluster::Manager::loadOlder(const std::string& id, const std::size_t count)
{
    // an interface scrolled to the top of a transcript: its older entries back in memory (the core keeps them; every interface sees them)
    if (this->_replica) return this->call({{"cmd", "older"}, {"session", id}, {"count", count}});
    const std::shared_ptr<cluster::Session> session = this->get_(this->resolve(id));
    const std::size_t loaded = session->loadOlder(std::clamp<std::size_t>(count, 1, 5000));
    ++this->_version;
    return {{"loaded", loaded}, {"older", session->snapshot().older}};
}

_cold void cluster::Manager::limitMemory_(const std::vector<std::shared_ptr<cluster::Session>>& sessions)
{
    // the settings of the moment for each transcript, then all of them together: past the global budget, each keeps its share
    // (its newest entries), the older ones stay in its log (loaded again when its top is reached)
    const cluster::Config& config = this->config();
    const std::uintmax_t own = static_cast<std::uintmax_t>(std::max(1, config.memoryMb)) * 1024 * 1024;
    const std::uintmax_t total = static_cast<std::uintmax_t>(std::max(1, config.memoryTotalMb)) * 1024 * 1024;
    std::uintmax_t used = 0;
    std::vector<std::uintmax_t> each;
    for (const std::shared_ptr<cluster::Session>& session: sessions) {
        session->limitMemory(own, static_cast<std::size_t>(std::max(0, config.memoryEntries)));
        each.push_back(session->memoryUsed());
        used += each.back();
    }
    if (used <= total) return;
    for (std::size_t i = 0; i < sessions.size(); ++i)
        sessions[i]->trimTo(static_cast<std::uintmax_t>(static_cast<long double>(each[i]) * static_cast<long double>(total) / static_cast<long double>(used)));
    cluster::debug("core", "transcripts in memory: " + std::to_string(used / 1024 / 1024) + " MB past " + std::to_string(total / 1024 / 1024) + " MB, the oldest entries dropped");
}

_cold std::string cluster::Manager::setKeepWarm(const std::string& id, const std::string& value)
{
    // a session kept warm or not; marked while the mode is off: the mode becomes chosen (the marked ones only)
    if (this->_replica) return this->call({{"cmd", "keep_warm"}, {"session", id}, {"value", value}}).value("message", std::string());
    const std::string session = this->resolve(id);
    const std::shared_ptr<cluster::Session> target = this->get_(session);
    const bool on = value == "on" || (value != "off" && !cluster::keep_warm_on(this->config(), target->snapshotLight())); // past keep_warm_hours: not warm any more, on again
    target->setKeepWarm(on);
    std::string mode = this->config().keepWarm;
    if (on && mode == "off") {
        (void)cluster::settings_set(this->config(), "sessions", "keep_warm", "chosen");
        (void)this->config().reload();
        mode = "chosen";
    }
    this->save();
    ++this->_version;
    return std::string(on ? "kept warm ♨" : "no longer kept warm") + " (mode " + mode + (mode == "all" ? ": every session is kept warm whatever its mark" : "")
        + "; woken up " + std::to_string(this->config().keepWarmMargin) + " s before its " + std::to_string(this->config().keepWarmTtl) + " min cache expires)";
}

/* the prompt cache */
_nodiscard static std::int64_t cache_at_(const cluster::Snapshot& s)
{
    // the last request of the agent that was answered: its last message got back (kept over a rewind: the entries after the cut are gone, not
    // the cache of its prefix). A prompt sent and never answered (a wake-up that failed, an error, a process that died) wrote nothing: it does
    // not count, until the session has no answer at all (then its last prompt stands for it)
    if (s.spec.cachedAt > 0) return s.spec.cachedAt;
    for (auto it = s.entries.rbegin(); it != s.entries.rend(); ++it)
        if (it->kind == cluster::EntryKind::User) return it->time;
    return 0;
}

_nodiscard bool cluster::keep_warm_on(const cluster::Config& c, const cluster::Snapshot& s)
{
    return cluster::keep_warm_asked(c, s) && (c.keepWarmHours <= 0 || s.spec.usedAt <= 0 || cluster::now() - s.spec.usedAt <= static_cast<std::int64_t>(c.keepWarmHours) * 3600);
}

_nodiscard std::optional<std::int64_t> cluster::cache_left(const cluster::Config& c, const cluster::Snapshot& s)
{
    const std::int64_t at = cache_at_(s);
    if (at == 0) return std::nullopt;
    return static_cast<std::int64_t>(c.keepWarmTtl) * 60 - (cluster::now() - at);
}

_nodiscard std::string cluster::cache_text(const cluster::Config& c, const cluster::Snapshot& s)
{
    if (s.state == cluster::State::Working) return "in use";
    const std::optional<std::int64_t> left = cluster::cache_left(c, s);
    if (!left) return "-";
    if (*left > 0) return std::to_string(*left / 60) + ":" + (*left % 60 < 10 ? "0" : "") + std::to_string(*left % 60) + " left";
    return -*left < 60 ? "expired just now" : -*left < 7200 ? "expired " + std::to_string(-*left / 60) + " min ago" : "expired " + std::to_string(-*left / 3600) + " h ago";
}

_cold bool cluster::Manager::warmExpired(const std::string& id, const std::string& value)
{
    // on (or a toggle to on) while its last turn is older than the cache: left to cool unless the user wants it anyway
    const std::optional<cluster::Snapshot> s = this->snapshot(this->resolve(id));
    if (!s || value == "off" || (value == "toggle" && cluster::keep_warm_on(this->config(), *s)) || s->state == cluster::State::Working) return false;
    const std::optional<std::int64_t> left = cluster::cache_left(this->config(), *s);
    return left && *left <= 0;
}

_cold std::string cluster::Manager::warmNow(const std::string& id)
{
    const std::string done = this->setKeepWarm(id, "on");
    this->wake_(this->resolve(id));
    return done + " - woken up now: its context read again and cached";
}

_cold void cluster::Manager::wake_(const std::string& id)
{
    // the previous wake-up still the last prompt (nothing real since, its background not running: stopping the agent would stop it): taken back
    // first, then the same prompt sent again on the same cached prefix (read, almost nothing written), so the conversation holds one wake-up at
    // most instead of one more each time (its context and the cost of every later turn growing with them). Not a session of Claude Code, its
    // background running, or that prompt not found in its transcript (compacted): sent after it as before.
    // Given to the session directly (Session::wake), never by send: known as a wake-up by this path only, whatever anyone writes as a prompt; never held by an
    // update, a tiny turn that must not wait (the update waits for it, as for any turn running: the sessions would cool for as long as it waits on the agents of another);
    // the device of the last real prompt stays the one told of the next turn
    if (this->_closing) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "claude-cluster is quitting");
    const std::shared_ptr<cluster::Session> session = this->get_(id);
    try {
        if (session->takeBackWake()) {
            this->save();
            ++this->_version;
        }
    } catch (const utils::exception::IException& e) {
        cluster::debug("keepwarm", id + ": the last wake-up not taken back: " + e.info());
    }
    if (session->state() != cluster::State::Idle) return; // a real prompt came meanwhile: its turn keeps the cache
    this->refuseOver_(id, session->snapshotLight());
    (void)session->wake();
}

_cold void cluster::Manager::keepWarm_(void)
{
    // an idle session kept warm, its last answered request close to the end of its prompt cache (ttl - margin): a tiny turn now, the cache read
    // again (and kept), its background running or not (an idle session waiting on a long compilation loses its context as any other). Never when
    // it works, asks or queues; never after the cache expired (the core was down: left to cool) nor past keep_warm_hours after its last real use
    const cluster::Config& config = this->config();
    if (this->_replica || this->_closing || config.keepWarm == "off") return; // an update switching too: it can wait an hour on the agents of a session, the others cooling
    const std::int64_t now = cluster::now();
    const std::int64_t ttl = static_cast<std::int64_t>(config.keepWarmTtl) * 60;
    // checked every 10 s: a window under 16 s could fall between two checks (the cache then lost, the session left to cool)
    const std::int64_t margin = std::clamp<std::int64_t>(config.keepWarmMargin, 16, ttl - 5);
    const std::function<bool(const std::string&)> first = [this](const std::string& key) { // told once (its wake-ups, in their threads, too)
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_warmMissed.size() > 2000) this->_warmMissed.clear();
        return this->_warmMissed.insert(key).second;
    };
    for (const cluster::Snapshot& s: this->listLight(true)) { // without their entries: nothing copied every 10 s
        if (!cluster::keep_warm_asked(config, s) || s.spec.cachedAt <= 0) continue;
        if (!cluster::keep_warm_on(config, s)) { // the mark stays, the ♨ goes: told once, a prompt or /keepwarm warms it again
            if (first(s.spec.id + ":hours:" + std::to_string(s.spec.usedAt)))
                this->notice_(s.spec.id, "keep warm: stopped, no prompt for " + std::to_string(config.keepWarmHours) + " h (keep_warm_hours, 0: never stops); a prompt or /keepwarm warms it again");
            continue;
        }
        if (s.state != cluster::State::Idle || !s.permissions.empty() || s.queued > 0) continue;
        const std::int64_t last = s.spec.cachedAt; // answered requests only: a wake-up sent and failed is tried again until the cache expires
        if (now - last < ttl - margin) continue;
        if (now - last > ttl) { // the window missed (the core down, a wake-up failed): told once when it just happened, left to cool
            if (now - last - ttl < 120 && first(s.spec.id + ":" + std::to_string(last))) {
                cluster::debug("keepwarm", s.spec.name + ": cache expired " + std::to_string(now - last - ttl) + " s ago, no wake-up (left to cool)");
                this->notice_(s.spec.id, "keep warm: its prompt cache expired before a wake-up could run (left to cool; a prompt or /keepwarm warms it again)", true);
            }
            continue;
        }
        const std::string id = s.spec.id;
        const std::string name = s.spec.name;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (!this->_waking.insert(id).second) continue; // the one before still running
        }
        // out of the tick: a take-back reads its transcript and starts its agent again (seconds), the tick of every session never waits for it
        const std::function<void(void)> wake = [this, id, name, last, ttl, now]() {
            try {
                cluster::debug("keepwarm", name + ": wake-up, cache " + std::to_string(ttl - (now - last)) + " s left");
                this->wake_(id);
            } catch (const std::exception& e) {
                const std::string why = why_(e);
                cluster::debug("keepwarm", name + ": wake-up refused: " + why);
                bool told = false;
                {
                    std::lock_guard<std::mutex> lock(this->_mutex);
                    told = !this->_warmMissed.insert(id + ":refused:" + std::to_string(last)).second;
                }
                try {
                    if (!told) this->notice_(id, "keep warm: the wake-up was refused (" + why + "), tried again until the cache expires", true);
                } catch (const std::exception&) {} // whatever happens: the next wake-up of this session stays possible
            }
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_waking.erase(id);
        };
        try {
            cluster::guarded(wake).detach();
        } catch (const std::system_error&) { // no thread left: here
            wake();
        }
    }
}

_cold void cluster::Manager::togglePanel(const std::string& id, const std::string& panel)
{
    this->get_(id)->togglePanel(panel);
    this->save();
}

_cold void cluster::Manager::cd(const std::string& id, const std::string& cwd)
{
    if (id == GLOBAL_ID) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the folder of the global session is set by global.cwd");
    const std::filesystem::path path = cluster::resolve_folder(cwd); // a folder, or a project of the map by its name

    // A conversation belongs to its project: the session starts a new one in the new folder (same id / name / log)
    const std::shared_ptr<cluster::Session> session = this->replace_(id, [&path](cluster::SessionSpec& spec) {
        spec.cwd = path.string();
        spec.claudeId.clear();
    }, false);
    this->notice_(id, session->spec().name + " moved to " + cluster::short_path(path.string()));
    this->save();
}

/* global session */
_cold std::vector<std::string> cluster::Manager::globalArgs_(void) const
{
    std::vector<std::string> args = {"--mcp-config", cluster::mcp_config_path().string(), "--allowedTools", "mcp__claude-cluster"};
    const std::vector<std::string> prompt = prompt_args_(this->config().globalPrompt, true, cluster::method_text(this->config(), true), this->config().orchestration, this->config().workflow);
    args.insert(args.end(), prompt.begin(), prompt.end());
    return args;
}

_cold void cluster::Manager::writeMcpConfig_(void) const
{
    cluster::Json config = {{"mcpServers", cluster::mcp_servers(this->config().mcpServers)}}; // the custom ones ([mcp] servers)
    config["mcpServers"]["claude-cluster"] = {
        {"type", "stdio"}, {"command", cluster::self_exe().string()}, {"args", {"mcp"}},
        {"env", {{"CLAUDE_CLUSTER_SOCKET", cluster::socket_path().string()}}},
    };
    cluster::write_json(cluster::mcp_config_path(), config);
}

_cold void cluster::Manager::startGlobal(void)
{
    this->_startDecided = true;
    const cluster::Config& config = this->config();
    if (!config.globalEnabled) return;
    // one at a time (the answer to the previous-run question, the tick, its retry), never over a session that runs: only the first one gets its saved
    // conversation, a second one would replace it by a new one - and drop the session under the lock, its reader waiting for that lock
    if (this->_globalStarting.exchange(true)) return;
    struct Starting {
        std::atomic<bool>& flag;
        explicit Starting(std::atomic<bool>& f): flag(f) {};
        Starting(const Starting&) = delete;
        Starting& operator=(const Starting&) = delete;
        ~Starting() {this->flag = false;};
    } starting(this->_globalStarting);
    cluster::SessionSpec spec;
    bool saved = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_sessions.contains(GLOBAL_ID)) return;
        auto it = std::find_if(this->_previous.begin(), this->_previous.end(), [](const cluster::SessionSpec& s) {return s.global;});
        if (it != this->_previous.end()) {
            spec = *it;
            saved = true;
            this->_previous.erase(it);
        }
    }
    const cluster::SessionSpec taken = spec;
    bool listed = false;
    const std::function<void(const std::string&)> failed = [&](const std::string& why) {
        if (saved && !listed) { // its conversation for the next try, never a new one (a failed start, an agent not installed yet)
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_previous.push_back(taken);
        }
        this->notice_(GLOBAL_ID, "global session: " + why, true);
        this->retryGlobal_();
    };
    try {
        this->writeMcpConfig_();
        spec.id = GLOBAL_ID;
        spec.name = "global";
        spec.global = true;
        const std::string cwd = this->globalDir_().string();
        if (!spec.cwd.empty() && spec.cwd != cwd) spec.claudeId.clear(); // its conversation is kept by claude in the other folder
        spec.cwd = cwd;
        spec.backend = config.globalBackend + (config.globalModel.empty() ? "" : "/" + config.globalModel);
        if (spec.created == 0) spec.created = cluster::now();
        if (spec.panels.empty()) spec.panels = {"tokens", "context", "cost", "usage"};
        spec.mode = !config.globalMode.empty() ? config.globalMode : spec.mode.empty() ? config.mode : spec.mode;
        if (!config.globalEffort.empty()) spec.effort = config.globalEffort; // else its own (/effort), first: [agent]
        else if (spec.effort.empty()) spec.effort = config.effort;
        if (this->_auth.resolve(spec.backend).provider.driver != "claude") {
            this->notice_(GLOBAL_ID, "[agent] provider must use the claude driver (MCP control): '" + spec.backend + "' replaced by claude", true);
            spec.backend = "claude";
            spec.claudeId.clear();
        }
        // a background claude of the old Remote Control (claude --bg) still has the conversation: stopped (its conversation
        // kept), Remote Control goes on in the session itself
        if (!spec.claudeId.empty() && this->_auth.resolve(spec.backend).provider.driver == "claude") {
            const std::string claude = config.command("claude");
            const cluster::Captured agents = cluster::capture({claude, "agents", "--json"}, "", 15);
            const cluster::Json list = cluster::json_parse(agents.out, nullptr, false);
            if (list.is_array()) {
                for (const cluster::Json& agent: list) {
                    if (!agent.is_object()) continue;
                    const cluster::Json conversation = agent.value("sessionId", cluster::Json());
                    const cluster::Json id = agent.value("id", cluster::Json());
                    // the output of another tool: a field of another kind (null) is not this conversation
                    if (!conversation.is_string() || !id.is_string() || conversation.get<std::string>() != spec.claudeId || id.get<std::string>().empty()) continue;
                    (void)cluster::capture({claude, "stop", id.get<std::string>()}, "", 30);
                    this->setUi("remote", "true");
                    this->notice_(GLOBAL_ID, "the background Remote Control session was stopped: Remote Control goes on in the global session itself");
                }
            }
        }
        if (this->_globalNew) { // "start new": its transcript archived, the new conversation shown alone
            this->_globalNew = false;
            this->archive_(GLOBAL_ID);
        }
        const std::shared_ptr<cluster::Session> session = this->make_(spec);
        session->loadHistory();
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_sessions[GLOBAL_ID] = session;
        }
        listed = true;
        session->start();
    } catch (const utils::exception::IException& e) {
        failed(std::string(e.what()) + ": " + e.info());
    } catch (const std::exception& e) { // a bad conversation file (JSON...) must not leave the cluster without its global session
        failed(e.what());
    }
}

_cold void cluster::Manager::ensureGlobal_(void)
{
    // the global session must always be there: missing (and its start not waiting for the answer of the previous-run question), it is started
    // again, at most every 10 s, out of the tick (the start may wait for claude)
    if (!this->config().globalEnabled || this->_restoreAsk || !this->_startDecided || this->_closing || this->_globalRetrying.load() || this->_globalStarting.load()) return;
    if (this->snapshot(GLOBAL_ID)) return;
    const std::int64_t now = cluster::now();
    if (now - this->_globalEnsured < 10) return;
    this->_globalEnsured = now;
    if (this->_globalRetrying.exchange(true)) return;
    cluster::guarded([this]() {
        try {
            this->notice_(GLOBAL_ID, "the global session was missing: started again", true);
            this->startGlobal();
        } catch (const std::exception& e) { // tried again in 10 s
            cluster::debug("core", std::string("the global session not started: ") + e.what());
        }
        this->_globalRetrying = false;
    }).detach();
}

_cold void cluster::Manager::retryGlobal_(void)
{
    // the global session must always exist: started again every 15 s until its process runs
    if (this->_globalRetrying.exchange(true)) return;
    cluster::guarded([this]() {
        for (int attempt = 0; attempt < 240; ++attempt) {
            std::this_thread::sleep_for(std::chrono::seconds(15));
            const std::optional<cluster::Snapshot> snap = this->snapshot(GLOBAL_ID);
            if (snap && snap->state != cluster::State::Error) break;
            try {
                if (snap) this->get_(GLOBAL_ID)->restart();
                else this->startGlobal();
            } catch (...) {} // tried again at the next turn of the loop
        }
        this->_globalRetrying = false;
    }).detach();
}

_cold std::filesystem::path cluster::Manager::globalDir_(void) const
{
    if (!this->config().globalCwd.empty()) return cluster::expand_home(this->config().globalCwd);
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
    cluster::Json data = cluster::json_parse(in, nullptr, false);
    if (data.is_discarded() || !data.is_object()) return;
    cluster::Json& project = data["projects"][folder.string()];
    if (project.is_object() && project.value("hasTrustDialogAccepted", false)) return;
    project["hasTrustDialogAccepted"] = true;
    try {
        cluster::write_text(file, data.dump(2, ' ', false, cluster::JSON_LENIENT)); // its mode kept (its account in it: never more readable than it was)
    } catch (const utils::exception::IException&) {}
}

} // namespace

_cold void cluster::Manager::setRemote(const bool on)
{
    // the global session itself on claude.ai (live both ways), on again at every start until it is turned off
    const std::shared_ptr<cluster::Session> session = this->get_(GLOBAL_ID);
    if (on && this->config().globalCwd.empty()) trust_folder_(session->spec().cwd);
    session->remoteControl(on, this->config().remoteName);
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
                cluster::guarded([this, took]() {
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
                        const cluster::Json& input = asked->permissions.back().input;
                        const cluster::Json questions = input.is_object() && input.contains("questions") ? input["questions"] : cluster::Json::array();
                        n.requestId = asked->permissions.back().requestId;
                        // its options as buttons: a question of the shape expected only (as an agent wrote it: any shape, its notification still told)
                        if (questions.is_array() && questions.size() == 1 && questions[0].is_object() && questions[0].contains("options") && questions[0]["options"].is_array()
                            && !(questions[0].contains("multiSelect") && questions[0]["multiSelect"] != false))
                            for (std::size_t i = 0; i < questions[0]["options"].size() && i < 3; ++i) {
                                const cluster::Json& option = questions[0]["options"][i];
                                if (option.is_object() && option.contains("label") && option["label"].is_string()) n.actions.emplace_back("opt" + std::to_string(i), cluster::one_line(option["label"].get<std::string>(), 24));
                            }
                    }
                    if (event.session != GLOBAL_ID && this->snapshot(GLOBAL_ID)) n.actions.emplace_back("global", "Global answers");
                    n.actions.emplace_back("open", "Open");
                    (void)this->notify(n, "question");
                }
                const std::optional<cluster::Snapshot> snap = this->snapshot(event.session);
                if (this->config().globalAnswers && event.session != GLOBAL_ID && snap && !snap->permissions.empty() && this->snapshot(GLOBAL_ID)) {
                    // the global session may choose the answer (the user may answer it first: its popup)
                    const cluster::Permission& p = snap->permissions.back();
                    try {
                        this->send(GLOBAL_ID, "[question from " + name + " (" + event.session + ")] " + p.input.value("questions", cluster::Json::array()).dump(-1, ' ', false, cluster::JSON_LENIENT)
                            + "\nAnswer it with session_allow {session: \"" + event.session + "\", request_id: \"" + p.requestId + "\", answers: {question: label}} "
                            "when you know what the user wants; else leave it to the user (its popup is shown to them).", false, {}, "mcp");
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
            if (event.session != GLOBAL_ID && this->config().bypassToGlobal) {
                // a sub-session in a mode that never asks the user (bypassPermissions, auto, dontAsk): what it still asks - the safety check of
                // Claude Code too - decided by the global session (the user can still answer it)
                const std::optional<cluster::Snapshot> snap = this->snapshot(event.session);
                const std::string mode = snap ? snap->spec.mode : std::string();
                if (snap && (mode == "bypassPermissions" || mode == "auto" || mode == "dontAsk") && !snap->permissions.empty() && this->snapshot(GLOBAL_ID)) {
                    const cluster::Permission& p = snap->permissions.back();
                    try {
                        this->send(GLOBAL_ID, "[" + std::string(p.safety ? "stopped by the safety check of Claude Code" : "permission request") + " from " + name + " (" + event.session + ")] "
                            + p.tool + ": " + cluster::one_line(p.description, 400) + (p.reason.empty() ? "" : "\nwhy it asks: " + cluster::one_line(p.reason, 400))
                            + "\nIts mode is " + mode + " (it never asks the user): you decide with session_allow {session: \"" + event.session + "\", request_id: \"" + p.requestId
                            + "\", behavior: allow | deny}" + (p.safety ? " (allow: it runs anyway)" : "")
                            + ". Ask the user first (AskUserQuestion) when it may destroy data or you are unsure.", false, {}, "mcp");
                    } catch (const utils::exception::IException&) {}
                }
            }
            break;
        case cluster::Event::Kind::Tool:
            this->statsRecord("tool", {{"tool", event.text}, {"session", event.session}});
            break;
        case cluster::Event::Kind::TurnDone:
            this->recordUsage_(event.session);
            if (event.warm) {
                // a keep-warm wake-up over (its "ok"): nothing to tell - no notification (desktop, app, page), sound, face, notice,
                // reply to the global session nor failure popup; a failed one only noted
                if (event.error && !event.interrupted) this->notice_(event.session, name + ": keep-warm wake-up failed: " + cluster::one_line(event.text, 120), true);
                break;
            }
            if (event.error) {
                std::string lowered = event.text;
                std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});
                const bool hit = lowered.find("limit") != std::string::npos || lowered.find("budget") != std::string::npos;
                this->statsRecord(hit ? "limit" : "error", {{"session", event.session}, {"what", cluster::one_line(event.text, 160)}});
            }
            this->limitsWarn();
            if (event.session != GLOBAL_ID) this->turnCounted_(event.session, event.error || event.interrupted);
            if (event.session == GLOBAL_ID && this->_restartAfterTurn.exchange(false)) {
                // it changed its own system prompt: started again on it, the same conversation, once its turn is over
                cluster::guarded([this]() {
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
                if (event.session != GLOBAL_ID) this->replyEnd_(event, name);
            } else if (event.session == GLOBAL_ID) {
                if (this->onGlobalAnswer && !event.error) this->onGlobalAnswer(event.text);
                (void)this->notify({GLOBAL_ID, event.error ? "global session · error" : "global session", event.text, event.error ? cluster::Face::Waiting : cluster::Face::Happy,
                    event.error, event.error ? "dialog-warning" : "message", {{"open", "Open"}}}, event.error ? "error" : "global");
            } else {
                this->replyEnd_(event, name); // asked by the global session: its answer goes back to it
                this->notice_(event.session, name + (event.error ? " failed: " : " done: ") + cluster::one_line(plain_(event.text), 120), event.error);
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
            if (event.error && !event.interrupted) {
                // its turn failed (not a limit: it waits for its reset): its popup, Retry = continue it
                const std::string lower = cluster::lower(event.text);
                if (lower.find("limit") == std::string::npos && lower.find("budget") == std::string::npos) {
                    const std::string sid = event.session;
                    (void)this->fail("the turn of " + name + " (its last prompt)", name + " (" + sid + ")", event.text, [this, sid]() {(void)this->resume(sid);});
                }
            }
            this->taskDone_(event.session, event.error, event.interrupted, event.text);
            break;
        case cluster::Event::Kind::Exited: {
            // the global session must stay: its process ended by itself (not a stop) -> started again on its conversation
            const std::int64_t now = cluster::now();
            if (event.session == GLOBAL_ID && now - this->_globalRestart > 60) {
                this->_globalRestart = now;
                this->notice_(GLOBAL_ID, "global session: its process ended (" + event.text + "), started again");
                cluster::guarded([this]() {
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
            if (event.session != GLOBAL_ID) {
                this->replyEnd_(event, name, true);
                this->taskDone_(event.session, true, false, "its agent stopped: " + event.text); // its task ended (failed), never left running untold
            }
            {
                const std::string sid = event.session;
                (void)this->fail(name + " running (its agent)", name + " (" + sid + ")", event.text, [this, sid]() {(void)this->resume(sid);});
            }
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
    this->progressNotice_(notice); // an operation running: under it
    if (this->onNotice) this->onNotice(notice);
}

/* lifecycle */
_cold void cluster::Manager::pluginsRefresh(const std::string& source)
{
    if (this->_replica) {
        (void)this->call({{"cmd", "plugins_refresh"}, {"source", source}});
        return;
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_pluginPull.insert(source.empty() ? std::string("*") : source); // the next tick: pulled, listed again, the ticks applied
}

_cold cluster::Json cluster::Manager::pluginsJson(void)
{
    // every source: its settings (auto sync, paused), its dates (last sync, last commit), its counts (offered / installed / on), its items
    if (this->_replica) return this->call({{"cmd", "plugins"}});
    const cluster::Config& c = this->config();
    const std::function<bool(const std::map<std::string, bool>&, const std::string&)> on = [](const std::map<std::string, bool>& table, const std::string& key) {
        const auto it = table.find(key);
        return it != table.end() && it->second;
    };
    cluster::Json sources = cluster::Json::array();
    for (const cluster::PluginRepo& repo: cluster::plugins_index()) {
        if (std::find(c.pluginRepos.begin(), c.pluginRepos.end(), repo.url) == c.pluginRepos.end()) continue; // dropped since
        int installed = 0;
        int enabled = 0;
        int skills = 0;
        cluster::Json items = cluster::Json::array();
        for (const cluster::PluginItem& i: repo.items) {
            installed += i.installed ? 1 : 0;
            enabled += i.installed && i.enabled ? 1 : 0;
            skills += i.kind == "skill" ? 1 : 0;
            items.push_back({{"id", i.id}, {"name", i.name}, {"kind", i.kind}, {"description", i.description}, {"path", i.path}, {"installed", i.installed},
                {"enabled", i.enabled}, {"wanted", on(c.pluginSelect, i.id) || on(c.pluginSelect, repo.slug + ":*")}, {"off", on(c.pluginOff, i.id)}});
        }
        sources.push_back({{"url", repo.url}, {"slug", repo.slug}, {"kind", repo.kind}, {"dir", repo.dir}, {"marketplace", repo.marketplace}, {"error", repo.error},
            {"synced", repo.synced}, {"commit", repo.commit}, {"commit_time", repo.commitTime}, {"auto", cluster::plugin_auto(c.pluginAuto, repo.slug)},
            {"paused", on(c.pluginPaused, repo.slug)}, {"all", on(c.pluginSelect, repo.slug + ":*")}, {"offered", repo.items.size()}, {"skills", skills},
            {"plugins", static_cast<int>(repo.items.size()) - skills}, {"installed", installed}, {"enabled", enabled}, {"items", items}});
    }
    // a source given but not listed yet (being cloned)
    for (const std::string& url: c.pluginRepos)
        if (std::none_of(sources.begin(), sources.end(), [&url](const cluster::Json& s) {return s.value("url", std::string()) == url;}))
            sources.push_back({{"url", url}, {"slug", cluster::plugin_slug(url)}, {"kind", cluster::plugin_kind(url)}, {"pending", true}, {"items", cluster::Json::array()},
                {"auto", cluster::plugin_auto(c.pluginAuto, cluster::plugin_slug(url))}, {"paused", on(c.pluginPaused, cluster::plugin_slug(url))}, {"offered", 0},
                {"installed", 0}, {"enabled", 0}});
    return {{"sources", sources}, {"sync_hours", c.pluginSyncHours}, {"busy", this->_pluginsBusy.load()}, {"now", cluster::now()}};
}

_cold void cluster::Manager::pluginsSet(const cluster::Json& changes)
{
    // the ticks of several items / sources written in one go (a bulk action of the plugins page), the sources list too: applied by the core
    if (this->_replica) {
        (void)this->call({{"cmd", "plugins_set"}, {"changes", changes}});
        return;
    }
    std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> writes;
    const std::function<void(const std::string&, const std::string&, const cluster::SettingKind, const std::string&)> put = [&writes](const std::string& section,
        const std::string& key, const cluster::SettingKind kind, const std::string& value) {
        cluster::Setting setting;
        setting.section = section;
        setting.key = key;
        setting.kind = kind;
        writes[{section, key}] = {setting, value};
    };
    static const std::set<std::string> tables = {"select", "off", "auto", "paused"};
    for (const cluster::Json& change: changes.value("items", cluster::Json::array())) {
        const std::string table = change.value("table", std::string());
        const std::string key = change.value("key", std::string());
        if (!tables.contains(table) || key.empty() || key.find('"') != std::string::npos || key.find('\n') != std::string::npos)
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "plugins_set: a change of an unknown table or key");
        put("plugins." + table, "\"" + key + "\"", cluster::SettingKind::Bool, change.value("value", false) ? "true" : "false");
    }
    if (changes.contains("repos") && changes["repos"].is_array()) {
        std::string list;
        for (const cluster::Json& r: changes["repos"])
            if (r.is_string() && !cluster::trim(r.get<std::string>()).empty() && r.get<std::string>().find(',') == std::string::npos)
                list += (list.empty() ? "" : ",") + cluster::trim(r.get<std::string>());
        put("plugins", "repos", cluster::SettingKind::List, list);
    }
    if (changes.contains("sync_hours")) put("plugins", "sync_hours", cluster::SettingKind::Int, std::to_string(std::max(0, changes.value("sync_hours", 24))));
    if (writes.empty()) return;
    cluster::settings_write(this->config(), writes);
    (void)this->config().reload();
    ++this->_version;
}

_cold void cluster::Manager::pluginsSync_(void)
{
    // the repositories changed (or the first tick): cloned / pulled and listed, then the ticks applied; the ticks only: applied
    // a source asked (pulled now), the auto ones every sync_hours (and at the start of the core), a new one cloned
    const cluster::Config& config = this->config();
    const std::int64_t now = cluster::now();
    const int hours = config.pluginSyncHours;
    const bool due = hours > 0 && this->_pluginSyncAt > 0 && now - this->_pluginSyncAt >= static_cast<std::int64_t>(hours) * 3600;
    bool asked = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        asked = !this->_pluginPull.empty();
    }
    const bool repos = !this->_pluginsSeen || config.pluginRepos != this->_pluginRepos || asked || due;
    const bool select = config.pluginSelect != this->_pluginSelect || config.pluginOff != this->_pluginOff || config.pluginPaused != this->_pluginPaused;
    if (!repos && !select) return;
    if (this->_pluginsBusy.exchange(true)) return; // the next tick again
    const bool first = !this->_pluginsSeen;
    this->_pluginsSeen = true;
    this->_pluginRepos = config.pluginRepos;
    this->_pluginSelect = config.pluginSelect;
    this->_pluginOff = config.pluginOff;
    this->_pluginPaused = config.pluginPaused;
    std::set<std::string> pull;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        pull.swap(this->_pluginPull);
    }
    if (first || due) { // the ones synced by themselves
        for (const std::string& url: config.pluginRepos)
            if (cluster::plugin_auto(config.pluginAuto, cluster::plugin_slug(url))) pull.insert(cluster::plugin_slug(url));
        this->_pluginSyncAt = now;
    }
    const cluster::PluginSetup setup{config.pluginRepos, config.pluginSelect, config.pluginOff, config.pluginPaused, config.command("claude")};
    if (first && setup.repos.empty() && setup.select.empty()) {
        this->_pluginsBusy = false;
        return;
    }
    cluster::guarded([this, setup, repos, pull]() {
        try {
            if (repos) {
                std::size_t count = 0;
                std::string errors;
                for (const cluster::PluginRepo& repo: cluster::plugins_scan(setup, pull)) {
                    count += repo.items.size();
                    if (!repo.error.empty()) errors += " " + repo.slug + ": " + repo.error + ";";
                }
                this->notice_("", "plugins: " + std::to_string(setup.repos.size()) + " repositor" + (setup.repos.size() == 1 ? "y" : "ies") + ", "
                    + std::to_string(count) + " plugin(s) / skill(s) offered (settings > Plugins)" + errors, !errors.empty());
            }
            const std::string done = cluster::plugins_apply(setup);
            if (!done.empty()) this->notice_("", "plugins: " + done + " (for the sessions started from now)", done.find("failed:") != std::string::npos);
        } catch (const std::exception& e) {
            this->notice_("", std::string("plugins: ") + e.what(), true);
        }
        ++this->_version;
        this->_pluginsBusy = false;
    }).detach();
}

_cold void cluster::Manager::adoptProvider_(cluster::SessionSpec& spec) const
{
    // [agent] provider is the one of every session: a session of the last run moves to it (its model too, the models differ
    // between providers); the ones of another driver keep theirs (their conversation can only be resumed by their own driver)
    try {
        const std::string provider = this->config().provider;
        const cluster::Backend wanted = this->_auth.resolve(provider);
        const cluster::Backend current = this->_auth.resolve(spec.backend);
        if (wanted.provider.name == current.provider.name || wanted.provider.driver != current.provider.driver) return;
        spec.backend = provider;
        spec.model = wanted.model;
    } catch (const utils::exception::IException&) {} // an unknown provider: left as it was, told when it starts
}

_cold void cluster::Manager::load(void)
{
    this->startOrchestrator_(); // the heavy jobs of the sessions: queued, limited (its saved jobs settled)
    this->loadTaxonomy_();      // the forest of the categories / tags
    if (this->config().historyLimit > 0) // the prompt history kept to its limit at the start
        (void)cluster::keep_newest(cluster::data_dir() / "history.jsonl", static_cast<std::size_t>(this->config().historyLimit), 0);
    const std::optional<cluster::Json> state = cluster::read_json(state_file_());
    if (!state || !state->is_object()) return;
    // one item of another shape (edited by hand, an infinite number written as null by a version before) is left out, told: never the end of the core, every session unreachable
    std::vector<std::string> bad;
    const std::function<void(const std::string&, const std::function<void(void)>&)> read = [&bad](const std::string& what, const std::function<void(void)>& item) {
        try {
            item();
        } catch (const std::exception& e) {
            bad.push_back(what + ": " + e.what());
        }
    };
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        read("next", [&]() {this->_next = state->value("next", 1);});
        read("next_task", [&]() {this->_nextTask = state->value("next_task", 1);});
        read("next_schedule", [&]() {this->_nextSchedule = state->value("next_schedule", 1);});
        for (const cluster::Json& w: state->value("schedules", cluster::Json::array()))
            read("a schedule", [&]() {this->_schedules.push_back(w.get<cluster::Schedule>());});
        for (const cluster::Json& spec: state->value("sessions", cluster::Json::array()))
            read("a session", [&]() {
                this->_previous.push_back(spec_from_json_(spec));
                if (!this->_previous.back().global) this->adoptProvider_(this->_previous.back());
            });
        for (const cluster::Json& spec: state->value("trash", cluster::Json::array()))
            read("a session of the trash", [&]() {this->_trash.push_back(spec_from_json_(spec));});
        for (const cluster::Json& t: state->value("tasks", cluster::Json::array()))
            read("a task", [&]() {
                cluster::Task task = t.get<cluster::Task>();
                if (task.status == "running") task.status = "queued"; // interrupted by the end of the previous run
                this->_tasks.push_back(task);
            });
        read("ui", [&]() {this->_ui = state->value("ui", std::map<std::string, std::string>());});
        read("replies", [&]() {this->_replies = state->value("replies", std::map<std::string, int>());});
        read("followed", [&]() {this->_followed = state->value("followed", std::map<std::string, std::set<std::string>>());});
        read("one_shot", [&]() {this->_oneShot = state->value("one_shot", std::set<std::string>());});
        if (state->contains("queued") && (*state)["queued"].is_object() && !(*state)["queued"].empty()) // sent again once their session runs (handoff_)
            this->_handoff = {{"turns", cluster::Json::array()}, {"queued", (*state)["queued"]}, {"at", cluster::now()}};
        // a counter left out: the next ids still never one of a session, a task or a schedule kept
        for (const std::vector<cluster::SessionSpec>* specs: {&this->_previous, &this->_trash})
            for (const cluster::SessionSpec& spec: *specs)
                next_past_(this->_next, spec.id, 's');
        for (const cluster::Task& task: this->_tasks)
            next_past_(this->_nextTask, task.id, 't');
        for (const cluster::Schedule& w: this->_schedules)
            next_past_(this->_nextSchedule, w.id, 'w');
    }
    if (bad.empty()) return;
    std::error_code error;
    std::filesystem::copy_file(state_file_(), cluster::data_dir() / "state.json.bad", std::filesystem::copy_options::overwrite_existing, error); // the next save drops them: kept once
    for (const std::string& what: bad)
        cluster::debug("core", "state.json, left out: " + what);
    this->notice_("", std::to_string(bad.size()) + " item(s) of state.json not read, left out (" + bad.front() + "); a copy in state.json.bad", true);
}

_cold void cluster::Manager::save(void) const
{
    const std::lock_guard<std::mutex> saving(this->_saveMutex); // called from every thread: the last state written last
    cluster::Json state = {{"version", 1}};
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        state["next"] = this->_next;
        state["next_task"] = this->_nextTask;
        state["next_schedule"] = this->_nextSchedule;
        state["schedules"] = cluster::Json::array();
        for (const cluster::Schedule& w: this->_schedules)
            state["schedules"].push_back(w);
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
            state["tasks"].push_back(t);
        state["ui"] = this->_ui;
        // the callbacks still owed (who waits for which session): kept over a restart of the core
        state["replies"] = this->_replies;
        state["followed"] = this->_followed;
        state["one_shot"] = this->_oneShot;
        // the prompts waiting in the queues (and the ones of the last run not given back yet): a quit / crash keeps them, sent again
        cluster::Json queued = this->_handoff.is_object() && this->_handoff.contains("queued") && this->_handoff["queued"].is_object() ? this->_handoff["queued"] : cluster::Json::object();
        for (const auto &[id, session]: this->_sessions) {
            const std::vector<cluster::Queued> waiting = session->waiting();
            if (!waiting.empty()) queued[id] = waiting;
        }
        if (!queued.empty()) state["queued"] = queued;
    }
    try {
        cluster::write_json(state_file_(), state);
    } catch (const utils::exception::IException& e) { // the state is saved again at the next change
        cluster::debug("core", std::string("the state not saved: ") + e.info());
    }
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
    const std::uint64_t shown = restore && !previous.empty() ? this->progressBegin("Restoring the previous run (" + std::to_string(previous.size()) + " session" + (previous.size() == 1 ? "" : "s") + ")") : 0;
    std::size_t done = 0;
    for (cluster::SessionSpec& spec: previous) {
        if (shown != 0) this->progressStep(shown, "resuming " + std::to_string(++done) + " / " + std::to_string(previous.size()) + ": " + spec.name + " (" + cluster::short_path(spec.cwd) + ")");
        if (!restore) {
            spec.deleted = cluster::now();
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_trash.push_back(spec);
            continue;
        }
        try {
            const std::shared_ptr<cluster::Session> session = this->make_(spec);
            session->loadHistory();
            {
                std::lock_guard<std::mutex> lock(this->_mutex);
                this->_sessions[spec.id] = session;
                this->_order.push_back(spec.id);
            }
            session->start();
            if (shown != 0) this->progressLog(shown, "✓ " + spec.name + " · " + spec.backend);
        } catch (const std::exception& e) { // one session: never the others, never the start of the core
            const std::string why = why_(e);
            this->notice_(spec.id, spec.name + " not restored: " + why, true);
            if (shown != 0) this->progressLog(shown, "✗ " + spec.name + ": " + why);
        }
    }
    if (shown != 0) this->progressEnd(shown, true, std::to_string(previous.size()) + " session(s) restored", false);
    if (!previous.empty()) this->notice_("", std::to_string(previous.size()) + (restore ? " session(s) restored" : " session(s) moved to the trash"));
    this->save();
}

_cold void cluster::Manager::shutdown(void)
{
    if (this->_replica) { // an interface: the core goes on
        this->detach_();
        return;
    }
    // closing: the agents that end now are not "stopped by themselves" (no notice, no restart)
    this->_closing = true;
    if (this->_orch) this->_orch->stop(); // its state saved, the jobs left to settle at the next start
    if (this->_statsUp > 0) this->statsRecord("stop");
    {
        std::lock_guard<std::mutex> lock(this->_describer->mutex); // a description being written: dropped
        this->_describer->alive = false;
    }
    std::vector<std::shared_ptr<cluster::Session>> sessions;
    {
        // the queues kept aside first: a session stopped empties its own, and a save after it must still write them
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (!this->_handoff.is_object()) this->_handoff = {{"turns", cluster::Json::array()}, {"queued", cluster::Json::object()}, {"at", cluster::now()}};
        for (const auto &[id, session]: this->_sessions) {
            sessions.push_back(session);
            const std::vector<cluster::Queued> waiting = session->waiting();
            if (!waiting.empty()) this->_handoff["queued"][id] = waiting;
        }
    }
    this->save();
    for (const std::shared_ptr<cluster::Session>& session: sessions) {
        session->retire(true); // a prompt being sent: refused, never an agent left behind
        session->stop();
    }
}

_cold void cluster::Manager::tick(void)
{
    if (this->_replica) return; // the core does it
    // the face changed (a turn ended, its happy moment over, an error, a question): the interfaces redraw now
    const cluster::AppFace face = this->appFace(false);
    const int key = static_cast<int>(face.face) * 100000 + face.badge * 2 + (face.urgent ? 1 : 0);
    if (this->_faceKey.exchange(key) != key) ++this->_version;
    static int seconds = 0;
    this->ensureGlobal_();
    if (this->_statsUp == 0) this->statsRecord("start");
    if (cluster::now() - this->_statsUp >= 60) {
        this->_statsUp = cluster::now();
        this->statsRecord("up");
        (void)cluster::stats_refresh(*this);
    }
    if (this->config().reload()) {
        if (this->config().error().empty()) this->notice_("", "config reloaded");
        else this->notice_("", "config error (previous values kept): " + this->config().error(), true);
    }
    this->pluginsSync_();
    // the previous run not decided yet (its question waits, its answer runs): the sessions they name are not back, a task would fail on them, a schedule be consumed
    const bool waiting = this->_restoreAsk || !this->_startDecided;
    if (!this->_holding && !waiting) { // an update switching: no task, group or schedule starts a turn (they go on with the new version)
        this->schedule_();
        this->groupsTick_();
        this->fireSchedules_();
        std::vector<std::shared_ptr<cluster::Session>> sessions; // the ends of tasks no turn took
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (const auto &[id, session]: this->_sessions)
                sessions.push_back(session);
        }
        for (const std::shared_ptr<cluster::Session>& session: sessions)
            session->lateWakes();
    }
    if (!this->_heldReleased && !this->_holding && !waiting && seconds >= 3) { // the prompts the version before held: sent once its sessions are back
        this->_heldReleased = true;
        this->releaseHeld_();
    }
    if (++seconds % 300 == 1) this->purge();
    if (seconds % 3 == 0) this->upgradeCheck_();
    if (seconds % 5 == 0) cluster::accounts_status_set(this->accountsJson());
    const cluster::Config& config = this->config();
    if (seconds % 5 == 2 && config.keepAwake != "off") { // the computer kept from sleeping by itself (the idle suspend only)
        bool working = config.keepAwake == "always";
        for (const cluster::Snapshot& s: this->list(true))
            working = working || s.state == cluster::State::Working || s.queued > 0;
        cluster::keep_awake(working);
    }
    {
        // the prompts queued while a turn runs: given to claude after a moment (its next step)
        std::vector<std::shared_ptr<cluster::Session>> sessions;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (const auto &[id, session]: this->_sessions)
                sessions.push_back(session);
        }
        for (const std::shared_ptr<cluster::Session>& session: sessions) {
            session->giveQueued(config.queueGiven == "end" ? -1 : 1500);
            if (seconds % 60 == 17 && config.transcriptLimitMb > 0) session->limitLog(static_cast<std::uintmax_t>(config.transcriptLimitMb) * 1024 * 1024);
        }
        if (seconds % 60 == 19) this->limitMemory_(sessions);
        static std::int64_t warmAt = 0; // by time, not by the second of the tick: a slow tick would skip its second and a narrow window with it
        if (cluster::now() - warmAt >= 10) {
            warmAt = cluster::now();
            this->keepWarm_();
        }
    }
    if (seconds % 60 == 2) this->scanTranscripts_();        // the transcripts of Claude Code: what grew
    if (seconds % 30 == 3) this->refreshAccount_();         // the windows of the account (read from the shared copy, fetched once too old)
    if (seconds == 5) this->describeMissing_();
}

std::vector<cluster::Snapshot> cluster::Manager::list(const bool withGlobal) const
{
    if (this->_replica) {
        std::vector<cluster::Snapshot> all = this->replicaSessions_();
        if (!withGlobal) std::erase_if(all, [](const cluster::Snapshot& s) {return s.spec.global;});
        return all;
    }
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
    if (withGlobal && this->config().globalEnabled && (snapshots.empty() || !snapshots.front().spec.global)) {
        // the global session is always in the list: not there yet (loading, waiting for the answer to the previous-run question, started
        // again after a crash), a placeholder says what it is doing - never an empty interface, never sessions without it
        cluster::Snapshot placeholder;
        placeholder.spec.id = GLOBAL_ID;
        placeholder.spec.name = "global";
        placeholder.spec.global = true;
        placeholder.spec.backend = this->config().globalBackend;
        placeholder.spec.cwd = this->globalDir_().string();
        placeholder.spec.panels = {"tokens", "context", "cost", "usage"};
        const bool crashed = this->_globalRetrying.load() || this->_globalEnsured.load() > 0;
        const bool asking = this->_restoreAsk.load() || this->restorable();
        placeholder.state = crashed ? cluster::State::Error : cluster::State::Starting;
        const std::string what = asking ? "waiting for your answer (restore the previous sessions: r / n / q)" : crashed ? "not running: started again…" : "loading…";
        placeholder.entries.push_back({cluster::EntryKind::System, "global session: " + what, "", cluster::now()});
        placeholder.warning = "global session " + what;
        placeholder.warningAt = cluster::now();
        snapshots.insert(snapshots.begin(), std::move(placeholder));
    }
    return snapshots;
}

std::vector<cluster::Snapshot> cluster::Manager::listLight(const bool withGlobal) const
{
    // the sessions without their entries: what reads only their names, labels, states (the actions, the menus) never copies them
    if (this->_replica) return this->replicaLight_(withGlobal);
    std::vector<cluster::Snapshot> out;
    std::vector<std::shared_ptr<cluster::Session>> sessions;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto global = this->_sessions.find(GLOBAL_ID);
        if (withGlobal && global != this->_sessions.end()) sessions.push_back(global->second);
        for (const std::string& id: this->_order)
            sessions.push_back(this->_sessions.at(id));
    }
    for (const std::shared_ptr<cluster::Session>& session: sessions)
        out.push_back(session->snapshotLight());
    return out;
}

std::optional<cluster::Snapshot> cluster::Manager::snapshot(const std::string& id) const
{
    if (this->_replica) return this->replicaFind_(id); // never every session copied to find one
    try {
        return this->get_(id)->snapshot();
    } catch (const utils::exception::IException&) {
        return std::nullopt;
    }
}

std::vector<cluster::SessionSpec> cluster::Manager::trash(void) const
{
    if (this->_replica) return this->replicaState("trash").get<std::vector<cluster::SessionSpec>>();
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_trash;
}

_cold void cluster::Manager::scanMachine(void)
{
    if (this->_replica) {
        (void)this->call({{"cmd", "machine_scan"}});
        return;
    }
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
    cluster::guarded([this]() {
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
            const std::size_t repos = cluster::machine_scan(this->config(), report);
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
    if (this->_replica) {
        const cluster::Json machine = this->replicaState("machine");
        if (!machine.is_object()) return std::nullopt;
        return std::pair<cluster::MachineProgress, std::int64_t>{machine["progress"].get<cluster::MachineProgress>(), machine.value("end", std::int64_t{0})};
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_scan) return std::nullopt;
    return std::make_pair(*this->_scan, this->_scanEnd);
}

int cluster::Manager::machineAsk(void) const
{
    if (this->_replica) return this->replicaState("machine_ask").is_number() ? this->replicaState("machine_ask").get<int>() : 0;
    const int days = cluster::machine_days_old();
    if (days < 0) return this->ui("machine", "") != "no" ? 1 : 0;
    const int every = this->config().machineDays;
    if (every <= 0 || days < every) return 0;
    std::int64_t later = 0;
    try {
        later = std::stoll(this->ui("machine_later", "0"));
    } catch (const std::exception&) {} // a value of another shape in state.json: never answered
    return cluster::now() - later > static_cast<std::int64_t>(every) * 86400 ? 2 : 0;
}

_cold void cluster::Manager::restartGlobal(void)
{
    if (this->_replica) {
        (void)this->call({{"cmd", "restart_global"}});
        return;
    }
    (void)this->replace_(GLOBAL_ID, [](cluster::SessionSpec&) {}, false);
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

_cold static bool play_sound_(const std::string& sound)
{
    // a sound of the desktop theme: by what the desktop plays them with, else its file by the sound server; false: nothing here can play it
    if (cluster::has_command("canberra-gtk-play")) {
        cluster::detach({"canberra-gtk-play", "-i", sound});
        return true;
    }
    const std::string file = "/usr/share/sounds/freedesktop/stereo/" + sound + ".oga";
    if (!std::filesystem::exists(file)) return false;
    const std::string player = cluster::has_command("pw-play") ? "pw-play" : "paplay";
    if (!cluster::has_command(player)) return false;
    cluster::detach({player, file});
    return true;
}

_cold std::string cluster::Manager::playSound(const std::string& name)
{
    if (this->_replica) return this->call({{"cmd", "play_sound"}, {"sound", name}}).value("message", std::string());
    static constexpr const char* NOTHING = "nothing here can play it: canberra-gtk-play (libcanberra-gtk3), or pw-play / paplay and the sounds of the desktop (sound-theme-freedesktop)";
    if (name == "all") {
        if (!play_sound_(cluster::notify_sounds().front().first))
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, NOTHING);
        cluster::guarded([]() {
            for (std::size_t at = 1; at < cluster::notify_sounds().size(); ++at) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                (void)play_sound_(cluster::notify_sounds()[at].first);
            }
        }).detach();
        return "every sound, one after the other, on this computer";
    }
    for (const auto &[sound, when]: cluster::notify_sounds())
        if (sound == name) {
            if (!play_sound_(sound)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, NOTHING);
            return "played on this computer: " + sound + " (" + when + ")";
        }
    throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no sound '" + name + "'");
}

bool cluster::Manager::notify(const cluster::Notification& n, const std::string& kind)
{
    // failsafe: no notification daemon, no desktop... nothing said; by DBus (gdbus) when it is there - its clicks come
    // back with their activation token (the window takes the focus with it); else notify-send, its buttons waited for
    if (n.face == cluster::Face::Grin || n.face == cluster::Face::Happy) this->_moment = {n.face, cluster::now()};
    this->pushNotice_(n, kind);
    const bool demo = kind == "showcase"; // the Debug page: shown whatever the settings (what a notification looks like)
    const cluster::Config& c = this->config();
    if (!c.notify && !demo) return false;
    const std::map<std::string, bool> kinds = {{"global", c.notifyGlobal}, {"done", c.notifyDone}, {"permission", c.notifyPermission}, {"question", c.notifyQuestion},
        {"error", c.notifyError}, {"limit", c.notifyLimits}, {"web", c.notifyWeb}, {"task", c.notifyTasks}, {"reminder", c.notifyReminders},
        {"ready", c.notifyReady}};
    if (kinds.contains(kind) && !kinds.at(kind)) return false;
    // the last prompt of this session came from a web device: its page (or its app) notifies, not the desktop here
    if (kind != "web" && kind != "ready" && !n.session.empty() && this->origin(n.session).starts_with("web:")) return false;
    if (c.notifyWhen != "always" && this->_focused == 1 && !demo) return false; // in front of the user: its popups there
    const std::string key = n.session.empty() ? "-" : n.session;
    if (c.notifySound && !n.sound.empty()) (void)play_sound_(n.sound);
    static const bool dbus = cluster::has_command("gdbus");
    if (dbus) {
        // org.freedesktop.Notifications.Notify: the one of this session replaced, the body clickable (default), its buttons
        std::string replaces = "0";
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (this->_notifyIds.contains(key)) replaces = this->_notifyIds.at(key);
        }
        const std::function<std::string(const std::string&)> quoted = [](const std::string& text) {
            std::string out = "'";
            for (const char ch: text)
                out += ch == '\'' ? std::string("\\'") : ch == '\\' ? std::string("\\\\") : std::string(1, ch);
            return out + "'";
        };
        std::string actions = "[" + quoted("default") + ", " + quoted("Open");
        for (const auto &[action, label]: n.actions)
            actions += ", " + quoted(action) + ", " + quoted(label);
        actions += "]";
        const std::string hints = "{'desktop-entry': <'claude-cluster'>, 'urgency': <byte " + std::string(n.urgent ? "2" : "1") + ">}";
        const cluster::Captured got = cluster::capture({"gdbus", "call", "--session", "--dest", "org.freedesktop.Notifications", "--object-path", "/org/freedesktop/Notifications",
            "--method", "org.freedesktop.Notifications.Notify", "--", "claude-cluster", replaces, cluster::face_file(n.face).string(), n.title, markup_(cluster::one_line(n.body, 400)),
            actions, hints, n.urgent ? "0" : "-1"}, "", 10); // "--": the -1 (the default timeout) is an argument, never read as an option
        std::string id;
        for (const char ch: got.out)
            if (ch >= '0' && ch <= '9') id += ch;
            else if (!id.empty()) break;
        if (got.code != 0 || id.empty()) return false;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_notifyIds[key] = id;
            this->_notifyShown[id] = {n, ""};
            if (this->_notifyShown.size() > 50) this->_notifyShown.erase(this->_notifyShown.begin());
        }
        if (!this->_notifyMonitorUp.exchange(true)) {
            if (this->_notifyMonitor.joinable()) this->_notifyMonitor.join(); // gdbus monitor ended (the session bus restarted...): that thread has just ended; assigned over it: std::terminate
            this->_notifyMonitor = cluster::guarded(&cluster::Manager::notifyMonitor_, this);
        }
        return true;
    }
    std::vector<std::string> args = {"notify-send", "-a", "claude-cluster", "-i", cluster::face_file(n.face).string(), "-h", "string:desktop-entry:claude-cluster",
        "-u", n.urgent ? "critical" : "normal", "-p"};
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_notifyIds.contains(key)) args.insert(args.end(), {"-r", this->_notifyIds.at(key)});
    }
    args.insert(args.end(), {"-A", "default=Open"}); // its body clicked: its session, the interface to the front
    for (const auto &[action, label]: n.actions)
        args.insert(args.end(), {"-A", action + "=" + label});
    args.push_back("--"); // a title / body starting with "-" (a list of questions) is text, never an option
    args.push_back(n.title);
    args.push_back(markup_(cluster::one_line(n.body, 400)));
    cluster::guarded([this, args, n, key]() {
        const cluster::Captured out = cluster::capture(args, "", 3600); // its buttons (and its body): waited for
        std::string chosen;
        for (const std::string& raw: cluster::split(out.out, '\n')) {
            const std::string line = cluster::trim(raw);
            if (!line.empty() && std::all_of(line.begin(), line.end(), [](const char ch) {return ch >= '0' && ch <= '9';})) {
                std::lock_guard<std::mutex> lock(this->_mutex);
                this->_notifyIds[key] = line;
            } else if (line == "default" || std::any_of(n.actions.begin(), n.actions.end(), [&line](const std::pair<std::string, std::string>& a) {return a.first == line;})) {
                chosen = line;
            }
        }
        if (!chosen.empty()) this->notifyAction_(n, chosen, "");
    }).detach();
    return true;
}

_cold void cluster::Manager::notifyMonitor_(void)
{
    // the signals of the daemon, for the life of the core: ActivationToken (id, token) just before ActionInvoked (id, key)
    int ends[2] = {-1, -1};
    if (::pipe2(ends, O_CLOEXEC) != 0) { // xstyle: ignore LU-SYSCALL (a child that dies with this process: prctl between its fork and its exec)
        this->_notifyMonitorUp = false;
        return;
    }
    const pid_t child = ::fork(); // xstyle: ignore LU-SYSCALL (as above)
    if (child < 0) {
        (void)::close(ends[0]);
        (void)::close(ends[1]);
        this->_notifyMonitorUp = false;
        return;
    }
    if (child == 0) {
        (void)::prctl(PR_SET_PDEATHSIG, SIGKILL); // whatever ends this process, even a crash: never left behind
        (void)::dup2(ends[1], STDOUT_FILENO); // xstyle: ignore LU-SYSCALL (as above)
        const int nothing = ::open("/dev/null", O_WRONLY);
        if (nothing >= 0) (void)::dup2(nothing, STDERR_FILENO); // xstyle: ignore LU-SYSCALL (as above)
        (void)::execlp("gdbus", "gdbus", "monitor", "--session", "--dest", "org.freedesktop.Notifications", static_cast<char*>(nullptr));
        ::_exit(127);
    }
    (void)::close(ends[1]);
    this->_notifyMonitorPid = child;
    if (this->_notifyMonitorStop) (void)::kill(child, SIGKILL); // the manager going while this one started
    const int fd = ends[0];
    std::array<char, 4096> buffer{};
    std::string pending;
    for (;;) {
        const std::size_t end = pending.find('\n');
        if (end == std::string::npos) {
            const ssize_t size = ::read(fd, buffer.data(), buffer.size());
            if (size <= 0) break;
            pending.append(buffer.data(), static_cast<std::size_t>(size));
            continue;
        }
        const std::string line = pending.substr(0, end + 1);
        pending.erase(0, end + 1);
        // "/org/freedesktop/Notifications: org.freedesktop.Notifications.ActionInvoked (uint32 623, 'default')"
        const std::size_t signal = line.find("org.freedesktop.Notifications.");
        if (signal == std::string::npos) continue;
        const std::string rest = line.substr(signal + 30);
        const std::string name = rest.substr(0, rest.find(' '));
        const std::size_t number = rest.find("uint32 ");
        if (number == std::string::npos) continue;
        std::string id;
        for (std::size_t i = number + 7; i < rest.size() && rest[i] >= '0' && rest[i] <= '9'; ++i)
            id += rest[i];
        std::string text;
        if (const std::size_t open = rest.find('\'', number); open != std::string::npos) {
            const std::size_t close = rest.rfind('\'');
            if (close > open) text = rest.substr(open + 1, close - open - 1);
        }
        if (name == "ActivationToken") {
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (this->_notifyShown.contains(id)) this->_notifyShown[id].second = text;
        } else if (name == "ActionInvoked") {
            std::optional<std::pair<cluster::Notification, std::string>> shown;
            {
                std::lock_guard<std::mutex> lock(this->_mutex);
                if (this->_notifyShown.contains(id)) shown = this->_notifyShown[id];
            }
            if (shown) this->notifyAction_(shown->first, text, shown->second);
        } else if (name == "NotificationClosed") {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_notifyShown.erase(id);
        }
    }
    (void)::close(fd);
    this->_notifyMonitorPid = 0;
    (void)::waitpid(child, nullptr, 0);
    this->_notifyMonitorUp = false;
}

_cold void cluster::Manager::notifyAction_(const cluster::Notification& n, const std::string& chosen, const std::string& token)
{
    // its button: its owner (a device to approve...), else the session shown, its permission or question answered;
    // the body (default) or Open: the interface to the front (with the token of the click on Wayland)
    try {
        if (n.onAction && chosen != "default") {
            n.onAction(chosen);
            ++this->_version;
            return;
        }
        if (!n.session.empty() && n.session != "ready" && n.session != "map" && n.session != "voice" && this->snapshot(n.session)) this->requestFocus(n.session);
        if (chosen == "default" || chosen == "open") this->requestRaise(token);
        if (chosen == "allow" || chosen == "deny" || chosen == "always") {
            this->answer(n.session, n.requestId, chosen);
        } else if (chosen == "global") {
            const std::optional<cluster::Ask> ask = this->asking().current();
            if (ask && ask->session == n.session) (void)this->asking().toGlobal();
            else if (const std::optional<cluster::Snapshot> s = this->snapshot(n.session); s && !s->permissions.empty())
                this->askGlobal(n.session, s->permissions.back().requestId, s->permissions.back().input.value("questions", cluster::Json::array()));
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
}

cluster::AppFace cluster::Manager::appFace(const bool listening) const
{
    if (this->_replica) {
        const cluster::Json face = this->replicaState("face");
        cluster::AppFace shown = face.is_object() ? face.get<cluster::AppFace>() : cluster::AppFace{};
        // its own voice listening: attentive, like the core would show it
        if (listening && (shown.face == cluster::Face::Idle || shown.face == cluster::Face::Squint)) shown.face = cluster::Face::Attentive;
        return shown;
    }
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

void cluster::Manager::replyTo(const std::string& id, const std::string& by)
{
    // saved with the state: a callback still owed survives a restart of the core (an update, a crash)
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (by == GLOBAL_ID) ++this->_replies[id];
        if (by != id) this->_followed[id].insert(by); // a task of a sub-session for itself: never told to itself
    }
    this->save();
}

void cluster::Manager::userTouched(const std::string& id)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_oneShot.erase(id);
}

void cluster::Manager::replyUndo_(const std::string& id)
{
    // the prompt that was to be answered was not sent: its callback forgotten
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = this->_replies.find(id);
        if (it != this->_replies.end() && --it->second <= 0) this->_replies.erase(it);
    }
    this->save();
}

void cluster::Manager::replyEnd_(const cluster::Event& event, const std::string& name, const bool all)
{
    // an end of turn (done, error, interrupted, agent exited) of a session the global session started a turn on: one message back to it.
    // A turn no prompt started (a Monitor / background task woke the agent up) of a followed session is answered too.
    // A sub-session that queued a task on it is told its wake-up turns the same way (its first end: by the task).
    bool reply = false;
    bool owed = false;  // a callback owed was given: the saved state follows
    std::vector<std::string> subs;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = this->_replies.find(event.session);
        const auto followers = this->_followed.find(event.session);
        // the callback owed goes with the turn it is for: a turn of the user ending while the prompt of the global session waits in the queue is not its answer
        if (it != this->_replies.end() && it->second > 0 && (event.kind == cluster::Event::Kind::Exited || event.asked || !event.askedLater)) {
            reply = true;
            owed = true;
            if (all || --it->second == 0) this->_replies.erase(it);
        } else if (event.spontaneous && followers != this->_followed.end() && followers->second.contains(GLOBAL_ID)) {
            reply = true;
        }
        if (event.spontaneous && followers != this->_followed.end())
            for (const std::string& by: followers->second)
                if (by != GLOBAL_ID) subs.push_back(by);
    }
    if (owed) this->save();
    if (!reply && subs.empty()) return;
    const std::optional<cluster::Snapshot> snap = this->snapshot(event.session);
    const bool waiting = snap && cluster::background_busy(*snap); // its result not there yet: said, so nobody polls it
    const char* kind = event.kind == cluster::Event::Kind::Exited ? "[stopped: " : event.error ? "[error from " : event.interrupted ? "[interrupted: " : "[reply from ";
    const std::string text = std::string(kind) + name + " (" + event.session + ")]\n"
        + (event.spontaneous && event.kind != cluster::Event::Kind::Exited ? "(a turn woken up by its background task / Monitor done: its result)\n" : "")
        + (waiting && event.kind != cluster::Event::Kind::Exited ? "(its background still runs: its result comes back to you by itself as another [reply from ...] when it is done - no need to poll it)\n" : "")
        + cluster::head_utf8(event.text, 6000);
    for (const std::string& by: subs)
        this->tell_(by, text);
    if (!reply) return;
    const bool finished = !event.error && !event.interrupted; // a one-shot session closed after it (never while its background runs)
    const std::string id = event.session;
    cluster::guarded([this, text, id, finished]() {
        for (int tries = 0;; ++tries) {
            try {
                this->send(GLOBAL_ID, text, true, {}, "mcp");
                break;
            } catch (const utils::exception::IException& e) {
                // the global session not there right now (starting, restarting, its agent stopped): a callback is not dropped for that, 2 minutes for it to come back
                if (tries >= 24 || this->_closing) {
                    this->notice_(GLOBAL_ID, std::string("reply not given to the global session: ") + e.info(), true);
                    return;
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
        if (!finished) return;
        std::this_thread::sleep_for(std::chrono::seconds(2)); // the state settles, a prompt typed just after the answer is seen
        this->archiveOneShot_(id);
    }).detach();
}

void cluster::Manager::archiveOneShot_(const std::string& id)
{
    // a session started for one task, its answer relayed: closed to the trash (restorable) unless it is still of use - another prompt
    // waits, the agent asks something, the user typed in it or shows it right now
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (!this->_oneShot.contains(id) || this->_replies.contains(id) || (this->active() == id && this->_focused != 0)) return;
    }
    if (!this->config().archiveOneShot) return;
    const std::optional<cluster::Snapshot> snap = this->snapshot(id);
    if (!snap || snap->state != cluster::State::Idle || snap->queued > 0 || !snap->permissions.empty() || cluster::background_busy(*snap)) return; // its background
                                                                                     // still runs: its result to come (woken)
    try {
        this->close(id);
    } catch (const utils::exception::IException&) {}
}

_cold void cluster::Manager::showcasePermission(const std::string& id, const cluster::Permission& permission)
{
    this->get_(id)->showcasePermission(permission);
    ++this->_version;
}

_cold void cluster::Manager::showcaseState(const std::string& id, const cluster::State state)
{
    this->get_(id)->showcaseState(state);
    ++this->_version;
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

_cold cluster::Queued cluster::Manager::rewind(const std::string& id, const int back)
{
    if (this->_replica) return this->call({{"cmd", "rewind"}, {"session", id}, {"back", back}}).get<cluster::Queued>();
    std::vector<std::string> dropped;
    const cluster::Queued prompt = this->get_(id)->rewind(back, &dropped);
    if (this->config().stashRewound) // [ui] stash_rewound: the prompts after the one taken back are not lost
        for (const std::string& text: dropped)
            (void)this->stashPush({text, {}, {}}, id, true);
    this->save(); // the cut is kept over a restart (until the next prompt)
    ++this->_version;
    return prompt;
}

bool cluster::Manager::restorable(void) const
{
    if (this->_replica) return this->replicaState("restore_ask") == true;
    std::lock_guard<std::mutex> lock(this->_mutex);
    return std::any_of(this->_previous.begin(), this->_previous.end(), [](const cluster::SessionSpec& s) {return !s.global || !s.claudeId.empty();});
}

_cold void cluster::Manager::answerRestore(const bool restore)
{
    if (this->_replica) {
        (void)this->call({{"cmd", "restore_answer"}, {"restore", restore}});
        return;
    }
    // once: every interface shows the question, each may answer it. _restoreAsk stays on until its answer is done: the tick starts nothing of its own meanwhile
    // (a second global session on a blank spec, a task or a schedule failing on a session not back yet)
    if (this->_restoreAnswered.exchange(true)) return;
    try {
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
                spec.pin = nullptr;
            }
            this->_globalNew = true;
        }
        this->startGlobal();
    } catch (...) {
        this->_startDecided = true;
        this->_restoreAsk = false;
        throw;
    }
    this->_restoreAsk = false;
}

std::vector<cluster::SessionSpec> cluster::Manager::previous(void) const
{
    if (this->_replica) return this->replicaState("previous").get<std::vector<cluster::SessionSpec>>();
    std::lock_guard<std::mutex> lock(this->_mutex);
    std::vector<cluster::SessionSpec> list;
    std::copy_if(this->_previous.begin(), this->_previous.end(), std::back_inserter(list), [](const cluster::SessionSpec& s) {return !s.global;});
    return list;
}

std::vector<cluster::Notice> cluster::Manager::notices(const std::size_t max) const
{
    if (this->_replica) {
        // the ones of the core, and the core not reached (told here)
        std::vector<cluster::Notice> all = this->replicaState("notices").get<std::vector<cluster::Notice>>();
        if (const std::string error = this->replicaError(); !error.empty()) all.push_back({cluster::now(), "", error, true});
        if (all.size() > max) all.erase(all.begin(), all.end() - static_cast<std::ptrdiff_t>(max));
        return all;
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    const std::size_t start = this->_notices.size() > max ? this->_notices.size() - max : 0;
    return std::vector<cluster::Notice>(this->_notices.begin() + static_cast<long>(start), this->_notices.end());
}

std::string cluster::Manager::resolve(const std::string& idOrName) const
{
    if (this->_replica) {
        if (const std::string found = this->replicaResolve_(idOrName); !found.empty()) return found;
        if (idOrName.empty() || idOrName == GLOBAL_ID || idOrName == "global") return GLOBAL_ID;
        throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session '" + idOrName + "'");
    }
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
    if (this->_replica) return this->replicaVersion_() + this->_version;
    // not their sum: a session replaced (a new conversation, a restart: its version from 1 again) or closed made it go down, back to
    // a number an interface had already seen (its list never drawn again: a closed session still shown). What they are now (each
    // one, its object, its version) folded; another fold: the next number, never given before (from the time: after a restart too)
    std::uint64_t fold = 14695981039346656037ULL ^ this->_version;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const auto &[id, session]: this->_sessions)
            fold = (fold ^ (reinterpret_cast<std::uintptr_t>(session.get()) + session->version() * 2654435761ULL)) * 1099511628211ULL;
    }
    std::lock_guard<std::mutex> lock(this->_versionMutex);
    if (this->_versionGiven == 0) this->_versionGiven = static_cast<std::uint64_t>(cluster::now_ms());
    if (fold != this->_versionSeen) {
        this->_versionSeen = fold;
        ++this->_versionGiven;
    }
    return this->_versionGiven;
}

std::string cluster::Manager::ui(const std::string& key, const std::string& fallback) const
{
    if (this->_replica) {
        const cluster::Json ui = this->replicaState("ui");
        return ui.is_object() && ui.contains(key) && ui[key].is_string() ? ui[key].get<std::string>() : fallback;
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    auto it = this->_ui.find(key);
    return it != this->_ui.end() ? it->second : fallback;
}

void cluster::Manager::setUi(const std::string& key, const std::string& value)
{
    if (this->_replica) {
        (void)this->call({{"cmd", "ui_set"}, {"key", key}, {"value", value}});
        // read at once by the next step (the apply: popup after a model / mode / effort picked): not the sync of the core, a poll later
        this->replicaPatchUi(key, value);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_ui[key] = value;
    }
    ++this->_version;
    this->save();
}
