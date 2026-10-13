/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Config.hpp

File Description:
##  config.toml of claude-cluster (defaults, styles, profiles,
##  providers, keys), reloaded live
\**************************************************************/

#ifndef CLUSTER_CONFIG_H
    #define CLUSTER_CONFIG_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include <filesystem>           // std::filesystem::path, std::filesystem::file_time_type
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <map>                  // std::map

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Style {
    std::string name;
    bool dark = true;
    std::string bg;
    std::string surface;
    std::string text;
    std::string muted;
    std::string accent;
    std::string border;
    std::string ok;
    std::string warn;
    std::string error;
};

struct Profile {
    std::string name;
    std::string cwd;
    std::string backend;
    std::string mode;
    std::string prompt;
    std::vector<std::string> extraArgs;
};

struct ProviderConf {
    std::string name;
    std::string driver;         // claude | qwen | opencode | codex
    std::string baseUrl;
    std::string model;          // default model
    bool key = false;           // an API key is needed (stored by `auth login`)
};

struct VoiceConf {
    bool enabled = false;
    std::string mode = "push";  // push | auto | wake
    std::string wakeWord = "ok claude"; // wake mode: phrase(s) that trigger the listening, comma separated variants
    int wakeSeconds = 8;        // wake mode: the phrase alone arms the listening this long
    std::string wakeReply;      // wake mode: spoken when armed (ex: "Oui ?"), empty: nothing
    bool onlyMe = true;
    int confirmSeconds = 3;
    bool speak = true;
    std::size_t summaryChars = 220;
    std::string listenCommand = "voice-listen";
    std::string sayCommand = "voice-say";
    std::string sayEngine = "piper";    // voice-say -e: piper | kokoro | elevenlabs | cartesia (failing: Piper)
    std::string sayVoices;              // the voice of that engine per language: "fr=ff_siwis, en=bf_emma, es=kokoro:ef_dora"
    std::string voiceFr = "tom-medium"; // Piper voices: the engine piper, the fallback of the others
    std::string voiceEn = "en_GB-northern_english_male-medium";
    // said alone while a transcription waits: dropped / sent now (cancel: also stops the answer being read); the
    // words of voice-words.txt (edited in the editor), these ones of config.toml added
    std::vector<std::string> cancelWords;
    std::vector<std::string> sendWords;
    std::string words;              // the file of these words (empty: voice-words.txt of the config folder)
    std::string listenModel;        // voice-listen --model: parakeet | whisper | kroko-fr | groq | deepgram (empty: voice-models)
    std::string listenLanguage;     // voice-listen --language (whisper, the APIs): fr, en... (empty: found by the model)
    double endSilence = 0.0;        // voice-listen --end-silence: seconds of silence ending a sentence (0: its own, 1.0)
    std::string vocabulary;         // proper names written back in what is heard (empty: vocabulary.txt of the config folder)
};

// What the user tells about themselves (asked at the first map of this computer, the settings page: User)
struct UserConf {
    std::string nickname;
    std::string email;
    std::string editor;             // the main one (Ctrl+E too when [ui] editor is empty)
    std::string language;           // of the answers: fr | en | empty (the one of the message)
    std::string notes;              // anything the global session should know
};

// The web access: a browser of the same LAN drives claude-cluster after a 6-digit code (one at a time)
struct WebConf {
    bool enabled = false;           // opened at the start (else: web on, the palette, --web)
    int port = 8787;
    bool https = true;              // TLS, self-signed certificate (the microphone of the page needs it; the browser asks once)
};

// The background service (claude-cluster service ...): the wake phrase opens claude-cluster with its voice on
struct ServiceConf {
    std::string frontend = "auto";  // auto: a window, else a terminal | gui: a window (a terminal on its failure) | tty: a terminal
    std::string terminal;           // tty: the terminal and its option to run a command (empty: the first installed)
    std::string voiceMode = "auto"; // the voice of the claude-cluster opened: auto (every sentence) | wake | push
    std::string wakeWord;           // empty: [voice] wake_word
    std::string python = "~/.local/share/voice/venv/bin/python"; // the python of voice-listen (sherpa-onnx)
    std::string source;             // microphone (PulseAudio / PipeWire source), empty: the default one
};

//----------------------------------------------------------------//
/* CLASS */

class Config {
    private:
        std::filesystem::path _path;
        std::filesystem::file_time_type _mtime{};
        std::string _error;

        // ---------- Pre-Function -------- //
        _cold void defaults_(void);

    public:
        /* ui */
        std::string frontend = "auto";
        std::string layout = "list";
        std::string style = "html-dark";
        std::vector<std::string> panels = {"skills", "tokens", "context", "cost", "usage", "git"};
        std::string restore = "ask";
        bool notify = true;                     // desktop notifications at all
        bool notifyGlobal = true;               // the global session answered
        bool notifyDone = true;                 // a sub-session finished its turn
        bool notifyPermission = true;           // a permission waits
        bool vim = false;                       // vim mode of the prompt (Insert / Normal / Visual)
        int pasteLines = 3;                     // a paste of more lines becomes [Pasted text #N +L lines] (0: never)
        int pasteChars = 800;                   // or of more characters
        std::string editor;                     // Ctrl+E (empty: nvim, vim, vi, $VISUAL, $EDITOR, emacs -nw, nano)
        std::string imageViewer;                // right click on an image chip (empty: xdg-open)
        std::string imagePreview = "blocks";    // terminal preview: blocks (pixels as half blocks) | chafa (symbols)
        bool typewriter = true;                 // the answers typed as they come (also when they arrive at once)
        int typewriterSpeed = 400;              // characters per second (faster when a long text is behind)

        /* agent */
        std::string backend = "claude";
        std::string mode = "auto";      // permission mode of the new sessions (empty: the default of the agent)
        std::string effort;             // thinking / reasoning effort of the new sessions (empty: the default of the agent)
        std::map<std::string, std::vector<std::string>> models; // [models] <provider> = [...]: offered in the palette, /model
        bool allowBypass = true;
        std::vector<std::string> extraArgs;
        std::map<std::string, std::string> commands = {{"claude", "claude"}, {"qwen", "qwen"}, {"opencode", "opencode"}, {"codex", "codex"}};

        /* global */
        bool globalEnabled = true;
        std::string globalBackend = "claude";
        std::string globalCwd;                  // empty: a folder of its own (<data>/global, trusted for Remote Control)
        std::string globalModel;
        std::string globalMode = "bypassPermissions"; // never asked (empty: the mode of [agent])
        std::string globalEffort;               // empty: the one of [agent]
        std::string globalPrompt = "~/.config/claude-cluster/global.md";
        std::string remoteName = "claude-cluster";

        /* sessions */
        int trashDays = 14;
        int maxParallel = 4;
        double budget = 0.0;
        bool bypassToGlobal = true;             // the permissions a sub-session in bypassPermissions still asks: decided by the global session
        bool globalFork = false;                // the global session may fork a sub-session (MCP session_fork)
        bool globalEdit = false;                // the global session may rewrite the conversation of a sub-session (MCP session_edit)
        std::string sessionsPrompt = "~/.config/claude-cluster/sessions.md"; // system prompt of the sub-sessions (empty: none)

        cluster::VoiceConf voice;
        cluster::ServiceConf service;
        cluster::WebConf web;
        cluster::UserConf user;
        int machineDays = 30;                   // the map of this computer older: a sync proposed at start (0: never)
        std::string voiceForce;                 // --voice <mode>: the voice on in this mode for this run (over the file)
        std::map<std::string, std::string> keys;
        std::map<std::string, cluster::Style> styles;
        std::map<std::string, cluster::Profile> profiles;
        std::map<std::string, cluster::ProviderConf> providers;

        // ---------- Pre-Function -------- //
        _cold void load(void);                              // creates the default files on the first run
        _cold _nodiscard bool reload(void);                 // true when the file changed and was loaded again
        _cold _nodiscard const cluster::Style& currentStyle(void) const;
        _cold _nodiscard std::string command(const std::string& driver) const;
        _cold _nodiscard static std::string check(const std::string& text);   // a config.toml text: its parse error ("" valid)

        // ---------- Function -------- //
        _nodiscard inline const std::filesystem::path& path(void) const {return this->_path;};
        _nodiscard inline const std::string& error(void) const {return this->_error;};   // last parse error (the previous values are kept)

        // ---------- Constructor -------- //
        _cold Config(void);

        // ----------- Destructor --------- //
        _cold ~Config() = default;
};

} // namespace end

#endif /* CLUSTER_CONFIG_H */
