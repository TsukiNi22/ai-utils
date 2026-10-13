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
#include "cluster/Tools.hpp"
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
_cold cluster::Web::Web(cluster::Manager& manager, cluster::Voice* voice, const std::optional<bool> force)
    : _manager(manager), _voice(voice), _speech(voice != nullptr ? std::make_unique<cluster::WebVoice>(manager, *voice) : nullptr), _force(force)
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
    if (this->_speech) this->_speech->tick();
    if (wanted && (this->_fd == -1 || this->_port != config.web.port || this->_httpsWanted != config.web.https)) {
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
    this->_httpsWanted = this->_manager.config().web.https;
    const std::string tls = this->_httpsWanted ? this->tls_() : "";
    if (!tls.empty()) this->_manager.notice(GLOBAL_ID, "web access: no https (" + tls + "), plain http: no microphone in the page", true);
    this->newPin_();
    this->_running = true;
    this->_thread = std::thread(&cluster::Web::serve_, this);
    this->_manager.notice(GLOBAL_ID, "web access open: " + std::string(this->_tls != nullptr ? "https://" : "http://") + lan_address_() + ":"
        + std::to_string(port) + " code " + this->_pin.substr(0, 3) + " " + this->_pin.substr(3) + " (this network only, one browser at a time)");
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
    #ifdef CLUSTER_TLS
        if (this->_tls != nullptr) ::SSL_CTX_free(static_cast<SSL_CTX*>(this->_tls));
    #endif
    this->_tls = nullptr;
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

_cold std::string cluster::Web::tls_(void)
{
    // a self-signed certificate made once (config folder / web, its key 0600): the browser asks to accept it the first
    // time; its SHA-256 shown here to compare with the one of the browser
    #ifdef CLUSTER_TLS
        const std::filesystem::path dir = cluster::config_dir() / "web";
        const std::filesystem::path cert = dir / "cert.pem";
        const std::filesystem::path key = dir / "key.pem";
        std::error_code error;
        if (!std::filesystem::exists(cert) || !std::filesystem::exists(key)) {
            std::filesystem::create_directories(dir, error);
            ::chmod(dir.c_str(), S_IRWXU);
            std::array<char, 256> own{};
            (void)::gethostname(own.data(), own.size() - 1);
            std::string names = "subjectAltName=IP:127.0.0.1,DNS:localhost";
            if (lan_address_() != "127.0.0.1") names += ",IP:" + lan_address_();
            if (own[0] != '\0') names += ",DNS:" + std::string(own.data()) + ",DNS:" + std::string(own.data()) + ".local";
            const cluster::Captured made = cluster::capture({"openssl", "req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:prime256v1", "-nodes",
                "-days", "3650", "-subj", "/CN=claude-cluster", "-addext", names, "-keyout", key.string(), "-out", cert.string()}, "", 30);
            ::chmod(key.c_str(), S_IRUSR | S_IWUSR);
            if (made.code != 0) return "openssl could not make its certificate: " + cluster::one_line(made.out, 160);
        }
        SSL_CTX* context = ::SSL_CTX_new(::TLS_server_method());
        if (context == nullptr) return "OpenSSL";
        (void)::SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
        if (::SSL_CTX_use_certificate_file(context, cert.c_str(), SSL_FILETYPE_PEM) != 1 || ::SSL_CTX_use_PrivateKey_file(context, key.c_str(), SSL_FILETYPE_PEM) != 1) {
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
    status.url = std::string(this->_tls != nullptr ? "https://" : "http://") + lan_address_() + ":" + std::to_string(this->_port);
    status.https = this->_tls != nullptr;
    status.fingerprint = this->_fingerprint;
    status.pin = this->_pin.substr(0, 3) + " " + this->_pin.substr(3);
    status.client = this->_client;
    return status;
}

/* requests */
_cold static cluster::CompletionKind completion_kind_(const std::string& name)
{
    static const std::map<std::string, cluster::CompletionKind> kinds = {{"file", cluster::CompletionKind::File},
        {"folder", cluster::CompletionKind::Folder}, {"command", cluster::CompletionKind::Command}, {"mode", cluster::CompletionKind::Mode},
        {"model", cluster::CompletionKind::Model}, {"effort", cluster::CompletionKind::Effort}, {"trash", cluster::CompletionKind::Trash}};
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
        if (this->_tls != nullptr && same_lan_(peer)) {
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
        if (!same_lan_(peer)) respond_(conn, 403, "text/plain", "claude-cluster: this network only\n");
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
    static const std::set<std::string> allowed = {"list", "read", "status", "send", "allow", "interrupt", "spawn", "close", "rename", "mode",
        "model", "notices", "usage", "task_add", "task_list", "trash", "restore"};
    Conn& conn = *static_cast<Conn*>(connection);
    Request request;
    if (!read_request_(conn, request)) return;
    const std::string host = request.headers.contains("host") ? request.headers.at("host") : "";
    const std::string origin = request.headers.contains("origin") ? request.headers.at("origin") : "";
    const bool post = request.method == "POST";
    const std::string scheme = this->_tls == nullptr ? "http://" : "https://";
    const std::string secure = this->_tls != nullptr ? "; Secure" : "";

    if (!host_ok_(host)) {
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
            "Content-Security-Policy: default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src 'self' data:; "
            "media-src 'self' blob:\r\nPermissions-Policy: microphone=(self)\r\n");
        return;
    }
    if (post && request.path == "/api/login") {
        this->login_(connection, request.body, cookie_(request, "cc_web"), peer, secure);
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
        connected = !this->_token.empty() && cookie_(request, "cc_web") == this->_token && peer == this->_client;
        if (connected) this->_seen = now_ms_();
        if (connected && post && request.path == "/api/logout") {
            this->_manager.notice(GLOBAL_ID, "web: " + peer + " left");
            this->_token.clear();
            this->_client.clear();
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
        const cluster::VoiceConf& voice = this->_manager.config().voice;
        json_(conn, 200, {{"version", this->_manager.version()}, {"sessions", sessions}, {"session", session_json_(this->_manager, wanted)},
            {"client", peer}, {"voice", {{"available", this->_speech != nullptr}, {"confirm", voice.confirmSeconds},
                {"end_silence", voice.endSilence > 0.0 ? voice.endSilence : 1.0}, {"listen", voice.listenModel}, {"say", voice.sayEngine}}}});
    } else if (post && request.path == "/api/cmd") {
        cluster::Json command = body.is_object() ? body : cluster::Json::object();
        const std::string cmd = command.value("cmd", std::string());
        if (cmd == "spawn" && command.contains("cwd") && command["cwd"].is_string()) { // a relative folder: from the home folder
            const std::string cwd = cluster::trim(command["cwd"].get<std::string>());
            if (!cwd.starts_with("/") && !cwd.starts_with("~")) command["cwd"] = "~/" + cwd;
        }
        if (!allowed.contains(cmd)) json_(conn, 400, {{"ok", false}, {"error", "command not allowed from the web: '" + cmd + "'"}});
        else json_(conn, 200, cluster::Control::handle(this->_manager, command));
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

_cold void cluster::Web::login_(void* connection, const std::string& requestBody, const std::string& cookie, const std::string& peer, const std::string& secure)
{
    // the code: one browser at a time (another one refused while it is there), 5 wrong ones: a new code
    Conn& conn = *static_cast<Conn*>(connection);
    const cluster::Json body = cluster::Json::parse(requestBody.empty() ? "{}" : requestBody, nullptr, false);
    const std::string typed = body.is_object() ? body.value("pin", std::string()) : std::string();
    std::string digits;
    for (const char c: typed)
        if (std::isdigit(static_cast<unsigned char>(c))) digits += c;
    std::unique_lock<std::mutex> lock(this->_mutex);
    this->expire_();
    if (!this->_token.empty() && cookie != this->_token) {
        json_(conn, 423, {{"error", "already used by " + this->_client + ": one browser at a time"}});
    } else if (digits != this->_pin) {
        if (++this->_failures >= 5) {
            this->newPin_();
            this->_manager.notice(GLOBAL_ID, "web: 5 wrong codes (last from " + peer + "), new code " + this->_pin.substr(0, 3) + " "
                + this->_pin.substr(3), true);
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        json_(conn, 401, {{"error", "wrong code"}});
    } else {
        this->_failures = 0;
        this->_token = random_hex_(24);
        this->_client = peer;
        this->_seen = now_ms_();
        this->_manager.notice(GLOBAL_ID, "web: " + peer + " connected");
        json_(conn, 200, {{"ok", true}}, "Set-Cookie: cc_web=" + this->_token + "; Path=/; HttpOnly; SameSite=Strict" + secure + "\r\n");
    }
}

/* functions */
_cold std::string cluster::web_line(const cluster::WebStatus& status)
{
    if (!status.on) return status.error.empty() ? "" : "web: port not opened (" + status.error + ")";
    return "web " + status.url + " · code " + status.pin + " · " + (status.client.empty() ? std::string("free") : status.client + " connected");
}
