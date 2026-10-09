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
#include <algorithm>

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
        id = "t" + std::to_string(this->_nextTask++);
        this->_tasks.push_back({id, prompt, target, after, "queued", "", cluster::now()});
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
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_tasks;
}

_cold void cluster::Manager::taskDone_(const std::string& session, const bool error)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (cluster::Task& task: this->_tasks) {
            if (task.session != session || task.status != "running") continue;
            task.status = error ? "failed" : "done";
            changed = true;
        }
    }
    if (changed) this->save();
}

_cold void cluster::Manager::schedule_(void)
{
    // <task id, session id or folder to spawn>, decided under the lock, run after
    std::vector<std::pair<std::string, std::string>> starts;
    std::vector<std::pair<std::string, std::string>> spawns;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        std::size_t running = std::count_if(this->_tasks.begin(), this->_tasks.end(), [](const cluster::Task& t) {return t.status == "running";});
        std::set<std::string> busy;
        for (const cluster::Task& t: this->_tasks)
            if (t.status == "running") busy.insert(t.session);

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
            const std::string id = this->spawn({folder, "task-" + taskId, "", "", "", ""});
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
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (const cluster::Task& task: this->_tasks)
                if (task.id == taskId) prompt = task.prompt;
        }
        try {
            this->send(sessionId, prompt);
            this->notice_(sessionId, "task " + taskId + " started");
        } catch (const utils::exception::IException& e) {
            std::lock_guard<std::mutex> lock(this->_mutex);
            for (cluster::Task& task: this->_tasks)
                if (task.id == taskId) task.status = "failed";
        }
    }
    if (!starts.empty() || !spawns.empty()) this->save();
}
