/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Orchestrator-Settings.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Orchestrator.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Tools.hpp"

/* tools */
namespace {

using cluster::Setting;
using cluster::SettingKind;

_cold Setting flag_(const std::string& key, const std::string& label, const std::string& help, bool cluster::OrchConf::* field)
{
    return {"orchestrator", key, label, SettingKind::Bool, {}, help, false, [field](const cluster::Config& cf) {return cf.orch.*field ? std::string("true") : std::string("false");}};
}

_cold Setting count_(const std::string& key, const std::string& label, const std::string& help, int cluster::OrchConf::* field)
{
    return {"orchestrator", key, label, SettingKind::Int, {}, help, false, [field](const cluster::Config& cf) {return std::to_string(cf.orch.*field);}};
}

_cold Setting ratio_(const std::string& key, const std::string& label, const std::string& help, double cluster::OrchConf::* field)
{
    return {"orchestrator", key, label, SettingKind::Float, {}, help, false, [field](const cluster::Config& cf) {
        char text[32];
        std::snprintf(text, sizeof(text), "%g", cf.orch.*field);
        return std::string(text);
    }};
}

_cold Setting list_(const std::string& key, const std::string& label, const std::string& help, std::vector<std::string> cluster::OrchConf::* field)
{
    return {"orchestrator", key, label, SettingKind::List, {}, help, false, [field](const cluster::Config& cf) {
        std::string out;
        for (const std::string& item: cf.orch.*field)
            out += (out.empty() ? "" : ", ") + item;
        return out;
    }};
}

} // namespace

_cold cluster::SettingsPage cluster::orchestrator_settings_page(void)
{
    static const char* classes[5] = {"build", "tests", "container", "gpu", "custom"};
    cluster::SettingsPage page{"orchestrator", "Orchestrator", {}};
    std::vector<Setting>& s = page.settings;
    s.push_back(flag_("enabled", "Orchestrator", "the heavy jobs of the sessions (builds, tests, containers / media / GPU) wait in a queue and take turns: cluster-run, heavy_run, jobs_add. "
        "Off: cluster-run runs its command at once", &cluster::OrchConf::enabled));
    s.push_back({"orchestrator", "profile", "Profile", SettingKind::Choice, {"low", "normal", "max"}, "how much of the computer the jobs take\n"
        "• low: 1 thread per job, nice 19, ionice idle, half the usable cores at once\n• normal: 2-4 threads, nice 10, one job per usable core\n• max: the share of cores, nice 5, 1.5 jobs per core", false,
        [](const cluster::Config& cf) {return cf.orch.profile;},
        [](const std::string& v) {return v == "low" ? std::string("the computer stays usable") : v == "normal" ? std::string("balanced") : std::string("as fast as the cores allow");}});
    s.push_back(count_("max_jobs", "Heavy jobs at once", "running at once (0: from the hardware, the profile, the live load and memory)", &cluster::OrchConf::maxJobs));
    s.push_back(count_("max_per_session", "Jobs per session", "running at once for one session (0: no limit)", &cluster::OrchConf::maxPerSession));
    s.push_back(count_("reserved_cpu", "Cores left to me", "kept free for you and the interface", &cluster::OrchConf::reservedCpu));
    s.push_back(count_("reserved_ram_mb", "Memory left to me (MB)", "kept free for you and the interface", &cluster::OrchConf::reservedRamMb));
    s.push_back(count_("threads", "Threads per job", "the parallelism granted to a job (-j, --parallel, CARGO_BUILD_JOBS...); 0: from the profile (low: 1)", &cluster::OrchConf::threads));
    for (int i = 0; i < 5; ++i) {
        const std::string name = classes[i];
        s.push_back({"orchestrator", name + "_max", "Max " + name + " jobs", SettingKind::Int, {}, "of this class running at once (0: no more than the total)", false,
            [i](const cluster::Config& cf) {return std::to_string(cf.orch.classMax[i]);}});
        s.push_back({"orchestrator", name + "_ram_mb", "Memory of a " + name + " job (MB)", SettingKind::Int, {}, "estimated: a job starts only when it fits in the memory available", false,
            [i](const cluster::Config& cf) {return std::to_string(cf.orch.classRam[i]);}});
    }
    s.push_back(list_("patterns", "Heavy commands", "more commands that go through the queue, comma separated: \"text\" (class custom) or \"text = class\" "
        "(build, tests, container, gpu, custom); a command containing it is heavy", &cluster::OrchConf::patterns));
    s.push_back(list_("session_prio", "Session priorities", "\"s3=5\": the jobs of a session come first (higher) or last (negative)", &cluster::OrchConf::sessionPrio));
    s.push_back(list_("session_quota", "Session quotas", "\"s3=1\": jobs of a session running at once", &cluster::OrchConf::sessionQuota));
    s.push_back(count_("aging_seconds", "Aging (seconds)", "a waiting job gains one point every so many seconds, a priority step is 10: nothing starves", &cluster::OrchConf::agingSeconds));
    s.push_back(count_("focus_boost", "Session shown first", "points of the session you look at: its jobs go first", &cluster::OrchConf::focusBoost));
    s.push_back(count_("detect_cpu_seconds", "Detect heavy commands (CPU s)", "a command outside the queue that burned this many CPU seconds is listed and told (0: off)", &cluster::OrchConf::detectCpuSeconds));
    s.push_back({"orchestrator", "detect_action", "When one is detected", SettingKind::Choice, {"off", "notice", "tell"}, "notice: a notice and the jobs list; tell: its session gets a prompt too", false,
        [](const cluster::Config& cf) {return cf.orch.detectAction;}});
    s.push_back(ratio_("hold_load", "Hold: load per core", "load average (1 min) per core above which nothing new starts", &cluster::OrchConf::holdLoad));
    s.push_back(ratio_("reduce_load", "Reduce: load per core", "above it the running jobs are reniced and the lowest priority one stopped until it falls", &cluster::OrchConf::reduceLoad));
    s.push_back(count_("hold_mem_mb", "Hold: memory available (MB)", "below it nothing new starts", &cluster::OrchConf::holdMemMb));
    s.push_back(count_("reduce_mem_mb", "Reduce: memory available (MB)", "below it the running jobs are slowed", &cluster::OrchConf::reduceMemMb));
    s.push_back(count_("oom_mem_mb", "Out of memory (MB)", "below it, with Kill on out-of-memory, the newest job is killed", &cluster::OrchConf::oomMemMb));
    s.push_back(count_("hold_swap_mb", "Hold: swap used (MB)", "above it, with the memory short too, nothing new starts", &cluster::OrchConf::holdSwapMb));
    for (const char* pressure: {"cpu", "mem", "io"}) {
        const std::string name = pressure;
        s.push_back({"orchestrator", "psi_" + name + "_hold", "Hold: " + name + " pressure (%)", SettingKind::Int, {}, "pressure stall information (avg10, /proc/pressure): nothing new starts above it", false,
            [name](const cluster::Config& cf) {return std::to_string(name == "cpu" ? cf.orch.psiCpuHold : name == "mem" ? cf.orch.psiMemHold : cf.orch.psiIoHold);}});
        s.push_back({"orchestrator", "psi_" + name + "_reduce", "Reduce: " + name + " pressure (%)", SettingKind::Int, {}, "the running jobs are slowed above it", false,
            [name](const cluster::Config& cf) {return std::to_string(name == "cpu" ? cf.orch.psiCpuReduce : name == "mem" ? cf.orch.psiMemReduce : cf.orch.psiIoReduce);}});
    }
    s.push_back(count_("resume_after", "Resume after (seconds)", "calm for so long before the stopped jobs resume (hysteresis)", &cluster::OrchConf::resumeAfter));
    s.push_back(flag_("kill_on_oom", "Kill on out-of-memory", "under the out-of-memory threshold the newest job is killed (never by default: the jobs are only slowed)", &cluster::OrchConf::killOnOom));
    s.push_back(flag_("sub_add", "Sub-sessions may add jobs", "heavy_run, jobs_add for the sub-sessions", &cluster::OrchConf::subAdd));
    s.push_back(flag_("sub_kill_own", "Sub-sessions may kill their jobs", "job_kill on their own jobs", &cluster::OrchConf::subKillOwn));
    s.push_back(flag_("sub_see_others", "Sub-sessions see the other jobs", "jobs_list / job_status of the jobs of the other sessions", &cluster::OrchConf::subSeeOthers));
    s.push_back(flag_("notify_killed", "Notify a killed job", "a notification (desktop, app) when a job is killed", &cluster::OrchConf::notifyKilled));
    s.push_back(flag_("notify_done", "Notify a finished job", "a notification (desktop, app) when a job finishes", &cluster::OrchConf::notifyDone));
    s.push_back(count_("history", "Finished jobs kept", "the list of the finished jobs", &cluster::OrchConf::history));
    return page;
}
