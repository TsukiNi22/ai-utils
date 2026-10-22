/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 17/10/2026 by @author Tsukini

File Name:
##  @file Manager-Upgrade.cpp

File Description:
##  The hot upgrade: a newer binary installed over the running one (its version higher) offered to the user (Update /
##  Ignore / Quit); Update: the turns running end, the sessions saved, the core runs the new binary again (same pid,
##  same arguments, the sessions resumed), its terminals and windows follow it
\**************************************************************/

#include "cluster/Services.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Tools.hpp"
#include <sys/stat.h>
#include <filesystem>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <chrono>

//----------------------------------------------------------------//
/* TOOLS */

_cold static std::filesystem::path upgrade_file_(void)
{
    return cluster::data_dir() / "upgrade.json"; // {"ignored": "<version>"}
}

//----------------------------------------------------------------//
/* FUNCTION */

_cold void cluster::Manager::upgradeCheck_(void)
{
    // the binary on disk changed (an install): its version read; higher than this one, not ignored: offered
    if (this->_exeStamp == 0) {
        if (const std::optional<cluster::Json> kept = cluster::read_json(upgrade_file_()); kept && kept->is_object()) {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_upgradeIgnored = kept->value("ignored", std::string());
        }
        if (const char* from = std::getenv("CLAUDE_CLUSTER_UPGRADED_FROM"); from != nullptr) { // just updated: said once
            this->notice_("", std::string("claude-cluster updated: ") + from + " -> " + CLUSTER_VERSION + " (the sessions resumed)");
            ::unsetenv("CLAUDE_CLUSTER_UPGRADED_FROM");
        }
    }
    struct stat info{};
    const std::filesystem::path exe = cluster::self_exe();
    if (::stat(exe.c_str(), &info) != 0) return;
    const std::int64_t stamp = static_cast<std::int64_t>(info.st_ino) * 1000003 + static_cast<std::int64_t>(info.st_mtim.tv_sec) * 1000
        + info.st_mtim.tv_nsec / 1000000;
    if (this->_exeStamp == 0) {
        this->_exeStamp = stamp;
        return;
    }
    if (stamp == this->_exeStamp) return;
    const std::string version = cluster::binary_version(exe);
    if (version.empty()) return; // being written (an install): read again at the next check
    this->_exeStamp = stamp;
    std::string ignored;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        ignored = this->_upgradeIgnored;
    }
    if (cluster::version_compare(version, CLUSTER_VERSION) <= 0 || version == ignored) return; // the same, older, or ignored: nothing
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_upgradeVersion = version;
    }
    ++this->_version;
    this->notice_("", "claude-cluster " + version + " is installed (this one: " + CLUSTER_VERSION + "): Update / Ignore / Quit");
}

_cold cluster::UpgradeOffer cluster::Manager::upgradeOffer(void) const
{
    if (this->_replica) {
        const cluster::Json j = this->replicaState("upgrade");
        if (!j.is_object()) return {};
        return {j.value("version", std::string()), j.value("started", false), j.value("upgrading", false)};
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    return {this->_upgradeVersion, this->_upgradeStarted.load(), this->_upgrading.load()};
}

_cold std::string cluster::Manager::upgradeAct(const std::string& action)
{
    // update (the default) | ignore (this version) | quit (everything stopped, to start again by hand)
    if (this->_replica) return this->call({{"cmd", "upgrade"}, {"action", action}}).value("done", std::string());
    std::string version;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        version = this->_upgradeVersion;
    }
    if (version.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no newer claude-cluster installed");
    if (action == "ignore") {
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_upgradeIgnored = version;
            this->_upgradeVersion.clear();
        }
        cluster::write_json(upgrade_file_(), {{"ignored", version}});
        ++this->_version;
        return "claude-cluster " + version + " ignored (a newer one is offered again)";
    }
    if (action == "quit") {
        const std::vector<int> interfaces = cluster::interface_pids();
        std::thread([interfaces]() { // its answer sent first; the sessions saved by the quit of the core
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            for (const int pid: interfaces)
                (void)::kill(pid, SIGTERM);
            (void)::kill(::getpid(), SIGTERM);
        }).detach();
        return "claude-cluster stops: start it again for " + version;
    }
    if (action != "update") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "upgrade: update | ignore | quit");
    if (this->_upgradeStarted.exchange(true)) return "already updating to " + version;
    ++this->_version;
    std::thread([this, version]() {
        // the turns running end first (their sessions named), the sessions saved, then the new binary run in place of this one
        const std::uint64_t id = this->progressBegin("Updating claude-cluster to " + version);
        this->progressStep(id, "trying the new binary first (it must start on its own)");
        const std::filesystem::path exe = cluster::self_exe();
        std::string problem;
        if (cluster::binary_version(exe) != version) problem = "the binary on disk is not " + version + " (an install in progress?)";
        else problem = cluster::upgrade_selftest(exe);
        if (!problem.empty()) { // nothing was stopped: this version goes on, the update offered again
            this->progressEnd(id, false, "update to " + version + " cancelled, still on " + CLUSTER_VERSION + ": " + problem, false);
            this->notice_("", "update to " + version + " cancelled (" + problem + "): claude-cluster " + CLUSTER_VERSION + " goes on", true);
            this->_upgradeStarted = false;
            ++this->_version;
            return;
        }
        for (;;) {
            std::string names;
            for (const cluster::Snapshot& s: this->list(true))
                if (s.state == cluster::State::Working || s.queued > 0) names += (names.empty() ? "" : ", ") + s.spec.name;
            if (names.empty()) break;
            this->progressStep(id, "waiting for the turns running: " + names);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        this->progressAdvance(id, "saving the sessions");
        this->save();
        this->progressAdvance(id, "starting claude-cluster " + version + " (the terminals and windows follow it)");
        this->_upgrading = true; // the interfaces see it: they follow the new core
        ++this->_version;
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        cluster::upgrade_request(true);
        cluster::quit_signal = true; // the normal quit (sessions saved, stopped), then the exec (main)
    }).detach();
    return "updating to " + version + "…";
}
