/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Rewind.hpp

File Description:
##  The points of a conversation it can be taken back to: its
##  prompts, newest first, each with what going back to it drops
\**************************************************************/

#ifndef CLUSTER_REWIND_H
    #define CLUSTER_REWIND_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Entry
    #include <cstdint>              // std::int64_t
    #include <cstddef>              // std::size_t
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct RewindPoint {
    int back = 1;                   // prompts to take back (1: the last one): the argument of Manager::rewind
    std::int64_t time = 0;          // unix seconds of the prompt
    std::string label;              // its first line
    std::size_t messages = 0;       // entries of the conversation going back to it drops (the prompt included)
    std::size_t prompts = 0;        // of them, the prompts
    std::size_t tools = 0;          // tool calls of the answers it dropped
    std::size_t answerChars = 0;    // size of the answer to that prompt
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard std::vector<cluster::RewindPoint> rewind_points(const std::vector<cluster::Entry>& entries);   // newest first

} // namespace end

#endif /* CLUSTER_REWIND_H */
