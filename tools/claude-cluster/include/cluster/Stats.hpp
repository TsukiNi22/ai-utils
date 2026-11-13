/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Stats.hpp

File Description:
##  The Stats session: usage (cost and tokens per day, per model and provider, per session, cache and thinking) and activity
##  (uptime, working time, the turns by hour, tools and permissions, limits and errors) drawn as display blocks, refreshed in place
\**************************************************************/

#ifndef STATS_H
    #define STATS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* utils */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard

    /* project */
    #include "Types.hpp"                    // cluster::Json

    /* type */
    #include <string>                       // std::string
    #include <vector>                       // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* DEFINE */

class Manager;

//----------------------------------------------------------------//
/* FUNCTION */

// The blocks of the report (display blocks with a stable id, in the order they are shown), computed from the usage ledger and the
// events of stats.jsonl; days: the window of the daily charts
_cold _nodiscard std::vector<cluster::Json> stats_blocks(cluster::Manager& manager, const int days);

// The Stats session: made when missing, its blocks created / updated in place; its id; focus: the interfaces show it
_cold std::string stats_open(cluster::Manager& manager, const bool focus = false);

// Refreshed when it exists (the tick of the core): true when it did
_cold bool stats_refresh(cluster::Manager& manager);

} // namespace end
#endif /* STATS_H */
