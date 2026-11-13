/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file PromptStash.hpp

File Description:
##  The stash of prompts: the ones put aside (Ctrl+S), taken
##  back or rewound, kept in stash.json of the data folder
##  over the restarts, global (the session of origin is kept
##  with each one), bounded in number and in size
\**************************************************************/

#ifndef CLUSTER_PROMPTSTASH_H
    #define CLUSTER_PROMPTSTASH_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Draft
    #include <optional>             // std::optional
    #include <cstdint>              // std::int64_t
    #include <cstddef>              // std::size_t
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* CONSTANT */

constexpr std::size_t STASH_ENTRY_BYTES = 262144;      // one stashed prompt (its text, its pasted texts): more is refused
constexpr std::size_t STASH_TOTAL_BYTES = 4194304;     // the whole stash

//----------------------------------------------------------------//
/* ENUM */

enum class StashAdd {
    Added,
    Empty,      // nothing to stash
    Full,       // the maximum is reached (stash_full = refuse)
    TooBig,     // over the size limits
};

//----------------------------------------------------------------//
/* STRUCT */

struct StashEntry {
    cluster::Draft draft;
    std::int64_t time = 0;              // unix seconds
    std::string session;                // where it was written (empty: unknown, an older stash)
};

//----------------------------------------------------------------//
/* FUNCTION */

// oldest first, as stored
_cold _nodiscard std::vector<cluster::StashEntry> stash_read(void);
// force: an automatic copy (the prompt replaced by another one): never refused for the maximum; dropOldest: the oldest one makes room
_cold cluster::StashAdd stash_add(const cluster::Draft& draft, const std::string& session, const int max, const bool dropOldest, const bool force);
_cold _nodiscard std::optional<cluster::StashEntry> stash_remove(const std::size_t index);      // an index of stash_read
_cold void stash_insert(const std::size_t index, const cluster::StashEntry& entry);             // back where it was (undo)
_cold void stash_clear(void);
_cold _nodiscard std::string stash_label(const cluster::Draft& draft);                          // its first line, one line

} // namespace end

#endif /* CLUSTER_PROMPTSTASH_H */
