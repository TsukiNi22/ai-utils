/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Git.hpp

File Description:
##  Git panel of a session: current branch, graph of the commits
##  and branches, live diff (cached, refreshed every few seconds)
\**************************************************************/

#ifndef CLUSTER_GIT_H
    #define CLUSTER_GIT_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include <cstdint>              // std::int64_t
    #include <string>               // std::string
    #include <vector>               // std::vector
    #include <atomic>               // std::atomic
    #include <mutex>                // std::mutex
    #include <map>                  // std::map
    #include <set>                  // std::set

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct GitInfo {
    bool repository = false;
    std::string branch;
    std::vector<std::string> graph;     // git log --graph --oneline --decorate --all
    std::vector<std::string> diffStat;  // git diff --stat HEAD
    std::vector<std::string> diff;      // git diff HEAD (first lines)
    std::int64_t time = 0;
};

//----------------------------------------------------------------//
/* CLASS */

class Git {
    private:
        mutable std::mutex _mutex;
        mutable std::map<std::string, cluster::GitInfo> _cache;  // per folder
        mutable std::set<std::string> _refreshing;
        mutable std::atomic<std::uint64_t> _version{0};

    public:
        // ---------- Pre-Function -------- //
        _cold _nodiscard cluster::GitInfo info(const std::string& cwd, const bool withDiff, const int maxAge = 5) const;
        _cold _nodiscard static cluster::GitInfo read(const std::string& cwd, const bool withDiff);
        _nodiscard inline std::uint64_t version(void) const {return this->_version;};   // changes when a refresh ends (redraw)

        // ---------- Constructor -------- //
        Git(void) = default;
        ~Git() = default;
};

} // namespace end

#endif /* CLUSTER_GIT_H */
