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
    int turns = 0;
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
    std::string model;              // model given to the agent (from the backend)
    std::string mode = "default";   // permission mode
    std::string profile;
    std::vector<std::string> extraArgs;
    std::set<std::string> panels;
    bool global = false;
    std::int64_t created = 0;
    std::int64_t deleted = 0;       // trash: time of the close
    double cost = 0.0;              // kept over the runs (USD)
    std::int64_t tokens = 0;        // kept over the runs
};

struct Task {
    std::string id;                 // t1, t2...
    std::string prompt;
    std::string target;             // session id, project folder, or empty (any free session)
    std::vector<std::string> after; // ids of the tasks that must be done first
    std::string status = "queued";  // queued | running | done | failed | cancelled
    std::string session;            // session running it
    std::int64_t created = 0;
};

// Copy of a session for the front-ends (taken under the lock of the session)
struct Snapshot {
    cluster::SessionSpec spec;
    cluster::State state = cluster::State::Starting;
    cluster::Metrics metrics;
    std::vector<cluster::Entry> entries;
    std::vector<cluster::Permission> permissions;
    std::vector<cluster::ToolRun> tools;
    std::vector<std::string> skillsLoaded;
    std::vector<std::string> skillsAvailable;
    std::vector<std::string> files;     // files modified by Edit / Write / NotebookEdit
    std::string partial;                // text being streamed
    std::string modelUsed;              // from the init message
    std::string lastError;
    std::string remoteId;               // Remote Control: id of the background session
    int queued = 0;                     // prompts sent while working
};

//----------------------------------------------------------------//
/* FUNCTION */

_nodiscard const char* state_name(cluster::State state);

// Permission modes of Claude Code (2.1): <mode, description>; "default" is the old name of "manual"
_nodiscard inline const std::vector<std::pair<std::string, std::string>>& permission_modes(void) {static const std::vector<std::pair<std::string, std::string>> modes = {{"auto", "the classifier allows the safe actions (default of claude-cluster)"}, {"manual", "asks for every sensitive tool"}, {"default", "same as manual"}, {"acceptEdits", "file edits allowed, the rest asked"}, {"plan", "read only, plans"}, {"dontAsk", "never asks: what is not allowed is refused"}, {"bypassPermissions", "everything allowed"}}; return modes;};

} // namespace end

#endif /* CLUSTER_TYPES_H */
