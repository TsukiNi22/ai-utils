/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Core.hpp

File Description:
##  Core of claude-cluster: arguments, headless commands (list,
##  spawn, send...), auth, MCP server, and the start of the
##  terminal / window front-end
\**************************************************************/

#ifndef CLUSTER_CORE_H
    #define CLUSTER_CORE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #define _Arguments
    #include <utils/utils.hpp>      // _cold, _nodiscard, utils::arguments::ArgParser
    #include "Control.hpp"          // cluster::Control
    #include "Manager.hpp"          // cluster::Manager
    #include "Config.hpp"           // cluster::Config
    #include "Voice.hpp"            // cluster::Voice
    #include <memory>               // std::unique_ptr
    #include <atomic>               // std::atomic
    #include <thread>               // std::thread
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <map>                  // std::map

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Options {
    std::string frontend;           // tty | gui | empty (config)
    std::string backend;            // default backend of the new sessions (this run)
    std::string restore;            // ask | always | never | empty (config)
    std::string layout;
    std::string style;
    bool global = true;
    bool rtk = false;
    bool json = false;
    std::map<std::string, std::string> values; // --name, --mode, --profile, --prompt, --target, --after, --path
};

// Everything a front-end needs, with the 1 s tick (config reload, tasks, voice)
struct App {
    cluster::Config config;
    std::unique_ptr<cluster::Manager> manager;
    std::unique_ptr<cluster::Control> control;
    std::unique_ptr<cluster::Voice> voice;
    cluster::Options options;
    bool askRestore = false;        // the front-end asks restore / start clean
    std::atomic<bool> running{true};
    std::thread ticker;

    _cold void start(void);
    _cold void stop(void);
};

//----------------------------------------------------------------//
/* CLASS */

class Core {
    private:
        cluster::Options _options;
        std::vector<std::string> _words;            // command and its positional arguments
        utils::arguments::ArgParser _parser{"claude-cluster", "Run and drive several Claude Code (or other agent) sessions in parallel, "
            "with a global session managing them, in a terminal or a window."};
        std::string _completion;
        bool _version = false;
        int _exit = 0;

        // ---------- Pre-Function -------- //
        _cold void setup_(void);
        _cold _nodiscard std::vector<std::string> extract_(const int argc, char* argv[]);
        _cold void help_(void) const;
        _cold void completion_(void) const;         // Core-Completion.cpp
        _cold int ui_(const bool serve = false);    // serve: no front-end, until SIGINT / SIGTERM
        _cold int headless_(void);
        _cold int auth_(void);
        _cold void print_(const std::string& cmd, const cluster::Json& data) const;

    public:
        // ---------- Pre-Function -------- //
        _cold void init(const int argc, char* argv[]);
        _cold void run(void);

        // ---------- Function -------- //
        _nodiscard inline int exit(void) const {return this->_exit;};

        // ---------- Constructor -------- //
        _cold Core(void) = default;
        _cold ~Core() = default;
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold int run_tty(cluster::App& app);
#ifdef CLUSTER_GUI
_cold int run_gui(cluster::App& app);
#endif

} // namespace end

#endif /* CLUSTER_CORE_H */
