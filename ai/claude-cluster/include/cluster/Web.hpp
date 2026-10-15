/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file Web.hpp

File Description:
##  Web access of claude-cluster on the local network: a page in a
##  browser of the same LAN drives it, after a 6-digit code and the
##  approval of the host; several devices at once, the trusted ones
##  without code (removable), kicked one by one or all
\**************************************************************/

#ifndef WEB_H
    #define WEB_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "WebVoice.hpp"         // cluster::WebVoice
    #include "Manager.hpp"          // cluster::Manager
    #include "Voice.hpp"            // cluster::Voice
    #include <optional>             // std::optional
    #include <cstdint>              // std::int64_t
    #include <vector>               // std::vector
    #include <memory>               // std::unique_ptr
    #include <string>               // std::string
    #include <atomic>               // std::atomic
    #include <thread>               // std::thread
    #include <mutex>                // std::mutex
    #include <map>                  // std::map

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// A device of the web access: waiting for the host, connected, or remembered (trusted)
struct WebDevice {
    std::string id;                 // its id (a cookie of its browser)
    std::string name;               // "Firefox · Linux", or the name typed in its page
    std::string agent;              // its user agent
    std::string address;            // its address on the LAN
    std::string state;              // waiting (the host to approve) | connected | denied | trusted (remembered, not connected)
    bool trusted = false;           // no code, no approval
    std::int64_t since = 0;         // asked / connected / trusted (ms)
    std::int64_t seen = 0;          // its last request (ms)
};

struct WebStatus {
    bool on = false;                // the port is open
    std::string url;                // http://<the LAN address>:<port>
    std::string pin;                // the code asked by the page: "123 456"
    std::string client;             // the devices connected (their names), empty: none
    std::vector<cluster::WebDevice> devices;    // waiting and connected
    std::vector<cluster::WebDevice> trusted;    // remembered
    std::string error;              // why the port could not be opened
    bool https = false;             // TLS (self-signed certificate): the microphone of the page works
    std::string fingerprint;        // SHA-256 of the certificate, to compare with the one the browser shows
    std::string remoteUrl;          // reached from outside ([web] remote), empty: not
    std::string remoteState;        // how it is reached from outside, or why not
};

//----------------------------------------------------------------//
/* FUNCTION */

// "web http://192.168.1.12:8787 · code 123 456 · free" (connected: its address), empty: closed
_cold _nodiscard std::string web_line(const cluster::WebStatus& status);

//----------------------------------------------------------------//
/* CLASS */

class Web {
    private:
        cluster::Manager& _manager;
        cluster::Voice* _voice;         // its words, the proper names, the voice of the answers (nullptr: no voice in the page)
        std::unique_ptr<cluster::WebVoice> _speech;
        mutable std::mutex _mutex;
        std::mutex _portMutex;          // tick: the ticker and a command (web on / off) never open / close the port together
        int _fd = -1;
        int _port = 0;
        std::atomic<bool> _running{false};
        std::atomic<int> _busy{0};      // connections being served (waited for at the end)
        std::thread _thread;
        std::optional<bool> _force;     // web on / off, --web: over [web] enabled for this run
        std::string _pin;               // 6 digits, new at each opening and after 5 wrong ones
        int _failures = 0;
        struct Access {
            cluster::WebDevice device;
            std::string token;          // its cookie while it is connected (cc_web)
            std::string secret;         // trusted: its long cookie (cc_trust)
            bool giveTrust = false;     // trusted just now: its cookie given at its next look
        };
        std::map<std::string, Access> _access;      // by device: waiting / connected / denied (gone after 30 s without a request)
        std::map<std::string, Access> _trusted;     // by device: remembered (config folder / web / trusted.json, 0600)
        std::string _error;
        void* _tls = nullptr;           // SSL_CTX of https (nullptr: plain http)
        bool _httpsWanted = false;      // [web] https when it was opened
        std::string _fingerprint;
        std::string _remoteWanted;                  // [web] remote, remote_host, remote_port when it was opened
        std::vector<std::string> _remoteNames;      // the names / addresses used from outside (the certificate, the host checked)
        std::string _remoteUrl;
        std::string _remoteState;
        int _mapped = 0;                            // the port of the router opened by UPnP (closed with it)
        std::map<std::string, std::pair<int, std::int64_t>> _blocked; // a peer from outside: its wrong codes, blocked until (ms)

        // ---------- Pre-Function -------- //
        _cold void open_(const int port);
        _cold void close_(void);
        _cold void serve_(void);
        _cold void client_(const int fd, const std::string& peer);
        _cold void request_(void* connection, const std::string& peer, const bool toHttps);
        _cold void login_(void* connection, const void* request, const std::string& peer, const std::string& secure);
        _cold void ask_(const std::string& id);    // the host asked to approve it (popup, notification)
        _cold void loadTrusted_(void);
        _cold void saveTrusted_(void) const;
        _cold _nodiscard std::string tls_(void);   // https set up (its certificate made once), error text, empty: on
        _cold void newPin_(void);
        _cold void expire_(void);       // the devices gone, the waits too old (under _mutex)
        _cold void remote_(void);       // reached from outside: its names, its address, the port of the router (UPnP)
        _cold _nodiscard bool allowed_(const std::string& peer) const;  // this LAN, or from outside when [web] remote allows it

    public:
        // ---------- Pre-Function -------- //
        _cold void tick(void);              // the port follows [web] enabled / web on|off, [web] port
        _cold void setOn(const bool on);    // for this run
        _cold void kick(const std::vector<std::string>& ids = {});     // these devices out (none: all of them, a new code)
        _cold void approve(const std::string& id, const std::string& answer); // a device waiting: allow | trust | deny
        _cold void untrust(const std::vector<std::string>& ids = {});  // remembered no more (none: all), out too
        _cold void renewPin(void);
        _cold _nodiscard cluster::WebStatus status(void) const;

        // ---------- Constructor -------- //
        _cold Web(cluster::Manager& manager, cluster::Voice* voice, const std::optional<bool> force);

        // ----------- Destructor --------- //
        _cold ~Web();
};

} // namespace end

#endif /* WEB_H */
