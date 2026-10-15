/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Progress.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Manager.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Wire.hpp"
#include <cstdio>

/* a long operation: its popup (the title, what it does now, its last lines greyed) */
_cold std::uint64_t cluster::Manager::progressBegin(const std::string& title)
{
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    cluster::Progress p;
    p.id = this->_progressNext++;
    p.title = title;
    p.started = cluster::now_ms();
    this->_progress = p;
    ++this->_version;
    return p.id;
}

_cold void cluster::Manager::progressStep(const std::uint64_t id, const std::string& now)
{
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    if (!this->_progress || this->_progress->id != id) return;
    this->_progress->now = now;
    ++this->_version;
}

_cold void cluster::Manager::progressLog(const std::uint64_t id, const std::string& line)
{
    // its last lines (8) kept, the empty ones skipped
    const std::string text = cluster::trim(line);
    if (text.empty()) return;
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    if (!this->_progress || this->_progress->id != id) return;
    this->_progress->log.push_back(cluster::one_line(text, 160));
    if (this->_progress->log.size() > 8) this->_progress->log.erase(this->_progress->log.begin());
    ++this->_version;
}

_cold void cluster::Manager::progressEnd(const std::uint64_t id, const bool ok, const std::string& text)
{
    {
        std::lock_guard<std::mutex> lock(this->_progressMutex);
        if (!this->_progress || this->_progress->id != id) return;
        this->_progress->done = true;
        this->_progress->ok = ok;
        this->_progress->now = text;
        this->_progress->ended = cluster::now_ms();
    }
    this->notice_("", text, !ok);
    ++this->_version;
}

_cold std::optional<cluster::Progress> cluster::Manager::progress(void) const
{
    // the one shown: running, or ended less than 4 s ago; an interface of the core: its own (an install here) first
    std::optional<cluster::Progress> mine;
    {
        std::lock_guard<std::mutex> lock(this->_progressMutex);
        mine = this->_progress;
    }
    const std::function<bool(const std::optional<cluster::Progress>&)> shown = [](const std::optional<cluster::Progress>& p) {
        return p && (!p->done || cluster::now_ms() - p->ended < 4000);
    };
    if (shown(mine)) return mine;
    if (this->_replica) {
        const cluster::Json core = this->replicaState("progress");
        if (core.is_object()) {
            std::optional<cluster::Progress> theirs = core.get<cluster::Progress>();
            if (shown(theirs)) return theirs;
        }
    }
    return std::nullopt;
}

/* a program run, its output line by line (to the popup of a long operation) */
_cold int cluster::run_logged(const std::vector<std::string>& args, const std::function<void(const std::string&)>& line)
{
    std::string command;
    for (const std::string& arg: args) {
        std::string quoted = "'";
        for (const char c: arg)
            quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
        command += (command.empty() ? "" : " ") + quoted + "'";
    }
    command += " 2>&1";
    FILE* pipe = ::popen(command.c_str(), "r");
    if (pipe == nullptr) return -1;
    char buffer[1024];
    std::string pending;
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        pending += buffer;
        for (std::size_t nl = pending.find_first_of("\r\n"); nl != std::string::npos; nl = pending.find_first_of("\r\n")) {
            if (line) line(pending.substr(0, nl));
            pending.erase(0, nl + 1);
        }
    }
    if (!pending.empty() && line) line(pending);
    const int status = ::pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
