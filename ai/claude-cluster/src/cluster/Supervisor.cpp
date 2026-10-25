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
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <map>

//----------------------------------------------------------------//
/* TOOLS */

namespace {
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
    return {{"supervisor", CLUSTER_VERSION}, {"installed", cluster::supervisor_installed()}, {"instances", list}, {"web", cluster::supervisor_web()}};
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
    if (web.value("url", std::string()).empty()) return "the web access of the supervisor is starting (port " + std::to_string(cluster::Config().web.supervisorPort) + ")";
    return "the web access of the supervisor: " + web.value("url", std::string()) + (web.value("pin", std::string()).empty() ? std::string() : "  code " + web.value("pin", std::string()));
}

_cold int cluster::supervisor_tty(std::string& open)
{
    // every claude-cluster of this computer, read again every 1.5 s; F2: the details (their sessions); its own update: run again at once
    ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
    std::mutex mutex;
    std::vector<cluster::SupInstance> all = cluster::supervisor_scan();
    std::string installed = cluster::supervisor_installed();
    bool web = cluster::supervisor_web();
    bool details = false;
    int focus = 0;
    std::string note;
    std::string confirm;                // an action asked again before it runs: "stop", "force", "force*"
    std::atomic<bool> running{true};
    std::atomic<bool> updated{false};
    std::thread reader([&]() {
        while (running) {
            for (int i = 0; i < 15 && running; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!running) break;
            std::vector<cluster::SupInstance> fresh = cluster::supervisor_scan();
            const std::string disk = cluster::supervisor_installed();
            const bool on = cluster::supervisor_web();
            {
                std::lock_guard<std::mutex> lock(mutex);
                all = std::move(fresh);
                installed = disk;
                web = on;
            }
            if (cluster::version_compare(disk, CLUSTER_VERSION) > 0) { // its own update: run again on the new binary, nothing to keep
                updated = true;
                screen.Exit();
                break;
            }
            screen.PostEvent(ftxui::Event::Custom);
        }
    });
    const std::function<void(const std::string&)> act = [&](const std::string& action) {
        std::string name;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (all.empty()) return;
            name = all[static_cast<std::size_t>(std::clamp(focus, 0, static_cast<int>(all.size()) - 1))].name;
        }
        note = "…";
        std::thread([&, name, action]() {
            std::string done;
            try {
                done = action.ends_with("*") ? cluster::supervisor_act("*", action.substr(0, action.size() - 1)) : cluster::supervisor_act(name, action);
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
    const std::function<ftxui::Color(const cluster::SupInstance&)> tint = [](const cluster::SupInstance& it) {
        return !it.alive ? ftxui::Color::GrayDark : it.waiting > 0 ? ftxui::Color::Yellow : it.errors > 0 ? ftxui::Color::Red : it.working > 0 ? ftxui::Color::Green
            : it.background > 0 ? ftxui::Color::Magenta : ftxui::Color::GrayLight;
    };
    ftxui::Component view = ftxui::Renderer([&]() {
        std::lock_guard<std::mutex> lock(mutex);
        ftxui::Elements rows;
        rows.push_back(ftxui::hbox({ftxui::text(" claude-cluster supervisor ") | ftxui::bold, ftxui::text(std::string(CLUSTER_VERSION) + (installed != CLUSTER_VERSION ? " (installed " + installed + ")" : "")) | ftxui::dim,
            ftxui::filler(), ftxui::text(std::string(details ? "details" : "normal") + " (F2) · web " + (web ? "on" : "off") + " (w) ") | ftxui::dim}));
        rows.push_back(ftxui::separator());
        for (std::size_t i = 0; i < all.size(); ++i) {
            const cluster::SupInstance& it = all[i];
            const bool selected = static_cast<int>(i) == focus;
            std::string state = !it.alive ? "stopped · " + std::to_string(it.kept) + " session(s) kept" : std::to_string(it.sessions.size()) + " sessions: "
                + std::to_string(it.working) + " working · " + std::to_string(it.idle) + " idle · " + std::to_string(it.waiting) + " waiting for you · "
                + std::to_string(it.background) + " background" + (it.queued > 0 ? " · " + std::to_string(it.queued) + " queued" : "") + (it.errors > 0 ? " · " + std::to_string(it.errors) + " error" : "");
            std::string update;
            if (it.upgrading) update = "  ⟳ updating";
            else if (it.alive && cluster::version_compare(it.version, installed) < 0) update = "  ⬆ " + installed + " to install (u)";
            rows.push_back(ftxui::hbox({ftxui::text(selected ? " ▸ " : "   "), ftxui::text("● ") | ftxui::color(tint(it)),
                ftxui::text(it.name) | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 16), ftxui::text(it.alive ? it.version : "-") | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
                ftxui::text(state) | ftxui::color(tint(it)), ftxui::text(update) | ftxui::color(ftxui::Color::Cyan),
                ftxui::text(it.web ? "  web on" : "") | ftxui::dim}) | (selected ? ftxui::inverted : ftxui::nothing));
            if (!details || !it.alive) continue;
            rows.push_back(ftxui::text("       pid " + std::to_string(it.pid) + (it.web ? " · " + it.url : "") + (it.offered.empty() ? "" : " · offered " + it.offered)) | ftxui::dim);
            for (const cluster::Json& s: it.sessions) {
                const std::string st = s.value("state", std::string());
                std::string line = "       " + cluster::one_line(s.value("name", std::string()), 22) + "  " + (s.value("asking", 0) > 0 ? std::string("waits for you (a question / a permission)")
                    : s.value("background", false) ? std::string("idle, its background runs") : st);
                if (s.value("queued", 0) > 0) line += " · " + std::to_string(s.value("queued", 0)) + " queued";
                if (s.value("agents", 0) > 0) line += " · " + std::to_string(s.value("agents", 0)) + " agent(s)";
                if (s.value("keep_warm", false)) line += " · ♨";
                if (!s.value("model", std::string()).empty()) line += " · " + s.value("model", std::string());
                rows.push_back(ftxui::text(line) | ftxui::color(s.value("asking", 0) > 0 ? ftxui::Color::Yellow : st == "working" ? ftxui::Color::Green : st == "error" ? ftxui::Color::Red
                    : s.value("background", false) ? ftxui::Color::Magenta : ftxui::Color::GrayLight));
            }
        }
        rows.push_back(ftxui::filler());
        if (!confirm.empty()) rows.push_back(ftxui::text(" " + (confirm == "stop" ? std::string("stop it (its terminals / windows closed, its sessions kept)") : confirm == "force"
            ? std::string("force its switch (its turns and background stopped, then continued)") : std::string("force the switch of every one that is behind")) + "? y / n ") | ftxui::bold | ftxui::color(ftxui::Color::Yellow));
        else if (!note.empty()) rows.push_back(ftxui::text(" " + note) | ftxui::color(ftxui::Color::Cyan));
        rows.push_back(ftxui::text(" ↑↓ choose · Enter open · u update · U update all · f force · F force all · s stop · S start · w web · F2 details · q quit") | ftxui::dim);
        return ftxui::vbox(std::move(rows));
    });
    ftxui::Component keys = ftxui::CatchEvent(view, [&](const ftxui::Event& event) {
        if (event == ftxui::Event::Custom) return false;
        if (!confirm.empty()) {
            const std::string asked = confirm;
            confirm.clear();
            if (event == ftxui::Event::Character('y') || event == ftxui::Event::Character('Y')) act(asked == "force*" ? std::string("force*") : asked);
            return true;
        }
        const int count = static_cast<int>(all.size());
        if (event == ftxui::Event::ArrowDown) focus = count > 0 ? (focus + 1) % count : 0;
        else if (event == ftxui::Event::ArrowUp) focus = count > 0 ? (focus + count - 1) % count : 0;
        else if (event == ftxui::Event::F2) details = !details;
        else if (event == ftxui::Event::Character('u')) act("update");
        else if (event == ftxui::Event::Character('U')) act("update*");
        else if (event == ftxui::Event::Character('f')) confirm = "force";
        else if (event == ftxui::Event::Character('F')) confirm = "force*";
        else if (event == ftxui::Event::Character('s')) confirm = "stop";
        else if (event == ftxui::Event::Character('S')) act("start");
        else if (event == ftxui::Event::Character('w')) {
            note = "…";
            std::thread([&]() {
                const std::string done = cluster::supervisor_web_toggle();
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    note = done;
                }
                screen.PostEvent(ftxui::Event::Custom);
            }).detach();
        } else if (event == ftxui::Event::Return && count > 0) {
            open = all[static_cast<std::size_t>(std::clamp(focus, 0, count - 1))].name;
            screen.Exit();
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
    if (updated) { // the binary installed is newer: this supervisor runs it at once (the same choice: the supervisor again)
        ::setenv("CLAUDE_CLUSTER_SUPERVISOR", "1", 1);
        cluster::upgrade_request(false);
    }
    return 0;
}
