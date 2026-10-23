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

_cold void cluster::Manager::showcaseUpgrade(void)
{
    if (this->_replica) {
        (void)this->call({{"cmd", "showcase"}, {"what", "upgrade"}});
        return;
    }
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_upgradeShowcase = true;
        this->_upgradeVersion = "9.9.9";
    }
    ++this->_version;
}

/* the prompts held while an update switches */
_cold static std::filesystem::path held_file_(void)
{
    return cluster::data_dir() / "held.json"; // [{session, text, images, origin}]: sent by the next version
}

_cold bool cluster::Manager::hold_(const std::string& id, const std::string& text, const std::vector<std::string>& images, const std::string& origin)
{
    // an update switching: this prompt kept on disk (never lost, never a new turn now), sent by the new version once it runs
    if (!this->_holding) return false;
    cluster::Json held = cluster::read_json(held_file_()).value_or(cluster::Json::array());
    if (!held.is_array()) held = cluster::Json::array();
    held.push_back({{"session", id}, {"text", text}, {"images", images}, {"origin", origin}});
    cluster::write_json(held_file_(), held);
    this->notice_(id, "prompt held: an update is switching, it is sent by the new version");
    ++this->_version;
    return true;
}

_cold void cluster::Manager::holdQueues_(void)
{
    // the prompts waiting in the queue of a turn (not given to the agent yet): held in their order, the turn ends without them
    for (const cluster::Snapshot& s: this->list(true)) {
        std::vector<cluster::Queued> taken;
        while (const std::optional<cluster::Queued> queued = this->unqueue(s.spec.id))
            taken.push_back(*queued);
        for (auto it = taken.rbegin(); it != taken.rend(); ++it)
            (void)this->hold_(s.spec.id, it->text, it->images, it->origin);
    }
}

_cold std::size_t cluster::Manager::heldCount_(void) const
{
    const cluster::Json held = cluster::read_json(held_file_()).value_or(cluster::Json::array());
    return held.is_array() ? held.size() : 0;
}

_cold void cluster::Manager::releaseHeld_(void)
{
    // the version before held these prompts while it switched: sent in their order (a session gone: told, not sent)
    const cluster::Json held = cluster::read_json(held_file_()).value_or(cluster::Json::array());
    std::error_code error;
    std::filesystem::remove(held_file_(), error);
    if (!held.is_array() || held.empty()) return;
    std::size_t sent = 0;
    for (const cluster::Json& item: held) {
        const std::string session = item.value("session", std::string());
        try {
            this->send(session, item.value("text", std::string()), true, item.value("images", std::vector<std::string>()), item.value("origin", std::string()));
            ++sent;
        } catch (const utils::exception::IException& e) {
            this->notice_(session, "a prompt held for the update not sent (" + std::string(e.info()) + "): " + cluster::one_line(item.value("text", std::string()), 120), true);
        }
    }
    this->notice_("", std::to_string(sent) + " prompt(s) held during the update sent");
}

_cold std::string cluster::Manager::upgradeAct(const std::string& action)
{
    // update (the default) | ignore (this version) | quit (everything stopped, to start again by hand)
    if (this->_replica) return this->call({{"cmd", "upgrade"}, {"action", action}}).value("done", std::string());
    if ((action == "hold" || action == "release") && std::getenv("CLAUDE_CLUSTER_TEST") != nullptr) { // the tests: the hold of an update without a new binary
        if (action == "hold") {
            this->_holding = true;
            this->holdQueues_();
            return std::to_string(this->heldCount_()) + " held";
        }
        this->_holding = false;
        this->releaseHeld_();
        return "released";
    }
    if (this->_upgradeShowcase) { // the Debug page's pretend update: the popup closes, a progress popup plays, nothing is installed or stopped
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_upgradeShowcase = false;
            this->_upgradeVersion.clear();
        }
        ++this->_version;
        if (action == "update") {
            std::thread([this]() {
                const std::uint64_t id = this->progressBegin("Updating claude-cluster to 9.9.9 (showcase)");
                for (const char* step: {"trying the new binary first", "waiting for the turns running", "saving the sessions", "starting claude-cluster 9.9.9"}) {
                    this->progressStep(id, step);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
                }
                this->progressEnd(id, true, "showcase: nothing was installed or stopped", false);
            }).detach();
        }
        return "showcase: " + action + " (nothing installed or stopped)";
    }
    std::string version;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        version = this->_upgradeVersion;
    }
    if (version.empty()) {
        const std::string installed = cluster::binary_version(cluster::self_exe());
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, std::string("no newer claude-cluster installed (running ") + CLUSTER_VERSION
            + (installed.empty() ? std::string(", installed: unknown") : ", installed " + installed) + ")");
    }
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
        // from now on nothing starts a turn: the prompts, the queued ones, the tasks, the schedules and the wake-ups are held (held.json) and
        // sent by the new version; only the turns running are waited for (before: their queue started new ones, the update never came)
        this->_holding = true;
        this->holdQueues_();
        for (;;) {
            std::string names;
            for (const cluster::Snapshot& s: this->list(true))
                if (s.state == cluster::State::Working || s.state == cluster::State::Waiting)
                    names += (names.empty() ? "" : ", ") + s.spec.name + (s.state == cluster::State::Waiting ? " (waits for your answer)" : "");
            if (names.empty()) break;
            const std::size_t held = this->heldCount_();
            this->progressStep(id, "waiting for the turns running: " + names + (held > 0 ? " · " + std::to_string(held) + " prompt(s) held, sent after the update" : std::string()));
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
