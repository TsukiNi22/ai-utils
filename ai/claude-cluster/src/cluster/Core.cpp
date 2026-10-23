/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Core.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _Arguments
#define _IOManip
#include <utils/utils.hpp>
#include "cluster/VoiceEngines.hpp"
#include "cluster/StartMenu.hpp"
#include "cluster/Docs.hpp"
#include "cluster/Web.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Services.hpp"
#include "cluster/Service.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Icons.hpp"
#include "cluster/Demo.hpp"
#include "cluster/Core.hpp"
#include "cluster/Mcp.hpp"
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <unistd.h>
#include <csignal>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <csignal>
#include <thread>
#include <chrono>
#include <set>

/* tools */
_cold static std::optional<std::string> choice_(const std::string& value, const std::vector<std::string>& choices)
{
    if (std::find(choices.begin(), choices.end(), value) != choices.end()) return std::nullopt;
    std::string list;
    for (const std::string& choice: choices)
        list += (list.empty() ? "" : " | ") + choice;
    return "'" + value + "' is not one of: " + list;
}

_cold static bool tty_(void)
{
    return ::isatty(STDIN_FILENO) && ::isatty(STDOUT_FILENO);
}

_cold static bool foreground_(void)
{
    // the process group the terminal reads for: not started with & (nor sent to the background)
    const pid_t group = ::tcgetpgrp(STDIN_FILENO);
    return group == -1 || group == ::getpgrp();
}

_cold std::vector<int> cluster::interface_pids(void)
{
    // the terminals / windows of this install and this core (the same binary path, the same socket), not this one; not the
    // core itself, its service, its MCP servers nor a headless command
    std::vector<int> found;
    const std::string mine = cluster::self_exe().string();
    const char* env = std::getenv("CLAUDE_CLUSTER_SOCKET");
    const std::string socket = env != nullptr ? env : "";
    std::error_code error;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator("/proc", error)) {
        const std::string name = entry.path().filename().string();
        if (name.empty() || !std::all_of(name.begin(), name.end(), [](const char c) {return std::isdigit(static_cast<unsigned char>(c));})) continue;
        const int pid = std::stoi(name);
        if (pid == ::getpid()) continue;
        std::string exe = std::filesystem::read_symlink(entry.path() / "exe", error).string();
        if (error) continue;
        if (exe.ends_with(" (deleted)")) exe.resize(exe.size() - 10);
        if (exe != mine) continue;
        std::ifstream args(entry.path() / "cmdline", std::ios::binary);
        std::vector<std::string> argv;
        for (std::string arg; std::getline(args, arg, '\0');)
            argv.push_back(arg);
        if (argv.size() > 1 && !argv[1].starts_with("-")) continue; // serve, service, mcp, list...
        std::ifstream variables(entry.path() / "environ", std::ios::binary);
        std::string theirs;
        for (std::string var; std::getline(variables, var, '\0');)
            if (var.starts_with("CLAUDE_CLUSTER_SOCKET=")) theirs = var.substr(22);
        if (theirs == socket) found.push_back(pid);
    }
    return found;
}

_cold static bool display_(void)
{
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    const char* x11 = std::getenv("DISPLAY");
    return (wayland && *wayland) || (x11 && *x11);
}

/* app */
_cold void cluster::App::start(void)
{
    // 1 s tick: config reload, task queue, trash purge, voice confirmations
    this->ticker = std::thread([this]() {
        int ms = 0;
        while (this->running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (this->voice) this->voice->tick();
            if ((ms += 100) < 1000) continue;
            ms = 0;
            try {
                this->manager->tick();
            } catch (const utils::exception::IException&) {} // reported as notices by the manager
            if (this->web) this->web->tick();
        }
    });
}

_cold void cluster::App::stop(void)
{
    this->running = false;
    if (this->ticker.joinable()) this->ticker.join();
    if (this->manager) this->manager->setWeb(nullptr);
    if (this->manager) this->manager->setVoice(nullptr);
    this->web.reset();      // before the voice: a request of the page may still use it
    this->voice.reset();
    if (this->control) this->control->stop();
    if (this->manager) this->manager->shutdown();
    cluster::end_captures(2000); // a notification waiting for its click, a command still running: ended before the quit
}

/* setup */
_cold void cluster::Core::setup_(void)
{
    const std::function<std::optional<std::string>(const std::string&)> any = utils::arguments::defaultTrueParsingHook;
    const std::function<std::optional<std::string>(const std::string&)> modes = [](const std::string& v) {
        std::vector<std::string> names;
        for (const auto &[mode, description]: cluster::permission_modes())
            names.push_back(mode);
        return choice_(v, names);
    };
    const std::function<std::optional<std::string>(const std::string&)> layouts = [](const std::string& v) {return choice_(v, {"list", "grid", "graph", "hub", "tree", "tabs"});};
    const std::function<std::optional<std::string>(const std::string&)> shells = [](const std::string& v) {return choice_(v, {"bash", "zsh", "fish"});};

    /* front-end */
    this->_parser.setFlag("tty", {"t", "", "tty", ""}, {}, "Terminal interface (fails when stdin / stdout are not a terminal)");
    this->_parser.setFlag("gui", {"g", "", "gui", ""}, {}, "Window interface (fails when it is not built or there is no display)");
    this->_parser.setFlag("ttymode", {"", "", "tty-mode", ""}, {}, "-g: the window shows the terminal interface (a real terminal inside it; [ui] gui_mode = tty keeps it)");
    this->_parser.setFlag("backend", {"b", "", "backend", "CLAUDE_CLUSTER_BACKEND"}, {{"provider[/model]", true, any}},
        "Backend of the new sessions: claude (default), ollama/<model>, openai/<model>, opencode/<p>/<m>, codex... (auth list)");
    this->_parser.setFlag("restore", {"", "", "restore", ""}, {}, "Restore the sessions of the previous run without asking");
    this->_parser.setFlag("fresh", {"", "", "fresh", ""}, {}, "Start clean: the previous sessions go to the trash");
    this->_parser.setFlag("noglobal", {"", "", "no-global", ""}, {}, "No global session for this run");
    this->_parser.setFlag("layout", {"", "", "layout", ""}, {{"layout", true, layouts}}, "list | grid | graph | hub | tree (tabs: the window)");
    this->_parser.setFlag("style", {"", "", "style", ""}, {{"name", true, any}}, "Style of the interface (see the palette)");
    this->_parser.setFlag("voice", {"", "", "voice", ""}, {{"mode", true, [](const std::string& v) {return choice_(v, {"auto", "wake", "push"});}}},
        "Voice on for this run: auto (every sentence) | wake | push (the service opens claude-cluster with --voice auto)");
    this->_parser.setFlag("fork", {"", "", "fork", ""}, {}, "import: a copy of the conversation (Claude Code), the original untouched");
    this->_parser.setFlag("web", {"", "", "web", ""}, {}, "Web access on for this run: a browser of the same LAN drives it after the code shown");
    this->_parser.setFlag("verbose", {"v", "", "verbose", ""}, {}, "service run: the end of the text of every sound heard (tests)");
    this->_parser.setFlag("debug", {"", "", "debug", ""}, {}, "service setup / enable / run: the whole text heard in the journal (tests only)");
    this->_parser.setFlag("follow", {"", "", "follow", ""}, {}, "service logs: the new lines as they come");
    this->_parser.setFlag("yes", {"y", "", "yes", ""}, {}, "machine sync: no question (the scan accepted)");
    this->_parser.setFlag("rescan", {"", "", "rescan", ""}, {}, "Ask at start to sync the map of this computer (even recent or refused)");
    this->_parser.setFlag("linger", {"", "", "linger", ""}, {}, "serve: the core of the interfaces (started by them): it stops when the last one closes");
    this->_parser.setFlag("every", {"", "", "every", ""}, {{"minutes", true, any}}, "schedule add: repeated every N minutes");
    this->_parser.setFlag("notify", {"", "", "notify", ""}, {}, "schedule add: a desktop notification (not a prompt)");
    this->_parser.setFlag("cron", {"", "", "cron", ""}, {{"expr", true, any}}, "schedule add: a cron expression (\"*/15 9-17 * * mon-fri\")");
    this->_parser.setFlag("days", {"", "", "days", ""}, {{"mon-fri", true, any}}, "schedule add: only these days");
    this->_parser.setFlag("window", {"", "", "window", ""}, {{"09:00-18:00", true, any}}, "schedule add: only inside these hours");
    this->_parser.setFlag("quiet", {"", "", "quiet", ""}, {{"22:00-07:00", true, any}}, "schedule add: never inside these hours");
    this->_parser.setFlag("tz", {"", "", "tz", ""}, {{"zone", true, any}}, "schedule add: the timezone of the above (Europe/Paris)");
    this->_parser.setFlag("until", {"", "", "until", ""}, {{"when", true, any}}, "schedule add: ended after this time");
    this->_parser.setFlag("max-runs", {"", "", "max-runs", ""}, {{"n", true, any}}, "schedule add: ended after N runs");
    this->_parser.setFlag("dry-run", {"", "", "dry-run", ""}, {}, "schedule add: only show when it would fire");
    this->_parser.setFlag("priority", {"", "", "priority", ""}, {{"n", true, any}}, "task add: higher first");
    this->_parser.setFlag("retry", {"", "", "retry", ""}, {{"n", true, any}}, "task add: tried again N times after an error");
    this->_parser.setFlag("timeout", {"", "", "timeout", ""}, {{"minutes", true, any}}, "task add: stopped when running longer");

    /* headless */
    this->_parser.setFlag("name", {"n", "", "name", ""}, {{"name", true, any}}, "spawn: name of the session");
    this->_parser.setFlag("mode", {"m", "", "mode", ""}, {{"mode", true, modes}}, "spawn: permission mode");
    this->_parser.setFlag("profile", {"p", "", "profile", ""}, {{"name", true, any}}, "spawn: profile of the config");
    this->_parser.setFlag("prompt", {"", "", "prompt", ""}, {{"text", true, any}}, "spawn: first prompt");
    this->_parser.setFlag("target", {"", "", "target", ""}, {{"session|folder", true, any}}, "task add: session or folder");
    this->_parser.setFlag("after", {"", "", "after", ""}, {{"t1,t2", true, any}}, "task add: tasks to finish first");
    this->_parser.setFlag("category", {"", "", "category", ""}, {{"category", true, any}}, "list: only this category; label: the category of the session (empty: removed)");
    this->_parser.setFlag("tag", {"", "", "tag", ""}, {{"a,b", true, any}}, "list: only the sessions with these tags; label: the tags of the session (empty: removed)");
    this->_parser.setFlag("state", {"", "", "state", ""}, {{"state", true, any}}, "list: open (default) | archived | all | working | idle...");
    this->_parser.setFlag("force", {"f", "", "force", ""}, {}, "send: ignore the budget");
    this->_parser.setFlag("demo", {"", "", "demo", ""}, {}, "A demo: fictional projects, sessions, tasks and schedules in an instance of its own (nothing real is touched or spent; with --web: read only)");
    this->_parser.setFlag("rtk", {"", "", "rtk", "CLAUDE_CLUSTER_RTK"}, {}, "Compact output for an AI (headless commands)");
    this->_parser.setFlag("json", {"j", "", "json", ""}, {}, "Raw JSON output (headless commands)");

    /* misc */
    this->_parser.setFlag("version", {"V", "", "version", ""}, {}, "Print the version");
    this->_parser.setFlag("completion", {"", "", "completion", ""}, {{"shell", true, shells}}, "Print the completion script (bash | zsh | fish)");
    this->_parser.setDefaultUsage();
    this->_parser.setHelpHook([this](const utils::arguments::ArgParser&) {this->help_();});
}

_cold std::vector<std::string> cluster::Core::extract_(const int argc, char* argv[])
{
    // ArgParser has no positional arguments: the command and its words are taken out before the parsing
    static const std::unordered_set<std::string> values = {"b", "backend", "layout", "style", "n", "name", "m", "mode", "p", "profile",
        "prompt", "target", "after", "completion", "voice", "every", "category", "tag", "state"};
    std::vector<std::string> arguments = {argc > 0 ? argv[0] : "claude-cluster"};
    bool rest = false;

    for (int i = 1; i < argc; ++i) {
        const std::string token = argv[i];
        if (rest || token.size() < 2 || token[0] != '-') {
            this->_words.push_back(token);
            continue;
        }
        if (token == "--") {
            rest = true;
            continue;
        }
        arguments.push_back(token);
        if (token.find('=') != std::string::npos) continue;
        std::string name = token.substr(token.starts_with("--") ? 2 : 1);
        if (!token.starts_with("--")) name = name.substr(name.size() - 1);
        if (values.contains(name) && i + 1 < argc) arguments.push_back(argv[++i]);
    }
    return arguments;
}

_cold void cluster::Core::init(const int argc, char* argv[])
{
    cluster::complete_path(); // its agents found whoever started it
    this->setup_();
    const std::vector<std::string> arguments = this->extract_(argc, argv);
    const utils::arguments::ParsedUsages usages = this->_parser.parse(arguments);
    if (usages.empty()) return;
    if (std::find(arguments.begin(), arguments.end(), "--demo") != arguments.end()) // before anything reads a path: the demo folder, its own socket
        cluster::demo_prepare(std::find(arguments.begin(), arguments.end(), "--web") != arguments.end());

    for (const auto &[id, option, values]: usages[0].arguments) {
        const std::string value = values.empty() ? "" : values[0];
        if (id == "tty") this->_options.frontend = "tty";
        else if (id == "gui") this->_options.frontend = "gui";
        else if (id == "backend") this->_options.backend = value;
        else if (id == "restore") this->_options.restore = "always";
        else if (id == "fresh") this->_options.restore = "never";
        else if (id == "noglobal") this->_options.global = false;
        else if (id == "layout") this->_options.layout = value;
        else if (id == "style") this->_options.style = value;
        else if (id == "rtk") this->_options.rtk = true;
        else if (id == "json") this->_options.json = true;
        else if (id == "version") this->_version = true;
        else if (id == "completion") this->_completion = value;
        else if (id == "force") this->_options.values["force"] = "1";
        else this->_options.values[id] = value;
    }
    const char* rtk = std::getenv("CLAUDE_CLUSTER_RTK");
    if (rtk && std::string(rtk) != "0" && std::string(rtk) != "") this->_options.rtk = true;
}

/* run */
_cold void cluster::Core::run(void)
{
    if (this->_version) {
        std::cout << "claude-cluster " << CLUSTER_VERSION << std::endl;
        return;
    }
    if (!this->_completion.empty()) {
        this->completion_();
        return;
    }
    if (this->_words.empty()) {
        this->_exit = this->ui_();
        return;
    }
    const std::string& cmd = this->_words[0];
    if (cmd == "mcp") this->_exit = cluster::mcp_serve();
    else if (cmd == "demo-agent") this->_exit = cluster::demo_agent();
    else if (cmd == "__complete") this->complete_(this->_words.size() > 1 ? this->_words[1] : "");
    else if (cmd == "serve") this->_exit = this->ui_(true);
    else if (cmd == "auth") this->_exit = this->auth_();
    else if (cmd == "engines") this->_exit = this->engines_();
    else if (cmd == "docs") this->_exit = this->docs_();
    else if (cmd == "vt-test") this->_exit = cluster::vt_test(this->_words);
    else if (cmd == "icon") this->_exit = this->icon_();
    else if (cmd == "web" && this->_words.size() > 1 && (this->_words[1] == "trust-ca" || this->_words[1] == "untrust-ca")) this->_exit = this->trustCa_();
    else if (cmd == "machine") this->_exit = this->machine_();
    else if (cmd == "service") this->_exit = cluster::service_command(this->_words, {this->_options.values.contains("verbose"),
        this->_options.values.contains("debug"), this->_options.values.contains("follow")});
    else if (cmd == "help") this->help_();
    else this->_exit = this->headless_();
}

_cold bool cluster::Core::otherBuild_(const bool window)
{
    // the core running is of another build (installed again, older or newer): asked - replace it (its interfaces and it
    // stopped, this build started) or quit without anything; true: go on (the same build, or replaced)
    std::string theirs = "an older one (before 1.4.0)";
    std::string build;
    bool older = false; // it answered without hello: a version before it
    for (int i = 0; i < 30; ++i) {
        try {
            const cluster::Json r = cluster::Control::request({{"cmd", "hello"}});
            if (r.value("ok", false)) {
                build = r["data"].value("build", std::string());
                theirs = r["data"].value("version", std::string()) + " (" + build + ")";
                break;
            }
            older = r.value("error", std::string()).find("unknown command") != std::string::npos;
            if (older) break;
        } catch (const utils::exception::IException&) {}
        // not answered (a core stopping, starting): asked again a moment; gone: this one starts its own, nothing asked
        if (!cluster::Control::alive()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    if (build == cluster::build_id()) return true;
    if (build.empty() && !older) return true; // never answered (stuck): left to the attach, which tells it
    const std::vector<int> interfaces = cluster::interface_pids();
    const std::string question = "claude-cluster " + theirs + " runs the core of your sessions; this one is " + cluster::build_id() + ".\n\n"
        "Replace it: its terminals / windows (" + std::to_string(interfaces.size()) + ") and its core stopped (the sessions kept: resumed by this one), "
        "this version started.\nQuit: nothing changed (open the other version, or close it first).\n\nIts service, if any, runs on: claude-cluster service restart.";
    int choice = -1;
    #ifdef CLUSTER_GUI
        if (window) choice = cluster::gui_choice("claude-cluster: another version runs", question, {"Replace it", "Quit"});
    #endif
    if (!window) {
        std::cout << question << "\n\n[r] replace it   [q] quit (default) > " << std::flush;
        std::string answer;
        (void)std::getline(std::cin, answer);
        choice = cluster::lower(cluster::trim(answer)) == "r" ? 0 : 1;
    }
    if (choice != 0) {
        if (!window) std::cout << "nothing changed" << std::endl;
        return false;
    }
    // its interfaces first (they would start their core again), then the core; waited for (10 s, then killed)
    for (const int pid: interfaces)
        (void)::kill(pid, SIGTERM);
    const int core = cluster::Control::corePid();
    if (core > 0) (void)::kill(core, SIGTERM);
    for (int i = 0; i < 100 && (cluster::Control::alive() || (core > 0 && ::kill(core, 0) == 0)); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (core > 0 && ::kill(core, 0) == 0) (void)::kill(core, SIGKILL);
    for (const int pid: interfaces)
        if (::kill(pid, 0) == 0) (void)::kill(pid, SIGKILL);
    std::error_code error;
    if (!cluster::Control::alive()) std::filesystem::remove(cluster::socket_path(), error); // left by a killed one
    if (!window) std::cout << "replaced: this version starts" << std::endl;
    return true;
}

_cold int cluster::Core::ui_(const bool serve)
{
    (void)cluster::build_id(); // its build read now (the binary may be installed again while it runs)
    cluster::App app;
    app.options = this->_options;
    if (this->_options.values.contains("voice")) app.config.voiceForce = this->_options.values.at("voice");
    app.config.load();
    if (!app.config.error().empty()) std::cerr << "claude-cluster: config error (defaults used): " << app.config.error() << std::endl;
    if (!this->_options.backend.empty()) app.config.backend = this->_options.backend;
    if (!this->_options.global) app.config.globalEnabled = false;
    (void)app.config.currentStyle();
    if (std::getenv("CLAUDE_CLUSTER_INSTANCE") != nullptr) app.config.web.enabled = false; // an independent instance: no port taken from the main one
    cluster::install_icon(); // the menu, the dock and the notifications show it

    // Front-end: forced (fails when impossible) or auto (terminal first, then window)
    std::string frontend = this->_options.frontend.empty() ? app.config.frontend : this->_options.frontend;
    #ifdef CLUSTER_GUI
        const bool gui = true;
    #else
        const bool gui = false;
    #endif
    if (!serve && frontend == "tty" && !tty_())
        throw utils::exception::ErrorException(utils::exception::InternalCode::CliTTY, "--tty: stdin / stdout are not a terminal");
    if (!serve && frontend == "gui" && !gui)
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "--gui: the window front-end is not built (Qt6 Widgets missing at build time)");
    if (!serve && frontend == "gui" && !display_())
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "--gui: no display ($WAYLAND_DISPLAY / $DISPLAY)");
    if (!serve && frontend != "tty" && frontend != "gui") {
        // started with &: a window (it runs fine in the background), never a terminal interface left stopped there
        if (tty_() && (foreground_() || !gui || !display_())) frontend = "tty";
        else if (gui && display_()) frontend = "gui";
        else throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no terminal and no window available");
    }
    if (!serve && frontend == "tty" && !foreground_())
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the terminal interface runs in the foreground only (not with &): "
            "claude-cluster alone; in the background: claude-cluster --gui & or claude-cluster serve &");

    // the menu of the start (always, in a terminal that is really there): what to open or start; an isolated run (tests, the demo, a web terminal) has none
    if (!serve && frontend == "tty" && app.config.startMenu && this->_options.restore.empty() && !cluster::demo_enabled() && std::getenv("CLAUDE_CLUSTER_SOCKET") == nullptr
        && std::getenv("CLAUDE_CLUSTER_WEB_TTY") == nullptr && std::getenv("CLAUDE_CLUSTER_NO_MENU") == nullptr && !this->_options.values.contains("voice")) {
        const cluster::StartChoice choice = cluster::start_menu(CLUSTER_VERSION);
        if (choice.kind == cluster::StartChoice::Kind::Quit) return 0;
        if (choice.kind == cluster::StartChoice::Kind::Restore) this->_options.restore = "always";
        else if (choice.kind == cluster::StartChoice::Kind::New) this->_options.restore = "never";
        else if (choice.kind == cluster::StartChoice::Kind::Independent) cluster::start_instance(choice.instance);
        app.options = this->_options;
    }

    // Core: one per user (control socket), previous sessions, global session. An interface attaches to the core running, or to
    // one started for it (a process of its own: several terminals / windows at once, all in sync; the core goes on until the
    // last one closes); a core already running for serve: told before anything starts (no session, no thread left behind)
    const bool linger = serve && this->_options.values.contains("linger");
    if (serve && cluster::Control::alive())
        throw utils::exception::ErrorException(utils::exception::InternalCode::AlreadyRunning, "claude-cluster already runs (" + cluster::socket_path().string()
            + "): its terminal or window is open elsewhere, or its service runs (claude-cluster service status); drive it with the headless commands "
            "(claude-cluster list, send...) or its web access, or close it first");
    std::vector<std::string> coreArgs; // the options of this run that are the core's
    if (this->_options.restore == "always") coreArgs.push_back("--restore");
    if (this->_options.restore == "never") coreArgs.push_back("--fresh");
    if (!this->_options.global) coreArgs.push_back("--no-global");
    if (this->_options.values.contains("web")) coreArgs.push_back("--web");
    if (this->_options.values.contains("voice")) coreArgs.insert(coreArgs.end(), {"--voice", this->_options.values.at("voice")});
    if (!serve && app.config.sharedCore && cluster::Control::alive() && !this->otherBuild_(frontend == "gui"))
        return 0; // a core of another build kept: quit without anything
    const bool attached = !serve && app.config.sharedCore && (cluster::Control::alive() || cluster::start_core(coreArgs));
    cluster::debug_role(serve ? "core" : attached ? (std::getenv("CLAUDE_CLUSTER_WEB_TTY") != nullptr ? "web terminal" : frontend == "gui" ? "window" : "terminal")
        : (frontend == "gui" ? "window+core" : "terminal+core"));
    cluster::debug("core", serve ? std::string("started (") + (linger ? "by an interface: it stops with the last one" : "claude-cluster serve") + ")"
        : attached ? "interface attached to the core " + cluster::socket_path().string() : "this interface is the core (shared core off)");
    try {
    if (attached) {
        // an interface of the core: its state kept in sync, its actions run by the core (its voice, its web access, its sessions)
        app.manager = std::make_unique<cluster::Manager>(app.config);
        try {
            app.manager->attach();
        } catch (const utils::exception::IException& e) {
            // a claude-cluster of a version before the shared core: it can't take interfaces
            throw utils::exception::ErrorException(utils::exception::InternalCode::AlreadyRunning, "a claude-cluster of an older version runs ("
                + std::string(e.info()) + "): close it (its service: claude-cluster service restart), then open it again");
        }
        if (!this->_options.layout.empty()) app.manager->setUi("layout", this->_options.layout);
        if (!this->_options.style.empty()) app.manager->setUi("style", this->_options.style);
        app.askRestore = app.manager->restorable();
        app.askMachine = cluster::demo_enabled() ? 0 : this->_options.values.contains("rescan") ? 3 : app.manager->machineAsk();
        app.voice = std::make_unique<cluster::Voice>(*app.manager);
        app.start();
    } else {
    app.manager = std::make_unique<cluster::Manager>(app.config);
    app.control = std::make_unique<cluster::Control>(*app.manager);
    app.manager->load();
    if (!this->_options.layout.empty()) app.manager->setUi("layout", this->_options.layout);
    if (!this->_options.style.empty()) app.manager->setUi("style", this->_options.style);
    const std::string restore = this->_options.restore.empty() ? app.config.restore : this->_options.restore;
    bool askedLater = false;
    if (app.manager->restorable()) {
        if (restore == "always") app.manager->restorePrevious(true);
        else if (restore == "never") app.manager->restorePrevious(false);
        else if (linger) { // the core of the interfaces: its first interface asks
            app.manager->askRestoreLater();
            askedLater = true;
        } else if (serve) app.manager->restorePrevious(true); // nobody to ask
        else app.askRestore = true;
    }
    // the socket opened once the question is set: an interface attached before saw no question, and the core waited for its answer forever
    app.control->start();
    if (!app.askRestore && !askedLater) app.manager->startGlobal(); // asked: started by the answer (resumed or new); the tick starts it if it is missing anyway
    app.askMachine = serve || cluster::demo_enabled() ? 0 : this->_options.values.contains("rescan") ? 3 : app.manager->machineAsk(); // 3: forced (--rescan)
    app.voice = std::make_unique<cluster::Voice>(*app.manager);
    app.web = std::make_unique<cluster::Web>(*app.manager, app.voice.get(), this->_options.values.contains("web") ? std::optional<bool>(true) : std::nullopt);
    app.manager->setWeb(app.web.get());
    app.manager->setVoice(app.voice.get());
    app.start();
    if (cluster::demo_enabled()) cluster::demo_seed(*app.manager); // the fictional sessions, one after the other
    }
    } catch (...) { // a start failing: what it started stopped (its sessions, its threads)
        app.stop();
        throw;
    }

    int code = 0;
    cluster::catch_quit_signals(); // Ctrl+C, kill, the terminal closed: a normal quit (sessions saved, terminal restored)
    if (serve) {
        // Headless instance: the commands and the MCP server drive it, until a quit signal
        std::cerr << "claude-cluster: serving on " << cluster::socket_path().string() << " (Ctrl+C to stop)" << std::endl;
        const std::int64_t started = cluster::now();
        std::int64_t idleSince = 0;
        while (!cluster::quit_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            // the core of the interfaces: gone with its last one (none came within a minute: gone too); kept in background by
            // its last interface: gone once every session is idle for 5 s (an interface back meanwhile: kept as usual)
            if (linger && app.manager->clients() == 0 && (app.manager->hadClients() || cluster::now() - started > 60)) {
                if (!app.manager->background()) {
                    cluster::debug("core", app.manager->hadClients() ? "stops: its last interface closed" : "stops: no interface came within a minute");
                    break;
                }
                if (app.manager->busy()) idleSince = 0;
                else if (idleSince == 0) idleSince = cluster::now();
                else if (cluster::now() - idleSince >= 5) {
                    cluster::debug("core", "stops: every session kept in background is idle");
                    (void)app.manager->notifyDesktop("claude-cluster", "the sessions kept in background finished: the core stopped");
                    break;
                }
            }
        }
        if (cluster::quit_requested()) cluster::debug("core", "stops: a quit signal (Ctrl+C, kill, Stop all / Restart)");
        app.stop();
        if (cluster::demo_enabled()) std::atexit([]() {cluster::demo_cleanup(true);}); // the demo: its folder gone with its core
        return 0;
    }
    try {
        #ifdef CLUSTER_GUI
            code = frontend == "gui" ? cluster::run_gui(app) : cluster::run_tty(app);
        #else
            code = cluster::run_tty(app);
        #endif
    } catch (...) {
        app.stop();
        throw;
    }
    if (app.manager->background() && !app.manager->replica() && !cluster::quit_requested()) {
        // the core of this process kept: without interface, until every session is idle (the terminal stays busy; closed: they go on)
        std::cerr << "claude-cluster: the sessions keep running in background (this terminal stays busy until they finish; closing it is fine); "
            "Ctrl+C stops them" << std::endl;
        cluster::ignore_hangup();
        std::int64_t idleSince = 0;
        while (!cluster::quit_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (app.manager->busy()) idleSince = 0;
            else if (idleSince == 0) idleSince = cluster::now();
            else if (cluster::now() - idleSince >= 5) {
                (void)app.manager->notifyDesktop("claude-cluster", "the sessions kept in background finished: the core stopped");
                break;
            }
        }
    }
    app.stop();
    // the demo: its core gone, its folder too (at the very end of the process: the last writes of the core done)
    if (cluster::demo_enabled() && (serve || !attached)) std::atexit([]() {cluster::demo_cleanup(true);});
    return code;
}

/* headless */
_cold int cluster::Core::headless_(void)
{
    const std::vector<std::string>& w = this->_words;
    const std::string cmd = w[0];
    const std::function<std::string(const std::size_t, const std::string&)> word = [&](const std::size_t i, const std::string& what) -> std::string {
        if (i >= w.size()) throw utils::exception::ErrorException(utils::exception::InternalCode::ArgumentsNumber, cmd + ": missing <" + what + ">");
        return w[i];
    };
    const std::function<std::string(const std::size_t)> rest = [&](const std::size_t from) {
        std::string text;
        for (std::size_t i = from; i < w.size(); ++i)
            text += (text.empty() ? "" : " ") + w[i];
        return text;
    };
    const std::map<std::string, std::string>& v = this->_options.values;
    const std::function<std::string(const std::string&)> value = [&](const std::string& key) {return v.contains(key) ? v.at(key) : std::string();};
    cluster::Json request = {{"cmd", cmd}};
    std::string shown = cmd;

    if (cmd == "list") {
        for (const char* key: {"category", "tag", "state"})
            if (v.contains(key)) request[key] = v.at(key);
    } else if (cmd == "trash" || cmd == "notices" || cmd == "providers") {
    } else if (cmd == "label") {
        request.update({{"cmd", "describe"}, {"session", word(1, "session")}});
        if (v.contains("category")) request["category"] = v.at("category");
        if (v.contains("tag")) request["tags"] = value("tag");
        if (!v.contains("category") && !v.contains("tag")) word(2, "--category / --tag");
    } else if (cmd == "taxonomy") {
        const std::string action = w.size() > 1 ? w[1] : "list";
        request["action"] = action;
        if (action != "list") request["node"] = word(2, "node");
        if (action == "set_parent") request["parent"] = w.size() > 3 ? w[3] : "";
        else if (action == "rename") request["to"] = word(3, "new name");
        else if (action == "merge") request["to"] = word(3, "into");
        else if (action == "delete") request["strip"] = w.size() > 3 && w[3] == "strip";
    } else if (cmd == "web") {
        request["action"] = w.size() > 1 ? w[1] : "";
        if (request["action"] == "approve") request.update({{"id", word(2, "id")}, {"answer", w.size() > 3 ? w[3] : "allow"}});
        if (request["action"] == "kick" || request["action"] == "untrust" || request["action"] == "ban" || request["action"] == "unban") request["ids"] = w.size() > 2 ? w[2] : "";
    } else if (cmd == "external") {
        request["agent"] = w.size() > 1 ? w[1] : "";
    } else if (cmd == "import") {
        request.update({{"agent", word(1, "agent")}, {"id", word(2, "id")}, {"name", value("name")}, {"fork", v.contains("fork")}});
    } else if (cmd == "fork") {
        request.update({{"session", word(1, "session")}, {"name", value("name")}});
    } else if (cmd == "status" || cmd == "read" || cmd == "close" || cmd == "interrupt") {
        request["session"] = word(1, "session");
    } else if (cmd == "spawn") {
        request.update({{"cwd", w.size() > 1 ? w[1] : "."}, {"name", value("name")}, {"backend", this->_options.backend}, {"mode", value("mode")},
            {"profile", value("profile")}, {"prompt", value("prompt")}});
        if (!request["cwd"].get<std::string>().starts_with("/") && !request["cwd"].get<std::string>().starts_with("~"))
            request["cwd"] = std::filesystem::absolute(request["cwd"].get<std::string>()).string();
    } else if (cmd == "send") {
        request.update({{"session", word(1, "session")}, {"text", rest(2)}, {"force", v.contains("force")}});
        if (request["text"].get<std::string>().empty()) word(2, "text");
    } else if (cmd == "restore" || cmd == "purge") {
        request["session"] = w.size() > 1 ? w[1] : "";
    } else if (cmd == "rename") {
        request.update({{"session", word(1, "session")}, {"name", rest(2)}});
    } else if (cmd == "describe") {
        request.update({{"session", word(1, "session")}, {"text", rest(2)}});
    } else if (cmd == "continue" || cmd == "retry") {
        request.update({{"cmd", "resume"}, {"session", word(1, "session")}});
    } else if (cmd == "mode") {
        request.update({{"session", word(1, "session")}, {"mode", word(2, "mode")}});
    } else if (cmd == "model") {
        request.update({{"session", word(1, "session")}, {"model", w.size() > 2 ? w[2] : ""}});
    } else if (cmd == "effort") {
        request.update({{"cmd", "model"}, {"session", word(1, "session")}, {"effort", word(2, "level")}});
    } else if (cmd == "usage") {
        request["vendor"] = w.size() > 1 ? w[1] : "";
    } else if (cmd == "allow") {
        request.update({{"session", word(1, "session")}, {"behavior", w.size() > 2 ? w[2] : "allow"}, {"request_id", w.size() > 3 ? w[3] : ""}});
    } else if (cmd == "cd") {
        request.update({{"session", word(1, "session")}, {"cwd", std::filesystem::absolute(cluster::expand_home(word(2, "folder"))).string()}});
    } else if (cmd == "search") {
        request["text"] = rest(1);
    } else if (cmd == "export") {
        request.update({{"session", word(1, "session")}, {"path", w.size() > 2 ? w[2] : ""}});
    } else if (cmd == "remote") {
        request["on"] = word(1, "on|off") == "on";
    } else if (cmd == "schedule") {
        const std::string sub = word(1, "add|list|cancel");
        shown = "schedule_" + sub;
        request["cmd"] = shown;
        if (sub == "add") {
            request.update({{"when", word(2, "when")}, {"text", rest(3)}, {"session", value("target")},
                {"action", this->_options.values.contains("notify") ? "notify" : "prompt"}, {"every", value("every").empty() ? 0 : std::stoi(value("every"))}});
            for (const auto &[flag, key]: std::map<std::string, std::string>{{"cron", "cron"}, {"days", "days"}, {"window", "window"}, {"quiet", "quiet"}, {"tz", "tz"}, {"until", "until"}, {"max-runs", "max_runs"}})
                if (!value(flag).empty()) request[key] = value(flag);
            if (this->_options.values.contains("dry-run")) request["dry_run"] = true;
        } else if (sub == "cancel") {
            request["id"] = word(2, "schedule id");
        } else if (sub == "pause" || sub == "resume" || sub == "run" || sub == "ack") {
            request.update({{"cmd", "schedule_act"}, {"id", word(2, "schedule id")}, {"action", sub == "run" ? "trigger" : sub}});
        } else if (sub != "list") {
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "schedule " + sub + ": add | list | cancel | pause | resume | run | ack");
        }
    } else if (cmd == "task") {
        const std::string sub = word(1, "add|list|cancel");
        shown = "task_" + sub;
        request["cmd"] = shown;
        if (sub == "add") {
            request.update({{"prompt", rest(2)}, {"target", value("target")}, {"after", cluster::split(value("after"), ',')}});
            for (const char* key: {"priority", "retry", "timeout"})
                if (!value(key).empty()) request[std::string(key) == "timeout" ? "timeout_minutes" : key] = value(key);
        } else if (sub == "cancel") {
            request["id"] = word(2, "task id");
        } else if (sub == "retry" || sub == "force") {
            request.update({{"cmd", "task_act"}, {"id", word(2, "task id")}, {"action", sub}});
        } else if (sub != "list") {
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "task " + sub + ": add | list | cancel | retry | force");
        }
    } else {
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown command '" + cmd + "' (claude-cluster --help)");
    }

    const cluster::Json response = cluster::Control::request(request);
    if (!response.value("ok", false)) {
        std::cerr << "claude-cluster: " << cmd << ": " << response.value("error", std::string("error")) << std::endl;
        return 1;
    }
    this->print_(shown, response.value("data", cluster::Json()));
    return 0;
}

_cold static std::string join_tags_(const cluster::Json& session, const bool hash = false)
{
    // the tags of a session of the list: "a,b" (hash: "#a #b")
    std::string out;
    if (!session.contains("tags") || !session["tags"].is_array()) return out;
    for (const cluster::Json& tag: session["tags"])
        out += (out.empty() ? "" : hash ? " " : ",") + std::string(hash ? "#" : "") + tag.get<std::string>();
    return out;
}

_cold void cluster::Core::print_(const std::string& cmd, const cluster::Json& data) const
{
    if (this->_options.json) {
        std::cout << data.dump(2, ' ', false, cluster::JSON_LENIENT) << std::endl;
        return;
    }
    const bool rtk = this->_options.rtk;
    if (cmd == "external") {
        for (const cluster::Json& c: data)
            if (rtk) std::cout << c.value("agent", "") << "|" << c.value("id", "") << "|" << c.value("age", "") << "|" << cluster::short_path(c.value("cwd", ""))
                << "|" << c.value("title", "") << "\n";
            else std::cout << std::left << std::setw(9) << c.value("agent", "") << std::setw(10) << c.value("id", "").substr(0, 8) << std::setw(8) << c.value("age", "")
                << std::setw(34) << cluster::one_line(cluster::short_path(c.value("cwd", "")), 33) << cluster::one_line(c.value("title", ""), 70) << "\n";
        if (data.empty() && !rtk) std::cout << "no conversation of another agent" << "\n";
        else if (!rtk) std::cout << "\nopen one here: claude-cluster import <agent> <id> [--name n] [--fork: a copy, the original untouched]" << "\n";
        return;
    }
    if (cmd == "web") {
        // open: its address, its code, the browser connected; closed: how to open it
        const std::string action = data.value("action", std::string());
        if (rtk) {
            std::cout << (data.value("on", false) ? "on|" + data.value("url", std::string()) + "|" + data.value("pin", std::string()) + "|"
                + data.value("client", std::string()) : "off|" + data.value("error", std::string())) << "\n";
            return;
        }
        const bool on = data.value("on", false);
        if (!on)
            std::cout << "web access closed" << (data.value("error", std::string()).empty() ? "" : " (" + data.value("error", std::string()) + ")")
                << (action == "off" ? "" : "   open it: claude-cluster web on") << "\n";
        if (action == "kick") std::cout << (data.value("kicked", false) ? "disconnected" : "nobody connected") << "\n";
        if (action == "pin") std::cout << "a new code" << "\n";
        if (action == "approve") std::cout << "answered" << "\n";
        if (action == "untrust") std::cout << "trusted no more" << "\n";
        if (action == "ban") std::cout << "banned (refused from now, their code too)" << "\n";
        if (action == "unban") std::cout << "unbanned (the code asked again)" << "\n";
        if (on) std::cout << "web access open   " << data.value("url", std::string()) << "   code " << data.value("pin", std::string()) << "\n";
        // the devices: waiting for you, connected, trusted
        const std::function<void(const cluster::Json&, const std::string&)> devices = [](const cluster::Json& list, const std::string& title) {
            if (list.empty()) return;
            std::cout << title << "\n";
            for (const cluster::Json& d: list)
                std::cout << "  " << d.value("id", std::string()) << "  " << d.value("state", std::string()) << (d.value("trusted", false) && d.value("state", std::string()) != "trusted"
                    ? " (trusted)" : "") << "  " << d.value("name", std::string()) << "  " << d.value("address", std::string())
                    << (d.value("ping", -1) >= 0 ? "  ping " + std::to_string(d.value("ping", -1)) + " ms" : "") << "\n";
        };
        devices(data.value("devices", cluster::Json::array()), "devices:");
        devices(data.value("trusted", cluster::Json::array()), "trusted devices (no code, no approval):");
        devices(data.value("banned", cluster::Json::array()), "banned devices (refused):");
        if (on && data.value("devices", cluster::Json::array()).empty()) std::cout << "nobody connected: the code, then your approval here" << "\n";
        std::cout << "approve: claude-cluster web approve <id> allow|trust|deny · out: web kick [ids|all] · trust removed: web untrust [ids|all] · "
            "web ban <ids> / unban [ids|all]" << "\n";
        if (!data.value("remote", std::string()).empty()) std::cout << "from outside: " << data.value("remote", std::string()) << "\n";
        if (on && data.value("https", false))
            std::cout << "certificate SHA-256 " << data.value("fingerprint", std::string()) << " (the browser asks to accept it once: compare)" << "\n";
        else if (on)
            std::cout << "plain http: no microphone in the page ([web] https = true, then claude-cluster web off / on)" << "\n";
        return;
    }
    if (cmd == "taxonomy") {
        // the forest under global: node|parent|sessions (rtk), else the indented tree
        const cluster::Json parents = data.value("parents", cluster::Json::object());
        const cluster::Json labels = data.value("labels", cluster::Json::object());
        std::set<std::string> nodes;
        for (cluster::Json::const_iterator item = parents.begin(); item != parents.end(); ++item)
            nodes.insert(item.key());
        for (cluster::Json::const_iterator item = labels.begin(); item != labels.end(); ++item)
            nodes.insert(item.key());
        const std::function<std::string(const std::string&)> parentOf = [&parents](const std::string& node) {
            return parents.contains(node) && parents[node].is_string() ? parents[node].get<std::string>() : std::string();
        };
        const std::function<int(const std::string&)> count = [&labels](const std::string& node) {return labels.value(node, 0);};
        if (rtk) {
            std::cout << "node|parent|sessions" << "\n";
            for (const std::string& node: nodes)
                std::cout << node << "|" << (parentOf(node).empty() ? std::string("global") : parentOf(node)) << "|" << count(node) << "\n";
            return;
        }
        const std::function<void(const std::string&, const std::string&)> show = [&](const std::string& parent, const std::string& indent) {
            for (const std::string& node: nodes)
                if (parentOf(node) == parent) {
                    std::cout << indent << node << " (" << count(node) << ")" << "\n";
                    show(node, indent + "  ");
                }
        };
        std::cout << "global" << "\n";
        show("", "  ");
        std::cout << "set the parent: claude-cluster taxonomy set_parent <node> [parent] · rename / merge <node> <name> · delete <node> [strip]" << "\n";
        return;
    }
    if (cmd == "list") {
        if (rtk) std::cout << "id|name|state|backend|mode|tok_total|ctx%|cost|perm|cwd|summary|category|tags" << "\n";
        for (const cluster::Json& s: data) {
            // an archived session (the trash) has only its labels: no context, tokens or permissions
            const cluster::Json none = cluster::Json::object();
            const cluster::Json& context = s.contains("context") ? s.at("context") : none;
            const cluster::Json& tokens = s.contains("tokens") ? s.at("tokens") : none;
            const std::size_t permissions = s.contains("permissions") ? s.at("permissions").size() : 0;
            const std::int64_t window = std::max<std::int64_t>(1, context.value("window", std::int64_t{1}));
            const std::int64_t ctx = 100 * context.value("used", std::int64_t{0}) / window;
            const std::string cost = s.contains("cost_usd") && s.at("cost_usd").is_number() ? cluster::human_cost(s.at("cost_usd").get<double>()) : "local";
            const std::string tok = cluster::human_tokens(tokens.value("total", std::int64_t{0}));
            if (rtk) {
                std::cout << s.value("id", "") << "|" << s.value("name", "") << "|" << s.value("state", "") << "|" << s.value("backend", "") << "|"
                    << s.value("mode", "") << "|" << tok << "|" << ctx << "|" << cost << "|" << permissions << "|" << cluster::short_path(s.value("cwd", "")) << "|" << s.value("summary", std::string())
                    << "|" << s.value("category", std::string()) << "|" << join_tags_(s) << "\n";
            } else {
                std::cout << std::left << std::setw(4) << s.value("id", "") << std::setw(18) << cluster::one_line(s.value("name", ""), 17)
                    << std::setw(11) << s.value("state", "") << std::setw(22) << cluster::one_line(s.value("backend", ""), 21)
                    << std::setw(8) << tok << std::setw(6) << (std::to_string(ctx) + "%") << std::setw(9) << cost
                    << (permissions == 0 ? "" : "[" + std::to_string(permissions) + " perm] ") << cluster::short_path(s.value("cwd", ""))
                    << (s.value("summary", std::string()).empty() ? "" : "  " + s.value("summary", std::string()))
                    << (s.value("category", std::string()).empty() ? "" : "  [" + s.value("category", std::string()) + "]")
                    << (s.contains("tags") ? "  " + join_tags_(s, true) : "") << "\n";
            }
        }
        if (data.empty() && !rtk) std::cout << "no session" << "\n";
    } else if (cmd == "schedule_list") {
        for (const cluster::Json& w: data)
            std::cout << w.value("id", "") << (rtk ? "|" : "  ") << w.value("status", "") << (rtk ? "|" : "  ") << w.value("at", "") << (rtk ? "|" : "  ") << w.value("action", "")
                << (rtk ? "|" : " ") << w.value("session", "") << (w.value("every_minutes", 0) > 0 ? (rtk ? "|every " : "  every ") + std::to_string(w.value("every_minutes", 0)) + " min" : "")
                << (rtk ? "|" : "  ") << w.value("text", "") << (w.value("rule", std::string()).empty() ? "" : (rtk ? "|" : "  (") + w.value("rule", std::string()) + (rtk ? "" : ")")) << "\n";
        if (data.empty() && !rtk) std::cout << "nothing scheduled" << "\n";
    } else if (cmd == "task_list") {
        for (const cluster::Json& t: data)
            std::cout << t.value("id", "") << (rtk ? "|" : "  ") << t.value("status", "") << (rtk ? "|" : "  ") << t.value("session", "-")
                << (rtk ? "|" : "  ") << t.value("prompt", "") << "\n";
    } else if (cmd == "search") {
        for (const cluster::Json& h: data)
            std::cout << h.value("session", "") << (rtk ? "|" : " ") << h.value("name", "") << (rtk ? "|" : " ") << cluster::human_age(h.value("time", std::int64_t{0}))
                << (rtk ? "|" : ": ") << h.value("text", "") << "\n";
    } else if (cmd == "trash") {
        for (const cluster::Json& s: data)
            std::cout << s.value("id", "") << (rtk ? "|" : "  ") << s.value("name", "") << (rtk ? "|" : "  ") << cluster::human_age(s.value("deleted", std::int64_t{0}))
                << (rtk ? "|" : "  ") << cluster::short_path(s.value("cwd", "")) << (s.value("summary", std::string()).empty() ? "" : (rtk ? "|" : "  ") + s.value("summary", std::string()))
                << "\n";
    } else if (cmd == "providers") {
        for (const cluster::Json& p: data)
            std::cout << (p.value("ready", false) ? "ok" : "--") << (rtk ? "|" : "  ") << std::left << std::setw(rtk ? 0 : 15) << p.value("name", "")
                << (rtk ? "|" : " ") << std::setw(rtk ? 0 : 9) << p.value("driver", "") << (rtk ? "|" : " ") << p.value("detail", "") << "\n";
    } else if (cmd == "export" && data.contains("markdown")) {
        std::cout << data["markdown"].get<std::string>();
    } else if (cmd == "status" || cmd == "read") {
        if (rtk) {
            std::cout << data.dump(-1, ' ', false, cluster::JSON_LENIENT) << "\n";
            return;
        }
        std::cout << data.value("name", "") << " (" << data.value("id", "") << ") " << data.value("state", "") << " " << data.value("backend", "")
            << " " << cluster::short_path(data.value("cwd", "")) << "\n";
        for (const cluster::Json& m: data.value("messages", cluster::Json::array()))
            std::cout << "  [" << m.value("kind", "") << "] " << (m.value("tool", "").empty() ? "" : m.value("tool", "") + " ") << cluster::one_line(m.value("text", ""), 160) << "\n";
        for (const cluster::Json& p: data.value("permissions", cluster::Json::array()))
            std::cout << "  permission " << p.value("request_id", "") << ": " << p.value("tool", "") << " " << p.value("description", "") << "\n";
    } else if (cmd == "notices") {
        for (const cluster::Json& n: data)
            std::cout << cluster::clock_time(n.value("time", std::int64_t{0})) << " " << (n.value("error", false) ? "! " : "") << n.value("text", "") << "\n";
    } else if (!data.is_null()) {
        std::cout << (data.is_object() && data.contains("id") ? data["id"].get<std::string>() : data.dump(-1, ' ', false, cluster::JSON_LENIENT)) << "\n";
    } else if (!rtk) {
        std::cout << "ok" << "\n";
    }
    std::cout << std::flush;
}

_cold int cluster::Core::trustCa_(void)
{
    // the authority of the web access trusted by this computer (Firefox, Chrome, curl read the system store through
    // p11-kit): the password asked by its window (sudo -A, else pkexec)
    const bool remove = this->_words[1] == "untrust-ca";
    const std::filesystem::path ca = cluster::config_dir() / "web" / "ca.pem";
    if (!std::filesystem::exists(ca)) {
        std::cerr << "no authority yet: open the web access once with https (claude-cluster web on)" << std::endl;
        return 1;
    }
    if (!cluster::has_command("trust")) {
        std::cerr << "no 'trust' command (p11-kit): import " << ca.string() << " in the browser (Settings > Certificates > Authorities)" << std::endl;
        return 1;
    }
    const std::vector<std::string> trust = remove ? std::vector<std::string>{"trust", "anchor", "--remove", ca.string()}
        : std::vector<std::string>{"trust", "anchor", "--store", ca.string()};
    const std::filesystem::path askpass = cluster::expand_home("~/.local/bin/sudo-askpass");
    std::vector<std::string> command;
    if (std::filesystem::exists(askpass) && cluster::has_command("sudo")) {
        ::setenv("SUDO_ASKPASS", askpass.c_str(), 1);
        command = {"sudo", "-A"};
    } else if (cluster::has_command("pkexec")) {
        command = {"pkexec"};
    } else {
        command = {"sudo"};
    }
    command.insert(command.end(), trust.begin(), trust.end());
    const cluster::Captured done = cluster::capture(command, "", 300);
    if (done.code != 0) {
        std::cerr << "not done: " << cluster::one_line(done.out, 300) << std::endl;
        return 1;
    }
    std::cout << (remove ? "the authority of claude-cluster is trusted no more by this computer"
        : "the authority of claude-cluster is trusted by this computer: its web page opens without warning (restart the browser)") << std::endl;
    return 0;
}

_cold int cluster::Core::icon_(void)
{
    // a face named, or the one of the claude-cluster running (its state, its badge): the SVG on the output
    static const std::set<std::string> names = {"happy", "grin", "attentive", "waiting", "idle", "working"};
    const std::string name = this->_words.size() > 1 ? this->_words[1] : "";
    if (!name.empty() && !names.contains(name)) {
        std::cerr << "claude-cluster icon [happy | grin | attentive | waiting | idle | working] [badge]" << std::endl;
        return 1;
    }
    int badge = 0;
    try {
        badge = this->_words.size() > 2 ? std::stoi(this->_words[2]) : 0;
    } catch (const std::exception&) {}
    if (!name.empty()) {
        std::cout << cluster::face_svg(cluster::face_of(name), badge, false) << std::endl;
        return 0;
    }
    try {
        const cluster::Json face = cluster::Control::request({{"cmd", "face"}}).value("data", cluster::Json::object());
        std::cout << face.value("svg", std::string()) << std::endl;
    } catch (const utils::exception::IException&) {
        std::cout << cluster::face_svg(cluster::Face::Idle) << std::endl; // not running: asleep
    }
    return 0;
}

/* auth */
_cold int cluster::Core::engines_(void)
{
    // the engines of the voice: speech to text, text to speech; what each one handles, its limits, its free use, its links
    cluster::Config config;
    config.load();
    const cluster::Json engines = cluster::voice_engines_json(config);
    const bool rtk = this->_options.rtk;
    std::string kind;

    for (const cluster::Json& e: engines) {
        if (rtk) {
            std::cout << e["id"].get<std::string>() << "|" << e["kind"].get<std::string>() << "|" << (e["local"].get<bool>() ? "local" : "api")
                << "|" << (e["ready"].get<bool>() ? "ready" : "--") << (e["used"].get<bool>() ? "|used" : "") << "|" << e["state"].get<std::string>()
                << "|" << e["languages"].get<std::string>() << "|+ " << e["strengths"].get<std::string>() << "|- " << e["limits"].get<std::string>()
                << "|free: " << e["free"].get<std::string>() << "|" << e["link"].get<std::string>() << "\n";
            continue;
        }
        if (e["kind"].get<std::string>() != kind) {
            kind = e["kind"].get<std::string>();
            std::cout << (kind == "speech to text" ? "" : "\n") << "== " << kind << " (" << (kind == "speech to text" ? "[voice] listen_model" : "[voice] say_engine") << ")\n";
        }
        std::cout << (e["used"].get<bool>() ? "* " : "  ") << std::left << std::setw(11) << e["id"].get<std::string>()
            << (e["ready"].get<bool>() ? "ready  " : "--     ") << e["state"].get<std::string>() << "\n";
        for (const std::string& line: cluster::voice_engine_lines(*cluster::voice_engine(kind == "speech to text" ? "listen" : "say", e["id"].get<std::string>())))
            std::cout << "      " << line << "\n";
    }
    if (!rtk) std::cout << "\n* used   change: settings page > Voice (F9), or config.toml   a key: claude-cluster auth login <provider>\n";
    return 0;
}

_cold int cluster::Core::docs_(void)
{
    // claude-cluster docs [open [page|screen]] | search <words> | url [page|screen] | serve [port] [lan]: the documentation embedded in this binary
    if (!cluster::docs_built()) {
        std::cerr << "the documentation is not in this build (python3 3.11+ is needed to build claude-cluster with it)" << std::endl;
        return 1;
    }
    const std::string sub = this->_words.size() > 1 ? this->_words[1] : "open";
    if (sub == "serve") { // a read only server of the documentation alone (no core): 8790 by default, this computer only unless 'lan'
        int port = 8790;
        bool lan = false;
        for (std::size_t i = 2; i < this->_words.size(); ++i) {
            if (this->_words[i] == "lan") lan = true;
            else if (const std::string& word = this->_words[i]; !word.empty() && word.size() <= 5 && std::all_of(word.begin(), word.end(), ::isdigit)
                && std::stoi(word) > 0 && std::stoi(word) < 65536)
                port = std::stoi(word);
            else {
                std::cerr << "claude-cluster docs serve [port] [lan]" << std::endl;
                return 1;
            }
        }
        return cluster::docs_serve(port, lan);
    }
    const bool known = sub == "open" || sub == "search" || sub == "url";
    std::string rest;
    for (std::size_t i = known ? 2 : 1; i < this->_words.size(); ++i)
        rest += (rest.empty() ? "" : " ") + this->_words[i];
    if (sub == "search" || !known) {
        const std::string lang = std::getenv("LANG") != nullptr && std::string(std::getenv("LANG")).starts_with("fr") ? "fr" : "en";
        const cluster::Json found = cluster::docs_search(rest, lang, 8);
        for (const cluster::Json& hit: found["results"])
            std::cout << hit.value("k", std::string()) << "  " << hit.value("t", std::string()) << "  " << hit.value("u", std::string()) << "\n";
        if (found["results"].empty()) std::cout << "nothing found\n";
        return found["results"].empty() ? 1 : 0;
    }
    const std::string page = cluster::docs_page(rest);
    if (sub == "url") {
        std::cout << "file://" << (cluster::docs_extract() / page.substr(0, page.find('#'))).string() << (page.find('#') == std::string::npos ? "" : page.substr(page.find('#'))) << "\n";
        return 0;
    }
    const cluster::DocOpened opened = cluster::docs_open(rest);
    std::cout << (opened.opened ? "opened " : "no display, open it in a browser: ") << opened.url << "\n";
    return 0;
}

_cold int cluster::Core::auth_(void)
{
    cluster::Config config;
    config.load();
    cluster::Auth auth(config);
    const std::string sub = this->_words.size() > 1 ? this->_words[1] : "list";

    if (sub == "list" || sub == "status") {
        for (const cluster::Provider& p: auth.providers()) {
            const auto [ready, detail] = auth.status(p);
            if (this->_options.rtk) std::cout << (ready ? "ok" : "--") << "|" << p.name << "|" << p.driver << "|" << detail << "\n";
            else std::cout << (ready ? "ok " : "-- ") << std::left << std::setw(15) << p.name << std::setw(10) << p.driver << detail
                << (this->_words.size() > 2 || sub == "status" ? "" : "") << "\n";
        }
        if (!this->_options.rtk) std::cout << "\nbackend = <provider>[/<model>]   login: claude-cluster auth login <provider>   remove: claude-cluster auth logout <provider>\n";
        return 0;
    }
    if (this->_words.size() < 3) throw utils::exception::ErrorException(utils::exception::InternalCode::ArgumentsNumber, "auth " + sub + " <provider>");
    if (sub == "login") auth.login(this->_words[2]);
    else if (sub == "logout") auth.logout(this->_words[2]);
    else throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "auth list | login <provider> | logout <provider>");
    return 0;
}

/* machine */
_cold int cluster::Core::machine_(void)
{
    const std::string sub = this->_words.size() > 1 ? this->_words[1] : "show";
    if (sub == "path") {
        std::cout << cluster::machine_file().string() << std::endl;
        return 0;
    }
    if (sub == "show") {
        std::ifstream file(cluster::machine_file());
        if (!file) {
            std::cout << "no map of this computer yet: claude-cluster machine sync" << std::endl;
            return 1;
        }
        std::cout << file.rdbuf();
        return 0;
    }
    if (sub != "sync") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "machine sync [--yes] | show | path");
    std::cout << cluster::machine_scope() << std::endl;
    if (!this->_options.values.contains("yes")) {
        if (!::isatty(STDIN_FILENO)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "machine sync: --yes (no terminal to ask)");
        std::cout << "[y/N] " << std::flush;
        std::string answer;
        std::getline(std::cin, answer);
        if (cluster::lower(cluster::trim(answer)) != "y" && cluster::lower(cluster::trim(answer)) != "yes") {
            std::cout << "nothing scanned" << std::endl;
            return 0;
        }
    }
    // the questions about the user still empty (a terminal: asked, its suggestion kept with Enter)
    cluster::Config config;
    config.load();
    if (!this->_options.values.contains("yes") && ::isatty(STDIN_FILENO)) {
        for (const cluster::ProfileQuestion& q: cluster::machine_questions(config)) {
            std::string choices;
            for (const std::string& choice: q.choices)
                choices += (choices.empty() ? "" : ", ") + (choice.empty() ? std::string("(none)") : choice);
            std::cout << q.question << (choices.empty() ? "" : " (" + choices + ")") << " [" << q.value << "]: " << std::flush;
            std::string answer;
            std::getline(std::cin, answer);
            answer = cluster::trim(answer).empty() ? q.value : cluster::trim(answer);
            (void)cluster::settings_set(config, "user", q.key, answer);
        }
        (void)config.reload();
    }
    std::size_t shown = 0;
    const std::size_t repos = cluster::machine_scan(config, [&shown](const cluster::MachineProgress& progress) {
        // the lines of the log done since the last report (the log keeps the last 3)
        for (; shown < progress.finished && !progress.log.empty(); ++shown)
            std::cout << "✓ " << progress.log[progress.log.size() - std::min(progress.log.size(), progress.finished - shown)] << std::endl;
    });
    std::cout << "written: " << cluster::machine_file().string() << " (" << repos << " repositories)" << std::endl;
    return 0;
}

/* help */
_cold void cluster::Core::help_(void) const
{
    const bool tty = ::isatty(::fileno(stdout)) && !std::getenv("NO_COLOR");
    const std::string bold = tty ? utils::iomanip::strong() : "";
    const std::string green = tty ? utils::iomanip::color(utils::iomanip::Color::Green) : "";
    const std::string reset = tty ? utils::iomanip::reset() : "";

    std::cout << bold << "claude-cluster " << CLUSTER_VERSION << reset << " - " << this->_parser.getDescription() << "\n\n"
        << bold << "USAGE" << reset << "\n"
        << "    claude-cluster [--tty | --gui] [--backend B] [--restore | --fresh]   interface (terminal by default)\n"
        << "    claude-cluster <command> [args] [--rtk | --json]                    headless, on the running instance\n"
        << "    claude-cluster auth list | login <provider> | logout <provider>      providers and credentials\n"
        << "    claude-cluster service setup | enable | disable | status | remove     \"ok claude\" opens claude-cluster (background)\n\n"
        << bold << "COMMANDS" << reset << "\n";
    for (const cluster::Command& command: cluster::commands()) {
        const std::string usage = command.name + (command.usage.empty() ? "" : " " + command.usage);
        std::cout << "    " << green << usage << reset << std::string(usage.size() < 32 ? 32 - usage.size() : 1, ' ') << command.description << "\n";
    }
    std::cout << "\n" << bold << "FLAGS" << reset << "\n";
    for (const auto &[id, flag]: this->_parser.getFlags()) {
        const auto &[shortName, flagName, longName, env] = flag.flag;
        std::string usage = (shortName.empty() ? "    " : "-" + shortName + ", ") + "--" + longName;
        for (const auto &[name, mandatory, check]: flag.options)
            usage += " <" + name + ">";
        std::cout << "    " << green << usage << reset << std::string(usage.size() < 32 ? 32 - usage.size() : 1, ' ') << flag.description << "\n";
    }
    std::cout << "\n" << bold << "FILES" << reset << "\n"
        << "    " << cluster::short_path(cluster::config_dir().string()) << "/config.toml   settings (reloaded live), global.md: system prompt of the global session\n"
        << "    " << cluster::short_path(cluster::data_dir().string()) << "/   state.json (sessions, trash, tasks), logs/<id>.jsonl (history)\n";
}
