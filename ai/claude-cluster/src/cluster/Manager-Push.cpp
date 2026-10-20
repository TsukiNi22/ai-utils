/**************************************************************\
Edition:
##  @date 20/10/2026 by @author Tsukini

File Name:
##  @file Manager-Push.cpp

File Description:
##  The notifications of the desktop published to the Android app:
##  a log the app follows by a long poll (/api/notify)
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Manager.hpp"
#include "cluster/Tools.hpp"

#define PUSH_KEPT 100   // the notifications kept for an app that comes back

_cold void cluster::Manager::pushNotice_(const cluster::Notification& n, const std::string& kind)
{
    // the same kinds as the desktop (their settings), whatever [ui] notify (the desktop) and notify_when (the focus here): the app decides
    // by itself what to show (its own kinds, not in front); the Debug page notifications reach it too
    const cluster::Config& c = this->_config;
    const bool demo = kind == "showcase";
    if (!c.notifyApp && !demo) return;
    const std::map<std::string, bool> kinds = {{"global", c.notifyGlobal}, {"done", c.notifyDone}, {"permission", c.notifyPermission}, {"question", c.notifyQuestion},
        {"error", c.notifyError}, {"limit", c.notifyLimits}, {"web", c.notifyWeb}, {"task", c.notifyTasks}, {"reminder", c.notifyReminders},
        {"ready", c.notifyReady}};
    if (kinds.contains(kind) && !kinds.at(kind)) return;
    const std::string name = kind.empty() ? "message" : kind; // the notify tool, a core question: "message"
    std::lock_guard<std::mutex> lock(this->_mutex);
    const std::uint64_t seq = this->_pushSeq.load() + 1;
    this->_pushLog.push_back({{"seq", seq}, {"time", cluster::now()}, {"kind", name}, {"session", n.session}, {"title", n.title},
        {"body", cluster::one_line(n.body, 400)}, {"urgent", n.urgent}});
    while (this->_pushLog.size() > PUSH_KEPT)
        this->_pushLog.pop_front();
    this->_pushSeq = seq;
}

cluster::Json cluster::Manager::pushSince(const std::int64_t since) const
{
    // no number (-1) or one from before a restart of the core (over the last): nothing replayed, the app starts from now
    cluster::Json events = cluster::Json::array();
    std::lock_guard<std::mutex> lock(this->_mutex);
    const std::uint64_t seq = this->_pushSeq.load();
    if (since >= 0 && static_cast<std::uint64_t>(since) <= seq)
        for (const cluster::Json& event: this->_pushLog)
            if (event["seq"].get<std::uint64_t>() > static_cast<std::uint64_t>(since)) events.push_back(event);
    return {{"seq", seq}, {"events", events}, {"enabled", this->_config.notifyApp}};
}
