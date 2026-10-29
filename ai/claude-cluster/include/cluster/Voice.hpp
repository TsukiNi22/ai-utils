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
    bool paused = false;            // the user speaks: its countdown stopped (full again after the new text)
};

//----------------------------------------------------------------//
/* FUNCTION */

// what is missing for the voice, with the command to fix it (tools, enrollment, voices, speech models, engines, API keys)
_cold _nodiscard std::vector<std::string> voice_missing(const cluster::Config& config);

//----------------------------------------------------------------//
/* CLASS */

class Voice {
    private:
        cluster::Manager& _manager;
        mutable std::mutex _mutex;
        std::thread _listener;
        std::atomic<bool> _listening{false};       // voice-listen runs (its model is loaded once: ~10 s)
        std::atomic<bool> _ready{false};           // its model is loaded ("voice-listen: ready")
        std::int64_t _loadStart = cluster::now();   // voice-listen started: its model loading since
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
        std::string _model;                         // the speech model voice-listen was started on ([voice] listen_model)
        double _endSilence = 0.0;                   // and its end of a sentence ([voice] end_silence)
        std::string _language;                      // and the language spoken ([voice] listen_language)
        std::string _askSpoken;                     // the question of an agent read aloud (request:index)
        std::atomic<std::int64_t> _lastSound{0};    // unix ms of the last "…" / live text: the user speaks (nothing sent)

        // ---------- Pre-Function -------- //
        _cold void listen_(void);
        _cold void heard_(const std::string& text);
        _cold void dispatch_(const cluster::Pending& pending);
        _cold void say_(const std::string& text);
        _cold _nodiscard std::string clickAction_(const std::string& text);    // an action shown, said by its label: clicked (what it did)
        _cold _nodiscard bool command_(const std::string& text);    // cancel / send words said alone: done (true)
        _cold _nodiscard std::vector<cluster::Term> terms_(void);   // the vocabulary now (file, projects, sessions, user)
        _cold _nodiscard cluster::Json remote_(void) const;         // an interface of the core: the state of its voice (last sync)
        _cold bool forward_(const std::string& action, const std::string& text = "");    // an interface: run by the voice of its core (true)

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
        // the web access: a sentence recorded by the browser - the voice word it is (cancel / send / empty), its proper
        // names written back; voice-say and its arguments for a text (engine, voice of its language, Piper fallback)
        _cold _nodiscard std::string voiceWord(const std::string& text) const;
        _cold _nodiscard std::vector<std::string> sayArgs(const std::string& text) const;
        _nodiscard bool armed(void) const;                 // wake mode: the wake phrase was just heard
        _nodiscard std::vector<cluster::Pending> pending(void) const;
        _cold _nodiscard cluster::Json json(void) const;   // its state for the interfaces of the core (their sync)
        // push mode: F5 pressed; other modes: voice-listen runs (an interface: the voice of its core)
        _nodiscard bool listening(void) const;

        // ---------- Function -------- //
        // an interface of the core: the state of the voice of its core
        _nodiscard inline bool ready(void) const            {return this->_manager.replica() ? this->remote_().value("ready", false) : this->_ready.load();};
        _nodiscard inline bool running(void) const          {return this->_manager.replica() ? this->remote_().value("running", false) : this->_listening.load();};
        _nodiscard inline bool speaking(void) const         {return this->_manager.replica() ? this->remote_().value("speaking", false) : this->_sayPid > 0;};
        _nodiscard inline std::uint64_t version(void) const {return this->_manager.replica() ? this->remote_().value("version", std::uint64_t{0}) : this->_version.load();};
        _cold _nodiscard inline std::string correct(const std::string& text) {return cluster::vocabulary_correct(text, this->terms_());}; // the proper names written back
        _cold _nodiscard inline std::vector<std::string> setup(void) const   {return cluster::voice_missing(this->_manager.config());}; // what is missing

        // ---------- Constructor -------- //
        _cold Voice(cluster::Manager& manager);

        // ----------- Destructor --------- //
        _cold ~Voice();
};

} // namespace end

#endif /* CLUSTER_VOICE_H */
