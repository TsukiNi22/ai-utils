/**************************************************************\
Edition:
##  @date 11/10/2026 by @author Tsukini

File Name:
##  @file Machine.hpp

File Description:
##  Map of this computer for the global session (machine.md): the
##  OS, the user, the shell and its aliases (the ones replacing a
##  common command or dangerous), the tools, the git repositories
\**************************************************************/

#ifndef CLUSTER_MACHINE_H
    #define CLUSTER_MACHINE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Config.hpp"           // cluster::Config
    #include "Tools.hpp"            // cluster::config_dir
    #include <filesystem>           // std::filesystem::path
    #include <functional>           // std::function
    #include <cstdint>              // std::uint64_t
    #include <utility>              // std::pair
    #include <cstddef>              // std::size_t
    #include <vector>               // std::vector
    #include <string>               // std::string

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// Where a scan is: its steps (0 to do, 1 running, 2 done), the last lines done, what it does now, what comes next
struct MachineProgress {
    std::uint64_t id = 0;
    std::vector<std::pair<std::string, int>> steps;
    std::vector<std::string> log;       // the last 3 steps done ("✓ 198 aliases read")
    std::size_t finished = 0;           // steps done so far (the lines of the log not shown yet: finished - shown)
    std::string now;                    // what it is doing
    std::string next;                   // what comes next
    double fraction = 0.0;
    bool done = false;
    std::size_t repos = 0;
};

// A question about the user (what the system doesn't tell, or tells in several ways): [user] <key>
struct ProfileQuestion {
    std::string key;
    std::string question;
    std::string value;                  // its suggestion
    std::vector<std::string> choices;   // ← → among them (it can be typed too)
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard inline std::filesystem::path machine_file(void) {return cluster::config_dir() / "machine.md";};
_cold _nodiscard std::string machine_scope(void);           // what a scan reads and stores (asked before every scan)
_cold _nodiscard std::size_t machine_scan(const cluster::Config& config, const std::function<void(const cluster::MachineProgress&)>& report = {}); // repositories
_cold _nodiscard std::vector<cluster::ProfileQuestion> machine_questions(const cluster::Config& config);  // the [user] fields, at every sync
_cold _nodiscard int machine_days_old(void);                // age of the map in days (-1: none)
_cold _nodiscard std::string machine_age(void);             // "2026-10-11 14:03" of the file, empty: none

} // namespace end

#endif /* CLUSTER_MACHINE_H */
