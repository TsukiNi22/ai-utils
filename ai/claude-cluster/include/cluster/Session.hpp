/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Session.hpp

File Description:
##  One agent session: its process (claude stream-json, or one
##  process per turn for qwen / opencode / codex), the parsing of
##  its events, its state, metrics, transcript and permissions
\**************************************************************/

#ifndef CLUSTER_SESSION_H
    #define CLUSTER_SESSION_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #define _Encapsulation
    #include <utils/utils.hpp>      // _cold, _nodiscard, utils::encapsulation::Process
    #include "Types.hpp"            // cluster::Snapshot, cluster::Json
    #include "Auth.hpp"             // cluster::Backend, cluster::Env
    #include <functional>           // std::function
    #include <filesystem>           // std::filesystem::path
    #include <fstream>              // std::ofstream
    #include <atomic>               // std::atomic
    #include <thread>               // std::thread
    #include <memory>               // std::unique_ptr
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <mutex>                // std::mutex
    #include <deque>                // std::deque

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Event {
    enum class Kind {Init, TurnDone, Permission, Exited};
    Kind kind = Kind::Init;
    std::string session;
    std::string text;
    bool error = false;
};

struct Launch {
    cluster::SessionSpec spec;
    cluster::Backend backend;
    cluster::Env env;
    std::string command;                    // program of the driver
    bool allowBypass = true;                // claude: --allow-dangerously-skip-permissions (bypassPermissions possible)
    std::vector<std::string> extraArgs;     // added by the manager (global session: MCP, system prompt)
    std::filesystem::path log;              // JSONL of the transcript
    std::function<void(const cluster::Event&)> listener;
};

//----------------------------------------------------------------//
/* CLASS */

class Session {
    private:
        mutable std::mutex _mutex;
        cluster::Snapshot _data;
        cluster::Launch _launch;
        std::unique_ptr<utils::encapsulation::Process> _process;
        int _in = -1;                       // write end of the stdin of the process
        int _outFd = -1;                    // read ends of its stdout / stderr
        int _errFd = -1;
        std::thread _worker;
        std::atomic<bool> _stopping{false};
        std::atomic<bool> _busy{false};     // per-turn drivers: a process is running
        std::atomic<std::uint64_t> _version{1};
        std::deque<std::string> _pending;   // per-turn drivers: prompts waiting for the running turn
        std::string _stderr;                // tail of the error output
        std::string _lastText;              // last assistant text of the turn
        cluster::Usage _turn;               // finished messages of the running turn
        cluster::Usage _live;               // message being streamed
        double _costBase = 0.0;             // cost before the current process (claude resets its total)
        double _processCost = 0.0;          // last total_cost_usd of the current process
        std::ofstream _log;

        // ---------- Pre-Function -------- //
        _cold _nodiscard std::vector<std::string> args_(const std::string& prompt) const;
        _cold void spawn_(const std::vector<std::string>& args, const bool input);   // starts the process (env -C ...)
        _cold void read_(void);             // loop of the worker: lines of stdout / stderr until the end
        _cold void runTurns_(void);         // worker of the per-turn drivers
        _cold void finished_(int code);     // the process ended
        _cold void write_(const cluster::Json& message);
        _cold void line_(const std::string& line);
        _cold void claude_(const cluster::Json& event);    // claude & qwen stream-json
        _cold void opencode_(const cluster::Json& event);
        _cold void codex_(const cluster::Json& event);
        _cold void entry_(const cluster::EntryKind kind, const std::string& text, const std::string& tool = "");
        _cold void tool_(const std::string& id, const std::string& name, const cluster::Json& input);
        _cold void turnDone_(const std::string& text, const bool error);
        _cold inline void touch_(void) {++this->_version;};
        _cold void emit_(const cluster::Event::Kind kind, const std::string& text = "", const bool error = false);

    public:
        // ---------- Pre-Function -------- //
        _cold void start(void);
        _cold void send(const std::string& text, const std::vector<std::string>& images = {}); // images: [Image #N] of the text
        _cold void answer(const std::string& requestId, const std::string& behavior); // allow | always | deny
        _cold void setMode(const std::string& mode);
        _cold void interrupt(void);
        _cold void stop(void);
        _cold void rename(const std::string& name);
        _cold void togglePanel(const std::string& panel);
        _cold void loadHistory(const std::size_t max);     // last entries of the log (restored session)

        // ---------- Function -------- //
        _nodiscard cluster::Snapshot snapshot(void) const;
        _nodiscard cluster::SessionSpec spec(void) const;
        _nodiscard cluster::State state(void) const;
        _nodiscard inline std::uint64_t version(void) const {return this->_version;};
        _nodiscard inline bool persistent(void) const {return this->_launch.backend.provider.driver == "claude";};   // claude driver: one process for the whole session
        _nodiscard inline const std::string& id(void) const {return this->_launch.spec.id;};

        // ---------- Constructor -------- //
        _cold Session(cluster::Launch launch);
        _cold ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
};

} // namespace end

#endif /* CLUSTER_SESSION_H */
