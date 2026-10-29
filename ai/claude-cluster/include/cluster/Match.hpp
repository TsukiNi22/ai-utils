/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Match.hpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef MATCH_H
    #define MATCH_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    #define _Attribute
    #include <utils/utils.hpp> // _cold, _nodiscard
    #include <optional>        // std::optional
    #include <string>          // std::string
    #include <atomic>          // std::atomic

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* VARIABLE */

inline std::atomic<bool> matching_raw_flag{false}; // [ui] matching = raw

//----------------------------------------------------------------//
/* FUNCTION */

// The matching of the completions, the palette and the search of the settings ([ui] matching): c2dmp-hsm (libutils, a
// heuristic distance: typos and misplaced letters found, ranked by distance) or raw (a prefix, then contained)
_cold inline void set_matching(const std::string& mode) {cluster::matching_raw_flag = mode == "raw";}; // "c2dmp" | "raw"
_cold _nodiscard inline bool matching_raw(void)         {return cluster::matching_raw_flag;};
_cold _nodiscard std::optional<float> match_score(const std::string& query, const std::string& text);   // one word / name: lower is closer, none: no match
_cold _nodiscard std::optional<float> match_words(const std::string& query, const std::string& text);   // every word of the query close to a word of the text

} // namespace end
#endif /* MATCH_H */
