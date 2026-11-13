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
    #include "Hardware.hpp"                 // cluster::Hardware
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

// A graphic card: its memory, its use and its temperature now (-1: unknown)
struct SupGpu {
    std::string name;
    std::string driver;
    long vramTotalMb = 0;
    long vramUsedMb = -1;
    double busy = -1;
    double tempC = -1;
};

// The performance of the computer (the P popup of the supervisor): what it has, what it does now, the memory of each claude-cluster core
struct SupPerf {
    std::int64_t at = 0;
    std::string cpu;
    int physical = 1;
    int threads = 1;
    int mhz = 0;
    double tempC = -1;
    double cpuPercent = 0;
    double load1 = 0;
    double load5 = 0;
    double load15 = 0;
    long memTotalMb = 0;
    long memAvailMb = 0;
    long swapTotalMb = 0;
    long swapUsedMb = 0;
    std::vector<cluster::SupGpu> gpus;
    long diskTotalGb = 0;
    long diskFreeGb = 0;
    double psiCpu = -1;
    double psiMem = -1;
    double psiIo = -1;
    std::string power;
    std::string uptime;
    std::vector<std::pair<std::string, long>> cores;    // <its name, its memory in MB>
    cluster::Hardware hardware;                         // read every 30 s
    std::int64_t scanned = 0;
    unsigned long long lastTotal = 0;                   // /proc/stat at the reading before (the CPU busy since)
    unsigned long long lastIdle = 0;
};

// An agent running on this computer outside claude-cluster (a terminal of its own): working | question | background | idle
struct SupAgent {
    int pid = 0;
    std::string agent;              // claude | codex | opencode | qwen | gemini | aider | ollama
    std::string cwd;
    std::string title;              // its conversation (the newest of its folder)
    std::string id;                 // its id (claude --resume <id>): opened in claude-cluster from it
    std::string state;
    std::string tty;                // pts/3
    double cpu = 0;                 // % of one core since the reading before
    std::int64_t updated = 0;       // its transcript written last (unix s)
    std::string needs;              // a job waiting for you: what it asks (and the reply it suggests)
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
_cold _nodiscard cluster::Json supervisor_web_state(void);                  // its web access as its core tells it (devices too)
_cold std::string supervisor_web_device(const std::string& what, const std::string& id, const std::string& answer); // approve | kick | untrust | ban | unban
_cold std::string supervisor_web_service(const std::string& action);     // beyond the LAN: reopen | tailscale_up | tailscale_operator
_cold bool supervisor_web_close_mine(void);                                 // the web access this supervisor opened, closed (its quit)
_cold void supervisor_perf(cluster::SupPerf& meter, const std::vector<cluster::SupInstance>& all);
_cold _nodiscard std::vector<cluster::SupAgent> supervisor_agents(void);    // the agents running outside claude-cluster
_cold std::string supervisor_agent_open(const cluster::SupAgent& agent);    // its conversation in the main claude-cluster (a copy)

} // namespace end

#endif /* SUPERVISOR_H */
