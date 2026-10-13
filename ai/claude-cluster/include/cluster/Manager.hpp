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
    #include "Machine.hpp"          // cluster::MachineProgress
    #include "Session.hpp"          // cluster::Session, cluster::Event
    #include "Config.hpp"           // cluster::Config
    #include "Types.hpp"            // cluster::Snapshot, cluster::Task
    #include "Auth.hpp"             // cluster::Auth
    #include <functional>           // std::function
    #include <optional>             // std::optional
    #include <memory>               // std::shared_ptr
    #include <atomic>               // std::atomic
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <mutex>                // std::mutex
    #include <deque>                // std::deque
    #include <map>                  // std::map

    //----------------------------------------------------------------//
    /* DEFINE */

    #define GLOBAL_ID "g" // id of the global session

namespace cluster { // namespace start
class Web;
//----------------------------------------------------------------//
/* STRUCT */

struct SpawnRequest {
    std::string cwd;
    std::string name;
    std::string backend;        // empty: config / profile
    std::string mode;           // empty: config / profile
    std::string profile;
    std::string prompt;         // first prompt (optional)
    std::string effort;         // empty: config
    std::string resume{};       // the id of a conversation of the agent to go on with (claude --resume...)
    bool fork = false;          // claude: resumed as a copy (--fork-session), the original untouched
    std::vector<cluster::Entry> history{};  // its transcript so far, shown in the session
    std::string note{};         // a line of the system after it (where it comes from)
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
        std::int64_t _globalRestart = 0;                // last time the global session was started again (its process ended)
        std::atomic<bool> _closing{false};              // claude-cluster quits: the agents ending are not an error
        std::map<std::string, int> _replies;            // sessions whose next turn ends are sent back to the global session
        bool _globalNew = false;                        // the global session starts a new conversation (marked in its transcript)
        std::atomic<std::uint64_t> _focusVersion{0};
        std::string _active = "g";                      // the session they show
        std::vector<cluster::Task> _tasks;
        std::vector<cluster::Schedule> _schedules;      // things to do at a time (kept in the state)
        int _nextSchedule = 1;
        std::deque<cluster::Notice> _notices;
        std::map<std::string, std::string> _ui;         // layout / style chosen live (state.json)
        int _next = 1;
        int _nextTask = 1;
        std::atomic<std::uint64_t> _version{1};
        std::atomic<std::uint64_t> _machineRequest{0};  // the global session asked to map this computer again (the UI asks)
        std::atomic<bool> _restartAfterTurn{false};     // its system prompt changed by itself: started again after its turn
        std::atomic<int> _focused{-1};                  // the interface has the focus: 1, not: 0, unknown: -1 (notified)
        cluster::Web* _web = nullptr;                   // the web access of this instance (owned by the App)
        std::vector<cluster::UsageRecord> _usage;       // the usage ledger (usage.jsonl), in time order
        std::map<std::string, cluster::UsageReport> _limits; // per vendor: the last limits seen (5 h / 7 d windows)

        // ---------- Pre-Function -------- //
        _cold _nodiscard std::shared_ptr<cluster::Session> get_(const std::string& id) const;
        _cold _nodiscard std::shared_ptr<cluster::Session> make_(cluster::SessionSpec spec);
        _cold void event_(const cluster::Event& event);
        _cold void notice_(const std::string& session, const std::string& text, const bool error = false);
        _cold _nodiscard std::vector<std::string> globalArgs_(void) const;
        _cold _nodiscard std::filesystem::path globalDir_(void) const;     // global.cwd, empty: <data>/global
        _cold void writeMcpConfig_(void) const;
        _cold void fireSchedules_(void);                // the schedules due: done, then again (every) or removed
        _cold void schedule_(void);                         // Manager-Tasks.cpp
        _cold void taskDone_(const std::string& session, const bool error);
        _cold void loadUsage_(void);                        // Manager-Usage.cpp
        _cold void recordUsage_(const std::string& session); // a turn finished: in the ledger, its limits kept
        void archive_(const std::string& id);               // its transcript to logs/archive/<id>-<time>.jsonl (search only)
        _cold void relaunch_(const std::string& id, const cluster::SessionSpec& spec); // started again with this spec (same conversation)

    public:
        std::function<void(const std::string&)> onGlobalAnswer;   // voice: text of an answer of the global session
        std::function<void(const cluster::Notice&)> onNotice;

        // ---------- Pre-Function -------- //
        _cold void load(void);                              // state.json: trash, tasks, previous sessions, ui choices

        /* usage (Manager-Usage.cpp) */
        _cold _nodiscard std::string vendorOf(const std::string& backend) const;   // its provider ("" unknown)
        _cold _nodiscard cluster::UsageReport usage(const std::string& vendor) const;
        _cold _nodiscard std::vector<std::string> usageVendors(void) const;         // the ones in the ledger
        _cold _nodiscard std::vector<std::pair<std::string, std::string>> usageLines(const cluster::Snapshot& snap) const; // panel "usage"
        _cold _nodiscard cluster::Json usageJson(const std::string& vendor) const;  // every vendor when empty
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
            const std::vector<std::string>& images = {});               // not in the history: the prompts typed by the user only (addHistory)
        _cold void clear(const std::string& id);                         // /clear: a new conversation, the transcript archived (empty)
        _cold void setModel(const std::string& id, const std::string& model);   // /model: the session again on this model (same conversation)
        _cold void setEffort(const std::string& id, const std::string& effort); // /effort: thinking / reasoning (default, low... max)
        _cold void close(const std::string& id);
        _cold _nodiscard std::string restore(const std::string& id);
        _cold void purge(const std::string& id = "");       // delete for good from the trash (empty: the expired ones, "*": all)
        _cold void rename(const std::string& id, const std::string& name);
        _cold void setMode(const std::string& id, const std::string& mode);
        _cold _nodiscard cluster::Queued rewind(const std::string& id); // the last turn removed, its prompt back (Esc Esc)
        _cold void cd(const std::string& id, const std::string& cwd);
        _cold void togglePanel(const std::string& id, const std::string& panel);
        _cold void setRemote(const bool on);

        /* tasks (Manager-Tasks.cpp) */
        _cold _nodiscard std::string addTask(const std::string& prompt, const std::string& target, const std::vector<std::string>& after);
        _cold _nodiscard std::string addSchedule(const std::string& when, const std::string& action, const std::string& session, const std::string& text, const int every);
        _cold _nodiscard std::vector<cluster::Schedule> schedules(void) const;
        _cold void cancelSchedule(const std::string& id);
        _cold void cancelTask(const std::string& id);
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
        _cold _nodiscard cluster::Json toJson(const cluster::Snapshot& snapshot, const bool full) const;
        _cold void addHistory(const cluster::Draft& draft, const std::string& session); // prompts sent (history.jsonl)
        _cold _nodiscard std::vector<cluster::Draft> drafts(const std::size_t max = 500, const std::string& session = "") const; // newest first, no
                                                            // duplicate; a session: its prompts only (the arrows), none: every one (Ctrl+R)
        _cold _nodiscard std::vector<std::string> history(const std::size_t max = 500, const std::string& session = "") const; // their texts
        _cold void stashPush(const cluster::Draft& draft);                              // prompt put aside (stash.json)
        _cold _nodiscard std::optional<cluster::Draft> stashPop(void);
        _cold _nodiscard std::size_t stashSize(void) const;

        /* state */
        _nodiscard std::vector<cluster::Snapshot> list(const bool withGlobal = true) const;
        _nodiscard std::optional<cluster::Snapshot> snapshot(const std::string& id) const;
        _nodiscard std::vector<cluster::SessionSpec> trash(void) const;
        _nodiscard std::vector<cluster::SessionSpec> previous(void) const;
        _nodiscard std::vector<cluster::Notice> notices(const std::size_t max = 50) const;
        _nodiscard std::string resolve(const std::string& idOrName) const;   // id from an id or a name
        _nodiscard std::uint64_t version(void) const;
        _nodiscard std::string ui(const std::string& key, const std::string& fallback) const;
        void setUi(const std::string& key, const std::string& value);
        void requestFocus(const std::string& id);           // the interfaces show this session (the global session asks it)
        void replyTo(const std::string& id);                // the end of the next turn of this session sent to the global session
        bool notifyDesktop(const std::string& title, const std::string& text, const bool urgent = false) const; // false: notifications off
        _cold void scanMachine(void);                       // the map of this computer written again (in the background, a notice)
        _nodiscard int machineAsk(void) const;              // asked at start: 0 no, 1 no map yet (not refused), 2 an old map (sync proposed)
        _nodiscard std::optional<std::pair<cluster::MachineProgress, std::int64_t>> machineProgress(void) const; // the last scan, ms of its end (0: running)
        _cold void restartGlobal(void);                     // its system prompt read again, the same conversation

        // ---------- Function -------- //
        _cold inline void notice(const std::string& session, const std::string& text, const bool error = false) {this->notice_(session, text, error);}; // the notices of the interfaces
        inline void requestMachineSync(void) {++this->_machineRequest; ++this->_version;};  // the interfaces ask the user (its popup)
        _nodiscard inline std::uint64_t machineRequest(void) const {return this->_machineRequest;};
        inline void restartGlobalAfterTurn(void) {this->_restartAfterTurn = true;};          // after the turn running (it asked it)
        inline void setFocused(const bool focused) {this->_focused = focused ? 1 : 0;}; // the window / terminal has the focus
        inline void machineLater(void) {this->setUi("machine_later", std::to_string(cluster::now()));}; // an old map: proposed again machine_days later
        inline void machineDecline(void) {this->setUi("machine", "no");};
        _nodiscard inline std::pair<std::uint64_t, std::string> focusRequest(void) const {return {this->_focusVersion, this->_focus};};
        inline void noteActive(const std::string& id) {this->_active = id;};   // the session shown (ui_state)
        _nodiscard inline std::string active(void) const {return this->_active;};
        _nodiscard inline cluster::Auth& auth(void) {return this->_auth;};
        _nodiscard inline cluster::Config& config(void) {return this->_config;};
        _nodiscard inline bool remote(void) const {return this->ui("remote", "false") == "true";}; // Remote Control of the global session
        inline void setWeb(cluster::Web* web) {this->_web = web;};
        _nodiscard inline cluster::Web* web(void) const {return this->_web;};
        _cold inline void answer(const std::string& id, const std::string& requestId, const std::string& behavior) {this->get_(id)->answer(requestId, behavior);};
        _cold inline void answerQuestions(const std::string& id, const std::string& requestId, const cluster::Json& answers) {this->get_(id)->answerQuestions(requestId, answers);};
        _cold inline void interrupt(const std::string& id) {this->get_(id)->interrupt();};
        _cold _nodiscard inline std::optional<cluster::Queued> unqueue(const std::string& id) {return this->get_(id)->unqueue();}; // last queued prompt, removed

        // ---------- Constructor -------- //
        _cold Manager(cluster::Config& config);

        // ----------- Destructor --------- //
        _cold ~Manager();
};

} // namespace end

#endif /* CLUSTER_MANAGER_H */
