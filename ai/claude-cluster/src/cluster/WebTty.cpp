/**************************************************************\
Edition:
##  @date 16/10/2026 by @author Tsukini

File Name:
##  @file WebTty.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _SManip
#include <utils/utils.hpp>
#include "cluster/WebTty.hpp"
#include "cluster/Tools.hpp"
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <algorithm>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <chrono>
#include <array>
#include <pty.h>

#define OUT_KEPT 524288

/* the pty */
_cold cluster::Pty cluster::pty_spawn(const int cols, const int rows, const bool web)
{
    // the environment made before the fork (the child only execs): TERM of a modern terminal, the web rules when asked
    std::vector<std::string> env;
    for (char** e = environ; e != nullptr && *e != nullptr; ++e) {
        const std::string var(*e);
        if (var.starts_with("TERM=") || var.starts_with("COLORTERM=") || var.starts_with("COLUMNS=") || var.starts_with("LINES=") || var.starts_with("CLAUDE_CLUSTER_WEB_TTY=")) continue;
        env.push_back(var);
    }
    env.emplace_back("TERM=xterm-256color");
    env.emplace_back("COLORTERM=truecolor");
    if (web) env.emplace_back("CLAUDE_CLUSTER_WEB_TTY=1");
    std::vector<char*> envp;
    for (std::string& var: env)
        envp.push_back(var.data());
    envp.push_back(nullptr);
    const std::string exe = cluster::self_exe().string();
    std::string arg = "--tty";
    char* argv[] = {const_cast<char*>(exe.c_str()), arg.data(), nullptr};
    winsize size{};
    size.ws_col = static_cast<unsigned short>(std::clamp(cols, 20, 500));
    size.ws_row = static_cast<unsigned short>(std::clamp(rows, 5, 200));
    cluster::Pty pty;
    const pid_t pid = ::forkpty(&pty.fd, nullptr, nullptr, &size);
    if (pid < 0) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, std::string("no pty: ") + std::strerror(errno));
    if (pid == 0) {
        ::execve(exe.c_str(), argv, envp.data());
        ::_exit(127);
    }
    pty.pid = static_cast<int>(pid);
    return pty;
}

_cold void cluster::pty_resize(const cluster::Pty& pty, const int cols, const int rows)
{
    if (pty.fd < 0) return;
    winsize size{};
    size.ws_col = static_cast<unsigned short>(std::clamp(cols, 20, 500));
    size.ws_row = static_cast<unsigned short>(std::clamp(rows, 5, 200));
    (void)::ioctl(pty.fd, TIOCSWINSZ, &size); // the kernel tells the terminal (SIGWINCH)
}

_cold void cluster::pty_close(cluster::Pty& pty)
{
    cluster::pty_kill(pty);
    if (pty.fd >= 0) ::close(pty.fd);
    pty.fd = -1;
}

_cold void cluster::pty_kill(cluster::Pty& pty)
{
    if (pty.pid > 0) {
        (void)::kill(pty.pid, SIGHUP);
        for (int i = 0; i < 20; ++i) {
            if (::waitpid(pty.pid, nullptr, WNOHANG) == pty.pid) {
                pty.pid = -1;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        (void)::kill(pty.pid, SIGKILL);
        (void)::waitpid(pty.pid, nullptr, 0);
    }
    pty.pid = -1;
}

/* the terminals of the page */
_cold cluster::WebTty::~WebTty()
{
    this->closeAll();
}

_cold void cluster::WebTty::read_(Session* session)
{
    std::array<char, 16384> chunk{};
    while (true) {
        const ssize_t size = ::read(session->pty.fd, chunk.data(), chunk.size());
        std::lock_guard<std::mutex> lock(session->mutex);
        if (size <= 0) { // the end (the terminal quit, the pty closed)
            session->alive = false;
            session->changed.notify_all();
            return;
        }
        session->out.append(chunk.data(), static_cast<std::size_t>(size));
        if (session->out.size() > OUT_KEPT) { // the oldest dropped (a page far behind draws from what is left)
            const std::size_t drop = session->out.size() - OUT_KEPT;
            session->out.erase(0, drop);
            session->base += drop;
        }
        session->changed.notify_all();
    }
}

_cold void cluster::WebTty::drop_(const std::shared_ptr<Session>& session)
{
    cluster::pty_kill(session->pty); // the read ends (EIO), its thread joined, then only its fd closed (never reused under a read)
    if (session->reader.joinable()) session->reader.join();
    if (session->pty.fd >= 0) ::close(session->pty.fd);
    session->pty.fd = -1;
}

_cold void cluster::WebTty::resize_(Session& session, const int cols, const int rows)
{
    session.cols = std::clamp(cols, 20, 500);
    session.rows = std::clamp(rows, 5, 200);
    cluster::pty_resize(session.pty, session.cols, session.rows);
}

_cold cluster::Json cluster::WebTty::open(const std::string& device, const int cols, const int rows)
{
    std::shared_ptr<Session> old;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = this->_sessions.find(device);
        if (it != this->_sessions.end()) {
            std::lock_guard<std::mutex> hold(it->second->mutex);
            if (it->second->alive) { // kept: resized to the page, drawn again (a size change twice: the terminal redraws itself)
                this->resize_(*it->second, cols, std::clamp(rows, 5, 200) + 1);
                this->resize_(*it->second, cols, rows);
                return {{"cols", it->second->cols}, {"rows", it->second->rows}, {"next", it->second->base + it->second->out.size()}, {"kept", true}};
            }
            old = it->second;
            this->_sessions.erase(it);
        }
        if (this->_sessions.size() >= 8) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "8 terminals of the page at once at most");
    }
    if (old) this->drop_(old);
    std::shared_ptr<Session> session = std::make_shared<Session>();
    session->cols = std::clamp(cols, 20, 500);
    session->rows = std::clamp(rows, 5, 200);
    session->pty = cluster::pty_spawn(session->cols, session->rows, true);
    session->seen = cluster::now_ms();
    Session* raw = session.get();
    session->reader = std::thread(&cluster::WebTty::read_, this, raw);
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_sessions[device] = session;
    return {{"cols", raw->cols}, {"rows", raw->rows}, {"next", 0}, {"kept", false}};
}

_cold cluster::Json cluster::WebTty::read(const std::string& device, const std::uint64_t from, const int waitMs)
{
    std::shared_ptr<Session> session; // kept alive for the whole poll, even closed meanwhile
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = this->_sessions.find(device);
        if (it == this->_sessions.end()) return {{"alive", false}, {"none", true}, {"from", from}, {"next", from}, {"data", ""}};
        session = it->second;
    }
    std::unique_lock<std::mutex> lock(session->mutex);
    session->seen = cluster::now_ms();
    const std::function<bool()> ready = [&session, from]() {return !session->alive || session->base + session->out.size() > from;};
    if (!ready()) session->changed.wait_for(lock, std::chrono::milliseconds(std::clamp(waitMs, 0, 30000)), ready);
    const std::uint64_t end = session->base + session->out.size();
    const std::uint64_t start = std::max(from, session->base);
    const std::string data = start < end ? session->out.substr(static_cast<std::size_t>(start - session->base)) : std::string();
    return {{"alive", session->alive}, {"from", start}, {"next", end}, {"data", utils::smanip::codec::Base64Codec().encode(data)}, {"cols", session->cols}, {"rows", session->rows}};
}

_cold void cluster::WebTty::write(const std::string& device, const std::string& bytes)
{
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = this->_sessions.find(device);
        if (it == this->_sessions.end()) return;
        session = it->second;
    }
    std::lock_guard<std::mutex> lock(session->mutex); // the keys in order; the others never wait on a slow pty
    if (session->pty.fd < 0) return;
    for (std::size_t written = 0; written < bytes.size();) {
        const ssize_t size = ::write(session->pty.fd, bytes.data() + written, bytes.size() - written);
        if (size <= 0) return;
        written += static_cast<std::size_t>(size);
    }
}

_cold void cluster::WebTty::resize(const std::string& device, const int cols, const int rows)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    auto it = this->_sessions.find(device);
    if (it == this->_sessions.end()) return;
    std::lock_guard<std::mutex> hold(it->second->mutex);
    this->resize_(*it->second, cols, rows);
}

_cold void cluster::WebTty::close(const std::string& device)
{
    std::shared_ptr<Session> gone;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = this->_sessions.find(device);
        if (it == this->_sessions.end()) return;
        gone = it->second;
        this->_sessions.erase(it);
    }
    this->drop_(gone);
}

_cold void cluster::WebTty::expire(void)
{
    std::vector<std::shared_ptr<Session>> gone;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        const std::int64_t now = cluster::now_ms();
        for (auto it = this->_sessions.begin(); it != this->_sessions.end();) {
            std::int64_t seen = 0;
            bool alive = true;
            {
                std::lock_guard<std::mutex> hold(it->second->mutex);
                seen = it->second->seen;
                alive = it->second->alive;
            }
            if (now - seen > 120000 || (!alive && now - seen > 10000)) {
                gone.push_back(it->second);
                it = this->_sessions.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (const std::shared_ptr<Session>& session: gone)
        this->drop_(session);
}

_cold void cluster::WebTty::closeAll(void)
{
    std::vector<std::shared_ptr<Session>> gone;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (auto &[device, session]: this->_sessions)
            gone.push_back(session);
        this->_sessions.clear();
    }
    for (const std::shared_ptr<Session>& session: gone)
        this->drop_(session);
}

_cold std::size_t cluster::WebTty::count(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_sessions.size();
}
