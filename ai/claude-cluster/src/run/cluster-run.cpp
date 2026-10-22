/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file cluster-run.cpp

File Description:
##  cluster-run [--class c] [--prio n] [--name x] [--cwd d] -- cmd...
##  The wrapper of a heavy command: asks the resource orchestrator
##  of claude-cluster for a slot over its control socket, waits
##  for it (a "queued" line on stderr), then runs the command in
##  the low profile with its output and exit code passed through
\**************************************************************/

#include "cluster/RunProfile.hpp"
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <functional>
#include <algorithm>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <poll.h>
#include <cstdio>
#include <cerrno>
#include <chrono>
#include <string>
#include <vector>

extern char** environ;

using Json = nlohmann::json;

/* tools */
namespace {

volatile sig_atomic_t g_signal = 0;     // the last signal received (forwarded to the command)
bool g_quiet = false;

void on_signal_(const int signal)
{
    g_signal = signal;
}

void say_(const std::string& text) // xstyle: ignore CPP-SINGLE-STATEMENT (helper of this file)
{
    if (!g_quiet) (void)!::write(STDERR_FILENO, ("cluster-run: " + text + "\n").c_str(), text.size() + 14);
}

void warn_(const std::string& text) // xstyle: ignore CPP-SINGLE-STATEMENT (helper of this file)
{
    (void)!::write(STDERR_FILENO, ("cluster-run: " + text + "\n").c_str(), text.size() + 14);
}

std::string socket_path_(void)
{
    for (const char* name: {"CLUSTER_RUN_SOCKET", "CLAUDE_CLUSTER_SOCKET"}) {
        const char* env = std::getenv(name);
        if (env != nullptr && *env != '\0') return env;
    }
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg != nullptr && *xdg != '\0') return std::string(xdg) + "/claude-cluster/control.sock";
    return "/tmp/claude-cluster-" + std::to_string(::getuid()) + "/control.sock";
}

int connect_(const std::string& path)
{
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); // xstyle: ignore LU-SOCKET (standalone binary, libutils not linked)
    if (fd == -1) return -1;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        ::close(fd);
        return -1;
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1) {
        ::close(fd);
        return -1;
    }
    return fd;
}

bool send_(const int fd, const Json& message)
{
    const std::string line = message.dump(-1, ' ', false, Json::error_handler_t::replace) + "\n";
    std::size_t sent = 0;
    while (sent < line.size()) {
        const ssize_t size = ::send(fd, line.data() + sent, line.size() - sent, MSG_NOSIGNAL);
        if (size <= 0) return false;
        sent += static_cast<std::size_t>(size);
    }
    return true;
}

// one line of the core: 1 read, 0 timeout, -1 closed
int read_line_(const int fd, std::string& buffer, Json& message, const int timeout)
{
    for (;;) {
        const std::size_t nl = buffer.find('\n');
        if (nl != std::string::npos) {
            try {
                message = Json::parse(buffer.substr(0, nl));
            } catch (const Json::exception&) {
                message = Json::object();
            }
            buffer.erase(0, nl + 1);
            return 1;
        }
        pollfd poller{fd, POLLIN, 0};
        const int ready = ::poll(&poller, 1, timeout);
        if (ready <= 0) return 0; // a timeout, or a signal (the caller looks at g_signal)
        char chunk[4096];
        const ssize_t size = ::recv(fd, chunk, sizeof(chunk), 0);
        if (size <= 0) return -1;
        buffer.append(chunk, static_cast<std::size_t>(size));
    }
}

std::string join_(const std::vector<std::string>& words)
{
    std::string out;
    for (const std::string& word: words)
        out += (out.empty() ? "" : " ") + word;
    return out;
}

void usage_(void)
{
    warn_("usage: cluster-run [--class build|tests|container|gpu|custom] [--prio N] [--name X] [--cwd DIR] [--session ID] [--timeout-minutes N] [--ram-mb N] [--quiet] [--explain [--threads N]] -- command [args...]");
}

struct Options {
    std::string cls;
    std::string name;
    std::string cwd;
    std::string session;
    int prio = 0;
    int timeoutMinutes = 0;
    int ramMb = 0;
    int threads = 1;                // --explain only
    bool explain = false;           // shows the command as it would run, runs nothing
    std::vector<std::string> command;
};

/* the command */
// runs it with the grant; returns its exit code (the one of the shell: 128 + signal). fd: the core (-1: none)
int run_(const Options& options, const cluster::Grant& grant, const bool limited, const int core)
{
    const std::vector<std::string> own = [&]() {
        std::vector<std::string> out;
        for (char** e = environ; e != nullptr && *e != nullptr; ++e)
            out.push_back(*e);
        return out;
    }();
    std::vector<std::string> argv = limited ? cluster::grant_argv(options.command, grant.threads) : options.command;
    const bool teeOut = !::isatty(STDOUT_FILENO);
    const bool teeErr = !::isatty(STDERR_FILENO);
    int outPipe[2] = {-1, -1};
    int errPipe[2] = {-1, -1};
    if ((teeOut && ::pipe(outPipe) != 0) || (teeErr && ::pipe(errPipe) != 0)) { // xstyle: ignore LU-SYSCALL (standalone binary)
        warn_(std::string("no pipe: ") + std::strerror(errno));
        return 126;
    }
    const pid_t pid = ::fork(); // xstyle: ignore LU-SYSCALL (standalone binary)
    if (pid < 0) {
        warn_(std::string("fork: ") + std::strerror(errno));
        return 126;
    }
    if (pid == 0) {
        ::setpgid(0, 0);
        if (teeOut) {
            ::dup2(outPipe[1], STDOUT_FILENO); // xstyle: ignore LU-SYSCALL (standalone binary)
            ::close(outPipe[0]);
            ::close(outPipe[1]);
        }
        if (teeErr) {
            ::dup2(errPipe[1], STDERR_FILENO); // xstyle: ignore LU-SYSCALL (standalone binary)
            ::close(errPipe[0]);
            ::close(errPipe[1]);
        }
        if (core >= 0) ::close(core);
        for (const int s: {SIGINT, SIGTERM, SIGHUP, SIGPIPE})
            std::signal(s, SIG_DFL);
        if (!options.cwd.empty() && ::chdir(options.cwd.c_str()) != 0) {
            warn_("cannot enter " + options.cwd + ": " + std::strerror(errno));
            ::_exit(126);
        }
        if (limited) {
            cluster::grant_apply(grant);
            for (const auto &[key, value]: cluster::grant_env(grant.threads, own))
                ::setenv(key.c_str(), value.c_str(), 1);
        }
        std::vector<char*> args;
        for (std::string& arg: argv)
            args.push_back(arg.data());
        args.push_back(nullptr);
        ::execvp(args[0], args.data());
        warn_(argv[0] + ": " + std::strerror(errno));
        ::_exit(errno == ENOENT ? 127 : 126);
    }
    ::setpgid(pid, pid); // both sides: whichever runs first
    if (teeOut) ::close(outPipe[1]);
    if (teeErr) ::close(errPipe[1]);
    int coreFd = core;
    std::string buffer;
    if (coreFd >= 0) (void)send_(coreFd, {{"event", "started"}, {"pid", pid}});
    std::string tail;
    std::string killedBy;
    std::string killedWhy;
    bool coreWarned = false;
    int status = 0;
    bool exited = false;
    int forwarded = 0;
    int fds[3] = {teeOut ? outPipe[0] : -1, teeErr ? errPipe[0] : -1, coreFd};
    const std::function<void(const int)> pump = [&](const int k) { // what the command wrote, passed on and kept at the end
        char chunk[16384];
        const ssize_t size = ::read(fds[k], chunk, sizeof(chunk));
        if (size <= 0) {
            ::close(fds[k]);
            fds[k] = -1;
            return;
        }
        tail = cluster::tail_of(tail + std::string(chunk, static_cast<std::size_t>(size)), 8192);
        for (ssize_t done = 0; done < size;) {
            const ssize_t written = ::write(k == 0 ? STDOUT_FILENO : STDERR_FILENO, chunk + done, static_cast<std::size_t>(size - done));
            if (written <= 0) break;
            done += written;
        }
    };
    while (!exited || fds[0] >= 0 || fds[1] >= 0) {
        if (g_signal != 0 && g_signal != forwarded) { // our own signal: to the command
            forwarded = g_signal;
            ::kill(-pid, g_signal);
        }
        pollfd polls[3];
        int count = 0;
        int which[3];
        for (int i = 0; i < 3; ++i)
            if (fds[i] >= 0 && !(i == 2 && exited)) {
                polls[count] = {fds[i], POLLIN, 0};
                which[count++] = i;
            }
        const int ready = ::poll(polls, static_cast<nfds_t>(count), exited ? 50 : 200); // none to watch: it only waits
        if (ready > 0)
            for (int i = 0; i < count; ++i) {
                if (!(polls[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
                const int k = which[i];
                if (k < 2) {
                    pump(k);
                } else { // the core
                    Json message;
                    int got = 0;
                    while ((got = read_line_(fds[2], buffer, message, 0)) > 0)
                        if (message.value("event", std::string()) == "killed") {
                            killedBy = message.value("by", std::string("claude-cluster"));
                            killedWhy = message.value("reason", std::string());
                            ::kill(-pid, SIGTERM);
                        }
                    if (got < 0) {
                        if (!coreWarned) say_("claude-cluster is gone: the command goes on, no longer counted");
                        coreWarned = true;
                        fds[2] = -1;
                        coreFd = -1;
                    }
                }
            }
        if (!exited && ::waitpid(pid, &status, WNOHANG) == pid) exited = true;
        // a pipe that stays open after the exit (a daemon of the command): read what is there, then stop waiting for it
        if (exited && ready == 0) {
            for (int k = 0; k < 2; ++k)
                if (fds[k] >= 0) {
                    ::close(fds[k]);
                    fds[k] = -1;
                }
        }
    }
    if (coreFd >= 0 && killedBy.empty() && WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM) {
        // the core stopped the command itself and tells who right after: its event is waited for a moment (the command ended first)
        Json message;
        for (int tries = 0; tries < 5 && killedBy.empty(); ++tries) {
            if (read_line_(coreFd, buffer, message, 100) > 0 && message.value("event", std::string()) == "killed") {
                killedBy = message.value("by", std::string("claude-cluster"));
                killedWhy = message.value("reason", std::string());
            }
        }
    }
    const int code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    if (coreFd >= 0) {
        (void)send_(coreFd, {{"event", "exit"}, {"code", code}, {"signal", WIFSIGNALED(status) ? WTERMSIG(status) : 0}, {"tail", tail}});
        Json message;
        (void)read_line_(coreFd, buffer, message, 2000);
        ::close(coreFd);
    }
    if (!killedBy.empty()) {
        warn_("killed by " + killedBy + (killedWhy.empty() ? "" : ": " + killedWhy));
        return 143;
    }
    return code;
}

} // namespace

int main(int argc, char* argv[])
{
    Options options;
    options.session = std::getenv("CLUSTER_RUN_SESSION") != nullptr ? std::getenv("CLUSTER_RUN_SESSION") : "";
    g_quiet = std::getenv("CLUSTER_RUN_QUIET") != nullptr && std::string(std::getenv("CLUSTER_RUN_QUIET")) != "0";
    int i = 1;
    for (; i < argc; ++i) {
        const std::string arg = argv[i]; // xstyle: ignore LU-ARGS (standalone binary)
        const std::function<std::string(void)> value = [&]() {
            if (i + 1 >= argc) {
                usage_();
                std::exit(2);
            }
            return std::string(argv[++i]);
        };
        if (arg == "--") {
            ++i;
            break;
        }
        if (arg == "--class") options.cls = value();
        else if (arg == "--prio") options.prio = std::atoi(value().c_str());
        else if (arg == "--name") options.name = value();
        else if (arg == "--cwd") options.cwd = value();
        else if (arg == "--session") options.session = value();
        else if (arg == "--timeout-minutes") options.timeoutMinutes = std::atoi(value().c_str());
        else if (arg == "--ram-mb") options.ramMb = std::atoi(value().c_str());
        else if (arg == "--quiet") g_quiet = true;
        else if (arg == "--explain") options.explain = true;
        else if (arg == "--threads") options.threads = std::max(1, std::atoi(value().c_str()));
        else if (arg == "-h" || arg == "--help") {
            usage_();
            return 0;
        } else if (arg.starts_with("-")) {
            warn_("unknown option " + arg);
            usage_();
            return 2;
        } else {
            break; // the command, no "--"
        }
    }
    for (; i < argc; ++i)
        options.command.push_back(argv[i]);
    if (options.command.empty()) {
        usage_();
        return 2;
    }
    if (options.name.empty()) options.name = options.command[0].substr(options.command[0].rfind('/') == std::string::npos ? 0 : options.command[0].rfind('/') + 1);
    if (options.explain) { // what the granted share does to the command, for the tests and for curious people
        std::string line;
        for (const std::string& word: cluster::grant_argv(options.command, options.threads))
            line += (line.empty() ? "" : " ") + word;
        std::string vars;
        for (const auto &[key, value]: cluster::grant_env(options.threads))
            vars += (vars.empty() ? "" : " ") + key + "=" + value;
        std::printf("command: %s\nenv: %s\n", line.c_str(), vars.c_str());
        return 0;
    }
    for (const int s: {SIGINT, SIGTERM, SIGHUP}) {
        struct sigaction action{};
        action.sa_handler = on_signal_;
        sigemptyset(&action.sa_mask);
        ::sigaction(s, &action, nullptr);
    }
    std::signal(SIGPIPE, SIG_IGN);
    // the low profile when the core can't say (not running, off): 1 thread, nice 19, ionice idle
    const cluster::Grant low{1, 19, 3, 0};
    if (std::getenv("CLUSTER_RUN_OFF") != nullptr && std::string(std::getenv("CLUSTER_RUN_OFF")) == "1") return run_(options, low, false, -1);
    const std::string socket = socket_path_();
    const std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
    std::string lastLine;
    bool first = true;
    for (;;) {
        const int fd = connect_(socket);
        if (fd < 0) {
            if (first) {
                warn_("claude-cluster is not running: the command runs directly, in the low profile");
                return run_(options, low, true, -1);
            }
            if (std::chrono::steady_clock::now() - began > std::chrono::seconds(30)) {
                warn_("claude-cluster is not back: the command runs directly, in the low profile");
                return run_(options, low, true, -1);
            }
            ::sleep(1);
            continue;
        }
        first = false;
        Json request = {{"cmd", "job_run"}, {"session", options.session}, {"class", options.cls}, {"prio", options.prio}, {"name", options.name}, {"cwd", options.cwd},
            {"argv", options.command}, {"command", join_(options.command)}, {"timeout_minutes", options.timeoutMinutes}, {"ram_mb", options.ramMb}, {"wrapper_pid", ::getpid()}};
        if (request["cwd"] == "") {
            char here[4096];
            request["cwd"] = ::getcwd(here, sizeof(here)) != nullptr ? std::string(here) : std::string();
        }
        if (!send_(fd, request)) {
            ::close(fd);
            continue;
        }
        std::string buffer;
        for (;;) {
            if (g_signal != 0) { // asked to stop while it waits: the job is dropped with the connection
                ::close(fd);
                return 128 + g_signal;
            }
            Json message;
            const int got = read_line_(fd, buffer, message, 500);
            if (got < 0) { // the core went: tried again for a while
                ::close(fd);
                warn_("lost claude-cluster while waiting: trying again");
                break;
            }
            if (got == 0) continue;
            const std::string event = message.value("event", std::string());
            if (!message.value("ok", false)) {
                warn_("refused: " + message.value("error", std::string("?")));
                ::close(fd);
                return 125;
            }
            if (event == "queued") {
                const std::string line = "queued: position " + std::to_string(message.value("position", 0)) + " of " + std::to_string(message.value("total", 0))
                    + (message.value("waiting_for", std::string()).empty() ? std::string() : ", waiting for " + message.value("waiting_for", std::string()))
                    + ", ETA about " + std::to_string(message.value("eta_s", 0) / 60) + " min";
                if (line != lastLine) say_(line);
                lastLine = line;
            } else if (event == "killed") {
                warn_("killed by " + message.value("by", std::string("?")) + (message.value("reason", std::string()).empty() ? "" : ": " + message.value("reason", std::string())) + " (it never ran)");
                ::close(fd);
                return 143;
            } else if (event == "go") {
                if (message.value("unmanaged", false)) {
                    ::close(fd);
                    return run_(options, low, false, -1);
                }
                if (!lastLine.empty()) say_("started");
                cluster::Grant grant;
                grant.threads = std::max(1, message.value("threads", 1));
                grant.nice = message.value("nice", 19);
                grant.ioClass = message.value("io_class", 3);
                grant.ioLevel = message.value("io_level", 0);
                return run_(options, grant, true, fd);
            }
        }
        if (std::chrono::steady_clock::now() - began > std::chrono::seconds(30)) {
            warn_("claude-cluster is not back: the command runs directly, in the low profile");
            return run_(options, low, true, -1);
        }
        ::sleep(1);
    }
}
