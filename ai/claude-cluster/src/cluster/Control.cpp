/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Control.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _Encapsulation
#include <utils/utils.hpp>
#include "cluster/Control.hpp"
#include "cluster/Tools.hpp"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include <array>

/* tools */
_cold static sockaddr_un address_(const std::filesystem::path& path)
{
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.string().size() >= sizeof(addr.sun_path))
        throw utils::exception::ErrorException(utils::exception::InternalCode::SocketInit, "socket path too long: " + path.string());
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    return addr;
}

_cold static int connect_(const std::filesystem::path& path)
{
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); // xstyle: ignore LU-SOCKET (unix socket: libutils only has TCP)
    if (fd == -1) return -1;
    const sockaddr_un addr = address_(path);
    if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1) {
        ::close(fd);
        return -1;
    }
    return fd;
}

_cold static bool send_line_(const int fd, const std::string& line)
{
    const std::string data = line + "\n";
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t size = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (size <= 0) return false;
        sent += static_cast<std::size_t>(size);
    }
    return true;
}

_cold static std::optional<std::string> read_line_(const int fd, std::string& buffer, const int timeout, bool& closed)
{
    utils::encapsulation::Poll poll;
    poll.link(fd, EPOLLIN);
    std::array<char, 8192> chunk{};
    closed = false;
    while (buffer.find('\n') == std::string::npos) {
        if (poll.wait(timeout).empty()) return std::nullopt;
        const ssize_t size = ::recv(fd, chunk.data(), chunk.size(), 0);
        if (size <= 0) {
            closed = true;
            return std::nullopt;
        }
        buffer.append(chunk.data(), static_cast<std::size_t>(size));
    }
    const std::size_t nl = buffer.find('\n');
    std::string line = buffer.substr(0, nl);
    buffer.erase(0, nl + 1);
    return line;
}

_cold static std::string arg_(const cluster::Json& request, const char* key, const std::string& fallback = "")
{
    if (!request.contains(key) || request[key].is_null()) return fallback;
    return request[key].is_string() ? request[key].get<std::string>() : request[key].dump();
}

/* constructor */
_cold cluster::Control::Control(cluster::Manager& manager)
    : _manager(manager), _path(cluster::socket_path())
{
}

_cold cluster::Control::~Control()
{
    this->stop();
}

/* server */
_cold bool cluster::Control::alive(void)
{
    const int fd = connect_(cluster::socket_path());
    if (fd == -1) return false;
    ::close(fd);
    return true;
}

_cold void cluster::Control::start(void)
{
    std::error_code error;
    std::filesystem::create_directories(this->_path.parent_path(), error);
    ::chmod(this->_path.parent_path().c_str(), S_IRWXU);
    if (cluster::Control::alive())
        throw utils::exception::ErrorException(utils::exception::InternalCode::AlreadyRunning,
            "claude-cluster is already running (" + this->_path.string() + "): use the headless commands (claude-cluster list...)");
    std::filesystem::remove(this->_path, error); // left by a crash

    this->_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); // xstyle: ignore LU-SOCKET (unix socket: libutils only has TCP)
    if (this->_fd == -1) throw utils::exception::ErrorException(utils::exception::InternalCode::Socket, std::strerror(errno));
    const sockaddr_un addr = address_(this->_path);
    if (::bind(this->_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1 || ::listen(this->_fd, 16) == -1) {
        const std::string reason = std::strerror(errno);
        ::close(this->_fd);
        this->_fd = -1;
        throw utils::exception::ErrorException(utils::exception::InternalCode::SocketInit, this->_path.string() + ": " + reason);
    }
    ::chmod(this->_path.c_str(), S_IRUSR | S_IWUSR);
    this->_running = true;
    this->_thread = std::thread(&cluster::Control::serve_, this);
}

_cold void cluster::Control::stop(void)
{
    if (!this->_running) return;
    this->_running = false;
    if (this->_thread.joinable()) this->_thread.join();
    if (this->_fd != -1) ::close(this->_fd);
    this->_fd = -1;
    std::error_code error;
    std::filesystem::remove(this->_path, error);
}

_cold void cluster::Control::serve_(void)
{
    utils::encapsulation::Poll poll;
    poll.link(this->_fd, EPOLLIN);
    while (this->_running) {
        if (poll.wait(300).empty()) continue;
        const int client = ::accept4(this->_fd, nullptr, nullptr, SOCK_CLOEXEC);
        if (client == -1) continue;
        std::thread(&cluster::Control::client_, this, client).detach();
    }
}

_cold void cluster::Control::client_(const int fd)
{
    // One connection = any number of requests (the MCP server keeps it open)
    std::string buffer;
    bool closed = false;
    while (this->_running && !closed) {
        const std::optional<std::string> line = read_line_(fd, buffer, 500, closed);
        if (!line) continue;
        cluster::Json response;
        try {
            response = cluster::Control::handle(this->_manager, cluster::Json::parse(*line));
        } catch (const cluster::Json::exception& e) {
            response = {{"ok", false}, {"error", std::string("bad request: ") + e.what()}};
        }
        if (!send_line_(fd, response.dump())) break;
    }
    ::close(fd);
}

/* client */
_cold cluster::Json cluster::Control::request(const cluster::Json& request)
{
    const int fd = connect_(cluster::socket_path());
    if (fd == -1) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning,
        "claude-cluster is not running (start it: claude-cluster)");
    std::string buffer;
    bool closed = false;
    const bool sent = send_line_(fd, request.dump());
    const std::optional<std::string> line = sent ? read_line_(fd, buffer, 120000, closed) : std::nullopt;
    ::close(fd);
    if (!line) throw utils::exception::ErrorException(utils::exception::InternalCode::Communication, "no answer from claude-cluster");
    return cluster::Json::parse(*line);
}

/* commands */
_cold cluster::Json cluster::Control::handle(cluster::Manager& manager, const cluster::Json& request)
{
    const std::string cmd = arg_(request, "cmd");
    const std::function<std::string()> session = [&]() {return manager.resolve(arg_(request, "session"));};
    cluster::Json data;

    try {
        if (cmd == "list") {
            data = cluster::Json::array();
            for (const cluster::Snapshot& snap: manager.list(request.value("global", true)))
                data.push_back(manager.toJson(snap, false));
        } else if (cmd == "status" || cmd == "read") {
            const std::optional<cluster::Snapshot> snap = manager.snapshot(session());
            if (snap) data = manager.toJson(*snap, true);
        } else if (cmd == "spawn") {
            data = {{"id", manager.spawn({arg_(request, "cwd"), arg_(request, "name"), arg_(request, "backend"),
                arg_(request, "mode"), arg_(request, "profile"), arg_(request, "prompt")})}};
        } else if (cmd == "send") {
            const std::string id = session();
            manager.send(id, arg_(request, "text"), request.value("force", false));
            data = {{"id", id}, {"queued", true}};
        } else if (cmd == "close") {
            manager.close(session());
        } else if (cmd == "restore") {
            data = {{"id", manager.restore(arg_(request, "session"))}};
        } else if (cmd == "purge") {
            manager.purge(arg_(request, "session"));
        } else if (cmd == "rename") {
            manager.rename(session(), arg_(request, "name"));
        } else if (cmd == "mode") {
            manager.setMode(session(), arg_(request, "mode"));
        } else if (cmd == "allow") {
            manager.answer(session(), arg_(request, "request_id"), arg_(request, "behavior", "allow"));
        } else if (cmd == "interrupt") {
            manager.interrupt(session());
        } else if (cmd == "cd") {
            manager.cd(session(), arg_(request, "cwd"));
        } else if (cmd == "trash") {
            data = cluster::Json::array();
            for (const cluster::SessionSpec& spec: manager.trash())
                data.push_back({{"id", spec.id}, {"name", spec.name}, {"cwd", spec.cwd}, {"backend", spec.backend}, {"deleted", spec.deleted}});
        } else if (cmd == "task_add") {
            data = {{"id", manager.addTask(arg_(request, "prompt"), arg_(request, "target"), request.value("after", std::vector<std::string>()))}};
        } else if (cmd == "task_list") {
            data = cluster::Json::array();
            for (const cluster::Task& t: manager.tasks())
                data.push_back({{"id", t.id}, {"prompt", cluster::one_line(t.prompt, 200)}, {"target", t.target}, {"after", t.after}, {"status", t.status}, {"session", t.session}});
        } else if (cmd == "task_cancel") {
            manager.cancelTask(arg_(request, "id"));
        } else if (cmd == "search") {
            data = cluster::Json::array();
            for (const cluster::Hit& hit: manager.search(arg_(request, "text")))
                data.push_back({{"session", hit.session}, {"name", hit.name}, {"time", hit.time}, {"text", hit.text}});
        } else if (cmd == "export") {
            const std::string markdown = manager.exportMarkdown(arg_(request, "session"));
            const std::string path = arg_(request, "path");
            if (path.empty()) {
                data = {{"markdown", markdown}};
            } else {
                std::ofstream(cluster::expand_home(path)) << markdown;
                data = {{"path", cluster::expand_home(path).string()}};
            }
        } else if (cmd == "remote") {
            manager.setRemote(request.value("on", true));
        } else if (cmd == "notices") {
            data = cluster::Json::array();
            for (const cluster::Notice& n: manager.notices(30))
                data.push_back({{"time", n.time}, {"session", n.session}, {"text", n.text}, {"error", n.error}});
        } else if (cmd == "providers") {
            data = cluster::Json::array();
            for (const cluster::Provider& p: manager.auth().providers()) {
                const auto [ready, detail] = manager.auth().status(p);
                data.push_back({{"name", p.name}, {"driver", p.driver}, {"ready", ready}, {"detail", detail}, {"local", p.local}, {"description", p.description}});
            }
        } else {
            return {{"ok", false}, {"error", "unknown command '" + cmd + "'"}};
        }
    } catch (const utils::exception::IException& e) {
        return {{"ok", false}, {"error", std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info())}};
    }
    return {{"ok", true}, {"data", data}};
}
