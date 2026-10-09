/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Control.hpp

File Description:
##  Control socket of a running claude-cluster (unix socket, one
##  JSON request / response per line): used by the headless CLI
##  and by the MCP server of the global session
\**************************************************************/

#ifndef CLUSTER_CONTROL_H
    #define CLUSTER_CONTROL_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Manager.hpp"          // cluster::Manager
    #include "Types.hpp"            // cluster::Json
    #include <filesystem>           // std::filesystem::path
    #include <atomic>               // std::atomic
    #include <thread>               // std::thread

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class Control {
    private:
        cluster::Manager& _manager;
        std::filesystem::path _path;
        int _fd = -1;
        std::atomic<bool> _running{false};
        std::thread _thread;

        // ---------- Pre-Function -------- //
        _cold void serve_(void);
        _cold void client_(const int fd);

    public:
        // ---------- Pre-Function -------- //
        _cold void start(void);             // throws AlreadyRunning when another instance owns the socket
        _cold void stop(void);
        _cold _nodiscard static cluster::Json handle(cluster::Manager& manager, const cluster::Json& request);
        _cold _nodiscard static cluster::Json request(const cluster::Json& request);  // client side, throws when nothing runs
        _cold _nodiscard static bool alive(void);

        // ---------- Constructor -------- //
        _cold Control(cluster::Manager& manager);
        _cold ~Control();
};

} // namespace end

#endif /* CLUSTER_CONTROL_H */
