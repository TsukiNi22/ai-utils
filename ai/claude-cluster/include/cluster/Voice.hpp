/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Voice.hpp

File Description:
##  Voice of the global session: voice-listen (push-to-talk or
##  automatic, wake word, only the user's voice), confirmation of
##  the transcription, routing to a session, spoken summaries
\**************************************************************/

#ifndef CLUSTER_VOICE_H
    #define CLUSTER_VOICE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Manager.hpp"          // cluster::Manager
    #include <atomic>               // std::atomic
    #include <thread>               // std::thread
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <mutex>                // std::mutex

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Pending {
    std::string text;
    std::string target;             // session id
    std::int64_t deadline = 0;      // unix ms: sent at this time
};

//----------------------------------------------------------------//
/* CLASS */

class Voice {
    private:
        cluster::Manager& _manager;
        mutable std::mutex _mutex;
        std::thread _listener;
        std::atomic<bool> _listening{false};
        std::atomic<pid_t> _listenPid{-1};
        std::atomic<pid_t> _sayPid{-1};
        std::vector<cluster::Pending> _pending;
        std::string _partial;
        std::atomic<std::uint64_t> _version{0};
        std::atomic<std::int64_t> _armedUntil{0};  // wake mode: unix ms until which the next sentence is the command

        // ---------- Pre-Function -------- //
        _cold void listen_(void);
        _cold void heard_(const std::string& text);
        _cold void dispatch_(const cluster::Pending& pending);
        _cold void say_(const std::string& text, const std::string& voice = "");

    public:
        // ---------- Pre-Function -------- //
        _cold void start(void);             // starts listening (auto / wake modes: always, push mode: key pressed)
        _cold void stop(void);
        _cold void toggle(void);            // push-to-talk key (press: listen, press again: stop)
        _cold void tick(void);              // sends the confirmed transcriptions
        _cold void confirm(void);           // send the pending transcription now
        _cold void cancel(void);            // drop it
        _cold void edit(const std::string& text);
        _cold void speak(const std::string& text);
        _cold void mute(void);
        _cold _nodiscard std::vector<std::string> setup(void) const;   // what is missing, with the command to fix it

        // ---------- Function -------- //
        _nodiscard inline bool listening(void) const {return this->_listening;};
        _nodiscard inline bool speaking(void) const {return this->_sayPid > 0;};
        _nodiscard bool armed(void) const;                 // wake mode: the wake phrase was just heard
        _nodiscard std::vector<cluster::Pending> pending(void) const;
        _nodiscard inline std::uint64_t version(void) const {return this->_version;};

        // ---------- Constructor -------- //
        _cold Voice(cluster::Manager& manager);
        _cold ~Voice();
};

} // namespace end

#endif /* CLUSTER_VOICE_H */
