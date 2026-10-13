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
#include "cluster/Settings.hpp"
#include "cluster/Service.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Icons.hpp"
#include "cluster/Core.hpp"
#include "cluster/Mcp.hpp"
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <csignal>
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
    this->web.reset();      // before the voice: a request of the page may still use it
    this->voice.reset();
    if (this->control) this->control->stop();
    if (this->manager) this->manager->shutdown();
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
    const std::function<std::optional<std::string>(const std::string&)> layouts = [](const std::string& v) {return choice_(v, {"list", "grid", "tabs"});};
    const std::function<std::optional<std::string>(const std::string&)> shells = [](const std::string& v) {return choice_(v, {"bash", "zsh", "fish"});};

    /* front-end */
    this->_parser.setFlag("tty", {"t", "", "tty", ""}, {}, "Terminal interface (fails when stdin / stdout are not a terminal)");
    this->_parser.setFlag("gui", {"g", "", "gui", ""}, {}, "Window interface (fails when it is not built or there is no display)");
    this->_parser.setFlag("backend", {"b", "", "backend", "CLAUDE_CLUSTER_BACKEND"}, {{"provider[/model]", true, any}},
        "Backend of the new sessions: claude (default), ollama/<model>, openai/<model>, opencode/<p>/<m>, codex... (auth list)");
    this->_parser.setFlag("restore", {"", "", "restore", ""}, {}, "Restore the sessions of the previous run without asking");
    this->_parser.setFlag("fresh", {"", "", "fresh", ""}, {}, "Start clean: the previous sessions go to the trash");
    this->_parser.setFlag("noglobal", {"", "", "no-global", ""}, {}, "No global session for this run");
    this->_parser.setFlag("layout", {"", "", "layout", ""}, {{"layout", true, layouts}}, "list | grid | tabs");
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
    this->_parser.setFlag("every", {"", "", "every", ""}, {{"minutes", true, any}}, "schedule add: repeated every N minutes");
    this->_parser.setFlag("notify", {"", "", "notify", ""}, {}, "schedule add: a desktop notification (not a prompt)");

    /* headless */
    this->_parser.setFlag("name", {"n", "", "name", ""}, {{"name", true, any}}, "spawn: name of the session");
    this->_parser.setFlag("mode", {"m", "", "mode", ""}, {{"mode", true, modes}}, "spawn: permission mode");
    this->_parser.setFlag("profile", {"p", "", "profile", ""}, {{"name", true, any}}, "spawn: profile of the config");
    this->_parser.setFlag("prompt", {"", "", "prompt", ""}, {{"text", true, any}}, "spawn: first prompt");
    this->_parser.setFlag("target", {"", "", "target", ""}, {{"session|folder", true, any}}, "task add: session or folder");
    this->_parser.setFlag("after", {"", "", "after", ""}, {{"t1,t2", true, any}}, "task add: tasks to finish first");
    this->_parser.setFlag("force", {"f", "", "force", ""}, {}, "send: ignore the budget");
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
        "prompt", "target", "after", "completion", "voice", "every"};
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
    else if (cmd == "__complete") this->complete_(this->_words.size() > 1 ? this->_words[1] : "");
    else if (cmd == "serve") this->_exit = this->ui_(true);
    else if (cmd == "auth") this->_exit = this->auth_();
    else if (cmd == "engines") this->_exit = this->engines_();
    else if (cmd == "icon") this->_exit = this->icon_();
    else if (cmd == "web" && this->_words.size() > 1 && (this->_words[1] == "trust-ca" || this->_words[1] == "untrust-ca")) this->_exit = this->trustCa_();
    else if (cmd == "machine") this->_exit = this->machine_();
    else if (cmd == "service") this->_exit = cluster::service_command(this->_words, {this->_options.values.contains("verbose"),
        this->_options.values.contains("debug"), this->_options.values.contains("follow")});
    else if (cmd == "help") this->help_();
    else this->_exit = this->headless_();
}

_cold int cluster::Core::ui_(const bool serve)
{
    cluster::App app;
    app.options = this->_options;
    if (this->_options.values.contains("voice")) app.config.voiceForce = this->_options.values.at("voice");
    app.config.load();
    if (!app.config.error().empty()) std::cerr << "claude-cluster: config error (defaults used): " << app.config.error() << std::endl;
    if (!this->_options.backend.empty()) app.config.backend = this->_options.backend;
    if (!this->_options.global) app.config.globalEnabled = false;
    (void)app.config.currentStyle();
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
        if (tty_()) frontend = "tty";
        else if (gui && display_()) frontend = "gui";
        else throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no terminal and no window available");
    }

    // Core: one instance per user (control socket), previous sessions, global session
    app.manager = std::make_unique<cluster::Manager>(app.config);
    app.control = std::make_unique<cluster::Control>(*app.manager);
    app.control->start();
    app.manager->load();
    if (!this->_options.layout.empty()) app.manager->setUi("layout", this->_options.layout);
    if (!this->_options.style.empty()) app.manager->setUi("style", this->_options.style);
    const std::string restore = this->_options.restore.empty() ? app.config.restore : this->_options.restore;
    if (app.manager->restorable()) {
        if (restore == "always") app.manager->restorePrevious(true);
        else if (restore == "never") app.manager->restorePrevious(false);
        else if (serve) app.manager->restorePrevious(true); // nobody to ask
        else app.askRestore = true;
    }
    if (!app.askRestore) app.manager->startGlobal(); // asked: started by the answer (resumed or new)
    app.askMachine = serve ? 0 : this->_options.values.contains("rescan") ? 3 : app.manager->machineAsk(); // 3: forced (--rescan)
    app.voice = std::make_unique<cluster::Voice>(*app.manager);
    app.web = std::make_unique<cluster::Web>(*app.manager, app.voice.get(), this->_options.values.contains("web") ? std::optional<bool>(true) : std::nullopt);
    app.manager->setWeb(app.web.get());
    app.start();

    int code = 0;
    cluster::catch_quit_signals(); // Ctrl+C, kill, the terminal closed: a normal quit (sessions saved, terminal restored)
    if (serve) {
        // Headless instance: the commands and the MCP server drive it, until a quit signal
        std::cerr << "claude-cluster: serving on " << cluster::socket_path().string() << " (Ctrl+C to stop)" << std::endl;
        while (!cluster::quit_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        app.stop();
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
    app.stop();
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

    if (cmd == "list" || cmd == "trash" || cmd == "notices" || cmd == "providers") {
    } else if (cmd == "web") {
        request["action"] = w.size() > 1 ? w[1] : "";
        if (request["action"] == "approve") request.update({{"id", word(2, "id")}, {"answer", w.size() > 3 ? w[3] : "allow"}});
        if (request["action"] == "kick" || request["action"] == "untrust") request["ids"] = w.size() > 2 ? w[2] : "";
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
        if (sub == "add") request.update({{"when", word(2, "when")}, {"text", rest(3)}, {"session", value("target")},
            {"action", this->_options.values.contains("notify") ? "notify" : "prompt"}, {"every", value("every").empty() ? 0 : std::stoi(value("every"))}});
        else if (sub == "cancel") request["id"] = word(2, "schedule id");
        else if (sub != "list") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "schedule " + sub + ": add | list | cancel");
    } else if (cmd == "task") {
        const std::string sub = word(1, "add|list|cancel");
        shown = "task_" + sub;
        request["cmd"] = shown;
        if (sub == "add") request.update({{"prompt", rest(2)}, {"target", value("target")}, {"after", cluster::split(value("after"), ',')}});
        else if (sub == "cancel") request["id"] = word(2, "task id");
        else if (sub != "list") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "task " + sub + ": add | list | cancel");
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

_cold void cluster::Core::print_(const std::string& cmd, const cluster::Json& data) const
{
    if (this->_options.json) {
        std::cout << data.dump(2) << std::endl;
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
        if (!data.value("on", false)) {
            std::cout << "web access closed" << (data.value("error", std::string()).empty() ? "" : " (" + data.value("error", std::string()) + ")")
                << (action == "off" ? "" : "   open it: claude-cluster web on") << "\n";
            return;
        }
        if (action == "kick") std::cout << (data.value("kicked", false) ? "disconnected" : "nobody connected") << "\n";
        if (action == "pin") std::cout << "a new code" << "\n";
        if (action == "approve") std::cout << "answered" << "\n";
        if (action == "untrust") std::cout << "trusted no more" << "\n";
        std::cout << "web access open   " << data.value("url", std::string()) << "   code " << data.value("pin", std::string()) << "\n";
        // the devices: waiting for you, connected, trusted
        const std::function<void(const cluster::Json&, const std::string&)> devices = [](const cluster::Json& list, const std::string& title) {
            if (list.empty()) return;
            std::cout << title << "\n";
            for (const cluster::Json& d: list)
                std::cout << "  " << d.value("id", std::string()) << "  " << d.value("state", std::string()) << (d.value("trusted", false) && d.value("state", std::string()) != "trusted"
                    ? " (trusted)" : "") << "  " << d.value("name", std::string()) << "  " << d.value("address", std::string()) << "\n";
        };
        devices(data.value("devices", cluster::Json::array()), "devices:");
        devices(data.value("trusted", cluster::Json::array()), "trusted devices (no code, no approval):");
        if (data.value("devices", cluster::Json::array()).empty()) std::cout << "nobody connected: the code, then your approval here" << "\n";
        std::cout << "approve: claude-cluster web approve <id> allow|trust|deny · out: web kick [ids|all] · trust removed: web untrust [ids|all]" << "\n";
        if (data.value("https", false))
            std::cout << "certificate SHA-256 " << data.value("fingerprint", std::string()) << " (the browser asks to accept it once: compare)" << "\n";
        else
            std::cout << "plain http: no microphone in the page ([web] https = true, then claude-cluster web off / on)" << "\n";
        return;
    }
    if (cmd == "list") {
        if (rtk) std::cout << "id|name|state|backend|mode|tok_total|ctx%|cost|perm|cwd" << "\n";
        for (const cluster::Json& s: data) {
            const std::int64_t window = std::max<std::int64_t>(1, s["context"].value("window", std::int64_t{1}));
            const std::int64_t ctx = 100 * s["context"].value("used", std::int64_t{0}) / window;
            const std::string cost = s["cost_usd"].is_number() ? cluster::human_cost(s["cost_usd"].get<double>()) : "local";
            const std::string tok = cluster::human_tokens(s["tokens"].value("total", std::int64_t{0}));
            if (rtk) {
                std::cout << s.value("id", "") << "|" << s.value("name", "") << "|" << s.value("state", "") << "|" << s.value("backend", "") << "|"
                    << s.value("mode", "") << "|" << tok << "|" << ctx << "|" << cost << "|" << s["permissions"].size() << "|" << cluster::short_path(s.value("cwd", "")) << "\n";
            } else {
                std::cout << std::left << std::setw(4) << s.value("id", "") << std::setw(18) << cluster::one_line(s.value("name", ""), 17)
                    << std::setw(11) << s.value("state", "") << std::setw(22) << cluster::one_line(s.value("backend", ""), 21)
                    << std::setw(8) << tok << std::setw(6) << (std::to_string(ctx) + "%") << std::setw(9) << cost
                    << (s["permissions"].empty() ? "" : "[" + std::to_string(s["permissions"].size()) + " perm] ") << cluster::short_path(s.value("cwd", "")) << "\n";
            }
        }
        if (data.empty() && !rtk) std::cout << "no session" << "\n";
    } else if (cmd == "schedule_list") {
        for (const cluster::Json& w: data)
            std::cout << w.value("id", "") << (rtk ? "|" : "  ") << w.value("at", "") << (rtk ? "|" : "  ") << w.value("action", "")
                << (rtk ? "|" : " ") << w.value("session", "") << (w.value("every_minutes", 0) > 0 ? (rtk ? "|every " : "  every ") + std::to_string(w.value("every_minutes", 0)) + " min" : "")
                << (rtk ? "|" : "  ") << w.value("text", "") << "\n";
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
                << (rtk ? "|" : "  ") << cluster::short_path(s.value("cwd", "")) << "\n";
    } else if (cmd == "providers") {
        for (const cluster::Json& p: data)
            std::cout << (p.value("ready", false) ? "ok" : "--") << (rtk ? "|" : "  ") << std::left << std::setw(rtk ? 0 : 15) << p.value("name", "")
                << (rtk ? "|" : " ") << std::setw(rtk ? 0 : 9) << p.value("driver", "") << (rtk ? "|" : " ") << p.value("detail", "") << "\n";
    } else if (cmd == "export" && data.contains("markdown")) {
        std::cout << data["markdown"].get<std::string>();
    } else if (cmd == "status" || cmd == "read") {
        if (rtk) {
            std::cout << data.dump() << "\n";
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
        std::cout << (data.is_object() && data.contains("id") ? data["id"].get<std::string>() : data.dump()) << "\n";
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
