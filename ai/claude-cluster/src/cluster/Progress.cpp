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
#include <thread>
#include <cstdio>

// the operation of this thread (its steps told by the code it calls: busy_step, busy_log)
static thread_local cluster::Manager* busy_manager_ = nullptr;
static thread_local std::uint64_t busy_id_ = 0;

/* a long operation: its popup (the title, what it does now, its last lines greyed) */
_cold std::uint64_t cluster::Manager::progressBegin(const std::string& title, const std::string& session)
{
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    cluster::Progress p;
    p.id = this->_progressNext++;
    p.title = title;
    p.session = session;
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

_cold void cluster::Manager::progressAdvance(const std::uint64_t id, const std::string& now)
{
    // the step before done (in its log), this one running
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    if (!this->_progress || this->_progress->id != id) return;
    if (!this->_progress->now.empty()) this->_progress->log.push_back("✓ " + this->_progress->now);
    if (this->_progress->log.size() > 8) this->_progress->log.erase(this->_progress->log.begin());
    this->_progress->now = now;
    ++this->_version;
}

_cold void cluster::Manager::progressLog(const std::uint64_t id, const std::string& line)
{
    // its last lines (8) kept, the empty ones skipped
    const std::string text = cluster::trim(line);
    if (text.empty()) return;
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    if (!this->_progress || this->_progress->id != id || this->_progress->done) return;
    this->_progress->log.push_back(cluster::one_line(text, 160));
    if (this->_progress->log.size() > 8) this->_progress->log.erase(this->_progress->log.begin());
    ++this->_version;
}

_cold void cluster::Manager::progressEnd(const std::uint64_t id, const bool ok, const std::string& text, const bool notice)
{
    {
        std::lock_guard<std::mutex> lock(this->_progressMutex);
        if (!this->_progress || this->_progress->id != id) return;
        this->_progress->done = true;
        this->_progress->ok = ok;
        this->_progress->now = text;
        this->_progress->ended = cluster::now_ms();
    }
    if (notice) this->notice_("", text, !ok);
    ++this->_version;
}

_cold void cluster::Manager::progressDrop(const std::uint64_t id)
{
    // gone without its end shown (fast, or its failure shown instead)
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    if (!this->_progress || this->_progress->id != id) return;
    this->_progress.reset();
    ++this->_version;
}

_cold void cluster::Manager::progressNotice_(const cluster::Notice& notice)
{
    // what is told while an operation runs: under it (its session, or claude-cluster)
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    if (!this->_progress || this->_progress->done) return;
    if (!this->_progress->session.empty() && !notice.session.empty() && notice.session != this->_progress->session) return;
    this->_progress->log.push_back(cluster::one_line(notice.text, 160));
    if (this->_progress->log.size() > 8) this->_progress->log.erase(this->_progress->log.begin());
}

_cold std::optional<cluster::Progress> cluster::Manager::progress(void) const
{
    // the one shown: running for 0.4 s at least (a fast one: no popup flashing), then its end (1.5 s, 4 s failed); an
    // interface of the core: its own (an install here) first
    std::optional<cluster::Progress> mine;
    {
        std::lock_guard<std::mutex> lock(this->_progressMutex);
        mine = this->_progress;
    }
    const std::function<bool(const std::optional<cluster::Progress>&)> shown = [](const std::optional<cluster::Progress>& p) {
        const std::int64_t now = cluster::now_ms();
        if (!p) return false;
        if (!p->done) return now - p->started >= 400;
        return p->ended - p->started >= 400 && now - p->ended < (p->ok ? 1500 : 4000);
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

/* what failed of what the user did: its popup (OK, Retry) */
_cold std::string cluster::Manager::fail(const std::string& action, const std::string& source, const std::string& error, const std::function<void(void)>& retry)
{
    std::string id;
    {
        std::lock_guard<std::mutex> lock(this->_progressMutex);
        for (std::pair<cluster::Failure, std::function<void(void)>>& known: this->_failures)
            if (known.first.action == action && known.first.error == error) { // the same again: once
                known.first.at = cluster::now();
                return known.first.id;
            }
        cluster::Failure f{(this->_replica ? "l" : "c") + std::to_string(this->_failureNext++), action, source, error, static_cast<bool>(retry), cluster::now()};
        id = f.id;
        this->_failures.emplace_back(f, retry);
        if (this->_failures.size() > 20) this->_failures.pop_front();
    }
    ++this->_version;
    return id;
}

_cold std::vector<cluster::Failure> cluster::Manager::failures(void) const
{
    // the ones of this process, then the ones of the core (an interface of it), the oldest first
    std::vector<cluster::Failure> list;
    std::set<std::string> gone;
    {
        std::lock_guard<std::mutex> lock(this->_progressMutex);
        for (const std::pair<cluster::Failure, std::function<void(void)>>& known: this->_failures)
            list.push_back(known.first);
        gone = this->_failureGone;
    }
    if (this->_replica) {
        const cluster::Json core = this->replicaState("failures");
        if (core.is_array())
            for (const cluster::Json& f: core)
                if (!gone.contains(f.value("id", std::string()))) list.push_back(f.get<cluster::Failure>());
    }
    return list;
}

_cold void cluster::Manager::failureAnswer(const std::string& id, const bool retry)
{
    // OK: gone; Retry: gone, what failed run again (failing again: its popup again)
    std::optional<std::pair<cluster::Failure, std::function<void(void)>>> found;
    {
        std::lock_guard<std::mutex> lock(this->_progressMutex);
        for (auto it = this->_failures.begin(); it != this->_failures.end(); ++it)
            if (it->first.id == id) {
                found = *it;
                this->_failures.erase(it);
                break;
            }
        if (!found && this->_replica) this->_failureGone.insert(id);
    }
    ++this->_version;
    if (!found) {
        if (this->_replica) {
            try {
                (void)this->call({{"cmd", "failure"}, {"id", id}, {"answer", retry ? "retry" : "ok"}});
            } catch (const utils::exception::IException&) {}
        }
        return;
    }
    if (!retry || !found->second) return;
    std::thread([this, f = found->first, again = found->second]() {
        try {
            again();
        } catch (const utils::exception::IException& e) {
            (void)this->fail(f.action, f.source, std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info()), again);
        }
    }).detach();
}

/* an operation of this thread: its popup, its steps told by the code it calls */
cluster::Busy::Busy(cluster::Manager& manager, const std::string& title, const std::string& session) : _manager(manager)
{
    if (busy_manager_ != nullptr) { // inside another one: a step of it
        this->_nested = true;
        cluster::busy_step(title);
        return;
    }
    this->_id = manager.progressBegin(title, session);
    busy_manager_ = &manager;
    busy_id_ = this->_id;
}

cluster::Busy::~Busy()
{
    this->drop();
}

void cluster::Busy::end(const bool ok, const std::string& text)
{
    // done: its result shown a moment (when it took time); failed: its popup of failure instead (told by the caller)
    if (this->_nested || this->_closed) return;
    this->_closed = true;
    if (ok) this->_manager.progressEnd(this->_id, true, text.empty() ? "done" : cluster::one_line(text, 200), false);
    else this->_manager.progressDrop(this->_id);
    busy_manager_ = nullptr;
    busy_id_ = 0;
}

void cluster::Busy::drop(void)
{
    if (this->_nested || this->_closed) return;
    this->_closed = true;
    this->_manager.progressDrop(this->_id);
    busy_manager_ = nullptr;
    busy_id_ = 0;
}

_cold void cluster::busy_step(const std::string& text) // xstyle: ignore CPP-SINGLE-STATEMENT (its thread state lives here)
{
    if (busy_manager_ != nullptr) busy_manager_->progressAdvance(busy_id_, text);
}

_cold void cluster::busy_log(const std::string& line) // xstyle: ignore CPP-SINGLE-STATEMENT (its thread state lives here)
{
    if (busy_manager_ != nullptr) busy_manager_->progressLog(busy_id_, line);
}

_cold bool& cluster::failure_popups(void)
{
    static thread_local bool popups = true;
    return popups;
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
