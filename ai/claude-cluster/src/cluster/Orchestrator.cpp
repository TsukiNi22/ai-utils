/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Orchestrator.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Orchestrator.hpp"
#include "cluster/RunProfile.hpp"
#include "cluster/Tools.hpp"
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <csignal>
#include <fstream>
#include <cstring>
#include <chrono>
#include <cmath>

extern char** environ;

namespace fs = std::filesystem;

/* tools */
namespace {

constexpr const char* CLASSES[5] = {"build", "tests", "container", "gpu", "custom"};

_cold int class_index_(const std::string& cls)
{
    for (int i = 0; i < 5; ++i)
        if (cls == CLASSES[i]) return i;
    return 4;
}

_cold bool ended_(const cluster::Job& job) // xstyle: ignore CPP-SINGLE-STATEMENT (helper of this file)
{
    return job.state == "done" || job.state == "failed" || job.state == "killed";
}

_cold std::string duration_(const std::int64_t ms)
{
    const std::int64_t s = std::max<std::int64_t>(0, ms) / 1000;
    if (s < 60) return std::to_string(s) + "s";
    if (s < 3600) return std::to_string(s / 60) + "m" + std::to_string(s % 60) + "s";
    return std::to_string(s / 3600) + "h" + std::to_string(s % 3600 / 60) + "m";
}

// "s3=5" -> 5 for s3 (0 when missing)
_cold int pair_(const std::vector<std::string>& list, const std::string& session)
{
    for (const std::string& item: list) {
        const std::size_t eq = item.find('=');
        if (eq != std::string::npos && cluster::trim(item.substr(0, eq)) == session) return static_cast<int>(std::strtol(cluster::trim(item.substr(eq + 1)).c_str(), nullptr, 10));
    }
    return 0;
}

_cold std::uint64_t start_of_(const int pid)
{
    // the start time of a process (field 22 of its stat): the same process later, not a pid used again
    const std::string line = cluster::read_small("/proc/" + std::to_string(pid) + "/stat");
    const std::size_t close = line.rfind(')');
    if (close == std::string::npos) return 0;
    std::vector<std::string> fields = cluster::split(line.substr(close + 2), ' ');
    return fields.size() > 19 ? std::strtoull(fields[19].c_str(), nullptr, 10) : 0;
}

_cold cluster::Json to_json_(const cluster::Job& j)
{
    return {{"id", j.id}, {"name", j.name}, {"class", j.cls}, {"session", j.session}, {"origin", j.origin}, {"command", j.command}, {"argv", j.argv}, {"cwd", j.cwd},
        {"prio", j.prio}, {"state", j.state}, {"reason", j.reason}, {"by", j.by}, {"stopped_by", j.stoppedBy}, {"created", j.created}, {"started", j.started},
        {"ended", j.ended}, {"stopped_at", j.stoppedAt}, {"stopped_ms", j.stoppedMs}, {"pid", j.pid}, {"pid_start", j.pidStart}, {"code", j.code}, {"threads", j.threads},
        {"ram_mb", j.ramMb}, {"timeout", j.timeoutSeconds}, {"callback", j.callback}, {"tail", j.tail}, {"log", j.log}};
}

_cold cluster::Job from_json_(const cluster::Json& o)
{
    cluster::Job j;
    j.id = o.value("id", std::string());
    j.name = o.value("name", std::string());
    j.cls = o.value("class", std::string("custom"));
    j.session = o.value("session", std::string());
    j.origin = o.value("origin", std::string("run"));
    j.command = o.value("command", std::string());
    j.argv = o.value("argv", std::vector<std::string>());
    j.cwd = o.value("cwd", std::string());
    j.prio = o.value("prio", 0);
    j.state = o.value("state", std::string("failed"));
    j.reason = o.value("reason", std::string());
    j.by = o.value("by", std::string());
    j.stoppedBy = o.value("stopped_by", std::string());
    j.created = o.value("created", std::int64_t{0});
    j.started = o.value("started", std::int64_t{0});
    j.ended = o.value("ended", std::int64_t{0});
    j.stoppedAt = o.value("stopped_at", std::int64_t{0});
    j.stoppedMs = o.value("stopped_ms", std::int64_t{0});
    j.pid = o.value("pid", 0);
    j.pidStart = o.value("pid_start", std::uint64_t{0});
    j.code = o.value("code", -1);
    j.threads = o.value("threads", 0);
    j.ramMb = o.value("ram_mb", 0);
    j.timeoutSeconds = o.value("timeout", 0);
    j.callback = o.value("callback", false);
    j.tail = o.value("tail", std::string());
    j.log = o.value("log", std::string());
    return j;
}

_cold fs::path state_file_(void) // xstyle: ignore CPP-SINGLE-STATEMENT (helper of this file)
{
    return cluster::data_dir() / "orchestrator.json";
}

} // namespace

/* constructor */
_cold cluster::Orchestrator::Orchestrator(cluster::Config& config, cluster::Orchestrator::Hooks hooks)
    : _config(config), _hooks(std::move(hooks))
{
}

_cold cluster::Orchestrator::~Orchestrator()
{
    this->stop();
}

/* life */
_cold void cluster::Orchestrator::start(void)
{
    this->load();
    this->rescan();
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_host = cluster::host_sample(this->_config.orch.procDir);
    }
    this->_running = true;
    this->_thread = std::thread(&cluster::Orchestrator::loop_, this);
}

_cold void cluster::Orchestrator::stop(void)
{
    if (this->_running.exchange(false) && this->_thread.joinable()) this->_thread.join();
    this->save();
}

_cold void cluster::Orchestrator::rescan(void)
{
    const cluster::OrchConf c = this->_config.orch;
    const cluster::Hardware hardware = cluster::hardware_scan(c.procDir, c.sysDir);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_hardware = hardware;
        this->touch_();
    }
    try {
        cluster::write_json(cluster::data_dir() / "hardware.json", cluster::hardware_json(hardware));
    } catch (const utils::exception::IException&) {}
}

_cold void cluster::Orchestrator::save(void)
{
    cluster::Json jobs = cluster::Json::array();
    int next = 0;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const cluster::Job& job: this->_jobs)
            jobs.push_back(to_json_(job));
        next = this->_next;
        this->_dirty = false;
        this->_lastSave = cluster::now_ms();
    }
    try {
        cluster::write_json(state_file_(), {{"next", next}, {"jobs", jobs}});
    } catch (const utils::exception::IException&) {}
}

_cold void cluster::Orchestrator::load(void)
{
    // the jobs of the previous run: what a dead core left is settled (docs/orchestrator.md, "Persistence")
    const std::optional<cluster::Json> state = cluster::read_json(state_file_());
    if (!state) return;
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_next = state->value("next", 1);
    const std::int64_t now = cluster::now_ms();
    for (const cluster::Json& o: state->value("jobs", cluster::Json::array())) {
        cluster::Job job = from_json_(o);
        if (job.id.empty()) continue;
        if (job.state == "queued" && job.origin == "wrap") {
            job.state = "failed";
            job.by = "restart";
            job.reason = "core restarted: run it again";
            job.ended = now;
        } else if (job.state == "running" || job.state == "stopped") {
            if (job.origin == "run" && job.pid > 1 && start_of_(job.pid) == job.pidStart && job.pidStart != 0) { // its output went with the core: stopped for good
                ::kill(-job.pid, SIGCONT);
                ::kill(-job.pid, SIGKILL);
            }
            job.state = "failed";
            job.by = "restart";
            job.reason = job.origin == "run" ? "core restarted: its output was lost, run it again" : "core restarted: its command, if it still runs, is no longer counted";
            job.ended = now;
        }
        this->_jobs.push_back(std::move(job));
    }
    this->touch_();
}

_cold void cluster::Orchestrator::loop_(void)
{
    std::int64_t lastSecond = 0;
    while (this->_running) {
        this->reap_();
        const std::int64_t now = cluster::now_ms();
        if (now - lastSecond >= 1000) {
            lastSecond = now;
            this->second_();
        }
        this->flush_();
        std::unique_lock<std::mutex> lock(this->_mutex);
        this->_cv.wait_for(lock, std::chrono::milliseconds(250));
    }
    this->flush_();
}

/* effects on the manager, never run under the lock */
_cold void cluster::Orchestrator::post_(std::function<void(void)> effect)
{
    std::lock_guard<std::mutex> lock(this->_outMutex);
    this->_outbox.push_back(std::move(effect));
}

_cold void cluster::Orchestrator::flush_(void)
{
    std::vector<std::function<void(void)>> effects;
    {
        std::lock_guard<std::mutex> lock(this->_outMutex);
        effects.swap(this->_outbox);
    }
    for (const std::function<void(void)>& effect: effects)
        effect();
}

_cold void cluster::Orchestrator::touch_(void)
{
    ++this->_version;
    this->_dirty = true;
    this->_cv.notify_all();
    if (this->_hooks.changed) this->post_(this->_hooks.changed);
}

/* the second: sample, pressure, schedule, save */
_cold void cluster::Orchestrator::second_(void)
{
    const cluster::OrchConf c = this->_config.orch;
    if (this->_hooks.active) this->_active = this->_hooks.active();
    const bool scan = cluster::now_ms() - this->_lastScan >= 5000;
    if (scan && this->_hooks.sessions) this->_agents = this->_hooks.sessions();
    const cluster::Host host = cluster::host_sample(c.procDir);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_host = host;
        const std::int64_t now = cluster::now_ms();
        for (cluster::Job& job: this->_jobs) // a timeout
            if ((job.state == "running" || job.state == "stopped") && job.timeoutSeconds > 0 && job.started > 0
                && now - job.started - job.stoppedMs > static_cast<std::int64_t>(job.timeoutSeconds) * 1000)
                this->kill_(job, "timeout", "ran past its timeout of " + std::to_string(job.timeoutSeconds) + " s");
        if (scan) this->processes_(c);
        this->pressure_(c);
        this->schedule_();
        this->trim_(c);
    }
    if (this->_dirty) this->save();
}

/* the live level of pressure */
_cold int cluster::Orchestrator::level_(const cluster::OrchConf& c, const double scale) const
{
    // 0 ok, 1 hold, 2 reduce; scale < 1: the thresholds tightened (the way back: hysteresis)
    const cluster::Host& h = this->_host;
    const double hold = scale;
    const double mem = 1.0 / scale;
    const std::function<bool(double, int)> over = [](const double value, const double limit) {return value >= 0 && limit > 0 && value >= limit;};
    if (h.loadPerCore >= c.reduceLoad * hold || (c.reduceMemMb > 0 && h.memAvailMb <= c.reduceMemMb * mem && h.memTotalMb > 0)
        || over(h.psiCpu, c.psiCpuReduce * hold) || over(h.psiMem, c.psiMemReduce * hold) || over(h.psiMemFull, c.psiMemReduce * hold / 3.0) || over(h.psiIo, c.psiIoReduce * hold))
        return 2;
    const bool memShort = h.memAvailMb < 2.0 * c.holdMemMb * mem || (h.psiMem >= 0 && h.psiMem >= c.psiMemHold * hold / 4.0);
    if (h.loadPerCore >= c.holdLoad * hold || (h.memTotalMb > 0 && h.memAvailMb <= c.holdMemMb * mem) || (c.holdSwapMb > 0 && h.swapUsedMb >= c.holdSwapMb * mem && memShort)
        || over(h.psiCpu, c.psiCpuHold * hold) || over(h.psiMem, c.psiMemHold * hold) || over(h.psiIo, c.psiIoHold * hold))
        return 1;
    return 0;
}

_cold void cluster::Orchestrator::pressure_(const cluster::OrchConf& c)
{
    const std::int64_t now = cluster::now_ms();
    const int raw = this->level_(c, 1.0);
    const int calm = this->level_(c, 0.8);
    const int before = this->_level;
    if (raw >= this->_level) {
        this->_level = raw;
        this->_calmSince = 0;
    } else if (calm < this->_level) { // the way down: under the tightened thresholds for resume_after seconds
        if (this->_calmSince == 0) this->_calmSince = now;
        if (now - this->_calmSince >= static_cast<std::int64_t>(c.resumeAfter) * 1000) {
            this->_level = std::max(raw, calm);
            this->_calmSince = 0;
        }
    } else {
        this->_calmSince = 0;
    }
    const cluster::Host& h = this->_host;
    char why[160];
    std::snprintf(why, sizeof(why), "load %.1f/core, %ld MB free, swap %ld MB%s", h.loadPerCore, h.memAvailMb, h.swapUsedMb,
        h.psiMem >= 0 ? (" , memory stall " + std::to_string(static_cast<int>(h.psiMem)) + "%").c_str() : "");
    this->_levelWhy = this->_level == 0 ? "" : why;
    if (before != this->_level) this->touch_();
    if (!c.enabled) return;
    std::vector<cluster::Job*> active;
    std::vector<cluster::Job*> stopped;
    for (cluster::Job& job: this->_jobs) {
        if (job.state == "running") active.push_back(&job);
        else if (job.state == "stopped" && job.stoppedBy == "pressure") stopped.push_back(&job);
    }
    if (this->_level == 2) {
        for (cluster::Job* job: active) // slowed first: the lowest priority class of the system
            if (!job->reniced && job->pid > 1) {
                (void)cluster::grant_group(job->pid, 19);
                job->reniced = true;
            }
        if (c.killOnOom && h.memTotalMb > 0 && h.memAvailMb < c.oomMemMb && !active.empty() && now - this->_lastReduce >= 5000) {
            cluster::Job* newest = *std::max_element(active.begin(), active.end(), [](const cluster::Job* a, const cluster::Job* b) {return a->started < b->started;});
            this->_lastReduce = now;
            this->kill_(*newest, "pressure", "killed to avoid an out-of-memory (" + std::to_string(h.memAvailMb) + " MB free)");
            return;
        }
        if (active.size() > 1 && now - this->_lastReduce >= 2000) { // the lowest score, the newest first, one at a time, never the last active one
            cluster::Job* lowest = nullptr;
            for (cluster::Job* job: active) {
                if (job->pid <= 1 || job->granted == false) continue;
                if (!lowest || this->score_(c, *job, now) < this->score_(c, *lowest, now) || (this->score_(c, *job, now) == this->score_(c, *lowest, now) && job->started > lowest->started)) lowest = job;
            }
            if (lowest) {
                this->signal_(*lowest, SIGSTOP);
                lowest->state = "stopped";
                lowest->stoppedBy = "pressure";
                lowest->stoppedAt = now;
                lowest->reason = "stopped by the pressure of the computer (" + this->_levelWhy + ")";
                this->_lastReduce = now;
                this->touch_();
            }
        }
    } else if (this->_level == 0 && !stopped.empty() && now - this->_lastResume >= 2000) { // one at a time, the best score first
        cluster::Job* best = *std::max_element(stopped.begin(), stopped.end(), [&](const cluster::Job* a, const cluster::Job* b) {return this->score_(c, *a, now) < this->score_(c, *b, now);});
        this->signal_(*best, SIGCONT);
        best->state = "running";
        best->stoppedMs += now - best->stoppedAt;
        best->stoppedAt = 0;
        best->stoppedBy.clear();
        best->reason.clear();
        best->reniced = false;
        this->_lastResume = now;
        this->touch_();
    }
}

/* the table of the processes: the memory of the jobs, a heavy command outside the queue */
_cold void cluster::Orchestrator::processes_(const cluster::OrchConf& c)
{
    this->_lastScan = cluster::now_ms();
    struct Proc {
        int pid = 0;
        int ppid = 0;
        int pgrp = 0;
        std::string comm;
        std::int64_t cpu = 0;   // ticks, its own and its waited-for children's
        std::int64_t rss = 0;   // pages
    };
    std::map<int, Proc> table;
    std::error_code error;
    for (const fs::directory_entry& entry: fs::directory_iterator("/proc", error)) {
        const std::string name = entry.path().filename().string();
        if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos) continue;
        const std::string line = cluster::read_small((entry.path() / "stat").string());
        const std::size_t open = line.find('(');
        const std::size_t close = line.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close + 2 >= line.size()) continue;
        const std::vector<std::string> f = cluster::split(line.substr(close + 2), ' ');
        if (f.size() < 22) continue;
        Proc p;
        p.pid = std::atoi(name.c_str());
        p.comm = line.substr(open + 1, close - open - 1);
        p.ppid = std::atoi(f[1].c_str());
        p.pgrp = std::atoi(f[2].c_str());
        p.cpu = std::atoll(f[11].c_str()) + std::atoll(f[12].c_str()) + std::atoll(f[13].c_str()) + std::atoll(f[14].c_str());
        p.rss = std::atoll(f[21].c_str());
        table[p.pid] = p;
    }
    const long page = ::sysconf(_SC_PAGESIZE) / 1024;
    std::set<int> managed;
    for (cluster::Job& job: this->_jobs) {
        if (job.state != "running" && job.state != "stopped") continue;
        if (job.pid > 1) managed.insert(job.pid);
        std::int64_t rss = 0;
        for (const auto &[pid, p]: table)
            if (job.pid > 1 && p.pgrp == job.pid) rss += p.rss;
        job.rssMb = static_cast<int>(rss * page / 1024);
    }
    // detection: a command of a session, outside the queue, past detect_cpu_seconds of CPU (the shells and the agents themselves left out)
    std::erase_if(this->_detected, [&](const cluster::Json& d) {return !table.contains(d.value("pid", 0));});
    if (c.detectCpuSeconds <= 0 || c.detectAction == "off") return;
    std::map<int, std::string> agents;
    for (const auto &[session, pid]: this->_agents)
        if (pid > 1) agents[pid] = session;
    const long ticks = ::sysconf(_SC_CLK_TCK);
    static const std::set<std::string> shells = {"sh", "bash", "zsh", "dash", "fish", "node", "claude", "tmux", "ssh", "sleep", "cat", "tail", "less", "vim", "nvim", "claude-cluster", "setsid"};
    for (const auto &[pid, p]: table) {
        if (agents.contains(pid) || shells.contains(p.comm) || managed.contains(p.pgrp) || this->_seen.contains(pid) || p.cpu / ticks < c.detectCpuSeconds) continue;
        const auto parent = table.find(p.ppid);
        if (parent == table.end() || (!agents.contains(p.ppid) && !shells.contains(parent->second.comm))) continue;
        std::string session;
        int up = p.ppid;
        for (int depth = 0; depth < 40 && up > 1 && session.empty(); ++depth) {
            if (agents.contains(up)) session = agents[up];
            const auto next = table.find(up);
            up = next == table.end() ? 0 : next->second.ppid;
        }
        if (session.empty()) continue;
        this->_seen.insert(pid);
        if (this->_seen.size() > 2000) this->_seen.clear();
        std::string command = cluster::read_small("/proc/" + std::to_string(pid) + "/cmdline");
        std::replace(command.begin(), command.end(), '\0', ' ');
        command = cluster::one_line(command.empty() ? p.comm : command, 120);
        const int seconds = static_cast<int>(p.cpu / ticks);
        this->_detected.push_back({{"pid", pid}, {"session", session}, {"command", command}, {"cpu_seconds", seconds}, {"class", cluster::job_class(command, {}, c.patterns)}, {"at", cluster::now()}});
        if (this->_detected.size() > 20) this->_detected.erase(this->_detected.begin());
        const std::string text = "[orchestrator] your command `" + command + "` (pid " + std::to_string(pid) + ") burned " + std::to_string(seconds)
            + " s of CPU outside the queue: heavy commands go through `cluster-run -- <command>` (or heavy_run) so the sessions take turns";
        if (c.detectAction == "tell" && this->_hooks.tell) this->post_([this, session, text]() {this->_hooks.tell(session, text);});
        else if (this->_hooks.notice) this->post_([this, session, text]() {this->_hooks.notice(session + ": " + text, false);});
        this->touch_();
    }
}

/* the limits */
_cold int cluster::Orchestrator::slots_(const cluster::OrchConf& c) const
{
    if (c.maxJobs > 0) return c.maxJobs;
    const int cores = std::max(1, this->_hardware.cores > 1 ? this->_hardware.cores : this->_host.cores);
    const long mem = this->_hardware.memMb > 0 ? this->_hardware.memMb : this->_host.memTotalMb;
    const double factor = c.profile == "max" ? 1.5 : c.profile == "normal" ? 1.0 : 0.5;
    const int byCpu = std::max(1, static_cast<int>(std::max(1, cores - c.reservedCpu) * factor));
    const int byRam = mem > 0 ? std::max(1, static_cast<int>(std::max(0L, mem - c.reservedRamMb) / std::max(1, c.classRam[0]))) : byCpu;
    return std::clamp(std::min(byCpu, byRam), 1, 64);
}

_cold int cluster::Orchestrator::classLimit_(const cluster::OrchConf& c, const std::string& cls) const
{
    const int limit = c.classMax[class_index_(cls)];
    return limit > 0 ? limit : this->slots_(c);
}

_cold int cluster::Orchestrator::ram_(const cluster::OrchConf& c, const cluster::Job& job) const // xstyle: ignore CPP-SINGLE-STATEMENT (private, with the other limits)
{
    return job.ramMb > 0 ? job.ramMb : c.classRam[class_index_(job.cls)];
}

_cold int cluster::Orchestrator::grantThreads_(const cluster::OrchConf& c, const std::string& cls, const int active) const
{
    const int cores = std::max(1, this->_hardware.cores > 1 ? this->_hardware.cores : this->_host.cores);
    if (c.threads > 0) return std::min(c.threads, cores);
    if (c.profile == "low") return 1;
    const int share = std::max(1, std::max(1, cores - c.reservedCpu) / std::max(1, active + 1));
    if (c.profile == "normal") return std::clamp(share, 1, cls == "build" ? 4 : 2);
    return std::clamp(share, 1, cores);
}

_cold double cluster::Orchestrator::score_(const cluster::OrchConf& c, const cluster::Job& job, const std::int64_t now) const
{
    // 10 per priority step, the session shown by the user (the global one a half: the user talks to it), the waiting time
    double score = 10.0 * job.prio + 10.0 * pair_(c.sessionPrio, job.session);
    if (!job.session.empty() && job.session == this->_active) score += c.focusBoost;
    else if (job.session == GLOBAL_ID) score += c.focusBoost / 2.0;
    score += static_cast<double>(std::max<std::int64_t>(0, now - job.created)) / 1000.0 / c.agingSeconds;
    return score;
}

_cold std::string cluster::Orchestrator::blocked_(const cluster::OrchConf& c, const cluster::Job& job, const int active) const
{
    const int slots = this->slots_(c);
    if (active >= slots) return "slot (" + std::to_string(active) + "/" + std::to_string(slots) + " running)";
    int mine = 0;
    int sameClass = 0;
    std::int64_t young = 0;
    const std::int64_t now = cluster::now_ms();
    for (const cluster::Job& other: this->_jobs) {
        if (other.state != "running") continue;
        if (!job.session.empty() && other.session == job.session) ++mine;
        if (other.cls == job.cls) ++sameClass;
        if (now - other.started < 20000) young += this->ram_(c, other); // started a moment ago: its memory is not used yet
    }
    int quota = c.maxPerSession;
    if (const int own = pair_(c.sessionQuota, job.session); own > 0) quota = own;
    if (!job.session.empty() && quota > 0 && mine >= quota) return "session limit (" + std::to_string(mine) + " of " + job.session + " running)";
    const int limit = this->classLimit_(c, job.cls);
    if (sameClass >= limit) return "class " + job.cls + " busy (" + std::to_string(sameClass) + "/" + std::to_string(limit) + ")";
    const bool forced = active == 0 && this->_level < 2 && (this->_host.memTotalMb == 0 || this->_host.memAvailMb > c.oomMemMb); // nothing runs: one starts anyway
    if (!forced) {
        const long free = this->_host.memAvailMb - c.reservedRamMb - young;
        if (this->_host.memTotalMb > 0 && this->ram_(c, job) > free) return "RAM (needs " + std::to_string(this->ram_(c, job)) + " MB, " + std::to_string(std::max(0L, free)) + " MB free)";
        if (this->_level > 0) return "pressure (" + this->_levelWhy + ")";
    }
    return "";
}

/* the schedule */
_cold void cluster::Orchestrator::schedule_(void)
{
    const cluster::OrchConf c = this->_config.orch;
    const std::int64_t now = cluster::now_ms();
    std::vector<cluster::Job*> waiting;
    int active = 0;
    for (cluster::Job& job: this->_jobs) {
        if (job.state == "running") ++active;
        else if (job.state == "queued") waiting.push_back(&job);
    }
    std::stable_sort(waiting.begin(), waiting.end(), [&](const cluster::Job* a, const cluster::Job* b) {
        const double sa = this->score_(c, *a, now);
        const double sb = this->score_(c, *b, now);
        return sa != sb ? sa > sb : a->created < b->created;
    });
    int position = 0;
    for (cluster::Job* job: waiting) {
        ++position;
        const std::string why = c.enabled ? this->blocked_(c, *job, active) : std::string();
        if (why.empty()) {
            job->threads = c.enabled ? this->grantThreads_(c, job->cls, active) : 0;
            if (job->origin == "run" && !this->spawn_(*job, c)) continue; // failed to start: its state says why
            job->state = "running";
            job->started = now;
            job->reason.clear();
            job->granted = true;
            ++active;
            this->touch_();
            continue;
        }
        if (job->reason != why || job->lastPosition != position) {
            job->reason = why;
            job->lastPosition = position;
            this->touch_();
        }
        if (why.starts_with("slot") || why.starts_with("pressure")) { // nothing below can start either: they wait for the same
            int rank = position;
            for (auto it = std::find(waiting.begin(), waiting.end(), job); it != waiting.end(); ++it, ++rank) {
                if ((*it)->reason != why || (*it)->lastPosition != rank) this->touch_();
                (*it)->lastPosition = rank;
                (*it)->reason = why;
            }
            break;
        }
        if (why.starts_with("RAM") && static_cast<double>(now - job->created) / 1000.0 > 300) break; // waited long: no smaller job passes it
    }
}

/* a job of the core */
_cold bool cluster::Orchestrator::spawn_(cluster::Job& job, const cluster::OrchConf& c)
{
    int out[2];
    if (::pipe2(out, O_CLOEXEC) != 0) { // xstyle: ignore LU-SYSCALL (posix_spawn: a group of its own, signals reset)
        job.state = "failed";
        job.reason = std::string("no pipe: ") + std::strerror(errno);
        job.ended = cluster::now_ms();
        return false;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, out[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, out[1], STDERR_FILENO);
    const std::string cwd = job.cwd.empty() ? std::string() : cluster::expand_home(job.cwd).string();
    if (!cwd.empty()) posix_spawn_file_actions_addchdir_np(&actions, cwd.c_str());
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    sigset_t none;
    sigset_t defaults;
    sigemptyset(&none);
    sigemptyset(&defaults);
    sigaddset(&defaults, SIGPIPE); // the core ignores it: the command does not
    sigaddset(&defaults, SIGINT);
    sigaddset(&defaults, SIGTERM);
    posix_spawnattr_setsigmask(&attr, &none);
    posix_spawnattr_setsigdefault(&attr, &defaults);
    posix_spawnattr_setpgroup(&attr, 0);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
    // nice / ionice by the programs of the system when there (inherited from the first instruction), else by the core right after
    const cluster::Grant grant{std::max(1, job.threads), c.profile == "low" ? 19 : c.profile == "normal" ? 10 : 5, c.profile == "low" ? 3 : 2, c.profile == "normal" ? 7 : 4};
    std::vector<std::string> argv;
    if (job.threads > 0 && cluster::has_command("nice")) argv = {"nice", "-n", std::to_string(grant.nice)};
    if (job.threads > 0 && cluster::has_command("ionice")) {
        argv.insert(argv.end(), {"ionice", "-c", std::to_string(grant.ioClass)});
        if (grant.ioClass == 2) argv.insert(argv.end(), {"-n", std::to_string(grant.ioLevel)});
    }
    argv.insert(argv.end(), {"sh", "-c", job.command});
    std::vector<char*> args;
    for (std::string& arg: argv)
        args.push_back(arg.data());
    args.push_back(nullptr);
    std::vector<std::string> own;
    for (char** e = environ; e != nullptr && *e != nullptr; ++e)
        own.push_back(*e);
    std::vector<std::string> env;
    if (job.threads > 0) {
        const std::vector<std::pair<std::string, std::string>> given = cluster::grant_env(job.threads, own);
        for (const std::string& line: own) {
            const std::string key = line.substr(0, line.find('='));
            if (std::none_of(given.begin(), given.end(), [&](const std::pair<std::string, std::string>& g) {return g.first == key;})) env.push_back(line);
        }
        for (const auto &[key, value]: given)
            env.push_back(key + "=" + value);
    } else {
        env = own;
    }
    env.push_back("CLUSTER_RUN_JOB=" + job.id);
    std::vector<char*> envp;
    for (std::string& line: env)
        envp.push_back(line.data());
    envp.push_back(nullptr);
    pid_t pid = 0;
    const int code = ::posix_spawnp(&pid, args[0], &actions, &attr, args.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    ::close(out[1]);
    if (code != 0) {
        ::close(out[0]);
        job.state = "failed";
        job.reason = std::string("not started: ") + std::strerror(code);
        job.ended = cluster::now_ms();
        this->finish_(job, "failed", 127, "", job.reason);
        return false;
    }
    ::fcntl(out[0], F_SETFL, ::fcntl(out[0], F_GETFL) | O_NONBLOCK);
    job.pid = pid;
    job.pidStart = start_of_(pid);
    job.outFd = out[0];
    job.child = true;
    if (!(job.threads > 0 && cluster::has_command("nice"))) (void)cluster::grant_group(pid, grant.nice);
    if (job.log.empty()) {
        std::error_code error;
        fs::create_directories(cluster::data_dir() / "jobs", error);
        job.log = (cluster::data_dir() / "jobs" / (job.id + ".log")).string();
    }
    return true;
}

_cold void cluster::Orchestrator::signal_(const cluster::Job& job, const int signal) const // xstyle: ignore CPP-SINGLE-STATEMENT (private, with kill_)
{
    if (job.pid > 1) ::kill(-job.pid, signal);
}

/* the exit and the output of the jobs of the core */
_cold void cluster::Orchestrator::reap_(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    const std::int64_t now = cluster::now_ms();
    for (cluster::Job& job: this->_jobs) {
        if (job.termAt > 0 && now - job.termAt >= 5000) { // asked to end, still there
            if (job.pid > 1) ::kill(-job.pid, SIGKILL);
            job.termAt = 0;
        }
        if (!job.child) continue;
        if (job.outFd >= 0) {
            char buffer[65536];
            for (;;) {
                const ssize_t size = ::read(job.outFd, buffer, sizeof(buffer));
                if (size <= 0) break;
                job.tail = cluster::tail_of(job.tail + std::string(buffer, static_cast<std::size_t>(size)), 8192);
                std::error_code error;
                if (!job.log.empty() && (!fs::exists(job.log, error) || fs::file_size(job.log, error) < 20u * 1024 * 1024)) {
                    std::ofstream log(job.log, std::ios::app | std::ios::binary);
                    log.write(buffer, size);
                }
            }
        }
        int status = 0;
        if (::waitpid(job.pid, &status, WNOHANG) != job.pid) continue;
        if (job.outFd >= 0) { // what is left in the pipe
            char buffer[65536];
            ssize_t size = 0;
            while ((size = ::read(job.outFd, buffer, sizeof(buffer))) > 0)
                job.tail = cluster::tail_of(job.tail + std::string(buffer, static_cast<std::size_t>(size)), 8192);
            ::close(job.outFd);
            job.outFd = -1;
        }
        job.child = false;
        job.termAt = 0;
        const int code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        ::kill(-job.pid, SIGKILL); // what the command left behind (a daemon of its group)
        if (!ended_(job))
            this->finish_(job, code == 0 ? "done" : "failed", code, "", code == 0 ? "" : (WIFSIGNALED(status) ? "ended by the signal " + std::to_string(WTERMSIG(status)) : "exit " + std::to_string(code)));
    }
}

/* the end of a job */
_cold void cluster::Orchestrator::finish_(cluster::Job& job, const std::string& state, const int code, const std::string& by, const std::string& reason)
{
    if (ended_(job)) return;
    const std::int64_t now = cluster::now_ms();
    if (job.state == "stopped" && job.stoppedAt > 0) job.stoppedMs += now - job.stoppedAt;
    job.state = state;
    job.code = code;
    job.by = by;
    if (!reason.empty()) job.reason = reason;
    else if (state == "done") job.reason.clear();
    job.stoppedBy.clear();
    job.stoppedAt = 0;
    job.ended = now;
    job.granted = true;
    const cluster::OrchConf c = this->_config.orch;
    const std::string id = job.id;
    const std::string session = job.session;
    // what the owner reads: the end of its job, explicit
    std::string text = "[job " + id + " " + (state == "done" ? "finished" : state) + (state == "killed" && !by.empty() ? " by " + by : "") + "] "
        + (job.name.empty() ? cluster::one_line(job.command, 100) : job.name);
    if (state == "done") text += ": exit 0 in " + duration_(job.ended - job.started - job.stoppedMs);
    else if (state == "failed") text += ": exit " + std::to_string(code) + (job.reason.empty() ? "" : " (" + job.reason + ")") + " after " + duration_(job.ended - std::max(job.started, job.created));
    else text += (job.reason.empty() ? "" : ": " + job.reason) + " after " + duration_(job.ended - std::max(job.started, job.created));
    const std::string tail = cluster::tail_of(job.tail, 1500);
    if (!tail.empty()) text += "\n" + tail;
    if (job.callback && !session.empty() && this->_hooks.tell && by != "archived") this->post_([this, session, text]() {this->_hooks.tell(session, text);});
    const bool bad = state != "done";
    if (this->_hooks.notify && ((state == "killed" && c.notifyKilled && by != "archived") || (state == "done" && c.notifyDone)))
        this->post_([this, id, state, text, bad]() {this->_hooks.notify("job", "job " + id + " " + state, cluster::one_line(text, 160), bad);});
    this->touch_();
}

_cold void cluster::Orchestrator::kill_(cluster::Job& job, const std::string& by, const std::string& reason)
{
    if (ended_(job)) return;
    if ((job.state == "running" || job.state == "stopped") && job.pid > 1) {
        this->signal_(job, SIGCONT); // a stopped group would not see the TERM
        this->signal_(job, SIGTERM);
        job.termAt = cluster::now_ms();
    }
    this->finish_(job, "killed", 143, by, reason);
}

_cold void cluster::Orchestrator::trim_(const cluster::OrchConf& c)
{
    // the finished jobs kept to the history (the oldest first removed, their log too)
    std::size_t finished = static_cast<std::size_t>(std::count_if(this->_jobs.begin(), this->_jobs.end(), [](const cluster::Job& j) {return ended_(j);}));
    for (auto it = this->_jobs.begin(); it != this->_jobs.end() && finished > static_cast<std::size_t>(c.history);) {
        if (ended_(*it) && !it->child && it->termAt == 0) {
            std::error_code error;
            if (!it->log.empty()) fs::remove(it->log, error);
            it = this->_jobs.erase(it);
            --finished;
        } else {
            ++it;
        }
    }
}

_cold cluster::Job* cluster::Orchestrator::find_(const std::string& id)
{
    for (cluster::Job& job: this->_jobs)
        if (job.id == id) return &job;
    return nullptr;
}
