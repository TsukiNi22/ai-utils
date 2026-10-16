/**************************************************************\
Edition:
##  @date 16/10/2026 by @author Tsukini

File Name:
##  @file WebTty.hpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef WEBTTY_H
    #define WEBTTY_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Json
    #include <condition_variable>   // std::condition_variable
    #include <cstdint>              // std::uint64_t
    #include <memory>               // std::shared_ptr
    #include <string>               // std::string
    #include <thread>               // std::thread
    #include <vector>               // std::vector
    #include <mutex>                // std::mutex
    #include <map>                  // std::map

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// A terminal interface of claude-cluster (claude-cluster --tty) in a pty: its process, its master side
struct Pty {
    int pid = -1;
    int fd = -1;
};

//----------------------------------------------------------------//
/* FUNCTION */

// claude-cluster --tty started in a pty of cols x rows (the window in its terminal mode, the terminal page of the web
// access: web: its rules - CLAUDE_CLUSTER_WEB_TTY); the master side to read / write, resized with pty_resize
_cold _nodiscard cluster::Pty pty_spawn(const int cols, const int rows, const bool web);
_cold void pty_resize(const cluster::Pty& pty, const int cols, const int rows);
_cold void pty_kill(cluster::Pty& pty);         // the process killed (SIGHUP, then SIGKILL), waited for; its fd kept (a reader ends on its own)
_cold void pty_close(cluster::Pty& pty);        // killed, its fd closed (no reader left on it)

//----------------------------------------------------------------//
/* CLASS */

// The terminals of the web page (the TTY port): one per device, its output kept for the page to read (long polls),
// its keys written, gone after 2 min without a read
class WebTty {
    private:
        struct Session {
            cluster::Pty pty;
            int cols = 80;
            int rows = 24;
            std::string out;            // the last output (512 KB at most)
            std::uint64_t base = 0;     // the offset of out[0] since the start
            bool alive = true;
            std::int64_t seen = 0;      // ms of the last read by the page
            std::mutex mutex;
            std::condition_variable changed;
            std::thread reader;
        };
        std::map<std::string, std::shared_ptr<Session>> _sessions;  // by device id (shared: a long poll keeps its session alive)
        std::mutex _mutex;

        // ---------- Pre-Function -------- //
        _cold void read_(Session* session);     // the pty read until its end (a thread per terminal)
        _cold void drop_(const std::shared_ptr<Session>& session);
        _cold void resize_(Session& session, const int cols, const int rows);

    public:
        // ---------- Constructor -------- //
        _cold WebTty() = default;

        // ---------- Destructor -------- //
        _cold ~WebTty();

        // ---------- Pre-Function -------- //
        _cold cluster::Json open(const std::string& device, const int cols, const int rows);    // started (or kept: resized); {cols, rows}
        _cold cluster::Json read(const std::string& device, const std::uint64_t from, const int waitMs); // {from, data (base64), next, alive}
        _cold void write(const std::string& device, const std::string& bytes);
        _cold void resize(const std::string& device, const int cols, const int rows);
        _cold void close(const std::string& device);
        _cold void expire(void);                // the ones not read for 2 min: closed
        _cold void closeAll(void);
        _cold _nodiscard std::size_t count(void);
};

} // namespace end
#endif /* WEBTTY_H */
