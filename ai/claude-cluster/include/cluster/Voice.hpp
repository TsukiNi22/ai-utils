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
    #include "Vocabulary.hpp"       // cluster::Term
    #include "Manager.hpp"          // cluster::Manager
    #include <filesystem>           // std::filesystem::file_time_type
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
        std::atomic<bool> _listening{false};       // voice-listen runs (its model is loaded once: ~10 s)
        std::atomic<bool> _ready{false};           // its model is loaded ("voice-listen: ready")
        std::atomic<bool> _open{false};            // push mode: F5 opened the listening
        std::atomic<bool> _paused{false};          // auto / wake modes: listening stopped by the key
        std::atomic<std::int64_t> _graceUntil{0};  // push mode: a sentence ending just after F5 is kept
        std::atomic<std::int64_t> _lastStart{0};
        std::atomic<pid_t> _listenPid{-1};
        std::atomic<pid_t> _sayPid{-1};
        std::vector<cluster::Pending> _pending;
        std::string _partial;
        std::atomic<std::uint64_t> _version{0};
        std::atomic<std::int64_t> _armedUntil{0};  // wake mode: unix ms until which the next sentence is the command
        std::vector<cluster::Term> _fileTerms;      // the vocabulary file and the projects of the map (read again when changed)
        std::filesystem::file_time_type _fileTime{};
        std::filesystem::file_time_type _machineTime{};

        // ---------- Pre-Function -------- //
        _cold void listen_(void);
        _cold void heard_(const std::string& text);
        _cold void dispatch_(const cluster::Pending& pending);
        _cold void say_(const std::string& text, const std::string& voice = "");
        _cold _nodiscard bool command_(const std::string& text);    // cancel / send words said alone: done (true)
        _cold _nodiscard std::vector<cluster::Term> terms_(void);   // the vocabulary now (file, projects, sessions, user)

    public:
        // ---------- Pre-Function -------- //
        _cold void start(void);             // starts listening (auto / wake modes: always, push mode: key pressed)
        _cold void stop(void);
        _cold void toggle(void);            // push-to-talk key (press: listen, press again: stop)
        _cold void tick(void);              // sends the confirmed transcriptions
        _cold void confirm(void);           // send the pending transcription now
        _cold void cancel(void);            // drop them all
        _cold void cancelLast(void);        // drop the last one
        _cold void edit(const std::string& text);
        _cold void speak(const std::string& text);
        _cold void mute(void);
        _cold _nodiscard std::vector<std::string> setup(void) const;   // what is missing, with the command to fix it
        _nodiscard bool armed(void) const;                 // wake mode: the wake phrase was just heard
        _nodiscard std::vector<cluster::Pending> pending(void) const;

        // ---------- Function -------- //
        // push mode: F5 pressed; other modes: voice-listen runs
        _nodiscard inline bool listening(void) const {return this->_manager.config().voice.mode == "push" ? this->_open.load() && this->_listening.load() : this->_listening.load();};
        _nodiscard inline bool ready(void) const {return this->_ready;};
        _nodiscard inline bool running(void) const {return this->_listening;};
        _nodiscard inline bool speaking(void) const {return this->_sayPid > 0;};
        _nodiscard inline std::uint64_t version(void) const {return this->_version;};

        // ---------- Constructor -------- //
        _cold Voice(cluster::Manager& manager);

        // ----------- Destructor --------- //
        _cold ~Voice();
};

} // namespace end

#endif /* CLUSTER_VOICE_H */
