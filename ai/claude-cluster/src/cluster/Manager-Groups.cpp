/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Manager-Groups.cpp

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
#include <thread>

/* tools */
namespace {

_cold std::vector<std::filesystem::path> projects_(const std::filesystem::path& under, const std::string& has, const std::string& name)
{
    // the project folders under a folder (3 levels): a git repository, or holding the file `has`; their name holding `name`
    static const std::set<std::string> skipped = {"node_modules", "build", "target", "dist", "vendor", "venv", ".venv", "__pycache__"};
    std::vector<std::filesystem::path> found;
    std::error_code error;
    const std::string wanted = cluster::lower(name);
    const std::function<void(const std::filesystem::path&, int)> walk = [&](const std::filesystem::path& dir, const int depth) {
        if (found.size() >= 200) return;
        const bool project = has.empty() ? std::filesystem::exists(dir / ".git", error) : std::filesystem::exists(dir / has, error);
        if (project && depth > 0) {
            if (wanted.empty() || cluster::lower(dir.filename().string()).find(wanted) != std::string::npos) found.push_back(dir);
            return; // a project: not searched inside
        }
        if (depth >= 3) return;
        for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, error)) {
            const std::string base = entry.path().filename().string();
            if (!entry.is_directory(error) || entry.is_symlink(error) || base.starts_with(".") || skipped.contains(base)) continue;
            walk(entry.path(), depth + 1);
        }
    };
    walk(under, 0);
    std::sort(found.begin(), found.end());
    return found;
}

_cold std::string took_(const std::int64_t from, const std::int64_t to)
{
    const std::int64_t seconds = std::max<std::int64_t>(0, to - from);
    return seconds < 60 ? std::to_string(seconds) + " s" : std::to_string(seconds / 60) + " min " + std::to_string(seconds % 60) + " s";
}

} // namespace

/* groups */
_cold cluster::Json cluster::Manager::fanout(const cluster::Json& args)
{
    // several prompts at once (a request on several projects): their tasks in the queue, the global session woken once
    // with their answers as it asked (all, the first, n of them, a time), to merge them
    struct Want {
        std::string target;     // a session id or a folder
        std::string label;      // its name for the global session
        std::string prompt;
        std::vector<int> after; // indexes in this group
    };
    std::vector<Want> wants;
    const std::string common = args.value("prompt", std::string());
    for (const cluster::Json& t: args.value("tasks", cluster::Json::array())) {
        if (!t.is_object()) continue;
        Want want{"", "", t.value("prompt", common), {}};
        if (t.contains("session") && t["session"].is_string() && !t["session"].get<std::string>().empty()) {
            want.target = this->resolve(t["session"].get<std::string>());
            want.label = this->snapshot(want.target) ? this->snapshot(want.target)->spec.name : want.target;
        } else if (t.contains("cwd") && t["cwd"].is_string() && !t["cwd"].get<std::string>().empty()) {
            std::error_code error;
            const std::filesystem::path folder = std::filesystem::weakly_canonical(cluster::expand_home(t["cwd"].get<std::string>()), error);
            if (!std::filesystem::is_directory(folder, error))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no folder " + t["cwd"].get<std::string>());
            want.target = folder.string();
            want.label = folder.filename().string();
        } else {
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "a task needs a session or a cwd");
        }
        for (const cluster::Json& a: t.value("after", cluster::Json::array()))
            if (a.is_number_integer()) want.after.push_back(a.get<int>());
        wants.push_back(want);
    }

    // a pattern: the projects found under a folder; past 10 of them, or sessions to create, shown to the user first
    if (args.contains("match") && args["match"].is_object()) {
        const cluster::Json& match = args["match"];
        const std::filesystem::path under = cluster::expand_home(match.value("under", std::string("~")));
        const std::vector<std::filesystem::path> found = projects_(under, match.value("has", std::string()), match.value("name", std::string()));
        const std::size_t max = static_cast<std::size_t>(std::clamp(match.value("max", 30), 1, 50));
        if (found.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no project matches under " + under.string());
        if (found.size() > max)
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, std::to_string(found.size()) + " projects match (more than "
                + std::to_string(max) + "): a narrower match (name, has) or the folders given one by one");
        cluster::Json projects = cluster::Json::array();
        std::size_t created = 0;
        for (const std::filesystem::path& folder: found) {
            std::string session;
            for (const cluster::Snapshot& snap: this->list(false))
                if (snap.spec.cwd == folder.string()) session = snap.spec.id;
            created += session.empty() ? 1 : 0;
            projects.push_back({{"folder", cluster::short_path(folder.string())}, {"session", session.empty() ? "new" : session}});
            wants.push_back({session.empty() ? folder.string() : session, folder.filename().string(), match.value("prompt", common), {}});
        }
        if ((found.size() > 10 || created > 0) && !args.value("confirmed", false))
            return {{"needs_confirmation", true}, {"projects", projects}, {"new_sessions", created},
                {"next", "show this list to the user (the sessions marked new are created); when they agree, call session_fanout again with the same "
                    "arguments and confirmed: true (or a narrower match)"}};
    }
    if (wants.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no task: tasks or match");
    for (const Want& want: wants)
        if (cluster::trim(want.prompt).empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "a task without its prompt");

    // the group and its tasks (the queue: a place each, as many at once as the parallel sessions allow)
    cluster::Group group;
    const cluster::Json wait = args.value("wait", cluster::Json::object());
    group.label = cluster::one_line(args.value("label", std::string("group")), 80);
    group.wait = wait.value("mode", std::string("all"));
    if (group.wait != "all" && group.wait != "first" && group.wait != "count")
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "wait.mode: all | first | count");
    group.count = std::max(1, wait.value("count", 1));
    const double minutes = wait.value("timeout_minutes", 0.0);
    group.deadline = minutes > 0 ? cluster::now() + static_cast<std::int64_t>(minutes * 60) : 0;
    group.stopOthers = wait.value("stop_others", false);
    group.retry = args.value("retry", true);
    group.merge = args.value("merge", std::string("here"));
    group.table = args.value("table", false);
    group.created = cluster::now();
    cluster::Json made = cluster::Json::array();
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        const std::size_t waiting = std::count_if(this->_tasks.begin(), this->_tasks.end(), [](const cluster::Task& t) {return t.status == "queued";});
        if (waiting + wants.size() > static_cast<std::size_t>(std::max(1, this->_config.taskQueue)))
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the task queue is full (" + std::to_string(waiting) + " waiting, "
                + std::to_string(this->_config.taskQueue) + " at most: [sessions] task_queue)");
        group.id = "f" + std::to_string(this->_nextGroup++);
        std::vector<std::string> ids;
        for (const Want& want: wants) {
            cluster::Task task;
            task.id = "t" + std::to_string(this->_nextTask++);
            task.prompt = "[from the global session] (group " + group.id + ": " + group.label + ") " + want.prompt;
            task.target = want.target;
            task.created = cluster::now();
            task.group = group.id;
            task.label = want.label;
            ids.push_back(task.id);
            this->_tasks.push_back(task);
        }
        for (std::size_t i = 0; i < wants.size(); ++i)
            for (const int a: wants[i].after)
                if (a >= 0 && static_cast<std::size_t>(a) < ids.size() && static_cast<std::size_t>(a) != i)
                    std::find_if(this->_tasks.begin(), this->_tasks.end(), [&](const cluster::Task& t) {return t.id == ids[i];})->after.push_back(ids[static_cast<std::size_t>(a)]);
        group.tasks = ids;
        for (std::size_t i = 0; i < ids.size(); ++i)
            made.push_back({{"task", ids[i]}, {"target", wants[i].label}});
        this->_groups.push_back(group);
    }
    // its progress card, in the transcript of the global session
    try {
        const std::string card = this->display(GLOBAL_ID, {{"type", "steps"}, {"title", "group " + group.id + " · " + group.label}, {"steps", cluster::Json::array()}});
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (cluster::Group& g: this->_groups)
            if (g.id == group.id) g.card = card;
    } catch (const utils::exception::IException&) {}
    this->notice_(GLOBAL_ID, "group " + group.id + " (" + group.label + "): " + std::to_string(wants.size()) + " task(s) queued");
    this->save();
    this->schedule_();
    this->groupUpdate_(group.id);
    return {{"group", group.id}, {"tasks", made},
        {"next", "end your turn: you are woken with their answers as [group " + group.id + " ...] (group_status for its progress, group_cancel to stop it)"}};
}

_cold cluster::Json cluster::Manager::groupStatus(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    cluster::Json list = cluster::Json::array();
    for (const cluster::Group& group: this->_groups) {
        if (!id.empty() && group.id != id) continue;
        cluster::Json tasks = cluster::Json::array();
        for (const std::string& taskId: group.tasks)
            for (const cluster::Task& t: this->_tasks)
                if (t.id == taskId)
                    tasks.push_back({{"task", t.id}, {"target", t.label}, {"session", t.session}, {"status", t.status}, {"retries", t.retries},
                        {"answer", cluster::one_line(t.answer, 300)}});
        list.push_back({{"group", group.id}, {"label", group.label}, {"wait", group.wait}, {"woken", group.woken}, {"finished", group.finished}, {"tasks", tasks}});
    }
    if (!id.empty() && list.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no group " + id);
    return id.empty() ? list : list[0];
}

_cold void cluster::Manager::groupCancel(const std::string& id, const bool interrupt)
{
    // its tasks not started: cancelled; the running ones: interrupted when asked (else their answers still come)
    std::vector<std::string> running;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto group = std::find_if(this->_groups.begin(), this->_groups.end(), [&](const cluster::Group& g) {return g.id == id;});
        if (group == this->_groups.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no group " + id);
        for (cluster::Task& t: this->_tasks) {
            if (t.group != id) continue;
            if (t.status == "queued") t.status = "cancelled";
            else if (t.status == "running" && interrupt) running.push_back(t.session);
        }
        group->woken = true; // asked to stop: no wake up with it
    }
    for (const std::string& session: running)
        try {
            this->interrupt(session);
        } catch (const utils::exception::IException&) {}
    this->save();
    this->groupUpdate_(id);
}

_cold void cluster::Manager::groupsTick_(void)
{
    // a group with a time: woken with what came when it is over
    std::vector<std::string> due;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const cluster::Group& group: this->_groups)
            if (!group.woken && group.deadline > 0 && group.deadline <= cluster::now()) due.push_back(group.id);
    }
    for (const std::string& id: due)
        this->groupUpdate_(id);
}

_cold void cluster::Manager::groupUpdate_(const std::string& id)
{
    // its card; the global session woken once when its wait is met (the others' answers later, together); a notification at its end
    std::string wake;
    std::string card;
    cluster::Json steps = cluster::Json::array();
    std::vector<std::string> stop;
    bool finishedNow = false;
    std::string summary;    // its progress (the title of its card, its notification)
    std::string merge;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto group = std::find_if(this->_groups.begin(), this->_groups.end(), [&](const cluster::Group& g) {return g.id == id;});
        if (group == this->_groups.end()) return;
        std::vector<cluster::Task*> tasks;
        for (const std::string& taskId: group->tasks)
            for (cluster::Task& t: this->_tasks)
                if (t.id == taskId) tasks.push_back(&t);
        std::size_t done = 0;
        std::size_t over = 0;
        for (const cluster::Task* t: tasks) {
            done += t->status == "done" ? 1 : 0;
            over += t->status == "done" || t->status == "failed" || t->status == "cancelled" ? 1 : 0;
            const std::string state = t->status == "done" ? "done" : t->status == "running" ? "current" : t->status == "failed" ? "error" : "todo";
            std::string text = t->label + " - " + cluster::one_line(t->prompt.substr(t->prompt.find(") ") == std::string::npos ? 0 : t->prompt.find(") ") + 2), 60);
            if (t->status == "done" || t->status == "failed") text += " (" + took_(t->started, t->ended) + (t->retries > 0 ? ", tried again" : "") + ")";
            else if (t->status == "queued") text += t->after.empty() ? " (waiting for a free place)" : " (after its dependencies)";
            else if (t->status == "cancelled") text += " (cancelled)";
            steps.push_back({{"text", text}, {"state", state}});
        }
        card = group->card;
        summary = group->label + " · " + std::to_string(done) + "/" + std::to_string(tasks.size()) + " done";
        const bool finished = over == tasks.size();
        const bool met = finished || (group->wait == "first" && done >= 1) || (group->wait == "count" && done >= static_cast<std::size_t>(group->count))
            || (group->deadline > 0 && group->deadline <= cluster::now());
        const std::function<std::string(const cluster::Task*)> answer = [&](const cluster::Task* t) {
            const std::optional<cluster::Snapshot> snap = this->_sessions.contains(t->session) ? std::optional<cluster::Snapshot>(this->_sessions.at(t->session)->snapshot()) : std::nullopt;
            std::string text = "--- " + t->label + " (" + t->session + (snap ? ", " + cluster::short_path(snap->spec.cwd) : std::string()) + ") · "
                + (t->status == "done" ? "done in " : "failed after ") + took_(t->started, t->ended) + "\n";
            return text + (t->answer.size() > 6000 ? t->answer.substr(0, 6000) + "\n[...]" : t->answer) + "\n";
        };
        if (met && !group->woken) {
            group->woken = true;
            std::string still;
            for (cluster::Task* t: tasks) {
                if (t->status == "done" || t->status == "failed") {
                    wake += answer(t);
                    group->told.insert(t->id);
                } else if (t->status == "queued" || t->status == "running") {
                    still += (still.empty() ? "" : ", ") + t->label;
                    if (group->stopOthers) {
                        if (t->status == "running") stop.push_back(t->session);
                        t->status = "cancelled";
                    }
                }
            }
            const std::string head = "[group " + group->id + " \"" + group->label + "\": " + std::to_string(done) + "/" + std::to_string(tasks.size()) + " done"
                + (group->deadline > 0 && group->deadline <= cluster::now() && !finished ? ", its time is over" : "") + "]\n";
            const std::string tail = still.empty() ? std::string() : group->stopOthers ? "stopped: " + still + "\n"
                : "still working: " + still + " (their answers come later as [group " + group->id + ": the other answers])\n";
            wake = head + wake + tail;
            merge = group->merge;
            if (group->merge != "session")
                wake += "Merge these answers into one answer for the user, in their language: by project when they differ, what they share once, "
                    "conflicts and failures named" + std::string(group->table ? "; also a comparison table of them in your transcript (session_display, session "
                    "\"global\", type table)" : "") + ".";
        } else if (group->woken && finished && !group->finished) {
            std::string late;
            for (cluster::Task* t: tasks)
                if ((t->status == "done" || t->status == "failed") && !group->told.contains(t->id)) {
                    late += answer(t);
                    group->told.insert(t->id);
                }
            if (!late.empty()) wake = "[group " + group->id + ": the other answers]\n" + late + "Complete your merged answer with them.";
            merge = "here";
        }
        if (finished && !group->finished) {
            group->finished = true;
            finishedNow = true;
        }
    }
    if (!card.empty())
        try {
            this->displayUpdate(GLOBAL_ID, card, {{"type", "steps"}, {"title", "group " + id + " · " + summary}, {"steps", steps}});
        } catch (const utils::exception::IException&) {}
    for (const std::string& session: stop)
        try {
            this->interrupt(session);
        } catch (const utils::exception::IException&) {}
    if (!wake.empty()) {
        std::thread([this, wake, merge, id]() {
            try {
                if (merge == "session") {
                    // a big merge: done by a session of its own, its answer to the global session (its context kept small)
                    const std::string merger = this->spawn({"~", "merge-" + id, "", "", "", "", ""});
                    this->replyTo(merger);
                    this->send(merger, "[from the global session] Merge these answers into one answer (by project when they differ, what they share once, "
                        "conflicts and failures named), compact, for the global session that rewrites it for the user.\n\n" + wake);
                    this->notice_(GLOBAL_ID, "group " + id + ": merged by " + merger);
                } else {
                    this->send(GLOBAL_ID, wake, true);
                }
            } catch (const utils::exception::IException& e) {
                this->notice_(GLOBAL_ID, std::string("group " + id + " not given to the global session: ") + e.info(), true);
            }
        }).detach();
    }
    if (finishedNow) (void)this->notify({"group-" + id, "group " + summary, "every task of the group is over", cluster::Face::Grin, false, "complete"}, "task");
    ++this->_version;
}
