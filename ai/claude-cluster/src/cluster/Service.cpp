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

_cold void setup_(void)
{
    // the unit of the user (started with the graphical session: the mic, the display)
    write_script_();
    std::error_code error;
    std::filesystem::create_directories(cluster::service_unit().parent_path(), error);
    std::ofstream(cluster::service_unit())
        << "[Unit]\n"
        << "Description=claude-cluster: the wake phrase opens claude-cluster with its voice on\n"
        << "After=graphical-session.target pipewire-pulse.service\n"
        << "PartOf=graphical-session.target\n\n"
        << "[Service]\n"
        << "ExecStart=" << cluster::self_exe().string() << " service run\n"
        << "Restart=on-failure\n"
        << "RestartSec=10\n"
        << "Nice=10\n\n"
        << "[Install]\n"
        << "WantedBy=graphical-session.target\n";
    (void)systemctl_({"daemon-reload"});
    std::cout << "service written: " << cluster::short_path(cluster::service_unit().string()) << " (detector " << cluster::short_path(cluster::service_script().string()) << ")" << std::endl;
}

_cold std::vector<std::string> launcher_(const cluster::Config& config)
{
    // claude-cluster with its voice on, in a window or in a terminal (outside the unit: it lives on when the service stops)
    const cluster::ServiceConf& s = config.service;
    const std::vector<std::string> app = {cluster::self_exe().string(), s.frontend == "gui" ? "--gui" : "--tty", "--voice", s.voiceMode};
    std::string line;
    for (const std::string& word: app)
        line += (line.empty() ? "" : " ") + quote_(word);
    std::vector<std::string> command;
    if (s.frontend == "gui") {
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
    if (cluster::has_command("systemd-run")) command.insert(command.begin(), {"systemd-run", "--user", "--collect", "--quiet", "--"});
    return command;
}

_cold void launch_(const cluster::Config& config)
{
    const std::vector<std::string> command = launcher_(config);
    std::cerr << "service: wake phrase heard, opening claude-cluster (" << command[command.size() > 5 ? 5 : 0] << ")" << std::endl;
    const cluster::Captured out = cluster::capture(command, "", 20);
    if (out.code != 0) std::cerr << "service: the launch failed: " << cluster::one_line(out.out, 300) << std::endl;
    // started when its control socket answers (claude-cluster: one instance)
    for (int i = 0; i < 40 && !stopping_ && !cluster::Control::alive(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
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

        void start(const cluster::Config& config, const bool verbose)
        {
            const std::string python = cluster::expand_home(config.service.python).string();
            std::vector<std::string> args = {cluster::service_script().string(), "--listen", which_(config.voice.listenCommand)};
            const std::string phrases = config.service.wakeWord.empty() ? config.voice.wakeWord : config.service.wakeWord;
            for (const std::string& phrase: cluster::split(phrases, ','))
                args.insert(args.end(), {"--phrase", phrase});
            if (!config.service.source.empty()) args.insert(args.end(), {"--source", config.service.source});
            if (verbose) args.push_back("--verbose"); // xstyle: ignore LU-VERBOSE (a flag of wake.py, not a log level)
            this->_out = std::make_unique<utils::encapsulation::Pipe>();
            this->_out->trigger();
            for (const int fd: {this->_out->getRead(), this->_out->getWrite()})
                ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC);
            this->_process = std::make_unique<utils::encapsulation::Process>();
            this->_process->dup(this->_out->getWrite(), STDOUT_FILENO);
            this->_pid = this->_process->spawn(python, args);
            this->_out->closeWrite();
            this->_poll = std::make_unique<utils::encapsulation::Poll>();
            this->_poll->link(this->_out->getRead(), EPOLLIN);
        }

        void stop(void)
        {
            // the microphone is released at once (the instance opened listens itself)
            if (this->_pid <= 0) return;
            ::kill(this->_pid, SIGTERM);
            (void)this->_process->wait();
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
                    this->stop();
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

_cold int run_(const bool verbose)
{
    // while an instance runs (opened by the service or by hand: claude-cluster is one instance) the detector is off
    std::signal(SIGTERM, on_signal_);
    std::signal(SIGINT, on_signal_);
    cluster::Config config;
    config.load();
    write_script_();
    Detector detector;
    int failures = 0;
    std::cerr << "service: listening for \"" << (config.service.wakeWord.empty() ? config.voice.wakeWord : config.service.wakeWord) << "\"" << std::endl;
    while (!stopping_) {
        (void)config.reload();
        if (cluster::Control::alive()) {
            detector.stop();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        if (!detector.running()) {
            if (failures > 0) std::this_thread::sleep_for(std::chrono::seconds(std::min(60, 5 * failures))); // a broken detector: slower
            try {
                detector.start(config, verbose);
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
    return 0;
}

} // namespace

_cold int cluster::service_command(const std::vector<std::string>& words, const bool verbose)
{
    const std::string sub = words.size() > 1 ? words[1] : "status";
    if (sub == "run") return run_(verbose);
    if (sub == "setup") {
        setup_();
        std::cout << "next: claude-cluster service enable (started now and with the session)" << std::endl;
        return 0;
    }
    if (sub == "enable") {
        setup_();
        return systemctl_({"enable", "--now", UNIT});
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
            << ", " << (enabled.empty() ? "disabled" : enabled) << "\ninstance: " << (cluster::Control::alive() ? "running (the detector waits)" : "none") << std::endl;
        return 0;
    }
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument,
        "service setup | enable | disable | start | stop | restart | status | remove | run [--verbose]");
}
