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
    #include <algorithm>            // std::any_of
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

// a dump never throws on a text cut inside a character (a bad byte becomes U+FFFD): one bad string must not kill the core
inline constexpr Json::error_handler_t JSON_LENIENT = Json::error_handler_t::replace;

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
    std::int64_t cacheCreation = 0;     // written to the cache (5 min and 1 h)
    std::int64_t cacheCreation1h = 0;   // the part kept 1 h (priced 2x, the 5 min one 1.25x)
    std::int64_t thinking = 0;          // of the output

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
    std::int64_t input = 0;         // its breakdown (0 on the records of older runs)
    std::int64_t cacheWrite = 0;
    std::int64_t cacheWrite1h = 0;
    std::int64_t cacheRead = 0;
    std::int64_t output = 0;
    std::int64_t thinking = 0;
    std::int64_t durMs = 0;         // how long the turn lasted (0: unknown)
};

struct UsageSum {
    std::int64_t tokens = 0;
    std::int64_t cache = 0;
    double cost = 0.0;
    bool costKnown = true;          // false: some turns of a local / unknown price
    int turns = 0;
    std::int64_t input = 0;         // its breakdown
    std::int64_t cacheWrite = 0;
    std::int64_t cacheWrite1h = 0;
    std::int64_t cacheRead = 0;
    std::int64_t output = 0;
    std::int64_t thinking = 0;
};

// A window of the account (5 h, 7 d, 7 d of a model, the extra usage): what is used of it, when it resets
struct UsageWindow {
    std::string name;               // "5 h", "7 d", "7 d opus", "7 d sonnet", "extra usage"
    double used = -1.0;             // 0..1 (-1 unknown)
    std::int64_t reset = 0;         // unix seconds (0 unknown)
    double usedDollars = -1.0;      // the extra usage: credits used / its limit (-1: none)
    double limitDollars = -1.0;
    std::string locked;             // why it is locked (empty: not)
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
    std::vector<cluster::UsageWindow> windows;  // the account (the OAuth usage endpoint): every window it tells
    std::int64_t accountAt = 0;     // unix seconds of the last read of the account (0: never)
    std::string source;             // where the sums come from
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
    std::string origin{};           // User: where the prompt came from (user | mcp | task | schedule), empty: the user / an old transcript (cluster::prompt_origin)
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
    std::string image;              // a picture it read or made (a screenshot): its path, opened from the rounds of the turn
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
    std::string category;           // its category (one, lower case: bugfix, docs...), set by the global session or the user
    std::vector<std::string> tags;  // its labels (several, lower case), same
    std::int64_t turns = 0;         // its turns finished
    cluster::Json pin;              // the pinned item (null: none): {kind: display | message, id, title, by, at, role, text}
};

// A shell command checked by a trigger (on_event command of a task, if.command of a schedule): what it must print or return, how
// often it is run, when the trigger gives up
struct CommandMatch {
    std::string out;                // its output (trimmed) must be exactly this ("": not checked)
    std::string contains;           // its output must hold this
    std::string regex;              // its output must match this (searched, ECMAScript)
    int code = 0;                   // the exit code wanted
    bool codeAsked = false;         // a code asked: another code is "not yet", never a failure; else a code other than 0 is a failure
    int every = 30;                 // seconds between two checks of a task trigger (2 at least: "constantly")
    int failAfter = 0;              // failures in a row before the trigger gives up (0: never; 1: at the first)
    int failures = 0;               // failures in a row so far
    std::string last;               // what the last check printed / why it failed
};

// What a task waits for besides its dependencies (task_add on_event): a session idle, a commit pushed, a command (its output / code), a file
struct TaskEvent {
    std::string type;               // "" none | session_idle | git_push | command | file
    std::string arg;                // the session | the repository | the command | the file
    std::string arg2;               // git_push: the branch (default: the current one); file: "changed" (its modification, not only its presence)
    std::string base;               // what it was when the task was queued (the commit, the modification time)
    std::int64_t checked = 0;       // last check (unix seconds)
    bool met = false;
    cluster::CommandMatch match;    // command: what it must print / return, its interval, its failures
};

// A task that makes its own session (the options of session_spawn) instead of looking for a free one
struct TaskSpawn {
    std::string cwd;                // "" none
    std::string name;
    std::string backend;
    std::string mode;
    std::string effort;
    std::string profile;
    bool oneShot = false;           // its session closed to the trash (restorable) when the task is over
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
    std::vector<std::string> afterSessions; // sessions that must be idle first
    cluster::TaskEvent on;          // the event it waits for
    cluster::TaskSpawn spawn;       // its own session
    bool spawned = false;           // its session was made by it (counted in the cap of its author)
    int priority = 0;               // higher first (default 0)
    int timeout = 0;                // minutes of running, then stopped (0: none)
    int retry = 0;                  // tried again this many times after an error
    int retryDelay = 0;             // minutes before a new try
    std::int64_t notBefore = 0;     // not before this time (a retry waiting)
    int maxConcurrency = 0;         // at most this many tasks of its lane running (0: no limit)
    std::string lane;               // the group the limit counts in ("" the default one)
    double maxCost = 0.0;           // USD of its session while it runs, then stopped (0: none)
    std::int64_t maxTokens = 0;     // tokens, same
    double costBase = 0.0;          // the session at its start
    std::int64_t tokensBase = 0;
    std::string result;             // its answer kept (size capped), the {{tN.result}} of the next prompts
    std::string expect;             // the output asked: compact | a file path
    bool notify = false;            // a notification when it ends
    bool quiet = false;             // its author is not told when it ends
    std::string schedule;           // the schedule that queued it
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

// A run of a schedule (its history)
struct ScheduleRun {
    std::int64_t at = 0;
    std::string note;               // what it did, or why it was skipped
};

// What a schedule checks before it runs (all that are set must hold; not met: the run is skipped)
struct ScheduleIf {
    std::string sessionIdle;        // a session (id / name) idle
    std::string command;            // a shell command: exiting 0, or what match asks
    cluster::CommandMatch match;    // its output / code wanted, its failures before the schedule gives up
    std::string fileChanged;        // a file modified since the last run (or since it was made)
    std::int64_t fileBase = 0;      // its modification time then
    std::string usage;              // "5h>80", "7d<30": a limit window used above / below a percent
};

// Something to do at a time: a prompt to a session (the global one by default), a notification, a session, a task or a command
// without any model; once or repeated (every N minutes, a cron expression), inside its days / windows, under conditions
struct Schedule {
    std::string id;                 // w1, w2...
    std::int64_t at = 0;            // next time (unix seconds)
    int every = 0;                  // minutes between two (0: once)
    std::string action = "prompt";  // prompt | notify | spawn | task | command
    std::string session = "g";
    std::string text;
    std::string by;                 // who made it: a session (g: the global one), empty: you
    std::string status = "waiting"; // waiting | paused | done (ended) | cancelled
    std::string reason;             // cancelled / ended: why
    bool archived = false;
    int runs = 0;                   // times it ran
    std::int64_t last = 0;          // its last run
    std::string cron;               // "min hour day month weekday" (instead of every)
    std::string days;               // "mon-fri": only these days
    std::string window;             // "09:00-18:00": only inside
    std::string quiet;              // "22:00-07:00": never inside
    std::string tz;                 // the zone of the above ("" this computer's)
    std::int64_t until = 0;         // ended after this time (0: none)
    int maxRuns = 0;                // ended after this many runs (0: none)
    std::string reset;              // "5h" | "7d": at each reset of this limit window (instead of when / every)
    cluster::ScheduleIf cond;
    cluster::Json options = cluster::Json::object();    // spawn / task: the options of the session / of the task; command: {command, output: prompt | notify, cwd}
    bool ack = false;               // asked again until acknowledged (schedule_act ack)
    int nag = 10;                   // minutes between two asks
    int skipped = 0;                // runs skipped (outside a window, a condition not met)
    std::string note;               // what its last pass did (ran, skipped and why)
    std::vector<cluster::ScheduleRun> history;      // its last runs (10)
    bool pending = false;           // ack: ran, asked again until acknowledged
    int asks = 0;                   // ack: times asked in this round
    bool polling = false;           // reset: waiting for the new reset time (the usage not read yet)
    bool busy = false;              // being run (not kept)
    bool now = false;               // run now asked (not kept)
};

// What failed of what the user did (a request, a command, a prompt): its popup, OK / Retry
struct Failure {
    std::string id;                 // c<n>: of the core, l<n>: of this interface
    std::string action;             // what the user did
    std::string source;             // where it failed (the session, the router, a program...)
    std::string error;
    bool retry = false;             // Retry offered
    std::int64_t at = 0;
    std::string other;              // a third choice offered (its label: "Use port 8791"), empty: none
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

// What a session runs in the background itself: an agent (its Agent tool: a sub-agent of claude, not a sub-session of claude-cluster) or
// a shell (a command run in the background: a script, a server, a watcher)
struct SubAgent {
    std::string kind = "agent";     // agent | shell
    std::string taskId;             // the task of claude (its stop_task), empty: not told yet (an older claude)
    std::string toolUseId;          // the Agent call that made it (its messages carry it as parent_tool_use_id)
    std::string description;
    std::string type;               // its kind (general-purpose, Explore, a custom one...)
    std::string prompt;             // what it was asked (cut)
    std::string model;              // the model of its messages (empty: not seen yet)
    std::string effort;             // the thinking of its session (an agent inherits it)
    std::string status = "running"; // running | done | failed | stopped
    std::string summary;            // what it says it does / did
    std::string lastTool;
    std::string error;
    std::string outputFile;         // a shell: the file of its output (its last lines shown)
    bool background = false;        // run in the background (its session goes on meanwhile)
    std::string label;              // safe | dangerous (cluster::command_label of what it runs, at its start): an update stops a young safe one, waits for the rest
    bool stopping = false;          // a stop asked, not confirmed yet
    std::int64_t started = 0;       // ms
    std::int64_t ended = 0;         // ms, 0: running
    std::int64_t tokens = 0;
    int toolUses = 0;
    std::vector<cluster::Entry> entries;    // its activity: what it says, its tools and their results (the last ones)
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
    int queueGiven = 0;                 // the first ones already given to the agent (taken at its next step): not editable
    std::uint64_t version = 0;          // its version when taken (an interface of another process: sent again when it changed)
    std::string warning;                // a problem of its interfaces (two prompts at once...), shown a minute on it
    std::int64_t warningAt = 0;
    std::size_t older = 0;              // entries of its log before the first one here (memory kept by size: loaded again on demand, cmd older)
    std::vector<cluster::Todo> todos;   // its todo list now (its last TodoWrite)
    std::vector<cluster::SubAgent> agents;  // the agents it made (its Agent tool), the last 40
};

//----------------------------------------------------------------//
/* FUNCTION */

_nodiscard const char* state_name(cluster::State state);

// its turn over, but shells / tasks / agents of it still running in the background (shown purple, ~_~): idle only once all of them ended
_nodiscard inline bool background_busy(const cluster::Snapshot& s) {return s.state == cluster::State::Idle && s.permissions.empty() && std::any_of(s.agents.begin(), s.agents.end(), [](const cluster::SubAgent& a) {return a.status == "running";});};

// The labels of a session: "a b c" (the tags, to edit) and "bugfix · #a #b" (its category and tags, to show)
_nodiscard inline std::string join_labels(const std::vector<std::string>& tags) {std::string out; for (const std::string& tag: tags) out += (out.empty() ? "" : " ") + tag; return out;};
_nodiscard inline std::string label_text(const cluster::SessionSpec& spec) {std::string out = spec.category; for (const std::string& tag: spec.tags) out += (out.empty() ? "#" : " #") + tag; return out;};

// Permission modes of Claude Code (2.1): <mode, description>; "default" is the old name of "manual"
_nodiscard inline const std::vector<std::pair<std::string, std::string>>& permission_modes(void) {static const std::vector<std::pair<std::string, std::string>> modes = {{"auto", "the classifier allows the safe actions (default of claude-cluster)"}, {"manual", "asks for every sensitive tool"}, {"default", "same as manual"}, {"acceptEdits", "file edits allowed, the rest asked"}, {"plan", "read only, plans"}, {"dontAsk", "never asks: what is not allowed is refused"}, {"bypassPermissions", "everything allowed"}}; return modes;};

} // namespace end

#endif /* CLUSTER_TYPES_H */
