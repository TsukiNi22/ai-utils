/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file External.hpp

File Description:
##  The conversations of the agents of this computer opened outside
##  claude-cluster (Claude Code, Codex, opencode, Qwen Code): listed,
##  their transcript read, opened again as a session (resumed)
\**************************************************************/

#ifndef EXTERNAL_H
    #define EXTERNAL_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Entry, cluster::Json
    #include <filesystem>           // std::filesystem::path
    #include <optional>             // std::optional
    #include <cstdint>              // std::int64_t
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Conversation {
    std::string agent;              // claude | codex | opencode | qwen
    std::string id;                 // its id for the agent (claude --resume <id>)
    std::string title;              // its title (Claude Code: its own), else the first prompt
    std::string cwd;
    std::string first;              // the first prompt of the user
    std::int64_t updated = 0;       // unix seconds
    std::uintmax_t size = 0;        // bytes of its transcript
    std::filesystem::path path;     // its transcript (empty: opencode, in its own database)
};

//----------------------------------------------------------------//
/* FUNCTION */

// the conversations of an agent (empty: every one), the newest first, the ones a session of claude-cluster runs left out
_cold _nodiscard std::vector<cluster::Conversation> external_conversations(const std::string& agent, const std::size_t limit,
    const std::vector<std::string>& known = {});

// one of them by its id (a prefix is enough when it is unique)
_cold _nodiscard std::optional<cluster::Conversation> external_conversation(const std::string& agent, const std::string& id);

// its transcript as entries of claude-cluster (the active branch of Claude Code), the last max ones
_cold _nodiscard std::vector<cluster::Entry> external_entries(const cluster::Conversation& conversation, const std::size_t max);

// the backend of a session of claude-cluster resuming it (claude, codex, opencode, a provider of Qwen Code)
_cold _nodiscard std::string external_backend(const std::string& agent);

// as JSON (listing, MCP)
_cold _nodiscard cluster::Json external_json(const cluster::Conversation& conversation);

} // namespace end

#endif /* EXTERNAL_H */
