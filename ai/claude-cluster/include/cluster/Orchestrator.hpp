/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Orchestrator.hpp

File Description:
##  The resource orchestrator: the heavy jobs of the sessions
##  (builds, tests, containers, GPU) wait in a queue, a limited
##  number runs at once, the limit follows the hardware, the
##  live load and the settings (design: docs/orchestrator.md)
\**************************************************************/

#ifndef CLUSTER_ORCHESTRATOR_H
    #define CLUSTER_ORCHESTRATOR_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Hardware.hpp"         // cluster::Hardware, cluster::Host
    #include "Settings.hpp"         // cluster::SettingsPage
    #include "Config.hpp"           // cluster::Config, cluster::OrchConf
    #include "Types.hpp"            // cluster::Json
    #include <condition_variable>   // std::condition_variable
    #include <functional>           // std::function
    #include <filesystem>           // std::filesystem::path
    #include <optional>             // std::optional
    #include <cstdint>              // std::int64_t
    #include <atomic>               // std::atomic
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <thread>               // std::thread
    #include <mutex>                // std::mutex
    #include <set>                  // std::set
    #include <map>                  // std::map

    //----------------------------------------------------------------//
    /* DEFINE */

    #define GLOBAL_ID "g" // id of the global session

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Job {
    std::string id;                 // j1, j2...
    std::string name;
    std::string cls;                // build | tests | container | gpu | custom
    std::string session;            // its owner: g, s3... (empty: the user)
    std::string origin;             // wrap: a cluster-run of a session | run: started by the core (heavy_run, jobs_add)
    std::string command;            // as shown
    std::vector<std::string> argv;  // wrap: the command given
    std::string cwd;
    int prio = 0;                   // -10..10, higher first
    std::string state = "queued";   // queued | running | stopped | done | failed | killed
    std::string reason;             // why it ended / waits
    std::string by;                 // who killed it: user | global | session | pressure | archived | timeout | restart | wrapper
    std::string stoppedBy;          // user | pressure (state stopped)
    std::int64_t created = 0;       // unix ms
    std::int64_t started = 0;
    std::int64_t ended = 0;
    std::int64_t stoppedAt = 0;
    std::int64_t stoppedMs = 0;     // the time spent stopped
    int pid = 0;                    // its process group
    std::uint64_t pidStart = 0;     // start time of that process (/proc stat): the same one later
    int code = -1;
    int threads = 0;                // granted
    int ramMb = 0;                  // estimated
    int rssMb = 0;                  // seen
    int timeoutSeconds = 0;         // killed past it (0: none)
    bool callback = false;          // the owner is woken up when it ends
    std::string tail;               // the end of its output
    std::string log;                // run: the file of its whole output
    // not saved
    bool granted = false;           // started (wrap: the go may be sent)
    bool child = false;             // run: a child of the core still to wait for
    bool stopWanted = false;        // stopped before its pid was known
    bool reniced = false;
    int outFd = -1;
    std::int64_t termAt = 0;        // a TERM sent: KILL after 5 s
    std::int64_t lastQueued = 0;    // wrap: the last "queued" line sent
    std::string lastWaiting;
    int lastPosition = 0;
};

struct OrchHooks {                  // what the manager gives the orchestrator
    std::function<std::string(void)> active;                                    // the session the user shows
    std::function<std::string(const std::string&)> nameOf;                      // a session's name
    std::function<void(const std::string&, const std::string&)> tell;           // a session woken up (a prompt), else a notice
    std::function<void(const std::string&, const bool)> notice;                 // a notice of the interfaces
    std::function<void(const std::string&, const std::string&, const std::string&, const bool)> notify;    // kind, title, text, urgent: the desktop / the app
    std::function<std::vector<std::pair<std::string, int>>(void)> sessions;     // id and agent process of every session
    std::function<void(void)> changed;                                          // the interfaces redraw
};

// who asks: the user (CLI, interface), the global session, a sub-session (its own jobs, the settings)
struct OrchCaller {
    std::string kind = "user";      // user | global | sub
    std::string session;            // sub: its id
};

//----------------------------------------------------------------//
/* CLASS */

class Orchestrator {
    private:
        cluster::Config& _config;
        cluster::OrchHooks _hooks;
        mutable std::mutex _mutex;
        std::condition_variable _cv;
        std::vector<cluster::Job> _jobs;
        int _next = 1;
        cluster::Hardware _hardware;
        cluster::Host _host;
        int _level = 0;                 // pressure: 0 ok, 1 hold (nothing new starts), 2 reduce (slow the running ones)
        std::string _levelWhy;          // what made it
        std::int64_t _calmSince = 0;
        std::int64_t _lastReduce = 0;
        std::int64_t _lastResume = 0;
        std::int64_t _lastScan = 0;     // the table of the processes (rss, detection)
        std::int64_t _lastSave = 0;
        bool _dirty = false;
        std::set<int> _seen;            // processes already told as heavy
        std::vector<cluster::Json> _detected;
        std::atomic<bool> _running{false};
        std::thread _thread;
        std::atomic<std::uint64_t> _version{1};
        std::mutex _outMutex;           // the effects on the manager (tell, notify...) run once the lock is released
        std::vector<std::function<void(void)>> _outbox;
        std::string _active;            // the session shown (read each second, outside the lock)
        std::vector<std::pair<std::string, int>> _agents;   // the agent process of every session (the same)

        // ---------- Pre-Function -------- //
        _cold void loop_(void);
        _cold void second_(void);                   // the sample, the pressure, the schedule, the saves
        _cold void reap_(void);                     // the jobs started by the core: exit, output, the TERM then KILL
        _cold void schedule_(void);                 // the next jobs started (the lock held)
        _cold void pressure_(const cluster::OrchConf& c);   // the level and what it asks of the running jobs (the lock held)
        _cold void processes_(const cluster::OrchConf& c);  // the table of /proc: rss of the jobs, heavy commands outside the queue
        _cold _nodiscard int level_(const cluster::OrchConf& c, const double scale) const;
        _cold _nodiscard int slots_(const cluster::OrchConf& c) const;      // the heavy jobs that may run now (auto, or max_jobs)
        _cold _nodiscard int classLimit_(const cluster::OrchConf& c, const std::string& cls) const;
        _cold _nodiscard int ram_(const cluster::OrchConf& c, const cluster::Job& job) const;
        _cold _nodiscard int grantThreads_(const cluster::OrchConf& c, const std::string& cls, const int active) const;
        _cold _nodiscard double score_(const cluster::OrchConf& c, const cluster::Job& job, const std::int64_t now) const;
        _cold _nodiscard std::string blocked_(const cluster::OrchConf& c, const cluster::Job& job, const int active) const;   // why it waits (empty: it may start)
        _cold bool spawn_(cluster::Job& job, const cluster::OrchConf& c);   // a job of the core started (false: not started, its state says why)
        _cold void signal_(const cluster::Job& job, const int signal) const;
        _cold void finish_(cluster::Job& job, const std::string& state, const int code, const std::string& by, const std::string& reason);   // its end: the owner told
        _cold void kill_(cluster::Job& job, const std::string& by, const std::string& reason);
        _cold _nodiscard cluster::Job* find_(const std::string& id);
        _cold _nodiscard bool mine_(const cluster::Job& job, const cluster::OrchCaller& caller) const;
        _cold _nodiscard cluster::Json jobJson_(const cluster::Job& job, const cluster::OrchConf& c, const bool full) const;
        _cold _nodiscard cluster::Json waitFor_(const std::string& id, const int seconds, const cluster::OrchCaller& caller);
        _cold _nodiscard cluster::Json add_(const cluster::Json& request, const cluster::OrchCaller& caller);
        _cold _nodiscard cluster::Json list_(const cluster::Json& request, const cluster::OrchCaller& caller) const;
        _cold _nodiscard cluster::Json config_(const cluster::Json& request, const cluster::OrchCaller& caller);
        _cold _nodiscard std::map<std::string, int> positions_(const cluster::OrchConf& c) const;   // the place of every waiting job (1 = next)
        _cold _nodiscard double etaOf_(const cluster::OrchConf& c, const cluster::Job& job, const int position) const;
        _cold void post_(std::function<void(void)> effect);    // an effect on the manager, run by flush_ outside the lock
        _cold void flush_(void);
        _cold void touch_(void);                    // a change: the waiters, the interfaces, the save (the lock held)
        _cold void trim_(const cluster::OrchConf& c);   // the finished ones kept to the history

    public:
        using Hooks = cluster::OrchHooks;
        using Caller = cluster::OrchCaller;

        // ---------- Pre-Function -------- //
        _cold void start(void);                     // the saved jobs, the scan, the thread
        _cold void stop(void);                      // the thread ends, the state saved
        _cold void save(void);
        _cold void load(void);
        _cold void rescan(void);                    // the hardware scanned again and saved
        // the commands of the control socket (job_*, heavy_run, hardware_scan, orch_*): the answer, throws when refused
        _cold _nodiscard cluster::Json handle(const std::string& cmd, const cluster::Json& request, const cluster::OrchCaller& caller);
        // a cluster-run wrapper on its connection: returns when its command ended or the connection dropped
        _cold void serve(const int fd, const cluster::Json& request);
        _cold void sessionGone(const std::string& session, const bool purge);  // closed / archived: its jobs removed or killed; purge: forgotten
        _cold _nodiscard cluster::Json state(void) const;                       // what the interfaces show: the gauges, the limits, the jobs

        // ---------- Function -------- //
        _nodiscard inline std::uint64_t version(void) const {return this->_version.load();};

        // ---------- Constructor -------- //
        _cold Orchestrator(cluster::Config& config, cluster::OrchHooks hooks);

        // ----------- Destructor --------- //
        _cold ~Orchestrator();
};

//----------------------------------------------------------------//
/* FUNCTION */

// "build" | "tests" | "container" | "gpu" | "custom" for a heavy command (the built-in names, then the patterns of the settings), "" a light one
_cold _nodiscard std::string job_class(const std::string& command, const std::vector<std::string>& argv, const std::vector<std::string>& patterns);
_cold _nodiscard std::vector<std::string> orchestrator_sub_tools(const cluster::OrchConf& conf);   // the tools of a sub-session the settings open
_cold _nodiscard cluster::SettingsPage orchestrator_settings_page(void);      // the Orchestrator page of the settings
_cold _nodiscard std::vector<cluster::Json> orchestrator_tools(void);          // the MCP tools (Mcp.cpp lists them)
_cold _nodiscard std::string orchestrator_command(const std::string& tool);    // the control command of an MCP tool ("" : not one of them)
_cold std::filesystem::path orchestrator_bin_dir(void);                        // <runtime>/bin: cluster-run for the PATH of the sessions (made when missing)

} // namespace end

#endif /* CLUSTER_ORCHESTRATOR_H */
