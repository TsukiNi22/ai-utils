/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Manager-Tasks.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Manager.hpp"
#include "cluster/Options.hpp"
#include "cluster/Policy.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Cron.hpp"
#include "cluster/Wire.hpp"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <sstream>
#include <ctime>
#include <regex>
#include <set>
#include <map>

/* helpers */
namespace {

namespace opt = cluster::opt;

// the model / thinking of a session a task starts: the policy of the user applies like for session_spawn (a costly one is dropped, not asked)
_cold void spawn_policy_(const cluster::Config& config, cluster::TaskSpawn& spawn)
{
    const cluster::PolicyConf& policy = config.policy;
    const std::size_t slash = spawn.backend.find('/');
    std::string model = slash == std::string::npos ? std::string() : spawn.backend.substr(slash + 1);
    const std::string provider = slash == std::string::npos ? spawn.backend : spawn.backend.substr(0, slash);
    if (model.empty() && !policy.defaultModel.empty() && policy.modelMode != "free" && provider.find('/') == std::string::npos) {
        const std::string base = provider.empty() ? config.backend.substr(0, config.backend.find('/')) : provider;
        if (base == "claude" || base == "anthropic" || base == "anthropic-api") {
            model = policy.defaultModel;
            spawn.backend = base + "/" + model;
        }
    }
    if (spawn.effort.empty() && !policy.defaultEffort.empty() && policy.effortMode != "free") spawn.effort = policy.defaultEffort;
    const cluster::PolicyVerdict verdict = cluster::policy_check(config, model, spawn.effort, "", "");
    if (verdict.kind == "refuse") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "refused by the policy: " + verdict.reason);
    if (verdict.kind != "ask") return;
    if (cluster::policy_costly_model(config, model)) { // the user decides a costly one, never a task: the default of the policy instead
        spawn.backend = spawn.backend.substr(0, spawn.backend.find('/'));
        if (!policy.defaultModel.empty() && policy.modelMode != "free" && !cluster::policy_costly_model(config, policy.defaultModel)) spawn.backend += "/" + policy.defaultModel;
    }
    if (cluster::policy_costly_effort(config, spawn.effort)) spawn.effort.clear();
}

} // namespace

/* the MCP of a sub-session */
_cold std::vector<std::string> cluster::Manager::subArgs_(const std::string& id) const
{
    // its own server (CLAUDE_CLUSTER_SESSION: the tools of the sub-sessions, its tasks / schedules only); the settings [agent] sub_* open it
    if (!this->_config.subRead && !this->_config.subTasks && !this->_config.subSchedules && !this->_config.orch.enabled) return {};
    const std::filesystem::path path = cluster::socket_path().parent_path() / ("mcp-" + id + ".json");
    try {
        cluster::write_json(path, {{"mcpServers", {{"claude-cluster", {{"type", "stdio"}, {"command", cluster::self_exe().string()}, {"args", {"mcp"}},
            {"env", {{"CLAUDE_CLUSTER_SOCKET", cluster::socket_path().string()}, {"CLAUDE_CLUSTER_SESSION", id}}}}}}}});
    } catch (const utils::exception::IException&) {
        return {};
    }
    return {"--mcp-config", path.string(), "--allowedTools", "mcp__claude-cluster"};
}

/* tasks */
_cold std::string cluster::Manager::addTask(const std::string& prompt, const std::string& target, const std::vector<std::string>& after, const std::string& by)
{
    return this->addTaskJson({{"prompt", prompt}, {"target", target}, {"after", after}}, by);
}

_cold std::size_t cluster::Manager::spawnedBy_(const std::string& by) const
{
    // the sessions its tasks start: waiting to, running, or still open after (the lock held)
    std::size_t count = 0;
    for (const cluster::Task& t: this->_tasks) {
        if (t.by != by) continue;
        const bool open = t.spawned && this->_sessions.contains(t.session);
        if (open || ((t.status == "queued" || t.status == "running") && !t.spawn.cwd.empty())) ++count;
    }
    return count;
}

_cold std::string cluster::Manager::addTaskJson(const cluster::Json& o, const std::string& by)
{
    cluster::Task task;
    task.prompt = opt::text(o, "prompt");
    if (cluster::trim(task.prompt).empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "empty task");
    task.target = opt::text(o, "target");
    task.after = opt::list(o, "after");
    task.afterSessions = opt::list(o, "after_sessions");
    task.by = by;
    task.created = cluster::now();
    task.priority = static_cast<int>(std::clamp<std::int64_t>(opt::number(o, "priority", 0), -100, 100));
    task.timeout = static_cast<int>(std::clamp<std::int64_t>(opt::number(o, "timeout_minutes", 0), 0, 100000));
    task.retry = static_cast<int>(std::clamp<std::int64_t>(opt::number(o, "retry", 0), 0, 10));
    task.retryDelay = static_cast<int>(std::clamp<std::int64_t>(opt::number(o, "retry_delay_minutes", 0), 0, 10000));
    task.maxConcurrency = static_cast<int>(std::clamp<std::int64_t>(opt::number(o, "max_concurrency", 0), 0, 50));
    task.lane = opt::text(o, "lane");
    task.maxCost = std::max(0.0, opt::decimal(o, "max_cost"));
    task.maxTokens = std::max<std::int64_t>(0, opt::number(o, "max_tokens", 0));
    task.expect = cluster::trim(opt::text(o, "expect"));
    task.notify = opt::flag(o, "notify", false);
    task.quiet = !opt::flag(o, "reply", true);
    task.schedule = opt::text(o, "schedule");
    if (o.contains("not_before")) task.notBefore = opt::number(o, "not_before", 0);
    // when it starts besides its dependencies: an event
    if (const auto it = o.find("on_event"); it != o.end() && it->is_object() && !opt::text(*it, "type").empty()) {
        task.on.type = cluster::lower(opt::text(*it, "type"));
        if (task.on.type == "session_idle") task.on.arg = opt::text(*it, "session");
        else if (task.on.type == "git_push") {
            task.on.arg = opt::text(*it, "repo");
            task.on.arg2 = opt::text(*it, "branch");
        } else if (task.on.type == "command") task.on.arg = opt::text(*it, "command");
        else if (task.on.type == "file") {
            task.on.arg = opt::text(*it, "path");
            task.on.arg2 = opt::flag(*it, "changed", false) ? "changed" : "";
            if (task.on.arg2 == "changed") task.on.base = std::to_string(opt::mtime(task.on.arg));
        } else {
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "on_event type: session_idle | git_push | command | file");
        }
        if (cluster::trim(task.on.arg).empty())
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "on_event " + task.on.type + ": its "
                + std::string(task.on.type == "session_idle" ? "session" : task.on.type == "git_push" ? "repo" : task.on.type == "command" ? "command" : "path") + " is missing");
    }
    // its own session
    if (const auto it = o.find("spawn"); it != o.end() && it->is_object() && !opt::text(*it, "cwd").empty()) {
        task.spawn = {opt::text(*it, "cwd"), opt::text(*it, "name"), opt::text(*it, "backend"), opt::text(*it, "mode"), opt::text(*it, "effort"), opt::text(*it, "profile"), opt::flag(*it, "one_shot", false)};
        if (!task.target.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "spawn and target: one of them (a task starts its own session, or looks for one)");
        (void)cluster::resolve_folder(task.spawn.cwd); // a folder (or a project of the map) that exists, said now
        spawn_policy_(this->_config, task.spawn);
    }
    // {{tN.result}} in the prompt: its task done first
    static const std::regex reference(R"(\{\{\s*(t\d+)\.\w+\s*\}\})");
    for (std::sregex_iterator it(task.prompt.begin(), task.prompt.end(), reference), end; it != end; ++it)
        if (std::find(task.after.begin(), task.after.end(), (*it)[1].str()) == task.after.end()) task.after.push_back((*it)[1].str());
    std::string id;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const std::string& dep: task.after)
            if (std::none_of(this->_tasks.begin(), this->_tasks.end(), [&](const cluster::Task& t) {return t.id == dep;}))
                throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no task '" + dep + "'");
        // the task queue: the tasks waiting for a free place, bounded
        const std::size_t waiting = std::count_if(this->_tasks.begin(), this->_tasks.end(), [](const cluster::Task& t) {return t.status == "queued";});
        if (waiting >= static_cast<std::size_t>(std::max(1, this->_config.taskQueue)))
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the task queue is full (" + std::to_string(waiting)
                + " waiting: [sessions] task_queue)");
        if (!by.empty() && by != GLOBAL_ID && !task.spawn.cwd.empty() && this->spawnedBy_(by) >= static_cast<std::size_t>(this->_config.subSpawnCap))
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the cap of sessions started by your tasks / schedules is reached ("
                + std::to_string(this->_config.subSpawnCap) + ": [agent] sub_spawn_cap): wait for one to end, or reuse a session");
        id = "t" + std::to_string(this->_nextTask++);
        task.id = id;
        this->_tasks.push_back(task);
    }
    this->notice_("", "task " + id + " queued: " + cluster::one_line(task.prompt, 80));
    this->save();
    this->schedule_();
    return id;
}

_cold void cluster::Manager::cancelTask(const std::string& id)
{
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_tasks.begin(), this->_tasks.end(), [&](const cluster::Task& t) {return t.id == id;});
        if (it == this->_tasks.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no task '" + id + "'");
        if (it->status == "queued" || it->status == "running") {
            it->status = "cancelled";
            it->ended = cluster::now();
        }
    }
    this->save();
}

_cold std::vector<cluster::Task> cluster::Manager::tasks(void) const
{
    if (this->_replica) return this->replicaState("tasks").get<std::vector<cluster::Task>>();
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_tasks;
}

_cold cluster::Json cluster::Manager::taskJson(const cluster::Task& t) const
{
    cluster::Json out = {{"id", t.id}, {"prompt", cluster::one_line(t.prompt, 200)}, {"target", t.target}, {"after", t.after}, {"status", t.status}, {"session", t.session},
        {"group", t.group}, {"by", t.by}, {"reason", t.reason}, {"archived", t.archived}, {"created", t.created}, {"started", t.started}, {"ended", t.ended},
        {"label", t.label}, {"forced", t.forced}, {"error", t.status == "failed" ? t.reason : std::string()}};
    if (!t.afterSessions.empty()) out["after_sessions"] = t.afterSessions;
    if (!t.on.type.empty()) out["on_event"] = {{"type", t.on.type}, {"arg", t.on.arg}, {"arg2", t.on.arg2}, {"met", t.on.met}};
    if (!t.spawn.cwd.empty() || t.spawned) out["spawn"] = {{"cwd", t.spawn.cwd}, {"name", t.spawn.name}, {"backend", t.spawn.backend}, {"mode", t.spawn.mode}, {"effort", t.spawn.effort},
        {"one_shot", t.spawn.oneShot}, {"spawned", t.spawned}};
    if (t.priority != 0) out["priority"] = t.priority;
    if (t.timeout > 0) out["timeout_minutes"] = t.timeout;
    if (t.retry > 0) out["retry"] = {{"max", t.retry}, {"done", t.retries}, {"delay_minutes", t.retryDelay}};
    if (t.notBefore > cluster::now()) out["not_before_in_minutes"] = (t.notBefore - cluster::now()) / 60;
    if (t.maxConcurrency > 0) out["max_concurrency"] = {{"max", t.maxConcurrency}, {"lane", t.lane}};
    if (t.maxCost > 0.0) out["max_cost"] = t.maxCost;
    if (t.maxTokens > 0) out["max_tokens"] = t.maxTokens;
    if (!t.expect.empty()) out["expect"] = t.expect;
    if (!t.schedule.empty()) out["schedule"] = t.schedule;
    if (!t.result.empty()) out["result"] = t.result;
    out["traits"] = cluster::task_traits(t);
    return out;
}

_cold std::string cluster::Manager::taskSet(const std::string& id, const cluster::Json& patch)
{
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_tasks.begin(), this->_tasks.end(), [&](const cluster::Task& t) {return t.id == id;});
        if (it == this->_tasks.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no task '" + id + "'");
        if (it->status != "queued" && it->status != "running") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is " + it->status + ": ended");
        if (patch.contains("priority")) it->priority = static_cast<int>(std::clamp<std::int64_t>(opt::number(patch, "priority", 0), -100, 100));
        if (patch.contains("timeout_minutes")) it->timeout = static_cast<int>(std::clamp<std::int64_t>(opt::number(patch, "timeout_minutes", 0), 0, 100000));
        if (patch.contains("retry")) it->retry = static_cast<int>(std::clamp<std::int64_t>(opt::number(patch, "retry", 0), 0, 10));
        if (patch.contains("retry_delay_minutes")) it->retryDelay = static_cast<int>(std::clamp<std::int64_t>(opt::number(patch, "retry_delay_minutes", 0), 0, 10000));
        if (patch.contains("max_cost")) it->maxCost = std::max(0.0, opt::decimal(patch, "max_cost"));
        if (patch.contains("max_tokens")) it->maxTokens = std::max<std::int64_t>(0, opt::number(patch, "max_tokens", 0));
        if (patch.contains("max_concurrency")) it->maxConcurrency = static_cast<int>(std::clamp<std::int64_t>(opt::number(patch, "max_concurrency", 0), 0, 50));
        if (patch.contains("notify")) it->notify = opt::flag(patch, "notify", false);
        if (patch.contains("not_before")) it->notBefore = opt::number(patch, "not_before", 0);
        if (patch.contains("prompt") && it->status == "queued" && !cluster::trim(opt::text(patch, "prompt")).empty()) it->prompt = opt::text(patch, "prompt");
    }
    this->save();
    ++this->_version;
    this->schedule_();
    return "task " + id + " changed";
}

_cold void cluster::Manager::enforceTasks_(void)
{
    // a running task past its timeout or its budget: stopped, its author told why
    struct Stop {
        std::string id;
        std::string session;
        std::string by;
        std::string reason;
    };
    std::vector<Stop> stops;
    {
        const std::int64_t now = cluster::now();
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (cluster::Task& t: this->_tasks) {
            if (t.status != "running" || t.session.empty() || !t.reason.empty() || t.started == 0) continue;
            std::string why;
            if (t.timeout > 0 && now - t.started > static_cast<std::int64_t>(t.timeout) * 60) why = "timeout: running for more than " + std::to_string(t.timeout) + " minutes";
            if (why.empty() && (t.maxCost > 0.0 || t.maxTokens > 0)) {
                auto it = this->_sessions.find(t.session);
                if (it == this->_sessions.end()) continue;
                const cluster::Metrics metrics = it->second->snapshot().metrics;
                const double cost = metrics.cost - t.costBase;
                const std::int64_t tokens = metrics.total.total() + metrics.current.total() - t.tokensBase;
                if (t.maxCost > 0.0 && cost > t.maxCost) why = "budget exceeded: $" + std::to_string(cost).substr(0, 6) + " of $" + std::to_string(t.maxCost).substr(0, 6);
                else if (t.maxTokens > 0 && tokens > t.maxTokens) why = "budget exceeded: " + cluster::human_tokens(tokens) + " tokens of " + cluster::human_tokens(t.maxTokens);
            }
            if (why.empty()) continue;
            t.reason = why;
            stops.push_back({t.id, t.session, t.by, why});
        }
    }
    for (const Stop& stop: stops) {
        if (stop.by == GLOBAL_ID) this->replyUndo_(stop.session); // told once, with the reason, not by the end of the turn too
        try {
            this->interrupt(stop.session);
        } catch (const utils::exception::IException&) {}
        this->notice_(stop.session, "task " + stop.id + " stopped - " + stop.reason, true);
        this->tell_(stop.by, "[tasks] task " + stop.id + " stopped - " + stop.reason + " (what it answered so far is in task_list)");
    }
}

_cold void cluster::Manager::taskDone_(const std::string& session, const bool error, const bool interrupted, const std::string& text)
{
    // its answer kept (its result, capped; its group gets it); an error: tried again (its own retry, or once when its group allows it)
    bool changed = false;
    std::vector<cluster::Task> ended;
    std::set<std::string> groups;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (cluster::Task& task: this->_tasks) {
            if (task.session != session || task.status != "running") continue;
            changed = true;
            task.answer = text;
            task.ended = cluster::now();
            const std::size_t size = static_cast<std::size_t>(std::max(200, this->_config.taskResultChars));
            task.result = opt::cap(text, size);
            if (!task.expect.empty() && opt::is_path(task.expect)) { // the output asked in a file: its start is the result
                std::ifstream in(cluster::expand_home(task.expect));
                std::stringstream content;
                content << in.rdbuf();
                task.result = in && !content.str().empty() ? "[file " + task.expect + "]\n" + opt::cap(content.str(), size) : "[file " + task.expect + " not written]\n" + task.result;
            }
            if (!task.group.empty()) groups.insert(task.group);
            const bool groupRetry = std::any_of(this->_groups.begin(), this->_groups.end(), [&task](const cluster::Group& g) {return g.id == task.group && g.retry;});
            const bool retry = error && !interrupted && task.reason.empty() && task.retries < std::max(task.retry, groupRetry ? 1 : 0);
            if (retry) {
                ++task.retries;
                task.status = "queued";
                task.target = session;
                task.notBefore = task.retryDelay > 0 ? cluster::now() + static_cast<std::int64_t>(task.retryDelay) * 60 : 0;
                continue;
            }
            // its end said as it is: done (it finished), failed (an error), stopped (its turn interrupted before it finished: not done)
            task.status = error ? "failed" : interrupted ? "stopped" : "done";
            if (task.status == "stopped" && task.reason.empty()) task.reason = "its turn was interrupted before it finished";
            ended.push_back(task);
        }
    }
    if (!changed) return;
    this->save();
    for (const std::string& group: groups)
        this->groupUpdate_(group);
    this->schedule_(); // a place free: the next one at once
    for (const cluster::Task& task: ended) {
        if (task.notify || this->_config.notifyTaskEach)
            (void)this->notify({"task-" + task.id, "task " + task.id + (task.status == "done" ? " done" : " " + task.status), cluster::one_line(task.status == "done" ? task.result : task.reason, 160),
                task.status == "done" ? cluster::Face::Grin : cluster::Face::Waiting, task.status != "done", task.status == "done" ? "complete" : "dialog-warning"}, "task");
        // a sub-session that queued it: told (the global session has its reply, a group its own end)
        const bool sub = !task.by.empty() && task.by != GLOBAL_ID && task.group.empty() && !task.quiet;
        if (sub) this->tell_(task.by, "[tasks] task " + task.id + " " + task.status + (task.status == "done" ? "\n" + task.result : ": " + (task.reason.empty() ? cluster::one_line(task.answer, 300) : task.reason)));
        if (task.spawned && task.spawn.oneShot) {
            std::thread([this, session]() { // closed to the trash after a moment (the state settles), unless it is still of use
                std::this_thread::sleep_for(std::chrono::seconds(2));
                const std::optional<cluster::Snapshot> snap = this->snapshot(session);
                const bool shown = this->_active == session && this->_focused != 0;
                if (!snap || shown || snap->state != cluster::State::Idle || snap->queued > 0 || !snap->permissions.empty()) return;
                try {
                    this->close(session);
                } catch (const utils::exception::IException&) {}
            }).detach();
        }
    }
    if (!groups.empty()) return; // a group: its own end told
    // the queue done: one summary
    std::size_t done = 0;
    std::size_t failed = 0;
    bool open = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const cluster::Task& task: this->_tasks) {
            open |= task.status == "queued" || task.status == "running";
            done += task.status == "done" ? 1 : 0;
            failed += task.status == "failed" ? 1 : 0;
        }
    }
    if (!open)
        (void)this->notify({"tasks", "task queue finished", std::to_string(done) + " done" + (failed > 0 ? ", " + std::to_string(failed) + " failed" : std::string()),
            failed > 0 ? cluster::Face::Waiting : cluster::Face::Grin, false, "complete"}, "task");
}

_cold void cluster::Manager::schedule_(void)
{
    // <task id, session id>, decided under the lock, run after
    std::vector<std::pair<std::string, std::string>> starts;
    std::vector<std::string> spawns;
    std::vector<std::pair<std::string, std::string>> failed;    // <task id, who queued it>: failed here, its reason told
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        const std::int64_t now = cluster::now();
        // the parallel sessions: every sub-session working (or waiting on a permission, starting), and the tasks being started;
        // past the limit the tasks wait in the queue, sent as soon as a place is free
        std::set<std::string> busy;
        for (const std::string& id: this->_order) {
            const cluster::State state = this->_sessions.at(id)->snapshot().state;
            if (state == cluster::State::Working || state == cluster::State::Waiting || state == cluster::State::Starting) busy.insert(id);
        }
        for (const cluster::Task& t: this->_tasks)
            if (t.status == "running") busy.insert(t.session.empty() ? "task:" + t.id : t.session);
        std::size_t running = busy.size();
        // a session by its id or its name
        const std::function<std::shared_ptr<cluster::Session>(const std::string&)> find = [this](const std::string& name) -> std::shared_ptr<cluster::Session> {
            for (const auto &[id, session]: this->_sessions)
                if (id == name || session->spec().name == name) return session;
            return nullptr;
        };
        const std::function<void(cluster::Task&, const std::string&)> fail = [&](cluster::Task& task, const std::string& why) {
            task.status = "failed"; // nothing will ever run it
            task.reason = why;
            task.ended = now;
            failed.emplace_back(task.id, task.by);
        };

        // the highest priority first, then the oldest
        std::vector<std::size_t> order(this->_tasks.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::stable_sort(order.begin(), order.end(), [this](const std::size_t a, const std::size_t b) {return this->_tasks[a].priority > this->_tasks[b].priority;});
        for (const std::size_t index: order) {
            cluster::Task& task = this->_tasks[index];
            if (task.status != "queued" || task.notBefore > now || (!task.forced && running >= static_cast<std::size_t>(std::max(1, this->_config.maxParallel)))) continue;
            bool ready = true;
            for (const std::string& dep: task.after) {
                auto it = std::find_if(this->_tasks.begin(), this->_tasks.end(), [&](const cluster::Task& t) {return t.id == dep;});
                if (it == this->_tasks.end() || it->status == "failed" || it->status == "cancelled" || it->status == "stopped") { // never as if it finished
                    fail(task, it == this->_tasks.end() ? "its dependency " + dep + " does not exist any more"
                        : "its dependency " + dep + " " + it->status + (it->reason.empty() ? "" : ": " + it->reason));
                    ready = false;
                    break;
                }
                if (it->status != "done") ready = false;
            }
            if (!ready) continue;
            // the sessions it waits for: idle (and nothing queued for them)
            std::vector<std::string> waits = task.afterSessions;
            if (task.on.type == "session_idle" && !task.on.met) waits.push_back(task.on.arg);
            for (const std::string& name: waits) {
                const std::shared_ptr<cluster::Session> session = find(name);
                if (!session) {
                    fail(task, "the session '" + name + "' it waits for does not exist (closed or never created)");
                    ready = false;
                    break;
                }
                const cluster::Snapshot snap = session->snapshot();
                if (snap.state != cluster::State::Idle || snap.queued > 0) ready = false;
            }
            if (!ready) continue;
            if (task.on.type == "session_idle") task.on.met = true;
            if (!task.on.type.empty() && !task.on.met) continue; // an event of the checks in the background (command, git, file)
            if (task.maxConcurrency > 0 && static_cast<int>(std::count_if(this->_tasks.begin(), this->_tasks.end(),
                [&task](const cluster::Task& t) {return t.status == "running" && t.lane == task.lane;})) >= task.maxConcurrency) continue;

            // its own session: started (the options of session_spawn); else a free one: the target (id / name), one in the target folder, or any idle sub-session
            if (!task.spawn.cwd.empty() && (!task.spawned || !this->_sessions.contains(task.session))) {
                task.status = "running";
                task.session.clear();
                spawns.push_back(task.id);
                ++running;
                continue;
            }
            const bool folder = task.target.find('/') != std::string::npos || task.target.starts_with("~") || task.target == ".";
            if (!folder && !task.target.empty() && !find(task.target)) {
                fail(task, "its target session '" + task.target + "' does not exist (closed or never created)");
                continue;
            }
            const std::string wantedCwd = folder ? std::filesystem::weakly_canonical(cluster::expand_home(task.target)).string() : "";
            std::string chosen;
            for (const std::string& id: this->_order) {
                const cluster::Snapshot snap = this->_sessions.at(id)->snapshot();
                if (busy.contains(id) || snap.state != cluster::State::Idle) continue;
                const bool match = task.target.empty() || (folder ? snap.spec.cwd == wantedCwd : (id == task.target || snap.spec.name == task.target));
                if (match) {
                    chosen = id;
                    break;
                }
            }
            if (!chosen.empty()) {
                task.status = "running";
                task.session = chosen;
                busy.insert(chosen);
                starts.emplace_back(task.id, chosen);
                ++running;
            } else if (folder) {
                task.status = "running";
                spawns.push_back(task.id);
                ++running;
            }
        }
    }
    for (const std::string& taskId: spawns) {
        try {
            cluster::SpawnRequest request;
            std::string by;
            bool capped = false;
            {
                std::lock_guard<std::mutex> lock(this->_mutex);
                for (const cluster::Task& task: this->_tasks) {
                    if (task.id != taskId) continue;
                    by = task.by;
                    const bool own = !task.spawn.cwd.empty();
                    request.cwd = own ? task.spawn.cwd : task.target; // a task of a folder: a session of its own there
                    request.name = !task.spawn.name.empty() ? task.spawn.name : !task.label.empty() ? task.label : "task-" + taskId; // a task of a group: named after its project
                    request.backend = task.spawn.backend;
                    request.mode = task.spawn.mode;
                    request.effort = task.spawn.effort;
                    request.profile = task.spawn.profile;
                    capped = own && !by.empty() && by != GLOBAL_ID && this->spawnedBy_(by) > static_cast<std::size_t>(std::max(0, this->_config.subSpawnCap));
                }
            }
            if (capped) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the cap of sessions started by your tasks / schedules is reached ([agent] sub_spawn_cap)");
            const std::string id = this->spawn(request);
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Task& task: this->_tasks)
                if (task.id == taskId) {
                    task.session = id;
                    task.spawned = !task.spawn.cwd.empty();
                    starts.emplace_back(taskId, id);
                }
        } catch (const utils::exception::IException& e) {
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Task& task: this->_tasks)
                if (task.id == taskId) {
                    task.status = "failed";
                    task.reason = "its session could not be started: " + std::string(e.info());
                    task.ended = cluster::now();
                    failed.emplace_back(task.id, task.by);
                }
        }
    }
    for (const auto &[taskId, sessionId]: starts) {
        std::string prompt;
        std::string group;
        std::string by;
        std::string expect;
        std::optional<cluster::Snapshot> snap = this->snapshot(sessionId);
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Task& task: this->_tasks) {
                if (task.id != taskId) continue;
                prompt = task.prompt;
                group = task.group;
                by = task.by;
                expect = task.expect;
                task.started = cluster::now();
                task.reason.clear();
                task.costBase = snap ? snap->metrics.cost : 0.0;
                task.tokensBase = snap ? snap->metrics.total.total() + snap->metrics.current.total() : 0;
                // {{t3.result}}: the result of that task (also .answer, .status, .error, .session)
                std::string rendered;
                std::size_t from = 0;
                const std::regex placeholder(R"(\{\{\s*(t\d+)\.(\w+)\s*\}\})");
                for (std::sregex_iterator it(prompt.begin(), prompt.end(), placeholder), end; it != end; ++it) {
                    const std::string key = (*it)[2].str();
                    auto before = std::find_if(this->_tasks.begin(), this->_tasks.end(), [&](const cluster::Task& t) {return t.id == (*it)[1].str();});
                    rendered += prompt.substr(from, static_cast<std::size_t>(it->position()) - from);
                    rendered += before == this->_tasks.end() ? "[no task " + (*it)[1].str() + "]" : key == "status" ? before->status : key == "error" ? before->reason
                        : key == "session" ? before->session : before->result.empty() ? opt::cap(before->answer, static_cast<std::size_t>(std::max(200, this->_config.taskResultChars))) : before->result;
                    from = static_cast<std::size_t>(it->position() + it->length());
                }
                prompt = rendered + prompt.substr(from);
                // the answers of the tasks it comes after: given to it (a chain between projects), unless the prompt took them with a placeholder
                for (const std::string& dep: task.after)
                    for (const cluster::Task& before: this->_tasks)
                        if (before.id == dep && !before.answer.empty() && task.prompt.find("{{" + dep + ".") == std::string::npos && task.prompt.find("{{ " + dep + ".") == std::string::npos)
                            prompt += "\n\n[the answer of " + (before.label.empty() ? before.id : before.label) + " (" + before.id + "), this task builds on it]\n"
                                + (cluster::head_utf8(before.answer, 4000));
            }
        }
        if (expect == "compact") prompt += "\n\n[output asked: answer compactly - the facts only, no recap; your answer is kept as the result of this task and may be chained into other prompts]";
        else if (!expect.empty() && opt::is_path(expect)) prompt += "\n\n[output asked: write your result to " + expect + " and answer with that path and one line only]";
        const bool answered = by == GLOBAL_ID && group.empty(); // a task of the global session: its answer comes back to it (a group has its own)
        try {
            if (answered) this->replyTo(sessionId); // before: a short turn could end before it is known
            try {
                this->send(sessionId, prompt, false, {}, prompt.starts_with("[from the global session]") ? "mcp" : "task");
            } catch (const utils::exception::IException&) {
                if (answered) this->replyUndo_(sessionId);
                throw;
            }
            this->notice_(sessionId, "task " + taskId + " started");
            if (!group.empty()) this->groupUpdate_(group);
        } catch (const utils::exception::IException& e) {
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Task& task: this->_tasks)
                if (task.id == taskId) {
                    task.status = "failed";
                    task.reason = "its prompt could not be sent to " + sessionId + ": " + std::string(e.info());
                    task.ended = cluster::now();
                    failed.emplace_back(task.id, task.by);
                }
        }
    }
    if (!failed.empty()) {
        // a task failed before any turn: the session that queued it is told why (a chain: its dependents fail in the next pass)
        this->save();
        for (const auto &[taskId, by]: failed) {
            std::string why;
            std::string group;
            {
                std::lock_guard<std::mutex> lock(this->_mutex);
                for (const cluster::Task& task: this->_tasks)
                    if (task.id == taskId) {
                        why = task.reason;
                        group = task.group;
                    }
            }
            if (!group.empty()) this->groupUpdate_(group);
            else this->tell_(by, "[tasks] task " + taskId + " failed: " + why);
        }
        this->schedule_(); // the tasks after it fail with their reason too
    }
    if (!starts.empty() || !spawns.empty()) this->save();
}

/* the Tasks session: acted on by hand */
_cold void cluster::Manager::tell_(const std::string& by, const std::string& text)
{
    // the session that made it: told (a prompt: it may react); you or a session gone: a notice
    if (!by.empty() && this->snapshot(by)) {
        try {
            this->send(by, text, true, {}, "task");
            return;
        } catch (const utils::exception::IException&) {}
    }
    this->notice_("", text);
}

_cold std::string cluster::Manager::taskAct(const std::string& id, const std::string& action, const std::string& reason)
{
    if (this->_replica) return this->call({{"cmd", "task_act"}, {"id", id}, {"action", action}, {"reason", reason}}).value("done", std::string());
    cluster::Task task;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_tasks.begin(), this->_tasks.end(), [&](const cluster::Task& t) {return t.id == id;});
        if (it == this->_tasks.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no task '" + id + "'");
        if (action == "cancel") {
            if (it->status != "queued" && it->status != "running")
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is " + it->status + ": nothing to cancel");
            it->status = "cancelled";
            it->reason = reason;
            it->ended = cluster::now();
        } else if (action == "force") {
            if (it->status != "queued") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is " + it->status + ": not waiting");
            it->forced = true;
        } else if (action == "retry") {
            if (it->status != "failed" && it->status != "cancelled" && it->status != "stopped") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is " + it->status + ": only a failed / cancelled one");
            it->status = "queued"; // again, from the start (its session goes on where it is)
            it->reason.clear();
            it->ended = 0;
            it->archived = false;
            it->notBefore = 0;
            if (it->spawned) it->target = it->session;
        } else if (action == "archive" || action == "unarchive") {
            it->archived = action == "archive";
        } else if (action != "delete") {
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "cancel | delete | archive | unarchive | force | retry");
        }
        task = *it;
        if (action == "delete") this->_tasks.erase(it);
    }
    this->save();
    ++this->_version;
    if (action == "force" || action == "retry") this->schedule_(); // started now, past the limit / again
    const std::string what = action == "cancel" ? "cancelled" : action == "delete" ? "deleted" : action == "force" ? "started now" : action == "retry" ? "queued again" : action + "d";
    if ((action == "cancel" || action == "delete") && !reason.empty())
        this->tell_(task.by, "[tasks] your task " + id + " (\"" + cluster::one_line(task.prompt, 80) + "\") was " + what + " by the user: " + reason);
    else
        this->notice_("", "task " + id + " " + what);
    return "task " + id + " " + what;
}

_cold void cluster::Manager::dropFor_(const std::string& session, const bool gone, const std::string& name)
{
    // a session closed: what waits for it (its target, or what it made) cancelled; deleted for good: removed
    std::size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (gone) {
            count += std::erase_if(this->_tasks, [&](const cluster::Task& t) {return t.target == session || (!name.empty() && t.target == name) || t.by == session || t.session == session;});
            count += std::erase_if(this->_schedules, [&](const cluster::Schedule& w) {return w.session == session || w.by == session;});
        } else {
            for (cluster::Task& t: this->_tasks)
                if ((t.target == session || (!name.empty() && t.target == name) || t.by == session) && t.status == "queued") {
                    t.status = "cancelled";
                    t.reason = "its session was closed";
                    ++count;
                }
            for (cluster::Schedule& w: this->_schedules)
                if ((w.session == session || w.by == session) && w.status == "waiting") {
                    w.status = "cancelled";
                    w.reason = "its session was closed";
                    ++count;
                }
        }
    }
    if (count == 0) return;
    this->notice_("", std::to_string(count) + " task(s) / schedule(s) of " + session + (gone ? " removed with it" : " cancelled (closed)"));
    ++this->_version;
}
