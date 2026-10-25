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
#define _SManip
#include <utils/utils.hpp>
#include "cluster/Attachments.hpp"
#include "cluster/Templates.hpp"
#include "cluster/Control.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Display.hpp"
#include "cluster/Docs.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Origin.hpp"
#include "cluster/Icons.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Demo.hpp"
#include "cluster/Wire.hpp"
#include "cluster/Web.hpp"
#include "cluster/Simple.hpp"
#ifdef CLUSTER_TLS
    #include <openssl/x509.h>
    #include <openssl/ssl.h>
    #include <openssl/pem.h>
    #include <openssl/err.h>
    #include <openssl/evp.h>
#endif
#include <netinet/tcp.h>
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
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <fstream>
#include <poll.h>
#include <atomic>
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

_cold static bool same_secret_(const std::string& a, const std::string& b)
{
    // compared whole whatever matches: its time tells nothing (the code, the secret of a trusted device)
    volatile unsigned char diff = static_cast<unsigned char>(a.size() ^ b.size());
    for (std::size_t i = 0; i < std::max(a.size(), b.size()); ++i)
        diff = static_cast<unsigned char>(diff | ((i < a.size() ? static_cast<unsigned char>(a[i]) : 0) ^ (i < b.size() ? static_cast<unsigned char>(b[i]) : 0)));
    return diff == 0;
}

// the connections of each address at once (8 at most: one attacker never takes every thread)
static std::mutex per_ip_mutex_;
static std::map<std::string, int> per_ip_;

_cold static bool ip_take_(const std::string& peer)
{
    std::lock_guard<std::mutex> lock(per_ip_mutex_);
    if (per_ip_[peer] >= 8) return false;
    ++per_ip_[peer];
    return true;
}

_cold static void ip_give_(const std::string& peer)
{
    std::lock_guard<std::mutex> lock(per_ip_mutex_);
    if (--per_ip_[peer] <= 0) per_ip_.erase(peer);
}

_cold static std::string safe_filename_(const std::string& name)
{
    // in a header: printable ASCII only, no quote (a name made by an agent: no header injected)
    std::string out;
    for (const char c: name)
        out += (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') ? c : '_';
    return out.empty() ? std::string("file") : out;
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

static std::atomic<std::size_t> attach_cap_{0}; // the largest upload (settings: Attachments), set at each request

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
            if (length > (request.path == "/api/listen" ? 16000000U : request.path == "/api/attach" ? attach_cap_.load() + 8192U : 1000000U)) return false;
            // a big body (a recording, a prompt): a device connected only (its cookie); a stranger: a few KB, 10 s
            if (length > 4096 && !(request.headers.contains("cookie") && request.headers.at("cookie").find("cc_web=") != std::string::npos)) return false;
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

/* the WebSocket of the terminal page: its socket non-blocking, read and written by one thread (a TLS connection
   never by two) */
_cold static ssize_t conn_read_now_(Conn& conn, char* data, const std::size_t size)
{
    // what is there: n bytes, 0 nothing now, -1 closed / an error
    #ifdef CLUSTER_TLS
        if (conn.ssl != nullptr) {
            const int got = ::SSL_read(conn.ssl, data, static_cast<int>(size));
            if (got > 0) return got;
            const int error = ::SSL_get_error(conn.ssl, got);
            return error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE ? 0 : -1;
        }
    #endif
    const ssize_t got = ::recv(conn.fd, data, size, MSG_DONTWAIT);
    if (got > 0) return got;
    return got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) ? 0 : -1;
}

_cold static bool conn_pending_(Conn& conn)
{
    // TLS: bytes already decrypted, the socket itself empty
    #ifdef CLUSTER_TLS
        if (conn.ssl != nullptr) return ::SSL_pending(conn.ssl) > 0;
    #endif
    (void)conn;
    return false;
}

_cold static bool conn_write_now_(Conn& conn, const std::string& data)
{
    // all of it, waiting while the socket is full (5 s at most: a page that reads nothing is dropped)
    const std::int64_t end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + 5000;
    std::size_t sent = 0;
    while (sent < data.size()) {
        ssize_t size = 0;
        bool wait = false;
        bool written = false;   // by TLS
        #ifdef CLUSTER_TLS
            if (conn.ssl != nullptr) {
                written = true;
                size = ::SSL_write(conn.ssl, data.data() + sent, static_cast<int>(data.size() - sent));
                if (size <= 0) {
                    const int error = ::SSL_get_error(conn.ssl, static_cast<int>(size));
                    if (error != SSL_ERROR_WANT_WRITE && error != SSL_ERROR_WANT_READ) return false;
                    wait = true;
                }
            }
        #endif
        if (!written) {
            size = ::send(conn.fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL | MSG_DONTWAIT);
            if (size < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) return false;
            wait = size < 0;
        }
        if (wait) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() > end) return false;
            pollfd room{conn.fd, POLLOUT | POLLIN, 0};
            (void)::poll(&room, 1, 50);
            continue;
        }
        sent += static_cast<std::size_t>(size);
    }
    return true;
}

_cold static bool ws_send_(Conn& conn, const int opcode, const std::string& payload)
{
    // one frame of the server (final, never masked)
    std::string frame(1, static_cast<char>(0x80 | opcode));
    const std::uint64_t size = payload.size();
    if (size < 126) {
        frame += static_cast<char>(size);
    } else if (size < 65536) {
        frame += static_cast<char>(126);
        frame += static_cast<char>((size >> 8) & 0xFF);
        frame += static_cast<char>(size & 0xFF);
    } else {
        frame += static_cast<char>(127);
        for (int i = 7; i >= 0; --i)
            frame += static_cast<char>((size >> (8 * i)) & 0xFF);
    }
    return conn_write_now_(conn, frame + payload);
}

static std::atomic<bool> hsts_on_{false}; // https on: the browsers keep https for a year (Strict-Transport-Security)

_cold static void respond_(Conn& conn, const int code, const std::string& type, const std::string& body, const std::string& extra = "")
{
    static const std::map<int, std::string> reasons = {{200, "OK"}, {400, "Bad Request"}, {401, "Unauthorized"}, {403, "Forbidden"},
        {301, "Moved Permanently"}, {404, "Not Found"}, {423, "Locked"}, {500, "Internal Server Error"}};
    const std::string reason = reasons.contains(code) ? reasons.at(code) : "Error";
    (void)send_all_(conn, "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size())
        + "\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\n"
        + (hsts_on_ ? "Strict-Transport-Security: max-age=31536000\r\n" : "") + extra
        + "Connection: close\r\n\r\n" + body);
}

_cold static void json_(Conn& conn, const int code, const cluster::Json& body, const std::string& extra = "")
{
    respond_(conn, code, "application/json; charset=utf-8", body.dump(-1, ' ', false, cluster::Json::error_handler_t::replace), extra);
}

// the documentation (embedded in the binary): public like /ca.crt, nothing secret in it; cached by the browser (one ETag per build), framed by this
// origin only (the panel of the page), scripts of the site only
_cold static void docs_(Conn& conn, const Request& request)
{
    const std::string path = request.path.size() > 6 ? request.path.substr(6) : std::string();
    if (request.path == "/docs") {
        respond_(conn, 301, "text/plain", "docs\n", "Location: /docs/\r\n");
        return;
    }
    if (path == "search") { // /docs/search?q=text&lang=fr: the ranking of libutils (c2dmp-hsm), the same engine as the search of the settings
        const std::string query = request.query.contains("q") ? request.query.at("q") : "";
        const std::string lang = request.query.contains("lang") && request.query.at("lang") == "fr" ? "fr" : "en";
        json_(conn, 200, cluster::docs_search(cluster::trim(query).substr(0, 200), lang, 8));
        return;
    }
    const std::optional<cluster::DocAsset> asset = cluster::docs_built() ? cluster::docs_asset(path) : std::nullopt;
    if (!asset) {
        respond_(conn, 404, "text/plain", cluster::docs_built() ? "no such page\n" : "the documentation was not built into this claude-cluster (python3 3.11+ is needed at build)\n");
        return;
    }
    const std::string headers = "Cache-Control: public, max-age=3600\r\nETag: " + asset->etag + "\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\n"
        "X-Frame-Options: SAMEORIGIN\r\nContent-Security-Policy: default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self'; img-src 'self' data:; "
        "connect-src 'self'; object-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'self'\r\n" + (hsts_on_ ? "Strict-Transport-Security: max-age=31536000\r\n" : "");
    if (request.headers.contains("if-none-match") && request.headers.at("if-none-match") == asset->etag) {
        (void)send_all_(conn, "HTTP/1.1 304 Not Modified\r\n" + headers + "Connection: close\r\n\r\n");
        return;
    }
    (void)send_all_(conn, "HTTP/1.1 200 OK\r\nContent-Type: " + asset->mime + "\r\nContent-Length: " + std::to_string(asset->body.size()) + "\r\n" + headers
        + "Connection: close\r\n\r\n" + asset->body);
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

_cold static cluster::Json light_display_(const cluster::Json& display)
{
    // its pictures left out (asked once by the page: /api/display), the rest whole
    cluster::Json light = display;
    for (const char* heavy: {"data", "svg", "png"})
        if (light.erase(heavy) > 0) light["heavy"] = true;
    if (light.contains("files") && light["files"].is_array())
        for (cluster::Json& f: light["files"])
            if (f.is_object() && f.erase("preview") > 0) light["heavy"] = true;
    return light;
}

_cold static cluster::Json session_json_(cluster::Manager& manager, const std::string& id)
{
    // the session shown: its state, permissions, questions and the last 150 entries of its transcript
    const std::optional<cluster::Snapshot> snap = manager.snapshot(id);
    if (!snap) return nullptr;
    cluster::Json json = manager.toJson(*snap, false);
    if (snap->state == cluster::State::Working) json["turn"] = cluster::turn_state(*snap); // its step, the tools running, the tokens
    json["agent_list"] = cluster::Json::array(); // its sub-agents (their activity asked when one is opened: agents)
    for (const cluster::SubAgent& a: snap->agents) {
        cluster::Json item = a;
        item.erase("entries");
        item.erase("prompt");
        json["agent_list"].push_back(std::move(item));
    }
    json["messages"] = cluster::Json::array();
    const std::size_t start = snap->entries.size() > 150 ? snap->entries.size() - 150 : 0;
    for (std::size_t i = start; i < snap->entries.size(); ++i) {
        const cluster::Entry& e = snap->entries[i];
        if (e.kind == cluster::EntryKind::Thinking) continue;
        if (e.kind == cluster::EntryKind::Display) {
            // its pictures left out (asked once by the page: /api/display), the rest whole
            cluster::Json light = cluster::Json::parse(e.text, nullptr, false);
            if (!light.is_object()) continue;
            light = light_display_(light);
            json["messages"].push_back({{"kind", "display"}, {"text", light.dump(-1, ' ', false, cluster::JSON_LENIENT)}, {"tool", ""}, {"time", e.time}, {"index", i}});
            continue;
        }
        const bool long_text = e.kind == cluster::EntryKind::Assistant || e.kind == cluster::EntryKind::User;
        std::string text = e.text.size() > (long_text ? 30000 : 600) ? e.text.substr(0, long_text ? 30000 : 600) + " [...]" : e.text;
        cluster::Json message = {{"kind", kind_name_(e.kind)}, {"text", text}, {"tool", e.tool}, {"time", e.time}, {"index", i}};
        if (e.kind == cluster::EntryKind::User) message["origin"] = cluster::prompt_origin(e.origin, e.text); // user | mcp | task | schedule
        json["messages"].push_back(message);
    }
    // the pinned item, drawn at the bottom of the view: the display as it is now (its entry index for a picture), or the message
    if (const cluster::Json block = cluster::display_pinned(*snap); block.is_object()) {
        json["pin_block"] = light_display_(block);
        if (snap->spec.pin.value("kind", std::string()) == "display")
            for (std::size_t i = snap->entries.size(); i > 0; --i)
                if (snap->entries[i - 1].kind == cluster::EntryKind::Display && snap->entries[i - 1].text.find("\"id\":\"" + snap->spec.pin.value("id", std::string()) + "\"") != std::string::npos) {
                    json["pin_index"] = i - 1;
                    break;
                }
    }
    return json;
}

/* constructor */
_cold int cluster::docs_serve(const int port, const bool lan)
{
    // the documentation alone on a port: nothing but GET / HEAD of /docs (read only: no core, no session, no setting behind it); this computer only,
    // or the LAN too (every interface, a peer outside the LAN refused); one thread per request, 32 at most
    if (!cluster::docs_built()) {
        std::cerr << "the documentation is not in this build (python3 3.11+ is needed to build claude-cluster with it)" << std::endl;
        return 1;
    }
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); // xstyle: ignore LU-SOCKET (HTTP: one thread per request, the peer address)
    const int on = 1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(lan ? INADDR_ANY : INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (fd == -1 || ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1 || ::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1
        || ::listen(fd, 16) == -1) {
        std::cerr << "docs serve: port " << port << ": " << std::strerror(errno) << std::endl;
        if (fd != -1) ::close(fd);
        return 1;
    }
    std::cout << "documentation (read only): http://" << (lan ? lan_address_() : std::string("127.0.0.1")) << ":" << port << "/docs/"
        << (lan ? "  (this computer and its LAN)" : "  (this computer; 'lan': its LAN too)") << "\nCtrl+C to stop" << std::endl;
    static std::atomic<int> busy{0};
    while (true) {
        sockaddr_in peer{};
        socklen_t size = sizeof(peer);
        const int client = ::accept4(fd, reinterpret_cast<sockaddr*>(&peer), &size, SOCK_CLOEXEC);
        if (client == -1) continue;
        std::array<char, INET_ADDRSTRLEN> text{};
        ::inet_ntop(AF_INET, &peer.sin_addr, text.data(), text.size());
        if (busy >= 32 || !same_lan_(text.data())) {
            ::close(client);
            continue;
        }
        ++busy;
        std::thread([client]() {
            const timeval wait{10, 0};
            (void)::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait));
            Conn conn{client};
            Request request;
            if (read_request_(conn, request)) {
                if (request.method != "GET" && request.method != "HEAD") respond_(conn, 405, "text/plain", "read only\n", "Allow: GET, HEAD\r\n");
                else if (request.path == "/") respond_(conn, 301, "text/plain", "docs\n", "Location: /docs/\r\n");
                else if (request.path == "/docs" || request.path.starts_with("/docs/")) docs_(conn, request);
                else respond_(conn, 404, "text/plain", "only the documentation here: /docs/\n");
            }
            ::close(client);
            --busy;
        }).detach();
    }
}

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
    if (this->_certThread.joinable()) this->_certThread.join();
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
    } else if (this->_fd != -1 && this->_tls != nullptr && !this->_sni.name.empty() && cluster::now() - this->_sniAt > 86400) {
        this->certAsync_(); // its certificate of the tailnet: renewed (swapped in, nobody disconnected)
    }
}

_cold int cluster::Web::listen_(const int port, std::string& error)
{
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); // xstyle: ignore LU-SOCKET (HTTP: one thread per request, the peer address)
    const int on = 1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // every interface, the peers outside the LAN refused (same_lan_)
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (fd == -1) error = std::strerror(errno);
    else if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1 || ::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1
        || ::listen(fd, 8) == -1) error = std::strerror(errno);
    if (!error.empty() && fd != -1) {
        ::close(fd);
        return -1;
    }
    return fd;
}

_cold static int discovery_port_(void)
{
    const char* other = std::getenv("CLAUDE_CLUSTER_DISCOVERY_PORT");
    return other != nullptr && std::atoi(other) > 0 ? std::atoi(other) : DISCOVERY_PORT;
}

_cold static int find_socket_(void)
{
    // the app searching this network: its broadcast received on every interface (UDP); not opened (taken): its address typed in the app
    // (one core only: another one on it already, this one not found - never two sharing its datagrams)
    const int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0); // xstyle: ignore LU-SOCKET (UDP: a datagram, its sender answered)
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // the peers outside the LAN never answered (same_lan_)
    addr.sin_port = htons(static_cast<std::uint16_t>(discovery_port_()));
    if (fd == -1) return -1;
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1) {
        ::close(fd);
        return -1;
    }
    return fd;
}

_cold static std::string authority_(void)
{
    // the fingerprint of the authority of this computer (what the app pinned): SHA-256 of its certificate, AA:BB:... as the app shows it
    std::string out;
    #ifdef CLUSTER_TLS
        FILE* file = std::fopen((cluster::config_dir() / "web" / "ca.pem").c_str(), "r");
        if (file == nullptr) return out;
        X509* ca = ::PEM_read_X509(file, nullptr, nullptr, nullptr);
        (void)std::fclose(file);
        if (ca == nullptr) return out;
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        unsigned int size = 0;
        if (::X509_digest(ca, ::EVP_sha256(), digest.data(), &size) == 1)
            for (unsigned int i = 0; i < size; ++i) {
                static const char* hex = "0123456789ABCDEF";
                out += std::string(i ? ":" : "") + hex[digest[i] >> 4] + hex[digest[i] & 15];
            }
        ::X509_free(ca);
    #endif
    return out;
}

_cold void cluster::Web::open_(const int port)
{
    std::string error;
    const int fd = cluster::Web::listen_(port, error);
    const bool inUse = errno == EADDRINUSE;
    cluster::debug("web", fd >= 0 ? "open on the port " + std::to_string(port) : "not opened on the port " + std::to_string(port) + ": " + error);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_port = port;
        if (!error.empty()) {
            if (error != this->_error) {
                this->_manager.notice(GLOBAL_ID, "web access: port " + std::to_string(port) + " not opened (" + error + ")", true);
                (void)this->_manager.fail("Open the web access", "port " + std::to_string(port) + " of this computer", error
                    + (inUse ? " (another program uses it: change Port in the settings, or stop it)" : ""), [m = &this->_manager]() {if (m->web() != nullptr) m->web()->reopen();});
            }
            this->_error = error;
            return;
        }
        this->_error.clear();
    }
    // its steps in a popup (the router, the certificate: seconds), its lock free meanwhile: the interfaces read its state
    // (closed until its end), they never wait for it
    cluster::Busy busy(this->_manager, "Opening the web access");
    cluster::busy_step("port " + std::to_string(port) + " opened on this computer");
    // the terminal page: its own port (0: none); taken: told, the page without it
    this->_ttyPort = this->_manager.config().web.ttyPort;
    if (this->_ttyPort > 0 && this->_ttyPort != port) {
        std::string ttyError;
        this->_ttyFd = cluster::Web::listen_(this->_ttyPort, ttyError);
        if (this->_ttyFd == -1) this->_manager.notice(GLOBAL_ID, "web access: the terminal page port " + std::to_string(this->_ttyPort) + " not opened (" + ttyError + ")", true);
        else cluster::busy_step("port " + std::to_string(this->_ttyPort) + " opened for the terminal page");
    }
    this->_httpsWanted = this->_manager.config().web.https;
    this->remote_(); // its names first: in the certificate
    if (this->_httpsWanted) cluster::busy_step("its certificate (https)");
    const std::string tls = this->_httpsWanted ? this->tls_() : "";
    this->_authority = this->_tls != nullptr ? authority_() : "";
    this->_findFd = find_socket_();
    cluster::debug("web", this->_findFd >= 0 ? "found by the app: UDP " + std::to_string(discovery_port_()) : "UDP " + std::to_string(discovery_port_()) + " taken: not found by the app");
    if (!tls.empty()) cluster::debug("web", "no https: " + tls);
    if (!tls.empty()) this->_manager.notice(GLOBAL_ID, "web access: no https (" + tls + "), plain http: no microphone in the page", true);
    if (!this->_certWhy.empty()) // tailscale without its real certificate: the authority of this computer meanwhile (a warning until it is trusted)
        (void)this->_manager.fail("Get the real certificate of the tailnet (no browser warning)", "tailscale cert", this->_certWhy, [m = &this->_manager]() {
            if (m->web() != nullptr) m->web()->reopen();
        });
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_fd = fd; // open now
        hsts_on_ = this->_tls != nullptr;
        this->newPin_();
        this->loadTrusted_();
        this->loadBanned_();
        this->_running = true;
        this->_thread = std::thread(&cluster::Web::serve_, this);
    }
    this->_manager.notice(GLOBAL_ID, "web access open: " + std::string(this->_tls != nullptr ? "https://" : "http://") + lan_address_() + ":"
        + std::to_string(port) + " code " + this->_pin.substr(0, 3) + " " + this->_pin.substr(3) + " (this network only; each new device approved here)");
    if (!this->_remoteState.empty()) this->_manager.notice(GLOBAL_ID, "web access from outside: " + this->_remoteState, !this->_remoteOk);
    const std::string remote = this->_manager.config().web.remote;
    // its result (outside not reached: ✗, then its failure popup after it)
    busy.end(remote == "off" || this->_remoteOk, "web access open: " + (this->_remoteOk && !this->_remoteUrl.empty() ? this->_remoteUrl + " (outside), " : std::string())
        + (this->_tls != nullptr ? "https://" : "http://") + lan_address_() + ":" + std::to_string(port) + (remote != "off" && !this->_remoteOk ? " - not reached from outside" : ""));
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
    this->_sni.name.clear();
    this->_certWhy.clear();
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
        const cluster::Json domains = status.is_object() ? status.value("CertDomains", cluster::Json::array()) : cluster::Json::array();
        this->_sni.name = domains.is_array() && !domains.empty() && domains[0].is_string() ? cluster::lower(domains[0].get<std::string>()) : "";
        this->_certWhy = this->_sni.name.empty() ? "the HTTPS certificates of your tailnet are off: turn them on (login.tailscale.com › DNS › HTTPS Certificates), "
            "then Ask the router again (Services › Outside)" : "";
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
            if (conf.ttyPort > 0 && map.code == 0) { // the terminal page too
                const int ttyOutside = conf.ttyRemotePort > 0 ? conf.ttyRemotePort : conf.ttyPort;
                cluster::busy_step("asking the router to open its port " + std::to_string(ttyOutside) + " for the terminal page");
                const cluster::Captured tty = cluster::capture({"upnpc", "-e", "claude-cluster tty", "-r", std::to_string(conf.ttyPort), std::to_string(ttyOutside), "TCP"}, "", 20);
                if (tty.code == 0 && tty.out.find("is redirected") != std::string::npos) this->_mappedTty = ttyOutside;
            }
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
    cluster::debug("web", "closed");
    this->_running = false;
    if (this->_thread.joinable()) this->_thread.join();
    for (int i = 0; i < 50 && this->_busy > 0; ++i) // the requests being served end (a waiting state returns within 0.2 s)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (this->_mapped > 0) { // the port of the router closed again
        (void)cluster::capture({"upnpc", "-d", std::to_string(this->_mapped), "TCP"}, "", 20);
        this->_mapped = 0;
    }
    if (this->_mappedTty > 0) {
        (void)cluster::capture({"upnpc", "-d", std::to_string(this->_mappedTty), "TCP"}, "", 20);
        this->_mappedTty = 0;
    }
    this->_ttys.closeAll(); // the terminals of the page: gone with the port
    if (this->_ttyFd != -1) ::close(this->_ttyFd);
    this->_ttyFd = -1;
    if (this->_findFd != -1) ::close(this->_findFd);
    this->_findFd = -1;
    this->_foundAt.clear();
    std::lock_guard<std::mutex> lock(this->_mutex);
    ::close(this->_fd);
    this->_fd = -1;
    #ifdef CLUSTER_TLS
        if (this->_tls != nullptr) ::SSL_CTX_free(static_cast<SSL_CTX*>(this->_tls));
        if (void* old = this->_sni.context.exchange(nullptr)) ::SSL_CTX_free(static_cast<SSL_CTX*>(old));
    #endif
    this->_tls = nullptr;
    this->_access.clear();
}

_cold void cluster::Web::serve_(void)
{
    utils::encapsulation::Poll poll;
    poll.link(this->_fd, EPOLLIN);
    if (this->_ttyFd != -1) poll.link(this->_ttyFd, EPOLLIN);
    if (this->_findFd != -1) poll.link(this->_findFd, EPOLLIN);
    std::int64_t expired = 0;
    while (this->_running) {
        const std::vector<epoll_event> ready = poll.wait(300);
        if (cluster::now_ms() - expired > 10000) { // the terminals of the page nobody reads any more
            expired = cluster::now_ms();
            this->_ttys.expire();
        }
        if (ready.empty()) continue;
        if (this->_findFd != -1 && ready.front().data.fd == this->_findFd) {
            this->found_();
            continue;
        }
        const bool tty = this->_ttyFd != -1 && ready.front().data.fd == this->_ttyFd;
        sockaddr_in peer{};
        socklen_t size = sizeof(peer);
        const int client = ::accept4(tty ? this->_ttyFd : this->_fd, reinterpret_cast<sockaddr*>(&peer), &size, SOCK_CLOEXEC);
        if (client == -1) continue;
        std::array<char, INET_ADDRSTRLEN> text{};
        ::inet_ntop(AF_INET, &peer.sin_addr, text.data(), text.size());
        if (this->_busy >= 64 || !ip_take_(text.data())) { // full, or this address alone takes too many: told, closed
            static const std::string full = "HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/plain\r\nContent-Length: 20\r\nConnection: close\r\n\r\nclaude-cluster: busy\n";
            (void)::send(client, full.data(), full.size(), MSG_NOSIGNAL);
            ::close(client);
            continue;
        }
        ++this->_busy;
        std::thread(&cluster::Web::client_, this, client, std::string(text.data()), tty).detach();
    }
}

_cold void cluster::Web::found_(void)
{
    // the app searching this network (its broadcast): this computer told to it - its name, its port, the authority it trusts; this LAN
    // only, one answer per address every 0.5 s, nothing secret (its code and your yes are still asked at its connection)
    std::array<char, 64> packet{};
    sockaddr_in peer{};
    socklen_t size = sizeof(peer);
    const ssize_t got = ::recvfrom(this->_findFd, packet.data(), packet.size(), 0, reinterpret_cast<sockaddr*>(&peer), &size);
    if (got <= 0 || !this->_manager.config().web.discovery) return;
    std::array<char, INET_ADDRSTRLEN> text{};
    ::inet_ntop(AF_INET, &peer.sin_addr, text.data(), text.size());
    if (!std::string(packet.data(), static_cast<std::size_t>(got)).starts_with(DISCOVERY_ASK) || !same_lan_(text.data())) return;
    if (this->_foundAt.size() > 256) this->_foundAt.clear();
    std::int64_t& last = this->_foundAt[text.data()];
    if (cluster::now_ms() - last < 500) return;
    last = cluster::now_ms();
    const std::string answer = cluster::Json{{"app", "claude-cluster"}, {"name", cluster::host_name()}, {"port", this->_port}, {"https", this->_tls != nullptr},
        {"authority", this->_authority}, {"version", CLUSTER_VERSION}}.dump();
    (void)::sendto(this->_findFd, answer.data(), answer.size(), MSG_NOSIGNAL, reinterpret_cast<const sockaddr*>(&peer), size);
}

#ifdef CLUSTER_TLS
    _cold static int sni_(SSL* ssl, int*, void* arg)
    {
        // the name of the tailnet asked (SNI): its real certificate (Let's Encrypt); any other name / address: the one of the
        // authority of this computer
        const cluster::WebSniView* sni = static_cast<const cluster::WebSniView*>(arg);
        const char* asked = ::SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
        void* context = sni->context->load();
        if (asked != nullptr && context != nullptr && cluster::lower(asked) == *sni->name) (void)::SSL_set_SSL_CTX(ssl, static_cast<SSL_CTX*>(context));
        return SSL_TLSEXT_ERR_OK;
    }
#endif

_cold void cluster::Web::tailnetCert_(void)
{
    // tailscale cert: a real certificate of <computer>.<tailnet>.ts.net (Let's Encrypt, renewed by tailscale), swapped in
    // without closing the port; not got: why (this user not allowed, the HTTPS certificates of the tailnet off)
    #ifdef CLUSTER_TLS
        if (this->_sni.name.empty() || !cluster::has_command("tailscale")) return;
        const std::filesystem::path dir = cluster::config_dir() / "web";
        const std::filesystem::path cert = dir / "tailnet-cert.pem";
        const std::filesystem::path key = dir / "tailnet-key.pem";
        cluster::busy_step("its real certificate from the tailnet (tailscale cert " + this->_sni.name + ", Let's Encrypt)");
        const cluster::Captured got = cluster::capture({"tailscale", "cert", "--cert-file", cert.string(), "--key-file", key.string(), this->_sni.name}, "", 120);
        ::chmod(key.c_str(), S_IRUSR | S_IWUSR);
        for (const std::string& line: cluster::split(got.out, '\n'))
            cluster::busy_log(line);
        this->_sniAt = cluster::now();
        if (got.code != 0) {
            const std::string out = cluster::lower(got.out);
            this->_certWhy = out.find("access denied") != std::string::npos || out.find("operator") != std::string::npos || out.find("permission") != std::string::npos
                ? "this user may not ask tailscale for certificates: allow it once (Services › Outside › Allow tailscale certificates: tailscale set --operator="
                    + std::string(std::getenv("USER") != nullptr ? std::getenv("USER") : "you") + ")"
                : out.find("https") != std::string::npos || out.find("not enabled") != std::string::npos || out.find("support") != std::string::npos
                ? "the HTTPS certificates of your tailnet are off: turn them on (login.tailscale.com › DNS › HTTPS Certificates)"
                : "tailscale cert: " + cluster::one_line(got.out, 200);
            return;
        }
        SSL_CTX* context = ::SSL_CTX_new(::TLS_server_method());
        if (context == nullptr) return;
        (void)::SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
        (void)::SSL_CTX_set_options(context, SSL_OP_NO_RENEGOTIATION | SSL_OP_CIPHER_SERVER_PREFERENCE);
        if (::SSL_CTX_use_certificate_chain_file(context, cert.c_str()) != 1 || ::SSL_CTX_use_PrivateKey_file(context, key.c_str(), SSL_FILETYPE_PEM) != 1) {
            ::SSL_CTX_free(context);
            this->_certWhy = "the certificate of the tailnet unreadable (" + cluster::short_path(dir.string()) + ")";
            return;
        }
        this->_certWhy.clear();
        // the old one: freed once its connections end (their SSL hold it)
        if (void* old = this->_sni.context.exchange(context)) ::SSL_CTX_free(static_cast<SSL_CTX*>(old));
    #endif
}

_cold void cluster::Web::certAsync_(void)
{
    // the real certificate of the tailnet got apart (tailscale cert waits for Let's Encrypt: seconds, a minute): the page
    // served meanwhile with the authority of this computer; its popup, its failure told (Retry: again)
    if (this->_sni.name.empty() || !cluster::has_command("tailscale") || this->_certBusy.exchange(true)) return;
    if (this->_certThread.joinable()) this->_certThread.join();
    this->_certThread = std::thread([this]() {
        cluster::Busy busy(this->_manager, "The real certificate of the tailnet (" + this->_sni.name + ")");
        this->tailnetCert_();
        const std::string why = this->_certWhy;
        busy.end(why.empty(), why.empty() ? "served to " + this->_sni.name + " (no browser warning)" : why);
        if (!why.empty())
            (void)this->_manager.fail("Get the real certificate of the tailnet (no browser warning)", "tailscale cert", why, [this]() {this->certAsync_();});
        this->_certBusy = false;
    });
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
        // a CN is 64 characters at most (openssl refuses more): a long host name (a reverse DNS name given by the network) cut, the
        // short name (before its first dot) in the authority; the full names stay in the subjectAltName
        const std::function<std::string(const std::string&)> cn = [](const std::string& text) {return text.substr(0, 64);};
        // the authority of this computer: made once (trusted once per device, it stays)
        if (!std::filesystem::exists(ca) || !std::filesystem::exists(caKey)) {
            std::array<char, 256> own{};
            (void)::gethostname(own.data(), own.size() - 1);
            const std::string host = own[0] != '\0' ? std::string(own.data()).substr(0, std::string(own.data()).find('.')) : "this computer";
            const cluster::Captured made = cluster::capture({"openssl", "req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:prime256v1", "-nodes",
                "-days", "7300", "-subj", "/O=claude-cluster/CN=" + cn("claude-cluster on " + host),
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
                "-subj", "/O=claude-cluster/CN=" + cn(own[0] != '\0' ? own.data() : "claude-cluster"), "-keyout", key.string(), "-out", request.string()}, "", 30);
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
        (void)::SSL_CTX_set_options(context, SSL_OP_NO_RENEGOTIATION | SSL_OP_CIPHER_SERVER_PREFERENCE);
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
        // reached by tailscale: its real certificate too, served to its name (SNI)
        this->_sniView = {&this->_sni.context, &this->_sni.name};
        ::SSL_CTX_set_tlsext_servername_callback(context, sni_);
        ::SSL_CTX_set_tlsext_servername_arg(context, &this->_sniView);
        this->certAsync_();
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
    // the addresses blocked: forgotten once their time is over (never a map growing with every attacker)
    std::erase_if(this->_blocked, [now](const std::pair<const std::string, std::pair<int, std::int64_t>>& b) {return b.second.second != 0 && b.second.second < now;});
    if (this->_blocked.size() > 10000) std::erase_if(this->_blocked, [](const std::pair<const std::string, std::pair<int, std::int64_t>>& b) {return b.second.second == 0;});
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
    status.ttyPort = this->_ttyFd != -1 ? this->_ttyPort : 0;
    status.fingerprint = this->_fingerprint;
    status.certificate = this->_tls == nullptr ? "none" : this->_sni.context.load() != nullptr ? "tailscale" : "authority";
    status.certificateWhy = this->_certWhy;
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
        {"shell", cluster::CompletionKind::Shell}, {"ex", cluster::CompletionKind::Ex}, {"setting", cluster::CompletionKind::Setting}, {"session", cluster::CompletionKind::Session}};
    return kinds.contains(name) ? kinds.at(name) : cluster::CompletionKind::None;
}

_cold void cluster::Web::client_(const int fd, const std::string& peer, const bool tty)
{
    // One request per connection; this LAN only, https when on (a plain request: sent to https)
    Conn conn{fd};
    const timeval receive{10, 0};
    const timeval send{30, 0};
    const int nodelay = 1;
    (void)::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &receive, sizeof(receive));
    (void)::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &send, sizeof(send));
    (void)::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)); // a key, a frame: sent at once (no Nagle wait)
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
        else if (ready) this->request_(&conn, peer, this->_tls != nullptr && plain, tty);
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
    ip_give_(peer);
    --this->_busy;
}

_cold void cluster::Web::attach_(void* connection, const void* parsed, const cluster::Json& body, const std::string& peer)
{
    // the files of a prompt: uploaded (the body is the file), linked (a page on this computer), listed, removed, served back (only the
    // ones of the session asked, never a path of another session)
    Conn& conn = *static_cast<Conn*>(connection);
    const Request& request = *static_cast<const Request*>(parsed);
    const cluster::Config& config = this->_manager.config();
    const std::function<std::string(const char*)> query = [&request](const char* key) {return request.query.contains(key) ? request.query.at(key) : std::string();};
    const std::function<std::string(const char*)> text = [&body](const char* key) {return body.is_object() && body.contains(key) && body[key].is_string() ? body[key].get<std::string>() : std::string();};
    const bool post = request.method == "POST";
    try {
        const std::string wanted = post && request.path != "/api/attach" ? text("session") : query("session");
        const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(wanted.empty() ? GLOBAL_ID : wanted);
        if (!snap) {
            json_(conn, 404, {{"ok", false}, {"error", "no such session"}});
            return;
        }
        const std::string session = snap->spec.id;
        if (post && request.path == "/api/attach") {
            json_(conn, 200, {{"ok", true}, {"attachment", cluster::attach_store(config, session, query("name"), request.body).json()}});
        } else if (post && request.path == "/api/attach/link") {
            // a path of THIS computer: only from a page opened on it (the browsers never give the path of a file, it is typed or dropped as text)
            if (!(peer.starts_with("127.") || peer == "::1")) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "a file is linked from this computer only: upload it");
            json_(conn, 200, {{"ok", true}, {"attachment", cluster::attach_link(config, session, text("path"), cluster::AttachScope::User).json()}});
        } else if (post && request.path == "/api/attach/remove") {
            json_(conn, 200, {{"ok", cluster::attach_remove(config, session, text("id"))}});
        } else if (request.method == "GET" && request.path == "/api/attach/list") {
            cluster::Json list = cluster::Json::array();
            for (const cluster::Attachment& item: cluster::attach_list(config, session)) list.push_back(item.json());
            json_(conn, 200, {{"ok", true}, {"attachments", list}});
        } else if (request.method == "GET" && request.path == "/api/attach/file") {
            const std::string path = cluster::attach_resolve(config, session, query("path"));
            std::error_code error;
            if (path.empty() || std::filesystem::file_size(path, error) > 512ULL * 1024 * 1024) {
                json_(conn, 404, {{"error", "no such attachment"}});
                return;
            }
            std::ifstream in(path, std::ios::binary);
            const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            std::string mime = cluster::attach_sniff(bytes.substr(0, 4096));
            const bool image = mime == "image/png" || mime == "image/jpeg" || mime == "image/gif" || mime == "image/webp";
            if (!image) mime = "application/octet-stream"; // text, svg, html...: downloaded, never rendered by the page
            respond_(conn, 200, mime, bytes, std::string("Content-Disposition: ") + (image ? "inline" : "attachment") + "; filename=\"" + safe_filename_(std::filesystem::path(path).filename().string())
                + "\"\r\nContent-Security-Policy: sandbox; default-src 'none'\r\n");
        } else {
            json_(conn, 404, {{"error", "unknown"}});
        }
    } catch (const std::exception& error) {
        json_(conn, 200, {{"ok", false}, {"error", error.what()}});
    }
}

_cold void cluster::Web::request_(void* connection, const std::string& peer, const bool toHttps, const bool tty)
{
    // The right host, no other site; the page, the code; the browser connected for the API
    static const std::set<std::string> allowed = {"list", "read", "status", "send", "allow", "interrupt", "unqueue", "rewind", "spawn", "close", "rename", "describe", "resume", "apply", "mode",
        "model", "notices", "usage", "task_add", "task_list", "trash", "restore", "external", "import", "fork", "transcript", "ask", "display_act", "display_get", "pin", "unpin", "settings_get", "simple_words", "settings_suggest", "ask_preview", "settings_search", "settings_set", "ui_state", "delete",
        "projects", "agents", "agent_stop", "send_next", "install_package", "failure", "services", "service_act", "showcase", "task_act", "orch_state", "job_status", "job_kill", "job_stop", "job_resume", "job_prio", "schedule_act", "schedule_add", "schedule_list", "todos", "task_cancel", "schedule_cancel", "schedule_edit",
        "plugins", "plugins_set", "plugins_refresh"};
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
    attach_cap_ = static_cast<std::size_t>(this->_manager.config().attach.maxMb) * 1048576U;
    if (tty && request.method == "GET" && (request.path == "/" || request.path == "/index.html")) {
        // the terminal page (the browser only: the app has its page); its script by a nonce too
        const std::string agent = request.headers.contains("user-agent") ? request.headers.at("user-agent") : "";
        if (agent.find("claude-cluster-app/") != std::string::npos) {
            respond_(conn, 403, "text/plain", "claude-cluster: the terminal page is for a browser, not the app (its page: port " + std::to_string(this->_port) + ")\n");
            return;
        }
        const std::string nonce = random_hex_(16);
        std::string page(cluster::templates::TTY_PAGE);
        if (const std::size_t at = page.find("<script>"); at != std::string::npos) page.replace(at, 8, "<script nonce=\"" + nonce + "\">");
        if (const std::size_t at = page.find("__PAGE_PORT__"); at != std::string::npos) page.replace(at, 13, std::to_string(this->_port));
        // its WebSocket: this host only (wss / ws of the address it was opened by)
        const std::string socket = (this->_tls != nullptr ? "wss://" : "ws://") + host;
        respond_(conn, 200, "text/html; charset=utf-8", page, "Content-Security-Policy: default-src 'self'; style-src 'unsafe-inline'; script-src 'nonce-" + nonce
            + "'; img-src 'self' data:; connect-src 'self' " + socket + "; object-src 'none'; base-uri 'none'; form-action 'self'\r\n");
        return;
    }
    if (request.method == "GET" && (request.path == "/" || request.path == "/index.html")) {
        // its script allowed by a nonce of this response alone: a script injected in the page (an escaping bug) never runs
        const std::string nonce = random_hex_(16);
        std::string page(cluster::templates::WEB_PAGE);
        if (const std::size_t at = page.find("<script>"); at != std::string::npos) page.replace(at, 8, "<script nonce=\"" + nonce + "\">");
        respond_(conn, 200, "text/html; charset=utf-8", page,
            "Content-Security-Policy: default-src 'self'; style-src 'unsafe-inline'; script-src 'nonce-" + nonce + "'; img-src 'self' data: blob:; "
            "media-src 'self' data: blob:; worker-src 'self'; object-src 'none'; base-uri 'none'; form-action 'self'\r\nPermissions-Policy: microphone=(self)\r\n");
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
    if (request.method == "GET" && (request.path == "/docs" || request.path.starts_with("/docs/"))) { // the documentation: no login (nothing secret in it)
        docs_(conn, request);
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
    const cluster::Json body = post && request.path != "/api/listen" && request.path != "/api/tty/write" && request.path != "/api/attach"
        ? cluster::Json::parse(request.body.empty() ? "{}" : request.body, nullptr, false) : cluster::Json::object();
    const std::function<std::string(const char*)> field = [&body](const char* key) {
        return body.is_object() && body.contains(key) && body[key].is_string() ? body[key].get<std::string>() : std::string();
    };

    if (request.path.starts_with("/api/tty/")) {
        // the terminal of this device (the terminal port only, the browser only)
        const std::string agent = request.headers.contains("user-agent") ? request.headers.at("user-agent") : "";
        if (!tty || agent.find("claude-cluster-app/") != std::string::npos) {
            json_(conn, 404, {{"error", "the terminal page lives on its own port, for a browser"}});
            return;
        }
        const std::string device = cookie_(request, "cc_dev");
        if (request.method == "GET" && request.path == "/api/tty/ws") {
            this->ws_(connection, &request, device, peer);
        } else if (post && request.path == "/api/tty/open") {
            json_(conn, 200, this->_ttys.open(device, body.value("cols", 120), body.value("rows", 36)));
        } else if (request.method == "GET" && request.path == "/api/tty/read") {
            std::uint64_t from = 0;
            try {
                from = request.query.contains("from") ? std::stoull(request.query.at("from")) : 0;
            } catch (const std::exception&) {}
            json_(conn, 200, this->_ttys.read(device, from, 15000));
        } else if (post && request.path == "/api/tty/write") {
            this->_ttys.write(device, request.body);
            json_(conn, 200, {{"ok", true}});
        } else if (post && request.path == "/api/tty/resize") {
            this->_ttys.resize(device, body.value("cols", 120), body.value("rows", 36));
            json_(conn, 200, {{"ok", true}});
        } else if (post && request.path == "/api/tty/close") {
            this->_ttys.close(device);
            json_(conn, 200, {{"ok", true}});
        } else {
            json_(conn, 404, {{"error", "unknown"}});
        }
    } else if (request.path == "/api/logout") {
        json_(conn, 200, {{"ok", true}}, "Set-Cookie: cc_web=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict" + secure + "\r\n");
    } else if (post && request.path == "/api/ping") {
        // its round trip before this one, measured by the page (the ping of the devices)
        if (body.is_object() && body.contains("rtt") && body["rtt"].is_number()) this->ping(cookie_(request, "cc_dev"), body["rtt"].get<int>());
        json_(conn, 200, {{"ok", true}});
    } else if (request.method == "GET" && request.path == "/api/notify") {
        // the notifications of the desktop for the Android app: the ones after `since` (a number told by the last answer), waits for one
        // 25 s at most; no number (the app just started): only the current one, nothing replayed
        std::int64_t since = -1;
        try {
            since = request.query.contains("since") ? std::stoll(request.query.at("since")) : -1;
        } catch (const std::exception&) {}
        const std::int64_t end = now_ms_() + 25000;
        while (this->_running && since >= 0 && static_cast<std::int64_t>(this->_manager.pushSeq()) == since && now_ms_() < end)
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        json_(conn, 200, this->_manager.pushSince(since));
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
        for (const cluster::Snapshot& snap: this->_manager.shown()) {
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
        const cluster::OriginColors colors = cluster::origin_colors(); // the colours of the prompts by origin (the page has the defaults of each theme)
        const cluster::Json promptColors = {{"on", colors.enabled}, {"user", colors.user}, {"mcp", colors.mcp}, {"task", colors.task}};
        const cluster::VoiceConf& voice = this->_manager.config().voice;
        const cluster::AppFace face = this->_manager.appFace(false);
        // the Tasks session of the list: what waits / runs, what is scheduled
        const std::vector<cluster::Task> tasks = this->_manager.tasks();
        const std::vector<cluster::Schedule> planned = this->_manager.schedules();
        const auto open = std::count_if(tasks.begin(), tasks.end(), [](const cluster::Task& t) {return t.status == "queued" || t.status == "running";});
        const auto waiting = std::count_if(planned.begin(), planned.end(), [](const cluster::Schedule& w) {return w.status == "waiting";});
        // the taxonomy of the labels (the graph nests the sessions by it) and the labels of the archived sessions (greyed in the graph)
        cluster::Json archived = cluster::Json::array();
        for (const cluster::SessionSpec& spec: this->_manager.trash())
            archived.push_back({{"id", spec.id}, {"name", spec.name}, {"category", spec.category}, {"tags", spec.tags}});
        json_(conn, 200, {{"version", version()}, {"remote", this->_remoteUrl}, {"ask", this->_manager.asking().json()}, {"sessions", sessions}, {"session", session_json_(this->_manager, wanted)},
            {"group", this->_manager.ui("group_by_category", config.groupByCategory ? "true" : "false") == "true"},
            {"taxonomy", this->_manager.taxonomyJson()}, {"archived", archived},
            {"tasks_open", open}, {"schedules_waiting", waiting}, {"simple", cluster::simple_json(this->_manager)},
            {"progress", this->_manager.progressAll()}, {"now_ms", cluster::now_ms()}, {"failures", this->_manager.failures()},
            {"face", {{"name", cluster::face_name(face.face)}, {"text", cluster::face_text(face.face)}, {"badge", face.badge},
                {"svg", cluster::face_svg(face.face, face.badge, face.urgent)}}},
            {"attach", {{"enabled", config.attach.enabled}, {"root", cluster::attach_root(config).string()}, {"max_mb", config.attach.maxMb}, {"max_files", config.attach.maxFiles}, {"link", config.attach.link && (peer.starts_with("127.") || peer == "::1")}}},
            {"client", peer}, {"device", cookie_(request, "cc_dev")}, {"tty_port", this->_ttyFd != -1 ? this->_ttyPort : 0}, {"style", style}, {"prompt_colors", promptColors}, {"version_app", CLUSTER_VERSION},
            {"update", [this]() { // a newer claude-cluster installed, an update waiting (chosen in a terminal or a window of the computer)
                const cluster::UpgradeOffer offer = this->_manager.upgradeOffer();
                return cluster::Json{{"version", offer.version}, {"started", offer.started}, {"upgrading", offer.upgrading}, {"target", offer.target}, {"waiting", offer.waiting},
                    {"jump", offer.jump}, {"notes", offer.notes}};
            }()}, {"voice", {{"available", this->_speech != nullptr}, {"confirm", voice.confirmSeconds},
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
            const std::string name = safe_filename_(path.filename().string());
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
        command["origin"] = "web"; // a device of the page (never the global session nor an interface, whatever it says)
        const std::string cmd = command.value("cmd", std::string());
        if (cmd == "spawn" && command.contains("cwd") && command["cwd"].is_string()) { // a relative folder: from the home folder
            const std::string cwd = cluster::trim(command["cwd"].get<std::string>());
            if (!cwd.starts_with("/") && !cwd.starts_with("~")) command["cwd"] = "~/" + cwd;
        }
        // a shell of this computer from the page: only when allowed ([web] shell)
        const std::string text = command.contains("text") && command["text"].is_string() ? cluster::trim(command["text"].get<std::string>()) : "";
        const bool shell = cmd == "send" && (text.starts_with("!") || (text.starts_with(":") && text.find('!') != std::string::npos));
        if (!allowed.contains(cmd) || (cluster::demo_enabled() && !cluster::demo_web_allows(cmd))) {
            json_(conn, 400, {{"ok", false}, {"error", cluster::demo_enabled() && allowed.contains(cmd) ? "the demo is read only from the web: '" + cmd + "' is not available"
                : "command not allowed from the web: '" + cmd + "'"}});
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
        if (field("kind") == "known") { // every name a /word may be (the colours of the prompt)
            json_(conn, 200, {{"names", cluster::slash_known(this->_manager, session)}});
            return;
        }
        const cluster::CompletionProvider provider = cluster::completion_provider(this->_manager, [session]() {return session;});
        const cluster::CompletionKind kind = completion_kind_(field("kind"));
        cluster::Json items = cluster::Json::array();
        if (kind != cluster::CompletionKind::None)
            for (const cluster::Candidate& c: provider(kind, field("query")))
                items.push_back({{"value", c.value}, {"label", c.label}, {"description", c.description}, {"directory", c.directory}, {"source", c.source}});
        json_(conn, 200, {{"items", items}});
    } else if (post && request.path == "/api/listen") {
    } else if (request.path.starts_with("/api/attach")) {
        this->attach_(connection, &request, body, peer);
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
    // from outside: always approved by the host; 5 wrong codes block the address 15 minutes (the LAN too: no code rotated on purpose)
    const bool outside = !same_lan_(peer);
    if (request.method != "GET" && this->_blocked.contains(peer) && this->_blocked.at(peer).second > now_ms_()) {
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
    if (trusted != this->_trusted.end() && !cookie_(request, "cc_trust").empty() && same_secret_(cookie_(request, "cc_trust"), trusted->second.secret)) {
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
    } else if (!same_secret_(digits, this->_pin)) {
        if (++this->_blocked[peer].first >= 5) {
            this->_blocked[peer] = {0, now_ms_() + 15 * 60 * 1000};
            this->_manager.notice(GLOBAL_ID, "web: 5 wrong codes from " + peer + (outside ? " (outside the LAN)" : "") + ": blocked 15 min", true);
            (void)this->_manager.notify({"web-blocked", "web access", "5 wrong codes from " + peer + (outside ? " (outside the LAN)" : "") + ": blocked 15 min",
                cluster::Face::Waiting, true, "dialog-warning"}, "web");
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
        this->_blocked.erase(peer);
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
        status.ttyPort = data.value("tty_port", 0);
        status.fingerprint = data.value("fingerprint", std::string());
        status.certificate = data.value("certificate", std::string());
        status.certificateWhy = data.value("certificate_why", std::string());
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

/* the WebSocket of the terminal page */
_cold void cluster::Web::ws_(void* connection, const void* data, const std::string& device, const std::string& peer)
{
    // its upgrade: from the page of this computer only (its Origin: a WebSocket is never stopped by the browser for
    // another site), a WebSocket of version 13; its terminal opened (or kept), then its frames
    Conn& conn = *static_cast<Conn*>(connection);
    const Request& request = *static_cast<const Request*>(data);
    const std::function<std::string(const char*)> header = [&request](const char* name) {
        return request.headers.contains(name) ? request.headers.at(name) : std::string();
    };
    const std::string scheme = this->_tls == nullptr ? "http://" : "https://";
    if (header("origin") != scheme + header("host")) {
        json_(conn, 403, {{"error", "another site"}});
        return;
    }
    if (cluster::lower(header("upgrade")) != "websocket" || cluster::lower(header("connection")).find("upgrade") == std::string::npos
        || header("sec-websocket-version") != "13" || header("sec-websocket-key").size() != 24) {
        json_(conn, 400, {{"error", "a WebSocket (version 13) only"}});
        return;
    }
    #ifndef CLUSTER_TLS
        json_(conn, 501, {{"error", "no WebSocket in this build: the page polls"}});
        (void)device;
        (void)peer;
    #else
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (this->_sockets[device] >= 4) {
                json_(conn, 429, {{"error", "4 terminal pages of this device at once at most"}});
                return;
            }
            ++this->_sockets[device];
        }
        try {
            int cols = 120;
            int rows = 36;
            try {
                if (request.query.contains("cols")) cols = std::stoi(request.query.at("cols"));
                if (request.query.contains("rows")) rows = std::stoi(request.query.at("rows"));
            } catch (const std::exception&) {}
            (void)this->_ttys.open(device, cols, rows);
            // Sec-WebSocket-Accept: base64(SHA-1(key + the GUID of RFC 6455))
            const std::string text = header("sec-websocket-key") + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
            unsigned int size = 0;
            if (::EVP_Digest(text.data(), text.size(), digest.data(), &size, ::EVP_sha1(), nullptr) == 1) {
                const std::string accept = utils::smanip::codec::Base64Codec().encode(std::string(reinterpret_cast<const char*>(digest.data()), size));
                if (send_all_(conn, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n"))
                    this->wsLoop_(connection, device, peer, cookie_(request, "cc_web"));
            }
        } catch (const std::exception& e) {
            json_(conn, 500, {{"error", e.what()}});
        }
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (--this->_sockets[device] <= 0) this->_sockets.erase(device);
    #endif
}

_cold void cluster::Web::wsLoop_(void* connection, const std::string& device, const std::string& peer, const std::string& token)
{
    // one thread: the keys and the messages of the page read, the frames of its terminal sent (the rows changed, 60
    // fps at most, at once after a quiet time); its device checked every second, the page silent 30 s: closed
    Conn& conn = *static_cast<Conn*>(connection);
    const int flags = ::fcntl(conn.fd, F_GETFL, 0);
    (void)::fcntl(conn.fd, F_SETFL, flags | O_NONBLOCK);
    const std::function<std::int64_t()> now = []() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    };
    int watcher = this->_ttys.watch(device);
    std::vector<std::string> sent;
    bool full = true;
    bool dirty = true;
    std::int64_t dirtySince = now();  // its first output not sent yet
    std::int64_t nextFrame = 0;
    std::int64_t heard = now();
    std::int64_t checked = now();
    std::int64_t pinged = now();
    std::string in;
    std::string message;    // a message in fragments
    int messageOp = 0;
    std::array<char, 16384> chunk{};
    const std::function<void(int, const std::string&)> close = [&conn](const int code, const std::string& why) {
        std::string payload(1, static_cast<char>((code >> 8) & 0xFF));
        payload += static_cast<char>(code & 0xFF);
        payload += why.substr(0, 100);
        (void)ws_send_(conn, 0x8, payload);
    };
    // one message of the page: its keys (binary), {t: resize | ping | full} (text)
    const std::function<bool(int, const std::string&)> handle = [&](const int op, const std::string& payload) {
        if (op == 0x2) {
            this->_ttys.write(device, payload);
            return true;
        }
        const cluster::Json json = cluster::Json::parse(payload, nullptr, false);
        if (!json.is_object()) return true;
        const std::string type = json.value("t", std::string());
        if (type == "resize") {
            this->_ttys.resize(device, json.value("cols", 120), json.value("rows", 36));
            dirty = true;
        } else if (type == "ping") { // the page measures its latency: answered at once
            return ws_send_(conn, 0x1, cluster::Json{{"t", "pong"}, {"n", json.value("n", 0.0)}}.dump(-1, ' ', false, cluster::JSON_LENIENT));
        } else if (type == "full") {
            full = true;
            dirty = true;
        }
        return true;
    };
    while (this->_running) {
        // the device still connected (kicked, logged out, banned, another address: closed); seen: kept
        if (now() - checked >= 1000) {
            checked = now();
            bool still = false;
            {
                std::lock_guard<std::mutex> lock(this->_mutex);
                auto it = this->_access.find(device);
                still = it != this->_access.end() && it->second.device.state == "connected" && !token.empty() && same_secret_(it->second.token, token)
                    && it->second.device.address == peer;
                if (still) it->second.device.seen = now_ms_();
            }
            if (!still) {
                close(4001, "not connected");
                break;
            }
            if (now() - heard > 30000) { // the page gone without a word (its pongs included)
                close(4002, "silent");
                break;
            }
            if (now() - pinged >= 10000) { // a ping of the server: a page in a background tab answers it, throttled or not
                pinged = now();
                if (!ws_send_(conn, 0x9, "")) break;
            }
            if (watcher < 0) watcher = this->_ttys.watch(device); // its terminal opened again elsewhere
        }
        // a frame waiting: sent once the terminal ended its update (at once), 2 ms polls meanwhile
        const int wait = conn_pending_(conn) ? 0 : dirty ? std::max(1, static_cast<int>(std::max<std::int64_t>(0, nextFrame - now()))) : 250;
        std::array<pollfd, 2> fds{pollfd{conn.fd, POLLIN, 0}, pollfd{watcher, POLLIN, 0}};
        (void)::poll(fds.data(), watcher >= 0 ? 2 : 1, wait);
        if (watcher >= 0 && (fds[1].revents & POLLIN) != 0) {
            std::uint64_t count = 0;
            (void)!::read(watcher, &count, sizeof(count));
            if (!dirty) dirtySince = now();
            dirty = true;
        }
        if ((fds[0].revents & (POLLIN | POLLHUP | POLLERR)) != 0 || conn_pending_(conn)) {
            bool gone = false;
            while (true) {
                const ssize_t got = conn_read_now_(conn, chunk.data(), chunk.size());
                if (got < 0) gone = true;
                if (got <= 0) break;
                in.append(chunk.data(), static_cast<std::size_t>(got));
                if (in.size() > 2000000) gone = true;
                if (gone) break;
            }
            // its frames: masked (a page), 1 MB at most, no extension bit
            bool stop = gone;
            while (!stop && in.size() >= 2) {
                const unsigned char b0 = static_cast<unsigned char>(in[0]);
                const unsigned char b1 = static_cast<unsigned char>(in[1]);
                const bool fin = (b0 & 0x80) != 0;
                const int op = b0 & 0x0F;
                std::uint64_t length = b1 & 0x7F;
                std::size_t at = 2;
                if ((b0 & 0x70) != 0 || (b1 & 0x80) == 0) {
                    close(1002, "protocol");
                    stop = true;
                    break;
                }
                if (length == 126) {
                    if (in.size() < 4) break;
                    length = (static_cast<std::uint64_t>(static_cast<unsigned char>(in[2])) << 8) | static_cast<unsigned char>(in[3]);
                    at = 4;
                } else if (length == 127) {
                    if (in.size() < 10) break;
                    length = 0;
                    for (std::size_t i = 2; i < 10; ++i)
                        length = (length << 8) | static_cast<unsigned char>(in[i]);
                    at = 10;
                }
                if (length > 1000000 || message.size() + length > 1000000) {
                    close(1009, "too big");
                    stop = true;
                    break;
                }
                if (in.size() < at + 4 + length) break;
                std::string payload = in.substr(at + 4, static_cast<std::size_t>(length));
                for (std::size_t i = 0; i < payload.size(); ++i)
                    payload[i] = static_cast<char>(payload[i] ^ in[at + (i % 4)]);
                in.erase(0, at + 4 + static_cast<std::size_t>(length));
                heard = now();
                if (op == 0x8) { // closed by the page
                    close(1000, "bye");
                    stop = true;
                } else if (op == 0x9) {
                    stop = !ws_send_(conn, 0xA, payload);
                } else if (op == 0xA) { // the pong of our ping: heard (above), nothing else
                } else if (op == 0x0) { // the next fragment
                    message += payload;
                    if (fin) {
                        stop = !handle(messageOp, message);
                        message.clear();
                    }
                } else if (op == 0x1 || op == 0x2) {
                    if (fin) {
                        stop = !handle(op, payload);
                    } else {
                        messageOp = op;
                        message = payload;
                    }
                }
            }
            if (stop) break;
        }
        // the screen whole (its update ended) or waited 50 ms; 4 ms between two frames at least (250 / s)
        if (dirty && now() >= nextFrame && (this->_ttys.settled(device) || now() - dirtySince > 50)) {
            const cluster::Json frame = this->_ttys.frame(device, sent, full);
            full = false;
            dirty = false;
            nextFrame = now() + 4;
            if (!frame.is_null()) {
                if (!ws_send_(conn, 0x1, frame.dump(-1, ' ', false, cluster::Json::error_handler_t::replace))) break;
                if (!frame.value("alive", true)) { // the terminal quit: told, closed
                    close(1000, "the terminal quit");
                    break;
                }
            }
        }
    }
    this->_ttys.unwatch(device, watcher);
}
