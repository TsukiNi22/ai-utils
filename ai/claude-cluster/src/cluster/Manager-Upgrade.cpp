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
#include "cluster/Notes.hpp"
#include "cluster/Web.hpp"
#include <regex>
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

_cold static std::string jump_(const std::string& from, const std::string& to)
{
    // the size of the step: its major, minor or patch number changed
    const std::vector<std::string> a = cluster::split(from, '.');
    const std::vector<std::string> b = cluster::split(to, '.');
    if (a.empty() || b.empty() || a[0] != b[0]) return "major";
    if (a.size() < 2 || b.size() < 2 || a[1] != b[1]) return "minor";
    return "patch";
}

_cold static std::vector<std::string> notes_of_(const std::filesystem::path& exe)
{
    // what the binary on disk brings over this one: its own notes (--notes), read before it is run for real
    const cluster::Captured got = cluster::capture({exe.string(), "--notes", CLUSTER_VERSION}, "", 10);
    std::vector<std::string> notes;
    if (got.code != 0) return notes;
    for (const std::string& line: cluster::split(got.out, '\n'))
        if (!cluster::trim(line).empty() && notes.size() < 40) notes.push_back(cluster::clean_utf8(cluster::trim(line)));
    return notes;
}

_cold std::vector<std::string> cluster::release_notes(const std::string& since)
{
    // the subjects of the commits embedded at build time: "type(claude-cluster): 1.4.103 - what it does"
    static const std::regex line(R"(^(\w+)\(claude-cluster\): (\d+\.\d+\.\d+) - (.*)$)");
    std::vector<std::string> notes;
    for (const std::string& subject: cluster::split(CLUSTER_NOTES, '\n')) {
        std::smatch m;
        if (!std::regex_match(subject, m, line)) continue;
        if (!since.empty() && cluster::version_compare(m[2].str(), since) <= 0) continue;
        if (cluster::version_compare(m[2].str(), CLUSTER_VERSION) > 0) continue; // a commit after this build: not in it
        notes.push_back(m[1].str() + " " + m[2].str() + " " + cluster::one_line(m[3].str(), 400));
    }
    return notes;
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
    std::vector<std::string> notes = notes_of_(exe);
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_upgradeVersion = version; // an update waiting meanwhile: offered again for this one (Update moves it here)
        this->_upgradeNotes = std::move(notes);
        this->_upgradeJump = jump_(CLUSTER_VERSION, version);
    }
    ++this->_version;
    this->notice_("", "claude-cluster " + version + " is installed (this one: " + CLUSTER_VERSION + (this->_upgradeStarted ? ", an update already waits" : "")
        + "): Update / Ignore / Quit");
}

_cold cluster::UpgradeOffer cluster::Manager::upgradeOffer(void) const
{
    if (this->_replica) {
        const cluster::Json j = this->replicaState("upgrade");
        if (!j.is_object()) return {};
        return {j.value("version", std::string()), j.value("started", false), j.value("upgrading", false), j.value("target", std::string()), j.value("waiting", std::string()),
            j.value("jump", std::string()), j.value("notes", std::vector<std::string>())};
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    return {this->_upgradeVersion, this->_upgradeStarted.load(), this->_upgrading.load(), this->_upgradeTarget, this->_upgradeWaiting, this->_upgradeJump, this->_upgradeNotes};
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

/* what is on for this run, across the switch */
_cold static std::filesystem::path keep_file_(void)
{
    return cluster::data_dir() / "upgrade-keep.json"; // {web, background, web_before_background, at}: read once by the new version
}

_cold void cluster::Manager::upgradeKeep_(void)
{
    // the switch keeps the config and the sessions; what was set for this run only (the web access opened or closed by hand, the core
    // kept in background with no interface) is written here, the new version sets it again at its start
    cluster::Json keep = {{"at", cluster::now()}, {"background", this->_background}};
    if (this->_web != nullptr) keep["web"] = this->_web->status().on;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        keep["web_before_background"] = this->_webBeforeBackground;
    }
    cluster::write_json(keep_file_(), keep);
}

_cold void cluster::Manager::upgradeKept(cluster::Web* web)
{
    // just updated (the version before wrote it a moment ago): the web access as it was, the background as it was
    const std::optional<cluster::Json> keep = cluster::read_json(keep_file_());
    std::error_code error;
    std::filesystem::remove(keep_file_(), error);
    if (!keep || !keep->is_object() || cluster::now() - keep->value("at", std::int64_t{0}) > 600) return; // an old file: not this switch
    std::vector<std::string> back;
    if (web != nullptr && keep->value("web", cluster::Json()).is_boolean()) {
        web->setOn(keep->value("web", false));
        back.push_back(std::string("the web access ") + (keep->value("web", false) ? "open" : "closed"));
    }
    if (keep->value("background", false)) { // no interface came back: it still stops once every session is idle
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_background = true;
        this->_backgroundSince = cluster::now();
        this->_webBeforeBackground = keep->value("web_before_background", false);
        back.push_back("kept in background");
    }
    if (!back.empty()) {
        std::string text;
        for (const std::string& part: back)
            text += (text.empty() ? "" : ", ") + part;
        this->notice_("", "as before the update: " + text);
    }
}

_cold std::string cluster::Manager::upgradeAct(const std::string& action)
{
    // update (the default) | ignore (this version) | quit (everything stopped, to start again by hand)
    if (this->_replica) return this->call({{"cmd", "upgrade"}, {"action", action}}).value("done", std::string());
    if ((action == "hold" || action == "release" || action == "keep") && std::getenv("CLAUDE_CLUSTER_TEST") != nullptr) { // the tests: an update without a new binary
        if (action == "keep") {
            this->upgradeKeep_();
            return "kept";
        }
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
    if (this->_upgradeStarted.exchange(true)) { // one waits already: never a second one, it goes to this version (checked again before the switch)
        std::string before;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            before = this->_upgradeTarget;
            this->_upgradeTarget = version;
        }
        ++this->_version;
        return before == version ? "already updating to " + version : "the update waiting now goes to " + version + " (instead of " + before + ")";
    }
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_upgradeTarget = version;
    }
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
            const std::int64_t now = cluster::now_ms();
            for (const cluster::Snapshot& s: this->list(true)) {
                const bool agents = std::any_of(s.agents.begin(), s.agents.end(), [now](const cluster::SubAgent& a) {
                    return a.status == "running" && now - a.started < 3600 * 1000; // its agents in the background too (an hour at most)
                });
                if (s.state == cluster::State::Working || s.state == cluster::State::Waiting || agents)
                    names += (names.empty() ? "" : ", ") + s.spec.name + (s.state == cluster::State::Waiting ? " (waits for your answer)" : agents && s.state != cluster::State::Working ? " (its agents)" : "");
            }
            const std::size_t held = this->heldCount_();
            {
                std::lock_guard<std::mutex> lock(this->_mutex);
                this->_upgradeWaiting = names.empty() ? std::string() : names + (held > 0 ? " · " + std::to_string(held) + " prompt(s) held" : std::string());
            }
            if (names.empty()) break;
            this->progressStep(id, "waiting for the turns running: " + names + (held > 0 ? " · " + std::to_string(held) + " prompt(s) held, sent after the update" : std::string()));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        // the binary switched to: the one on disk now (a newer one installed meanwhile), tested again; a problem: nothing switched, what was held sent
        std::string target;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            target = this->_upgradeTarget;
        }
        const std::string onDisk = cluster::binary_version(exe);
        std::string late = onDisk.empty() || cluster::version_compare(onDisk, CLUSTER_VERSION) <= 0 ? "the binary on disk is no longer a newer version" : std::string();
        if (late.empty() && onDisk != version) late = cluster::upgrade_selftest(exe);
        if (!late.empty()) {
            this->_holding = false;
            this->releaseHeld_();
            this->progressEnd(id, false, "update cancelled, still on " + std::string(CLUSTER_VERSION) + ": " + late, false);
            this->notice_("", "update cancelled (" + late + "): claude-cluster " + CLUSTER_VERSION + " goes on, the held prompts sent", true);
            this->_upgradeStarted = false;
            ++this->_version;
            return;
        }
        if (onDisk != target) this->progressStep(id, "a newer version is on disk: switching to " + onDisk);
        this->progressAdvance(id, "saving the sessions");
        this->save();
        this->upgradeKeep_(); // the web access opened by hand, the background: on again in the new version
        this->progressAdvance(id, "starting claude-cluster " + onDisk + " (the terminals and windows follow it)");
        this->_upgrading = true; // the interfaces see it: they follow the new core
        ++this->_version;
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        cluster::upgrade_request(true);
        cluster::quit_signal = true; // the normal quit (sessions saved, stopped), then the exec (main)
    }).detach();
    return "updating to " + version + "…";
}
