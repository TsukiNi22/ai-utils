/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Manager-Jobs.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Orchestrator.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Tools.hpp"

/* the resource orchestrator of the core (docs/orchestrator.md) */
_cold void cluster::Manager::startOrchestrator_(void)
{
    if (this->_replica || this->_orch) return;
    cluster::Orchestrator::Hooks hooks;
    hooks.active = [this]() {return this->active();};
    hooks.nameOf = [this](const std::string& id) {
        const std::optional<cluster::Snapshot> snap = this->snapshot(id);
        return snap ? snap->spec.name : id;
    };
    hooks.tell = [this](const std::string& session, const std::string& text) {this->tell_(session, text);};
    hooks.notice = [this](const std::string& text, const bool error) {this->notice_("", text, error);};
    hooks.notify = [this](const std::string& kind, const std::string& title, const std::string& text, const bool urgent) {
        cluster::Notification n;
        n.title = title;
        n.body = text;
        n.urgent = urgent;
        n.face = urgent ? cluster::Face::Attentive : cluster::Face::Happy;
        n.sound = urgent ? "dialog-warning" : "complete";
        this->notify(n, kind == "job" ? "task" : kind); // the notifications of the tasks (their setting)
    };
    hooks.sessions = [this]() {
        std::vector<std::pair<std::string, int>> out;
        for (const cluster::Snapshot& snap: this->list(true))
            if (snap.pid > 1) out.push_back({snap.spec.id, snap.pid});
        return out;
    };
    hooks.changed = [this]() {++this->_version;};
    this->_orch = std::make_unique<cluster::Orchestrator>(this->_config, std::move(hooks));
    this->_orch->start();
}

_cold std::vector<std::pair<std::string, std::string>> cluster::Manager::jobEnv_(const std::string& session) const
{
    // the environment of a session: its id and the socket for cluster-run, and the folder holding it first in the PATH
    const char* path = std::getenv("PATH");
    const std::string bin = cluster::orchestrator_bin_dir().string();
    return {{"CLUSTER_RUN_SESSION", session}, {"CLUSTER_RUN_SOCKET", cluster::socket_path().string()}, {"PATH", bin + ":" + (path ? path : "/usr/local/bin:/usr/bin:/bin")}};
}

_cold cluster::Json cluster::Manager::jobsCommand(const std::string& cmd, const cluster::Json& request)
{
    if (this->_replica) {
        cluster::Json call = request;
        call["cmd"] = cmd;
        return this->call(call);
    }
    if (!this->_orch) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "the orchestrator is not running");
    return this->_orch->handle(cmd, request, {});
}

_cold cluster::Json cluster::Manager::jobsState(void) const
{
    if (this->_replica) return this->replicaState("jobs");
    return this->_orch ? this->_orch->state() : cluster::Json::object();
}

_cold cluster::Json cluster::Manager::runHeavy_(const std::string& command, const std::string& cwd, const int timeout, const std::string& session)
{
    // a command of a schedule: a heavy one waits its turn in the orchestrator (null: not heavy / no orchestrator: the caller runs it)
    if (this->_replica || !this->_orch || !this->_config.orch.enabled || cluster::job_class(command, {}, this->_config.orch.patterns).empty()) return nullptr;
    const std::string owner = session.empty() ? std::string() : session;
    cluster::Json made = this->_orch->handle("job_add", {{"command", command}, {"cwd", cwd}, {"callback", false}, {"session", owner}, {"name", cluster::one_line(command, 40)},
        {"timeout_minutes", std::max(1, timeout / 60)}}, {});
    const std::string id = made.value("id", std::string());
    const std::int64_t end = cluster::now() + std::max(1, timeout);
    cluster::Json state = made;
    while (!state.value("done", false) && state.value("state", std::string()) != "done" && state.value("state", std::string()) != "failed" && state.value("state", std::string()) != "killed" && cluster::now() < end) {
        state = this->_orch->handle("job_wait", {{"id", id}, {"timeout", 20}}, {});
        if (this->_closing) break;
    }
    if (state.value("state", std::string()) == "queued" || state.value("state", std::string()) == "running" || state.value("state", std::string()) == "stopped") { // too long: given up
        (void)this->_orch->handle("job_kill", {{"id", id}, {"reason", "its schedule gave up waiting"}}, {});
        state = this->_orch->handle("job_status", {{"id", id}}, {});
    }
    return {{"exit", state.value("exit", -1)}, {"tail", state.value("tail", std::string())}, {"message", state.value("message", std::string())}};
}
