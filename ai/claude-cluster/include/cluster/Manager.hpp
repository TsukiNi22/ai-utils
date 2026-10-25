/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Manager.hpp

File Description:
##  Every session of the cluster: spawn / close / trash / restore,
##  the global session (MCP, Remote Control), permissions, budget,
##  notifications, task queue, persistence, search & export
\**************************************************************/

#ifndef CLUSTER_MANAGER_H
    #define CLUSTER_MANAGER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "PromptStash.hpp"      // cluster::StashAdd
    #include "Taxonomy.hpp"         // cluster::Taxonomy
    #include "Machine.hpp"          // cluster::MachineProgress
    #include "Session.hpp"          // cluster::Session, cluster::Event
    #include "Config.hpp"           // cluster::Config
    #include "Asking.hpp"           // cluster::Asking
    #include "Types.hpp"            // cluster::Snapshot, cluster::Task
    #include "Icons.hpp"            // cluster::Notification, cluster::AppFace
    #include "Usage.hpp"            // cluster::Transcripts
    #include "Auth.hpp"             // cluster::Auth
    #include <condition_variable>   // std::condition_variable
    #include <functional>           // std::function
    #include <optional>             // std::optional
    #include <memory>               // std::shared_ptr
    #include <atomic>               // std::atomic
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <mutex>                // std::mutex
    #include <deque>                // std::deque
    #include <set>                  // std::set
    #include <map>                  // std::map

    //----------------------------------------------------------------//
    /* DEFINE */

    #define GLOBAL_ID "g" // id of the global session
    #define TASKS_ID "tasks" // the Tasks session of the interfaces (its todo lists, the task queue, the schedules): not a session
    #define KEEP_WARM_MARK "[claude-cluster keep-warm:" // the start of a keep-warm prompt (never a real prompt of the user)

namespace cluster { // namespace start
class Web;
class Voice;
struct ReplicaState;                // Manager-Replica.cpp: this process an interface of another one (its core)
class Orchestrator;                // Orchestrator.hpp: the heavy jobs of the sessions queued and limited
//----------------------------------------------------------------//
/* STRUCT */

struct UpgradeOffer {               // a newer claude-cluster installed over the running one
    std::string version;            // empty: none (or ignored); the newest installed (one may come while an update waits)
    bool started = false;           // Update chosen: the turns running end first
    bool upgrading = false;         // the core about to run the new binary (the interfaces follow it)
    std::string target;             // started: the version it switches to (a newer one offered meanwhile: Update moves it there)
    std::string waiting;            // started: what it waits for (the turns running, a permission)
    std::string jump;               // patch | minor | major (from the running version)
    std::vector<std::string> notes; // what it brings: "fix 1.4.103 the smooth update...", the newest first
};

// The release notes embedded in this binary (the commits of claude-cluster): "kind version text" of the versions after `since`, the newest first
_cold _nodiscard std::vector<std::string> release_notes(const std::string& since);

struct ShellJob {                   // a !command running or done (the background shells: /shells, /kill)
    std::string id;                 // b1, b2...
    std::string session;
    std::string command;
    std::string cwd;
    std::int64_t started = 0;
    std::int64_t ended = 0;
    std::atomic<int> pid{0};        // its process group
    bool done = false;
    bool background = false;        // given up by its waiter: its end is told in the conversation
    bool killed = false;
    int code = -1;
    std::string output;
};

struct Link {                       // an interface and its core
    bool lost = false;              // not reached: reconnecting
    bool upgrading = false;         // lost because the core is being updated: this interface follows it
    int tries = 0;
    std::int64_t since = 0;         // unix ms
    std::int64_t next = 0;          // unix ms of the next try
};

struct SpawnRequest {
    std::string cwd;
    std::string name;
    std::string backend;        // empty: config / profile
    std::string mode;           // empty: config / profile
    std::string profile;
    std::string prompt;         // first prompt (optional)
    std::string effort;         // empty: config
    bool cheap = false;         // a session of claude-cluster itself (Stats, showcase): the cheapest model and the lowest thinking
    std::string resume{};       // the id of a conversation of the agent to go on with (claude --resume...)
    bool fork = false;          // claude: resumed as a copy (--fork-session), the original untouched
    std::vector<cluster::Entry> history{};  // its transcript so far, shown in the session
    std::string note{};         // a line of the system after it (where it comes from)
    bool reply = false;         // its first prompt's answer (and the later wake-up turns) sent back to the global session
    bool oneShot = false;       // a session for one task: closed to the trash once its answer is relayed (sessions.archive_one_shot)
    std::string origin{};       // of its first prompt: mcp | task | schedule (empty: the user)
};

struct Notice {
    std::int64_t time = 0;
    std::string session;
    std::string text;
    bool error = false;
};

struct Hit {
    std::string session;
    std::string name;
    std::int64_t time = 0;
    std::string text;
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold bool start_core(const std::vector<std::string>& args = {});   // the core in a process of its own (its options), false not started
// the session kept warm (its prompt cache never let expire): [sessions] keep_warm = all, or chosen and the session marked
_nodiscard inline bool keep_warm_on(const cluster::Config& c, const cluster::SessionSpec& s) {return c.keepWarm == "all" || (c.keepWarm == "chosen" && s.keepWarm);};
// the same for a session as it is now: background, only while it waits on its background (shells / agents running, a long compilation)
_nodiscard inline bool keep_warm_on(const cluster::Config& c, const cluster::Snapshot& s) {return c.keepWarm == "background" ? cluster::background_busy(s) : keep_warm_on(c, s.spec);};

//----------------------------------------------------------------//
/* CLASS */

class Manager {
    private:
        cluster::Config& _config;
        cluster::Auth _auth;
        mutable std::mutex _mutex;
        std::map<std::string, std::shared_ptr<cluster::Session>> _sessions;
        std::vector<std::string> _order;                // sub-sessions, in creation order
        std::vector<cluster::SessionSpec> _trash;
        std::vector<cluster::SessionSpec> _previous;    // sessions of the previous run (restore prompt)
        std::string _focus;                             // the session the interfaces are asked to show
        std::optional<cluster::MachineProgress> _scan;  // the last map of this computer (its progress)
        std::int64_t _scanEnd = 0;                      // ms of its end (0: running)
        std::atomic<std::int64_t> _globalEnsured{0};    // the last time the tick started the missing global session
        std::atomic<bool> _globalRetrying{false};       // a loop starts the global session again until it runs
        std::int64_t _globalRestart = 0;                // last time the global session was started again (its process ended)
        std::atomic<bool> _closing{false};              // claude-cluster quits: the agents ending are not an error
        std::map<std::string, int> _replies;            // sessions whose next turn ends are sent back to the global session
        std::set<std::string> _oneShot;                 // sessions to close once their answer is relayed (untouched by the user since)
        cluster::Json _handoff;                         // handoff.json read once at the start: the turns / queued prompts not given back yet
        bool _handoffRead = false;
        std::map<std::string, std::set<std::string>> _followed; // session -> who asked it (the global session, a sub-session's task): its wake-up turns come back to them too
        bool _globalNew = false;                        // the global session starts a new conversation (marked in its transcript)
        std::atomic<std::uint64_t> _focusVersion{0};
        std::atomic<std::uint64_t> _raiseVersion{0};    // a notification clicked: the interface to the front
        std::string _active = "g";                      // the session they show
        std::vector<cluster::Task> _tasks;
        std::vector<cluster::Group> _groups;            // the requests of the global session on several sessions (this run)
        int _nextGroup = 1;
        std::vector<cluster::Schedule> _schedules;      // things to do at a time (kept in the state)
        std::thread _bgThread;                          // the checks of the tasks' events and the schedules due (commands, git): off the tick
        std::atomic<bool> _bgBusy{false};
        std::int64_t _bgPoll = 0;                       // the last pass over the events of the tasks
        int _nextSchedule = 1;
        std::deque<cluster::Notice> _notices;
        std::map<std::string, std::string> _ui;         // layout / style chosen live (state.json)
        cluster::Taxonomy _taxonomy;                    // the forest of the categories / tags (taxonomy.json)
        int _next = 1;
        int _nextTask = 1;
        std::atomic<std::uint64_t> _version{1};
        mutable std::mutex _versionMutex;          // version(): its sessions as last seen, the number given for them (only grows)
        mutable std::uint64_t _versionSeen = 0;
        mutable std::uint64_t _versionGiven = 0;
        mutable std::mutex _progressMutex;        // the long operation shown (its popup)
        std::vector<cluster::Progress> _progresses; // the long operations (the oldest first), their results kept 1.5 s
        std::uint64_t _progressNext = 1;
        std::deque<std::pair<cluster::Failure, std::function<void(void)>>> _failures; // failed, their retry (under _progressMutex)
        std::map<std::string, std::function<void(void)>> _failureOther;              // their third choice, by id (under _progressMutex)
        std::uint64_t _failureNext = 1;
        std::set<std::string> _failureGone;        // the ones of the core answered here (until its next sync)
        std::atomic<int> _faceKey{-1};             // the face of the app last seen by the tick (its change: a new version)
        std::atomic<bool> _pluginsBusy{false};     // plugins of repositories: listed / installed by a thread
        std::map<std::string, std::shared_ptr<cluster::ShellJob>> _jobs;   // the !commands (the last 30)
        std::mutex _jobMutex;
        std::condition_variable _jobEnded;
        int _jobN = 0;
        cluster::Json _accountsProviders;          // the state of the providers (a keyring read starts a process: read in a thread, kept)
        std::int64_t _accountsAt = 0;
        std::atomic<bool> _accountsBusy{false};
        std::map<std::string, std::function<void(const std::string&)>> _coreAsks;   // the questions of the core waiting for their answer
        std::atomic<int> _coreAskN{0};
        bool _upgradeShowcase = false;             // the Debug page's pretend update: its popup, nothing installed / stopped
        std::string _upgradeVersion;               // a newer binary installed (offered), its version ignored, the binary seen
        std::string _upgradeTarget;                // the version an update started switches to
        std::string _upgradeWaiting;               // what it waits for (shown while it waits)
        std::string _upgradeJump;                  // patch | minor | major
        std::vector<std::string> _upgradeNotes;    // what the offered version brings (read from its binary)
        std::string _upgradeIgnored;               // read by the Update card (upgradeIgnored)
        std::int64_t _exeStamp = 0;
        std::atomic<bool> _upgradeStarted{false};
        std::atomic<bool> _upgrading{false};
        std::atomic<bool> _holding{false};              // an update waiting for the turns: what would start a turn is held (held.json), sent by the new version
        bool _heldReleased = false;                     // the prompts held by the version before: sent (once)
        bool _pluginsSeen = false;
        std::vector<std::string> _pluginRepos;     // the [plugins] last applied
        std::map<std::string, bool> _pluginSelect;
        std::map<std::string, bool> _pluginOff;
        std::map<std::string, bool> _pluginPaused;
        std::set<std::string> _pluginPull;         // the sources to pull at the next tick ("*": every one), under _mutex
        std::int64_t _pluginSyncAt = 0;            // the last pull of the auto ones (every [plugins] sync_hours)
        std::atomic<std::uint64_t> _machineRequest{0};  // the global session asked to map this computer again (the UI asks)
        std::atomic<bool> _restartAfterTurn{false};     // its system prompt changed by itself: started again after its turn
        std::deque<cluster::Json> _pushLog;             // the last notifications published to the app (100)
        std::atomic<std::uint64_t> _pushSeq{0};         // the number of the last one (a poll waits for it to grow)
        std::map<std::string, std::string> _notifyIds;  // the notification shown per session (replaced by the next one)
        std::map<std::string, std::pair<cluster::Notification, std::string>> _notifyShown;   // by its DBus id: the notification, its token when clicked
        std::thread _notifyMonitor;                     // gdbus monitor: the clicks of the notifications (ActionInvoked, ActivationToken)
        std::atomic<bool> _notifyMonitorUp{false};
        std::string _raiseToken;                        // the activation token of the last click (XDG_ACTIVATION_TOKEN: the window to the front)
        std::map<std::string, std::string> _origin;     // who sent the last prompt of a session: "host" or "web:<device>" (notified)
        std::pair<cluster::Face, std::int64_t> _moment{cluster::Face::Idle, 0};   // a finished turn / a global answer: shown a moment
        std::map<std::string, int> _limitLevel;         // a limit close: the level already told (80, 95, 100)
        std::atomic<bool> _readyTold{false};            // "claude-cluster is ready" told (once a run)
        std::int64_t _startedAt = cluster::now();       // the start of this run
        std::atomic<int> _focused{-1};                  // the interface has the focus: 1, not: 0, unknown: -1 (notified)
        cluster::Web* _web = nullptr;                   // the web access of this instance (owned by the App)
        std::unique_ptr<cluster::Asking> _asking;       // the question of an agent being answered (the interfaces, the voice)
        mutable std::mutex _statsMutex;                 // stats.jsonl
        std::int64_t _statsUp = 0;                      // the last sign of life written
        std::vector<cluster::UsageRecord> _usage;       // the usage ledger (usage.jsonl), in time order
        std::map<std::string, cluster::UsageReport> _limits; // per vendor: the last limits seen (5 h / 7 d windows), the account's
        cluster::Transcripts _transcripts;              // the transcripts of Claude Code (every session of this computer)
        std::atomic<bool> _usageBusy{false};            // a scan of the transcripts running
        std::atomic<bool> _accountBusy{false};          // a read of the account running
        std::int64_t _accountTried = 0;                 // the last look at the shared copy of the account (unix seconds)
        std::atomic<bool> _usageStop{false};            // the manager ends: the scan stops, the threads joined
        std::thread _scanThread;                        // the scan of the transcripts (joined before the next one, at the end)
        std::thread _accountThread;                     // the read of the account
        mutable std::map<std::string, std::pair<std::int64_t, cluster::UsageReport>> _usageCache;   // an interface of the core: the report of the core, 2 s
        struct Describer {                              // the descriptions being written (a thread of their own, one at a time)
            std::mutex mutex;
            bool alive = true;                          // false: claude-cluster quits (a description written after it: dropped)
            bool running = false;
            std::deque<std::string> queue;              // the sessions to describe
        };
        std::shared_ptr<Describer> _describer = std::make_shared<Describer>();
        std::shared_ptr<cluster::ReplicaState> _replica; // this process: an interface of another core (null: it is the core)
        cluster::Voice* _voice = nullptr;               // the voice of this core (its actions run here)
        std::map<std::string, std::int64_t> _clients;   // the interfaces of other processes: last seen (sync)
        std::map<std::string, bool> _clientFocus;       // their focus (one in front: no notification)
        bool _hadClients = false;
        std::size_t _historyWrites = 0;                 // prompts added since the history was last cut
        bool _background = false;                       // kept without interface: the web closed, stopped by itself once every session is idle
        bool _webBeforeBackground = false;              // the web access was open when the core went in background: opened again by an interface back
        std::int64_t _backgroundSince = 0;              // unix s: when it went in background (0: not)
        std::map<std::string, std::pair<std::string, std::int64_t>> _lastSender;   // by session: the interface of its last prompt, when
        std::atomic<bool> _restoreAsk{false};           // started alone (a core of its own): the previous run asked by its first interface
        std::string _lastActive;                        // a replica: the session shown, told to the core once

        // ---------- Pre-Function -------- //
        _cold _nodiscard std::shared_ptr<cluster::Session> get_(const std::string& id) const;
        _cold _nodiscard std::shared_ptr<cluster::Session> make_(cluster::SessionSpec spec);
        _cold void event_(const cluster::Event& event);
        void replyUndo_(const std::string& id);
        void archiveOneShot_(const std::string& id);        // a one-shot session whose answer was relayed: closed to the trash when nothing keeps it
        std::unique_ptr<cluster::Orchestrator> _orch;   // the core: the queue of the heavy jobs (null: an interface of another core)
        void replyEnd_(const cluster::Event& event, const std::string& name, const bool all = false);   // an end of turn: the global session told
        _cold bool describe_(const std::string& id, const std::string& text, const bool given); // false: no such session
        _cold void describeAuto_(const std::string& id);    // written by claude-cluster (its thread)
        _cold void loadTaxonomy_(void);                     // Manager-Taxonomy.cpp: taxonomy.json read
        _cold void saveTaxonomy_(void) const;               // ... written
        _cold void relabel_(const std::string& from, const std::string& to);   // a label renamed / merged / removed (to empty) on every session
        _cold void turnCounted_(const std::string& id, const bool failed);
        _cold void describeMissing_(void);
        _cold _nodiscard std::vector<cluster::Snapshot> replicaSessions_(void) const;
        _cold _nodiscard std::uint64_t replicaVersion_(void) const;
        _cold void notice_(const std::string& session, const std::string& text, const bool error = false);
        _cold void progressNotice_(const cluster::Notice& notice);   // an operation running: the notice under it
        _cold _nodiscard std::vector<cluster::Progress> progressQueue_(void) const; // this process's ones, then the core's ones
        _cold _nodiscard std::vector<std::string> globalArgs_(void) const;
        _cold _nodiscard std::filesystem::path globalDir_(void) const;     // global.cwd, empty: <data>/global
        _cold void writeMcpConfig_(void) const;
        _cold void ensureGlobal_(void);             // the tick: the global session started when it is missing
        _cold void retryGlobal_(void);                  // the global session started again until it runs
        _cold _nodiscard std::vector<std::string> subArgs_(const std::string& id) const;   // the MCP of a sub-session (its tasks / schedules): its arguments, none when closed
        _cold void fireSchedules_(void);                // the schedules due: done, then again (every) or removed
        _cold void handoff_(void);                      // what a replaced core of another version had going (handoff.json), given back here
        _cold void schedule_(void);                         // Manager-Tasks.cpp
        _cold void enforceTasks_(void);                     // the running tasks: past their timeout / budget stopped
        _cold void bgPass_(const std::vector<std::string>& due);   // Manager-Schedules.cpp, in a thread: the events, the schedules due
        _cold void runSchedule_(const std::string& id);             // one schedule due: its conditions, its action, its next time
        _cold void applySchedule_(cluster::Schedule& w, const cluster::Json& options, const bool creating, const std::int64_t now);   // its fields set (an error: why)
        _cold void pollEvents_(void);                               // the events of the queued tasks (command, git, file)
        _cold _nodiscard std::string execute_(const cluster::Schedule& w);   // its action done; what it did
        _cold _nodiscard bool conditionMet_(cluster::ScheduleIf& cond, std::string& why);
        _cold void advance_(cluster::Schedule& w, const std::int64_t now, const bool fired);    // its next time (or its end), under the lock
        _cold _nodiscard std::int64_t resetAt_(const std::string& window) const;   // the next reset of the 5h / 7d limit (0: unknown; the lock held)
        _cold _nodiscard std::size_t spawnedBy_(const std::string& by) const;      // the sessions of its tasks waiting or open (the lock held)
        _cold void pluginsSync_(void);                      // [plugins] changed: listed / installed (a thread)
        _cold void upgradeCheck_(void);                     // the binary on disk newer: offered (Manager-Upgrade.cpp)
        _cold bool hold_(const std::string& id, const std::string& text, const std::vector<std::string>& images, const std::string& origin); // held: true
        _cold void holdQueues_(void);                       // the prompts queued in the sessions: held too (none starts a turn before the switch)
        _cold void releaseHeld_(void);                      // the prompts held by the version before: sent now
        _cold void holdAsk_(const cluster::Snapshot& s);    // a session waiting for the user: its request held (asked again after), its turn stopped
        _cold void holdAsks_(std::set<std::string>& asked); // every session waiting for the user (the ones not in asked yet): holdAsk_
        _cold void upgradeKeep_(void);                      // what is on for this run written before the switch (upgrade-keep.json)
        _cold _nodiscard std::size_t heldCount_(void) const;
        _cold void taskDone_(const std::string& session, const bool error, const bool interrupted, const std::string& text);
        _cold void groupUpdate_(const std::string& id);     // Manager-Groups.cpp: its card, the global session woken, its end
        _cold void groupsTick_(void);                       // the groups whose time is over
        _cold void loadUsage_(void);                        // Manager-Usage.cpp
        _cold void scanTranscripts_(void);                  // in a thread: what grew
        _cold void refreshAccount_(void);                   // in a thread: the windows of the account
        _cold void limitMemory_(const std::vector<std::shared_ptr<cluster::Session>>& sessions);
        _cold void restartYoung_(std::set<std::string>& restarted);  // an update: the young safe background items stopped (run again after it), noted
        _cold void keepWarm_(void);                                  // the idle sessions kept warm woken up before their prompt cache expires // the transcripts in memory: each within its budget, all within the global one
        _cold void recordUsage_(const std::string& session); // a turn finished: in the ledger, its limits kept
        void archive_(const std::string& id);               // its transcript to logs/archive/<id>-<time>.jsonl (search only)
        _cold void relaunch_(const std::string& id, const cluster::SessionSpec& spec); // started again with this spec (same conversation)

    public:
        std::function<void(const std::string&)> onGlobalAnswer;   // voice: text of an answer of the global session
        std::function<void(const cluster::Notice&)> onNotice;

        // ---------- Pre-Function -------- //
        _cold void adoptProvider_(cluster::SessionSpec& spec) const; // a session of the last run on the provider of the config
        _cold void load(void);                              // state.json: trash, tasks, previous sessions, ui choices

        /* several interfaces on one core (Manager-Replica.cpp) */
        _cold void attach(void);                            // this process an interface of the core running
        _cold _nodiscard cluster::Json call(cluster::Json request) const;  // a command run by the core (throws: refused, not reached)
        _cold _nodiscard cluster::Json replicaState(const std::string& key) const;   // a part of the last sync
        _cold _nodiscard std::uint64_t syncVersion(void);          // the core: the version its interfaces wait for (sessions, questions, voice)
        _cold _nodiscard std::string replicaError(void) const;              // the core not reached ("": in sync)
        _cold _nodiscard cluster::Link link(void) const;                    // an interface: the core lost (since, tries, the next one)
        _cold void replicaPatchUi(const std::string& key, const std::string& value);   // a ui value set here, read before the next sync
        _cold void retryNow(void);                                          // an interface: the next try now (the core started again if gone)
        _cold void detach_(void);                           // this interface closed: told to the core
        _cold void clientSeen(const std::string& client);   // the core: an interface synced
        _cold void startOrchestrator_(void);                // Manager-Jobs.cpp
        _cold _nodiscard cluster::Json runHeavy_(const std::string& command, const std::string& cwd, const int timeout, const std::string& session);   // a heavy command of a schedule through the queue (null: not heavy)
        _cold _nodiscard std::vector<std::pair<std::string, std::string>> jobEnv_(const std::string& session) const;   // the PATH / variables of cluster-run for a session
        _cold void clientGone(const std::string& client);
        _cold void clientFocus(const std::string& client, const bool focused);
        _cold _nodiscard std::size_t clients(void) const;   // its interfaces alive
        _cold void setBackground(const bool on); // the core kept without interface: its web access closed; off: an interface came back
        _cold void requestBackground(void); // an interface quitting with sessions running: its core kept (a replica: told)
        _cold _nodiscard bool busy(void) const; // a session working, or with prompts queued: something to finish
        _cold void noteSender(const std::string& id, const std::string& client); // two interfaces sending at once: told
        _cold _nodiscard cluster::Json syncState(const cluster::Json& request);  // what an interface gets

        /* usage (Manager-Usage.cpp) */
        _cold _nodiscard std::string vendorOf(const std::string& backend) const;   // its provider ("" unknown)
        _cold _nodiscard cluster::UsageReport usage(const std::string& vendor) const;
        _cold void pluginsRefresh(const std::string& source = "");   // a plugin source (empty: every one) pulled / listed again, the ticks applied
        _cold _nodiscard cluster::Json pluginsJson(void);        // the sources: their settings, dates, counts, items (the plugins page of every interface)
        _cold void pluginsSet(const cluster::Json& changes);    // [{"table": select|off|auto|paused, "key", "value": bool}], {"repos": [...]}: written at once
        _cold void askUser(const std::string& session, const std::string& header, const std::string& question, const std::vector<std::pair<std::string, std::string>>& options,
            const std::function<void(const std::string&)>& onAnswer);                 // the core asks the user (a popup on every interface): the label chosen, or "deny"
        _cold _nodiscard cluster::Json modelsPolicyJson(void);        // the policy + the models (tier, price, the user's cost), the efforts, the limits: the tool of the global session
        _cold std::string modelChange(const std::string& id, const std::string& model, const std::string& effort, const bool gated); // gated: the policy of the global session
        _cold std::function<void(const std::string&)> takeCoreAsk_(const std::string& requestId);   // the callback of a question of the core, once
        _cold _nodiscard double spent(const std::int64_t since) const;      // what the sessions spent since then (the scope of [limits])
        _cold _nodiscard std::string limitReached(void) const;               // [limits]: the one reached (empty: none)
        _cold void limitsWarn(void);                                         // the share of a limit reached: told once
        _cold _nodiscard cluster::Json accountsJson(void);                   // the providers (state) and the spending, cached a few seconds
        _cold _nodiscard std::string authKind(const std::string& backend) const;   // api (an API key: billed per token) | auth (a login: a subscription) | local
        _cold void showcaseUpgrade(void);                                   // the Debug page: a pretend newer version offered
        _cold _nodiscard cluster::UpgradeOffer upgradeOffer(void) const;    // a newer claude-cluster installed (the core's)
        _cold std::string upgradeAct(const std::string& action);            // update | ignore | quit
        _cold void upgradeKept(cluster::Web* web);                          // just updated: what was on before the switch on again (the web access, background)
        _cold _nodiscard std::vector<std::string> usageVendors(void) const;         // the ones in the ledger
        _cold _nodiscard std::vector<std::pair<std::string, std::string>> usageLines(const cluster::Snapshot& snap) const; // panel "usage"
        _cold _nodiscard cluster::Json usageJson(const std::string& vendor) const;  // every vendor when empty
        _cold void usageRefresh(void);              // now: the transcripts, the account (a panel opened, the command)
        _cold void save(void) const;
        _cold void restorePrevious(const bool restore);     // restore the previous sessions, or move them to the trash
        _cold void answerRestore(const bool restore);       // the question at start: + the global session (resumed or new)
        _nodiscard bool restorable(void) const;              // sessions of the previous run, or a global conversation
        _cold void startGlobal(void);
        _cold void shutdown(void);                          // stop every session (kept for the next run)
        _cold void tick(void);                              // every second: config reload, tasks, schedules, purge of the trash

        /* sessions */
        _cold _nodiscard std::string spawn(const cluster::SpawnRequest& request);
        _cold void send(const std::string& id, const std::string& text, const bool force = false,
            const std::vector<std::string>& images = {}, const std::string& origin = {}); // origin: mcp | task | schedule (empty: the user); not in the history: the prompts typed by the user only (addHistory)
        _cold void clear(const std::string& id);                         // /clear: a new conversation, the transcript archived (empty)
        _cold void setBackend(const std::string& id, const std::string& provider); // the session again on another provider (anthropic: the login, anthropic-api: an API key), same model and conversation
        _cold void setModel(const std::string& id, const std::string& model);   // /model: the session again on this model (same conversation)
        _cold void setEffort(const std::string& id, const std::string& effort); // /effort: thinking / reasoning (default, low... max)
        _cold void close(const std::string& id);
        _cold _nodiscard std::string restore(const std::string& id);
        _cold void purge(const std::string& id = "");       // delete for good from the trash (empty: the expired ones, "*": all)
        _cold void rename(const std::string& id, const std::string& name);
        _cold void askGlobal(const std::string& id, const std::string& requestId, const cluster::Json& questions); // its questions answered by the global session
        _cold void chatAbout(const cluster::Ask& ask);
        _cold _nodiscard std::string resume(const std::string& id);   // retry now / continue (what it did)
        _cold _nodiscard cluster::Json shell(const std::string& id, const std::string& command);  // !command in its folder: {code, lines, output, share}; {interactive}; {background}
        _cold _nodiscard cluster::Json shells(void);                                            // the !commands (running first): id, session, command, seconds, code, lines, tail
        _cold std::string shellKill(const std::string& which);                                   // a job by its id, or all: stopped with what it started
        _cold cluster::Json shellRecord_(const std::shared_ptr<cluster::Session>& session, const std::string& command, std::string output, const int code, const std::string& note);
        _cold _nodiscard std::string shareShell(const std::string& id, const std::string& choice); // the !command held: yes | no | always | never
        // a model / mode / effort where asked: here (the session given), global, subs (every sub-session), all, new (the default
        // of the next sub-sessions) or a session id; what it did
        _cold _nodiscard std::string applyTo(const std::string& kind, const std::string& value, const std::string& scope, const std::string& current);      // "Chat about this": its last message answered by the model of the session (background)
        _cold void describe(const std::string& id, const std::string& text); // its short description (empty: written again)
        // the work method: what the global session reads, the sample of the habits of the user it may judge (no model), its proposal (the user
        // answers Allow / Deny: the question returned) and a session it started counted (true: the detection is due)
        _cold _nodiscard cluster::Json methodJson(void);
        _cold _nodiscard cluster::Json methodSample(const std::size_t max);
        _cold _nodiscard std::string methodPropose(const std::string& preset, const std::string& text, const std::string& reason);
        _cold _nodiscard bool methodSpawned(void);
        // its category (none given: kept; "" removes it) and tags (tags: replaces them; add / remove: changes them), the trash too
        _cold void label(const std::string& id, const std::optional<std::string>& category, const std::optional<std::vector<std::string>>& tags,
            const std::vector<std::string>& add = {}, const std::vector<std::string>& remove = {});
        _cold void setMode(const std::string& id, const std::string& mode);
        _cold _nodiscard cluster::Queued rewind(const std::string& id, const int back = 1); // the last `back` prompts removed from the conversation, the oldest of them back (Esc)
        _cold void cd(const std::string& id, const std::string& cwd);
        _cold void togglePanel(const std::string& id, const std::string& panel);
        _cold cluster::Json loadOlder(const std::string& id, const std::size_t count); // older entries of its transcript loaded again from its log ({loaded, older})
        _cold std::string setKeepWarm(const std::string& id, const std::string& value); // on | off | toggle: the session kept warm (the mode chosen if off)
        _cold void setRemote(const bool on);

        /* tasks (Manager-Tasks.cpp) */
        _cold _nodiscard std::string addTask(const std::string& prompt, const std::string& target, const std::vector<std::string>& after, const std::string& by = "");
        // task_add with every option (after_sessions, on_event, spawn, priority, timeout, retry, budget, expect...), a session (by) capped
        _cold _nodiscard std::string addTaskJson(const cluster::Json& options, const std::string& by = "");
        // schedule_add with every option; dry: nothing made, only when it would fire; returns {id, next, next_runs}
        _cold _nodiscard cluster::Json addScheduleJson(const cluster::Json& options, const std::string& by = "", const bool dry = false);
        _cold _nodiscard cluster::Json scheduleEdit(const std::string& id, const cluster::Json& patch);   // when, text, every, cron, days, window... changed
        _cold _nodiscard std::string taskSet(const std::string& id, const cluster::Json& patch);          // priority, timeout, retry... of a task not ended
        _cold _nodiscard cluster::Json taskJson(const cluster::Task& task) const;           // for a model (task_list)
        _cold _nodiscard cluster::Json scheduleJson(const cluster::Schedule& w) const;      // for a model: its rules, next times, last runs (schedule_list)
        _cold _nodiscard std::string addSchedule(const std::string& when, const std::string& action, const std::string& session, const std::string& text, const int every, const std::string& by = "");
        _cold _nodiscard std::vector<cluster::Schedule> schedules(void) const;
        _cold void cancelSchedule(const std::string& id);
        _cold void cancelTask(const std::string& id);
        // cancel | delete | archive | unarchive | force (started now) | retry (a failed one again); a reason: told to the session that made it
        _cold _nodiscard std::string taskAct(const std::string& id, const std::string& action, const std::string& reason = "");
        // cancel | delete | archive | unarchive | trigger / run (now) | pause | resume | snooze (reason: minutes) | ack
        _cold _nodiscard std::string scheduleAct(const std::string& id, const std::string& action, const std::string& reason = "");
        _cold void dropFor_(const std::string& session, const bool gone, const std::string& name = ""); // its tasks / schedules: cancelled (closed) or removed
        _cold void tell_(const std::string& by, const std::string& text);    // a session told (a prompt), else a notice
        _cold _nodiscard cluster::Json fanout(const cluster::Json& args);           // several prompts as a group (tasks), woken with their answers
        _cold _nodiscard cluster::Json groupStatus(const std::string& id) const;   // "": every group
        _cold void groupCancel(const std::string& id, const bool interrupt);
        _cold _nodiscard std::vector<cluster::Task> tasks(void) const;

        /* history (Store.cpp) */
        _cold _nodiscard std::vector<cluster::Hit> search(const std::string& text, const std::size_t max = 50) const;
        _cold _nodiscard std::string exportMarkdown(const std::string& id) const;
        // the conversations of the agents opened outside, forks, transcripts (Manager-External.cpp)
        _cold _nodiscard std::vector<std::string> conversationIds(void) const;     // the ones opened here (not offered again)
        _cold _nodiscard std::string importConversation(const std::string& agent, const std::string& id, const std::string& name, const bool fork);
        _cold _nodiscard std::string forkSession(const std::string& id, const std::string& name);
        _cold _nodiscard cluster::Json transcript(const std::string& id, const long from, const std::size_t count) const;
        _cold void loadContext(const std::string& id, const std::string& source);  // session:<id> | <agent>:<id> | a file
        _cold _nodiscard cluster::Json conversationMessages(const std::string& id) const;
        _cold _nodiscard std::string editConversation(const std::string& id, const cluster::Json& edits);
        _cold _nodiscard std::string display(const std::string& id, const cluster::Json& display); // a rich display in its transcript, no prompt: its id
        _cold void displayUpdate(const std::string& id, const std::string& displayId, const cluster::Json& display);  // replaced (its id kept)
        // the user on a display: tick (checklist), click (actions: its prompt sent), submit (form: its values to the global
        // session), play (audio, on this computer); what it did
        _cold _nodiscard std::string displayAct(const std::string& id, const std::string& displayId, const cluster::Json& act);
        _cold _nodiscard std::optional<cluster::Json> displayOf(const std::string& id, const std::string& displayId) const;
        _cold _nodiscard cluster::Json toJson(const cluster::Snapshot& snapshot, const bool full) const;
        _cold void addHistory(const cluster::Draft& draft, const std::string& session); // prompts sent (history.jsonl)
        _cold _nodiscard std::vector<cluster::Draft> drafts(const std::size_t max = 500, const std::string& session = "") const; // newest first, no
                                                            // duplicate; a session: its prompts only (the arrows), none: every one (Ctrl+R)
        _cold _nodiscard std::vector<std::string> history(const std::size_t max = 500, const std::string& session = "") const; // their texts
        _cold cluster::StashAdd stashPush(const cluster::Draft& draft, const std::string& session = "", const bool force = true); // prompt put aside (stash.json); force: a copy, never refused for [ui] stash_max
        _cold _nodiscard std::optional<cluster::Draft> stashPop(const std::string& session = "");    // the newest of this session first, else the newest
        _cold void draftSave(const std::string& key, const cluster::Draft& draft) const;         // the prompt typed, not sent (drafts.json): kept over a quit / crash
        _cold _nodiscard std::optional<cluster::Draft> draftLoad(const std::string& key) const;  // key: the interface (tty, gui...)
        _cold _nodiscard std::size_t stashSize(void) const;

        /* state */
        _nodiscard std::vector<cluster::Snapshot> list(const bool withGlobal = true) const;
        _nodiscard std::vector<cluster::Snapshot> shown(void) const;    // list(), in the order of the interfaces (grouped by category when asked)
        // the taxonomy of the categories / tags (Manager-Taxonomy.cpp): categories and tags form a forest under the root "global"; replicated
        _cold _nodiscard cluster::Taxonomy taxonomy(void) const;
        _cold _nodiscard cluster::Json taxonomyJson(void) const;        // {root, parents: {node: parent}, edit: the global session may edit it}
        // action: set_parent (node, other = the parent, "" / global: the root) | rename (other = the new name) | delete (strip: the label removed from the sessions too)
        // | merge (other = the node it joins); throws why (a cycle, a bad name)
        _cold void taxonomyEdit(const std::string& action, const std::string& node, const std::string& other, const bool strip = false);
        _cold _nodiscard std::vector<std::string> groupPath(const cluster::SessionSpec& spec) const;   // its group in the list: the path of its category / first tag (empty: none)
        /* heavy jobs (Manager-Jobs.cpp) */
        _cold _nodiscard cluster::Json jobsCommand(const std::string& cmd, const cluster::Json& request = cluster::Json::object());  // job_list, job_kill...: the core's orchestrator (a replica: through the core)
        _cold _nodiscard cluster::Json jobsState(void) const;                      // the gauges, limits and jobs the interfaces show
        _nodiscard inline cluster::Orchestrator* orchestrator(void) const {return this->_orch.get();};

        _nodiscard std::optional<cluster::Snapshot> snapshot(const std::string& id) const;
        _nodiscard std::vector<cluster::SessionSpec> trash(void) const;
        _nodiscard std::vector<cluster::SessionSpec> previous(void) const;
        _nodiscard std::vector<cluster::Notice> notices(const std::size_t max = 50) const;
        _nodiscard std::string resolve(const std::string& idOrName) const;   // id from an id or a name
        _nodiscard std::uint64_t version(void) const;
        _nodiscard std::string ui(const std::string& key, const std::string& fallback) const;
        void setUi(const std::string& key, const std::string& value);
        void requestFocus(const std::string& id);           // the interfaces show this session (the global session asks it)
        void userTouched(const std::string& id);            // the user typed in this session: a one-shot one stays
        // the pinned item of a session's view (one at most): a display (its id, an inline one put in the transcript first) or a message (its index);
        // by: user | global | sub (the settings may refuse the last two); unpin: removed; pinOf: {kind, id, title...} or null
        _cold _nodiscard cluster::Json pin(const std::string& id, const cluster::Json& what, const std::string& by = "user");
        _cold void unpin(const std::string& id, const std::string& by = "user");
        _cold _nodiscard cluster::Json pinOf(const std::string& id) const;
        void replyTo(const std::string& id, const std::string& by = GLOBAL_ID); // the end of its next turn sent to the global session; by a sub-session: its wake-up turns told to it
        bool notifyDesktop(const std::string& title, const std::string& text, const bool urgent = false); // false: notifications off
        // its face, buttons, sound; one per session (replaced); kind: global | done | permission | question | error | limit | web | task |
        // reminder (its setting, [ui] notify_when); false: not shown
        bool notify(const cluster::Notification& notification, const std::string& kind = "");
        // the notifications published to the Android app ([ui] notify_app): its long poll asks the ones after a number (Manager-Push.cpp)
        _nodiscard cluster::Json pushSince(const std::int64_t since) const;    // {seq, events: [{seq, time, kind, session, title, body, urgent}]}
        _nodiscard cluster::AppFace appFace(const bool listening) const;   // the face of the app now, its badge
        void setOrigin(const std::string& id, const std::string& origin);  // its last prompt from a web device: "web:<device>"
        _nodiscard std::string origin(const std::string& id) const;         // "host" (default) or "web:<device>"
        _cold void scanMachine(void);                       // the map of this computer written again (in the background, a notice)
        _nodiscard int machineAsk(void) const;              // asked at start: 0 no, 1 no map yet (not refused), 2 an old map (sync proposed)
        _nodiscard std::optional<std::pair<cluster::MachineProgress, std::int64_t>> machineProgress(void) const; // the last scan, ms of its end (0: running)
        _cold void restartGlobal(void);                     // its system prompt read again, the same conversation
        _cold void setFocused(const bool focused);          // the window / terminal has the focus (a replica: told to the core)
        _cold void noteActive(const std::string& id);       // the session shown (ui_state; a replica: told to the core when it changes)
        _nodiscard std::pair<std::uint64_t, std::string> focusRequest(void) const;
        _nodiscard std::uint64_t raiseRequest(void) const;  // a notification clicked: to the front
        _nodiscard std::string raiseToken(void) const;      // its activation token (Wayland: the window may take the focus with it; empty: none)
        _cold void pushNotice_(const cluster::Notification& n, const std::string& kind);   // a notification of the desktop, also for the app
        _cold void notifyAction_(const cluster::Notification& n, const std::string& chosen, const std::string& token);  // a button / the body clicked
        _cold void notifyMonitor_(void);                    // the signals of the notification daemon, for the life of the core
        _nodiscard std::uint64_t machineRequest(void) const;
        _cold void answer(const std::string& id, const std::string& requestId, const std::string& behavior);
        _cold void answerQuestions(const std::string& id, const std::string& requestId, const cluster::Json& answers);
        _cold void showcasePermission(const std::string& id, const cluster::Permission& permission); // the showcase (Showcase.cpp)
        _cold void showcaseState(const std::string& id, const cluster::State state);
        _cold void interrupt(const std::string& id);
        _cold void sendNext(const std::string& id);   // its next queued prompt now (the running turn interrupted)
        _cold void stopAgent(const std::string& id, const std::string& task);   // one sub-agent of the session stopped (the session goes on)
        // a long operation: its popup in every interface (begin, what it does now, a line of its log, its end)
        _cold std::uint64_t progressBegin(const std::string& title, const std::string& session = "");
        _cold void progressStep(const std::uint64_t id, const std::string& now);
        _cold void progressAdvance(const std::uint64_t id, const std::string& now);    // the step before in its log (done)
        _cold void progressLog(const std::uint64_t id, const std::string& line);
        _cold void progressEnd(const std::uint64_t id, const bool ok, const std::string& text, const bool notice = true);
        _cold void progressDrop(const std::uint64_t id);
        // what failed of what the user did: its popup in every interface (OK, Retry: run again), answered once
        _cold std::string fail(const std::string& action, const std::string& source, const std::string& error, const std::function<void(void)>& retry = {},
            const std::string& otherLabel = "", const std::function<void(void)>& other = {});  // other: a third choice (its label on its button)
        _cold _nodiscard std::vector<cluster::Failure> failures(void) const;
        _cold void failureAnswer(const std::string& id, const std::string& answer);   // ok | retry | other
        _cold inline void failureAnswer(const std::string& id, const bool retry) {this->failureAnswer(id, std::string(retry ? "retry" : "ok"));};
        _cold _nodiscard std::optional<cluster::Progress> progress(void) const;    // the one shown (the first of the queue running 0.4 s, or its result)
        _cold _nodiscard std::vector<cluster::Progress> progressAll(void) const;   // this process's ones (running, results < 1.5 s)
        _cold _nodiscard std::optional<cluster::Queued> unqueue(const std::string& id); // last queued prompt, removed
        _nodiscard std::vector<cluster::UsageRecord> usageLedger(void) const;                               // a copy of the usage ledger
        _cold void statsRecord(const std::string& kind, const cluster::Json& fields = cluster::Json::object()); // one event of the activity (core only)
        _nodiscard std::vector<cluster::Json> statsEvents(const std::int64_t since) const;                  // the events since (unix s)

        // ---------- Function -------- //
        _nodiscard inline bool replica(void) const {return this->_replica != nullptr;};
        _nodiscard inline bool hadClients(void) const {return this->_hadClients;};
        _nodiscard inline bool background(void) const {return this->_background;};
        _nodiscard inline std::int64_t backgroundSince(void) const {return this->_background ? this->_backgroundSince : 0;};
        inline void askRestoreLater(void) {this->_restoreAsk = true;};   // its first interface asks the previous run
        inline void setVoice(cluster::Voice* voice) {this->_voice = voice;};
        _nodiscard inline cluster::Voice* voice(void) const {return this->_voice;};
        _nodiscard inline std::int64_t startedAt(void) const {return this->_startedAt;};   // the start of this run (unix s)
        _cold _nodiscard inline bool progressActive(void) const {return !this->progressQueue_().empty();}; // one running or its result shown (the interfaces redraw)
        _cold inline void notice(const std::string& session, const std::string& text, const bool error = false) {this->notice_(session, text, error);}; // the notices of the interfaces
        inline void requestMachineSync(void) {++this->_machineRequest; ++this->_version;};  // the interfaces ask the user (its popup)
        inline void restartGlobalAfterTurn(void) {this->_restartAfterTurn = true;};          // after the turn running (it asked it)
        inline void machineLater(void) {this->setUi("machine_later", std::to_string(cluster::now()));}; // an old map: proposed again machine_days later
        inline void machineDecline(void) {this->setUi("machine", "no");};
        inline void requestRaise(const std::string& token = "") {{std::lock_guard<std::mutex> lock(this->_mutex); this->_raiseToken = token;} ++this->_raiseVersion; ++this->_version;};
        _nodiscard inline std::string active(void) const {return this->_active;};
        _nodiscard inline cluster::Auth& auth(void) {return this->_auth;};
        _nodiscard inline cluster::Config& config(void) {return this->_config;};
        _nodiscard inline bool remote(void) const {return this->ui("remote", "false") == "true";}; // Remote Control of the global session
        inline void setWeb(cluster::Web* web) {this->_web = web;};
        _nodiscard inline cluster::Asking& asking(void) {return *this->_asking;};
        _nodiscard inline cluster::Web* web(void) const {return this->_web;};
        _nodiscard inline std::uint64_t pushSeq(void) const {return this->_pushSeq.load();};   // the last notification published to the app

        // ---------- Constructor -------- //
        _cold Manager(cluster::Config& config);

        // ----------- Destructor --------- //
        _cold ~Manager();
};

} // namespace end

#endif /* CLUSTER_MANAGER_H */
