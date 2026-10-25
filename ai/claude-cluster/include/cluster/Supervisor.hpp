/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 25/10/2026 by @author Tsukini

File Name:
##  @file Supervisor.hpp

File Description:
##  The supervisor: every claude-cluster of this computer (the main one and the independent instances) at a glance - running or not,
##  its version, its sessions (working, idle, waiting for you, in the background), its update - and light actions on them (open, update
##  smooth or forced, one or all, stop, start); the terminal view (normal / details: F2), its web access for the page and the app
\**************************************************************/

#ifndef SUPERVISOR_H
    #define SUPERVISOR_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* utils */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard

    /* type */
    #include "Wire.hpp"                     // cluster::Json
    #include <filesystem>                   // std::filesystem::path
    #include <cstdint>                      // std::int64_t
    #include <string>                       // std::string
    #include <vector>                       // std::vector

    //----------------------------------------------------------------//
    /* DEFINE */

    #define SUPERVISOR_INSTANCE ".supervisor" // the instance of the web access of the supervisor (never listed with the others)

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct SupInstance {                // a claude-cluster of this computer, as the supervisor sees it
    std::string name;               // "main" or the name of the independent instance
    bool main = false;              // the main one (the global session of the user, its web access)
    std::filesystem::path data;     // its data folder
    std::filesystem::path socket;   // its control socket
    bool alive = false;             // its core answers
    std::string version;            // the version of its core ("" stopped)
    std::string build;
    int pid = 0;
    cluster::Json sessions = cluster::Json::array();   // {id, name, state, background, queued, asking, agents, model, keep_warm}
    int working = 0;                // its sessions now: working, idle, waiting for the user (a question / a permission), in the background
    int idle = 0;
    int waiting = 0;
    int background = 0;
    int errors = 0;
    int queued = 0;                 // prompts waiting in their queues
    bool web = false;               // its web access open
    std::string url;
    bool upgrading = false;         // an update of it running
    std::string offered;            // a newer version offered to it ("" none)
    std::size_t kept = 0;           // stopped: the sessions it would restore
    std::int64_t last = 0;          // its last use (unix seconds)
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard std::vector<cluster::SupInstance> supervisor_scan(void);     // every claude-cluster of this computer, read now (a few ms each)
_cold _nodiscard cluster::Json supervisor_json(const std::vector<cluster::SupInstance>& all);   // the same for the page / the app
_cold _nodiscard std::string supervisor_installed(void);                      // the version installed on disk (this binary)
// an action on one of them (its name; "*": every running one): open | update | force | stop | start; what was done
_cold std::string supervisor_act(const std::string& name, const std::string& action);
_cold void supervisor_env(const std::string& name);                           // this process as an interface of that one (open)
_cold _nodiscard bool supervisor_web(void);                                   // the web access of the supervisor runs
_cold std::string supervisor_web_toggle(void);                                // started / stopped (its own core, port [web] supervisor_port)
_cold int supervisor_tty(std::string& open);                                  // the terminal view; open: the instance chosen to open ("" none)

} // namespace end

#endif /* SUPERVISOR_H */
