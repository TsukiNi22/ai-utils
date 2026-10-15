/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Manager-Replica.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Control.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Voice.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Wire.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <thread>

/* the replica: an interface of another process (several terminals / windows on one core) */
struct cluster::ReplicaState {
    std::atomic<bool> running{false};
    std::string client;                         // its id for the core: <computer>:<pid>
    mutable std::mutex mutex;
    std::vector<cluster::Snapshot> sessions;    // in the order of the core (the global one first)
    cluster::Json state = cluster::Json::object();  // the rest of the last sync
    std::string error;                          // the core not reached: why
    std::uint64_t seen = 0;                     // the version of the core last seen
    std::atomic<std::uint64_t> version{1};      // changed at each sync that brought something (the interfaces redraw)
    std::atomic<int> fd{-1};                    // the socket of the sync waiting (shut down at the quit: it ends at once)
    std::thread thread;                         // the sync (joined at the quit)
};

_cold static void sync_once_(const std::shared_ptr<cluster::ReplicaState>& replica, const bool wait)
{
    // the state of the core: what changed since the versions known (waited for up to 10 s), the sessions sent again only when
    // they changed
    cluster::Json known = cluster::Json::object();
    std::uint64_t seen = 0;
    {
        std::lock_guard<std::mutex> lock(replica->mutex);
        for (const cluster::Snapshot& snap: replica->sessions)
            known[snap.spec.id] = snap.version;
        seen = wait ? replica->seen : 0;
    }
    const cluster::Json response = cluster::Control::request({{"cmd", "sync"}, {"client", replica->client}, {"v", seen}, {"known", known}}, &replica->fd);
    if (!response.value("ok", false)) throw utils::exception::ErrorException(utils::exception::InternalCode::Communication, response.value("error", std::string("sync")));
    cluster::Json data = response.value("data", cluster::Json::object());
    std::lock_guard<std::mutex> lock(replica->mutex);
    std::vector<cluster::Snapshot> sessions;
    for (const cluster::Json& item: data.value("sessions", cluster::Json::array())) {
        const std::string id = item.value("id", std::string());
        if (item.contains("snapshot")) {
            sessions.push_back(item["snapshot"].get<cluster::Snapshot>());
            continue;
        }
        auto it = std::find_if(replica->sessions.begin(), replica->sessions.end(), [&id](const cluster::Snapshot& s) {return s.spec.id == id;});
        if (it != replica->sessions.end()) sessions.push_back(*it);
    }
    replica->sessions = std::move(sessions);
    data.erase("sessions");
    const bool changed = data.value("version", std::uint64_t{0}) != replica->seen || !replica->error.empty();
    replica->seen = data.value("version", std::uint64_t{0});
    replica->state = std::move(data);
    replica->error.clear();
    if (changed) ++replica->version;
}

_cold void cluster::Manager::attach(void)
{
    // this process: an interface of the core running (its state kept in sync by a thread of its own, its actions sent to it);
    // the core gone: started again (its sessions resumed), the interfaces go on
    std::array<char, 256> host{};
    (void)::gethostname(host.data(), host.size() - 1);
    this->_replica = std::make_shared<cluster::ReplicaState>();
    this->_replica->client = std::string(host.data()) + ":" + std::to_string(::getpid());
    sync_once_(this->_replica, false);
    this->_replica->running = true;
    this->_replica->thread = std::thread([replica = this->_replica]() {
        int failures = 0;
        while (replica->running) {
            try {
                sync_once_(replica, true);
                failures = 0;
            } catch (const std::exception& e) {
                {
                    std::lock_guard<std::mutex> lock(replica->mutex);
                    replica->error = std::string("the core of claude-cluster not reached (") + e.what() + "): reconnecting";
                }
                ++replica->version;
                // the core gone (the last interface closed it, a crash): started again once in a while, its sessions resumed
                if (++failures % 5 == 1 && replica->running && !cluster::Control::alive()) cluster::start_core();
                for (int i = 0; i < 10 && replica->running; ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });
}

_cold cluster::Json cluster::Manager::call(cluster::Json request) const
{
    // a command run by the core (this process: an interface of it)
    if (this->_replica) request["client"] = this->_replica->client;
    const cluster::Json response = cluster::Control::request(request);
    if (!response.value("ok", false))
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, response.value("error", std::string("refused by the core")));
    return response.value("data", cluster::Json());
}

_cold cluster::Json cluster::Manager::replicaState(const std::string& key) const
{
    if (!this->_replica) return {};
    std::lock_guard<std::mutex> lock(this->_replica->mutex);
    return this->_replica->state.value(key, cluster::Json());
}

_cold std::string cluster::Manager::replicaError(void) const
{
    if (!this->_replica) return "";
    std::lock_guard<std::mutex> lock(this->_replica->mutex);
    return this->_replica->error;
}

_cold std::vector<cluster::Snapshot> cluster::Manager::replicaSessions_(void) const
{
    std::lock_guard<std::mutex> lock(this->_replica->mutex);
    return this->_replica->sessions;
}

_cold std::uint64_t cluster::Manager::replicaVersion_(void) const
{
    // its type complete here only (not in the header)
    const std::uint64_t version = this->_replica->version;
    return version;
}

_cold void cluster::Manager::detach_(void)
{
    // this interface closed: told to the core (it stops when no interface is left, unless it is the service)
    if (!this->_replica) return;
    this->_replica->running = false;
    if (const int fd = this->_replica->fd; fd >= 0) (void)::shutdown(fd, SHUT_RDWR); // the sync waiting: it ends now
    if (this->_replica->thread.joinable()) this->_replica->thread.join();
    try {
        (void)this->call({{"cmd", "detach"}});
    } catch (const utils::exception::IException&) {}
}

/* the core side: its interfaces, what they get */
_cold void cluster::Manager::clientSeen(const std::string& client)
{
    if (client.empty()) return;
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_clients[client] = cluster::now();
    this->_hadClients = true;
}

_cold void cluster::Manager::clientGone(const std::string& client)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_clients.erase(client);
    this->_clientFocus.erase(client);
    this->_focused = std::any_of(this->_clientFocus.begin(), this->_clientFocus.end(), [](const std::pair<const std::string, bool>& f) {return f.second;}) ? 1 : 0;
}

_cold void cluster::Manager::clientFocus(const std::string& client, const bool focused)
{
    // the focus of claude-cluster: one of its interfaces in front (the notifications are for the others)
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_clientFocus[client] = focused;
    this->_focused = std::any_of(this->_clientFocus.begin(), this->_clientFocus.end(), [](const std::pair<const std::string, bool>& f) {return f.second;}) ? 1 : 0;
}

_cold std::size_t cluster::Manager::clients(void) const
{
    // the interfaces seen in the last 20 s (a crashed one forgotten)
    std::lock_guard<std::mutex> lock(this->_mutex);
    const std::int64_t now = cluster::now();
    return static_cast<std::size_t>(std::count_if(this->_clients.begin(), this->_clients.end(), [now](const std::pair<const std::string, std::int64_t>& c) {return now - c.second <= 20;}));
}

_cold void cluster::Manager::noteSender(const std::string& id, const std::string& client)
{
    // two interfaces sending to one session at the same moment: told on it (the second prompt waits behind the first)
    const std::int64_t now = cluster::now();
    std::string other;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto it = this->_lastSender.find(id);
        if (it != this->_lastSender.end() && it->second.first != client && now - it->second.second <= 3) other = it->second.first;
        this->_lastSender[id] = {client, now};
    }
    if (other.empty()) return;
    const std::string text = "two prompts at once from two interfaces (" + other + ", " + client + "): the second one waits behind the first";
    this->notice_(id, text, true);
    try {
        this->get_(id)->warn(text); // on the session itself, a minute
    } catch (const utils::exception::IException&) {}
}

_cold cluster::Json cluster::Manager::syncState(const cluster::Json& request)
{
    // everything an interface of another process shows: its sessions (sent again only when they changed), the notices, the
    // trash, the tasks, the ui choices, the question being answered, the face, the requests of focus / raise, the map
    const cluster::Json known = request.value("known", cluster::Json::object());
    cluster::Json sessions = cluster::Json::array();
    for (const cluster::Snapshot& snap: this->list(true)) {
        cluster::Json item = {{"id", snap.spec.id}};
        if (!known.contains(snap.spec.id) || known[snap.spec.id] != snap.version) item["snapshot"] = snap;
        sessions.push_back(std::move(item));
    }
    std::map<std::string, std::string> ui;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        ui = this->_ui;
    }
    const bool listening = this->_voice && this->_voice->listening();
    cluster::Json machine = nullptr;
    if (const std::optional<std::pair<cluster::MachineProgress, std::int64_t>> progress = this->machineProgress()) machine = {{"progress", progress->first}, {"end", progress->second}};
    return {{"version", this->version() + this->asking().version() * 1000003}, {"sessions", sessions}, {"notices", this->notices(50)},
        {"trash", this->trash()}, {"previous", this->previous()}, {"tasks", this->tasks()}, {"ui", ui}, {"asking", this->asking().json()},
        {"face", this->appFace(listening)}, {"focus", {this->_focusVersion.load(), this->_focus}}, {"raise", this->_raiseVersion.load()},
        {"machine_request", this->_machineRequest.load()}, {"machine_ask", this->machineAsk()}, {"machine", machine},
        {"restore_ask", this->_restoreAsk.load() && this->restorable()}, {"listening", listening}};
}

_cold bool cluster::start_core(const std::vector<std::string>& args)
{
    // the core of claude-cluster in a process of its own (setsid: no terminal); it stops when its last interface closes
    // its output in <data>/core.log (why it stopped)
    // put in the background by sh (which ends at once): nothing of this process waits for it
    std::vector<std::string> command = {"sh", "-c", "log=\"$1\"; shift; setsid \"$@\" >>\"$log\" 2>&1 </dev/null &", "sh", (cluster::data_dir() / "core.log").string(),
        cluster::self_exe().string(), "serve", "--linger"};
    command.insert(command.end(), args.begin(), args.end());
    cluster::detach(command);
    for (int i = 0; i < 100 && !cluster::Control::alive(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return cluster::Control::alive();
}

/* the replica: its reads from the last sync, its actions run by the core */
_cold void cluster::Manager::setFocused(const bool focused)
{
    if (!this->_replica) {
        this->_focused = focused ? 1 : 0;
        return;
    }
    if (this->_focused == (focused ? 1 : 0)) return;
    this->_focused = focused ? 1 : 0;
    try {
        (void)this->call({{"cmd", "client_focus"}, {"focused", focused}});
    } catch (const utils::exception::IException&) {}
}

_cold void cluster::Manager::noteActive(const std::string& id)
{
    this->_active = id;
    if (!this->_replica || id == this->_lastActive) return;
    this->_lastActive = id;
    try {
        (void)this->call({{"cmd", "active"}, {"session", id}});
    } catch (const utils::exception::IException&) {}
}

std::pair<std::uint64_t, std::string> cluster::Manager::focusRequest(void) const
{
    if (!this->_replica) return {this->_focusVersion, this->_focus};
    const cluster::Json focus = this->replicaState("focus");
    return focus.is_array() && focus.size() == 2 ? std::pair<std::uint64_t, std::string>{focus[0].get<std::uint64_t>(), focus[1].get<std::string>()}
        : std::pair<std::uint64_t, std::string>{0, ""};
}

std::uint64_t cluster::Manager::raiseRequest(void) const
{
    if (!this->_replica) return this->_raiseVersion;
    const cluster::Json raise = this->replicaState("raise");
    return raise.is_number() ? raise.get<std::uint64_t>() : 0;
}

std::uint64_t cluster::Manager::machineRequest(void) const
{
    if (!this->_replica) return this->_machineRequest;
    const cluster::Json request = this->replicaState("machine_request");
    return request.is_number() ? request.get<std::uint64_t>() : 0;
}

_cold void cluster::Manager::answer(const std::string& id, const std::string& requestId, const std::string& behavior)
{
    if (this->_replica) {
        (void)this->call({{"cmd", "allow"}, {"session", id}, {"request_id", requestId}, {"behavior", behavior}});
        return;
    }
    this->get_(id)->answer(requestId, behavior);
}

_cold void cluster::Manager::answerQuestions(const std::string& id, const std::string& requestId, const cluster::Json& answers)
{
    if (this->_replica) {
        (void)this->call({{"cmd", "allow"}, {"session", id}, {"request_id", requestId}, {"answers", answers}});
        return;
    }
    this->get_(id)->answerQuestions(requestId, answers);
}

_cold void cluster::Manager::interrupt(const std::string& id)
{
    if (this->_replica) {
        (void)this->call({{"cmd", "interrupt"}, {"session", id}});
        return;
    }
    this->get_(id)->interrupt();
}

_cold std::optional<cluster::Queued> cluster::Manager::unqueue(const std::string& id)
{
    if (this->_replica) {
        const cluster::Json queued = this->call({{"cmd", "unqueue"}, {"session", id}});
        return queued.is_object() ? std::optional<cluster::Queued>(queued.get<cluster::Queued>()) : std::nullopt;
    }
    return this->get_(id)->unqueue();
}

/* !command: the shell of the user in the folder of a session */
_cold cluster::Json cluster::Manager::shell(const std::string& id, const std::string& command)
{
    if (this->_replica) return this->call({{"cmd", "shell"}, {"session", id}, {"command", command}});
    // no session (the global one off): in the folder of claude-cluster, its output only told
    std::shared_ptr<cluster::Session> session;
    try {
        session = this->get_(this->resolve(id));
    } catch (const utils::exception::IException&) {}
    const std::string cwd = session ? cluster::expand_home(session->spec().cwd).string() : std::filesystem::current_path().string();
    const char* env = std::getenv("SHELL");
    const cluster::Captured out = cluster::capture({env != nullptr ? env : "/bin/sh", "-c", command}, cwd, 300);
    std::string output = out.out;
    if (output.size() > 20000) output = "[...]\n" + cluster::tail_utf8(output, 20000);
    while (!output.empty() && output.back() == '\n') output.pop_back();
    if (!session) return {{"code", out.code}, {"lines", output.empty() ? 0 : std::count(output.begin(), output.end(), '\n') + 1}, {"output", output}, {"shown", false}};
    const std::int64_t now = cluster::now();
    session->append({{cluster::EntryKind::Tool, "$ " + command, "Shell", now},
        {cluster::EntryKind::ToolResult, (output.empty() ? std::string("(no output)") : output) + (out.code == 0 ? "" : "\n[exit " + std::to_string(out.code) + "]"), "Shell", now}});
    // like Claude Code: given to the agent with the next prompt
    session->addContext("<bash-input>" + command + "</bash-input>\n<bash-stdout>" + (output.size() > 6000 ? cluster::tail_utf8(output, 6000) : output)
        + "</bash-stdout>" + (out.code == 0 ? "" : "\n<bash-exit-code>" + std::to_string(out.code) + "</bash-exit-code>"));
    ++this->_version;
    return {{"code", out.code}, {"lines", output.empty() ? 0 : std::count(output.begin(), output.end(), '\n') + 1}, {"output", output}, {"shown", true}};
}
