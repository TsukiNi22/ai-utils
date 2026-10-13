/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file Web.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _Encapsulation
#include <utils/utils.hpp>
#include "cluster/Templates.hpp"
#include "cluster/Control.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Web.hpp"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <algorithm>
#include <ifaddrs.h>
#include <unistd.h>
#include <net/if.h>
#include <cstring>
#include <sstream>
#include <random>
#include <chrono>
#include <array>
#include <set>
#include <map>

/* tools */
struct Request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;     // names in lower case
    std::string body;
};

_cold static std::int64_t now_ms_(void)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

_cold static std::string random_digits_(const std::size_t count)
{
    std::random_device device;
    std::uniform_int_distribution<int> digit(0, 9);
    std::string out;
    for (std::size_t i = 0; i < count; ++i)
        out += static_cast<char>('0' + digit(device));
    return out;
}

_cold static std::string random_hex_(const std::size_t bytes)
{
    static const char* hex = "0123456789abcdef";
    std::random_device device;
    std::uniform_int_distribution<int> nibble(0, 15);
    std::string out;
    for (std::size_t i = 0; i < bytes * 2; ++i)
        out += hex[nibble(device)];
    return out;
}

_cold static std::string url_decode_(const std::string& text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+') out += ' ';
        else if (text[i] == '%' && i + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[i + 1])) && std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
            out += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            out += text[i];
        }
    }
    return out;
}

_cold static bool send_all_(const int fd, const std::string& data)
{
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t size = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (size <= 0) return false;
        sent += static_cast<std::size_t>(size);
    }
    return true;
}

_cold static bool read_request_(const int fd, Request& request)
{
    // One request, 10 s at most: its headers (16 KB at most), its body (Content-Length, 1 MB at most)
    utils::encapsulation::Poll poll;
    poll.link(fd, EPOLLIN);
    std::array<char, 8192> chunk{};
    std::string data;
    const std::int64_t end = now_ms_() + 10000;
    std::size_t head = std::string::npos;
    std::size_t length = 0;

    while (true) {
        if (head == std::string::npos && (head = data.find("\r\n\r\n")) != std::string::npos) {
            std::istringstream lines(data.substr(0, head));
            std::string line;
            std::getline(lines, line);
            std::istringstream first(line);
            std::string target;
            first >> request.method >> target;
            const std::size_t mark = target.find('?');
            request.path = target.substr(0, mark);
            if (mark != std::string::npos)
                for (const std::string& pair: cluster::split(target.substr(mark + 1), '&')) {
                    const std::size_t equal = pair.find('=');
                    if (equal != std::string::npos) request.query[url_decode_(pair.substr(0, equal))] = url_decode_(pair.substr(equal + 1));
                }
            while (std::getline(lines, line)) {
                const std::size_t colon = line.find(':');
                if (colon != std::string::npos) request.headers[cluster::lower(cluster::trim(line.substr(0, colon)))] = cluster::trim(line.substr(colon + 1));
            }
            try {
                length = request.headers.contains("content-length") ? std::stoul(request.headers["content-length"]) : 0;
            } catch (const std::exception&) {
                return false;
            }
            if (length > 1000000) return false;
        }
        if (head != std::string::npos && data.size() >= head + 4 + length) {
            request.body = data.substr(head + 4, length);
            return !request.method.empty();
        }
        if (head == std::string::npos && data.size() > 16384) return false;
        const std::int64_t left = end - now_ms_();
        if (left <= 0 || poll.wait(static_cast<int>(left)).empty()) return false;
        const ssize_t size = ::recv(fd, chunk.data(), chunk.size(), 0);
        if (size <= 0) return false;
        data.append(chunk.data(), static_cast<std::size_t>(size));
    }
}

_cold static void respond_(const int fd, const int code, const std::string& type, const std::string& body, const std::string& extra = "")
{
    static const std::map<int, std::string> reasons = {{200, "OK"}, {400, "Bad Request"}, {401, "Unauthorized"}, {403, "Forbidden"},
        {404, "Not Found"}, {423, "Locked"}, {500, "Internal Server Error"}};
    const std::string reason = reasons.contains(code) ? reasons.at(code) : "Error";
    (void)send_all_(fd, "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size())
        + "\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\n" + extra
        + "Connection: close\r\n\r\n" + body);
}

_cold static void json_(const int fd, const int code, const cluster::Json& body, const std::string& extra = "")
{
    respond_(fd, code, "application/json; charset=utf-8", body.dump(-1, ' ', false, cluster::Json::error_handler_t::replace), extra);
}

_cold static bool private_(const std::uint32_t ip)
{
    // host order: 10/8, 172.16/12, 192.168/16, 169.254/16 (link-local), 127/8 (this computer): never a public address
    return (ip >> 24) == 10 || (ip >> 20) == 0xAC1 || (ip >> 16) == 0xC0A8 || (ip >> 16) == 0xA9FE || (ip >> 24) == 127;
}

_cold static bool same_lan_(const std::string& peer)
{
    // the browser on this computer, or on the network of one of its interfaces (same subnet), with a private address only
    in_addr address{};
    if (::inet_pton(AF_INET, peer.c_str(), &address) != 1) return false;
    const std::uint32_t ip = ntohl(address.s_addr);
    if (!private_(ip)) return false;
    if ((ip >> 24) == 127) return true;
    ifaddrs* list = nullptr;
    if (::getifaddrs(&list) == -1) return false;
    bool found = false;
    for (const ifaddrs* it = list; it != nullptr && !found; it = it->ifa_next) {
        if (it->ifa_addr == nullptr || it->ifa_netmask == nullptr || it->ifa_addr->sa_family != AF_INET || !(it->ifa_flags & IFF_UP)) continue;
        // xstyle: ignore-next LU-SOCKET (the addresses of the interfaces read, no socket)
        const std::uint32_t own = ntohl(reinterpret_cast<const sockaddr_in*>(it->ifa_addr)->sin_addr.s_addr);
        const std::uint32_t mask = ntohl(reinterpret_cast<const sockaddr_in*>(it->ifa_netmask)->sin_addr.s_addr);
        found = (it->ifa_flags & IFF_LOOPBACK) == 0 && mask != 0 && (own & mask) == (ip & mask);
    }
    ::freeifaddrs(list);
    return found;
}

_cold static std::string lan_address_(void)
{
    // the address the other devices of the LAN reach: 192.168.x first, then 10.x, then 172.16-31.x
    ifaddrs* list = nullptr;
    if (::getifaddrs(&list) == -1) return "127.0.0.1";
    std::string best;
    int rank = 9;
    for (const ifaddrs* it = list; it != nullptr; it = it->ifa_next) {
        if (it->ifa_addr == nullptr || it->ifa_addr->sa_family != AF_INET || !(it->ifa_flags & IFF_UP) || (it->ifa_flags & IFF_LOOPBACK)) continue;
        const std::uint32_t ip = ntohl(reinterpret_cast<const sockaddr_in*>(it->ifa_addr)->sin_addr.s_addr);
        const int r = (ip >> 16) == 0xC0A8 ? 0 : (ip >> 24) == 10 ? 1 : (ip >> 20) == 0xAC1 ? 2 : 9;
        if (r < rank) {
            std::array<char, INET_ADDRSTRLEN> text{};
            ::inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in*>(it->ifa_addr)->sin_addr, text.data(), text.size());
            best = text.data();
            rank = r;
        }
    }
    ::freeifaddrs(list);
    return best.empty() ? "127.0.0.1" : best;
}

_cold static bool host_ok_(const std::string& host)
{
    // an address, localhost or the name of this computer: a public name pointed at the LAN (DNS rebinding) refused
    std::string name = cluster::lower(host.substr(0, host.rfind(':') == std::string::npos ? host.size() : host.rfind(':')));
    in_addr address{};
    if (::inet_pton(AF_INET, name.c_str(), &address) == 1) return true;
    std::array<char, 256> own{};
    (void)::gethostname(own.data(), own.size() - 1);
    const std::string me = cluster::lower(own.data());
    return name == "localhost" || (!me.empty() && (name == me || name == me + ".local" || name == me + ".lan" || name == me + ".home"));
}

_cold static std::string cookie_(const Request& request, const std::string& name)
{
    if (!request.headers.contains("cookie")) return "";
    for (const std::string& part: cluster::split(request.headers.at("cookie"), ';')) {
        const std::string pair = cluster::trim(part);
        if (pair.starts_with(name + "=")) return pair.substr(name.size() + 1);
    }
    return "";
}

_cold static std::string kind_name_(const cluster::EntryKind kind)
{
    switch (kind) {
        case cluster::EntryKind::User: return "user";
        case cluster::EntryKind::Assistant: return "assistant";
        case cluster::EntryKind::Thinking: return "thinking";
        case cluster::EntryKind::Tool: return "tool";
        case cluster::EntryKind::ToolResult: return "tool_result";
        case cluster::EntryKind::System: return "system";
        default: return "error";
    }
}

_cold static cluster::Json session_json_(cluster::Manager& manager, const std::string& id)
{
    // the session shown: its state, permissions, questions and the last 150 entries of its transcript
    const std::optional<cluster::Snapshot> snap = manager.snapshot(id);
    if (!snap) return nullptr;
    cluster::Json json = manager.toJson(*snap, false);
    json["messages"] = cluster::Json::array();
    const std::size_t start = snap->entries.size() > 150 ? snap->entries.size() - 150 : 0;
    for (std::size_t i = start; i < snap->entries.size(); ++i) {
        const cluster::Entry& e = snap->entries[i];
        if (e.kind == cluster::EntryKind::Thinking) continue;
        const bool long_text = e.kind == cluster::EntryKind::Assistant || e.kind == cluster::EntryKind::User;
        std::string text = e.text.size() > (long_text ? 30000 : 600) ? e.text.substr(0, long_text ? 30000 : 600) + " [...]" : e.text;
        json["messages"].push_back({{"kind", kind_name_(e.kind)}, {"text", text}, {"tool", e.tool}, {"time", e.time}});
    }
    return json;
}

/* constructor */
_cold cluster::Web::Web(cluster::Manager& manager, const std::optional<bool> force)
    : _manager(manager), _force(force)
{
}

_cold cluster::Web::~Web()
{
    this->close_();
}

/* port */
_cold void cluster::Web::tick(void)
{
    // the port follows [web] enabled (or web on / off for this run) and [web] port; the browser gone frees the place
    std::lock_guard<std::mutex> port(this->_portMutex);
    const cluster::Config& config = this->_manager.config();
    bool wanted = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        wanted = this->_force.value_or(config.web.enabled);
        this->expire_();
    }
    if (wanted && (this->_fd == -1 || this->_port != config.web.port)) {
        this->close_();
        this->open_(config.web.port);
    } else if (!wanted && this->_fd != -1) {
        this->close_();
        this->_manager.notice(GLOBAL_ID, "web access closed");
    }
}

_cold void cluster::Web::open_(const int port)
{
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); // xstyle: ignore LU-SOCKET (HTTP: one thread per request, the peer address)
    const int on = 1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // every interface, the peers outside the LAN refused (same_lan_)
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    std::string error;
    if (fd == -1) error = std::strerror(errno);
    else if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1 || ::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1
        || ::listen(fd, 8) == -1) error = std::strerror(errno);
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_port = port;
    if (!error.empty()) {
        if (fd != -1) ::close(fd);
        if (error != this->_error) this->_manager.notice(GLOBAL_ID, "web access: port " + std::to_string(port) + " not opened (" + error + ")", true);
        this->_error = error;
        return;
    }
    this->_error.clear();
    this->_fd = fd;
    this->newPin_();
    this->_running = true;
    this->_thread = std::thread(&cluster::Web::serve_, this);
    this->_manager.notice(GLOBAL_ID, "web access open: http://" + lan_address_() + ":" + std::to_string(port) + " code " + this->_pin.substr(0, 3) + " "
        + this->_pin.substr(3) + " (this network only, one browser at a time)");
}

_cold void cluster::Web::close_(void)
{
    if (this->_fd == -1) return;
    this->_running = false;
    if (this->_thread.joinable()) this->_thread.join();
    for (int i = 0; i < 50 && this->_busy > 0; ++i) // the requests being served end (a waiting state returns within 0.2 s)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::lock_guard<std::mutex> lock(this->_mutex);
    ::close(this->_fd);
    this->_fd = -1;
    this->_token.clear();
    this->_client.clear();
}

_cold void cluster::Web::serve_(void)
{
    utils::encapsulation::Poll poll;
    poll.link(this->_fd, EPOLLIN);
    while (this->_running) {
        if (poll.wait(300).empty()) continue;
        sockaddr_in peer{};
        socklen_t size = sizeof(peer);
        const int client = ::accept4(this->_fd, reinterpret_cast<sockaddr*>(&peer), &size, SOCK_CLOEXEC);
        if (client == -1) continue;
        std::array<char, INET_ADDRSTRLEN> text{};
        ::inet_ntop(AF_INET, &peer.sin_addr, text.data(), text.size());
        ++this->_busy;
        std::thread(&cluster::Web::client_, this, client, std::string(text.data())).detach();
    }
}

/* access */
_cold void cluster::Web::newPin_(void)
{
    this->_pin = random_digits_(6);
    this->_failures = 0;
}

_cold void cluster::Web::expire_(void)
{
    if (this->_token.empty() || now_ms_() - this->_seen < 30000) return;
    this->_manager.notice(GLOBAL_ID, "web: " + this->_client + " gone (nothing for 30 s), the place is free");
    this->_token.clear();
    this->_client.clear();
}

_cold void cluster::Web::setOn(const bool on)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_force = on;
}

_cold void cluster::Web::kick(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->newPin_();
    if (!this->_client.empty()) this->_manager.notice(GLOBAL_ID, "web: " + this->_client + " disconnected from here, new code " + this->_pin.substr(0, 3)
        + " " + this->_pin.substr(3));
    this->_token.clear();
    this->_client.clear();
}

_cold void cluster::Web::renewPin(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->newPin_();
}

_cold cluster::WebStatus cluster::Web::status(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    cluster::WebStatus status;
    status.on = this->_fd != -1;
    status.error = this->_error;
    if (!status.on) return status;
    status.url = "http://" + lan_address_() + ":" + std::to_string(this->_port);
    status.pin = this->_pin.substr(0, 3) + " " + this->_pin.substr(3);
    status.client = this->_client;
    return status;
}

/* requests */
_cold void cluster::Web::client_(const int fd, const std::string& peer)
{
    // One request per connection; this LAN only, the right host, the browser connected for the API
    static const std::set<std::string> allowed = {"list", "read", "status", "send", "allow", "interrupt", "spawn", "close", "rename", "mode",
        "model", "notices", "usage", "task_add", "task_list", "trash", "restore"};
    Request request;
    try {
        if (!same_lan_(peer)) {
            respond_(fd, 403, "text/plain", "claude-cluster: this network only\n");
        } else if (read_request_(fd, request)) {
            const std::string host = request.headers.contains("host") ? request.headers.at("host") : "";
            const std::string origin = request.headers.contains("origin") ? request.headers.at("origin") : "";
            const bool post = request.method == "POST";
            if (!host_ok_(host)) {
                respond_(fd, 403, "text/plain", "claude-cluster: open it by the address of this computer\n");
            } else if (post && !origin.empty() && origin != "http://" + host) {
                json_(fd, 403, {{"error", "another site"}});
            } else if (request.method == "GET" && (request.path == "/" || request.path == "/index.html")) {
                respond_(fd, 200, "text/html; charset=utf-8", std::string(cluster::templates::WEB_PAGE),
                    "Content-Security-Policy: default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src 'self' data:\r\n");
            } else if (post && request.path == "/api/login") {
                const cluster::Json body = cluster::Json::parse(request.body.empty() ? "{}" : request.body, nullptr, false);
                const std::string typed = body.is_object() ? body.value("pin", std::string()) : std::string();
                std::string digits;
                for (const char c: typed)
                    if (std::isdigit(static_cast<unsigned char>(c))) digits += c;
                std::unique_lock<std::mutex> lock(this->_mutex);
                this->expire_();
                if (!this->_token.empty() && cookie_(request, "cc_web") != this->_token) {
                    json_(fd, 423, {{"error", "already used by " + this->_client + ": one browser at a time"}});
                } else if (digits != this->_pin) {
                    if (++this->_failures >= 5) {
                        this->newPin_();
                        this->_manager.notice(GLOBAL_ID, "web: 5 wrong codes (last from " + peer + "), new code " + this->_pin.substr(0, 3) + " "
                            + this->_pin.substr(3), true);
                    }
                    lock.unlock();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    json_(fd, 401, {{"error", "wrong code"}});
                } else {
                    this->_failures = 0;
                    this->_token = random_hex_(24);
                    this->_client = peer;
                    this->_seen = now_ms_();
                    this->_manager.notice(GLOBAL_ID, "web: " + peer + " connected");
                    json_(fd, 200, {{"ok", true}}, "Set-Cookie: cc_web=" + this->_token + "; Path=/; HttpOnly; SameSite=Strict\r\n");
                }
            } else if (request.path.starts_with("/api/")) {
                bool connected = false;
                {
                    std::lock_guard<std::mutex> lock(this->_mutex);
                    this->expire_();
                    connected = !this->_token.empty() && cookie_(request, "cc_web") == this->_token && peer == this->_client;
                    if (connected) this->_seen = now_ms_();
                    if (connected && post && request.path == "/api/logout") {
                        this->_manager.notice(GLOBAL_ID, "web: " + peer + " left");
                        this->_token.clear();
                        this->_client.clear();
                    }
                }
                if (!connected) {
                    json_(fd, 401, {{"error", "not connected"}});
                } else if (request.path == "/api/logout") {
                    json_(fd, 200, {{"ok", true}}, "Set-Cookie: cc_web=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict\r\n");
                } else if (request.method == "GET" && request.path == "/api/state") {
                    // waits for a change (15 s at most): the page asks again at once, no polling
                    std::uint64_t seen = 0;
                    try {
                        seen = request.query.contains("v") ? std::stoull(request.query.at("v")) : 0;
                    } catch (const std::exception&) {}
                    const std::int64_t end = now_ms_() + 15000;
                    while (this->_running && this->_manager.version() == seen && now_ms_() < end)
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    const std::string wanted = request.query.contains("session") && !request.query.at("session").empty() ? request.query.at("session") : GLOBAL_ID;
                    cluster::Json sessions = cluster::Json::array();
                    for (const cluster::Snapshot& snap: this->_manager.list(true))
                        sessions.push_back(this->_manager.toJson(snap, false));
                    json_(fd, 200, {{"version", this->_manager.version()}, {"sessions", sessions}, {"session", session_json_(this->_manager, wanted)},
                        {"client", peer}});
                } else if (post && request.path == "/api/cmd") {
                    const cluster::Json body = cluster::Json::parse(request.body.empty() ? "{}" : request.body, nullptr, false);
                    const std::string cmd = body.is_object() ? body.value("cmd", std::string()) : "";
                    if (!allowed.contains(cmd)) json_(fd, 400, {{"ok", false}, {"error", "command not allowed from the web: '" + cmd + "'"}});
                    else json_(fd, 200, cluster::Control::handle(this->_manager, body));
                } else {
                    json_(fd, 404, {{"error", "unknown"}});
                }
            } else {
                respond_(fd, 404, "text/plain", "not found\n");
            }
        }
    } catch (const std::exception& e) {
        json_(fd, 500, {{"error", e.what()}});
    }
    ::close(fd);
    --this->_busy;
}

/* functions */
_cold std::string cluster::web_line(const cluster::WebStatus& status)
{
    if (!status.on) return status.error.empty() ? "" : "web: port not opened (" + status.error + ")";
    return "web " + status.url + " · code " + status.pin + " · " + (status.client.empty() ? std::string("free") : status.client + " connected");
}
