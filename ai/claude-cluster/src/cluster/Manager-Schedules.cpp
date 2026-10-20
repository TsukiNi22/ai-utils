/**************************************************************\
Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Manager-Schedules.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Manager.hpp"
#include "cluster/Options.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Cron.hpp"
#include "cluster/Wire.hpp"
#include <filesystem>
#include <functional>
#include <algorithm>
#include <regex>
#include <map>
#include <set>

/* rules of time */
namespace {

namespace opt = cluster::opt;

constexpr std::int64_t TASK_ARCHIVE_SECONDS = 1800;     // a finished task stays in the list this long, then archived
constexpr std::size_t HISTORY_RUNS = 10;                // the runs kept in a schedule
constexpr int MAX_ASKS = 12;                            // a reminder asking again until acknowledged: at most this many times
constexpr std::int64_t RECHECK_SECONDS = 300;           // a one-time schedule whose window / condition is not met: looked at again

_cold std::int64_t when_(const std::string& when, const std::string& tz)
{
    // +30m / +2h / +1d / +90s, HH:MM (today, else tomorrow), YYYY-MM-DD HH:MM (in its zone), unix seconds
    const std::string w = cluster::trim(when);
    const std::int64_t now = cluster::now();
    std::smatch match;
    if (std::regex_match(w, match, std::regex(R"(\+?(\d+)\s*([smhd]))"))) {
        static const std::map<std::string, std::int64_t> units = {{"s", 1}, {"m", 60}, {"h", 3600}, {"d", 86400}};
        return now + std::stoll(match[1]) * units.at(match[2]);
    }
    if (std::regex_match(w, std::regex(R"(\d{9,})"))) return std::stoll(w);
    const std::int64_t at = cluster::zone_parse(w, tz, now);
    if (at > 0) return at;
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument,
        "when: +30m, +2h, +1d, HH:MM (today, else tomorrow), YYYY-MM-DD HH:MM - not '" + when + "'");
}

// the next time after t by the rules of w (its cron, or its every counted from base), inside its days / windows (0: none)
_cold std::int64_t next_after_(const cluster::Schedule& w, const std::int64_t base, const std::int64_t t)
{
    if (!w.cron.empty()) {
        std::int64_t from = t;
        for (int i = 0; i < 60; ++i) {
            const std::int64_t next = cluster::cron_next(w.cron, w.tz, from);
            if (next == 0 || cluster::time_allowed(w.days, w.window, w.quiet, w.tz, next)) return next;
            from = next;
        }
        return 0;
    }
    if (w.every <= 0) return 0;
    const std::int64_t step = static_cast<std::int64_t>(w.every) * 60;
    std::int64_t next = t < base ? base : base + ((t - base) / step + 1) * step;
    if (!cluster::time_allowed(w.days, w.window, w.quiet, w.tz, next)) next = cluster::time_next_allowed(w.days, w.window, w.quiet, w.tz, next);
    return next;
}

// the keys of the options of an action, given in "options" or at the top
const std::vector<std::string>& option_keys_(void)
{
    static const std::vector<std::string> keys = {"cwd", "name", "backend", "mode", "effort", "profile", "one_shot", "target", "after", "after_sessions", "on_event", "priority",
        "timeout_minutes", "retry", "retry_delay_minutes", "max_concurrency", "lane", "max_cost", "max_tokens", "expect", "notify", "reply", "command", "output", "timeout_seconds"};
    return keys;
}

_cold std::string usage_cond_check_(const std::string& usage)
{
    static const std::regex pattern(R"((5h|7d)\s*[<>]\s*\d+(\.\d+)?\s*%?)");
    return std::regex_match(usage, pattern) ? "" : "if.usage: 5h>80 or 7d<30 (a limit window used above / below a percent), not '" + usage + "'";
}

} // namespace

/* the fields of a schedule */
_cold void cluster::Manager::applySchedule_(cluster::Schedule& w, const cluster::Json& o, const bool creating, const std::int64_t now)
{
    const std::function<void(const std::string&)> bad = [](const std::string& why) {
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, why);
    };
    bool timing = creating;     // its next time to compute again
    if (o.contains("action")) {
        w.action = cluster::lower(opt::text(o, "action"));
        if (w.action.empty()) w.action = "prompt";
        static const std::set<std::string> actions = {"prompt", "notify", "spawn", "task", "command"};
        if (!actions.contains(w.action)) bad("action: prompt | notify | spawn | task | command");
    }
    if (o.contains("text")) w.text = opt::text(o, "text");
    if (o.contains("session") || creating) {
        const std::string session = opt::text(o, "session");
        w.session = w.action == "prompt" || w.action == "command" ? this->resolve(session.empty() ? std::string(GLOBAL_ID) : session) : "";
    }
    if (o.contains("every")) {
        w.every = static_cast<int>(std::clamp<std::int64_t>(opt::number(o, "every", 0), 0, 5256000));
        timing = true;
    }
    if (o.contains("cron")) {
        w.cron = cluster::trim(opt::text(o, "cron"));
        if (!w.cron.empty() && !cluster::cron_check(w.cron).empty()) bad(cluster::cron_check(w.cron));
        timing = true;
    }
    if (o.contains("days") || o.contains("weekdays")) {
        w.days = cluster::trim(opt::text(o, o.contains("days") ? "days" : "weekdays"));
        if (!cluster::days_check(w.days).empty()) bad(cluster::days_check(w.days));
        timing = true;
    }
    for (const char* key: {"window", "quiet"}) {
        if (!o.contains(key)) continue;
        (std::string(key) == "window" ? w.window : w.quiet) = cluster::trim(opt::text(o, key));
        if (!cluster::window_check(opt::text(o, key)).empty()) bad(cluster::window_check(opt::text(o, key)));
        timing = true;
    }
    if (o.contains("tz") || o.contains("timezone")) {
        w.tz = cluster::trim(opt::text(o, o.contains("tz") ? "tz" : "timezone"));
        if (!cluster::zone_check(w.tz).empty()) bad(cluster::zone_check(w.tz));
        timing = true;
    }
    if (o.contains("until")) w.until = opt::text(o, "until").empty() || opt::text(o, "until") == "0" ? 0 : when_(opt::text(o, "until"), w.tz);
    if (o.contains("max_runs")) w.maxRuns = static_cast<int>(std::clamp<std::int64_t>(opt::number(o, "max_runs", 0), 0, 1000000));
    if (o.contains("ack")) w.ack = opt::flag(o, "ack", false);
    if (o.contains("nag")) w.nag = static_cast<int>(std::clamp<std::int64_t>(opt::number(o, "nag", 10), 1, 1440));
    if (o.contains("reset")) {
        w.reset = cluster::lower(opt::text(o, "reset"));
        if (!w.reset.empty() && w.reset != "5h" && w.reset != "7d") bad("reset: 5h | 7d (the limit window whose reset triggers it)");
        timing = true;
    }
    if (const auto it = o.find("if"); it != o.end() && it->is_object()) {
        w.cond.sessionIdle = opt::text(*it, "session_idle");
        w.cond.command = opt::text(*it, "command");
        w.cond.fileChanged = opt::text(*it, "file_changed");
        w.cond.fileBase = w.cond.fileChanged.empty() ? 0 : opt::mtime(w.cond.fileChanged);
        w.cond.usage = cluster::trim(opt::text(*it, "usage"));
        if (!w.cond.usage.empty() && !usage_cond_check_(w.cond.usage).empty()) bad(usage_cond_check_(w.cond.usage));
        if (!w.cond.sessionIdle.empty()) (void)this->resolve(w.cond.sessionIdle);
    }
    // the options of spawn / task / command: in "options" or at the top
    if (const auto it = o.find("options"); it != o.end() && it->is_object()) w.options = *it;
    for (const std::string& key: option_keys_())
        if (o.contains(key)) w.options[key] = o.at(key);
    if (w.action == "command" && cluster::trim(opt::text(w.options, "command")).empty()) bad("action command: the command to run (command)");
    if (w.action == "spawn") {
        if (opt::text(w.options, "cwd").empty()) bad("action spawn: the project folder (cwd)");
        (void)cluster::resolve_folder(opt::text(w.options, "cwd"));
    }
    if (w.action != "command" && cluster::trim(w.text).empty()) bad("schedule: the text is empty");

    if (!timing) return;
    const std::string when = opt::text(o, "when");
    std::int64_t at = 0;
    if (!w.reset.empty()) {
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            at = this->resetAt_(w.reset);
        }
        w.polling = at == 0;
        if (at == 0) bad("reset " + w.reset + ": the reset time of that limit is not known yet (usage not read)");
    } else if (!w.cron.empty()) {
        at = next_after_(w, w.at, now);
        if (at == 0) bad("cron '" + w.cron + "' never fires inside its days / windows");
    } else if (!when.empty()) {
        at = when_(when, w.tz);
        if (!cluster::time_allowed(w.days, w.window, w.quiet, w.tz, at)) at = std::max(at, cluster::time_next_allowed(w.days, w.window, w.quiet, w.tz, at));
    } else if (w.every > 0) {
        at = next_after_(w, now, now);
    } else if (creating || o.contains("every") || o.contains("cron") || o.contains("reset")) {
        bad("when (+30m, HH:MM, YYYY-MM-DD HH:MM), cron, every or reset is needed");
    } else {
        at = w.at; // only a rule of days / windows changed on a one-time schedule
        if (!cluster::time_allowed(w.days, w.window, w.quiet, w.tz, at)) at = std::max(at, cluster::time_next_allowed(w.days, w.window, w.quiet, w.tz, at));
    }
    if (at <= 0) bad("when: no time found");
    w.at = at;
    if (w.until > 0 && w.at > w.until) bad("the first time is after until");
}

_cold cluster::Json cluster::Manager::scheduleJson(const cluster::Schedule& w) const
{
    cluster::Json out = {{"id", w.id}, {"status", w.status}, {"action", w.action}, {"text", cluster::one_line(w.text, 200)}, {"by", w.by}, {"reason", w.reason}, {"archived", w.archived},
        {"runs", w.runs}, {"last", w.last}, {"skipped", w.skipped}, {"note", w.note}};
    if (w.action == "prompt" || w.action == "command") out["session"] = w.session;
    if (w.status == "waiting" || w.status == "paused") {
        out["at"] = cluster::zone_clock(w.at, w.tz);
        out["at_unix"] = w.at;
        out["in_minutes"] = (w.at - cluster::now()) / 60;
        cluster::Json next = cluster::Json::array();   // the times after it
        std::int64_t base = w.at;
        for (int i = 0; i < 3 && w.reset.empty(); ++i) {
            base = next_after_(w, w.at, base);
            if (base == 0) break;
            next.push_back(cluster::zone_clock(base, w.tz));
        }
        out["then"] = next;
    }
    if (w.every > 0) out["every_minutes"] = w.every;
    for (const auto &[key, value]: std::map<std::string, std::string>{{"cron", w.cron}, {"days", w.days}, {"window", w.window}, {"quiet", w.quiet}, {"tz", w.tz}, {"reset", w.reset}})
        if (!value.empty()) out[key] = value;
    if (w.until > 0) out["until"] = cluster::zone_clock(w.until, w.tz);
    if (w.maxRuns > 0) out["max_runs"] = w.maxRuns;
    if (w.ack) out["ack"] = {{"nag_minutes", w.nag}, {"pending", w.pending}, {"asked", w.asks}};
    cluster::Json cond = cluster::Json::object();
    if (!w.cond.sessionIdle.empty()) cond["session_idle"] = w.cond.sessionIdle;
    if (!w.cond.command.empty()) cond["command"] = w.cond.command;
    if (!w.cond.fileChanged.empty()) cond["file_changed"] = w.cond.fileChanged;
    if (!w.cond.usage.empty()) cond["usage"] = w.cond.usage;
    if (!cond.empty()) out["if"] = cond;
    if (!w.options.empty()) out["options"] = w.options;
    cluster::Json history = cluster::Json::array();
    for (std::size_t i = w.history.size() > 5 ? w.history.size() - 5 : 0; i < w.history.size(); ++i)
        history.push_back({{"at", cluster::zone_clock(w.history[i].at, w.tz)}, {"note", w.history[i].note}});
    out["history"] = history;
    out["rule"] = cluster::schedule_rule(w);
    return out;
}

_cold cluster::Json cluster::Manager::addScheduleJson(const cluster::Json& options, const std::string& by, const bool dry)
{
    cluster::Schedule w;
    const std::int64_t now = cluster::now();
    this->applySchedule_(w, options, true, now);
    w.by = by;
    if (dry) { // nothing made: when it would fire
        cluster::Json times = cluster::Json::array({cluster::zone_clock(w.at, w.tz)});
        std::int64_t base = w.at;
        for (int i = 0; i < 4 && w.reset.empty(); ++i) {
            base = next_after_(w, w.at, base);
            if (base == 0 || (w.until > 0 && base > w.until)) break;
            times.push_back(cluster::zone_clock(base, w.tz));
        }
        return {{"dry_run", true}, {"would_fire", times}, {"action", w.action}, {"in_minutes", (w.at - now) / 60}};
    }
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (!by.empty() && by != GLOBAL_ID && w.action == "spawn") {
            std::size_t waiting = std::count_if(this->_schedules.begin(), this->_schedules.end(), [&](const cluster::Schedule& s) {return s.by == by && s.action == "spawn" && s.status == "waiting";});
            if (this->spawnedBy_(by) + waiting >= static_cast<std::size_t>(this->_config.subSpawnCap))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the cap of sessions started by your tasks / schedules is reached ("
                    + std::to_string(this->_config.subSpawnCap) + ": [agent] sub_spawn_cap)");
        }
        w.id = "w" + std::to_string(this->_nextSchedule++);
        this->_schedules.push_back(w);
    }
    this->save();
    ++this->_version;
    return {{"id", w.id}, {"next", cluster::zone_clock(w.at, w.tz)}, {"in_minutes", (w.at - now) / 60}};
}

_cold std::string cluster::Manager::addSchedule(const std::string& when, const std::string& action, const std::string& session, const std::string& text, const int every,
    const std::string& by)
{
    return this->addScheduleJson({{"when", when}, {"action", action}, {"session", session}, {"text", text}, {"every", every}}, by).value("id", std::string());
}

_cold cluster::Json cluster::Manager::scheduleEdit(const std::string& id, const cluster::Json& patch)
{
    cluster::Schedule w;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_schedules.begin(), this->_schedules.end(), [&](const cluster::Schedule& s) {return s.id == id;});
        if (it == this->_schedules.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no schedule " + id);
        if (it->status != "waiting" && it->status != "paused") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is " + it->status + ": ended");
        w = *it;
    }
    cluster::Schedule changed = w;
    this->applySchedule_(changed, patch, false, cluster::now());
    if (patch.contains("pause")) changed.status = opt::flag(patch, "pause", false) ? "paused" : "waiting";
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_schedules.begin(), this->_schedules.end(), [&](const cluster::Schedule& s) {return s.id == id;});
        if (it == this->_schedules.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no schedule " + id);
        changed.busy = it->busy;
        changed.now = it->now;
        *it = changed;
    }
    this->save();
    ++this->_version;
    return this->scheduleJson(changed);
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

/* the limits of the account */
_cold std::int64_t cluster::Manager::resetAt_(const std::string& window) const
{
    // the lock held: what the account and the sessions told of the limits (usage() takes the lock itself)
    const auto limits = this->_limits.find("anthropic");
    if (limits == this->_limits.end()) return 0;
    const cluster::UsageReport& report = limits->second;
    const std::string name = window == "5h" ? "5 h" : "7 d";
    for (const cluster::UsageWindow& u: report.windows)
        if (u.name == name && u.reset > cluster::now()) return u.reset;
    const std::int64_t reset = window == "5h" ? report.reset5h : report.reset7d;
    return reset > cluster::now() ? reset : 0;
}

_cold bool cluster::Manager::conditionMet_(const cluster::ScheduleIf& cond, std::string& why)
{
    if (!cond.sessionIdle.empty()) {
        std::optional<cluster::Snapshot> snap;
        try {
            snap = this->snapshot(this->resolve(cond.sessionIdle));
        } catch (const utils::exception::IException&) {}
        if (!snap || snap->state != cluster::State::Idle || snap->queued > 0) {
            why = "session " + cond.sessionIdle + " is not idle";
            return false;
        }
    }
    if (!cond.command.empty()) {
        const cluster::Captured result = cluster::capture({"/bin/sh", "-c", cond.command}, "", 30);
        if (result.code != 0) {
            why = "`" + cluster::one_line(cond.command, 60) + "` exited " + std::to_string(result.code);
            return false;
        }
    }
    if (!cond.fileChanged.empty() && opt::mtime(cond.fileChanged) == cond.fileBase) {
        why = cond.fileChanged + " did not change";
        return false;
    }
    if (!cond.usage.empty()) {
        std::smatch match;
        if (std::regex_match(cond.usage, match, std::regex(R"((5h|7d)\s*([<>])\s*(\d+(?:\.\d+)?)\s*%?)"))) {
            const cluster::UsageReport report = this->usage("anthropic");
            double used = match[1] == "5h" ? report.limit5h : report.limit7d;
            for (const cluster::UsageWindow& u: report.windows)
                if (u.name == (match[1] == "5h" ? "5 h" : "7 d") && u.used >= 0.0) used = u.used;
            const double percent = used * 100.0;
            if (used < 0.0) {
                why = "the " + match[1].str() + " limit is not known";
                return false;
            }
            const double wanted = std::stod(match[3]);
            if (match[2] == ">" ? percent <= wanted : percent >= wanted) {
                why = "the " + match[1].str() + " limit is at " + std::to_string(static_cast<int>(percent)) + " %, not " + match[2].str() + " " + match[3].str();
                return false;
            }
        }
    }
    return true;
}

/* run */
_cold std::string cluster::Manager::execute_(const cluster::Schedule& w)
{
    const std::string tag = "[scheduled " + w.id + (w.pending && w.asks > 0 ? ", again " + std::to_string(w.asks + 1) + " - schedule_act ack stops it" : "") + "] ";
    if (w.action == "notify") {
        this->notice_(GLOBAL_ID, "scheduled " + w.id + ": " + w.text);
        (void)this->notify({"schedule-" + w.id, "reminder", w.text, cluster::Face::Attentive, false, "message-new-instant"}, "reminder");
        return "notification";
    }
    if (w.action == "spawn" || w.action == "task") {
        cluster::Json task = w.options;
        task["prompt"] = w.text;
        task["schedule"] = w.id;
        if (w.action == "spawn") {
            task["spawn"] = {{"cwd", opt::text(w.options, "cwd")}, {"name", opt::text(w.options, "name").empty() ? "sched-" + w.id : opt::text(w.options, "name")}, {"backend", opt::text(w.options, "backend")},
                {"mode", opt::text(w.options, "mode")}, {"effort", opt::text(w.options, "effort")}, {"profile", opt::text(w.options, "profile")}, {"one_shot", opt::flag(w.options, "one_shot", true)}};
        }
        return "task " + this->addTaskJson(task, w.by) + " queued";
    }
    if (w.action == "command") {
        // a program of the user, no model: only its output is sent, when there is one
        const std::string command = opt::text(w.options, "command");
        const cluster::Captured result = cluster::capture({"/bin/sh", "-c", command}, opt::text(w.options, "cwd"), static_cast<int>(std::clamp<std::int64_t>(opt::number(w.options, "timeout_seconds", 60), 1, 600)));
        const std::string out = cluster::trim(result.out);
        if (out.empty()) return "`" + cluster::one_line(command, 50) + "` exited " + std::to_string(result.code) + ", no output: nothing sent";
        const std::string shown = opt::cap(out, 4000);
        if (opt::text(w.options, "output") == "notify") {
            (void)this->notify({"schedule-" + w.id, w.text.empty() ? "scheduled command" : w.text, cluster::one_line(shown, 300), cluster::Face::Attentive, false, "message-new-instant"}, "reminder");
            return "output notified (exit " + std::to_string(result.code) + ")";
        }
        this->send(w.session, tag + (w.text.empty() ? "" : w.text + "\n") + "output of `" + cluster::one_line(command, 80) + "` (exit " + std::to_string(result.code) + "):\n" + shown, true);
        return "output sent to " + w.session + " (exit " + std::to_string(result.code) + ")";
    }
    this->send(w.session, tag + w.text, true);
    this->notice_(w.session, "scheduled " + w.id + " sent");
    return "prompt sent to " + w.session;
}

_cold void cluster::Manager::advance_(cluster::Schedule& w, const std::int64_t now, const bool fired)
{
    // its next time, or its end (under the lock)
    const std::function<void(const std::string&)> end = [&w](const std::string& why) {
        w.status = "done";
        w.reason = why;
    };
    if (w.until > 0 && now >= w.until) return end("until reached");
    if (w.maxRuns > 0 && w.runs >= w.maxRuns) return end("max_runs reached");
    if (!w.reset.empty()) {
        const std::int64_t reset = this->resetAt_(w.reset);
        w.polling = reset == 0;
        w.at = reset != 0 ? reset : now + 600; // the new reset time is not known yet: looked for every 10 minutes
    } else if (!w.cron.empty() || w.every > 0) {
        const std::int64_t next = next_after_(w, w.at, now);
        if (next == 0) return end("no next time");
        w.at = next;
    } else if (fired) {
        return end("ran once");
    } else {
        const std::int64_t at = now + RECHECK_SECONDS; // once, not run (a window, a condition): looked at again
        w.at = cluster::time_allowed(w.days, w.window, w.quiet, w.tz, at) ? at : std::max(at, cluster::time_next_allowed(w.days, w.window, w.quiet, w.tz, at));
    }
    if (w.until > 0 && w.at > w.until) end("until reached");
}

_cold void cluster::Manager::runSchedule_(const std::string& id)
{
    cluster::Schedule w;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_schedules.begin(), this->_schedules.end(), [&](const cluster::Schedule& s) {return s.id == id;});
        if (it == this->_schedules.end()) return;
        w = *it;
    }
    const std::int64_t now = cluster::now();
    const bool forced = w.now;
    std::string note;
    bool fire = true;
    bool recheck = false;   // reset: not a reset yet
    std::string why;
    if (!w.reset.empty() && !forced) {
        std::int64_t reset = 0;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            reset = this->resetAt_(w.reset);
        }
        if (w.polling && reset > 0) { // the new reset time is known now: waited for
            fire = false;
            recheck = true;
            w.at = reset;
            w.polling = false;
            note = "next reset of " + w.reset + " at " + cluster::zone_clock(reset, w.tz);
        } else if (w.polling) {
            fire = false;
            recheck = true;
            w.at = now + 600;
            note = "waiting for the reset time of " + w.reset;
        }
    }
    if (fire && !forced && !cluster::time_allowed(w.days, w.window, w.quiet, w.tz, now)) {
        fire = false;
        note = "skipped: outside its days / window";
    } else if (fire && !forced && !this->conditionMet_(w.cond, why)) {
        fire = false;
        note = "skipped: " + why;
    }
    if (fire) {
        try {
            note = this->execute_(w);
        } catch (const utils::exception::IException& e) {
            note = std::string("failed: ") + e.info();
            this->notice_(w.session, "scheduled " + w.id + " not sent: " + e.info(), true);
        }
    }
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_schedules.begin(), this->_schedules.end(), [&](const cluster::Schedule& s) {return s.id == id;});
        if (it == this->_schedules.end()) return;
        cluster::Schedule& s = *it;
        s.busy = false;
        s.now = false;
        s.note = note;
        if (recheck) {
            s.at = w.at;
            s.polling = w.polling;
        } else if (fire) {
            ++s.runs;
            s.last = now;
            s.history.push_back({now, note});
            if (s.history.size() > HISTORY_RUNS) s.history.erase(s.history.begin(), s.history.end() - static_cast<std::ptrdiff_t>(HISTORY_RUNS));
            if (!s.cond.fileChanged.empty()) s.cond.fileBase = opt::mtime(s.cond.fileChanged);
            if (s.status == "waiting" && s.ack && !(s.maxRuns > 0 && s.runs >= s.maxRuns)) { // asked again until acknowledged
                s.pending = true;
                if (++s.asks >= MAX_ASKS) {
                    s.pending = false;
                    s.asks = 0;
                    s.note += " (not acknowledged after " + std::to_string(MAX_ASKS) + " asks)";
                    this->advance_(s, now, true);
                } else {
                    s.at = now + static_cast<std::int64_t>(s.nag) * 60;
                }
            } else if (s.status == "waiting" && !(forced && s.at > now)) { // a run asked by hand keeps its next time
                this->advance_(s, now, true);
            }
        } else {
            ++s.skipped;
            if (s.pending) s.at = now + static_cast<std::int64_t>(s.nag) * 60;
            else this->advance_(s, now, false);
        }
    }
    this->save();
    ++this->_version;
}

_cold void cluster::Manager::pollEvents_(void)
{
    // the events of the queued tasks that need a look outside (a command, git, a file): not every second
    const std::int64_t now = cluster::now();
    if (now - this->_bgPoll < 5) return;
    this->_bgPoll = now;
    std::vector<std::pair<std::string, cluster::TaskEvent>> checks;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const cluster::Task& t: this->_tasks) {
            if (t.status != "queued" || t.on.met || t.notBefore > now) continue;
            const std::int64_t interval = t.on.type == "file" ? 5 : t.on.type == "command" ? 30 : t.on.type == "git_push" ? 60 : 0;
            if (interval > 0 && now - t.on.checked >= interval) checks.emplace_back(t.id, t.on);
        }
    }
    bool met = false;
    for (auto &[id, on]: checks) {
        on.checked = now;
        if (on.type == "file") {
            const bool exists = std::filesystem::exists(cluster::expand_home(on.arg));
            on.met = on.arg2 == "changed" ? exists && std::to_string(opt::mtime(on.arg)) != on.base : exists;
        } else if (on.type == "command") {
            on.met = cluster::capture({"/bin/sh", "-c", on.arg}, "", 30).code == 0;
        } else if (on.type == "git_push") {
            const std::string repo = cluster::expand_home(on.arg).string();
            std::string branch = on.arg2;
            if (branch.empty()) branch = cluster::trim(cluster::capture({"git", "-C", repo, "rev-parse", "--abbrev-ref", "HEAD"}, "", 10).out);
            const cluster::Captured remote = cluster::capture({"git", "-C", repo, "ls-remote", "origin", "refs/heads/" + branch}, "", 25);
            const std::string head = remote.code == 0 ? remote.out.substr(0, remote.out.find_first_of(" \t")) : "";
            if (on.base.empty()) on.base = head; // the commit when the task was queued (seen at the first look)
            else on.met = !head.empty() && head != on.base;
        }
        met |= on.met;
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (cluster::Task& t: this->_tasks)
            if (t.id == id) t.on = on;
    }
    if (!checks.empty()) {
        this->save();
        ++this->_version;
    }
    if (met) this->schedule_();
}

_cold void cluster::Manager::bgPass_(const std::vector<std::string>& due)
{
    this->pollEvents_();
    for (const std::string& id: due)
        this->runSchedule_(id);
}

_cold void cluster::Manager::fireSchedules_(void)
{
    // due: its action (a prompt wakes its session up, a notification, a task, a command), then again (every, cron) or ended; the checks
    // that may take a while (commands, git) run in a thread of their own, never in the second of the tick
    const std::int64_t now = cluster::now();
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (cluster::Task& task: this->_tasks) // the finished tasks leave the list after a while (their archive)
            if (!task.archived && task.status != "queued" && task.status != "running" && now - (task.ended > 0 ? task.ended : task.created) > TASK_ARCHIVE_SECONDS) task.archived = true;
    }
    this->enforceTasks_();
    if (this->_bgBusy.exchange(true)) return;
    std::vector<std::string> due;
    bool poll = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (cluster::Schedule& w: this->_schedules) {
            if (w.status != "waiting" || w.busy || (w.at > now && !w.now)) continue;
            w.busy = true;
            due.push_back(w.id);
        }
        poll = now - this->_bgPoll >= 5 && std::any_of(this->_tasks.begin(), this->_tasks.end(), [](const cluster::Task& t) {return t.status == "queued" && !t.on.type.empty() && !t.on.met && t.on.type != "session_idle";});
    }
    if (due.empty() && !poll) {
        this->_bgBusy = false;
        return;
    }
    if (this->_bgThread.joinable()) this->_bgThread.join();
    this->_bgThread = std::thread([this, due]() {
        try {
            this->bgPass_(due);
        } catch (...) {
            std::lock_guard<std::mutex> lock(this->_mutex); // never a thread ending on an exception: the schedules free again
            for (cluster::Schedule& w: this->_schedules)
                w.busy = false;
        }
        this->_bgBusy = false;
    });
}

_cold std::string cluster::Manager::scheduleAct(const std::string& id, const std::string& action, const std::string& reason)
{
    if (this->_replica) return this->call({{"cmd", "schedule_act"}, {"id", id}, {"action", action}, {"reason", reason}}).value("done", std::string());
    cluster::Schedule w;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = std::find_if(this->_schedules.begin(), this->_schedules.end(), [&](const cluster::Schedule& s) {return s.id == id;});
        if (it == this->_schedules.end()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no schedule " + id);
        const bool live = it->status == "waiting" || it->status == "paused";
        const std::function<void()> not_live = [&]() {
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is " + it->status + ": nothing to " + action);
        };
        if (action == "cancel") {
            if (!live) not_live();
            it->status = "cancelled";
            it->reason = reason;
        } else if (action == "trigger" || action == "run") {
            if (it->status != "waiting") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is " + it->status + ": " + (it->status == "paused" ? "resume it first" : "not waiting"));
            it->now = true; // run at the next pass (its conditions skipped); a repeated one keeps its next time
        } else if (action == "pause") {
            if (it->status != "waiting") not_live();
            it->status = "paused";
        } else if (action == "resume") {
            if (it->status != "paused") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is " + it->status + ": not paused");
            it->status = "waiting";
            if (it->at <= cluster::now() && (it->every > 0 || !it->cron.empty())) this->advance_(*it, cluster::now(), false); // missed times not run in a burst
        } else if (action == "snooze") {
            if (!live) not_live();
            std::int64_t minutes = 30;
            try {
                if (!cluster::trim(reason).empty()) minutes = std::clamp<std::int64_t>(std::stoll(cluster::trim(reason)), 1, 43200);
            } catch (const std::exception&) {
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "snooze: the minutes, as a number");
            }
            it->at = cluster::now() + minutes * 60;
            it->pending = false;
            it->asks = 0;
        } else if (action == "ack") {
            if (!it->ack || !it->pending) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, id + " is not waiting for an acknowledgement");
            it->pending = false;
            it->asks = 0;
            this->advance_(*it, cluster::now(), true);
        } else if (action == "archive" || action == "unarchive") {
            it->archived = action == "archive";
        } else if (action != "delete") {
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "cancel | delete | archive | unarchive | trigger | pause | resume | snooze | ack");
        }
        w = *it;
        if (action == "delete") this->_schedules.erase(it);
    }
    this->save();
    ++this->_version;
    if (action == "trigger" || action == "run") this->fireSchedules_();
    const std::string what = action == "cancel" ? "cancelled" : action == "delete" ? "deleted" : action == "trigger" || action == "run" ? "run now" : action == "pause" ? "paused"
        : action == "resume" ? "resumed" : action == "snooze" ? "snoozed" : action == "ack" ? "acknowledged" : action + "d";
    if ((action == "cancel" || action == "delete") && !reason.empty())
        this->tell_(w.by, "[tasks] your schedule " + id + " (\"" + cluster::one_line(w.text, 80) + "\") was " + what + " by the user: " + reason);
    else
        this->notice_("", "schedule " + id + " " + what);
    return "schedule " + id + " " + what;
}
