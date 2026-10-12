/**************************************************************\
Edition:
##  @date 11/10/2026 by @author Tsukini

File Name:
##  @file Service.cpp

File Description:
##  Background service: the systemd --user unit (setup, enable,
##  remove...) and its loop: the wake word detector (wake.py, the
##  recognition only on loud sounds) while no claude-cluster runs,
##  then claude-cluster opened with its voice on
\**************************************************************/

/* include */
#define _Attribute
#define _Exception
#define _Encapsulation
#include <utils/utils.hpp>
#include "cluster/Templates.hpp"
#include "cluster/Service.hpp"
#include "cluster/Control.hpp"
#include "cluster/Config.hpp"
#include "cluster/Tools.hpp"
#include <sys/epoll.h>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <csignal>
#include <fstream>
#include <fcntl.h>
#include <chrono>
#include <memory>
#include <thread>
#include <array>

namespace {

constexpr const char* UNIT = "claude-cluster.service";
volatile std::sig_atomic_t stopping_ = 0;

_cold void on_signal_(int)
{
    stopping_ = 1;
}

_cold std::string which_(const std::string& name)
{
    // a command of the PATH -> its file (voice-listen: its model paths are read by wake.py)
    if (name.find('/') != std::string::npos) return cluster::expand_home(name).string();
    const char* path = std::getenv("PATH");
    for (const std::string& dir: cluster::split(path ? path : "", ':'))
        if (::access((std::filesystem::path(dir) / name).c_str(), X_OK) == 0) return (std::filesystem::path(dir) / name).string();
    const std::filesystem::path local = cluster::expand_home("~/.local/bin") / name; // systemd: a short PATH
    return ::access(local.c_str(), X_OK) == 0 ? local.string() : name;
}

_cold std::string quote_(const std::string& word)
{
    std::string out = "'";
    for (const char c: word)
        out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
}

_cold void write_script_(void)
{
    // the detector shipped in the binary: written again at every start (same version as claude-cluster)
    std::error_code error;
    std::filesystem::create_directories(cluster::data_dir(), error);
    std::ofstream(cluster::service_script()) << cluster::templates::WAKE_SCRIPT;
}

_cold int systemctl_(const std::vector<std::string>& args)
{
    std::vector<std::string> command = {"systemctl", "--user"};
    command.insert(command.end(), args.begin(), args.end());
    const cluster::Captured out = cluster::capture(command, "", 30);
    if (!cluster::trim(out.out).empty()) std::cout << cluster::trim(out.out) << std::endl;
    return out.code;
}

_cold void setup_(const bool debug)
{
    // the unit of the user (started with the graphical session: the mic, the display); debug: the text heard in its journal
    write_script_();
    std::error_code error;
    std::filesystem::create_directories(cluster::service_unit().parent_path(), error);
    std::ofstream(cluster::service_unit())
        << "[Unit]\n"
        << "Description=claude-cluster: the wake phrase opens claude-cluster with its voice on\n"
        << "After=graphical-session.target pipewire-pulse.service\n"
        << "PartOf=graphical-session.target\n\n"
        << "[Service]\n"
        << "ExecStart=" << cluster::self_exe().string() << " service run" << (debug ? " --debug" : "") << "\n"
        << "Environment=PYTHONUNBUFFERED=1\n"
        << "Restart=on-failure\n"
        << "RestartSec=10\n"
        << "Nice=10\n\n"
        << "[Install]\n"
        << "WantedBy=graphical-session.target\n";
    (void)systemctl_({"daemon-reload"});
    std::cout << "service written: " << cluster::short_path(cluster::service_unit().string()) << " (detector " << cluster::short_path(cluster::service_script().string()) << ")"
        << (debug ? ", debug: the text heard goes to its journal (claude-cluster service logs; setup again without --debug to stop it)" : "") << std::endl;
}

_cold bool debug_unit_(void)
{
    // the unit written with --debug
    std::ifstream in(cluster::service_unit());
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text.find("service run --debug") != std::string::npos;
}

_cold std::string join_(const std::vector<std::string>& words)
{
    std::string line;
    for (const std::string& word: words)
        line += (line.empty() ? "" : " ") + quote_(word);
    return line;
}

_cold std::vector<std::string> launcher_(const cluster::Config& config, const std::string& unit, const bool gui)
{
    // claude-cluster with its voice on, in a window or in a terminal (outside the unit: it lives on when the service stops)
    const cluster::ServiceConf& s = config.service;
    const std::vector<std::string> app = {cluster::self_exe().string(), gui ? "--gui" : "--tty", "--voice", s.voiceMode};
    const std::string line = join_(app);
    std::vector<std::string> command;
    if (gui) {
        command = app;
    } else if (!cluster::trim(s.terminal).empty()) {
        command = cluster::split(cluster::trim(s.terminal), ' ');
        command.insert(command.end(), app.begin(), app.end());
    } else {
        // the first terminal installed, with its own way to run a command
        const std::vector<std::pair<std::string, std::vector<std::string>>> terminals = {
            {"ptyxis", {"ptyxis", "--new-window", "-x", line}}, {"kgx", {"kgx", "-e", line}}, {"gnome-terminal", {"gnome-terminal", "--"}},
            {"konsole", {"konsole", "-e"}}, {"kitty", {"kitty"}}, {"alacritty", {"alacritty", "-e"}}, {"wezterm", {"wezterm", "start", "--"}},
            {"foot", {"foot"}}, {"xterm", {"xterm", "-e"}}};
        for (const auto &[name, prefix]: terminals) {
            if (!cluster::has_command(name)) continue;
            command = prefix;
            if (name != "ptyxis" && name != "kgx") command.insert(command.end(), app.begin(), app.end());
            break;
        }
        if (command.empty())
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "service: no terminal found ([service] terminal)");
    }
    if (cluster::has_command("systemd-run")) command.insert(command.begin(), {"systemd-run", "--user", "--collect", "--quiet", "--unit=" + unit, "--"});
    return command;
}

_cold std::string attempt_(const cluster::Config& config, const bool gui)
{
    // claude-cluster in a unit of its own (its output: journalctl --user -u <unit>); started when its control socket
    // answers (""), failed when its unit fails or nothing answers in 20 s (why)
    static int launches = 0;
    const std::string unit = "claude-cluster-app-" + std::to_string(cluster::now()) + "-" + std::to_string(++launches) + ".service";
    std::vector<std::string> command;
    try {
        command = launcher_(config, unit, gui);
    } catch (const utils::exception::IException& e) {
        return e.info();
    }
    const bool transient = command.front() == "systemd-run";
    std::cerr << "service: opening claude-cluster in " << (gui ? "a window" : "a terminal") << ": " << join_(command) << std::endl;
    const cluster::Captured out = cluster::capture(command, "", 20);
    if (out.code != 0) return "the launch failed (" + (out.code < 0 ? std::string("killed / timed out") : "exit " + std::to_string(out.code)) + ")"
        + (cluster::trim(out.out).empty() ? std::string() : ": " + cluster::one_line(out.out, 400));
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (int i = 0; i < 40 && !stopping_; ++i) {
        if (cluster::Control::alive()) {
            std::cerr << "service: claude-cluster started in " << (gui ? "a window" : "a terminal") << " ("
                << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
                << " ms" << (transient ? ", its output: journalctl --user -u " + unit : std::string()) << ")" << std::endl;
            return "";
        }
        // its unit failed (--collect unloads it at once: its journal tells it)
        const std::string journal = transient && i % 4 == 3 ? cluster::capture({"journalctl", "--user", "-u", unit, "-n", "20", "--no-pager", "-o", "cat"}, "", 10).out : "";
        if (journal.find("Failed with result") != std::string::npos) {
            std::cerr << "service: claude-cluster failed at its start (unit " << unit << "), its last lines:\n" << journal << std::flush;
            return "failed at its start (journalctl --user -u " + unit + ")";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return stopping_ ? "the service stopped" : "no answer on its control socket in 20 s" + (transient ? std::string(" (journalctl --user -u ") + unit + ")" : std::string());
}

_cold void launch_(const cluster::Config& config)
{
    // [service] frontend: auto = the window when it can open (built with it, a display), else / on its failure a
    // terminal; gui = the window, a terminal on its failure; tty = a terminal only; every failure logged
    const std::string frontend = config.service.frontend;
    std::vector<bool> tries = {false};
    if (frontend == "gui" || frontend == "auto") {
#ifdef CLUSTER_GUI
        const bool display = std::getenv("WAYLAND_DISPLAY") || std::getenv("DISPLAY");
        if (display) tries = {true, false};
        else std::cerr << "service: no window: no WAYLAND_DISPLAY / DISPLAY in the environment of the service (systemctl --user import-environment)" << std::endl;
#else
        std::cerr << "service: no window: this claude-cluster is built without its window (Qt6), a terminal instead" << std::endl;
#endif
    }
    std::cerr << "service: wake phrase heard" << std::endl;
    std::string failures;
    for (const bool gui: tries) {
        const std::string why = attempt_(config, gui);
        if (why.empty() || stopping_) return;
        std::cerr << "service: " << (gui ? "the window" : "the terminal") << " did not open: " << why << (gui ? ", trying a terminal" : "") << std::endl;
        failures += std::string(failures.empty() ? "" : "; ") + (gui ? "window: " : "terminal: ") + why;
    }
    std::cerr << "service: FAILED to open claude-cluster (" << failures << ")" << std::endl;
}

// The detector: wake.py, its stdout read here (one line per wake event)
class Detector {
    private:
        std::unique_ptr<utils::encapsulation::Process> _process;
        std::unique_ptr<utils::encapsulation::Pipe> _out;
        std::unique_ptr<utils::encapsulation::Poll> _poll;
        pid_t _pid = -1;
        std::string _buffer;

    public:
        _nodiscard inline bool running(void) const {return this->_pid > 0;};

        void start(const cluster::Config& config, const bool verbose, const bool debug)
        {
            const std::string python = cluster::expand_home(config.service.python).string();
            std::vector<std::string> args = {cluster::service_script().string(), "--listen", which_(config.voice.listenCommand)};
            const std::string phrases = config.service.wakeWord.empty() ? config.voice.wakeWord : config.service.wakeWord;
            for (const std::string& phrase: cluster::split(phrases, ','))
                args.insert(args.end(), {"--phrase", phrase});
            if (!config.service.source.empty()) args.insert(args.end(), {"--source", config.service.source});
            if (verbose) args.push_back("--verbose"); // xstyle: ignore LU-VERBOSE (a flag of wake.py, not a log level)
            if (debug) args.push_back("--debug");
            this->_out = std::make_unique<utils::encapsulation::Pipe>();
            this->_out->trigger();
            for (const int fd: {this->_out->getRead(), this->_out->getWrite()})
                ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC);
            this->_process = std::make_unique<utils::encapsulation::Process>();
            this->_process->dup(this->_out->getWrite(), STDOUT_FILENO);
            this->_pid = this->_process->spawn(python, args);
            this->_out->closeWrite();
            std::cerr << "service: detector started (pid " << this->_pid << ", " << python << ")" << std::endl;
            this->_poll = std::make_unique<utils::encapsulation::Poll>();
            this->_poll->link(this->_out->getRead(), EPOLLIN);
        }

        void stop(const bool ended = false)
        {
            // the microphone is released at once (the instance opened listens itself); ended: it stopped by itself
            if (this->_pid <= 0) return;
            if (!ended) ::kill(this->_pid, SIGTERM);
            const utils::encapsulation::Status status = this->_process->wait();
            if (ended) std::cerr << "service: the detector ended by itself (" << (status.exited ? "exit " + std::to_string(status.code) : "signal " + std::to_string(status.sig))
                << "), its messages above" << std::endl;
            this->_pid = -1;
            std::fill(this->_buffer.begin(), this->_buffer.end(), '\0');
            this->_buffer.clear();
            this->_poll.reset();
            this->_out.reset();
            this->_process.reset();
        }

        // true: the wake phrase was heard; false: nothing within the time; the end of the detector stops it
        _nodiscard bool wait(const int ms)
        {
            std::array<char, 256> chunk{};
            for (const epoll_event& event: this->_poll->wait(ms)) {
                const ssize_t size = ::read(event.data.fd, chunk.data(), chunk.size());
                if (size <= 0) {
                    this->stop(true);
                    return false;
                }
                this->_buffer.append(chunk.data(), static_cast<std::size_t>(size));
                std::fill(chunk.begin(), chunk.end(), '\0');
            }
            // {"wake": true} lines only: nothing heard is kept (the buffer zeroed)
            const bool wake = this->_buffer.find("\"wake\"") != std::string::npos;
            if (this->_buffer.find('\n') != std::string::npos) {
                std::fill(this->_buffer.begin(), this->_buffer.end(), '\0');
                this->_buffer.clear();
            }
            return wake;
        }

        ~Detector()
        {
            this->stop();
        }
};

_cold int run_(const bool verbose, const bool debug)
{
    // while an instance runs (opened by the service or by hand: claude-cluster is one instance) the detector is off
    std::signal(SIGTERM, on_signal_);
    std::signal(SIGINT, on_signal_);
    cluster::Config config;
    config.load();
    write_script_();
    Detector detector;
    int failures = 0;
    bool instance = false;
    std::cerr << "service: started (" << cluster::self_exe().string() << "), wake phrase \"" << (config.service.wakeWord.empty() ? config.voice.wakeWord : config.service.wakeWord)
        << "\", opens " << (config.service.frontend == "tty" ? "a terminal" : "a window, else a terminal") << " with the voice " << config.service.voiceMode
        << (debug ? ", DEBUG: the text heard is written here" : "") << std::endl;
    while (!stopping_) {
        if (config.reload()) std::cerr << "service: config reloaded" << std::endl;
        if (cluster::Control::alive()) {
            if (!instance) std::cerr << "service: an instance of claude-cluster runs, the detector paused (the microphone free)" << std::endl;
            instance = true;
            detector.stop();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        if (instance) std::cerr << "service: no instance any more, listening again" << std::endl;
        instance = false;
        if (!detector.running()) {
            if (failures > 0) {
                const int delay = std::min(60, 5 * failures);
                std::cerr << "service: detector failed " << failures << " time(s), next try in " << delay << " s" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(delay)); // a broken detector: slower
            }
            try {
                detector.start(config, verbose, debug);
            } catch (const utils::exception::IException& e) {
                std::cerr << "service: detector not started: " << e.info() << std::endl;
                ++failures;
                continue;
            }
        }
        if (detector.wait(1000)) {
            failures = 0;
            detector.stop();
            try {
                launch_(config);
            } catch (const utils::exception::IException& e) {
                std::cerr << "service: " << e.info() << std::endl;
            }
        } else if (!detector.running()) {
            ++failures;
        }
    }
    detector.stop();
    std::cerr << "service: stopped" << std::endl;
    return 0;
}

} // namespace

_cold int cluster::service_command(const std::vector<std::string>& words, const cluster::ServiceOptions& options)
{
    const std::string sub = words.size() > 1 ? words[1] : "status";
    if (sub == "run") return run_(options.verbose, options.debug);
    if (sub == "setup") {
        setup_(options.debug);
        std::cout << "next: claude-cluster service enable (started now and with the session), or restart when it runs" << std::endl;
        return 0;
    }
    if (sub == "enable") {
        setup_(options.debug);
        const int code = systemctl_({"enable", "--now", UNIT});
        return code != 0 ? code : systemctl_({"restart", UNIT}); // a running one takes the new unit (debug or not)
    }
    if (sub == "logs") {
        // its journal: what it heard (debug), the detector, the launches and their failures
        std::vector<std::string> command = {"journalctl", "--user", "-u", UNIT, "--no-pager", "-n", "200", "-o", "short-iso"};
        if (options.follow) command.push_back("--follow");
        std::vector<char*> argv;
        for (std::string& word: command)
            argv.push_back(word.data());
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "journalctl not found (the logs: systemd)");
    }
    if (sub == "disable") return systemctl_({"disable", "--now", UNIT});
    if (sub == "start" || sub == "stop" || sub == "restart") return systemctl_({sub, UNIT});
    if (sub == "remove") {
        (void)systemctl_({"disable", "--now", UNIT});
        std::error_code error;
        std::filesystem::remove(cluster::service_unit(), error);
        std::filesystem::remove(cluster::service_script(), error);
        (void)systemctl_({"daemon-reload"});
        std::cout << "service removed" << std::endl;
        return 0;
    }
    if (sub == "status") {
        const bool installed = std::filesystem::exists(cluster::service_unit());
        const std::string active = cluster::trim(cluster::capture({"systemctl", "--user", "is-active", UNIT}, "", 10).out);
        const std::string enabled = cluster::trim(cluster::capture({"systemctl", "--user", "is-enabled", UNIT}, "", 10).out);
        std::cout << "service: " << (installed ? "installed" : "not installed (claude-cluster service setup)") << ", " << (active.empty() ? "inactive" : active)
            << ", " << (enabled.empty() ? "disabled" : enabled) << (installed && debug_unit_() ? ", DEBUG (the text heard in its journal)" : "")
            << "\ninstance: " << (cluster::Control::alive() ? "running (the detector waits)" : "none") << "\nlogs: claude-cluster service logs [--follow]" << std::endl;
        return 0;
    }
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument,
        "service setup | enable | disable | start | stop | restart | status | remove | logs [--follow] | run [--verbose] (setup / enable / run: --debug)");
}
