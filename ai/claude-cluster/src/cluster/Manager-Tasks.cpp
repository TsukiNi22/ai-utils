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
_cold std::string cluster::Manager::addTask(const std::string& prompt, const std::string& target, const std::vector<std::string>& after)
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
        if (it->status == "queued" || it->status == "running") it->status = "cancelled";
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
            if (task.status != "queued" || running >= static_cast<std::size_t>(std::max(1, this->_config.maxParallel))) continue;
            bool ready = true;
            for (const std::string& dep: task.after) {
                auto it = std::find_if(this->_tasks.begin(), this->_tasks.end(), [&](const cluster::Task& t) {return t.id == dep;});
                if (it == this->_tasks.end() || it->status == "failed" || it->status == "cancelled") {
                    task.status = "failed"; // a dependency will never be done
                    ready = false;
                    break;
                }
                if (it->status != "done") ready = false;
            }
            if (!ready) continue;

            // A free session: the target (id / name), one in the target folder, or any idle sub-session
            const bool folder = task.target.find('/') != std::string::npos || task.target.starts_with("~") || task.target == ".";
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
                if (task.id == taskId) task.status = "failed";
        }
    }
    for (const auto &[taskId, sessionId]: starts) {
        std::string prompt;
        std::string group;
        {
            // the answers of the tasks it comes after: given to it (a chain between projects)
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Task& task: this->_tasks) {
                if (task.id != taskId) continue;
                prompt = task.prompt;
                group = task.group;
                task.started = cluster::now();
                for (const std::string& dep: task.after)
                    for (const cluster::Task& before: this->_tasks)
                        if (before.id == dep && !before.answer.empty())
                            prompt += "\n\n[the answer of " + (before.label.empty() ? before.id : before.label) + " (" + before.id + "), this task builds on it]\n"
                                + (before.answer.size() > 4000 ? before.answer.substr(0, 4000) + "\n[...]" : before.answer);
            }
        }
        try {
            this->send(sessionId, prompt);
            this->notice_(sessionId, "task " + taskId + " started");
            if (!group.empty()) this->groupUpdate_(group);
        } catch (const utils::exception::IException& e) {
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Task& task: this->_tasks)
                if (task.id == taskId) task.status = "failed";
        }
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

_cold std::string cluster::Manager::addSchedule(const std::string& when, const std::string& action, const std::string& session, const std::string& text, const int every)
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
        this->_schedules.push_back({id, at, std::max(every, 0), action, target, text});
    }
    this->save();
    ++this->_version;
    return id;
}

std::vector<cluster::Schedule> cluster::Manager::schedules(void) const
{
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
        for (cluster::Schedule& w: this->_schedules) {
            if (w.at > now) continue;
            due.push_back(w);
            if (w.every > 0)
                while (w.at <= now) w.at += static_cast<std::int64_t>(w.every) * 60;
        }
        std::erase_if(this->_schedules, [now](const cluster::Schedule& w) {return w.at <= now;});
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
