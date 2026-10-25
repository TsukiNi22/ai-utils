/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 25/10/2026 by @author Tsukini

File Name:
##  @file Supervisor.cpp

File Description:
##  The supervisor: every claude-cluster of this computer read on its socket, its light actions, its web access, its terminal view
\**************************************************************/

#include "cluster/Supervisor.hpp"
#include "cluster/StartMenu.hpp"
#include "cluster/Settings.hpp"
#include "cluster/External.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Control.hpp"
#include "cluster/Tools.hpp"
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <sys/stat.h>
#include <functional>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <set>
#include <map>

//----------------------------------------------------------------//
/* TOOLS */

namespace {
    // the settings as config.toml has them (a Config made bare holds the defaults only)
    cluster::Config loaded_(void)
    {
        cluster::Config config;
        config.load();
        return config;
    }

    std::string repeat_(const std::string& piece, const int count)
    {
        std::string out;
        for (int i = 0; i < count; ++i)
            out += piece;
        return out;
    }

    std::string fixed_(const double value, const int digits)
    {
        std::ostringstream text;
        text.setf(std::ios::fixed);
        text.precision(digits);
        text << value;
        return text.str();
    }

    std::mutex env_mutex_; // the environment of this process lent to a core started for an instance: one at a time

    // the socket and the data of the main claude-cluster: the ones of this process when it is not an instance; the core of the web access
    // of the supervisor (an instance of its own) is given them at its start
    const std::filesystem::path& main_socket_(void)
    {
        static const std::filesystem::path socket = []() {
            if (const char* given = std::getenv("CLAUDE_CLUSTER_MAIN_SOCKET")) return std::filesystem::path(given);
            if (std::getenv("CLAUDE_CLUSTER_INSTANCE") == nullptr) return cluster::socket_path();
            const std::string own = std::getenv("CLAUDE_CLUSTER_SOCKET") != nullptr ? std::getenv("CLAUDE_CLUSTER_SOCKET") : "";
            ::unsetenv("CLAUDE_CLUSTER_SOCKET");
            const std::filesystem::path main = cluster::socket_path();
            if (!own.empty()) ::setenv("CLAUDE_CLUSTER_SOCKET", own.c_str(), 1);
            return main;
        }();
        return socket;
    }

    const std::filesystem::path& main_data_(void)
    {
        static const std::filesystem::path data = []() {
            if (const char* given = std::getenv("CLAUDE_CLUSTER_MAIN_DATA")) return std::filesystem::path(given);
            return std::getenv("CLAUDE_CLUSTER_INSTANCE") == nullptr ? cluster::data_dir() : cluster::data_dir().parent_path().parent_path(); // <data>/instances/<name>
        }();
        return data;
    }

    // a request to the core on that socket; never throws: {"ok": false} when it does not answer
    cluster::Json ask_(const std::filesystem::path& socket, const cluster::Json& request)
    {
        if (socket.empty()) return {{"ok", false}};
        try {
            return cluster::Control::requestAt(socket, request);
        } catch (const std::exception& e) {
            return {{"ok", false}, {"error", e.what()}};
        }
    }

    // its last use: the conversation of its global session, else its saved state; the sessions it would restore
    std::int64_t last_used_(const std::filesystem::path& data)
    {
        for (const char* name: {"logs/g.jsonl", "state.json"}) {
            struct stat info{};
            if (::stat((data / name).c_str(), &info) == 0) return static_cast<std::int64_t>(info.st_mtim.tv_sec);
        }
        return 0;
    }

    std::size_t kept_(const std::filesystem::path& data)
    {
        const std::optional<cluster::Json> state = cluster::read_json(data / "state.json");
        std::size_t count = 0;
        if (state && state->is_object())
            for (const cluster::Json& spec: state->value("sessions", cluster::Json::array()))
                count += !spec.value("global", false) ? 1 : 0;
        return count;
    }

    void read_(cluster::SupInstance& it)
    {
        it.last = last_used_(it.data);
        const cluster::Json hello = ask_(it.socket, {{"cmd", "hello"}});
        it.alive = hello.value("ok", false);
        if (!it.alive) {
            it.kept = kept_(it.data);
            return;
        }
        const cluster::Json who = hello.value("data", cluster::Json::object());
        it.version = who.value("version", std::string());
        it.build = who.value("build", std::string());
        it.pid = who.value("pid", 0);
        it.offered = who.value("offered", std::string());
        it.upgrading = who.value("upgrading", false);
        for (const cluster::Json& s: ask_(it.socket, {{"cmd", "list"}}).value("data", cluster::Json::array())) {
            const std::string state = s.value("state", std::string());
            const bool background = s.value("background_busy", false);
            const std::size_t asking = s.value("permissions", cluster::Json::array()).size();
            it.working += state == "working" || state == "starting" ? 1 : 0;
            it.waiting += state == "permission" || asking > 0 ? 1 : 0;
            it.errors += state == "error" ? 1 : 0;
            it.background += background ? 1 : 0;
            it.idle += state == "idle" && !background && asking == 0 ? 1 : 0;
            it.queued += s.value("queued", 0);
            it.sessions.push_back({{"id", s.value("id", std::string())}, {"name", s.value("id", std::string()) == GLOBAL_ID ? std::string("global") : s.value("name", std::string())},
                {"state", state}, {"background", background}, {"queued", s.value("queued", 0)}, {"asking", asking},
                {"agents", s.value("agents", cluster::Json::object()).value("running", 0)}, {"model", s.value("model_label", std::string())},
                {"keep_warm", s.value("keep_warm", false)}});
        }
        const cluster::Json web = ask_(it.socket, {{"cmd", "web"}}).value("data", cluster::Json::object());
        it.web = web.value("on", false);
        it.url = web.value("url", std::string());
    }

    cluster::SupInstance find_(const std::string& name)
    {
        for (const cluster::SupInstance& it: cluster::supervisor_scan())
            if (it.name == name) return it;
        throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no claude-cluster named " + name);
    }

    // the environment of an instance for what is started now (its core), this process back on its own after
    void with_env_(const cluster::SupInstance& it, const std::function<void(void)>& run)
    {
        std::lock_guard<std::mutex> lock(env_mutex_);
        std::map<std::string, std::string> saved;
        for (const char* name: {"CLAUDE_CLUSTER_SOCKET", "CLAUDE_CLUSTER_DATA_DIR", "CLAUDE_CLUSTER_INSTANCE"})
            if (const char* value = std::getenv(name)) saved[name] = value;
        if (it.main) {
            ::unsetenv("CLAUDE_CLUSTER_SOCKET");
            ::unsetenv("CLAUDE_CLUSTER_DATA_DIR");
            ::unsetenv("CLAUDE_CLUSTER_INSTANCE");
        } else {
            ::setenv("CLAUDE_CLUSTER_SOCKET", it.socket.c_str(), 1);
            ::setenv("CLAUDE_CLUSTER_DATA_DIR", it.data.c_str(), 1);
            ::setenv("CLAUDE_CLUSTER_INSTANCE", it.name.c_str(), 1);
        }
        run();
        for (const char* name: {"CLAUDE_CLUSTER_SOCKET", "CLAUDE_CLUSTER_DATA_DIR", "CLAUDE_CLUSTER_INSTANCE"}) {
            if (saved.contains(name)) ::setenv(name, saved[name].c_str(), 1);
            else ::unsetenv(name);
        }
    }

    void start_(const cluster::SupInstance& it)
    {
        with_env_(it, []() {
            cluster::detach({cluster::self_exe().string(), "serve", "--restore"});
        });
    }

    std::string update_(const cluster::SupInstance& it)
    {
        // the smooth update of that one (its turns end first, its interfaces follow it); an ignored version offered again first
        if (!it.alive) return it.name + ": not running";
        cluster::Json answer = ask_(it.socket, {{"cmd", "upgrade"}, {"action", "update"}});
        if (!answer.value("ok", false)) {
            (void)ask_(it.socket, {{"cmd", "service_act"}, {"service", "update"}, {"action", "offer"}});
            answer = ask_(it.socket, {{"cmd", "upgrade"}, {"action", "update"}});
        }
        return it.name + ": " + (answer.value("ok", false) ? answer.value("data", std::string("updating")) : answer.value("error", std::string("not updated")));
    }

    std::string force_(const cluster::SupInstance& it)
    {
        // nothing waited for: its turns and its background stopped (continued / asked to run again by the new core, handoff.json of its
        // data folder), the update asked to it; still busy after 15 s: its core stopped by hand and started again on the binary installed
        if (!it.alive) return it.name + ": not running";
        cluster::Json handoff = {{"web", it.web}, {"turns", cluster::Json::array()}, {"queued", cluster::Json::object()}, {"background", cluster::Json::object()},
            {"at", cluster::now()}};
        for (const cluster::Json& s: it.sessions) {
            const std::string id = s.value("id", std::string());
            if (s.value("state", std::string()) == "working" || s.value("state", std::string()) == "permission") handoff["turns"].push_back(id);
            cluster::Json taken = cluster::Json::array();
            for (int n = std::min(50, s.value("queued", 0)); n > 0; --n) {
                const cluster::Json q = ask_(it.socket, {{"cmd", "unqueue"}, {"session", id}});
                if (!q.value("ok", false) || !q["data"].is_object()) break;
                taken.insert(taken.begin(), q["data"]);
            }
            if (!taken.empty()) handoff["queued"][id] = taken;
        }
        for (const cluster::Json& a: ask_(it.socket, {{"cmd", "agents"}}).value("data", cluster::Json::array())) {
            if (a.value("status", std::string()) != "running" || a.value("taskId", std::string()).empty()) continue;
            handoff["background"][a.value("session", std::string())].push_back({{"kind", a.value("kind", std::string("agent"))},
                {"description", a.value("description", std::string())}, {"prompt", a.value("prompt", std::string())}});
            (void)ask_(it.socket, {{"cmd", "agent_stop"}, {"session", a.value("session", std::string())}, {"task", a.value("taskId", std::string())}});
        }
        for (const cluster::Json& id: handoff["turns"])
            (void)ask_(it.socket, {{"cmd", "interrupt"}, {"session", id}});
        try {
            cluster::write_json(it.data / "handoff.json", handoff);
        } catch (const utils::exception::IException&) {}
        const bool asked = ask_(it.socket, {{"cmd", "upgrade"}, {"action", "update"}}).value("ok", false);
        for (int i = 0; asked && i < 150; ++i) {
            const cluster::Json hello = ask_(it.socket, {{"cmd", "hello"}});
            if (hello.value("ok", false) && hello["data"].value("build", std::string()) != it.build) return it.name + ": switched (nothing waited for)";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (it.pid > 0) (void)::kill(it.pid, SIGTERM);
        for (int i = 0; i < 100 && ask_(it.socket, {{"cmd", "hello"}}).value("ok", false); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (it.pid > 0 && ::kill(it.pid, 0) == 0) (void)::kill(it.pid, SIGKILL);
        start_(it);
        return it.name + ": stopped by hand and started again on " + cluster::supervisor_installed() + " (its interfaces follow it)";
    }
}

//----------------------------------------------------------------//
/* FUNCTION */

_cold std::vector<cluster::SupInstance> cluster::supervisor_scan(void)
{
    std::vector<cluster::SupInstance> all;
    cluster::SupInstance main;
    main.name = "main";
    main.main = true;
    main.data = main_data_();
    main.socket = main_socket_();
    read_(main);
    all.push_back(main);
    std::error_code error;
    std::vector<std::string> names;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(main_data_() / "instances", error))
        if (entry.is_directory(error) && !entry.path().filename().string().starts_with(".")) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    for (const std::string& name: names) {
        cluster::SupInstance it;
        it.name = name;
        it.data = main_data_() / "instances" / name;
        it.socket = it.data / "sock";
        read_(it);
        if (it.alive || it.kept > 0) all.push_back(it);
    }
    return all;
}

_cold std::string cluster::supervisor_installed(void)
{
    const std::string version = cluster::binary_version(cluster::self_exe());
    return version.empty() ? std::string(CLUSTER_VERSION) : version;
}

_cold cluster::Json cluster::supervisor_json(const std::vector<cluster::SupInstance>& all)
{
    cluster::Json list = cluster::Json::array();
    for (const cluster::SupInstance& it: all)
        list.push_back({{"name", it.name}, {"main", it.main}, {"alive", it.alive}, {"version", it.version}, {"pid", it.pid}, {"sessions", it.sessions},
            {"working", it.working}, {"idle", it.idle}, {"waiting", it.waiting}, {"background", it.background}, {"errors", it.errors}, {"queued", it.queued},
            {"web", it.web}, {"url", it.url}, {"upgrading", it.upgrading}, {"offered", it.offered}, {"kept", it.kept}, {"last", it.last}});
    cluster::Json outside = cluster::Json::array();
    for (const cluster::SupAgent& a: cluster::supervisor_agents())
        outside.push_back({{"pid", a.pid}, {"agent", a.agent}, {"cwd", a.cwd}, {"title", a.title}, {"id", a.id}, {"state", a.state}, {"tty", a.tty}, {"cpu", a.cpu},
            {"updated", a.updated}});
    return {{"supervisor", CLUSTER_VERSION}, {"installed", cluster::supervisor_installed()}, {"instances", list}, {"web", cluster::supervisor_web()}, {"agents", outside}};
}

_cold std::string cluster::supervisor_act(const std::string& name, const std::string& action)
{
    if (name == "*") { // every running one older than the binary installed
        const std::string installed = cluster::supervisor_installed();
        std::string done;
        for (const cluster::SupInstance& it: cluster::supervisor_scan())
            if (it.alive && cluster::version_compare(it.version, installed) < 0)
                done += (done.empty() ? "" : "\n") + (action == "force" ? force_(it) : update_(it));
        return done.empty() ? "every claude-cluster runs " + installed + " already" : done;
    }
    const cluster::SupInstance it = find_(name);
    if (action == "update") return update_(it);
    if (action == "force") return force_(it);
    if (action == "stop") {
        if (!it.alive) return it.name + ": not running";
        const cluster::Json answer = ask_(it.socket, {{"cmd", "service_act"}, {"service", "core"}, {"action", "stop"}});
        return it.name + ": " + (answer.value("ok", false) ? std::string("stopped (its terminals / windows closed, its sessions kept)") : answer.value("error", std::string("not stopped")));
    }
    if (action == "start") {
        if (it.alive) return it.name + ": runs already";
        start_(it);
        return it.name + ": started (" + std::to_string(it.kept) + " session(s) restored)";
    }
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "supervisor: open | update | force | stop | start, not " + action);
}

_cold void cluster::supervisor_env(const std::string& name)
{
    ::unsetenv("CLAUDE_CLUSTER_SUPERVISOR");
    if (name == "main") return;
    cluster::start_instance(name);
}

_cold bool cluster::supervisor_web(void)
{
    return ask_(main_data_() / "instances" / SUPERVISOR_INSTANCE / "sock", {{"cmd", "hello"}}).value("ok", false);
}

_cold std::string cluster::supervisor_web_toggle(void)
{
    // the web access of the supervisor: a core of its own (no session), its page shows the supervisor; its port [web] supervisor_port
    cluster::SupInstance it;
    it.name = SUPERVISOR_INSTANCE;
    it.data = main_data_() / "instances" / SUPERVISOR_INSTANCE;
    it.socket = it.data / "sock";
    if (cluster::supervisor_web()) {
        (void)ask_(it.socket, {{"cmd", "service_act"}, {"service", "core"}, {"action", "stop"}});
        return "the web access of the supervisor closed";
    }
    std::error_code error;
    std::filesystem::create_directories(it.data, error);
    ::setenv("CLAUDE_CLUSTER_MAIN_SOCKET", main_socket_().c_str(), 1); // what it reads: the main one and its instances
    ::setenv("CLAUDE_CLUSTER_SUPERVISOR_PID", std::to_string(::getpid()).c_str(), 1); // its core stops when this supervisor is gone (a crash too)
    std::ofstream(it.data / "owner") << ::getpid();
    ::setenv("CLAUDE_CLUSTER_MAIN_DATA", main_data_().c_str(), 1);
    with_env_(it, []() {
        ::setenv("CLAUDE_CLUSTER_SUPERVISOR", "1", 1);
        cluster::detach({cluster::self_exe().string(), "serve", "--no-global", "--web"});
        ::unsetenv("CLAUDE_CLUSTER_SUPERVISOR");
    });
    // its address once it listens (10 s at most), with its code; its error when it could not open (the port taken...)
    cluster::Json web = cluster::Json::object();
    for (int i = 0; i < 100 && web.value("url", std::string()).empty() && web.value("error", std::string()).empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        web = ask_(it.socket, {{"cmd", "web"}}).value("data", cluster::Json::object());
    }
    if (!web.value("error", std::string()).empty()) return "the web access of the supervisor did not open: " + web.value("error", std::string());
    if (web.value("url", std::string()).empty()) return "the web access of the supervisor is starting (port " + std::to_string(loaded_().web.supervisorPort) + ")";
    return "the web access of the supervisor: " + web.value("url", std::string()) + (web.value("pin", std::string()).empty() ? std::string() : "  code " + web.value("pin", std::string()));
}

_cold cluster::Json cluster::supervisor_web_state(void)
{
    // the web access of the supervisor as its core tells it (closed: on false)
    const cluster::Json web = ask_(main_data_() / "instances" / SUPERVISOR_INSTANCE / "sock", {{"cmd", "web"}});
    return web.value("ok", false) && web["data"].is_object() ? web["data"] : cluster::Json{{"on", false}};
}

_cold std::string cluster::supervisor_web_device(const std::string& what, const std::string& id, const std::string& answer)
{
    // a device of the supervisor's web access: approve <id> allow | trust | deny, kick, untrust, ban, unban
    cluster::Json request = {{"cmd", "web"}, {"action", what}};
    if (what == "approve") {
        request["id"] = id;
        request["answer"] = answer;
    } else {
        request["ids"] = id;
    }
    const cluster::Json done = ask_(main_data_() / "instances" / SUPERVISOR_INSTANCE / "sock", request);
    if (!done.value("ok", false)) return done.value("error", std::string("the web access of the supervisor is closed"));
    return what == "approve" ? (answer == "deny" ? "denied" : answer == "trust" ? "allowed and trusted" : "allowed") : what + ": done";
}

_cold bool cluster::supervisor_web_close_mine(void)
{
    // the web access this supervisor opened, closed with it (another supervisor's left alone)
    const std::filesystem::path owner = main_data_() / "instances" / SUPERVISOR_INSTANCE / "owner";
    std::ifstream in(owner);
    int pid = 0;
    in >> pid;
    if (pid != ::getpid() || !cluster::supervisor_web()) return false;
    (void)ask_(main_data_() / "instances" / SUPERVISOR_INSTANCE / "sock", {{"cmd", "service_act"}, {"service", "core"}, {"action", "stop"}});
    std::error_code error;
    std::filesystem::remove(owner, error);
    return true;
}

_cold void cluster::supervisor_perf(cluster::SupPerf& meter, const std::vector<cluster::SupInstance>& all)
{
    // what the computer has (read every 30 s: its names, its disk) and what it does now (CPU, memory, GPU, pressure)
    const std::int64_t now = cluster::now();
    if (meter.scanned == 0 || now - meter.scanned >= 30) {
        meter.hardware = cluster::hardware_scan();
        meter.scanned = now;
    }
    const cluster::Hardware& hw = meter.hardware;
    const cluster::Host host = cluster::host_sample();
    meter.cpu = hw.cpu;
    meter.physical = hw.physical;
    meter.threads = hw.cores;
    meter.load1 = host.load1;
    meter.load5 = host.load5;
    meter.load15 = host.load15;
    meter.memTotalMb = host.memTotalMb;
    meter.memAvailMb = host.memAvailMb;
    meter.swapTotalMb = host.swapTotalMb;
    meter.swapUsedMb = host.swapUsedMb;
    meter.psiCpu = host.psiCpu;
    meter.psiMem = host.psiMem;
    meter.psiIo = host.psiIo;
    meter.diskTotalGb = hw.diskTotalGb;
    meter.diskFreeGb = hw.diskFreeGb;
    meter.power = hw.power == "battery" ? "on battery" + (hw.batteryPercent >= 0 ? " " + std::to_string(hw.batteryPercent) + " %" : std::string()) : hw.power == "ac" ? "on mains" : "power unknown";
    // CPU: busy since the last reading (/proc/stat), its mean frequency, its hottest sensor
    {
        std::ifstream stat("/proc/stat");
        std::string cpu;
        unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
        stat >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
        const unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
        const unsigned long long rest = idle + iowait;
        if (meter.lastTotal > 0 && total > meter.lastTotal)
            meter.cpuPercent = 100.0 * (1.0 - static_cast<double>(rest - meter.lastIdle) / static_cast<double>(total - meter.lastTotal));
        meter.lastTotal = total;
        meter.lastIdle = rest;
    }
    {
        std::ifstream info("/proc/cpuinfo");
        double sum = 0;
        int count = 0;
        for (std::string line; std::getline(info, line);)
            if (line.starts_with("cpu MHz")) {
                sum += std::atof(line.substr(line.find(':') + 1).c_str());
                ++count;
            }
        meter.mhz = count > 0 ? static_cast<int>(sum / count) : 0;
    }
    meter.tempC = hw.maxTempC;
    std::error_code error;
    for (const std::filesystem::directory_entry& zone: std::filesystem::directory_iterator("/sys/class/thermal", error)) {
        std::ifstream t(zone.path() / "temp");
        long milli = 0;
        if (t >> milli) meter.tempC = std::max(meter.tempC, static_cast<double>(milli) / 1000.0);
    }
    // GPU: the cards found, their memory and their use now (nvidia-smi, amdgpu in /sys)
    meter.gpus.clear();
    std::vector<std::string> nvidia;
    if (std::any_of(hw.gpus.begin(), hw.gpus.end(), [](const cluster::Gpu& g) {return g.vendor == "nvidia";})) {
        const cluster::Captured got = cluster::capture({"nvidia-smi", "--query-gpu=memory.used,memory.total,utilization.gpu,temperature.gpu", "--format=csv,noheader,nounits"}, "", 3);
        if (got.code == 0) nvidia = cluster::split(cluster::trim(got.out), '\n');
    }
    std::size_t nv = 0;
    for (const cluster::Gpu& g: hw.gpus) {
        cluster::SupGpu one;
        one.name = g.name;
        one.driver = g.driver;
        one.vramTotalMb = g.vramMb;
        if (g.vendor == "nvidia" && nv < nvidia.size()) {
            const std::vector<std::string> v = cluster::split(nvidia[nv++], ',');
            if (v.size() >= 4) {
                one.vramUsedMb = std::atol(cluster::trim(v[0]).c_str());
                one.vramTotalMb = std::atol(cluster::trim(v[1]).c_str());
                one.busy = std::atof(cluster::trim(v[2]).c_str());
                one.tempC = std::atof(cluster::trim(v[3]).c_str());
            }
        }
        meter.gpus.push_back(one);
    }
    for (const std::filesystem::directory_entry& card: std::filesystem::directory_iterator("/sys/class/drm", error)) {
        const std::filesystem::path dev = card.path() / "device";
        if (!std::filesystem::exists(dev / "mem_info_vram_total", error) || card.path().filename().string().find('-') != std::string::npos) continue;
        long total = 0, used = 0, busy = -1;
        std::ifstream(dev / "mem_info_vram_total") >> total;
        std::ifstream(dev / "mem_info_vram_used") >> used;
        std::ifstream(dev / "gpu_busy_percent") >> busy;
        cluster::SupGpu* one = nullptr;
        for (cluster::SupGpu& g: meter.gpus)
            if (g.vramUsedMb < 0 && one == nullptr && (g.name.find("AMD") != std::string::npos || g.name.find("Radeon") != std::string::npos || g.driver == "amdgpu")) one = &g;
        if (one == nullptr) {
            meter.gpus.push_back({});
            one = &meter.gpus.back();
            one->name = "GPU " + card.path().filename().string();
            one->driver = "amdgpu";
        }
        one->vramTotalMb = total / (1024 * 1024);
        one->vramUsedMb = used / (1024 * 1024);
        one->busy = busy >= 0 ? static_cast<double>(busy) : -1;
        for (const std::filesystem::directory_entry& mon: std::filesystem::directory_iterator(dev / "hwmon", error)) {
            long milli = 0;
            if (std::ifstream(mon.path() / "temp1_input") >> milli) one->tempC = static_cast<double>(milli) / 1000.0;
        }
    }
    // up since, the memory of each claude-cluster core
    {
        std::ifstream up("/proc/uptime");
        double seconds = 0;
        up >> seconds;
        const long s = static_cast<long>(seconds);
        meter.uptime = s >= 86400 ? std::to_string(s / 86400) + "d " + std::to_string(s % 86400 / 3600) + "h" : std::to_string(s / 3600) + "h " + std::to_string(s % 3600 / 60) + "m";
    }
    meter.cores.clear();
    for (const cluster::SupInstance& it: all) {
        if (!it.alive || it.pid <= 0) continue;
        std::ifstream status("/proc/" + std::to_string(it.pid) + "/status");
        for (std::string line; std::getline(status, line);)
            if (line.starts_with("VmRSS:")) meter.cores.push_back({"claude-cluster " + it.name + " (its core)", std::atol(line.substr(6).c_str()) / 1024});
    }
    meter.at = now;
}

namespace {
    // the processes of this user: <pid, its parent, its name (argv[0] or the script node runs), its arguments>
    struct Proc {
        int pid = 0;
        int ppid = 0;
        std::string name;
        std::vector<std::string> argv;
        unsigned long long ticks = 0;   // its CPU time (utime + stime)
    };

    // Claude Code waiting for the user: its last entry a tool asked (a permission, a question) not answered yet
    bool waiting_(const std::filesystem::path& transcript)
    {
        std::ifstream in(transcript, std::ios::binary);
        if (!in) return false;
        in.seekg(0, std::ios::end);
        const std::streamoff size = in.tellg();
        in.seekg(std::max<std::streamoff>(0, size - 65536));
        std::string last;
        for (std::string line; std::getline(in, line);)
            if (line.find("\"type\":\"assistant\"") != std::string::npos || line.find("\"type\":\"user\"") != std::string::npos) last = line;
        return last.find("\"type\":\"assistant\"") != std::string::npos && last.find("\"tool_use\"") != std::string::npos;
    }

    // the title of a Claude Code conversation (its summary, else its first prompt), read once per transcript
    std::string title_(const std::filesystem::path& transcript)
    {
        static std::mutex lock;
        static std::map<std::string, std::string> titles;
        std::lock_guard<std::mutex> guard(lock);
        if (titles.contains(transcript.string())) return titles[transcript.string()];
        std::ifstream in(transcript);
        std::string title;
        std::string line;
        for (int n = 0; n < 400 && std::getline(in, line); ++n) {
            if (line.find("\"summary\"") == std::string::npos && line.find("\"type\":\"user\"") == std::string::npos) continue;
            const cluster::Json entry = cluster::Json::parse(line, nullptr, false);
            if (!entry.is_object()) continue;
            if (entry.value("type", std::string()) == "summary") {
                title = entry.value("summary", std::string());
                break;
            }
            if (title.empty() && entry.value("type", std::string()) == "user" && entry.contains("message") && entry["message"].is_object()) {
                const cluster::Json content = entry["message"].value("content", cluster::Json());
                if (content.is_string()) title = content.get<std::string>();
                else if (content.is_array())
                    for (const cluster::Json& part: content)
                        if (part.is_object() && part.value("type", std::string()) == "text" && title.empty()) title = part.value("text", std::string());
                if (title.starts_with("<")) title.clear(); // a command or a hook, not what was asked
            }
        }
        title = cluster::one_line(title, 80);
        titles[transcript.string()] = title;
        return title;
    }

    std::vector<Proc> procs_(void)
    {
        std::vector<Proc> out;
        std::error_code error;
        const uid_t me = ::getuid();
        for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator("/proc", error)) {
            const std::string id = entry.path().filename().string();
            if (id.empty() || !std::all_of(id.begin(), id.end(), [](const char c) {return std::isdigit(static_cast<unsigned char>(c));})) continue;
            struct stat info{};
            if (::stat(entry.path().c_str(), &info) != 0 || info.st_uid != me) continue;
            Proc p;
            p.pid = std::atoi(id.c_str());
            std::ifstream args(entry.path() / "cmdline", std::ios::binary);
            for (std::string arg; std::getline(args, arg, '\0');)
                p.argv.push_back(arg);
            if (p.argv.empty()) continue;
            std::ifstream stat(entry.path() / "stat");
            std::string line;
            std::getline(stat, line);
            const std::size_t close = line.rfind(')');
            if (close == std::string::npos) continue;
            std::istringstream rest(line.substr(close + 2));
            std::string state;
            rest >> state >> p.ppid;
            std::string skip;
            for (int i = 0; i < 9; ++i)
                rest >> skip;
            unsigned long long utime = 0, stime = 0;
            rest >> utime >> stime;
            p.ticks = utime + stime;
            const std::string first = std::filesystem::path(p.argv[0]).filename().string();
            p.name = (first == "node" || first == "bun" || first == "python3" || first == "python") && p.argv.size() > 1 ? std::filesystem::path(p.argv[1]).filename().string() : first;
            out.push_back(p);
        }
        return out;
    }
}

_cold std::vector<cluster::SupAgent> cluster::supervisor_agents(void)
{
    // the agents running on this computer outside claude-cluster (a terminal of their own): Claude Code, Codex, OpenCode, Qwen Code,
    // Gemini, Aider, Ollama; their state from their CPU, their transcript, their children (a shell running: their background)
    static std::mutex lock;
    static std::map<int, std::pair<unsigned long long, std::int64_t>> before; // pid: its CPU ticks, when (ms)
    std::lock_guard<std::mutex> guard(lock);
    static const std::map<std::string, std::string> agents = {{"claude", "claude"}, {"codex", "codex"}, {"opencode", "opencode"}, {"qwen", "qwen"},
        {"gemini", "gemini"}, {"aider", "aider"}, {"ollama", "ollama"}};
    const std::vector<Proc> all = procs_();
    std::map<int, const Proc*> by;
    for (const Proc& p: all)
        by[p.pid] = &p;
    const std::string mine = cluster::self_exe().string();
    const std::function<bool(int)> under_cluster = [&](int pid) { // started by claude-cluster (its sessions): not an outside one
        for (int depth = 0; depth < 12 && pid > 1; ++depth) {
            std::error_code error;
            std::string exe = std::filesystem::read_symlink("/proc/" + std::to_string(pid) + "/exe", error).string();
            if (exe.ends_with(" (deleted)")) exe.resize(exe.size() - 10);
            if (!error && (exe == mine || std::filesystem::path(exe).filename() == "claude-cluster")) return true; // any install of it
            const auto it = by.find(pid);
            if (it == by.end()) return false;
            pid = it->second->ppid;
        }
        return false;
    };
    const std::int64_t nowMs = cluster::now_ms();
    const long hz = ::sysconf(_SC_CLK_TCK);
    static std::map<std::string, std::pair<std::int64_t, std::vector<cluster::Conversation>>> conversations; // agent: <read at, its list>
    std::vector<cluster::SupAgent> out;
    std::map<int, std::pair<unsigned long long, std::int64_t>> seen;
    for (const Proc& p: all) {
        const auto agent = agents.find(p.name);
        if (agent == agents.end()) continue;
        if (by.contains(p.ppid) && agents.contains(by[p.ppid]->name)) continue; // a child of an agent (its own helpers)
        if (agent->second == "ollama" && !(p.argv.size() > 1 && (p.argv[1] == "serve" || p.argv[1] == "run" || p.argv[1] == "runner"))) continue;
        if (under_cluster(p.ppid)) continue;
        cluster::SupAgent a;
        a.pid = p.pid;
        a.agent = agent->second;
        std::error_code error;
        a.cwd = std::filesystem::read_symlink("/proc/" + std::to_string(p.pid) + "/cwd", error).string();
        const std::string tty = std::filesystem::read_symlink("/proc/" + std::to_string(p.pid) + "/fd/0", error).string();
        a.tty = tty.starts_with("/dev/pts/") ? tty.substr(5) : std::string();
        seen[p.pid] = {p.ticks, nowMs};
        if (before.contains(p.pid) && nowMs > before[p.pid].second)
            a.cpu = 100.0 * static_cast<double>(p.ticks - before[p.pid].first) / static_cast<double>(hz) / (static_cast<double>(nowMs - before[p.pid].second) / 1000.0);
        bool shell = false; // a shell or a command of its own running (its background, a tool)
        for (const Proc& c: all)
            shell = shell || (c.ppid == p.pid && (c.name == "bash" || c.name == "sh" || c.name == "zsh" || c.name == "fish")); // not its MCP servers
        // its conversation: the newest one of this agent in its folder
        std::int64_t updated = 0;
        std::filesystem::path transcript;
        if (a.agent == "claude") { // its folder in ~/.claude/projects (the path, every other character a -): its newest transcript, its title read once
            std::string folder;
            for (const char c: a.cwd)
                folder += std::isalnum(static_cast<unsigned char>(c)) ? c : '-';
            std::filesystem::file_time_type newest{};
            for (const std::filesystem::directory_entry& f: std::filesystem::directory_iterator(cluster::expand_home("~/.claude/projects") / folder, error))
                if (f.path().extension() == ".jsonl" && f.last_write_time(error) > newest) {
                    newest = f.last_write_time(error);
                    transcript = f.path();
                }
            if (!transcript.empty()) {
                a.id = transcript.stem().string();
                updated = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::file_clock::to_sys(newest).time_since_epoch()).count();
                a.title = title_(transcript);
            }
        } else if (a.agent != "ollama" && a.agent != "aider") { // the others: their list, read again every 15 s at most
            if (!conversations.contains(a.agent) || cluster::now() - conversations[a.agent].first >= 15)
                conversations[a.agent] = {cluster::now(), cluster::external_conversations(a.agent, 100)};
            for (const cluster::Conversation& c: conversations[a.agent].second)
                if (c.cwd == a.cwd && c.updated > updated) {
                    updated = c.updated;
                    a.id = c.id;
                    a.title = c.title.empty() ? c.first : c.title;
                    transcript = c.path;
                }
        } else if (a.agent == "ollama") {
            a.title = p.argv.size() > 2 ? p.argv[1] + " " + p.argv[2] : p.argv.size() > 1 ? p.argv[1] : std::string("ollama");
        }
        a.updated = updated;
        const bool fresh = updated > 0 && cluster::now() - updated < 4;
        if (a.cpu > 4 || fresh) a.state = "working";
        else if (shell) a.state = "background";
        else if (updated > 0 && cluster::now() - updated < 600 && a.agent == "claude") a.state = waiting_(transcript) ? "question" : "idle";
        else a.state = "idle";
        out.push_back(a);
    }
    before = seen;
    std::sort(out.begin(), out.end(), [](const cluster::SupAgent& x, const cluster::SupAgent& y) {return x.agent != y.agent ? x.agent < y.agent : x.pid < y.pid;});
    return out;
}

_cold std::string cluster::supervisor_agent_open(const cluster::SupAgent& agent)
{
    // its conversation in the main claude-cluster, as a copy (a fork: the terminal it runs in keeps its own)
    if (agent.id.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no conversation of it to open (" + agent.agent + ")");
    const cluster::Json done = ask_(main_socket_(), {{"cmd", "import"}, {"agent", agent.agent}, {"id", agent.id},
        {"name", cluster::one_line(agent.title.empty() ? agent.agent : agent.title, 30)}, {"fork", true}});
    if (!done.value("ok", false))
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, done.value("error", std::string("the main claude-cluster does not run (start it first)")));
    return "opened in the main claude-cluster as a copy (its terminal keeps the original)";
}

_cold int cluster::supervisor_tty(std::string& open)
{
    // every claude-cluster of this computer, read again every 1.5 s; F2: the details (their sessions); Enter: what to do with the one
    // chosen (a popup, the risky ones asked again); F1 the keys, F9 its web access (settings, devices), P the performance of the computer;
    // a device asking to come in: its popup; its own update: run again at once; quit: its web access closed with it
    ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
    screen.ForceHandleCtrlC(false); // Ctrl+C / Ctrl+Shift+C never quit it (q, Esc do)
    screen.ForceHandleCtrlZ(false);
    std::mutex mutex;
    std::vector<cluster::SupInstance> all = cluster::supervisor_scan();
    std::string installed = cluster::supervisor_installed();
    cluster::Json webState = cluster::supervisor_web_state();
    std::vector<cluster::SupAgent> agents = cluster::supervisor_agents(); // the agents running outside claude-cluster
    bool details = false;
    int focus = 0;
    std::string note;
    enum class Pop {None, Menu, Confirm, Help, Web, Perf};
    Pop pop = Pop::None;
    std::vector<std::pair<std::string, std::string>> menu; // <label, action> of the popup of the one chosen
    int menuAt = 0;
    std::string confirmAction;      // asked again: its action, its text
    std::string confirmText;
    std::string confirmName;
    bool confirmYes = false;
    int webTab = 0;                 // F9: 0 settings, 1 devices
    int webAt = 0;
    bool editing = false;
    std::string edit;
    std::set<std::string> asked;    // the devices already shown in an approval popup (answered or put aside)
    cluster::Json approve;          // the device waiting now (its popup)
    int approveAt = 0;
    cluster::SupPerf perf;
    std::atomic<bool> perfOn{false};
    std::atomic<bool> running{true};
    std::atomic<bool> updated{false};
    std::thread reader([&]() {
        cluster::SupPerf meter;
        while (running) {
            for (int i = 0; i < 15 && running; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!running) break;
            std::vector<cluster::SupInstance> fresh = cluster::supervisor_scan();
            const std::string disk = cluster::supervisor_installed();
            const cluster::Json web = cluster::supervisor_web_state();
            std::vector<cluster::SupAgent> outside = cluster::supervisor_agents();
            if (perfOn) cluster::supervisor_perf(meter, fresh);
            {
                std::lock_guard<std::mutex> lock(mutex);
                all = std::move(fresh);
                installed = disk;
                webState = web;
                agents = std::move(outside);
                if (perfOn) perf = meter;
            }
            if (cluster::version_compare(disk, CLUSTER_VERSION) > 0) { // its own update: run again on the new binary, nothing to keep
                updated = true;
                screen.Exit();
                break;
            }
            screen.PostEvent(ftxui::Event::Custom);
        }
    });
    const std::function<void(const std::function<std::string(void)>&)> background = [&](const std::function<std::string(void)>& work) {
        note = "…";
        std::thread([&, work]() {
            std::string done;
            try {
                done = work();
            } catch (const std::exception& e) {
                done = e.what();
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                note = cluster::one_line(done, 200);
            }
            screen.PostEvent(ftxui::Event::Custom);
        }).detach();
    };
    const std::function<void(const std::string&, const std::string&)> act = [&](const std::string& name, const std::string& action) {
        background([name, action]() {
            return action.ends_with("*") ? cluster::supervisor_act("*", action.substr(0, action.size() - 1)) : cluster::supervisor_act(name, action);
        });
    };
    const std::function<const cluster::SupInstance*(void)> chosen = [&]() -> const cluster::SupInstance* {
        if (all.empty() || focus >= static_cast<int>(all.size())) return nullptr;
        return &all[static_cast<std::size_t>(std::max(0, focus))];
    };
    const std::function<const cluster::SupAgent*(void)> chosenAgent = [&]() -> const cluster::SupAgent* { // the focus on an outside agent
        const int at = focus - static_cast<int>(all.size());
        return at >= 0 && at < static_cast<int>(agents.size()) ? &agents[static_cast<std::size_t>(at)] : nullptr;
    };
    const std::function<void(const std::string&, const std::string&, const std::string&)> ask_again = [&](const std::string& name, const std::string& action,
        const std::string& text) {
        confirmName = name;
        confirmAction = action;
        confirmText = text;
        confirmYes = false;
        pop = Pop::Confirm;
    };
    // the colours: the kind (the main one, an independent one), the state (its face like a session's: >_< working, •_• ! waiting for you...)
    const ftxui::Color accent = ftxui::Color::RGB(125, 156, 240);
    const ftxui::Color teal = ftxui::Color::RGB(94, 200, 190);
    const ftxui::Color warn = ftxui::Color::RGB(240, 168, 104);
    const ftxui::Color ok = ftxui::Color::RGB(111, 207, 151);
    const ftxui::Color bad = ftxui::Color::RGB(244, 113, 116);
    const ftxui::Color busy = ftxui::Color::RGB(187, 140, 242);
    const ftxui::Color muted = ftxui::Color::RGB(130, 136, 148);
    const ftxui::Color lit = ftxui::Color::RGB(36, 48, 74);
    const std::function<std::pair<std::string, ftxui::Color>(const cluster::SupInstance&)> face = [&](const cluster::SupInstance& it) {
        if (!it.alive) return std::pair<std::string, ftxui::Color>{"z_z", muted};
        if (it.upgrading) return std::pair<std::string, ftxui::Color>{"^_^ ⟳", accent};
        if (it.waiting > 0) return std::pair<std::string, ftxui::Color>{"•_• !", warn};
        if (it.errors > 0) return std::pair<std::string, ftxui::Color>{"o_o", bad};
        if (it.working > 0) return std::pair<std::string, ftxui::Color>{">_<", ok};
        if (it.background > 0) return std::pair<std::string, ftxui::Color>{">_< ~", busy};
        return std::pair<std::string, ftxui::Color>{"-_-", muted};
    };
    const std::function<ftxui::Element(const std::string&, ftxui::Elements)> popup = [&](const std::string& title, ftxui::Elements body) {
        ftxui::Elements rows = {ftxui::text(" " + title + " ") | ftxui::bold | ftxui::color(accent), ftxui::separator()};
        for (ftxui::Element& e: body)
            rows.push_back(std::move(e));
        return ftxui::vbox(std::move(rows)) | ftxui::borderRounded | ftxui::bgcolor(ftxui::Color::RGB(24, 26, 31)) | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 100)
            | ftxui::clear_under | ftxui::center;
    };
    const std::function<std::vector<cluster::Setting>(void)> web_settings = []() {
        // the settings of the web access, the port of the supervisor in place of the one of the main claude-cluster
        std::vector<cluster::Setting> out;
        const cluster::Config config = loaded_();
        for (const cluster::SettingsPage& page: cluster::settings_pages(config))
            for (const cluster::Setting& s: page.settings)
                if (s.section == "web" && s.kind != cluster::SettingKind::Action && s.key != "port" && s.key != "tty_port" && s.key != "enabled") out.push_back(s);
        std::stable_partition(out.begin(), out.end(), [](const cluster::Setting& s) {return s.key == "supervisor_port";});
        return out;
    };
    std::vector<cluster::Setting> webList = web_settings();
    const std::function<void(const cluster::Setting&, const std::string&)> set_web = [&](const cluster::Setting& s, const std::string& value) {
        const std::string error = cluster::setting_check(s, value);
        if (!error.empty()) {
            note = s.label + ": " + error;
            return;
        }
        cluster::Config config = loaded_();
        (void)cluster::settings_set(config, "web", s.key, value);
        webList = web_settings();
        note = s.label + " = " + value + " (every claude-cluster of this computer; the supervisor's access: closed and opened again to use it)";
    };
    ftxui::Component view = ftxui::Renderer([&]() {
        std::lock_guard<std::mutex> lock(mutex);
        const bool webOn = webState.value("on", false);
        ftxui::Elements rows;
        rows.push_back(ftxui::hbox({ftxui::text(" ^_^ claude-cluster supervisor ") | ftxui::bold | ftxui::color(accent),
            ftxui::text(std::string(CLUSTER_VERSION) + (installed != CLUSTER_VERSION ? " (installed " + installed + ")" : "")) | ftxui::color(muted), ftxui::filler(),
            ftxui::text(webOn ? " web " + webState.value("url", std::string()) + "  code " + webState.value("pin", std::string()) + " " : " web off ")
                | ftxui::color(webOn ? teal : muted),
            ftxui::text("· " + std::string(details ? "details" : "normal") + " (F2) · F1 help ") | ftxui::color(muted)}));
        rows.push_back(ftxui::separator() | ftxui::color(ftxui::Color::RGB(48, 52, 59)));
        for (std::size_t i = 0; i < all.size(); ++i) {
            const cluster::SupInstance& it = all[i];
            const bool selected = static_cast<int>(i) == focus;
            const std::pair<std::string, ftxui::Color> f = face(it);
            const std::string state = !it.alive ? "stopped · " + std::to_string(it.kept) + " session(s) kept" : std::to_string(it.sessions.size()) + " sessions · "
                + std::to_string(it.working) + " working · " + std::to_string(it.idle) + " idle · " + std::to_string(it.waiting) + " waiting for you · "
                + std::to_string(it.background) + " background" + (it.queued > 0 ? " · " + std::to_string(it.queued) + " queued" : "") + (it.errors > 0 ? " · " + std::to_string(it.errors) + " error" : "");
            std::string update;
            if (it.upgrading) update = "  ⟳ updating";
            else if (it.alive && cluster::version_compare(it.version, installed) < 0) update = "  ⬆ " + installed;
            ftxui::Element row = ftxui::hbox({ftxui::text(selected ? " ▌" : "  ") | ftxui::color(accent), ftxui::text(f.first) | ftxui::color(f.second) | ftxui::bold
                | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 7), ftxui::text(it.main ? "◆ " : "◇ ") | ftxui::color(it.main ? accent : teal),
                ftxui::text(it.name) | ftxui::bold | ftxui::color(it.main ? accent : teal) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 18),
                ftxui::text(it.main ? "main" : "independent") | ftxui::color(muted) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 13),
                ftxui::text(it.alive ? it.version : "-") | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 9), ftxui::text(state) | ftxui::color(it.alive ? ftxui::Color::RGB(200, 203, 210) : muted),
                ftxui::text(update) | ftxui::color(accent), ftxui::text(it.web ? "  ◉ web" : "") | ftxui::color(teal), ftxui::filler()});
            rows.push_back(selected ? row | ftxui::bgcolor(lit) : row);
            if (!details || !it.alive) continue;
            rows.push_back(ftxui::text("              pid " + std::to_string(it.pid) + (it.web ? " · " + it.url : "") + (it.offered.empty() ? "" : " · offered " + it.offered))
                | ftxui::color(muted));
            for (const cluster::Json& s: it.sessions) {
                const std::string st = s.value("state", std::string());
                const bool asking = s.value("asking", 0) > 0;
                const bool global = s.value("name", std::string()) == "global";
                const std::string mark = asking ? "•_• !" : st == "working" || st == "starting" ? ">_<" : st == "error" ? "o_o" : s.value("background", false) ? ">_< ~" : "-_-";
                const ftxui::Color tint = asking ? warn : st == "working" || st == "starting" ? ok : st == "error" ? bad : s.value("background", false) ? busy : muted;
                std::string line = (asking ? std::string("waits for you (a question / a permission)") : s.value("background", false) ? std::string("idle, its background runs") : st);
                if (s.value("queued", 0) > 0) line += " · " + std::to_string(s.value("queued", 0)) + " queued";
                if (s.value("agents", 0) > 0) line += " · " + std::to_string(s.value("agents", 0)) + " agent(s)";
                if (s.value("keep_warm", false)) line += " · ♨";
                if (!s.value("model", std::string()).empty()) line += " · " + s.value("model", std::string());
                rows.push_back(ftxui::hbox({ftxui::text("              "), ftxui::text(mark) | ftxui::color(tint) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 7),
                    ftxui::text((global ? "◆ " : "  ") + cluster::one_line(s.value("name", std::string()), 22)) | (global ? ftxui::bold : ftxui::nothing)
                    | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 26), ftxui::text(line) | ftxui::color(tint)}));
            }
        }
        // the agents of this computer outside claude-cluster (their own terminals): a coloured dot for their state
        rows.push_back(ftxui::text(""));
        rows.push_back(ftxui::hbox({ftxui::text(" Other agents on this computer ") | ftxui::bold | ftxui::color(accent),
            ftxui::text(agents.empty() ? "none running" : std::to_string(agents.size()) + " running · Enter: open one in claude-cluster") | ftxui::color(muted)}));
        for (std::size_t i = 0; i < agents.size(); ++i) {
            const cluster::SupAgent& a = agents[i];
            const bool selected = static_cast<int>(all.size() + i) == focus;
            const ftxui::Color dot = a.state == "working" ? ok : a.state == "question" ? warn : a.state == "background" ? busy : muted;
            const std::string state = a.state == "question" ? "waits for you" : a.state == "background" ? "its background runs" : a.state;
            ftxui::Element row = ftxui::hbox({ftxui::text(selected ? " ▌" : "  ") | ftxui::color(accent), ftxui::text("●  ") | ftxui::color(dot),
                ftxui::text(a.agent) | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10), ftxui::text(state) | ftxui::color(dot) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 22),
                ftxui::text(cluster::short_path(a.cwd)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 34), ftxui::text(cluster::one_line(a.title, 50)) | ftxui::color(ftxui::Color::RGB(200, 203, 210)),
                ftxui::filler(), ftxui::text("pid " + std::to_string(a.pid) + (a.tty.empty() ? "" : " · " + a.tty) + " · " + fixed_(a.cpu, 0) + " % ") | ftxui::color(muted)});
            rows.push_back(selected ? row | ftxui::bgcolor(lit) : row);
        }
        rows.push_back(ftxui::filler());
        if (!note.empty()) rows.push_back(ftxui::text(" " + note) | ftxui::color(teal));
        rows.push_back(ftxui::text(" ↑↓ choose · Enter what to do · u / U update (all) · F2 details · F9 web · P performance · F1 help · q quit") | ftxui::color(muted));
        ftxui::Element base = ftxui::vbox(std::move(rows));
        ftxui::Element over = ftxui::emptyElement();
        if (pop == Pop::Menu && chosenAgent() != nullptr) {
            const cluster::SupAgent& a = *chosenAgent();
            ftxui::Elements body = {ftxui::text(" " + a.agent + " · " + cluster::short_path(a.cwd) + " · pid " + std::to_string(a.pid)) | ftxui::color(muted),
                ftxui::text(" " + cluster::one_line(a.title.empty() ? std::string("(no conversation found)") : a.title, 80)), ftxui::text("")};
            for (std::size_t i = 0; i < menu.size(); ++i) {
                const bool on = static_cast<int>(i) == menuAt;
                ftxui::Element line = ftxui::text((on ? " ❯ " : "   ") + menu[i].first + " ");
                body.push_back(on ? line | ftxui::bgcolor(lit) | ftxui::bold : line);
            }
            body.push_back(ftxui::text(""));
            body.push_back(ftxui::text(" ↑↓ choose · Enter do it · Esc cancel") | ftxui::color(muted));
            over = popup(a.agent + " outside claude-cluster", std::move(body));
        } else if (pop == Pop::Menu && chosen() != nullptr) {
            const cluster::SupInstance& it = *chosen();
            ftxui::Elements body = {ftxui::text(" " + std::string(it.main ? "◆ the main claude-cluster" : "◇ an independent instance") + " · " + (it.alive ? it.version + " · "
                + std::to_string(it.sessions.size()) + " session(s)" : std::string("stopped"))) | ftxui::color(muted), ftxui::text("")};
            for (std::size_t i = 0; i < menu.size(); ++i) {
                const bool on = static_cast<int>(i) == menuAt;
                const bool risky = menu[i].second == "stop" || menu[i].second == "force";
                ftxui::Element line = ftxui::text((on ? " ❯ " : "   ") + menu[i].first + " ") | ftxui::color(risky ? warn : ftxui::Color::RGB(220, 222, 228));
                body.push_back(on ? line | ftxui::bgcolor(lit) | ftxui::bold : line);
            }
            body.push_back(ftxui::text(""));
            body.push_back(ftxui::text(" ↑↓ choose · Enter do it · Esc cancel") | ftxui::color(muted));
            over = popup(it.name, std::move(body));
        } else if (pop == Pop::Confirm) {
            ftxui::Elements body = {ftxui::paragraph(" " + confirmText) | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 70), ftxui::text(""),
                ftxui::hbox({ftxui::text("   "), ftxui::text(" Yes ") | (confirmYes ? ftxui::bgcolor(warn) | ftxui::color(ftxui::Color::Black) | ftxui::bold : ftxui::color(warn)),
                    ftxui::text("   "), ftxui::text(" No ") | (!confirmYes ? ftxui::bgcolor(accent) | ftxui::color(ftxui::Color::Black) | ftxui::bold : ftxui::color(accent))}),
                ftxui::text(""), ftxui::text(" ←→ choose · Enter · y / n · Esc no") | ftxui::color(muted)};
            over = popup("Sure?", std::move(body));
        } else if (pop == Pop::Help) {
            ftxui::Elements body;
            for (const auto &[key, what]: std::vector<std::pair<std::string, std::string>>{{"↑ ↓", "choose a claude-cluster"}, {"Enter", "what to do with it: open, update, force, stop, start (asked again)"},
                {"u / U", "its smooth update / every one behind the binary installed"}, {"f / F", "force its switch / every one behind (asked again)"},
                {"s / S", "stop it (asked again) / start a stopped one"}, {"F2", "normal / details (the sessions of each one)"},
                {"w", "the web access of the supervisor on / off"}, {"F9", "the web access: its settings, its devices (allow, kick, untrust, ban)"},
                {"P", "the performance of the computer, live (CPU, memory, GPU, disk)"}, {"↓ below", "the other agents of the computer (Claude Code, Codex, Ollama...): Enter opens one in claude-cluster"}, {"F1", "this help"}, {"q / Esc", "quit (its web access closed with it)"}})
                body.push_back(ftxui::hbox({ftxui::text("  " + key) | ftxui::bold | ftxui::color(accent) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12), ftxui::text(what)}));
            body.push_back(ftxui::text(""));
            body.push_back(ftxui::hbox({ftxui::text("  faces  ") | ftxui::color(muted), ftxui::text(">_< working  ") | ftxui::color(ok), ftxui::text("•_• ! waits for you  ") | ftxui::color(warn),
                ftxui::text(">_< ~ background  ") | ftxui::color(busy), ftxui::text("o_o error  ") | ftxui::color(bad), ftxui::text("-_- idle  z_z stopped") | ftxui::color(muted)}));
            body.push_back(ftxui::hbox({ftxui::text("  kinds  ") | ftxui::color(muted), ftxui::text("◆ the main claude-cluster  ") | ftxui::color(accent),
                ftxui::text("◇ an independent instance  ") | ftxui::color(teal), ftxui::text("◆ global: the global session") | ftxui::bold}));
            body.push_back(ftxui::text(""));
            body.push_back(ftxui::text(" any key closes it") | ftxui::color(muted));
            over = popup("Keys", std::move(body));
        } else if (pop == Pop::Web) {
            ftxui::Elements body = {ftxui::hbox({ftxui::text(webTab == 0 ? " [Settings] " : "  Settings  ") | (webTab == 0 ? ftxui::bold | ftxui::color(accent) : ftxui::color(muted)),
                ftxui::text(webTab == 1 ? " [Devices] " : "  Devices  ") | (webTab == 1 ? ftxui::bold | ftxui::color(accent) : ftxui::color(muted)), ftxui::filler(),
                ftxui::text(webOn ? webState.value("url", std::string()) + "  code " + webState.value("pin", std::string()) : std::string("closed (w opens it)")) | ftxui::color(webOn ? teal : muted)}),
                ftxui::text("")};
            if (webTab == 0) {
                const cluster::Config config = loaded_();
                for (std::size_t i = 0; i < webList.size(); ++i) {
                    const cluster::Setting& s = webList[i];
                    const bool on = static_cast<int>(i) == webAt;
                    std::string value = on && editing ? edit + "▏" : s.value(config);
                    if (!editing || !on) value = s.kind == cluster::SettingKind::Bool ? (value == "true" ? "● on" : "○ off") : s.kind == cluster::SettingKind::Choice ? "‹ " + value + " ›" : value;
                    ftxui::Element line = ftxui::hbox({ftxui::text(on ? " ❯ " : "   "), ftxui::text(s.label) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 34),
                        ftxui::text(value) | ftxui::color(on ? accent : ftxui::Color::RGB(200, 203, 210))});
                    body.push_back(on ? line | ftxui::bgcolor(lit) : line);
                }
                if (webAt >= 0 && webAt < static_cast<int>(webList.size()))
                    body.push_back(ftxui::paragraph(" " + cluster::one_line(webList[static_cast<std::size_t>(webAt)].description, 300)) | ftxui::color(muted)
                        | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 96));
                body.push_back(ftxui::text(" shared by every claude-cluster of this computer (but its port); the access closed and opened again uses them") | ftxui::color(muted));
                body.push_back(ftxui::text(" ↑↓ · Enter edit / switch · ←→ a choice · Tab devices · Esc close") | ftxui::color(muted));
            } else {
                std::vector<cluster::Json> devices;
                for (const char* list: {"devices", "trusted", "banned"})
                    for (const cluster::Json& d: webState.value(list, cluster::Json::array())) {
                        cluster::Json one = d;
                        one["list"] = list;
                        devices.push_back(one);
                    }
                if (devices.empty()) body.push_back(ftxui::text("   no device yet") | ftxui::color(muted));
                for (std::size_t i = 0; i < devices.size(); ++i) {
                    const cluster::Json& d = devices[i];
                    const bool on = static_cast<int>(i) == webAt;
                    const std::string kind = d.value("list", std::string());
                    const std::string state = kind == "banned" ? "banned" : kind == "trusted" ? "trusted" : d.value("state", std::string());
                    ftxui::Element line = ftxui::hbox({ftxui::text(on ? " ❯ " : "   "), ftxui::text(cluster::one_line(d.value("name", std::string("device")), 22))
                        | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 24), ftxui::text(d.value("address", std::string())) | ftxui::color(muted) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 18),
                        ftxui::text(state) | ftxui::color(state == "connected" ? ok : state == "waiting" ? warn : state == "banned" ? bad : muted)});
                    body.push_back(on ? line | ftxui::bgcolor(lit) : line);
                }
                body.push_back(ftxui::text(""));
                body.push_back(ftxui::text(" a allow · t allow and trust · d deny · k kick · x untrust · b ban · B unban · Tab settings · Esc close") | ftxui::color(muted));
            }
            over = popup("Web access of the supervisor", std::move(body));
        } else if (pop == Pop::Perf) {
            ftxui::Elements body;
            const std::function<ftxui::Element(const std::string&, double, const std::string&)> bar = [&](const std::string& label, const double ratio, const std::string& text) {
                const int width = 30;
                const int full = std::clamp(static_cast<int>(ratio * width + 0.5), 0, width);
                const ftxui::Color tint = ratio > 0.9 ? bad : ratio > 0.7 ? warn : ok;
                return ftxui::hbox({ftxui::text("  " + label) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12), ftxui::text(repeat_("█", full)) | ftxui::color(tint),
                    ftxui::text(repeat_("░", width - full)) | ftxui::color(ftxui::Color::RGB(60, 64, 72)), ftxui::text("  " + text)});
            };
            if (perf.at == 0) body.push_back(ftxui::text("  reading…") | ftxui::color(muted));
            else {
                body.push_back(ftxui::text("  " + perf.cpu) | ftxui::bold);
                body.push_back(ftxui::text("  " + std::to_string(perf.physical) + " cores · " + std::to_string(perf.threads) + " threads" + (perf.mhz > 0 ? " · " + std::to_string(perf.mhz) + " MHz" : "")
                    + (perf.tempC > 0 ? " · " + std::to_string(static_cast<int>(perf.tempC)) + " °C" : "")) | ftxui::color(muted));
                body.push_back(bar("CPU", perf.cpuPercent / 100.0, std::to_string(static_cast<int>(perf.cpuPercent)) + " % · load " + fixed_(perf.load1, 2) + " / " + fixed_(perf.load5, 2)
                    + " / " + fixed_(perf.load15, 2)));
                body.push_back(bar("RAM", perf.memTotalMb > 0 ? 1.0 - static_cast<double>(perf.memAvailMb) / static_cast<double>(perf.memTotalMb) : 0,
                    fixed_(static_cast<double>(perf.memTotalMb - perf.memAvailMb) / 1024.0, 1) + " / " + fixed_(static_cast<double>(perf.memTotalMb) / 1024.0, 1) + " GB"));
                if (perf.swapTotalMb > 0) body.push_back(bar("swap", static_cast<double>(perf.swapUsedMb) / static_cast<double>(perf.swapTotalMb),
                    fixed_(static_cast<double>(perf.swapUsedMb) / 1024.0, 1) + " / " + fixed_(static_cast<double>(perf.swapTotalMb) / 1024.0, 1) + " GB"));
                for (const cluster::SupGpu& g: perf.gpus) {
                    body.push_back(ftxui::text("  " + g.name + (g.driver.empty() ? "" : " (" + g.driver + ")")) | ftxui::bold);
                    if (g.vramTotalMb > 0) body.push_back(bar("VRAM", g.vramUsedMb >= 0 ? static_cast<double>(g.vramUsedMb) / static_cast<double>(g.vramTotalMb) : 0,
                        (g.vramUsedMb >= 0 ? fixed_(static_cast<double>(g.vramUsedMb) / 1024.0, 1) : std::string("?")) + " / " + fixed_(static_cast<double>(g.vramTotalMb) / 1024.0, 1) + " GB"));
                    if (g.busy >= 0) body.push_back(bar("GPU", g.busy / 100.0, std::to_string(static_cast<int>(g.busy)) + " %" + (g.tempC > 0 ? " · " + std::to_string(static_cast<int>(g.tempC)) + " °C" : "")));
                }
                if (perf.diskTotalGb > 0) body.push_back(bar("disk", 1.0 - static_cast<double>(perf.diskFreeGb) / static_cast<double>(perf.diskTotalGb),
                    std::to_string(perf.diskTotalGb - perf.diskFreeGb) + " / " + std::to_string(perf.diskTotalGb) + " GB (the data folder)"));
                if (perf.psiCpu >= 0) body.push_back(ftxui::text("  pressure (stalled, 10 s): cpu " + fixed_(perf.psiCpu, 1) + " % · memory " + fixed_(perf.psiMem, 1)
                    + " % · io " + fixed_(perf.psiIo, 1) + " %") | ftxui::color(muted));
                body.push_back(ftxui::text("  " + perf.power + " · up " + perf.uptime) | ftxui::color(muted));
                body.push_back(ftxui::text(""));
                for (const auto &[name, mb]: perf.cores)
                    body.push_back(ftxui::text("  " + name + ": " + fixed_(static_cast<double>(mb), 0) + " MB") | ftxui::color(muted));
            }
            body.push_back(ftxui::text(""));
            body.push_back(ftxui::text(" read again every 1.5 s · any key closes it") | ftxui::color(muted));
            over = popup("Performance of this computer", std::move(body));
        } else if (!approve.is_null() && approve.is_object()) {
            ftxui::Elements body = {ftxui::text("  " + approve.value("name", std::string("a device")) + " · " + approve.value("address", std::string())) | ftxui::bold,
                ftxui::paragraph("  " + cluster::one_line(approve.value("agent", std::string()), 160)) | ftxui::color(muted) | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 80),
                ftxui::text("")};
            const std::vector<std::string> choices = {"Allow", "Allow and trust (no code next time)", "Deny"};
            for (std::size_t i = 0; i < choices.size(); ++i) {
                const bool on = static_cast<int>(i) == approveAt;
                ftxui::Element line = ftxui::text((on ? " ❯ " : "   ") + choices[i] + " ") | ftxui::color(i == 2 ? bad : ftxui::Color::RGB(220, 222, 228));
                body.push_back(on ? line | ftxui::bgcolor(lit) | ftxui::bold : line);
            }
            body.push_back(ftxui::text(""));
            body.push_back(ftxui::text(" ↑↓ choose · Enter · Esc later (F9 > Devices)") | ftxui::color(muted));
            over = popup("A device asks to use the supervisor", std::move(body));
        }
        return ftxui::dbox({base, over});
    });
    ftxui::Component keys = ftxui::CatchEvent(view, [&](const ftxui::Event& event) {
        std::unique_lock<std::mutex> lock(mutex);
        if (event == ftxui::Event::Custom) {
            // a device waiting for its approval, not shown yet: its popup (when nothing else is open)
            if (approve.is_null() && pop == Pop::None)
                for (const cluster::Json& d: webState.value("devices", cluster::Json::array()))
                    if (d.value("state", std::string()) == "waiting" && !asked.contains(d.value("id", std::string()))) {
                        approve = d;
                        approveAt = 0;
                        break;
                    }
            return false;
        }
        if (event.is_mouse()) return false;
        if (event == ftxui::Event::Special("\x03") || event == ftxui::Event::Special("\x1a")) return true; // Ctrl+C / Ctrl+Z: nothing
        const int count = static_cast<int>(all.size() + agents.size());
        if (pop == Pop::Help || pop == Pop::Perf) {
            pop = Pop::None;
            perfOn = false;
            return true;
        }
        if (pop == Pop::Confirm) {
            if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight || event == ftxui::Event::Tab) confirmYes = !confirmYes;
            else if (event == ftxui::Event::Character('y') || event == ftxui::Event::Character('Y') || (event == ftxui::Event::Return && confirmYes)) {
                pop = Pop::None;
                lock.unlock();
                act(confirmName, confirmAction);
            } else if (event == ftxui::Event::Character('n') || event == ftxui::Event::Escape || event == ftxui::Event::Return) {
                pop = Pop::None;
                note = "nothing done";
            }
            return true;
        }
        if (pop == Pop::Menu) {
            const int n = static_cast<int>(menu.size());
            if (event == ftxui::Event::ArrowDown) menuAt = (menuAt + 1) % std::max(1, n);
            else if (event == ftxui::Event::ArrowUp) menuAt = (menuAt + n - 1) % std::max(1, n);
            else if (event == ftxui::Event::Escape) pop = Pop::None;
            else if (event == ftxui::Event::Return && n > 0 && chosenAgent() != nullptr) {
                const std::string action = menu[static_cast<std::size_t>(menuAt)].second;
                const cluster::SupAgent agent = *chosenAgent();
                pop = Pop::None;
                if (action == "agent_open") {
                    lock.unlock();
                    background([agent]() {return cluster::supervisor_agent_open(agent);});
                }
            } else if (event == ftxui::Event::Return && n > 0 && chosen() != nullptr) {
                const std::string action = menu[static_cast<std::size_t>(menuAt)].second;
                const std::string name = chosen()->name;
                pop = Pop::None;
                if (action == "cancel") return true;
                if (action == "open") {
                    open = name;
                    screen.Exit();
                } else if (action == "stop") {
                    ask_again(name, "stop", "Stop " + name + "? Its terminals and windows close, its sessions are kept (Start brings them back).");
                } else if (action == "force") {
                    ask_again(name, "force", "Force the switch of " + name + "? Its turns and its background are stopped now, then continued by the new version.");
                } else {
                    lock.unlock();
                    act(name, action);
                }
            }
            return true;
        }
        if (pop == Pop::Web) {
            if (editing) {
                if (event == ftxui::Event::Escape) editing = false;
                else if (event == ftxui::Event::Return) {
                    editing = false;
                    set_web(webList[static_cast<std::size_t>(webAt)], edit);
                } else if (event == ftxui::Event::Backspace) {
                    if (!edit.empty()) edit.pop_back();
                } else if (event.is_character()) {
                    edit += event.character();
                }
                return true;
            }
            if (event == ftxui::Event::Escape) pop = Pop::None;
            else if (event == ftxui::Event::Tab || event == ftxui::Event::TabReverse) {
                webTab = 1 - webTab;
                webAt = 0;
            } else if (event == ftxui::Event::ArrowDown) {
                ++webAt;
            } else if (event == ftxui::Event::ArrowUp) {
                webAt = std::max(0, webAt - 1);
            } else if (webTab == 0 && webAt < static_cast<int>(webList.size())) {
                const cluster::Setting s = webList[static_cast<std::size_t>(webAt)];
                const std::string now = s.value(loaded_());
                if (s.kind == cluster::SettingKind::Bool && (event == ftxui::Event::Return || event == ftxui::Event::Character(' '))) {
                    set_web(s, now == "true" ? "false" : "true");
                } else if (s.kind == cluster::SettingKind::Choice && !s.choices.empty() && (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight
                    || event == ftxui::Event::Return)) {
                    const auto at = std::find(s.choices.begin(), s.choices.end(), now);
                    const int i = at == s.choices.end() ? 0 : static_cast<int>(at - s.choices.begin());
                    const int n = static_cast<int>(s.choices.size());
                    set_web(s, s.choices[static_cast<std::size_t>((i + (event == ftxui::Event::ArrowLeft ? n - 1 : 1)) % n)]);
                } else if (event == ftxui::Event::Return) {
                    editing = true;
                    edit = now;
                }
            } else if (webTab == 1 && event.is_character()) {
                std::vector<cluster::Json> devices;
                for (const char* list: {"devices", "trusted", "banned"})
                    for (const cluster::Json& d: webState.value(list, cluster::Json::array()))
                        devices.push_back(d);
                if (webAt < static_cast<int>(devices.size())) {
                    const std::string id = devices[static_cast<std::size_t>(webAt)].value("id", std::string());
                    const std::string c = event.character();
                    const std::map<std::string, std::pair<std::string, std::string>> actions = {{"a", {"approve", "allow"}}, {"t", {"approve", "trust"}}, {"d", {"approve", "deny"}},
                        {"k", {"kick", ""}}, {"x", {"untrust", ""}}, {"b", {"ban", ""}}, {"B", {"unban", ""}}};
                    if (actions.contains(c)) {
                        const auto &[what, answer] = actions.at(c);
                        lock.unlock();
                        background([id, what, answer]() {return cluster::supervisor_web_device(what, id, answer);});
                    }
                }
            }
            if (webTab == 0) webAt = std::clamp(webAt, 0, std::max(0, static_cast<int>(webList.size()) - 1));
            return true;
        }
        if (approve.is_object()) { // the popup of a device waiting
            if (event == ftxui::Event::ArrowDown) approveAt = (approveAt + 1) % 3;
            else if (event == ftxui::Event::ArrowUp) approveAt = (approveAt + 2) % 3;
            else if (event == ftxui::Event::Escape || event == ftxui::Event::Return) {
                const std::string id = approve.value("id", std::string());
                asked.insert(id);
                const int at = approveAt;
                const bool answered = event == ftxui::Event::Return;
                approve = cluster::Json();
                if (answered) {
                    lock.unlock();
                    background([id, at]() {return cluster::supervisor_web_device("approve", id, at == 0 ? "allow" : at == 1 ? "trust" : "deny");});
                }
            }
            return true;
        }
        if (event == ftxui::Event::ArrowDown) focus = count > 0 ? (focus + 1) % count : 0;
        else if (event == ftxui::Event::ArrowUp) focus = count > 0 ? (focus + count - 1) % count : 0;
        else if (event == ftxui::Event::F2) details = !details;
        else if (event == ftxui::Event::F1 || event == ftxui::Event::Character('?')) pop = Pop::Help;
        else if (event == ftxui::Event::F9) {
            webList = web_settings();
            webTab = 0;
            webAt = 0;
            pop = Pop::Web;
        } else if (event == ftxui::Event::Character('p') || event == ftxui::Event::Character('P')) {
            perfOn = true;
            pop = Pop::Perf;
            lock.unlock();
            std::thread([&]() { // the first reading at once (the next with the list)
                cluster::SupPerf meter;
                std::vector<cluster::SupInstance> now;
                {
                    std::lock_guard<std::mutex> guard(mutex);
                    now = all;
                }
                cluster::supervisor_perf(meter, now);
                {
                    std::lock_guard<std::mutex> guard(mutex);
                    if (perf.at == 0) perf = meter;
                }
                screen.PostEvent(ftxui::Event::Custom);
            }).detach();
        } else if (event == ftxui::Event::Return && chosenAgent() != nullptr) {
            menu.clear();
            if (!chosenAgent()->id.empty()) menu.push_back({"Open in claude-cluster (a copy: its terminal keeps the original)", "agent_open"});
            menu.push_back({"Cancel", "cancel"});
            menuAt = 0;
            pop = Pop::Menu;
        } else if (event == ftxui::Event::Return && chosen() != nullptr) {
            const cluster::SupInstance& it = *chosen();
            menu.clear();
            if (it.alive) {
                menu.push_back({"Open it (the supervisor closes)", "open"});
                if (cluster::version_compare(it.version, installed) < 0) menu.push_back({"Update it to " + installed + " (smooth)", "update"});
                if (cluster::version_compare(it.version, installed) < 0) menu.push_back({"Force its switch to " + installed, "force"});
                menu.push_back({"Stop it", "stop"});
            } else {
                menu.push_back({"Start it (its sessions restored)", "start"});
                menu.push_back({"Start and open it", "open"});
            }
            menu.push_back({"Cancel", "cancel"});
            menuAt = 0;
            pop = Pop::Menu;
        } else if (event == ftxui::Event::Character('u') && chosen() != nullptr) {
            const std::string name = chosen()->name;
            lock.unlock();
            act(name, "update");
        } else if (event == ftxui::Event::Character('U')) {
            lock.unlock();
            act("", "update*");
        } else if (event == ftxui::Event::Character('f') && chosen() != nullptr) {
            ask_again(chosen()->name, "force", "Force the switch of " + chosen()->name + "? Its turns and its background are stopped now, then continued by the new version.");
        } else if (event == ftxui::Event::Character('F')) {
            ask_again("", "force*", "Force the switch of every claude-cluster behind the binary installed? Their turns and background are stopped now, then continued.");
        } else if (event == ftxui::Event::Character('s') && chosen() != nullptr) {
            ask_again(chosen()->name, "stop", "Stop " + chosen()->name + "? Its terminals and windows close, its sessions are kept (Start brings them back).");
        } else if (event == ftxui::Event::Character('S') && chosen() != nullptr) {
            const std::string name = chosen()->name;
            lock.unlock();
            act(name, "start");
        } else if (event == ftxui::Event::Character('w')) {
            lock.unlock();
            background([]() {return cluster::supervisor_web_toggle();});
        } else if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape) {
            screen.Exit();
        } else {
            return false;
        }
        return true;
    });
    screen.Loop(keys);
    running = false;
    if (reader.joinable()) reader.join();
    (void)cluster::supervisor_web_close_mine(); // quit: its web access never left open
    if (updated) { // the binary installed is newer: this supervisor runs it at once (the same choice: the supervisor again)
        ::setenv("CLAUDE_CLUSTER_SUPERVISOR", "1", 1);
        cluster::upgrade_request(false);
    }
    return 0;
}
