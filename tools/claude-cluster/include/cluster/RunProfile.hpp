/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file RunProfile.hpp

File Description:
##  The low profile of a heavy command (resource orchestrator):
##  its parallelism lowered to the share granted, nice, ionice.
##  Shared by the core and by the cluster-run wrapper (standard
##  library only)
\**************************************************************/

#ifndef RUNPROFILE_H
    #define RUNPROFILE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    #include <utility>              // std::pair
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Grant {
    int threads = 1;                // the parallelism granted to the command
    int nice = 19;
    int ioClass = 3;                // ioprio class: 1 realtime, 2 best-effort, 3 idle
    int ioLevel = 0;                // 0..7 (best-effort)
};

//----------------------------------------------------------------//
/* FUNCTION */

// The variables limiting the parallelism of the usual tools to `threads` (MAKEFLAGS, CMAKE_BUILD_PARALLEL_LEVEL, CARGO_BUILD_JOBS,
// OMP_THREAD_LIMIT - so that nproc says it too -, ...); `current` is the environment the command would get (a MAKEFLAGS with
// a jobserver is kept)
[[nodiscard]] std::vector<std::pair<std::string, std::string>> grant_env(const int threads, const std::vector<std::string>& current = {}); // xstyle: ignore LU-ATTRIBUTE (standard library only: shared with cluster-run)
// The command with its explicit -jN / --parallel N / --jobs N / -n N (make, ninja, cmake, ctest, cargo, pytest...) lowered to `threads`
// (never raised); ninja gets -jN when it has none
[[nodiscard]] std::vector<std::string> grant_argv(const std::vector<std::string>& argv, const int threads); // xstyle: ignore LU-ATTRIBUTE (same)
// nice and ionice of the calling process (the children inherit them)
void grant_apply(const cluster::Grant& grant);
// nice and ionice of a whole process group (the orchestrator, under pressure); false: none changed
bool grant_group(const int pgid, const int nice);
// The tail of a text (its last `max` bytes, cut at a line)
[[nodiscard]] std::string tail_of(const std::string& text, const std::size_t max); // xstyle: ignore LU-ATTRIBUTE (same)

} // namespace end

#endif /* RUNPROFILE_H */
