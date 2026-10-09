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
    std::string voiceFr = "tom-medium";
    std::string voiceEn = "en_GB-northern_english_male-medium";
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
        std::vector<std::string> panels = {"skills", "tokens", "context", "cost", "git"};
        std::string restore = "ask";
        bool notify = true;

        /* agent */
        std::string backend = "claude";
        std::string mode = "default";
        std::vector<std::string> extraArgs;
        std::map<std::string, std::string> commands = {{"claude", "claude"}, {"qwen", "qwen"}, {"opencode", "opencode"}, {"codex", "codex"}};

        /* global */
        bool globalEnabled = true;
        std::string globalBackend = "claude";
        std::string globalCwd = "~";
        std::string globalModel;
        std::string globalPrompt = "~/.config/claude-cluster/global.md";
        std::string remoteName = "claude-cluster";

        /* sessions */
        int trashDays = 14;
        int maxParallel = 4;
        double budget = 0.0;

        cluster::VoiceConf voice;
        std::map<std::string, std::string> keys;
        std::map<std::string, cluster::Style> styles;
        std::map<std::string, cluster::Profile> profiles;
        std::map<std::string, cluster::ProviderConf> providers;

        // ---------- Pre-Function -------- //
        _cold void load(void);                              // creates the default files on the first run
        _cold _nodiscard bool reload(void);                 // true when the file changed and was loaded again
        _cold _nodiscard const cluster::Style& currentStyle(void) const;
        _cold _nodiscard std::string command(const std::string& driver) const;

        // ---------- Function -------- //
        _nodiscard inline const std::filesystem::path& path(void) const {return this->_path;};
        _nodiscard inline const std::string& error(void) const {return this->_error;};   // last parse error (the previous values are kept)

        // ---------- Constructor -------- //
        _cold Config(void);
        _cold ~Config() = default;
};

} // namespace end

#endif /* CLUSTER_CONFIG_H */
