/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Git.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Tools.hpp"
#include "cluster/Git.hpp"
#include <sstream>
#include <thread>

/* tools */
_cold static std::vector<std::string> lines_(const std::string& text, const std::size_t max)
{
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line) && lines.size() < max)
        lines.push_back(line);
    return lines;
}

/* git */
_cold cluster::GitInfo cluster::Git::read(const std::string& cwd, const bool withDiff)
{
    cluster::GitInfo info;
    info.time = cluster::now();
    const cluster::Captured inside = cluster::capture({"git", "rev-parse", "--is-inside-work-tree"}, cwd, 5);
    if (inside.code != 0 || cluster::trim(inside.out) != "true") return info;

    info.repository = true;
    info.branch = cluster::trim(cluster::capture({"git", "branch", "--show-current"}, cwd, 5).out);
    if (info.branch.empty()) info.branch = "(detached " + cluster::trim(cluster::capture({"git", "rev-parse", "--short", "HEAD"}, cwd, 5).out) + ")";
    info.graph = lines_(cluster::capture({"git", "log", "--graph", "--oneline", "--decorate", "--all", "--color=never", "-n", "40"}, cwd, 5).out, 40);
    info.diffStat = lines_(cluster::capture({"git", "diff", "--stat", "HEAD", "--color=never"}, cwd, 5).out, 30);
    if (withDiff) info.diff = lines_(cluster::capture({"git", "diff", "HEAD", "--color=never"}, cwd, 5).out, 400);
    return info;
}

_cold cluster::GitInfo cluster::Git::info(const std::string& cwd, const bool withDiff, const int maxAge) const
{
    // Never blocks the UI: the cached value is returned, a stale one is refreshed by a thread
    cluster::GitInfo cached;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = this->_cache.find(cwd);
        if (it != this->_cache.end()) cached = it->second;
        const bool fresh = it != this->_cache.end() && cluster::now() - cached.time < maxAge && (!withDiff || !cached.diff.empty() || cached.diffStat.empty());
        if (fresh || this->_refreshing.contains(cwd)) return cached;
        this->_refreshing.insert(cwd);
    }
    std::thread([this, cwd, withDiff]() {
        cluster::GitInfo info = cluster::Git::read(cwd, withDiff);
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_cache[cwd] = info;
        this->_refreshing.erase(cwd);
        ++this->_version;
    }).detach();
    return cached;
}
