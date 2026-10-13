/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file WebVoice.hpp

File Description:
##  The voice of the web access: the sentences recorded by the
##  browser recognized by a voice-listen kept running (its model
##  loaded once), the answers made into speech by voice-say
\**************************************************************/

#ifndef WEBVOICE_H
    #define WEBVOICE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #define _Encapsulation
    #include <utils/utils.hpp>      // _cold, _nodiscard, utils::encapsulation::Process, utils::encapsulation::Pipe
    #include "Manager.hpp"          // cluster::Manager
    #include "Voice.hpp"            // cluster::Voice
    #include <filesystem>           // std::filesystem::path
    #include <cstdint>              // std::int64_t
    #include <memory>               // std::unique_ptr
    #include <string>               // std::string
    #include <atomic>               // std::atomic
    #include <mutex>                // std::mutex

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class WebVoice {
    private:
        cluster::Manager& _manager;
        cluster::Voice& _voice;
        std::mutex _mutex;
        std::unique_ptr<utils::encapsulation::Process> _process;   // voice-listen --files, running
        std::unique_ptr<utils::encapsulation::Pipe> _in;           // its stdin: the paths of the recordings
        std::unique_ptr<utils::encapsulation::Pipe> _out;          // its stdout: their texts
        std::string _buffer;
        std::string _model;             // the speech model / language it was started on
        std::string _language;
        std::int64_t _used = 0;         // its last use (ms): ended after 10 min unused
        std::atomic<int> _count{0};     // names of the temporary files
        std::filesystem::path _dir;     // the recordings, the speech made (0700, removed after use)

        // ---------- Pre-Function -------- //
        _cold void start_(void);
        _cold void stop_(void);
        _cold _nodiscard std::filesystem::path file_(const std::string& kind);

    public:
        // ---------- Pre-Function -------- //
        _cold _nodiscard std::string transcribe(const std::string& wav);   // its text (throws: no voice-listen, no answer)
        _cold _nodiscard std::string speech(const std::string& text);      // the wav of the text (throws: voice-say failed)
        _cold void tick(void);                                              // ended after 10 min unused

        // ---------- Constructor -------- //
        _cold WebVoice(cluster::Manager& manager, cluster::Voice& voice);

        // ----------- Destructor --------- //
        _cold ~WebVoice();
};

} // namespace end

#endif /* WEBVOICE_H */
