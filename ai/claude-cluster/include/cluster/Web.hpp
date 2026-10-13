/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file Web.hpp

File Description:
##  Web access of claude-cluster on the local network: a page in a
##  browser of the same LAN drives it, after a 6-digit code; one
##  remote browser at a time, the others refused while it is there
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
    #include <memory>               // std::unique_ptr
    #include <string>               // std::string
    #include <atomic>               // std::atomic
    #include <thread>               // std::thread
    #include <mutex>                // std::mutex

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct WebStatus {
    bool on = false;                // the port is open
    std::string url;                // http://<the LAN address>:<port>
    std::string pin;                // the code asked by the page: "123 456"
    std::string client;             // the address of the browser connected, empty: free
    std::string error;              // why the port could not be opened
    bool https = false;             // TLS (self-signed certificate): the microphone of the page works
    std::string fingerprint;        // SHA-256 of the certificate, to compare with the one the browser shows
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
        std::string _token;             // the one browser connected: its cookie
        std::string _client;            // and its address
        std::int64_t _seen = 0;         // its last request (ms): gone after 30 s without one
        std::string _error;
        void* _tls = nullptr;           // SSL_CTX of https (nullptr: plain http)
        bool _httpsWanted = false;      // [web] https when it was opened
        std::string _fingerprint;

        // ---------- Pre-Function -------- //
        _cold void open_(const int port);
        _cold void close_(void);
        _cold void serve_(void);
        _cold void client_(const int fd, const std::string& peer);
        _cold void request_(void* connection, const std::string& peer, const bool toHttps);
        _cold void login_(void* connection, const std::string& requestBody, const std::string& cookie, const std::string& peer, const std::string& secure);
        _cold _nodiscard std::string tls_(void);   // https set up (its certificate made once), error text, empty: on
        _cold void newPin_(void);
        _cold void expire_(void);       // the browser gone: the place free again (under _mutex)

    public:
        // ---------- Pre-Function -------- //
        _cold void tick(void);              // the port follows [web] enabled / web on|off, [web] port
        _cold void setOn(const bool on);    // for this run
        _cold void kick(void);              // the browser connected out (a new code)
        _cold void renewPin(void);
        _cold _nodiscard cluster::WebStatus status(void) const;

        // ---------- Constructor -------- //
        _cold Web(cluster::Manager& manager, cluster::Voice* voice, const std::optional<bool> force);

        // ----------- Destructor --------- //
        _cold ~Web();
};

} // namespace end

#endif /* WEB_H */
