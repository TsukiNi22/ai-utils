/**************************************************************\
Edition:
##  @date 11/10/2026 by @author Tsukini

File Name:
##  @file Service.hpp

File Description:
##  Background service (systemd --user): it listens for the wake
##  phrase ("ok claude") with almost no CPU and opens claude-cluster
##  (terminal or window, voice on) when no instance runs
\**************************************************************/

#ifndef CLUSTER_SERVICE_H
    #define CLUSTER_SERVICE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Tools.hpp"            // cluster::expand_home, cluster::data_dir
    #include <filesystem>           // std::filesystem::path
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct ServiceOptions {
    bool verbose = false;   // run: the end of the text of every sound on stderr (tests)
    bool debug = false;     // setup / enable / run: the whole text heard in the journal (tests only)
    bool follow = false;    // logs: the new lines as they come
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard inline std::filesystem::path service_unit(void) {return cluster::expand_home("~/.config/systemd/user") / "claude-cluster.service";};
_cold _nodiscard inline std::filesystem::path service_script(void) {return cluster::data_dir() / "wake.py";}; // the detector (from the binary)

// claude-cluster service setup | enable | disable | start | stop | restart | status | remove | logs | run [--verbose] [--debug]
_cold _nodiscard int service_command(const std::vector<std::string>& words, const cluster::ServiceOptions& options);

} // namespace end

#endif /* CLUSTER_SERVICE_H */
