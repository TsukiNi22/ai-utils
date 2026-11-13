/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file VoiceEngines.hpp

File Description:
##  The engines of the voice: speech to text (voice-listen) and
##  text to speech (voice-say), local or free-tier APIs, what
##  each handles, its limits, its link (the settings page shows it)
\**************************************************************/

#ifndef VOICEENGINES_H
    #define VOICEENGINES_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Config.hpp"           // cluster::Config
    #include "Types.hpp"            // cluster::Json
    #include "Tools.hpp"            // cluster::expand_home
    #include <filesystem>           // std::filesystem::exists
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct VoiceEngine {
    std::string id{};             // [voice] listen_model / say_engine value
    std::string kind{};           // listen (speech to text) | say (text to speech)
    std::string name{};
    bool local = true;            // runs on this computer (else an API: the text / audio sent to it)
    std::string provider{};       // its API key: claude-cluster auth login <provider> (empty: none)
    std::string languages{};
    std::string strengths{};      // better than the others
    std::string limits{};         // worse than the others
    std::string free{};           // its free use
    std::string voices{};         // say: its voices, how they are named and listed
    std::string link{};
    std::string keyLink{};        // where its key is made
    std::string folder{};         // local: its model under ~/.local/share/voice (installed: present)
    std::string install{};        // local: how it is installed
};

//----------------------------------------------------------------//
/* FUNCTION */

// every engine, speech to text first
_cold _nodiscard const std::vector<cluster::VoiceEngine>& voice_engines(void);

// the engine of a kind (listen | say), nullptr: unknown
_cold _nodiscard const cluster::VoiceEngine* voice_engine(const std::string& kind, const std::string& id);

// a local engine: its model present
_cold _nodiscard inline bool voice_engine_installed(const cluster::VoiceEngine& engine) {return engine.folder.empty() || std::filesystem::exists(cluster::expand_home("~/.local/share/voice") / engine.folder);};

// what it is, handles, does better / worse, its free use, voices, links: one line each
_cold _nodiscard std::vector<std::string> voice_engine_lines(const cluster::VoiceEngine& engine);

// the help of a choice of [voice] listen_model / say_engine (the settings page)
_cold _nodiscard std::string voice_engine_help(const std::string& kind, const std::string& id);

// every engine with its state: ready (installed / its key stored), used (the one of the config)
_cold _nodiscard cluster::Json voice_engines_json(const cluster::Config& config);

} // namespace end

#endif /* VOICEENGINES_H */
