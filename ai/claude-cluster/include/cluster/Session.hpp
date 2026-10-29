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
    #include <optional>             // std::optional
    #include <fstream>              // std::ofstream
    #include <atomic>               // std::atomic
    #include <thread>               // std::thread
    #include <memory>               // std::unique_ptr
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <mutex>                // std::mutex
    #include <deque>                // std::deque
    #include <set>                  // std::set

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Event {
    enum class Kind {Init, TurnDone, Permission, Exited, Prompt, Tool}; // Prompt: one sent from elsewhere (claude.ai)
    Kind kind = Kind::Init;
    std::string session;
    std::string text;
    bool error = false;
    bool interrupted = false;   // TurnDone: stopped by an interrupt (not an error, nothing to notify)
    bool spontaneous = false;   // TurnDone: no prompt started it (a background task or a Monitor woke the agent up)
};

struct Queued {
    std::string text;
    std::vector<std::string> images;
    std::string context{};      // given to the agent before the text, not shown (the !commands run since the last prompt)
    std::int64_t at = 0;        // unix ms when it was queued
    bool given = false;         // written to the agent while its turn runs (taken at its next step): no longer editable
    std::string origin{};       // user (empty) | mcp | task | schedule | wake (claude-cluster woke the agent for itself: its background
                                // done, a [job] / [tasks] told - a turn of its own): shown as the colour of the prompt
};

struct Launch {
    cluster::SessionSpec spec;
    cluster::Backend backend;
    cluster::Env env;
    std::string command;                    // program of the driver
    bool allowBypass = true;                // claude: --allow-dangerously-skip-permissions (bypassPermissions possible)
    std::vector<std::string> extraArgs;     // added by the manager (global session: MCP, system prompt)
    std::filesystem::path log;              // JSONL of the transcript
    int transcriptLimitMb = 0;              // its size kept under it (MB, 0: no limit)
    std::uintmax_t memoryBytes = 1 << 20;   // its entries in memory within this size (the older ones on demand from the log)
    std::size_t memoryEntries = 0;          // and at most this number of them (0: no count)
    std::function<void(const cluster::Event&)> listener;
    std::string remoteName;                 // claude: Remote Control on at every start under this name (empty: off)
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
        std::deque<cluster::Queued> _pending;   // prompts waiting for the end of the running turn (kept here: editable)
        std::string _stderr;                // tail of the error output
        std::string _lastText;              // last assistant text of the turn
        std::string _context;               // given with the next prompt (the !commands run since the last one)
        std::string _heldContext;           // a !command waiting for its choice (given or not)
        bool _interrupted = false;          // an interrupt was asked: the end of the turn is not an error
        bool _prompted = false;             // a prompt was written since the last end of turn (else the next one is a wake-up of the agent)
        cluster::Queued _lastPrompt;        // last prompt sent (rewind: back in the prompt with its images)
        std::set<std::string> _sent;        // uuids of the prompts written here (their replay is not shown twice)
        std::uintmax_t _memBytes = 1 << 20; // the entries in memory within this size, at most _memEntries of them (0: no count)
        std::size_t _memEntries = 0;
        std::int64_t _keepUntil = 0;        // older entries loaded on demand: kept until then (unix seconds), the budget raised to _keptBytes
        std::uintmax_t _keptBytes = 0;
        std::map<std::string, cluster::Json> _inputs;   // the input of the tools running (by tool_use id): a command stopped
        std::string _approved;              // a command the user allowed after the safety check stopped it: allowed when asked again
        std::string _modeBack;              // the mode it is run out of (auto: never asks), put back after
        std::int64_t _restoreAfter = 0;     // that mode put back once the turns reach this count
        int _noVerdict = 0;                 // the auto mode classifier without a verdict, in a row
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
        _cold void entry_(const cluster::EntryKind kind, const std::string& text, const std::string& tool = "", const std::string& origin = "");
        _cold void tool_(const std::string& id, const std::string& name, const cluster::Json& input);
        _cold cluster::SubAgent& agent_(const std::string& toolUseId, const std::string& taskId);  // the sub-agent of this call / task (made when new)
        _cold void agentTask_(const std::string& subtype, const cluster::Json& event);       // task_started / progress / updated / notification
        _cold void agentMessage_(const cluster::Json& event);                                 // what a sub-agent says and does
        _cold void picture_(const std::string& id, const cluster::Json& block, const bool error); // a picture in a tool result (or made by its command): kept on the call
        _cold void turnDone_(const std::string& text, const bool error);
        _cold void dispatch_(const cluster::Queued& prompt);  // claude: written now (lock held)
        _cold void queue_(void);                                // snapshot of the queue
        _cold void emit_(const cluster::Event::Kind kind, const std::string& text = "", const bool error = false, const bool interrupted = false, const bool spontaneous = false);

        // ------------ Function ---------- //
        _cold inline void touch_(void) {++this->_version;};
        _cold void trim_(void);                            // (locked) the entries in memory within the budget: the oldest ones dropped, counted in older

    public:
        // ---------- Pre-Function -------- //
        _cold void start(void);
        _cold void restart(void);                           // its reader done (process ended): started again
        _cold void send(const std::string& text, const std::vector<std::string>& images = {}, const std::string& origin = {}); // images: [Image #N] of the text
        _cold void answer(const std::string& requestId, const std::string& behavior); // allow | always | deny
        _cold void answerQuestions(const std::string& requestId, const cluster::Json& answers); // AskUserQuestion: {question: answer}
        _cold void showcasePermission(const cluster::Permission& permission); // the showcase: a permission / question answered here, never sent
        _cold void showcaseState(const cluster::State state);
        _cold void limitLog(const std::uintmax_t maxBytes);                    // its transcript kept within maxBytes (its oldest entries dropped)                 // the showcase: its state shown (its icon, the titles)
        _cold void setMode(const std::string& mode);
        _cold void interrupt(void);
        _cold void sendNext(void);                                         // the next queued prompt now: the turn interrupted, it sent
        _cold void stopAgent(const std::string& task);                     // one of its sub-agents stopped (its task id or its Agent call id)
        _cold void giveQueued(const int delayMs);                          // a claude turn running: the prompts queued for delayMs given to it (its next step); < 0: never
        _cold _nodiscard std::optional<cluster::Queued> unqueue(void);   // the last queued prompt, removed (to edit it)
        _cold void stop(void);
        _cold void rename(const std::string& name);
        _cold void describe(const std::string& text, const bool given);     // its short description (written by claude-cluster: not over one given)
        _cold _nodiscard std::vector<cluster::Queued> unfinished(void) const;  // the running turn's prompt and the queued ones (a restart: sent again)
        _cold _nodiscard std::vector<cluster::Queued> waiting(void) const;     // the queued prompts (given during the turn or not): saved, a quit keeps them
        _cold void setPin(const cluster::Json& pin);   // the pinned item of its view (null: none), kept with the session
        _cold void label(const std::string& category, const std::vector<std::string>& tags);  // its category and tags (kept by /clear, the trash, a restore)
        _cold std::int64_t countTurn(void);                                // a turn finished: its count
        _cold void warn(const std::string& text);                           // a problem of its interfaces, shown a minute on it
        _cold void taskTool_(const cluster::Json& tool, const std::string& result); // its todo list from TaskCreate / TaskList
        _cold void addContext(const std::string& text);                     // given to the agent with its next prompt (a !command and its output)
        _cold void holdContext(const std::string& text);                    // kept until shareContext() (the choice asked); a new one replaces it
        _cold bool shareContext(const bool give);                           // the one held given (or dropped); false: none held
        _cold void setClaudeId(const std::string& id);     // the conversation goes on under this id
        _cold void mark(const std::string& text);          // a line of the system in its transcript (and its log)
        _cold void append(const std::vector<cluster::Entry>& entries); // a transcript from elsewhere, shown and logged
        _cold void updateDisplay(const std::string& displayId, const std::string& text);
        void step_(const std::string& name);    // the turn goes to this step (under the lock)
        void turnStart_(void);                  // its steps from zero
        void noVerdict_(const std::string& tool);   // the auto mode classifier answered nothing for this tool (under the lock)  // a display replaced (its log written again)
        _cold void remoteControl(const bool on, const std::string& name); // claude: Remote Control of this session (claude.ai)
        _cold void togglePanel(const std::string& panel);
        _cold void setKeepWarm(const bool on);
        _cold void loadHistory(void);                      // the last entries of the log within the memory budget (restored session)
        _cold std::size_t loadOlder(const std::size_t count);  // up to count entries of the log before the first one in memory: loaded again (returns how many)
        _cold void limitMemory(const std::uintmax_t bytes, const std::size_t entries); // the memory budget of its entries (the settings of the moment)
        _cold _nodiscard std::uintmax_t memoryUsed(void) const; // the size of its entries in memory
        _cold void trimTo(const std::uintmax_t bytes);     // its oldest entries in memory dropped down to this size (the global budget)
        _cold _nodiscard cluster::Queued rewind(const int back = 1, std::vector<std::string>* dropped = nullptr, const bool byUser = true); // claude: the last `back` prompts removed from the conversation, the oldest of them returned (dropped: the prompts after it; byUser: a take-back of the user, what was removed kept in rewind/; false: a keep-warm wake-up)
        _nodiscard cluster::Snapshot snapshot(void) const;
        _nodiscard cluster::Snapshot snapshotLight(void) const; // without its entries (never copied): its name, labels, state, metrics
        _nodiscard cluster::SessionSpec spec(void) const;
        _nodiscard cluster::State state(void) const;
        _nodiscard std::tuple<cluster::State, std::size_t, bool> brief(void) const; // state, permissions waiting, one of them a safety check

        // ---------- Function -------- //
        _nodiscard inline std::uint64_t version(void) const {return this->_version;};
        _nodiscard inline bool persistent(void) const       {return this->_launch.backend.provider.driver == "claude";};   // claude driver: one process for the whole session
        _nodiscard inline const std::string& id(void) const {return this->_launch.spec.id;};

        // ------------ Operator ---------- //
        Session& operator=(const Session&) = delete;

        // ---------- Constructor -------- //
        _cold Session(cluster::Launch launch);
        Session(const Session&) = delete;

        // ----------- Destructor --------- //
        _cold ~Session();
};

} // namespace end

#endif /* CLUSTER_SESSION_H */
