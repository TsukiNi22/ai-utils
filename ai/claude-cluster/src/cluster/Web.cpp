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
#include "cluster/Actions.hpp"
#include "cluster/Display.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Icons.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Wire.hpp"
#include "cluster/Web.hpp"
#ifdef CLUSTER_TLS
    #include <openssl/x509.h>
    #include <openssl/ssl.h>
    #include <openssl/pem.h>
    #include <openssl/err.h>
#endif
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <functional>
#include <algorithm>
#include <ifaddrs.h>
#include <unistd.h>
#include <net/if.h>
#include <iterator>
#include <cstring>
#include <sstream>
#include <fstream>
#include <random>
#include <chrono>
#include <array>
#include <set>
#include <map>

/* tools */
// One connection: plain, or TLS (https: the microphone of the browsers needs it)
struct Conn {
    int fd = -1;
    #ifdef CLUSTER_TLS
        SSL* ssl = nullptr;
    #endif
};

struct Request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;     // names in lower case
    std::string body;
};

_cold static std::int64_t now_ms_(void)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); // a date: the devices keep it
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

_cold static ssize_t conn_read_(Conn& conn, char* data, const std::size_t size)
{
    #ifdef CLUSTER_TLS
        if (conn.ssl != nullptr) return ::SSL_read(conn.ssl, data, static_cast<int>(size));
    #endif
    return ::recv(conn.fd, data, size, 0);
}

_cold static bool send_all_(Conn& conn, const std::string& data)
{
    std::size_t sent = 0;
    while (sent < data.size()) {
        ssize_t size = 0;
        #ifdef CLUSTER_TLS
            if (conn.ssl != nullptr) size = ::SSL_write(conn.ssl, data.data() + sent, static_cast<int>(data.size() - sent));
            else size = ::send(conn.fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        #else
            size = ::send(conn.fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        #endif
        if (size <= 0) return false;
        sent += static_cast<std::size_t>(size);
    }
    return true;
}

_cold static bool read_request_(Conn& conn, Request& request)
{
    // One request: its headers (16 KB at most, 10 s), its body (Content-Length: 1 MB, a recording 16 MB, 60 s); the
    // socket reads time out by themselves (SO_RCVTIMEO)
    std::array<char, 16384> chunk{};
    std::string data;
    std::int64_t end = now_ms_() + 10000;
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
            if (length > (request.path == "/api/listen" ? 16000000U : 1000000U)) return false;
            if (length > 1000000) end = now_ms_() + 60000;
        }
        if (head != std::string::npos && data.size() >= head + 4 + length) {
            request.body = data.substr(head + 4, length);
            return !request.method.empty();
        }
        if (head == std::string::npos && data.size() > 16384) return false;
        if (now_ms_() > end) return false;
        const ssize_t size = conn_read_(conn, chunk.data(), chunk.size());
        if (size <= 0) return false;
        data.append(chunk.data(), static_cast<std::size_t>(size));
    }
}

_cold static void respond_(Conn& conn, const int code, const std::string& type, const std::string& body, const std::string& extra = "")
{
    static const std::map<int, std::string> reasons = {{200, "OK"}, {400, "Bad Request"}, {401, "Unauthorized"}, {403, "Forbidden"},
        {301, "Moved Permanently"}, {404, "Not Found"}, {423, "Locked"}, {500, "Internal Server Error"}};
    const std::string reason = reasons.contains(code) ? reasons.at(code) : "Error";
    (void)send_all_(conn, "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size())
        + "\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\n" + extra
        + "Connection: close\r\n\r\n" + body);
}

_cold static void json_(Conn& conn, const int code, const cluster::Json& body, const std::string& extra = "")
{
    respond_(conn, code, "application/json; charset=utf-8", body.dump(-1, ' ', false, cluster::Json::error_handler_t::replace), extra);
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

_cold static bool tailnet_(const std::string& peer)
{
    // the addresses of a tailscale network: 100.64.0.0/10 (shared address space)
    in_addr address{};
    if (::inet_pton(AF_INET, peer.c_str(), &address) != 1) return false;
    return (ntohl(address.s_addr) & 0xFFC00000u) == 0x64400000u;
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

_cold static std::vector<std::string> lan_addresses_(void)
{
    // every IPv4 address of this computer (the certificate names them all)
    std::vector<std::string> addresses;
    ifaddrs* list = nullptr;
    if (::getifaddrs(&list) == -1) return addresses;
    for (const ifaddrs* it = list; it != nullptr; it = it->ifa_next) {
        if (it->ifa_addr == nullptr || it->ifa_addr->sa_family != AF_INET || !(it->ifa_flags & IFF_UP) || (it->ifa_flags & IFF_LOOPBACK)) continue;
        std::array<char, INET_ADDRSTRLEN> text{};
        // xstyle: ignore-next LU-SOCKET (the addresses of the interfaces read, no socket)
        ::inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in*>(it->ifa_addr)->sin_addr, text.data(), text.size());
        if (std::find(addresses.begin(), addresses.end(), text.data()) == addresses.end()) addresses.emplace_back(text.data());
    }
    ::freeifaddrs(list);
    std::sort(addresses.begin(), addresses.end());
    return addresses;
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
        case cluster::EntryKind::Display: return "display";
        default: return "error";
    }
}

_cold static cluster::Json session_json_(cluster::Manager& manager, const std::string& id)
{
    // the session shown: its state, permissions, questions and the last 150 entries of its transcript
    const std::optional<cluster::Snapshot> snap = manager.snapshot(id);
    if (!snap) return nullptr;
    cluster::Json json = manager.toJson(*snap, false);
    if (snap->state == cluster::State::Working) json["turn"] = cluster::turn_state(*snap); // its step, the tools running, the tokens
    json["messages"] = cluster::Json::array();
    const std::size_t start = snap->entries.size() > 150 ? snap->entries.size() - 150 : 0;
    for (std::size_t i = start; i < snap->entries.size(); ++i) {
        const cluster::Entry& e = snap->entries[i];
        if (e.kind == cluster::EntryKind::Thinking) continue;
        if (e.kind == cluster::EntryKind::Display) {
            // its pictures left out (asked once by the page: /api/display), the rest whole
            cluster::Json light = cluster::Json::parse(e.text, nullptr, false);
            if (!light.is_object()) continue;
            for (const char* heavy: {"data", "svg", "png"})
                if (light.erase(heavy) > 0) light["heavy"] = true;
            if (light.contains("files") && light["files"].is_array())
                for (cluster::Json& f: light["files"])
                    if (f.is_object() && f.erase("preview") > 0) light["heavy"] = true;
            json["messages"].push_back({{"kind", "display"}, {"text", light.dump()}, {"tool", ""}, {"time", e.time}, {"index", i}});
            continue;
        }
        const bool long_text = e.kind == cluster::EntryKind::Assistant || e.kind == cluster::EntryKind::User;
        std::string text = e.text.size() > (long_text ? 30000 : 600) ? e.text.substr(0, long_text ? 30000 : 600) + " [...]" : e.text;
        json["messages"].push_back({{"kind", kind_name_(e.kind)}, {"text", text}, {"tool", e.tool}, {"time", e.time}});
    }
    return json;
}

/* constructor */
_cold cluster::Web::Web(cluster::Manager& manager, cluster::Voice* voice, const std::optional<bool> force)
    : _manager(manager), _voice(voice), _speech(voice != nullptr ? std::make_unique<cluster::WebVoice>(manager, *voice) : nullptr), _force(force)
{
    // its trusted and banned devices: known (and edited) while it is closed too
    this->loadTrusted_();
    this->loadBanned_();
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
    if (this->_speech) this->_speech->tick();
    const std::string remote = config.web.remote + "|" + config.web.remoteHost + "|" + std::to_string(config.web.remotePort);
    if (wanted && (this->_fd == -1 || this->_port != config.web.port || this->_httpsWanted != config.web.https || this->_remoteWanted != remote)) {
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
        if (error != this->_error) {
            this->_manager.notice(GLOBAL_ID, "web access: port " + std::to_string(port) + " not opened (" + error + ")", true);
            (void)this->_manager.fail("Open the web access", "port " + std::to_string(port) + " of this computer", error
                + (errno == EADDRINUSE ? " (another program uses it: change Port in the settings, or stop it)" : ""), [m = &this->_manager]() {if (m->web() != nullptr) m->web()->reopen();});
        }
        this->_error = error;
        return;
    }
    this->_error.clear();
    this->_fd = fd;
    // its steps in a popup (the router: seconds)
    cluster::Busy busy(this->_manager, "Opening the web access");
    cluster::busy_step("port " + std::to_string(port) + " opened on this computer");
    this->_httpsWanted = this->_manager.config().web.https;
    this->remote_(); // its names first: in the certificate
    if (this->_httpsWanted) cluster::busy_step("its certificate (https)");
    const std::string tls = this->_httpsWanted ? this->tls_() : "";
    if (!tls.empty()) this->_manager.notice(GLOBAL_ID, "web access: no https (" + tls + "), plain http: no microphone in the page", true);
    this->newPin_();
    this->loadTrusted_();
    this->loadBanned_();
    this->_running = true;
    this->_thread = std::thread(&cluster::Web::serve_, this);
    this->_manager.notice(GLOBAL_ID, "web access open: " + std::string(this->_tls != nullptr ? "https://" : "http://") + lan_address_() + ":"
        + std::to_string(port) + " code " + this->_pin.substr(0, 3) + " " + this->_pin.substr(3) + " (this network only; each new device approved here)");
    if (!this->_remoteState.empty()) this->_manager.notice(GLOBAL_ID, "web access from outside: " + this->_remoteState, !this->_remoteOk);
    busy.end(true, "web access open: " + (this->_remoteOk && !this->_remoteUrl.empty() ? this->_remoteUrl + " (outside), " : std::string())
        + (this->_tls != nullptr ? "https://" : "http://") + lan_address_() + ":" + std::to_string(port));
    const std::string remote = this->_manager.config().web.remote;
    if (remote != "off" && !this->_remoteOk) // asked but not reached: its popup (Retry: the router asked again)
        (void)this->_manager.fail(remote == "router" ? "Open the port of the router (the web access from outside)" : "Reach this computer by tailscale (the web access from outside)",
            remote == "router" ? "the router (UPnP, upnpc)" : "tailscale", this->_remoteWhy, [m = &this->_manager]() {if (m->web() != nullptr) m->web()->reopen();});
}

_cold void cluster::Web::reopen(void)
{
    std::lock_guard<std::mutex> port(this->_portMutex);
    this->_remoteWanted.clear(); // its next tick: opened again
    this->_error.clear();
}

_cold static std::string upnp_why_(const cluster::Captured& map)
{
    // why the router did not open its port: the line of upnpc telling it
    for (const std::string& line: cluster::split(map.out, '\n')) {
        const std::string lower = cluster::lower(line);
        if (lower.find("failed") != std::string::npos || lower.find("no igd") != std::string::npos || lower.find("no valid") != std::string::npos
            || lower.find("error") != std::string::npos)
            return cluster::trim(line) + (lower.find("no igd") != std::string::npos ? " (UPnP off in the box: turn it on in its admin page)" : "");
    }
    return map.code != 0 ? "upnpc exit " + std::to_string(map.code) : "the router answered without opening it (its UPnP refused)";
}

_cold void cluster::Web::remote_(void)
{
    // tailscale: its address and its name (MagicDNS); router: the port opened by UPnP, its public address (or the name given)
    const cluster::WebConf& conf = this->_manager.config().web;
    this->_remoteWanted = conf.remote + "|" + conf.remoteHost + "|" + std::to_string(conf.remotePort);
    this->_remoteNames.clear();
    this->_remoteUrl.clear();
    this->_remoteState.clear();
    this->_remoteOk = false;
    this->_remoteWhy.clear();
    if (conf.remote == "tailscale") {
        cluster::busy_step("tailscale: its address");
        if (!cluster::has_command("tailscale")) {
            this->_remoteState = "tailscale is not installed: install it on this computer and on the phone, log both in (tailscale up)";
            this->_remoteWhy = this->_remoteState;
            return;
        }
        const std::string ip = cluster::trim(cluster::split(cluster::capture({"tailscale", "ip", "-4"}, "", 10).out + "\n", '\n')[0]);
        if (ip.empty() || !tailnet_(ip)) {
            this->_remoteState = "tailscale is not connected (tailscale up)";
            this->_remoteWhy = "tailscale is not connected: log this computer in (sudo tailscale up)";
            return;
        }
        this->_remoteNames.push_back(ip);
        const cluster::Json status = cluster::Json::parse(cluster::capture({"tailscale", "status", "--json"}, "", 10).out, nullptr, false);
        std::string name = status.is_object() && status.contains("Self") && status["Self"].is_object() ? status["Self"].value("DNSName", std::string()) : "";
        while (name.ends_with(".")) name.pop_back();
        if (!name.empty()) this->_remoteNames.push_back(cluster::lower(name));
        this->_remoteUrl = "https://" + (name.empty() ? ip : cluster::lower(name)) + ":" + std::to_string(this->_port);
        this->_remoteState = this->_remoteUrl + " (tailscale: the devices of your tailnet, end to end)";
        this->_remoteOk = true;
    } else if (conf.remote == "router") {
        const int outside = conf.remotePort > 0 ? conf.remotePort : this->_port;
        std::string address;
        if (cluster::has_command("upnpc")) {
            cluster::busy_step("asking the router to open its port " + std::to_string(outside) + " (UPnP)");
            const cluster::Captured map = cluster::capture({"upnpc", "-e", "claude-cluster", "-r", std::to_string(this->_port), std::to_string(outside), "TCP"}, "", 20);
            for (const std::string& line: cluster::split(map.out, '\n'))
                cluster::busy_log(line);
            if (map.code == 0 && map.out.find("is redirected") != std::string::npos) this->_mapped = outside;
            else this->_remoteWhy = upnp_why_(map);
            cluster::busy_step("its public address (UPnP)");
            for (const std::string& line: cluster::split(cluster::capture({"upnpc", "-s"}, "", 20).out, '\n'))
                if (line.starts_with("ExternalIPAddress = ")) address = cluster::trim(line.substr(20));
        } else {
            this->_remoteWhy = "upnpc missing: the port of the router can't be opened by itself (install miniupnpc, or open it by hand in the box)";
        }
        if (!conf.remoteHost.empty()) this->_remoteNames.push_back(cluster::lower(conf.remoteHost));
        if (!address.empty()) this->_remoteNames.push_back(address);
        const std::string name = !conf.remoteHost.empty() ? cluster::lower(conf.remoteHost) : address;
        if (!name.empty()) this->_remoteUrl = "https://" + name + ":" + std::to_string(outside);
        this->_remoteState = (this->_remoteUrl.empty() ? std::string("no public address known (upnpc missing or refused; [web] remote_host: a dynamic DNS name)")
            : this->_remoteUrl) + (this->_mapped > 0 ? " (the router opened its port " + std::to_string(outside) + " to this computer by UPnP)"
            : " (the router did not open its port" + (this->_remoteWhy.empty() ? std::string() : ": " + this->_remoteWhy) + "; open the port " + std::to_string(outside)
            + " of your router to " + lan_address_() + ":" + std::to_string(this->_port) + ")") + "; open to the Internet: each new device approved here";
        this->_remoteOk = this->_mapped > 0 || !conf.remoteHost.empty(); // a name given: its port opened by hand
        if (!this->_remoteOk && this->_remoteWhy.empty()) this->_remoteWhy = "no public address known";
        if (!this->_remoteOk) this->_remoteWhy += "; open the port " + std::to_string(outside) + " of the router to " + lan_address_() + ":" + std::to_string(this->_port)
            + " in its admin page, or give its name (Name from outside) once done";
        if (!this->_remoteOk) this->_remoteState = "not reached from outside: " + this->_remoteWhy;
    }
}

_cold bool cluster::Web::allowed_(const std::string& peer) const
{
    // this LAN; from outside: the tailnet (tailscale), any address (router), https only
    if (same_lan_(peer)) return true;
    const std::string& remote = this->_manager.config().web.remote;
    if (this->_tls == nullptr || remote == "off") return false;
    return remote == "router" || (remote == "tailscale" && tailnet_(peer));
}

_cold void cluster::Web::close_(void)
{
    if (this->_fd == -1) return;
    this->_running = false;
    if (this->_thread.joinable()) this->_thread.join();
    for (int i = 0; i < 50 && this->_busy > 0; ++i) // the requests being served end (a waiting state returns within 0.2 s)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (this->_mapped > 0) { // the port of the router closed again
        (void)cluster::capture({"upnpc", "-d", std::to_string(this->_mapped), "TCP"}, "", 20);
        this->_mapped = 0;
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    ::close(this->_fd);
    this->_fd = -1;
    #ifdef CLUSTER_TLS
        if (this->_tls != nullptr) ::SSL_CTX_free(static_cast<SSL_CTX*>(this->_tls));
    #endif
    this->_tls = nullptr;
    this->_access.clear();
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

_cold std::string cluster::Web::tls_(void)
{
    // a self-signed certificate made once (config folder / web, its key 0600): the browser asks to accept it the first
    // time; its SHA-256 shown here to compare with the one of the browser
    #ifdef CLUSTER_TLS
        const std::filesystem::path dir = cluster::config_dir() / "web";
        const std::filesystem::path cert = dir / "cert.pem";
        const std::filesystem::path key = dir / "key.pem";
        const std::filesystem::path ca = dir / "ca.pem";
        const std::filesystem::path caKey = dir / "ca-key.pem";
        const std::filesystem::path names = dir / "cert-names.txt"; // the names the certificate was made for
        std::error_code error;
        std::filesystem::create_directories(dir, error);
        ::chmod(dir.c_str(), S_IRWXU);
        // the authority of this computer: made once (trusted once per device, it stays)
        if (!std::filesystem::exists(ca) || !std::filesystem::exists(caKey)) {
            std::array<char, 256> own{};
            (void)::gethostname(own.data(), own.size() - 1);
            const cluster::Captured made = cluster::capture({"openssl", "req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:prime256v1", "-nodes",
                "-days", "7300", "-subj", "/O=claude-cluster/CN=claude-cluster on " + std::string(own[0] != '\0' ? own.data() : "this computer"),
                "-addext", "basicConstraints=critical,CA:TRUE,pathlen:0", "-addext", "keyUsage=critical,keyCertSign,cRLSign", "-keyout", caKey.string(),
                "-out", ca.string()}, "", 30);
            ::chmod(caKey.c_str(), S_IRUSR | S_IWUSR);
            if (made.code != 0) return "openssl could not make its authority: " + cluster::one_line(made.out, 160);
            std::filesystem::remove(cert, error); // the old self-signed one: signed by the authority now
        }
        // the certificate of the server: every address of this computer, signed by the authority; made again when they
        // change or 30 days before its end (397 days: what the browsers accept)
        std::array<char, 256> own{};
        (void)::gethostname(own.data(), own.size() - 1);
        std::string wanted = "IP:127.0.0.1,DNS:localhost";
        for (const std::string& address: lan_addresses_())
            wanted += ",IP:" + address;
        if (own[0] != '\0') wanted += ",DNS:" + std::string(own.data()) + ",DNS:" + std::string(own.data()) + ".local";
        for (const std::string& name: this->_remoteNames) { // reached from outside: by these names too
            in_addr address{};
            const std::string entry = (::inet_pton(AF_INET, name.c_str(), &address) == 1 ? "IP:" : "DNS:") + name;
            if (("," + wanted + ",").find("," + entry + ",") == std::string::npos) wanted += "," + entry;
        }
        std::ifstream in(names);
        const std::string made((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const bool ending = cluster::capture({"openssl", "x509", "-checkend", std::to_string(30 * 86400), "-noout", "-in", cert.string()}, "", 10).code != 0;
        if (!std::filesystem::exists(cert) || !std::filesystem::exists(key) || made != wanted || ending) {
            const std::filesystem::path request = dir / "cert.csr";
            const std::filesystem::path extensions = dir / "cert.ext";
            std::ofstream(extensions) << "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\nsubjectAltName=" << wanted << "\n";
            const cluster::Captured req = cluster::capture({"openssl", "req", "-new", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:prime256v1", "-nodes",
                "-subj", "/O=claude-cluster/CN=" + std::string(own[0] != '\0' ? own.data() : "claude-cluster"), "-keyout", key.string(), "-out", request.string()}, "", 30);
            ::chmod(key.c_str(), S_IRUSR | S_IWUSR);
            const cluster::Captured sign = req.code != 0 ? req : cluster::capture({"openssl", "x509", "-req", "-in", request.string(), "-CA", ca.string(), "-CAkey",
                caKey.string(), "-CAcreateserial", "-days", "397", "-sha256", "-extfile", extensions.string(), "-out", cert.string()}, "", 30);
            std::filesystem::remove(request, error);
            std::filesystem::remove(extensions, error);
            if (sign.code != 0) return "openssl could not make its certificate: " + cluster::one_line(sign.out, 160);
            std::ofstream(names) << wanted;
        }
        SSL_CTX* context = ::SSL_CTX_new(::TLS_server_method());
        if (context == nullptr) return "OpenSSL";
        (void)::SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
        if (::SSL_CTX_use_certificate_chain_file(context, cert.c_str()) != 1 || ::SSL_CTX_use_PrivateKey_file(context, key.c_str(), SSL_FILETYPE_PEM) != 1) {
            ::SSL_CTX_free(context);
            std::filesystem::remove(cert, error);   // made again at the next opening
            std::filesystem::remove(key, error);
            return "its certificate unreadable (" + cluster::short_path(dir.string()) + "), made again at the next opening";
        }
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        unsigned int size = 0;
        std::string fingerprint;
        if (::X509_digest(::SSL_CTX_get0_certificate(context), ::EVP_sha256(), digest.data(), &size) == 1)
            for (unsigned int i = 0; i < size; ++i) {
                static const char* hex = "0123456789ABCDEF";
                fingerprint += std::string(i ? ":" : "") + hex[digest[i] >> 4] + hex[digest[i] & 15];
            }
        this->_fingerprint = fingerprint;
        this->_tls = context;
        return "";
    #else
        return "not built with OpenSSL";
    #endif
}

/* access */
_cold void cluster::Web::newPin_(void)
{
    this->_pin = random_digits_(6);
    this->_failures = 0;
}

_cold void cluster::Web::expire_(void)
{
    // connected: gone after 30 s without a request (its page asks every 15 s); waiting: 3 min; denied: told, then gone
    const std::int64_t now = now_ms_();
    for (auto it = this->_access.begin(); it != this->_access.end();) {
        const cluster::WebDevice& d = it->second.device;
        const bool gone = (d.state == "connected" && now - d.seen > 30000) || (d.state == "waiting" && now - d.since > 180000) || (d.state == "denied" && now - d.since > 60000);
        if (gone && d.state == "connected") this->_manager.notice(GLOBAL_ID, "web: " + d.name + " (" + d.address + ") gone (nothing for 30 s)");
        if (gone && d.state == "waiting") this->_manager.notice(GLOBAL_ID, "web: " + d.name + " (" + d.address + ") not approved in time");
        it = gone ? this->_access.erase(it) : std::next(it);
    }
}

_cold static std::string device_name_(const std::string& agent)
{
    // its browser and its system, from its user agent
    const std::function<bool(const char*)> has = [&agent](const char* word) {return agent.find(word) != std::string::npos;};
    const std::string browser = has("claude-cluster-app") ? "the app" : has("Edg/") ? "Edge" : has("Firefox/") ? "Firefox" : has("OPR/") ? "Opera"
        : has("Chrome/") ? "Chrome" : has("Safari/") ? "Safari" : "a browser";
    const std::string system = has("Android") ? "Android" : has("iPhone") ? "iPhone" : has("iPad") ? "iPad" : has("Windows") ? "Windows"
        : has("Mac OS") ? "macOS" : has("Linux") ? "Linux" : "";
    return browser + (system.empty() ? "" : " · " + system);
}

_cold void cluster::Web::loadTrusted_(void)
{
    this->_trusted.clear();
    const std::optional<cluster::Json> saved = cluster::read_json(cluster::config_dir() / "web" / "trusted.json");
    if (!saved || !saved->is_array()) return;
    for (const cluster::Json& d: *saved) {
        Access access;
        access.device = {d.value("id", std::string()), d.value("name", std::string()), d.value("agent", std::string()), d.value("address", std::string()),
            "trusted", true, d.value("since", static_cast<std::int64_t>(0)), d.value("seen", static_cast<std::int64_t>(0))};
        access.secret = d.value("secret", std::string());
        if (!access.device.id.empty() && access.secret.size() >= 32) this->_trusted[access.device.id] = access;
    }
}

_cold void cluster::Web::saveTrusted_(void) const
{
    // the secrets of the trusted devices: this user only
    cluster::Json saved = cluster::Json::array();
    for (const auto &[id, access]: this->_trusted)
        saved.push_back({{"id", id}, {"name", access.device.name}, {"agent", access.device.agent}, {"address", access.device.address}, {"since", access.device.since},
            {"seen", access.device.seen}, {"secret", access.secret}});
    const std::filesystem::path path = cluster::config_dir() / "web" / "trusted.json";
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    cluster::write_json(path, saved);
    ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
}

_cold void cluster::Web::loadBanned_(void)
{
    this->_banned.clear();
    const std::optional<cluster::Json> saved = cluster::read_json(cluster::config_dir() / "web" / "banned.json");
    if (!saved || !saved->is_array()) return;
    for (const cluster::Json& d: *saved)
        if (!d.value("id", std::string()).empty())
            this->_banned[d.value("id", std::string())] = {d.value("id", std::string()), d.value("name", std::string()), d.value("agent", std::string()),
                d.value("address", std::string()), "banned", false, d.value("since", static_cast<std::int64_t>(0)), 0};
}

_cold void cluster::Web::saveBanned_(void) const
{
    cluster::Json saved = cluster::Json::array();
    for (const auto &[id, d]: this->_banned)
        saved.push_back({{"id", id}, {"name", d.name}, {"agent", d.agent}, {"address", d.address}, {"since", d.since}});
    const std::filesystem::path path = cluster::config_dir() / "web" / "banned.json";
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    cluster::write_json(path, saved);
    ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
}

_cold bool cluster::Web::banned_(const std::string& id, const std::string& peer) const
{
    // its device; from outside the LAN its address too (a new cookie does not get it back in); on the LAN the code still asked
    if (!id.empty() && this->_banned.contains(id)) return true;
    if (same_lan_(peer)) return false;
    return std::any_of(this->_banned.begin(), this->_banned.end(), [&peer](const std::pair<const std::string, cluster::WebDevice>& b) {return b.second.address == peer;});
}

_cold void cluster::Web::ban(const std::vector<std::string>& ids)
{
    std::vector<std::string> out;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (const std::string& id: ids) {
            cluster::WebDevice d;
            if (auto a = this->_access.find(id); a != this->_access.end()) d = a->second.device;
            else if (auto t = this->_trusted.find(id); t != this->_trusted.end()) d = t->second.device;
            else continue;
            d.state = "banned";
            d.trusted = false;
            d.since = now_ms_();
            this->_banned[id] = d;
            this->_trusted.erase(id);
            out.push_back(id);
            this->_manager.notice(GLOBAL_ID, "web: " + d.name + " (" + d.address + ") banned (Web devices: unban it)");
        }
        if (out.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no such device");
        this->saveTrusted_();
        this->saveBanned_();
    }
    this->kick(out);
}

_cold void cluster::Web::unban(const std::vector<std::string>& ids)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    for (auto it = this->_banned.begin(); it != this->_banned.end();) {
        if (!ids.empty() && std::find(ids.begin(), ids.end(), it->first) == ids.end()) {
            ++it;
            continue;
        }
        this->_manager.notice(GLOBAL_ID, "web: " + it->second.name + " unbanned (its code asked again)");
        it = this->_banned.erase(it);
    }
    this->saveBanned_();
}

_cold void cluster::Web::ping(const std::string& id, const int rtt)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    auto it = this->_access.find(id);
    if (it == this->_access.end() || rtt < 0 || rtt > 120000) return;
    int& ping = it->second.device.ping;
    ping = ping < 0 ? rtt : (ping * 2 + rtt) / 3; // smoothed: a slow request does not jump it
}

_cold void cluster::Web::ask_(const std::string& id)
{
    // a device gave the code: the host says yes, yes and remember it, or no (the popup of the interface, the notification)
    const Access& access = this->_access.at(id);
    const cluster::WebDevice d = access.device;
    this->_manager.notice(GLOBAL_ID, "web: " + d.name + " (" + d.address + ") asks to connect: allow it here (its popup, the palette, the notification)");
    cluster::Notification n{"web-" + id, "web access · " + d.name + " asks to connect", d.address + " · " + cluster::one_line(d.agent, 120), cluster::Face::Attentive, true,
        "message-new-instant", {{"allow", "Allow"}, {"trust", "Allow and trust"}, {"deny", "Deny"}}};
    n.onAction = [this, id](const std::string& answer) {this->approve(id, answer);};
    std::thread([this, n]() {(void)this->_manager.notify(n, "web");}).detach();
}

_cold void cluster::Web::setOn(const bool on)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_force = on;
}

_cold void cluster::Web::kick(const std::vector<std::string>& ids)
{
    // these devices out (their pages back to the code); all of them: a new code too
    std::lock_guard<std::mutex> lock(this->_mutex);
    std::vector<std::string> names;
    for (auto it = this->_access.begin(); it != this->_access.end();) {
        if (!ids.empty() && std::find(ids.begin(), ids.end(), it->first) == ids.end()) {
            ++it;
            continue;
        }
        names.push_back(it->second.device.name + " (" + it->second.device.address + ")");
        it = this->_access.erase(it);
    }
    if (ids.empty()) this->newPin_();
    std::string list;
    for (const std::string& name: names)
        list += (list.empty() ? "" : ", ") + name;
    this->_manager.notice(GLOBAL_ID, "web: " + (list.empty() ? std::string("nobody connected") : list + " disconnected from here")
        + (ids.empty() ? ", new code " + this->_pin.substr(0, 3) + " " + this->_pin.substr(3) : std::string()));
}

_cold void cluster::Web::approve(const std::string& id, const std::string& answer)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    auto it = this->_access.find(id);
    if (it == this->_access.end() || it->second.device.state != "waiting")
        throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no device waiting: " + id);
    cluster::WebDevice& d = it->second.device;
    if (answer == "deny") {
        d.state = "denied";
        d.since = now_ms_();
        this->_manager.notice(GLOBAL_ID, "web: " + d.name + " (" + d.address + ") denied");
        return;
    }
    if (answer != "allow" && answer != "trust")
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "allow | trust | deny");
    d.state = "connected";
    d.since = d.seen = now_ms_();
    if (answer == "trust") { // remembered: no code, no approval next time (until removed)
        d.trusted = true;
        it->second.secret = random_hex_(32);
        it->second.giveTrust = true;
        Access remembered = it->second;
        remembered.device.state = "trusted";
        this->_trusted[id] = remembered;
        this->saveTrusted_();
    }
    this->_manager.notice(GLOBAL_ID, "web: " + d.name + " (" + d.address + ") connected" + (d.trusted ? " and trusted" : ""));
}

_cold void cluster::Web::untrust(const std::vector<std::string>& ids)
{
    std::vector<std::string> removed;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        for (auto it = this->_trusted.begin(); it != this->_trusted.end();) {
            if (!ids.empty() && std::find(ids.begin(), ids.end(), it->first) == ids.end()) {
                ++it;
                continue;
            }
            removed.push_back(it->first);
            this->_manager.notice(GLOBAL_ID, "web: " + it->second.device.name + " trusted no more");
            it = this->_trusted.erase(it);
        }
        this->saveTrusted_();
    }
    if (!removed.empty()) this->kick(removed);
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
    if (!status.on) { // closed: its trusted and banned devices still listed (edited while closed)
        for (const auto &[id, access]: this->_trusted)
            status.trusted.push_back(access.device);
        for (const auto &[id, d]: this->_banned)
            status.banned.push_back(d);
        return status;
    }
    status.url = std::string(this->_tls != nullptr ? "https://" : "http://") + lan_address_() + ":" + std::to_string(this->_port);
    status.https = this->_tls != nullptr;
    status.fingerprint = this->_fingerprint;
    status.remoteUrl = this->_remoteUrl;
    status.remoteState = this->_remoteState;
    status.remoteOk = this->_remoteOk;
    status.pin = this->_pin.substr(0, 3) + " " + this->_pin.substr(3);
    for (const auto &[id, access]: this->_access) {
        status.devices.push_back(access.device);
        if (access.device.state == "connected") status.client += (status.client.empty() ? "" : ", ") + access.device.name;
    }
    for (const auto &[id, access]: this->_trusted)
        status.trusted.push_back(access.device);
    for (const auto &[id, d]: this->_banned)
        status.banned.push_back(d);
    int sum = 0;
    int measured = 0;
    for (const cluster::WebDevice& d: status.devices)
        if (d.state == "connected" && d.ping >= 0) {
            sum += d.ping;
            ++measured;
        }
    status.ping = measured > 0 ? sum / measured : -1;
    return status;
}

/* requests */
_cold static cluster::CompletionKind completion_kind_(const std::string& name)
{
    static const std::map<std::string, cluster::CompletionKind> kinds = {{"file", cluster::CompletionKind::File},
        {"folder", cluster::CompletionKind::Folder}, {"command", cluster::CompletionKind::Command}, {"skill", cluster::CompletionKind::Skill}, {"mode", cluster::CompletionKind::Mode},
        {"model", cluster::CompletionKind::Model}, {"effort", cluster::CompletionKind::Effort}, {"trash", cluster::CompletionKind::Trash},
        {"shell", cluster::CompletionKind::Shell}, {"ex", cluster::CompletionKind::Ex}, {"setting", cluster::CompletionKind::Setting}};
    return kinds.contains(name) ? kinds.at(name) : cluster::CompletionKind::None;
}

_cold void cluster::Web::client_(const int fd, const std::string& peer)
{
    // One request per connection; this LAN only, https when on (a plain request: sent to https)
    Conn conn{fd};
    const timeval receive{10, 0};
    const timeval send{30, 0};
    (void)::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &receive, sizeof(receive));
    (void)::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &send, sizeof(send));
    bool plain = true;
    bool ready = true;
    #ifdef CLUSTER_TLS
        if (this->_tls != nullptr && this->allowed_(peer)) {
            char first = 0;
            if (::recv(fd, &first, 1, MSG_PEEK) == 1 && first == 0x16) { // a TLS handshake
                conn.ssl = ::SSL_new(static_cast<SSL_CTX*>(this->_tls));
                ::SSL_set_fd(conn.ssl, fd);
                ready = ::SSL_accept(conn.ssl) == 1;
                plain = false;
            }
        }
    #endif
    try {
        if (!this->allowed_(peer)) respond_(conn, 403, "text/plain", "claude-cluster: this network only\n");
        else if (ready) this->request_(&conn, peer, this->_tls != nullptr && plain);
    } catch (const std::exception& e) {
        json_(conn, 500, {{"error", e.what()}});
    }
    #ifdef CLUSTER_TLS
        if (conn.ssl != nullptr) {
            (void)::SSL_shutdown(conn.ssl);
            ::SSL_free(conn.ssl);
        }
    #endif
    ::close(fd);
    --this->_busy;
}

_cold void cluster::Web::request_(void* connection, const std::string& peer, const bool toHttps)
{
    // The right host, no other site; the page, the code; the browser connected for the API
    static const std::set<std::string> allowed = {"list", "read", "status", "send", "allow", "interrupt", "spawn", "close", "rename", "describe", "resume", "apply", "mode",
        "model", "notices", "usage", "task_add", "task_list", "trash", "restore", "external", "import", "fork", "transcript", "ask", "display_act", "display_get", "settings_get", "settings_search", "settings_set", "ui_state", "delete",
        "projects", "send_next", "install_package", "failure", "services", "service_act", "task_act", "schedule_act", "schedule_add", "schedule_list", "todos", "task_cancel", "schedule_cancel"};
    Conn& conn = *static_cast<Conn*>(connection);
    Request request;
    if (!read_request_(conn, request)) return;
    const std::string host = request.headers.contains("host") ? request.headers.at("host") : "";
    const std::string origin = request.headers.contains("origin") ? request.headers.at("origin") : "";
    const bool post = request.method == "POST";
    const std::string scheme = this->_tls == nullptr ? "http://" : "https://";
    const std::string secure = this->_tls != nullptr ? "; Secure" : "";

    const std::string named = cluster::lower(host.substr(0, host.rfind(':') == std::string::npos ? host.size() : host.rfind(':')));
    const bool remoteName = std::find(this->_remoteNames.begin(), this->_remoteNames.end(), named) != this->_remoteNames.end();
    if (!host_ok_(host) && !remoteName) {
        respond_(conn, 403, "text/plain", "claude-cluster: open it by the address of this computer\n");
        return;
    }
    if (toHttps) {
        respond_(conn, 301, "text/plain", "https\n", "Location: https://" + host + request.path + "\r\n");
        return;
    }
    if (post && !origin.empty() && origin != scheme + host) {
        json_(conn, 403, {{"error", "another site"}});
        return;
    }
    if (request.method == "GET" && (request.path == "/" || request.path == "/index.html")) {
        respond_(conn, 200, "text/html; charset=utf-8", std::string(cluster::templates::WEB_PAGE),
            "Content-Security-Policy: default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src 'self' data: blob:; "
            "media-src 'self' blob:; worker-src 'self'\r\nPermissions-Policy: microphone=(self)\r\n");
        return;
    }
    if (request.path == "/api/login") {
        this->login_(connection, &request, peer, secure);
        return;
    }
    if (request.method == "GET" && request.path == "/sw.js") {
        // the service worker of the page: its notifications (Chrome on Android needs one), a click on one opens its session
        static const std::string worker = R"JS(self.addEventListener("install", () => self.skipWaiting());
self.addEventListener("activate", (e) => e.waitUntil(self.clients.claim()));
self.addEventListener("notificationclick", (e) => {
    e.notification.close();
    const session = (e.notification.data || {}).session || "";
    e.waitUntil(self.clients.matchAll({type: "window", includeUncontrolled: true}).then((all) => {
        for (const client of all) { client.postMessage({open: session}); return client.focus(); }
        return self.clients.openWindow("/#" + encodeURIComponent(session));
    }));
});
)JS";
        respond_(conn, 200, "application/javascript; charset=utf-8", worker, "Service-Worker-Allowed: /\r\n");
        return;
    }
    if (request.method == "GET" && request.path == "/ca.crt") {
        // the authority of this computer: installed once on a device, the browser trusts its pages (no warning)
        std::ifstream in(cluster::config_dir() / "web" / "ca.pem");
        const std::string pem((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (pem.empty()) respond_(conn, 404, "text/plain", "no authority (plain http)\n");
        else respond_(conn, 200, "application/x-x509-ca-cert", pem, "Content-Disposition: attachment; filename=\"claude-cluster-ca.crt\"\r\n");
        return;
    }
    const std::filesystem::path app = cluster::data_dir() / "app";
    if (request.method == "GET" && request.path == "/api/version") {
        // the app: a newer one served here (app_version), the oldest one still supported (app_min: its protocol)
        std::ifstream in(app / "version.txt");
        std::string built;
        std::getline(in, built);
        json_(conn, 200, {{"version", CLUSTER_VERSION}, {"app_min", 1}, {"app_version", cluster::trim(built)}, {"app", std::filesystem::exists(app / "claude-cluster.apk")}});
        return;
    }
    if (request.method == "GET" && request.path == "/app.apk") { // the Android app built by setup.sh (no secret in it)
        std::ifstream in(app / "claude-cluster.apk", std::ios::binary);
        const std::string apk((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (apk.empty()) respond_(conn, 404, "text/plain", "no Android app built on this computer (the Android SDK: setup.sh install claude-cluster)\n");
        else respond_(conn, 200, "application/vnd.android.package-archive", apk, "Content-Disposition: attachment; filename=\"claude-cluster.apk\"\r\n");
        return;
    }
    if (!request.path.starts_with("/api/")) {
        respond_(conn, 404, "text/plain", "not found\n");
        return;
    }
    bool connected = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->expire_();
        // its device, its cookie of this connection, from its address
        auto it = this->_access.find(cookie_(request, "cc_dev"));
        connected = it != this->_access.end() && it->second.device.state == "connected" && !it->second.token.empty() && cookie_(request, "cc_web") == it->second.token
            && it->second.device.address == peer;
        if (connected) it->second.device.seen = now_ms_();
        if (connected && post && request.path == "/api/logout") {
            this->_manager.notice(GLOBAL_ID, "web: " + it->second.device.name + " (" + peer + ") left");
            this->_access.erase(it);
        }
    }
    if (!connected) {
        json_(conn, 401, {{"error", "not connected"}});
        return;
    }
    const cluster::Json body = post && request.path != "/api/listen" ? cluster::Json::parse(request.body.empty() ? "{}" : request.body, nullptr, false)
        : cluster::Json::object();
    const std::function<std::string(const char*)> field = [&body](const char* key) {
        return body.is_object() && body.contains(key) && body[key].is_string() ? body[key].get<std::string>() : std::string();
    };

    if (request.path == "/api/logout") {
        json_(conn, 200, {{"ok", true}}, "Set-Cookie: cc_web=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict" + secure + "\r\n");
    } else if (post && request.path == "/api/ping") {
        // its round trip before this one, measured by the page (the ping of the devices)
        if (body.is_object() && body.contains("rtt") && body["rtt"].is_number()) this->ping(cookie_(request, "cc_dev"), body["rtt"].get<int>());
        json_(conn, 200, {{"ok", true}});
    } else if (request.method == "GET" && request.path == "/api/state") {
        // waits for a change (15 s at most): the page asks again at once, no polling
        std::uint64_t seen = 0;
        try {
            seen = request.query.contains("v") ? std::stoull(request.query.at("v")) : 0;
        } catch (const std::exception&) {}
        const std::int64_t end = now_ms_() + 15000;
        // the version of the cluster and of the question being answered (both only grow)
        const std::function<std::uint64_t(void)> version = [this]() {
            (void)this->_manager.asking().check();
            return this->_manager.version() + this->_manager.asking().version();
        };
        while (this->_running && version() == seen && now_ms_() < end)
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const std::string wanted = request.query.contains("session") && !request.query.at("session").empty() ? request.query.at("session") : GLOBAL_ID;
        // each session: its state, its last messages (the cards of the grid and the board), its turn
        cluster::Json sessions = cluster::Json::array();
        for (const cluster::Snapshot& snap: this->_manager.list(true)) {
            cluster::Json item = this->_manager.toJson(snap, false);
            item["origin"] = this->_manager.origin(snap.spec.id); // who is notified for it: this device or not
            cluster::Json recent = cluster::Json::array();
            for (auto it = snap.entries.rbegin(); it != snap.entries.rend() && recent.size() < 6; ++it) {
                if (it->kind != cluster::EntryKind::User && it->kind != cluster::EntryKind::Assistant && it->kind != cluster::EntryKind::Tool
                    && it->kind != cluster::EntryKind::Error) continue;
                recent.insert(recent.begin(), cluster::Json{{"kind", it->kind == cluster::EntryKind::User ? "user" : it->kind == cluster::EntryKind::Assistant ? "assistant"
                    : it->kind == cluster::EntryKind::Tool ? "tool" : "error"}, {"text", cluster::one_line(it->text, 220)}, {"tool", it->tool}});
            }
            item["recent"] = recent;
            if (snap.state == cluster::State::Working) item["turn"] = cluster::turn_state(snap);
            sessions.push_back(item);
        }
        // the colors of the style of claude-cluster: the page takes them (the same look as the terminal and the window)
        const cluster::Config& config = this->_manager.config();
        const std::string styleName = this->_manager.ui("style", config.style);
        const cluster::Style& st = config.styles.contains(styleName) ? config.styles.at(styleName) : config.currentStyle();
        const cluster::Json style = {{"name", st.name}, {"dark", st.dark}, {"bg", st.bg}, {"surface", st.surface}, {"text", st.text}, {"muted", st.muted},
            {"accent", st.accent}, {"border", st.border}, {"ok", st.ok}, {"warn", st.warn}, {"error", st.error}};
        const cluster::VoiceConf& voice = this->_manager.config().voice;
        const cluster::AppFace face = this->_manager.appFace(false);
        // the Tasks session of the list: what waits / runs, what is scheduled
        const std::vector<cluster::Task> tasks = this->_manager.tasks();
        const std::vector<cluster::Schedule> planned = this->_manager.schedules();
        const auto open = std::count_if(tasks.begin(), tasks.end(), [](const cluster::Task& t) {return t.status == "queued" || t.status == "running";});
        const auto waiting = std::count_if(planned.begin(), planned.end(), [](const cluster::Schedule& w) {return w.status == "waiting";});
        json_(conn, 200, {{"version", version()}, {"remote", this->_remoteUrl}, {"ask", this->_manager.asking().json()}, {"sessions", sessions}, {"session", session_json_(this->_manager, wanted)},
            {"tasks_open", open}, {"schedules_waiting", waiting},
            {"progress", this->_manager.progress() ? cluster::Json(*this->_manager.progress()) : cluster::Json(nullptr)}, {"failures", this->_manager.failures()},
            {"face", {{"name", cluster::face_name(face.face)}, {"text", cluster::face_text(face.face)}, {"badge", face.badge},
                {"svg", cluster::face_svg(face.face, face.badge, face.urgent)}}},
            {"client", peer}, {"device", cookie_(request, "cc_dev")}, {"style", style}, {"version_app", CLUSTER_VERSION}, {"voice", {{"available", this->_speech != nullptr}, {"confirm", voice.confirmSeconds},
                {"end_silence", voice.endSilence > 0.0 ? voice.endSilence : 1.0}, {"listen", voice.listenModel}, {"say", voice.sayEngine}}}});
    } else if (request.method == "GET" && request.path == "/api/file") {
        // a file of a file display of this session (never another path): sent to download
        const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(request.query.contains("session") ? request.query.at("session") : GLOBAL_ID);
        std::size_t index = 0;
        std::size_t which = 0;
        try {
            index = request.query.contains("index") ? std::stoul(request.query.at("index")) : 0;
            which = request.query.contains("i") ? std::stoul(request.query.at("i")) : 0;
        } catch (const std::exception&) {}
        cluster::Json file;
        if (snap && index < snap->entries.size() && snap->entries[index].kind == cluster::EntryKind::Display) {
            const cluster::Json d = cluster::Json::parse(snap->entries[index].text, nullptr, false);
            if (d.is_object() && d.contains("files") && which < d["files"].size()) file = d["files"][which];
        }
        std::error_code error;
        const std::filesystem::path path = file.is_object() ? file.value("path", std::string()) : std::string();
        if (path.empty() || !std::filesystem::is_regular_file(path, error)) {
            json_(conn, 404, {{"error", "no such file"}});
        } else if (std::filesystem::file_size(path, error) > 512ULL * 1024 * 1024) {
            json_(conn, 413, {{"error", "bigger than 512 MB"}});
        } else {
            std::ifstream in(path, std::ios::binary);
            const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            std::string name = path.filename().string();
            std::replace(name.begin(), name.end(), '"', '_');
            respond_(conn, 200, file.value("mime", std::string("application/octet-stream")), bytes, "Content-Disposition: attachment; filename=\"" + name + "\"\r\n");
        }
    } else if (request.method == "GET" && request.path == "/api/display") {
        // a display whole (its picture): the page asks it once
        const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(request.query.contains("session") ? request.query.at("session") : GLOBAL_ID);
        std::size_t index = 0;
        try {
            index = request.query.contains("index") ? std::stoul(request.query.at("index")) : 0;
        } catch (const std::exception&) {}
        if (!snap || index >= snap->entries.size() || snap->entries[index].kind != cluster::EntryKind::Display) json_(conn, 404, {{"error", "no display"}});
        else respond_(conn, 200, "application/json; charset=utf-8", snap->entries[index].text, "Cache-Control: private, max-age=86400\r\n");
    } else if (post && request.path == "/api/cmd") {
        cluster::Json command = body.is_object() ? body : cluster::Json::object();
        const std::string cmd = command.value("cmd", std::string());
        if (cmd == "spawn" && command.contains("cwd") && command["cwd"].is_string()) { // a relative folder: from the home folder
            const std::string cwd = cluster::trim(command["cwd"].get<std::string>());
            if (!cwd.starts_with("/") && !cwd.starts_with("~")) command["cwd"] = "~/" + cwd;
        }
        // a shell of this computer from the page: only when allowed ([web] shell)
        const std::string text = command.contains("text") && command["text"].is_string() ? cluster::trim(command["text"].get<std::string>()) : "";
        const bool shell = cmd == "send" && (text.starts_with("!") || (text.starts_with(":") && text.find('!') != std::string::npos));
        if (!allowed.contains(cmd)) {
            json_(conn, 400, {{"ok", false}, {"error", "command not allowed from the web: '" + cmd + "'"}});
        } else if (shell && !this->_manager.config().web.shell) {
            json_(conn, 200, {{"ok", false}, {"error", "the shell from the page is off (settings: Web > Shell from the page)"}});
        } else {
            const cluster::Json answer = cluster::Control::handle(this->_manager, command);
            // a prompt of this device: it is the one notified for this session (not the desktop here)
            if ((cmd == "send" || cmd == "allow" || cmd == "ask") && answer.value("ok", false) && command.contains("session") && command["session"].is_string()
                && command["session"].get<std::string>() != TASKS_ID) // the Tasks session: not a session to notify
                this->_manager.setOrigin(command["session"].get<std::string>(), "web:" + cookie_(request, "cc_dev"));
            json_(conn, 200, answer);
        }
    } else if (post && request.path == "/api/complete") {
        // the completion of the prompt (@file, /command, /cd, /mode, /model...) and of the folder of a new session
        const std::string session = field("session");
        const cluster::CompletionProvider provider = cluster::completion_provider(this->_manager, [session]() {return session;});
        const cluster::CompletionKind kind = completion_kind_(field("kind"));
        cluster::Json items = cluster::Json::array();
        if (kind != cluster::CompletionKind::None)
            for (const cluster::Candidate& c: provider(kind, field("query")))
                items.push_back({{"value", c.value}, {"label", c.label}, {"description", c.description}, {"directory", c.directory}});
        json_(conn, 200, {{"items", items}});
    } else if (post && request.path == "/api/listen") {
        // a sentence recorded by the browser (wav): its text by the speech model of the settings, its voice word
        if (this->_speech == nullptr || this->_voice == nullptr) {
            json_(conn, 400, {{"error", "no voice in this instance"}});
            return;
        }
        const std::string text = cluster::trim(this->_speech->transcribe(request.body));
        if (const std::string done = this->_manager.asking().spoken(text); !done.empty()) { // a question shown: its words
            json_(conn, 200, {{"text", text}, {"word", ""}, {"question", done}});
            return;
        }
        const std::string word = this->_voice->voiceWord(text);
        json_(conn, 200, {{"text", word.empty() ? this->_voice->correct(text) : text}, {"word", word}});
    } else if (post && request.path == "/api/say") {
        // the last answer of a session (or a text) as its spoken summary: a wav made by voice-say, or the text and its
        // language (the voice of the browser)
        std::string text = field("text");
        const std::optional<cluster::Snapshot> snap = text.empty() ? this->_manager.snapshot(field("session").empty() ? GLOBAL_ID : field("session"))
            : std::nullopt;
        if (snap)
            for (auto it = snap->entries.rbegin(); it != snap->entries.rend() && text.empty(); ++it)
                if (it->kind == cluster::EntryKind::Assistant) text = it->text;
        const std::string summary = cluster::summary_of(text, this->_manager.config().voice.summaryChars);
        if (summary.empty() || !body.value("audio", false) || this->_speech == nullptr) json_(conn, 200, {{"text", summary}, {"lang", cluster::language_of(summary)}});
        else respond_(conn, 200, "audio/wav", this->_speech->speech(summary));
    } else {
        json_(conn, 404, {{"error", "unknown"}});
    }
}

_cold void cluster::Web::login_(void* connection, const void* requestData, const std::string& peer, const std::string& secure)
{
    // a trusted device: in at once; else the code (5 wrong ones: a new code), then the host approves it ([web] approve);
    // GET: how its wait goes (connected: its trust cookie given once)
    Conn& conn = *static_cast<Conn*>(connection);
    const Request& request = *static_cast<const Request*>(requestData);
    const std::string year = "; Path=/; Max-Age=31536000; HttpOnly; SameSite=Strict" + secure + "\r\n";
    const std::string session = "; Path=/; HttpOnly; SameSite=Strict" + secure + "\r\n";
    std::string id = cookie_(request, "cc_dev");
    const std::string agent = request.headers.contains("user-agent") ? request.headers.at("user-agent") : "";
    std::unique_lock<std::mutex> lock(this->_mutex);
    this->expire_();
    if (this->banned_(id, peer)) {
        json_(conn, 403, {{"error", "this device is banned by the computer"}, {"banned", true}});
        return;
    }
    // from outside: always approved by the host; 5 wrong codes block the address 15 minutes
    const bool outside = !same_lan_(peer);
    if (outside && request.method != "GET" && this->_blocked.contains(peer) && this->_blocked.at(peer).second > now_ms_()) {
        json_(conn, 429, {{"error", "too many wrong codes from this address: try again later"}});
        return;
    }
    if (request.method == "GET") {
        auto it = this->_access.find(id);
        if (it == this->_access.end() || cookie_(request, "cc_web") != it->second.token) {
            json_(conn, 200, {{"state", "none"}});
            return;
        }
        std::string cookies;
        if (it->second.device.state == "connected" && it->second.giveTrust) {
            cookies = "Set-Cookie: cc_trust=" + it->second.secret + year;
            it->second.giveTrust = false;
        }
        json_(conn, 200, {{"state", it->second.device.state}, {"name", it->second.device.name}, {"trusted", it->second.device.trusted}}, cookies);
        return;
    }
    const cluster::Json body = cluster::Json::parse(request.body.empty() ? "{}" : request.body, nullptr, false);
    const std::string typed = body.is_object() ? body.value("pin", std::string()) : std::string();
    std::string name = body.is_object() ? cluster::one_line(cluster::trim(body.value("name", std::string())), 40) : std::string();
    if (name.empty()) name = device_name_(agent);
    std::string digits;
    for (const char c: typed)
        if (std::isdigit(static_cast<unsigned char>(c))) digits += c;
    // remembered: its secret
    auto trusted = this->_trusted.find(id);
    if (trusted != this->_trusted.end() && !cookie_(request, "cc_trust").empty() && cookie_(request, "cc_trust") == trusted->second.secret) {
        Access access = trusted->second;
        access.device.state = "connected";
        access.device.address = peer;
        access.device.agent = agent;
        access.device.since = access.device.seen = now_ms_();
        access.token = random_hex_(24);
        trusted->second.device.seen = now_ms_();
        trusted->second.device.address = peer;
        this->saveTrusted_();
        this->_access[id] = access;
        this->_manager.notice(GLOBAL_ID, "web: " + access.device.name + " (" + peer + ") connected (trusted)");
        (void)this->_manager.notify({"web-" + id, "web access", access.device.name + " connected (trusted) · " + peer, cluster::Face::Happy, false, "service-login"}, "web");
        json_(conn, 200, {{"ok", true}, {"state", "connected"}, {"trusted", true}}, "Set-Cookie: cc_web=" + access.token + session);
        return;
    }
    if (digits.empty()) {
        json_(conn, 401, {{"error", "the code"}, {"need", "code"}});
    } else if (digits != this->_pin) {
        if (outside && ++this->_blocked[peer].first >= 5) {
            this->_blocked[peer] = {0, now_ms_() + 15 * 60 * 1000};
            this->_manager.notice(GLOBAL_ID, "web: 5 wrong codes from " + peer + " (outside the LAN): blocked 15 min", true);
            (void)this->_manager.notify({"web-blocked", "web access", "5 wrong codes from " + peer + " (outside the LAN): blocked 15 min", cluster::Face::Waiting, true,
                "dialog-warning"}, "web");
        }
        if (++this->_failures >= 5) {
            this->newPin_();
            this->_manager.notice(GLOBAL_ID, "web: 5 wrong codes (last from " + peer + "), new code " + this->_pin.substr(0, 3) + " "
                + this->_pin.substr(3), true);
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        json_(conn, 401, {{"error", "wrong code"}});
    } else {
        // the code right: this device waits for the host (or is in at once when [web] approve is off)
        this->_failures = 0;
        if (id.size() != 24) id = random_hex_(12);
        Access access;
        access.device = {id, name, agent, peer, "waiting", false, now_ms_(), now_ms_()};
        access.token = random_hex_(24);
        const bool approve = this->_manager.config().web.approve || outside;
        if (outside) this->_blocked.erase(peer);
        if (!approve) access.device.state = "connected";
        this->_access[id] = access;
        const std::string cookies = "Set-Cookie: cc_dev=" + id + year + "Set-Cookie: cc_web=" + access.token + session;
        if (approve) {
            this->ask_(id);
            json_(conn, 202, {{"ok", true}, {"state", "waiting"}, {"name", name}}, cookies);
        } else {
            this->_manager.notice(GLOBAL_ID, "web: " + name + " (" + peer + ") connected");
            (void)this->_manager.notify({"web-" + id, "web access", name + " connected · " + peer, cluster::Face::Happy, false, "service-login"}, "web");
            json_(conn, 200, {{"ok", true}, {"state", "connected"}}, cookies);
        }
    }
}

/* functions */
_cold std::string cluster::web_line(const cluster::WebStatus& status)
{
    if (!status.on) return status.error.empty() ? "" : "web: port not opened (" + status.error + ")";
    std::size_t connected = 0;
    std::size_t waiting = 0;
    for (const cluster::WebDevice& d: status.devices) {
        connected += d.state == "connected" ? 1 : 0;
        waiting += d.state == "waiting" ? 1 : 0;
    }
    // reached from outside: its public address first (the one to give away from home), the LAN one after; asked but not
    // reached: told
    std::string where = status.url;
    if (!status.remoteUrl.empty() && status.remoteOk) {
        std::string lan = status.url.substr(status.url.find("://") == std::string::npos ? 0 : status.url.find("://") + 3);
        where = status.remoteUrl + " (LAN " + lan + ")";
    } else if (!status.remoteState.empty()) {
        where += " · outside ✕";
    }
    return "web " + where + " · code " + status.pin + " · " + (connected == 0 ? std::string("nobody") : std::to_string(connected) + " connected")
        + (status.ping >= 0 ? " · ping " + std::to_string(status.ping) + " ms" : "") + (waiting > 0 ? " · " + std::to_string(waiting) + " waiting for you" : "");
}

_cold static cluster::WebDevice device_json_(const cluster::Json& d)
{
    return {d.value("id", std::string()), d.value("name", std::string()), d.value("agent", std::string()), d.value("address", std::string()),
        d.value("state", std::string()), d.value("trusted", false), d.value("since", static_cast<std::int64_t>(0)) * 1000, d.value("seen", static_cast<std::int64_t>(0)) * 1000,
        d.value("ping", -1)};
}

static std::mutex web_cache_mutex_;
static std::int64_t web_cache_at_ = 0;
static std::optional<cluster::WebStatus> web_cache_;

_cold std::optional<cluster::WebStatus> cluster::web_status(cluster::Manager& manager)
{
    // here: its state; an interface of the core: asked to the core (once a second at most, the screen redraws often)
    if (manager.web() != nullptr) return manager.web()->status();
    if (!manager.replica()) return std::nullopt;
    std::lock_guard<std::mutex> lock(web_cache_mutex_);
    if (now_ms_() - web_cache_at_ < 1000) return web_cache_;
    web_cache_at_ = now_ms_();
    try {
        const cluster::Json data = manager.call({{"cmd", "web"}, {"action", "devices"}});
        cluster::WebStatus status;
        status.on = data.value("on", false);
        status.url = data.value("url", std::string());
        status.pin = data.value("pin", std::string());
        status.client = data.value("client", std::string());
        status.error = data.value("error", std::string());
        status.https = data.value("https", false);
        status.fingerprint = data.value("fingerprint", std::string());
        status.remoteUrl = data.value("remote_url", std::string());
        status.remoteState = data.value("remote", std::string());
        status.remoteOk = data.value("remote_ok", false);
        status.ping = data.value("ping", -1);
        for (const char* list: {"devices", "trusted", "banned"})
            if (data.contains(list) && data[list].is_array())
                for (const cluster::Json& d: data[list])
                    (std::string(list) == "devices" ? status.devices : std::string(list) == "trusted" ? status.trusted : status.banned).push_back(device_json_(d));
        web_cache_ = status;
    } catch (const utils::exception::IException&) {
        web_cache_ = std::nullopt;
    }
    return web_cache_;
}

_cold void cluster::web_act(cluster::Manager& manager, const std::string& action, const std::vector<std::string>& ids, const std::string& id, const std::string& answer)
{
    if (cluster::Web* web = manager.web()) {
        if (action == "on" || action == "off") {
            web->setOn(action == "on");
            web->tick();
        } else if (action == "pin") web->renewPin();
        else if (action == "kick") web->kick(ids);
        else if (action == "approve") web->approve(id, answer);
        else if (action == "untrust") web->untrust(ids);
        else if (action == "ban") web->ban(ids);
        else if (action == "unban") web->unban(ids);
        return;
    }
    if (!manager.replica()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no web access here");
    std::string joined;
    for (const std::string& i: ids)
        joined += (joined.empty() ? "" : ",") + i;
    (void)manager.call({{"cmd", "web"}, {"action", action}, {"ids", joined}, {"id", id}, {"answer", answer}});
    std::lock_guard<std::mutex> lock(web_cache_mutex_);
    web_cache_at_ = 0; // its new state at the next look
}
