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
#include "cluster/Tools.hpp"
#include "cluster/Wire.hpp"
#include <algorithm>
#include <ctime>
#include <regex>
#include <set>
#include <map>

/* tasks */
namespace {

constexpr std::int64_t TASK_ARCHIVE_SECONDS = 1800;    // a finished task stays in the list this long, then archived

} // namespace

_cold std::string cluster::Manager::addTask(const std::string& prompt, const std::string& target, const std::vector<std::string>& after, const std::string& by)
{
    if (cluster::trim(prompt).empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "empty task");
    std::string id;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const std::string& dep: after)
            if (std::none_of(this->_tasks.begin(), this->_tasks.end(), [&](const cluster::Task& t) {return t.id == dep;}))
                throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no task '" + dep + "'");
        // the task queue: the tasks waiting for a free place, bounded
        const std::size_t waiting = std::count_if(this->_tasks.begin(), this->_tasks.end(), [](const cluster::Task& t) {return t.status == "queued";});
        if (waiting >= static_cast<std::size_t>(std::max(1, this->_config.taskQueue)))
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the task queue is full (" + std::to_string(waiting)
                + " waiting: [sessions] task_queue)");
        id = "t" + std::to_string(this->_nextTask++);
        cluster::Task task;
        task.id = id;
        task.prompt = prompt;
        task.target = target;
        task.after = after;
        task.by = by;
        task.created = cluster::now();
        this->_tasks.push_back(task);
    }
    this->notice_("", "task " + id + " queued: " + cluster::one_line(prompt, 80));
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

_cold void cluster::Manager::taskDone_(const std::string& session, const bool error, const bool interrupted, const std::string& text)
{
    // its answer kept (its group gets it); an error: tried once more in the same session when its group allows it
    bool changed = false;
    std::set<std::string> groups;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (cluster::Task& task: this->_tasks) {
            if (task.session != session || task.status != "running") continue;
            changed = true;
            task.answer = text;
            task.ended = cluster::now();
            if (!task.group.empty()) groups.insert(task.group);
            const bool retry = error && !interrupted && task.retries < 1 && std::any_of(this->_groups.begin(), this->_groups.end(),
                [&task](const cluster::Group& g) {return g.id == task.group && g.retry;});
            if (retry) {
                ++task.retries;
                task.status = "queued";
                task.target = session;
                continue;
            }
            task.status = error || interrupted ? "failed" : "done";
        }
    }
    if (!changed) return;
    this->save();
    for (const std::string& group: groups)
        this->groupUpdate_(group);
    this->schedule_(); // a place free: the next one at once
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
    // <task id, session id or folder to spawn>, decided under the lock, run after
    std::vector<std::pair<std::string, std::string>> starts;
    std::vector<std::pair<std::string, std::string>> spawns;
    std::vector<std::pair<std::string, std::string>> failed;    // <task id, who queued it>: failed here, its reason told
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
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

        for (cluster::Task& task: this->_tasks) {
            if (task.status != "queued" || (!task.forced && running >= static_cast<std::size_t>(std::max(1, this->_config.maxParallel)))) continue;
            bool ready = true;
            for (const std::string& dep: task.after) {
                auto it = std::find_if(this->_tasks.begin(), this->_tasks.end(), [&](const cluster::Task& t) {return t.id == dep;});
                if (it == this->_tasks.end() || it->status == "failed" || it->status == "cancelled") {
                    task.status = "failed"; // a dependency will never be done
                    task.reason = it == this->_tasks.end() ? "its dependency " + dep + " does not exist any more"
                        : "its dependency " + dep + " " + it->status + (it->reason.empty() ? "" : ": " + it->reason);
                    task.ended = cluster::now();
                    failed.emplace_back(task.id, task.by);
                    ready = false;
                    break;
                }
                if (it->status != "done") ready = false;
            }
            if (!ready) continue;

            // A free session: the target (id / name), one in the target folder, or any idle sub-session
            const bool folder = task.target.find('/') != std::string::npos || task.target.starts_with("~") || task.target == ".";
            if (!folder && !task.target.empty() && std::none_of(this->_sessions.begin(), this->_sessions.end(),
                [&task](const std::pair<const std::string, std::shared_ptr<cluster::Session>>& s) {return s.first == task.target || s.second->spec().name == task.target;})) {
                task.status = "failed"; // a session closed or never created: nothing will ever run it
                task.reason = "its target session '" + task.target + "' does not exist (closed or never created)";
                task.ended = cluster::now();
                failed.emplace_back(task.id, task.by);
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
                spawns.emplace_back(task.id, task.target);
                ++running;
            }
        }
    }
    for (const auto &[taskId, folder]: spawns) {
        try {
            std::string name = "task-" + taskId; // a task of a group: named after its project
            {
                std::lock_guard<std::mutex> lock(this->_mutex);
                for (const cluster::Task& task: this->_tasks)
                    if (task.id == taskId && !task.label.empty()) name = task.label;
            }
            const std::string id = this->spawn({folder, name, "", "", "", "", ""});
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Task& task: this->_tasks)
                if (task.id == taskId) {
                    task.session = id;
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
        {
            // the answers of the tasks it comes after: given to it (a chain between projects)
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Task& task: this->_tasks) {
                if (task.id != taskId) continue;
                prompt = task.prompt;
                group = task.group;
                by = task.by;
                task.started = cluster::now();
                for (const std::string& dep: task.after)
                    for (const cluster::Task& before: this->_tasks)
                        if (before.id == dep && !before.answer.empty())
                            prompt += "\n\n[the answer of " + (before.label.empty() ? before.id : before.label) + " (" + before.id + "), this task builds on it]\n"
                                + (before.answer.size() > 4000 ? before.answer.substr(0, 4000) + "\n[...]" : before.answer);
            }
        }
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

/* schedules */
namespace {

_cold std::int64_t when_(const std::string& when)
{
    // +30m / +2h / +1d / +90s, HH:MM (today, else tomorrow), YYYY-MM-DD HH:MM, unix seconds
    const std::string w = cluster::trim(when);
    const std::int64_t now = cluster::now();
    std::smatch match;
    if (std::regex_match(w, match, std::regex(R"(\+?(\d+)\s*([smhd]))"))) {
        static const std::map<std::string, std::int64_t> units = {{"s", 1}, {"m", 60}, {"h", 3600}, {"d", 86400}};
        return now + std::stoll(match[1]) * units.at(match[2]);
    }
    if (std::regex_match(w, std::regex(R"(\d{9,})"))) return std::stoll(w);
    std::tm local{};
    const std::time_t current = static_cast<std::time_t>(now);
    ::localtime_r(&current, &local);
    if (std::regex_match(w, match, std::regex(R"((\d{1,2}):(\d{2}))"))) {
        local.tm_hour = std::stoi(match[1]);
        local.tm_min = std::stoi(match[2]);
        local.tm_sec = 0;
        std::int64_t at = static_cast<std::int64_t>(std::mktime(&local));
        if (at <= now) at += 86400;
        return at;
    }
    if (std::regex_match(w, match, std::regex(R"((\d{4})-(\d{2})-(\d{2})[ T](\d{1,2}):(\d{2}))"))) {
        local.tm_year = std::stoi(match[1]) - 1900;
        local.tm_mon = std::stoi(match[2]) - 1;
        local.tm_mday = std::stoi(match[3]);
        local.tm_hour = std::stoi(match[4]);
        local.tm_min = std::stoi(match[5]);
        local.tm_sec = 0;
        local.tm_isdst = -1;
        return static_cast<std::int64_t>(std::mktime(&local));
    }
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument,
        "when: +30m, +2h, +1d, HH:MM (today, else tomorrow), YYYY-MM-DD HH:MM - not '" + when + "'");
}

} // namespace

_cold std::string cluster::Manager::addSchedule(const std::string& when, const std::string& action, const std::string& session, const std::string& text, const int every,
    const std::string& by)
{
    if (action != "prompt" && action != "notify")
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "action: prompt | notify");
    if (cluster::trim(text).empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "schedule: the text is empty");
    const std::int64_t at = when_(when);
    const std::string target = action == "prompt" ? this->resolve(session.empty() ? std::string(GLOBAL_ID) : session) : "";
    std::string id;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        id = "w" + std::to_string(this->_nextSchedule++);
        cluster::Schedule w;
        w.id = id;
        w.at = at;
        w.every = std::max(every, 0);
        w.action = action;
        w.session = target;
        w.text = text;
        w.by = by;
        this->_schedules.push_back(w);
    }
    this->save();
    ++this->_version;
    return id;
}

std::vector<cluster::Schedule> cluster::Manager::schedules(void) const
{
    if (this->_replica) return this->replicaState("schedules").get<std::vector<cluster::Schedule>>(); // an interface of the core
    std::lock_guard<std::mutex> lock(this->_mutex);
    std::vector<cluster::Schedule> list = this->_schedules;
    std::sort(list.begin(), list.end(), [](const cluster::Schedule& a, const cluster::Schedule& b) {return a.at < b.at;});
    return list;
}

_cold void cluster::Manager::cancelSchedule(const std::string& id)
{
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        const std::size_t before = this->_schedules.size();
        std::erase_if(this->_schedules, [&](const cluster::Schedule& w) {return w.id == id;});
        if (this->_schedules.size() == before) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no schedule " + id);
    }
    this->save();
    ++this->_version;
}

_cold void cluster::Manager::fireSchedules_(void)
{
    // due: a prompt to its session (it wakes up on it) or a notification; then again (every) or removed
    const std::int64_t now = cluster::now();
    std::vector<cluster::Schedule> due;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (cluster::Task& task: this->_tasks) // the finished tasks leave the list after a while (their archive)
            if (!task.archived && task.status != "queued" && task.status != "running" && now - (task.ended > 0 ? task.ended : task.created) > TASK_ARCHIVE_SECONDS) task.archived = true;
        for (cluster::Schedule& w: this->_schedules) {
            if (w.status != "waiting" || w.at > now) continue;
            due.push_back(w);
            ++w.runs;
            w.last = now;
            if (w.every > 0)
                while (w.at <= now) w.at += static_cast<std::int64_t>(w.every) * 60;
            else
                w.status = "done"; // once: kept in the list (its archive), not run again
        }
    }
    for (const cluster::Schedule& w: due) {
        if (w.action == "notify") {
            this->notice_(GLOBAL_ID, "scheduled " + w.id + ": " + w.text);
            (void)this->notify({"schedule-" + w.id, "reminder", w.text, cluster::Face::Attentive, false, "message-new-instant"}, "reminder");
            continue;
        }
        try {
            this->send(w.session, "[scheduled " + w.id + "] " + w.text, true);
            this->notice_(w.session, "scheduled " + w.id + " sent");
        } catch (const utils::exception::IException& e) {
            this->notice_(w.session, "scheduled " + w.id + " not sent: " + e.info(), true);
        }
    }
    if (!due.empty()) {
        this->save();
        ++this->_version;
    }
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
        } else if (action == "archive" || action == "unarchive") {
            it->archived = action == "archive";
        } else if (action != "delete") {
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "cancel | delete | archive | unarchive | force");
        }
        task = *it;
        if (action == "delete") this->_tasks.erase(it);
    }
    this->save();
    ++this->_version;
    if (action == "force") this->schedule_(); // started now, past the limit
    const std::string what = action == "cancel" ? "cancelled" : action == "delete" ? "deleted" : action == "force" ? "started now" : action + "d";
    if ((action == "cancel" || action == "delete") && !reason.empty())
        this->tell_(task.by, "[tasks] your task " + id + " (\"" + cluster::one_line(task.prompt, 80) + "\") was " + what + " by the user: " + reason);
    else
        this->notice_("", "task " + id + " " + what);
    return "task " + id + " " + what;
}

_cold std::string cluster::Manager::scheduleAct(const std::string& id, const std::string& action, const std::string& reason)
{
    if (this->_replica) return this->call({{"cmd", "schedule_act"}, {"id", id}, {"action", action}, {"reason", reason}}).value("done", std::string());
    cluster::Schedule w;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_schedules.begin(), this->_schedules.end(), [&](const cluster::Schedule& s) {return s.id == id;});
        if (it == this->_schedules.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no schedule " + id);
        if (action == "cancel") {
            if (it->status != "waiting") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is " + it->status + ": nothing to cancel");
            it->status = "cancelled";
            it->reason = reason;
        } else if (action == "trigger") {
            if (it->status != "waiting") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is " + it->status + ": not waiting");
            if (it->every == 0) it->at = cluster::now(); // once: now; repeated: one run now, its next time kept
        } else if (action == "archive" || action == "unarchive") {
            it->archived = action == "archive";
        } else if (action != "delete") {
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "cancel | delete | archive | unarchive | trigger");
        }
        w = *it;
        if (action == "delete") this->_schedules.erase(it);
    }
    if (action == "trigger" && w.every > 0) { // a repeated one: this run now, its next time kept
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Schedule& s: this->_schedules)
                if (s.id == id) {
                    ++s.runs;
                    s.last = cluster::now();
                }
        }
        if (w.action == "notify") (void)this->notify({"schedule-" + w.id, "reminder", w.text, cluster::Face::Attentive, false, "message-new-instant"}, "reminder");
        else this->send(w.session, "[scheduled " + w.id + "] " + w.text, true);
    }
    this->save();
    ++this->_version;
    if (action == "trigger" && w.every == 0) this->fireSchedules_();
    const std::string what = action == "cancel" ? "cancelled" : action == "delete" ? "deleted" : action == "trigger" ? "run now" : action + "d";
    if ((action == "cancel" || action == "delete") && !reason.empty())
        this->tell_(w.by, "[tasks] your schedule " + id + " (\"" + cluster::one_line(w.text, 80) + "\") was " + what + " by the user: " + reason);
    else
        this->notice_("", "schedule " + id + " " + what);
    return "schedule " + id + " " + what;
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
