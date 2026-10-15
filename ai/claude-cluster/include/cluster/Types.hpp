/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Types.hpp

File Description:
##  Data shared by the core and the front-ends: state, metrics,
##  transcript, permissions, tasks of the sessions
\**************************************************************/

#ifndef CLUSTER_TYPES_H
    #define CLUSTER_TYPES_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _nodiscard
    #include <nlohmann/json.hpp>    // nlohmann::json
    #include <cstdint>              // std::int64_t
    #include <utility>              // std::pair
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <set>                  // std::set
    #include <map>                  // std::map

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* TYPEDEF */

using Json = nlohmann::json;

//----------------------------------------------------------------//
/* ENUM */

enum class State {
    Starting,   // process started, no init message yet
    Idle,       // waiting for a prompt
    Working,    // a turn is running
    Waiting,    // a permission is pending
    Remote,     // handed to Remote Control (global session only)
    Error,      // the process died / a turn failed
    Stopped,    // closed
};

enum class EntryKind {
    User,
    Assistant,
    Thinking,
    Tool,
    ToolResult,
    System,
    Error,
    Display,    // a rich display put in by the global session (MCP session_display): its JSON (Display.hpp)
};

//----------------------------------------------------------------//
/* STRUCT */

struct Usage {
    std::int64_t input = 0;
    std::int64_t output = 0;
    std::int64_t cacheRead = 0;
    std::int64_t cacheCreation = 0;

    _nodiscard std::int64_t total(void) const {return input + output + cacheRead + cacheCreation;};
    _nodiscard std::int64_t context(void) const {return input + cacheRead + cacheCreation + output;};
};

struct Metrics {
    cluster::Usage last;            // last finished turn (kept when finished)
    cluster::Usage current;         // running turn, live
    cluster::Usage total;           // since the start of the session
    std::int64_t contextUsed = 0;   // tokens of the last request (input + cache + output)
    std::int64_t contextWindow = 200000;
    double cost = 0.0;              // USD since the start
    double lastCost = 0.0;          // USD of the last turn
    bool costKnown = true;          // false: local / non Anthropic provider (the cost of claude is a guess)
    double limit5h = -1.0;          // utilization of the 5 hour window (0..1, -1 unknown)
    double limit7d = -1.0;          // utilization of the 7 day window
    std::int64_t reset5h = 0;       // unix seconds of the reset of the 5 hour window (0 unknown)
    std::int64_t reset7d = 0;
    int turns = 0;
};

// A finished turn in the usage ledger (usage.jsonl): what a vendor was used for, over every session and run
struct UsageRecord {
    std::int64_t time = 0;          // unix seconds
    std::string vendor;             // provider (anthropic, ollama, openai, opencode...)
    std::string model;
    std::string session;
    std::int64_t tokens = 0;        // new tokens: input + written to the cache + output
    std::int64_t cache = 0;         // read again from the cache
    double cost = 0.0;              // USD (0: local / unknown)
    bool costKnown = true;
};

struct UsageSum {
    std::int64_t tokens = 0;
    std::int64_t cache = 0;
    double cost = 0.0;
    bool costKnown = true;          // false: some turns of a local / unknown price
    int turns = 0;
};

// The usage of a vendor: today (since midnight), the last 7 days, its whole life; its limits when it tells them
struct UsageReport {
    std::string vendor;
    cluster::UsageSum today;
    cluster::UsageSum week;
    cluster::UsageSum total;
    std::int64_t since = 0;         // first turn of the ledger
    double limit5h = -1.0;          // utilization 0..1 (-1 unknown), the last one seen in one of its sessions
    double limit7d = -1.0;
    std::int64_t reset5h = 0;
    std::int64_t reset7d = 0;
};

// A prompt as written: its text (images as [Image #N] in the order of the text, pastes in full), its images, its pasted
// texts (blocks again when it comes back)
struct Draft {
    std::string text;
    std::vector<std::string> images;
    std::vector<std::string> pastes;
};

struct Entry {
    cluster::EntryKind kind = cluster::EntryKind::System;
    std::string text;
    std::string tool;               // Tool / ToolResult: tool name
    std::int64_t time = 0;          // unix seconds
};

struct Permission {
    std::string requestId;
    std::string tool;
    std::string description;
    cluster::Json input;
    cluster::Json suggestions;      // permission_suggestions of claude ("always")
    std::int64_t time = 0;
    std::string reason{};           // why claude asks (decision_reason: its safety check...)
    bool safety = false;            // not a request: a command the safety check stopped without asking (auto mode); allowed, it runs again
};

struct ToolRun {
    std::string id;
    std::string name;
    std::string summary;            // command / file / pattern
    bool done = false;
    bool error = false;
    std::int64_t time = 0;
};

// Description of a session, saved in state.json (restore / trash)
struct SessionSpec {
    std::string id;                 // short id (s1, s2...), stable
    std::string name;               // renamable
    std::string cwd;
    std::string backend = "claude"; // <provider>[/<model>]
    std::string claudeId;           // session id of the agent (--resume / -r / -s)
    std::string leaf;               // claude: uuid of the last message of the conversation (its active branch)
    std::string resumeAt;           // claude: after a rewind, the message the conversation goes on from (until a prompt)
    bool fork = false;              // claude: the next start resumes claudeId as a copy (--fork-session), the original untouched
    std::string model;              // model given to the agent (from the backend)
    std::string mode = "default";   // permission mode
    std::string effort;             // thinking / reasoning effort (empty: the default of the agent)
    std::string profile;
    std::vector<std::string> extraArgs;
    std::set<std::string> panels;
    bool global = false;
    std::int64_t created = 0;
    std::int64_t deleted = 0;       // trash: time of the close
    double cost = 0.0;              // kept over the runs (USD)
    std::int64_t tokens = 0;        // kept over the runs
    std::string summary;            // a short description (what it works on, holds): given, or written by claude-cluster
    bool summaryAuto = true;        // written by claude-cluster (false: given by the user or the global session, kept)
    std::int64_t summaryTurns = 0;  // its turns when it was written (written again after more)
    std::int64_t turns = 0;         // its turns finished
};

struct Task {
    std::string id;                 // t1, t2...
    std::string prompt;
    std::string target;             // session id, project folder, or empty (any free session)
    std::vector<std::string> after; // ids of the tasks that must be done first
    std::string status = "queued";  // queued | running | done | failed | cancelled
    std::string session;            // session running it
    std::int64_t created = 0;
    std::string group;              // its group (session_fanout of the global session), "" none
    std::string label;              // its target told to the global session (the session, the project)
    std::string answer;             // its answer (the end of its turn)
    int retries = 0;                // tried again after an error
    std::int64_t started = 0;
    std::int64_t ended = 0;
    std::string by;                 // who queued it: a session (g: the global one), empty: you
    std::string reason;             // cancelled / deleted: why (told to the session that made it)
    bool archived = false;          // out of the list (its archive)
    bool forced = false;            // started without waiting for a free place
};

// A request of the global session on several sessions / projects: its tasks, how long it waits for them, their merge
struct Group {
    std::string id;                 // f1, f2...
    std::string label;
    std::string wait = "all";       // all | first | count: the global session woken when all / the first / count of them are done
    int count = 1;
    std::int64_t deadline = 0;      // woken then with what came (0: none)
    bool stopOthers = false;        // its wait met: the others stopped
    bool retry = true;              // a task failing: tried once more
    std::string merge = "here";     // here: merged by the global session | session: by a session of its own
    bool table = false;             // a comparison table asked too
    std::vector<std::string> tasks;
    std::set<std::string> told;     // tasks whose answer was given to the global session
    bool woken = false;
    bool finished = false;
    std::string card;               // its progress card (a display of the global session)
    std::int64_t created = 0;
};

// Something to do at a time: a prompt to a session (the global one by default) or a notification, repeated or not
struct Schedule {
    std::string id;                 // w1, w2...
    std::int64_t at = 0;            // next time (unix seconds)
    int every = 0;                  // minutes between two (0: once)
    std::string action = "prompt";  // prompt | notify
    std::string session = "g";
    std::string text;
    std::string by;                 // who made it: a session (g: the global one), empty: you
    std::string status = "waiting"; // waiting | done (once, it ran) | cancelled
    std::string reason;             // cancelled: why
    bool archived = false;
    int runs = 0;                   // times it ran
    std::int64_t last = 0;          // its last run
};

// What failed of what the user did (a request, a command, a prompt): its popup, OK / Retry
struct Failure {
    std::string id;                 // c<n>: of the core, l<n>: of this interface
    std::string action;             // what the user did
    std::string source;             // where it failed (the session, the router, a program...)
    std::string error;
    bool retry = false;             // Retry offered
    std::int64_t at = 0;
};

// A long operation shown in a popup (an install, the sessions restored...): what it does now, its last lines
struct Progress {
    std::uint64_t id = 0;
    std::string title;
    std::string session;            // its session (the notices of it under it), empty: claude-cluster
    std::string now;                // what it does now (its end: the result)
    std::vector<std::string> log;   // its last lines (greyed under it)
    bool done = false;
    bool ok = true;
    std::int64_t started = 0;       // ms
    std::int64_t ended = 0;         // ms
};

// A todo of a session (its TodoWrite: what it does, what is left)
struct Todo {
    std::string id;                 // TaskCreate: its number (TodoWrite: none)
    std::string content;
    std::string status;             // pending | in_progress | completed
    std::string activeForm;
};

// Copy of a session for the front-ends (taken under the lock of the session)
struct Snapshot {
    cluster::SessionSpec spec;
    cluster::State state = cluster::State::Starting;
    bool stopped = false;               // its last turn ended without its answer (interrupted, an error): to continue
    cluster::Metrics metrics;
    std::vector<cluster::Entry> entries;
    std::vector<cluster::Permission> permissions;
    std::vector<cluster::ToolRun> tools;
    std::vector<std::string> skillsLoaded;
    std::vector<std::string> skillsAvailable;
    std::vector<std::string> slashCommands;     // commands of the agent (/compact, skills...)
    std::map<std::string, std::string> commandHelp;  // their description (the answer of initialize)
    std::vector<std::string> files;     // files modified by Edit / Write / NotebookEdit
    std::string partial;                // text being streamed
    std::string thinking;               // thinking being streamed (cleared by the final block)
    std::string step;                   // what the turn does now: thinking | writing | tools | model (awaited) | a tool being written
    std::int64_t stepSince = 0;         // its start (ms)
    std::string stepDetail;             // what it waits on: the request (its context), the API retried (its error), a hook...
    std::int64_t turnSince = 0;         // the start of the turn (ms)
    std::size_t streamed = 0;           // characters streamed this turn (text, thinking, tool inputs): ~4 per token
    std::int64_t thinkingTokens = 0;    // thinking tokens of the turn (its text may be hidden by the model)
    std::vector<std::pair<std::string, std::int64_t>> steps;   // the steps done this turn, their duration (ms)
    std::string modelUsed;              // from the init message
    int pid = 0;                        // its process (the agent), 0: not running
    std::string lastError;
    std::string remoteUrl;              // Remote Control on: the session on claude.ai
    std::string bridgeState;            // Remote Control: ready | connected | disconnected... (empty: off)
    int queued = 0;                     // prompts waiting for the end of the running turn
    std::vector<std::string> queue;     // their text (Esc in the prompt: the last one back to edit)
    std::uint64_t version = 0;          // its version when taken (an interface of another process: sent again when it changed)
    std::string warning;                // a problem of its interfaces (two prompts at once...), shown a minute on it
    std::int64_t warningAt = 0;
    std::vector<cluster::Todo> todos;   // its todo list now (its last TodoWrite)
};

//----------------------------------------------------------------//
/* FUNCTION */

_nodiscard const char* state_name(cluster::State state);

// Permission modes of Claude Code (2.1): <mode, description>; "default" is the old name of "manual"
_nodiscard inline const std::vector<std::pair<std::string, std::string>>& permission_modes(void) {static const std::vector<std::pair<std::string, std::string>> modes = {{"auto", "the classifier allows the safe actions (default of claude-cluster)"}, {"manual", "asks for every sensitive tool"}, {"default", "same as manual"}, {"acceptEdits", "file edits allowed, the rest asked"}, {"plan", "read only, plans"}, {"dontAsk", "never asks: what is not allowed is refused"}, {"bypassPermissions", "everything allowed"}}; return modes;};

} // namespace end

#endif /* CLUSTER_TYPES_H */
