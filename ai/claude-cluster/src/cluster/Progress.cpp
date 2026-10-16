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

/* the long operations: a queue of popups (the title, what it does now, its last lines greyed), one shown at a time */
#define PROGRESS_SHOWN_MS 400   // a faster one: no popup (nothing flashing)
#define PROGRESS_END_MS 1500    // its result shown so long

_cold static cluster::Progress* progress_find_(std::vector<cluster::Progress>& list, const std::uint64_t id)
{
    const std::vector<cluster::Progress>::iterator it = std::find_if(list.begin(), list.end(), [id](const cluster::Progress& p) {return p.id == id;});
    return it == list.end() ? nullptr : &*it;
}

_cold static bool progress_visible_(const cluster::Progress& p, const std::int64_t now)
{
    // running for 0.4 s at least, then its result 1.5 s (when it ran 0.4 s at least)
    if (!p.done) return now - p.started >= PROGRESS_SHOWN_MS;
    return p.ended - p.started >= PROGRESS_SHOWN_MS && now - p.ended < PROGRESS_END_MS;
}

_cold std::uint64_t cluster::Manager::progressBegin(const std::string& title, const std::string& session)
{
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    const std::int64_t now = cluster::now_ms();
    std::erase_if(this->_progresses, [now](const cluster::Progress& p) {return p.done && now - p.ended >= PROGRESS_END_MS;}); // the old results
    cluster::Progress p;
    p.id = this->_progressNext++;
    p.title = title;
    p.session = session;
    p.started = now;
    this->_progresses.push_back(p);
    ++this->_version;
    return p.id;
}

_cold void cluster::Manager::progressStep(const std::uint64_t id, const std::string& now)
{
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    if (cluster::Progress* p = progress_find_(this->_progresses, id)) p->now = now;
    ++this->_version;
}

_cold void cluster::Manager::progressAdvance(const std::uint64_t id, const std::string& now)
{
    // the step before done (in its log), this one running
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    cluster::Progress* p = progress_find_(this->_progresses, id);
    if (p == nullptr) return;
    if (!p->now.empty()) p->log.push_back("✓ " + p->now);
    if (p->log.size() > 8) p->log.erase(p->log.begin());
    p->now = now;
    ++this->_version;
}

_cold void cluster::Manager::progressLog(const std::uint64_t id, const std::string& line)
{
    // its last lines (8) kept, the empty ones skipped
    const std::string text = cluster::trim(line);
    if (text.empty()) return;
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    cluster::Progress* p = progress_find_(this->_progresses, id);
    if (p == nullptr || p->done) return;
    p->log.push_back(cluster::one_line(text, 160));
    if (p->log.size() > 8) p->log.erase(p->log.begin());
    ++this->_version;
}

_cold void cluster::Manager::progressEnd(const std::uint64_t id, const bool ok, const std::string& text, const bool notice)
{
    {
        std::lock_guard<std::mutex> lock(this->_progressMutex);
        cluster::Progress* p = progress_find_(this->_progresses, id);
        if (p == nullptr) return;
        p->done = true;
        p->ok = ok;
        p->now = text;
        p->ended = cluster::now_ms();
    }
    if (notice) this->notice_("", text, !ok);
    ++this->_version;
}

_cold void cluster::Manager::progressDrop(const std::uint64_t id)
{
    // gone without its end shown (fast)
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    std::erase_if(this->_progresses, [id](const cluster::Progress& p) {return p.id == id;});
    ++this->_version;
}

_cold void cluster::Manager::progressNotice_(const cluster::Notice& notice)
{
    // what is told while an operation runs: under the last one running (its session, or claude-cluster)
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    for (std::vector<cluster::Progress>::reverse_iterator p = this->_progresses.rbegin(); p != this->_progresses.rend(); ++p) {
        if (p->done || (!p->session.empty() && !notice.session.empty() && notice.session != p->session)) continue;
        p->log.push_back(cluster::one_line(notice.text, 160));
        if (p->log.size() > 8) p->log.erase(p->log.begin());
        return;
    }
}

_cold std::vector<cluster::Progress> cluster::Manager::progressAll(void) const
{
    // the ones running and the results not expired (of this process): sent to the interfaces of the core, the web page
    std::lock_guard<std::mutex> lock(this->_progressMutex);
    const std::int64_t now = cluster::now_ms();
    std::vector<cluster::Progress> list;
    std::copy_if(this->_progresses.begin(), this->_progresses.end(), std::back_inserter(list), [now](const cluster::Progress& p) {
        return !p.done || now - p.ended < PROGRESS_END_MS;
    });
    return list;
}

_cold std::vector<cluster::Progress> cluster::Manager::progressQueue_(void) const
{
    // this process's ones, then the core's ones (an interface of it)
    std::vector<cluster::Progress> list = this->progressAll();
    if (this->_replica) {
        const cluster::Json core = this->replicaState("progress");
        if (core.is_array())
            for (const cluster::Json& p: core)
                list.push_back(p.get<cluster::Progress>());
    }
    return list;
}

_cold std::optional<cluster::Progress> cluster::Manager::progress(void) const
{
    // the queue: the first one visible (running for 0.4 s, or its result), the next ones after it
    const std::int64_t now = cluster::now_ms();
    for (const cluster::Progress& p: this->progressQueue_())
        if (progress_visible_(p, now)) return p;
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
    // its result shown a moment (when it took time: done ✓, failed ✗ - then its failure popup); failed without a text:
    // gone at once
    if (this->_nested || this->_closed) return;
    this->_closed = true;
    if (ok || !text.empty()) this->_manager.progressEnd(this->_id, ok, text.empty() ? "done" : cluster::one_line(text, 200), false);
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
    // never the terminal of the interface (a program asking something: it reads nothing, its own session)
    static const bool setsid = cluster::has_command("setsid");
    command = (setsid ? "setsid -w " : "") + command + " < /dev/null 2>&1"; // -w: its exit code kept whatever setsid does
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
