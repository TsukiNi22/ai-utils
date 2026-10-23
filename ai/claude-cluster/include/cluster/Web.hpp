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
    #include "WebTty.hpp"           // cluster::WebTty
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
    int ping = -1;                  // its round trip (ms, smoothed; measured by its page every 5 s), -1: not yet
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
    int ttyPort = 0;                // the terminal page served on this port (0: none)
    std::string fingerprint;        // SHA-256 of the certificate, to compare with the one the browser shows
    std::string certificate;        // the certificate served: tailscale (a real one of the tailnet) | authority (of this computer) | none
    std::string certificateWhy;     // tailscale asked but its certificate not got: why (what to do)
    std::string remoteUrl;          // reached from outside ([web] remote), empty: not
    std::string remoteState;        // how it is reached from outside, or why not
    bool remoteOk = false;          // reached from outside for sure (the router opened its port, tailscale on, a name given)
    std::vector<cluster::WebDevice> banned;     // refused (their code, their trust), until unbanned
    int ping = -1;                  // the mean of the devices connected (ms), -1: none measured
};

//----------------------------------------------------------------//
/* FUNCTION */

// "web http://192.168.1.12:8787 · code 123 456 · 2 connected · ping 24 ms", empty: closed
_cold _nodiscard std::string web_line(const cluster::WebStatus& status);
// the web access of the core, here or from an interface attached to it (the control, its state kept 1 s): none: no web access
_cold _nodiscard std::optional<cluster::WebStatus> web_status(cluster::Manager& manager);
// claude-cluster docs serve: the documentation alone on a port, read only (GET of /docs, no core, no session); lan: the peers of the LAN too
_cold _nodiscard int docs_serve(const int port, const bool lan);
// on | off | pin | kick [ids] | approve <id> <answer> | untrust [ids] | ban [ids] | unban [ids] (ids: none, all)
_cold void web_act(cluster::Manager& manager, const std::string& action, const std::vector<std::string>& ids = {}, const std::string& id = "",
    const std::string& answer = "");

//----------------------------------------------------------------//
/* CLASS */

// What the TLS callback of the web access reads: the certificate of the tailnet, the name it is served to
struct WebSniView {
    const std::atomic<void*>* context = nullptr;
    const std::string* name = nullptr;
};

class Web {
    private:
        cluster::Manager& _manager;
        cluster::Voice* _voice;         // its words, the proper names, the voice of the answers (nullptr: no voice in the page)
        std::unique_ptr<cluster::WebVoice> _speech;
        mutable std::mutex _mutex;
        std::mutex _portMutex;          // tick: the ticker and a command (web on / off) never open / close the port together
        int _fd = -1;
        int _port = 0;
        int _ttyFd = -1;                // the terminal page: its own port ([web] tty_port)
        int _ttyPort = 0;
        cluster::WebTty _ttys;          // the terminals of the page
        int _mappedTty = 0;             // the port of the router opened for it
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
        struct Sni {
            std::atomic<void*> context{nullptr};    // SSL_CTX of the certificate of the tailnet (Let's Encrypt)
            std::string name;                       // its name (<computer>.<tailnet>.ts.net): asked by it (SNI), served
        } _sni;
        std::string _certWhy;                       // the certificate of the tailnet not got: why
        std::thread _certThread;                    // getting it (tailscale cert: seconds, a minute): never the web waiting
        std::atomic<bool> _certBusy{false};
        std::int64_t _sniAt = 0;                    // unix s it was got (renewed every day)
        cluster::WebSniView _sniView;               // what its TLS callback reads
        bool _httpsWanted = false;      // [web] https when it was opened
        std::string _fingerprint;
        std::string _remoteWanted;                  // [web] remote, remote_host, remote_port when it was opened
        std::vector<std::string> _remoteNames;      // the names / addresses used from outside (the certificate, the host checked)
        std::string _remoteUrl;
        std::string _remoteState;
        int _mapped = 0;                            // the port of the router opened by UPnP (closed with it)
        bool _remoteOk = false;                     // reached from outside (see WebStatus)
        std::string _remoteWhy;                     // why not
        std::map<std::string, std::pair<int, std::int64_t>> _blocked; // a peer from outside: its wrong codes, blocked until (ms)
        std::map<std::string, cluster::WebDevice> _banned;              // by device (config folder / web / banned.json)
        std::map<std::string, int> _sockets;        // the WebSockets of the terminal page, by device (4 at most)

        // ---------- Pre-Function -------- //
        _cold void open_(const int port);
        _cold void close_(void);
        _cold void serve_(void);
        _cold void client_(const int fd, const std::string& peer, const bool tty);
        _cold void request_(void* connection, const std::string& peer, const bool toHttps, const bool tty);
        _cold void attach_(void* connection, const void* parsed, const cluster::Json& body, const std::string& peer);   // /api/attach...
        _cold _nodiscard static int listen_(const int port, std::string& error);  // a socket listening on every interface, -1: error
        _cold void login_(void* connection, const void* request, const std::string& peer, const std::string& secure);
        _cold void ws_(void* connection, const void* request, const std::string& device, const std::string& peer);  // the WebSocket of the terminal page
        _cold void wsLoop_(void* connection, const std::string& device, const std::string& peer, const std::string& token);
        _cold void ask_(const std::string& id);    // the host asked to approve it (popup, notification)
        _cold void loadTrusted_(void);
        _cold void saveTrusted_(void) const;
        _cold void loadBanned_(void);
        _cold void saveBanned_(void) const;
        _cold _nodiscard bool banned_(const std::string& id, const std::string& peer) const; // its device, or its address from outside
        _cold _nodiscard std::string tls_(void);   // https set up (its certificate made once), error text, empty: on
        _cold void tailnetCert_(void);              // the certificate of the tailnet got (tailscale cert), swapped in: _sni
        _cold void certAsync_(void);                // the same in a thread of its own, its popup, its failure told
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
        _cold void ban(const std::vector<std::string>& ids);            // refused from now (out, trusted no more), kept
        _cold void unban(const std::vector<std::string>& ids = {});    // allowed again (none: all)
        _cold void ping(const std::string& id, const int rtt);          // a round trip measured by its page
        _cold void renewPin(void);
        _cold void reopen(void);                    // opened again at its next tick (its port, the router asked again)
        _cold _nodiscard cluster::WebStatus status(void) const;

        // ---------- Constructor -------- //
        _cold Web(cluster::Manager& manager, cluster::Voice* voice, const std::optional<bool> force);

        // ----------- Destructor --------- //
        _cold ~Web();
};

} // namespace end

#endif /* WEB_H */
