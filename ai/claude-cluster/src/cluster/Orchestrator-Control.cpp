/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Orchestrator-Control.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Orchestrator.hpp"
#include "cluster/RunProfile.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Tools.hpp"
#include <sys/socket.h>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include <cctype>
#include <poll.h>
#include <chrono>
#include <regex>
#include <cmath>

namespace fs = std::filesystem;

/* tools */
namespace {

constexpr const char* CLASSES[5] = {"build", "tests", "container", "gpu", "custom"};

_cold bool ended_(const cluster::Job& job) // xstyle: ignore CPP-SINGLE-STATEMENT (helper of this file)
{
    return job.state == "done" || job.state == "failed" || job.state == "killed";
}

_cold bool known_class_(const std::string& cls)
{
    return std::any_of(std::begin(CLASSES), std::end(CLASSES), [&](const char* c) {return cls == c;});
}

_cold std::string duration_(const std::int64_t ms)
{
    const std::int64_t s = std::max<std::int64_t>(0, ms) / 1000;
    if (s < 60) return std::to_string(s) + "s";
    if (s < 3600) return std::to_string(s / 60) + "m" + std::to_string(s % 60) + "s";
    return std::to_string(s / 3600) + "h" + std::to_string(s % 3600 / 60) + "m";
}

[[noreturn]] _cold void refuse_(const std::string& why) // xstyle: ignore CPP-SINGLE-STATEMENT (helper of this file)
{
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, why);
}

_cold std::string str_(const cluster::Json& o, const char* key, const std::string& fallback = "")
{
    if (!o.contains(key) || o[key].is_null()) return fallback;
    return o[key].is_string() ? o[key].get<std::string>() : o[key].dump(-1, ' ', false, cluster::JSON_LENIENT);
}

_cold int int_(const cluster::Json& o, const char* key, const int fallback)
{
    if (!o.contains(key) || o[key].is_null()) return fallback;
    if (o[key].is_number()) return o[key].get<int>();
    if (o[key].is_string()) return static_cast<int>(std::strtol(o[key].get<std::string>().c_str(), nullptr, 10));
    return fallback;
}

_cold bool bool_(const cluster::Json& o, const char* key, const bool fallback)
{
    if (!o.contains(key) || o[key].is_null()) return fallback;
    if (o[key].is_boolean()) return o[key].get<bool>();
    if (o[key].is_string()) return o[key].get<std::string>() == "true";
    return fallback;
}

_cold std::string basename_(const std::string& path)
{
    const std::size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace

/* the class of a command */
_cold std::string cluster::job_class(const std::string& command, const std::vector<std::string>& argv, const std::vector<std::string>& patterns)
{
    std::string line = command;
    if (line.empty())
        for (const std::string& word: argv)
            line += (line.empty() ? "" : " ") + word;
    if (argv.size() >= 3 && (basename_(argv[0]) == "sh" || basename_(argv[0]) == "bash" || basename_(argv[0]) == "zsh") && argv[1] == "-c") line = argv[2]; // xstyle: ignore LU-ARGS (the words of a command, not our arguments)
    const std::string lower = cluster::lower(line);
    for (const std::string& pattern: patterns) { // the settings first: "text" or "text = class"
        const std::size_t eq = pattern.rfind('=');
        std::string text = cluster::trim(eq == std::string::npos ? pattern : pattern.substr(0, eq));
        std::string cls = eq == std::string::npos ? "custom" : cluster::trim(pattern.substr(eq + 1));
        if (!known_class_(cls)) {
            text = cluster::trim(pattern);
            cls = "custom";
        }
        if (!text.empty() && lower.find(cluster::lower(text)) != std::string::npos) return cls;
    }
    static const std::set<std::string> prefix = {"sudo", "nice", "ionice", "time", "env", "exec", "command", "nohup", "setsid", "stdbuf", "ccache", "timeout", "chrt", "taskset", "cluster-run", "rtk", "xargs"};
    static const std::set<std::string> build = {"make", "gmake", "ninja", "samu", "clang", "clang++", "gcc", "g++", "cc", "c++", "rustc", "ld", "lld", "tsc", "webpack", "gradle", "gradlew",
        "mvn", "mvnw", "ant", "bazel", "scons", "meson", "xcodebuild", "msbuild", "makepkg", "rpmbuild", "mock", "javac", "kotlinc", "swiftc", "zig", "nvcc", "esbuild", "rollup"};
    static const std::set<std::string> tests = {"ctest", "pytest", "py.test", "tox", "nox", "jest", "vitest", "mocha", "phpunit", "rspec", "unit_tests", "gtest", "karma", "playwright"};
    static const std::set<std::string> containers = {"ffmpeg", "handbrakecli", "x264", "x265", "blender"};
    static const std::set<std::string> engines = {"docker", "podman", "buildah", "nerdctl", "kaniko"};
    static const std::set<std::string> gpu = {"ollama", "llama-cli", "llama-server", "llama-bench", "whisper", "whisper-cli", "sherpa-onnx", "comfyui", "vllm"};
    std::vector<std::string> segments;
    {
        std::string current;
        for (const char c: line) {
            if (c == ';' || c == '&' || c == '|' || c == '\n' || c == '(' || c == ')') {
                if (!current.empty()) segments.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty()) segments.push_back(current);
    }
    for (const std::string& segment: segments) {
        std::vector<std::string> words;
        for (const std::string& word: cluster::split(segment, ' ')) {
            if (word.empty()) continue;
            const std::size_t eq = word.find('=');
            if (words.empty() && eq != std::string::npos && eq > 0 && word.find('/') > eq) continue; // VAR=value before the command
            words.push_back(word);
        }
        std::size_t at = 0;
        while (at < words.size() && (prefix.contains(basename_(words[at])) || words[at].starts_with("-") || (at > 0 && std::isdigit(static_cast<unsigned char>(words[at][0])))))
            ++at;
        if (at >= words.size()) continue;
        const std::string head = basename_(words[at]);
        const std::function<bool(const char*)> has = [&](const char* word) {return std::find(words.begin() + static_cast<long>(at) + 1, words.end(), std::string(word)) != words.end();};
        const std::function<std::string(void)> next = [&]() {return at + 1 < words.size() ? words[at + 1] : std::string();};
        if (build.contains(head)) {
            if ((head == "make" || head == "gmake" || head == "ninja") && (has("test") || has("check"))) return "tests";
            return "build";
        }
        if (head == "cmake" && (has("--build") || has("--install"))) return "build";
        if (head == "cargo" || head == "go" || head == "dotnet") {
            const std::string sub = next();
            if (sub == "test" || sub == "nextest") return "tests";
            if (sub == "build" || sub == "check" || sub == "clippy" || sub == "doc" || sub == "install" || sub == "rustc" || sub == "bench" || sub == "publish") return "build";
        }
        if (head == "npm" || head == "yarn" || head == "pnpm" || head == "bun") {
            if (has("test") || next() == "test") return "tests";
            if (has("build") || next() == "build") return "build";
        }
        if (head.starts_with("python") && (has("pytest") || has("unittest"))) return "tests";
        if (head.starts_with("python") && std::find(words.begin(), words.end(), std::string("torch")) != words.end()) return "gpu";
        if (tests.contains(head) || head.ends_with("_tests") || head.ends_with("_test") || head == "tests.sh" || (head.starts_with("test") && head.ends_with(".sh"))) return "tests";
        if (containers.contains(head)) return "container";
        if (engines.contains(head) && (next() == "build" || next() == "buildx" || has("--build") || (next() == "compose" && has("build")))) return "container";
        if (gpu.contains(head) || head.starts_with("whisper") || head.starts_with("voice-")) return "gpu";
        if (head == "build.sh" || head.starts_with("build_") || head == "build") return "build";
    }
    return "";
}

/* the tools of the MCP: told to the models */
_cold std::vector<cluster::Json> cluster::orchestrator_tools(void)
{
    const std::function<cluster::Json(const std::string&)> str = [](const std::string& d) {return cluster::Json{{"type", "string"}, {"description", d}};};
    const std::function<cluster::Json(const std::string&)> num = [](const std::string& d) {return cluster::Json{{"type", "integer"}, {"description", d}};};
    const std::function<cluster::Json(const std::string&)> flag = [](const std::string& d) {return cluster::Json{{"type", "boolean"}, {"description", d}};};
    const std::function<cluster::Json(const std::string&, const std::string&, const cluster::Json&, const std::vector<std::string>&)> tool = [](const std::string& name,
        const std::string& description, const cluster::Json& properties, const std::vector<std::string>& required) -> cluster::Json {
        return {{"name", name}, {"description", description}, {"inputSchema", {{"type", "object"}, {"properties", properties.is_null() ? cluster::Json::object() : properties}, {"required", required}}}};
    };
    const cluster::Json id = str("job id (j3)");
    const cluster::Json common = {{"command", str("the shell command")}, {"class", str("build | tests | container | gpu | custom (default: guessed from the command)")},
        {"prio", num("-10..10, higher first (default 0)")}, {"name", str("a short name shown in the queue")}, {"cwd", str("folder it runs in (default: the one of claude-cluster)")},
        {"timeout_minutes", num("killed when it runs longer")}, {"ram_mb", num("memory it needs, when not the usual of its class")}};
    cluster::Json run = common;
    run["timeout"] = num("seconds to wait here (default 100, 110 max): then the job is told waiting / running and goes on, follow it with job_status / job_wait");
    cluster::Json add = common;
    add["callback"] = flag("you are woken up with a message [job jN ...] (exit code, duration, tail) when it ends or is killed (default true)");
    add["session"] = str("the session the job belongs to and to wake up (global session only; default: you)");
    return {
        tool("heavy_run", "Run a HEAVY command (a build, the tests, a container / media job, a local model) through the resource orchestrator of this computer: it waits in the queue "
            "for a free slot, then runs in the low profile (nice, ionice, the parallelism lowered to the share granted), and its result comes back HERE: finished (exit code, duration, "
            "tail of its output), failed, killed (by whom), or - when it waits or runs longer than the timeout - waiting (position, ETA, what it waits for) / running: then go on with "
            "something else and ask job_status (cheap) or job_wait, never poll in a loop. Use it instead of running heavy commands yourself, or `cluster-run -- <cmd>` in your own shell. "
            "Light commands (ls, grep, git status...) never go through it.", run, {"command"}),
        tool("jobs_add", "Queue a heavy command WITHOUT waiting for it: you get its id and position at once, and are woken up with a message [job jN finished|failed|killed] "
            "(exit code, duration, tail of the output; killed: by whom) when it ends, so end your turn instead of polling. Same queue and low profile as heavy_run.", add, {"command"}),
        tool("jobs_list", "The jobs of the orchestrator: running, waiting (position, what they wait for), stopped; state all adds the finished ones. A sub-session sees its own jobs "
            "(the user's settings may show it the others).", {{"state", str("active (default: running, waiting, stopped) | all | waiting | running | stopped | finished | failed | killed")},
            {"session", str("only this session (the global session)")}}, {}),
        tool("job_status", "One job, cheap: its state - finished (exit code, duration, tail), killed (by whom: user / global / pressure / archived), failed, waiting (position, ETA, what it "
            "waits for), running, stopped (partial: stopped by the user or the pressure of the computer, resumed later) - and the end of its output.", {{"id", id}}, {"id"}),
        tool("job_wait", "Wait for a job to end (up to timeout seconds, 110 max) and get the same answer as job_status; when it is still waiting / running, wait again or end your turn "
            "(a job added by jobs_add wakes you up).", {{"id", id}, {"timeout", num("seconds (default 100)")}}, {"id"}),
        tool("job_kill", "Kill a job: removed from the queue when it waits, its process group terminated when it runs. The owner is told who did it.", {{"id", id}, {"reason", str("why")}}, {"id"}),
        tool("job_stop", "Stop a running job (SIGSTOP of its process group: it keeps its memory, uses no CPU) until job_resume; its slot is free for the next one.", {{"id", id}}, {"id"}),
        tool("job_resume", "Resume a stopped job (by you, or by the pressure of the computer).", {{"id", id}}, {"id"}),
        tool("job_prio", "Change the priority of a job (-10..10, higher first): it passes the others in the queue.", {{"id", id}, {"prio", num("-10..10")}}, {"id", "prio"}),
        tool("hardware_scan", "Scan this computer again (cores / threads, RAM, swap, disk, GPU, power, temperature), saved with the version of the scan; also tells the live load, memory, "
            "swap and pressure the limits of the orchestrator follow.", {}, {}),
        tool("orchestrator_config", "The settings of the orchestrator ([orchestrator]: profile low | normal | max, max_jobs, max_per_session, reserved CPU / RAM, threads per job, per class limits, "
            "thresholds, session priorities / quotas, what a sub-session may do) with the limits they give now. action get (default) | set (key, value) | enable | disable.",
            {{"action", str("get | set | enable | disable")}, {"key", str("a key of [orchestrator] (get lists them)")}, {"value", str("its new value")}}, {}),
    };
}

_cold std::string cluster::orchestrator_command(const std::string& tool)
{
    static const std::map<std::string, std::string> commands = {{"heavy_run", "heavy_run"}, {"jobs_add", "job_add"}, {"jobs_list", "job_list"}, {"job_status", "job_status"},
        {"job_wait", "job_wait"}, {"job_kill", "job_kill"}, {"job_stop", "job_stop"}, {"job_resume", "job_resume"}, {"job_prio", "job_prio"}, {"hardware_scan", "hardware_scan"},
        {"orchestrator_config", "orch_config"}};
    const auto it = commands.find(tool);
    return it == commands.end() ? "" : it->second;
}

_cold std::filesystem::path cluster::orchestrator_bin_dir(void)
{
    // <runtime>/bin/cluster-run -> the wrapper installed next to claude-cluster: the sessions find it in their PATH
    const fs::path dir = cluster::socket_path().parent_path() / "bin";
    std::error_code error;
    fs::create_directories(dir, error);
    const fs::path wrapper = cluster::self_exe().parent_path() / "cluster-run";
    const fs::path link = dir / "cluster-run";
    if (fs::exists(wrapper, error)) {
        if (!fs::is_symlink(link, error) || fs::read_symlink(link, error) != wrapper) {
            fs::remove(link, error);
            fs::create_symlink(wrapper, link, error);
        }
    }
    return dir;
}

/* who may see / act */
_cold std::vector<std::string> cluster::orchestrator_sub_tools(const cluster::OrchConf& c)
{
    if (!c.enabled) return {};
    std::vector<std::string> tools = {"jobs_list", "job_status", "job_wait"};
    if (c.subAdd) {
        tools.push_back("heavy_run");
        tools.push_back("jobs_add");
    }
    if (c.subKillOwn) tools.push_back("job_kill");
    return tools;
}

_cold bool cluster::Orchestrator::mine_(const cluster::Job& job, const cluster::Orchestrator::Caller& caller) const
{
    if (caller.kind != "sub") return true;
    return job.session == caller.session || this->_config.orch.subSeeOthers;
}

/* what a job tells */
_cold std::map<std::string, int> cluster::Orchestrator::positions_(const cluster::OrchConf& c) const
{
    const std::int64_t now = cluster::now_ms();
    std::vector<const cluster::Job*> waiting;
    for (const cluster::Job& job: this->_jobs)
        if (job.state == "queued") waiting.push_back(&job);
    std::stable_sort(waiting.begin(), waiting.end(), [&](const cluster::Job* a, const cluster::Job* b) {
        const double sa = this->score_(c, *a, now);
        const double sb = this->score_(c, *b, now);
        return sa != sb ? sa > sb : a->created < b->created;
    });
    std::map<std::string, int> out;
    int position = 0;
    for (const cluster::Job* job: waiting)
        out[job->id] = ++position;
    return out;
}

_cold double cluster::Orchestrator::etaOf_(const cluster::OrchConf& c, const cluster::Job& job, const int position) const
{
    // from the average duration of the finished jobs of its class (2 min when none): the ones ahead of it, per slot
    double total = 0;
    int count = 0;
    for (const cluster::Job& other: this->_jobs)
        if (other.cls == job.cls && other.state == "done" && other.started > 0 && other.ended > other.started) {
            total += static_cast<double>(other.ended - other.started - other.stoppedMs) / 1000.0;
            ++count;
        }
    const double average = count > 0 ? total / count : 120.0;
    const int slots = std::max(1, this->slots_(c));
    double soonest = average; // the first slot frees when the oldest running one has run its average
    const std::int64_t now = cluster::now_ms();
    for (const cluster::Job& other: this->_jobs)
        if (other.state == "running") soonest = std::min(soonest, std::max(0.0, average - static_cast<double>(now - other.started) / 1000.0));
    return std::ceil(static_cast<double>(position) / slots - 1.0) * average + soonest;
}

_cold cluster::Json cluster::Orchestrator::jobJson_(const cluster::Job& job, const cluster::OrchConf& c, const bool full) const
{
    const std::int64_t now = cluster::now_ms();
    cluster::Json out = {{"id", job.id}, {"class", job.cls}, {"session", job.session}, {"state", job.state}, {"prio", job.prio}, {"origin", job.origin}};
    if (!job.name.empty()) out["name"] = job.name;
    out["command"] = full ? job.command : cluster::one_line(job.command, 100);
    std::string status;
    std::string message;
    if (job.state == "queued") {
        const std::map<std::string, int> places = this->positions_(c);
        const auto place = places.find(job.id);
        const int position = place == places.end() ? 0 : place->second;
        const int total = static_cast<int>(places.size());
        status = "waiting";
        out["position"] = position;
        out["waiting_for"] = job.reason;
        out["eta_s"] = static_cast<std::int64_t>(this->etaOf_(c, job, std::max(1, position)));
        message = "waiting: position " + std::to_string(position) + " of " + std::to_string(total) + (job.reason.empty() ? "" : ", waiting for " + job.reason)
            + ", ETA about " + duration_(static_cast<std::int64_t>(this->etaOf_(c, job, std::max(1, position)) * 1000)) + ", queued " + duration_(now - job.created) + " ago";
    } else if (job.state == "running") {
        status = "running";
        message = "running for " + duration_(now - job.started - job.stoppedMs) + " with " + std::to_string(job.threads) + " thread(s)";
        if (job.stoppedMs > 0) message += " (partial: it was stopped " + duration_(job.stoppedMs) + ", then resumed)";
    } else if (job.state == "stopped") {
        status = "stopped";
        message = "stopped by " + job.stoppedBy + " for " + duration_(now - job.stoppedAt) + " (partial: it ran " + duration_(job.stoppedAt - job.started - job.stoppedMs) + ")"
            + (job.stoppedBy == "pressure" ? ", it resumes when the computer is calm again" : ", resume it with job_resume");
    } else if (job.state == "done") {
        status = "finished";
        message = "finished: exit 0 in " + duration_(job.ended - job.started - job.stoppedMs);
    } else if (job.state == "failed") {
        status = "failed";
        message = "failed: exit " + std::to_string(job.code) + (job.reason.empty() ? "" : " (" + job.reason + ")") + " after " + duration_(job.ended - std::max(job.started, job.created));
    } else {
        status = "killed";
        message = "killed" + (job.by.empty() ? std::string() : " by " + job.by) + (job.reason.empty() ? "" : ": " + job.reason) + " after " + duration_(job.ended - std::max(job.started, job.created));
    }
    out["status"] = status;
    out["message"] = message;
    if (job.threads > 0) out["threads"] = job.threads;
    if (job.started > 0) out["started"] = job.started / 1000;
    if (job.ended > 0) {
        out["ended"] = job.ended / 1000;
        out["exit"] = job.code;
        out["duration_s"] = std::max<std::int64_t>(0, job.ended - job.started - job.stoppedMs) / 1000;
    }
    if (!job.by.empty()) out["by"] = job.by;
    if (job.state == "stopped") out["stopped_by"] = job.stoppedBy;
    if (job.rssMb > 0) out["rss_mb"] = job.rssMb;
    if (full) {
        out["created"] = job.created / 1000;
        out["cwd"] = job.cwd;
        out["ram_mb"] = this->ram_(c, job);
        out["callback"] = job.callback;
        out["log"] = job.log;
        out["tail"] = cluster::tail_of(job.tail, 2000);
        if (!job.reason.empty() && ended_(job)) out["reason"] = job.reason;
        out["pid"] = job.pid;
    } else if (ended_(job) && !job.tail.empty()) {
        out["tail"] = cluster::tail_of(job.tail, 600);
    }
    return out;
}

/* the commands */
_cold cluster::Json cluster::Orchestrator::add_(const cluster::Json& request, const cluster::Orchestrator::Caller& caller)
{
    cluster::Job job;
    job.command = cluster::trim(str_(request, "command"));
    if (request.contains("argv") && request["argv"].is_array()) {
        job.argv = request["argv"].get<std::vector<std::string>>();
        if (job.command.empty())
            for (const std::string& word: job.argv)
                job.command += (job.command.empty() ? "" : " ") + word;
    }
    if (job.command.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "give the command to run");
    const cluster::OrchConf c = this->_config.orch;
    if (caller.kind == "sub" && !c.subAdd) refuse_("not allowed: a sub-session may not queue heavy jobs ([orchestrator] sub_add in the settings): ask the user");
    job.origin = str_(request, "origin", "run") == "wrap" ? "wrap" : "run";   // only the wrapper connection says "wrap": the "origin" of the callers (mcp / sub / web) is not the one of the job
    job.cls = str_(request, "class");
    if (job.cls.empty()) job.cls = cluster::job_class(job.command, job.argv, c.patterns);
    if (job.cls.empty()) job.cls = "custom";
    if (!known_class_(job.cls)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "class: build | tests | container | gpu | custom");
    job.name = cluster::one_line(str_(request, "name"), 60);
    job.cwd = str_(request, "cwd");
    job.prio = std::clamp(int_(request, "prio", 0), -10, caller.kind == "sub" ? 3 : 10);
    job.session = caller.kind == "sub" ? caller.session : caller.kind == "global" ? (str_(request, "session").empty() ? std::string(GLOBAL_ID) : str_(request, "session")) : str_(request, "session");
    job.callback = bool_(request, "callback", request.value("cmd", std::string()) != "heavy_run" && job.origin == "run") && !job.session.empty();
    job.ramMb = std::max(0, int_(request, "ram_mb", 0));
    job.timeoutSeconds = std::max(0, int_(request, "timeout_minutes", 0) * 60);
    job.created = cluster::now_ms();
    cluster::Json out;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        job.id = "j" + std::to_string(this->_next++);
        this->_jobs.push_back(job);
        this->schedule_();
        this->touch_();
        const cluster::Job& made = this->_jobs.back();
        out = this->jobJson_(made, c, false);
    }
    this->flush_();
    return out;
}

_cold cluster::Json cluster::Orchestrator::waitFor_(const std::string& id, const int seconds, const cluster::Orchestrator::Caller& caller)
{
    const cluster::OrchConf c = this->_config.orch;
    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() + std::chrono::seconds(std::clamp(seconds, 1, 110));
    std::unique_lock<std::mutex> lock(this->_mutex);
    for (;;) {
        const cluster::Job* job = this->find_(id);
        if (job == nullptr || !this->mine_(*job, caller)) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no job '" + id + "'" + (caller.kind == "sub" ? " of yours" : ""));
        if (ended_(*job) || std::chrono::steady_clock::now() >= end) {
            cluster::Json out = this->jobJson_(*job, c, true);
            out["done"] = ended_(*job);
            if (!ended_(*job)) out["note"] = "not finished: ask job_status, wait again, or end your turn (jobs_add wakes you up)";
            return out;
        }
        this->_cv.wait_for(lock, std::chrono::milliseconds(500));
    }
}

_cold cluster::Json cluster::Orchestrator::list_(const cluster::Json& request, const cluster::Orchestrator::Caller& caller) const
{
    const cluster::OrchConf c = this->_config.orch;
    const std::string state = cluster::lower(str_(request, "state", "active"));
    const std::string session = str_(request, "session");
    cluster::Json jobs = cluster::Json::array();
    std::lock_guard<std::mutex> lock(this->_mutex);
    int running = 0;
    int waiting = 0;
    int stopped = 0;
    for (const cluster::Job& job: this->_jobs) {
        if (!this->mine_(job, caller)) continue;
        if (job.state == "running") ++running;
        else if (job.state == "queued") ++waiting;
        else if (job.state == "stopped") ++stopped;
        if (!session.empty() && caller.kind != "sub" && job.session != session) continue;
        const bool active = job.state == "queued" || job.state == "running" || job.state == "stopped";
        const bool match = state == "all" || (state == "active" && active) || state == job.state || (state == "waiting" && job.state == "queued") || (state == "finished" && job.state == "done");
        if (match) jobs.push_back(this->jobJson_(job, c, false));
    }
    if (state == "all" || state == "finished" || state == "failed" || state == "killed") std::reverse(jobs.begin(), jobs.end()); // the latest first
    cluster::Json out = {{"jobs", jobs}, {"running", running}, {"waiting", waiting}, {"stopped", stopped}, {"slots", this->slots_(c)}, {"enabled", c.enabled}};
    if (this->_level > 0) out["pressure"] = this->_levelWhy;
    return out;
}

_cold cluster::Json cluster::Orchestrator::config_(const cluster::Json& request, const cluster::Orchestrator::Caller& caller)
{
    if (caller.kind == "sub") refuse_("not allowed: the settings of the orchestrator are the user's and the global session's");
    std::string action = cluster::lower(str_(request, "action", request.contains("key") ? "set" : "get"));
    std::string key = str_(request, "key");
    std::string value = str_(request, "value");
    if (action == "enable" || action == "disable") {
        key = "enabled";
        value = action == "enable" ? "true" : "false";
        action = "set";
    }
    if (action == "set") {
        if (key.empty() || key == "proc_dir" || key == "sys_dir") refuse_("give a key of [orchestrator] (orchestrator_config get lists them)");
        if (caller.kind == "global" && cluster::setting_protected("orchestrator", key)) refuse_("not allowed: orchestrator." + key + " is changed by the user only, in the settings page of claude-cluster: ask them");
        (void)cluster::settings_set(this->_config, "orchestrator", key, value);
        (void)this->_config.reload();
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->touch_();
        }
        this->flush_();
    }
    cluster::Json settings = cluster::Json::array();
    for (const cluster::SettingsPage& page: cluster::settings_pages(this->_config))
        for (const cluster::Setting& setting: page.settings)
            if (setting.section == "orchestrator") settings.push_back({{"key", setting.key}, {"label", setting.label}, {"value", setting.value(this->_config)}, {"help", setting.description}});
    cluster::Json out = this->state();
    out["settings"] = settings;
    if (action == "set") out["changed"] = {{"key", key}, {"value", value}};
    return out;
}

_cold cluster::Json cluster::Orchestrator::handle(const std::string& cmd, const cluster::Json& request, const cluster::Orchestrator::Caller& caller)
{
    const cluster::OrchConf c = this->_config.orch;
    cluster::Json request2 = request;
    request2["cmd"] = cmd;
    const std::string id = str_(request, "id");
    const std::string by = caller.kind == "sub" ? "session" : caller.kind == "global" ? "global" : "user";
    if (cmd == "job_add" || cmd == "heavy_run") {
        cluster::Json added = this->add_(request2, caller);
        if (cmd == "job_add") {
            added["note"] = bool_(request2, "callback", true) ? "you are woken up with [job " + added.value("id", std::string()) + " ...] when it ends: end your turn, or ask job_status" : "no callback: ask job_status";
            return added;
        }
        return this->waitFor_(added.value("id", std::string()), int_(request, "timeout", 100), caller);
    }
    if (cmd == "job_list") return this->list_(request, caller);
    if (cmd == "job_wait") return this->waitFor_(id, int_(request, "timeout", 100), caller);
    if (cmd == "hardware_scan") {
        if (caller.kind == "sub") refuse_("not allowed: the scan of the computer is for the user and the global session");
        this->rescan();
        return {{"hardware", cluster::hardware_json(this->_hardware)}, {"text", cluster::hardware_text(this->_hardware)}, {"host", cluster::host_json(this->_host)}};
    }
    if (cmd == "orch_config") return this->config_(request, caller);
    if (cmd == "orch_state") {
        if (caller.kind == "sub") refuse_("not allowed: a sub-session asks jobs_list");
        return this->state();
    }
    if (cmd == "job_status" && id.empty() && caller.kind != "sub") return this->list_(request, caller);
    if (cmd != "job_status" && cmd != "job_kill" && cmd != "job_stop" && cmd != "job_resume" && cmd != "job_prio")
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown orchestrator command " + cmd);
    if (caller.kind == "sub" && cmd != "job_status" && cmd != "job_kill") refuse_("not allowed: a sub-session adds, follows, waits and kills its own jobs only");
    if (caller.kind == "sub" && cmd == "job_kill" && !c.subKillOwn) refuse_("not allowed: a sub-session may not kill jobs ([orchestrator] sub_kill_own in the settings)");
    cluster::Json out;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        cluster::Job* job = this->find_(id);
        if (job == nullptr || !this->mine_(*job, caller) || (caller.kind == "sub" && cmd != "job_status" && job->session != caller.session))
            throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no job '" + id + "'" + (caller.kind == "sub" ? " of yours" : ""));
        const std::int64_t now = cluster::now_ms();
        if (cmd == "job_kill") {
            if (ended_(*job)) refuse_("job " + id + " is already " + job->state);
            this->kill_(*job, by, str_(request, "reason"));
            this->schedule_();
        } else if (cmd == "job_stop") {
            if (job->state != "running") refuse_("job " + id + " is " + job->state + ": only a running job is stopped");
            if (job->pid > 1) this->signal_(*job, SIGSTOP);
            else job->stopWanted = true;
            job->state = "stopped";
            job->stoppedBy = by;
            job->stoppedAt = now;
            job->reason = "stopped by " + by;
            this->touch_();
            this->schedule_(); // its slot is free
        } else if (cmd == "job_resume") {
            if (job->state != "stopped") refuse_("job " + id + " is " + job->state + ": only a stopped job is resumed");
            if (job->pid > 1) this->signal_(*job, SIGCONT);
            job->stopWanted = false;
            job->state = "running";
            job->stoppedMs += now - job->stoppedAt;
            job->stoppedAt = 0;
            job->stoppedBy.clear();
            job->reason.clear();
            this->touch_();
        } else if (cmd == "job_prio") {
            job->prio = std::clamp(int_(request, "prio", job->prio), -10, 10);
            this->touch_();
            this->schedule_();
        }
        out = this->jobJson_(*job, c, true);
    }
    this->flush_();
    return out;
}

/* what the interfaces show */
_cold cluster::Json cluster::Orchestrator::state(void) const
{
    const cluster::OrchConf c = this->_config.orch;
    std::lock_guard<std::mutex> lock(this->_mutex);
    cluster::Json jobs = cluster::Json::array();
    cluster::Json sessions = cluster::Json::object();
    cluster::Json classes = cluster::Json::object();
    int running = 0;
    int waiting = 0;
    int stopped = 0;
    int shown = 0;
    for (auto it = this->_jobs.rbegin(); it != this->_jobs.rend(); ++it) { // the latest first; the finished ones: a few
        const cluster::Job& job = *it;
        const bool active = job.state == "queued" || job.state == "running" || job.state == "stopped";
        if (job.state == "running") ++running;
        else if (job.state == "queued") ++waiting;
        else if (job.state == "stopped") ++stopped;
        if (active) {
            // its count read before it is written (a new entry is null: value() on it throws - every sync failed while a job was active)
            cluster::Json& entry = sessions[job.session.empty() ? "user" : job.session];
            if (!entry.is_object()) entry = cluster::Json::object();
            const char* key = job.state == "queued" ? "waiting" : "running";
            const int count = entry.value(key, 0);
            entry[key] = count + 1;
        }
        if (active || ++shown <= 20) jobs.push_back(this->jobJson_(job, c, false));
    }
    std::reverse(jobs.begin(), jobs.end());
    for (int i = 0; i < 5; ++i) {
        int count = 0;
        for (const cluster::Job& job: this->_jobs)
            if (job.state == "running" && job.cls == CLASSES[i]) ++count;
        classes[CLASSES[i]] = {{"running", count}, {"limit", this->classLimit_(c, CLASSES[i])}, {"ram_mb", c.classRam[i]}};
    }
    return {{"enabled", c.enabled}, {"profile", c.profile}, {"slots", this->slots_(c)}, {"running", running}, {"waiting", waiting}, {"stopped", stopped},
        {"level", this->_level}, {"pressure", this->_levelWhy}, {"host", cluster::host_json(this->_host)}, {"hardware", cluster::hardware_json(this->_hardware)},
        {"classes", classes}, {"sessions", sessions}, {"jobs", jobs}, {"detected", this->_detected}, {"version", this->_version.load()},
        {"guard", {{"on", c.memoryGuard}, {"floor_mb", this->guardFloor_(c)}, {"free_mb", this->_host.memAvailMb}, {"total_mb", this->_host.memTotalMb},
            {"min_mb", c.guardMinMb}, {"kills", this->_guardKills}}}};
}

/* a session gone */
_cold void cluster::Orchestrator::sessionGone(const std::string& session, const bool purge)
{
    if (session.empty()) return;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        bool changed = false;
        for (cluster::Job& job: this->_jobs)
            if (job.session == session && !ended_(job)) {
                this->kill_(job, "archived", "its session " + session + " was " + (purge ? "deleted" : "archived"));
                changed = true;
            }
        if (purge) changed |= std::erase_if(this->_jobs, [&](const cluster::Job& job) {return job.session == session && !job.child && job.termAt == 0;}) > 0;
        if (changed) {
            this->schedule_();
            this->touch_();
        }
    }
    this->flush_();
}

/* a cluster-run wrapper */
_cold void cluster::Orchestrator::serve(const int fd, const cluster::Json& request)
{
    const std::function<bool(const cluster::Json&)> send = [fd](const cluster::Json& message) {
        const std::string line = message.dump(-1, ' ', false, cluster::JSON_LENIENT) + "\n";
        std::size_t sent = 0;
        while (sent < line.size()) {
            const ssize_t size = ::send(fd, line.data() + sent, line.size() - sent, MSG_NOSIGNAL);
            if (size <= 0) return false;
            sent += static_cast<std::size_t>(size);
        }
        return true;
    };
    const cluster::OrchConf c = this->_config.orch;
    if (!c.enabled) { // nothing queued: it runs as it is
        (void)send({{"ok", true}, {"event", "go"}, {"unmanaged", true}, {"threads", 0}});
        return;
    }
    cluster::Json made = request;
    made["origin"] = "wrap";
    made["callback"] = false;
    const std::string session = str_(request, "session");
    cluster::Json added;
    try {
        made["cmd"] = "job_add";
        // a wrapper of a session is that session's job, the queue limits of a sub-session do not forbid it (sub_add is for the MCP)
        cluster::Orchestrator::Caller owner;
        owner.kind = "user";
        made["session"] = session;
        added = this->add_(made, owner);
    } catch (const utils::exception::IException& e) {
        (void)send({{"ok", false}, {"error", e.info()}});
        return;
    }
    const std::string id = added.value("id", std::string());
    std::string buffer;
    bool sentGo = false;
    bool sentKill = false;
    std::int64_t killedAt = 0;
    bool lost = false;
    for (;;) {
        // the answer of the job: queued (when it changes), go, killed
        std::string event;
        cluster::Json line;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            const cluster::Job* job = this->find_(id);
            if (job == nullptr) {
                (void)send({{"ok", true}, {"event", "killed"}, {"by", "user"}, {"reason", "the job was removed"}});
                return;
            }
            const std::int64_t now = cluster::now_ms();
            if (job->state == "queued") {
                const std::map<std::string, int> places = this->positions_(c);
                const int position = places.contains(id) ? places.at(id) : 0;
                if (job->lastWaiting != job->reason || job->lastQueued == 0 || (position != 0 && position != job->lastPosition && now - job->lastQueued >= 2000)) {
                    cluster::Job* mutableJob = this->find_(id);
                    mutableJob->lastWaiting = job->reason;
                    mutableJob->lastQueued = now;
                    event = "queued";
                    line = {{"ok", true}, {"event", "queued"}, {"id", id}, {"position", position}, {"total", static_cast<int>(places.size())}, {"waiting_for", job->reason},
                        {"eta_s", static_cast<std::int64_t>(this->etaOf_(c, *job, std::max(1, position)))}};
                }
            } else if ((job->state == "running" || job->state == "stopped") && !sentGo) {
                event = "go";
                line = {{"ok", true}, {"event", "go"}, {"id", id}, {"threads", job->threads}, {"profile", c.profile}, {"nice", c.profile == "low" ? 19 : c.profile == "normal" ? 10 : 5},
                    {"io_class", c.profile == "low" ? 3 : 2}, {"io_level", c.profile == "normal" ? 7 : 4}};
            } else if (ended_(*job) && job->origin == "wrap" && job->state == "killed" && !sentKill) {
                event = "killed";
                line = {{"ok", true}, {"event", "killed"}, {"id", id}, {"by", job->by}, {"reason", job->reason}};
            }
        }
        if (event == "go") sentGo = true;
        if (event == "killed") {
            sentKill = true;
            killedAt = cluster::now_ms();
        }
        if (!event.empty() && !send(line)) lost = true;
        if (sentKill && cluster::now_ms() - killedAt > 15000) break; // the wrapper did not answer: gone
        // what the wrapper says
        pollfd poller{fd, POLLIN, 0};
        const int ready = lost ? 1 : ::poll(&poller, 1, 200);
        if (ready > 0) {
            char chunk[4096];
            const ssize_t size = lost ? 0 : ::recv(fd, chunk, sizeof(chunk), 0);
            if (size <= 0) lost = true;
            else buffer.append(chunk, static_cast<std::size_t>(size));
        }
        std::size_t nl = std::string::npos;
        while ((nl = buffer.find('\n')) != std::string::npos) {
            cluster::Json message;
            try {
                message = cluster::Json::parse(buffer.substr(0, nl));
            } catch (const cluster::Json::exception&) {
                message = cluster::Json::object();
            }
            buffer.erase(0, nl + 1);
            const std::string kind = message.value("event", std::string());
            std::lock_guard<std::mutex> lock(this->_mutex);
            cluster::Job* job = this->find_(id);
            if (job == nullptr) continue;
            if (kind == "started") {
                job->pid = message.value("pid", 0);
                // its start time: the same process when the core looks again
                const std::string text = cluster::read_small("/proc/" + std::to_string(job->pid) + "/stat");
                const std::size_t close = text.rfind(')');
                const std::vector<std::string> fields = close == std::string::npos ? std::vector<std::string>() : cluster::split(text.substr(close + 2), ' ');
                job->pidStart = fields.size() > 19 ? std::strtoull(fields[19].c_str(), nullptr, 10) : 0;
                if (job->state == "killed") this->signal_(*job, SIGTERM);
                else if (job->state == "stopped" || job->stopWanted) this->signal_(*job, SIGSTOP);
                this->touch_();
            } else if (kind == "exit") {
                const int code = message.value("code", 0);
                job->tail = cluster::tail_of(message.value("tail", std::string()), 8192);
                if (!ended_(*job)) this->finish_(*job, code == 0 ? "done" : "failed", code, "", code == 0 ? "" : message.value("signal", 0) > 0 ? "ended by the signal " + std::to_string(message.value("signal", 0)) : "exit " + std::to_string(code));
                else if (job->state != "killed") job->code = code;
                job->pid = 0;
                (void)send({{"ok", true}, {"event", "bye"}});
                this->flush_();
                return;
            }
        }
        if (lost) break;
    }
    // the wrapper is gone (its connection dropped): the slot is released, its command ended
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        cluster::Job* job = this->find_(id);
        if (job != nullptr && !ended_(*job)) {
            this->kill_(*job, "wrapper", job->state == "queued" ? "its cluster-run ended while it waited" : "its cluster-run ended: the command was terminated");
            this->schedule_();
        }
    }
    this->flush_();
}
