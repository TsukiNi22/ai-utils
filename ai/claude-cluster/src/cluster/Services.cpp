/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Services.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Services.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Service.hpp"
#include "cluster/Control.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Voice.hpp"
#include "cluster/Web.hpp"
#include <csignal>
#include <fstream>
#include <thread>
#include <set>

#define SERVICE_UNIT "claude-cluster.service"

// the slow parts (systemd, its journal, tailscale): kept [services] delay seconds
static std::mutex slow_mutex_;
static std::int64_t slow_at_ = 0;
static cluster::Json slow_;

_cold static cluster::Json slow_state_(cluster::Manager& manager)
{
    const std::int64_t keep = static_cast<std::int64_t>(std::max(1, manager.config().services.delay)) * 1000;
    {
        std::lock_guard<std::mutex> lock(slow_mutex_);
        if (!slow_.is_null() && cluster::now_ms() - slow_at_ < keep) return slow_;
    }
    cluster::Json out;
    const bool installed = std::filesystem::exists(cluster::service_unit());
    out["installed"] = installed;
    out["active"] = cluster::trim(cluster::capture({"systemctl", "--user", "is-active", SERVICE_UNIT}, "", 5).out);
    out["enabled"] = cluster::trim(cluster::capture({"systemctl", "--user", "is-enabled", SERVICE_UNIT}, "", 5).out);
    std::string unit;
    if (installed) {
        std::ifstream file(cluster::service_unit());
        unit.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    out["debug"] = unit.find("service run --debug") != std::string::npos;
    // its journal: the last lines, their time and message ("2026-10-15T20:31:02+0200 host claude-cluster[12]: service: ...")
    cluster::Json log = cluster::Json::array();
    if (installed)
        for (const std::string& line: cluster::split(cluster::capture({"journalctl", "--user", "-u", SERVICE_UNIT, "--no-pager", "-n", "6", "-o", "short-iso"}, "", 5).out, '\n')) {
            if (cluster::trim(line).empty() || line.starts_with("-- ")) continue;
            const std::size_t t = line.find('T');
            const std::size_t colon = line.find(": ", line.find(' ') == std::string::npos ? 0 : line.find(' '));
            log.push_back((t != std::string::npos && t + 6 <= line.size() ? line.substr(t + 1, 5) + " " : std::string()) + (colon == std::string::npos ? line : line.substr(colon + 2)));
        }
    out["log"] = log;
    // tailscale: missing, not connected, connected (its name)
    std::string tailscale = "missing";
    if (cluster::has_command("tailscale")) {
        const cluster::Json status = cluster::Json::parse(cluster::capture({"tailscale", "status", "--json"}, "", 5).out, nullptr, false);
        tailscale = status.is_object() && status.value("BackendState", std::string()) == "Running" ? "connected" : "not connected";
    }
    out["tailscale"] = tailscale;
    out["upnpc"] = cluster::has_command("upnpc");
    std::lock_guard<std::mutex> lock(slow_mutex_);
    slow_ = out;
    slow_at_ = cluster::now_ms();
    return out;
}

_cold static void slow_forget_(void)
{
    // an action done: read again at once
    std::lock_guard<std::mutex> lock(slow_mutex_);
    slow_at_ = 0;
}

_cold cluster::Json cluster::services(cluster::Manager& manager)
{
    if (manager.replica()) return manager.call({{"cmd", "services"}});
    const cluster::Config& c = manager.config();
    const cluster::Json slow = slow_state_(manager);
    cluster::Json out;
    // service (systemd): its detector paused while claude-cluster runs (the microphone free for the voice of the core)
    const bool installed = slow.value("installed", false);
    const std::string active = slow.value("active", std::string());
    out["service"] = {{"installed", installed}, {"active", active.empty() ? "inactive" : active}, {"enabled", slow.value("enabled", std::string())},
        {"debug", slow.value("debug", false)}, {"log", slow.value("log", cluster::Json::array())},
        {"wake", c.service.wakeWord.empty() ? c.voice.wakeWord : c.service.wakeWord},
        {"detector", !installed ? "not installed (Setup writes it, Enable starts it with your session)" : active != "active" ? "stopped"
            : "paused: claude-cluster runs (the microphone is the voice's of the core); it listens for the wake phrase again once claude-cluster is closed"}};
    // web
    const std::optional<cluster::WebStatus> web = cluster::web_status(manager);
    std::size_t connected = 0;
    std::size_t waiting = 0;
    if (web)
        for (const cluster::WebDevice& d: web->devices) {
            connected += d.state == "connected" ? 1 : 0;
            waiting += d.state == "waiting" ? 1 : 0;
        }
    out["web"] = {{"on", web && web->on}, {"url", web ? web->url : ""}, {"pin", web ? web->pin : ""}, {"error", web ? web->error : ""},
        {"remote_url", web ? web->remoteUrl : ""}, {"remote", web ? web->remoteState : ""}, {"remote_ok", web && web->remoteOk},
        {"connected", connected}, {"waiting", waiting}, {"trusted", web ? web->trusted.size() : 0}, {"at_start", c.web.enabled}, {"mode", c.web.remote},
        {"https", web && web->https}, {"certificate", web ? web->certificate : ""}, {"certificate_why", web ? web->certificateWhy : ""},
        {"fingerprint", web ? web->fingerprint : ""}};
    // core
    out["core"] = {{"pid", ::getpid()}, {"since", manager.startedAt()}, {"version", CLUSTER_VERSION}, {"build", cluster::build_id()},
        {"interfaces", cluster::interface_pids().size()}, {"sessions", manager.list(false).size()}, {"shared", c.sharedCore}};
    // voice (of the core): what it hears now (what is being said, the transcriptions waiting)
    cluster::Voice* voice = manager.voice();
    cluster::Json heard = cluster::Json::array();
    if (voice != nullptr)
        for (const cluster::Pending& p: voice->pending())
            heard.push_back(p.text);
    out["voice"] = {{"enabled", c.voice.enabled}, {"mode", c.voice.mode}, {"running", voice != nullptr && voice->running()},
        {"listening", voice != nullptr && voice->listening()}, {"speaking", voice != nullptr && voice->speaking()}, {"heard", heard},
        {"missing", voice != nullptr ? cluster::Json(voice->setup()) : cluster::Json::array()}, {"engine", c.voice.listenCommand}};
    // mcp: the servers given to the global session, its state
    const std::optional<cluster::Snapshot> global = manager.snapshot(GLOBAL_ID);
    out["mcp"] = {{"servers", c.mcpServers}, {"global", global ? cluster::state_name(global->state) : std::string("off")}};
    // outside: tailscale / the router
    out["outside"] = {{"mode", c.web.remote}, {"upnpc", slow.value("upnpc", false)}, {"tailscale", slow.value("tailscale", std::string("missing"))},
        {"remote_ok", web && web->remoteOk}, {"remote_url", web ? web->remoteUrl : ""}, {"why", web ? web->remoteState : ""},
        {"port", c.web.remotePort > 0 ? c.web.remotePort : c.web.port}};
    out["refresh"] = c.services.refresh;
    out["delay"] = c.services.delay;
    return out;
}

_cold static std::string run_service_(const std::string& action)
{
    // claude-cluster service <action>: its output under the popup, its last line told when it fails
    std::string last;
    const int code = cluster::run_logged({cluster::self_exe().string(), "service", action}, [&last](const std::string& line) {
        cluster::busy_log(line);
        if (!cluster::trim(line).empty()) last = cluster::trim(line);
    });
    if (code != 0) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "claude-cluster service " + action + ": "
        + (last.empty() ? "exit " + std::to_string(code) : last));
    return "service " + action + ": done" + (last.empty() ? "" : " (" + cluster::one_line(last, 80) + ")");
}

_cold static std::string act_(cluster::Manager& manager, const std::string& service, const std::string& action)
{
    const cluster::Config& c = manager.config();
    const std::function<void(bool)> need = [&service, &action](const bool ok) {
        if (!ok) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown action " + action + " of " + service);
    };
    if (service == "service") {
        static const std::set<std::string> allowed = {"setup", "enable", "disable", "start", "stop", "restart", "remove"};
        need(allowed.contains(action));
        return run_service_(action);
    }
    if (service == "web" || service == "outside") {
        cluster::Web* web = manager.web();
        if (web == nullptr) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no web access in this core");
        if (action == "on" || action == "off") {
            web->setOn(action == "on");
            web->tick(); // applied now (its opening: its steps)
            const cluster::WebStatus status = web->status();
            if (action == "on" && !status.on) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not opened: " + status.error);
            return action == "on" ? "web access open: " + status.url : "web access closed";
        }
        if (action == "renew") {
            web->renewPin();
            return "a new code: " + web->status().pin;
        }
        if (action == "reopen") { // the router asked again (tailscale read again)
            web->reopen();
            web->tick();
            const cluster::WebStatus status = web->status();
            if (c.web.remote != "off" && !status.remoteOk) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, status.remoteState);
            return status.remoteOk ? "reached from outside: " + status.remoteUrl : "web access opened again";
        }
        if (action == "unmap") { // the redirection of the router removed (UPnP)
            const std::string port = std::to_string(c.web.remotePort > 0 ? c.web.remotePort : c.web.port);
            cluster::busy_step("upnpc -d " + port + " TCP");
            const int code = cluster::run_logged({"upnpc", "-d", port, "TCP"}, [](const std::string& line) {cluster::busy_log(line);});
            if (code != 0) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "upnpc -d " + port + ": exit " + std::to_string(code));
            return "the redirection of the port " + port + " removed (Beyond the LAN: off, or it comes back at the next opening)";
        }
        if (action == "tailscale_operator") { // this user may ask tailscale for its certificates (the password window), then got
            const std::string user = std::getenv("USER") != nullptr ? std::getenv("USER") : "";
            if (user.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no USER: run sudo tailscale set --operator=<you>");
            cluster::busy_step("pkexec tailscale set --operator=" + user);
            std::string last;
            const int code = cluster::run_logged({"pkexec", "tailscale", "set", "--operator=" + user}, [&last](const std::string& line) {
                cluster::busy_log(line);
                if (!cluster::trim(line).empty()) last = cluster::trim(line);
            });
            if (code != 0) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "tailscale set --operator: "
                + (last.empty() ? "exit " + std::to_string(code) : last));
            web->reopen();
            web->tick(); // its certificate got now
            const cluster::WebStatus status = web->status();
            if (!status.certificateWhy.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, status.certificateWhy);
            return status.certificate == "tailscale" ? "the real certificate of the tailnet served: no warning" : "allowed";
        }
        if (action == "tailscale_up") { // this computer logged in to its tailnet (the password window)
            cluster::busy_step("pkexec tailscale up");
            std::string last;
            const int code = cluster::run_logged({"pkexec", "tailscale", "up"}, [&last](const std::string& line) {
                cluster::busy_log(line);
                if (!cluster::trim(line).empty()) last = cluster::trim(line);
            });
            if (code != 0) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "tailscale up: " + (last.empty() ? "exit " + std::to_string(code) : last));
            return "tailscale up: done";
        }
        need(false);
    }
    if (service == "core") {
        if (!c.sharedCore) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction,
            "the core is this interface (Several terminals / windows at once: off): close it and open claude-cluster again");
        need(action == "restart" || action == "stop");
        const std::vector<int> interfaces = cluster::interface_pids();
        std::thread([action, interfaces]() {
            // its answer sent first; stop: its interfaces too (else they start a core again)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (action == "stop")
                for (const int pid: interfaces)
                    (void)::kill(pid, SIGTERM);
            (void)::kill(::getpid(), SIGTERM);
        }).detach();
        return action == "restart" ? "the core restarts: its terminals / windows start it again, the sessions resumed" : "claude-cluster stops (the core and its terminals / windows)";
    }
    if (service == "voice") {
        cluster::Voice* voice = manager.voice();
        if (voice == nullptr) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no voice in this core (Voice: off)");
        if (action == "listen") {
            voice->start();
            return "the voice listens";
        }
        if (action == "stop") {
            voice->stop();
            return "the voice stopped";
        }
        if (action == "mute") {
            voice->mute();
            return "muted";
        }
        need(false);
    }
    if (service == "mcp") {
        need(action == "restart_global");
        manager.restartGlobal();
        return "the global session started again (its MCP servers loaded again)";
    }
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown service " + service + " (service, web, core, voice, mcp, outside)");
}

_cold std::string cluster::service_act(cluster::Manager& manager, const std::string& service, const std::string& action)
{
    // its popup while it takes time, its failure (OK / Retry) when an interface asked it
    if (manager.replica()) return manager.call({{"cmd", "service_act"}, {"service", service}, {"action", action}, {"ui", true}}).value("message", std::string());
    const std::string title = (service == "service" ? std::string("Service (systemd)") : service == "web" ? std::string("Web access") : service == "core" ? std::string("Core")
        : service == "voice" ? std::string("Voice") : service == "mcp" ? std::string("MCP") : std::string("Outside")) + ": " + action;
    cluster::Busy busy(manager, title);
    try {
        const std::string message = act_(manager, service, action);
        slow_forget_();
        busy.end(true, message);
        return message;
    } catch (const utils::exception::IException& e) {
        const std::string error = std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info());
        busy.end(false, error); // its ✗, then its failure popup
        slow_forget_();
        if (cluster::failure_popups()) (void)manager.fail(title, service == "service" ? "systemctl --user (" SERVICE_UNIT ")" : service, error, [&manager, service, action]() {
            (void)cluster::service_act(manager, service, action);
        });
        throw;
    }
}

/* its card */
_cold static cluster::Json part_(const std::string& name, const cluster::Json& all)
{
    return all.is_object() && all.contains(name) && all[name].is_object() ? all[name] : cluster::Json::object();
}

_cold std::string cluster::service_summary(const std::string& name, const cluster::Json& all)
{
    if (!all.is_object()) return "reading…";
    if (all.contains("error")) return "✗ " + all.value("error", std::string());
    const cluster::Json s = part_(name, all);
    if (name == "service") return !s.value("installed", false) ? "not installed" : s.value("active", std::string()) + " · " + s.value("enabled", std::string())
        + (s.value("debug", false) ? " · debug" : "");
    if (name == "web") return s.value("on", false) ? "open · " + std::to_string(s.value("connected", 0)) + " connected"
        + (s.value("waiting", 0) > 0 ? " · " + std::to_string(s.value("waiting", 0)) + " waiting" : "") : std::string("closed");
    if (name == "core") {
        if (!s.value("shared", true)) return "in this interface";
        const std::int64_t up = cluster::now() - s.value("since", cluster::now());
        return "pid " + std::to_string(s.value("pid", 0)) + " · up " + (up < 3600 ? std::to_string(up / 60) + "m" : std::to_string(up / 3600) + "h" + std::to_string(up % 3600 / 60))
            + " · " + std::to_string(s.value("interfaces", 0)) + " interface(s) · " + std::to_string(s.value("sessions", 0)) + " session(s)";
    }
    if (name == "voice") return !s.value("enabled", false) ? std::string("off (Voice > enabled)") : s.value("listening", false) ? std::string("listening")
        : s.value("running", false) ? "on (" + s.value("mode", std::string()) + ")" : std::string("stopped");
    if (name == "mcp") return std::to_string(s.value("servers", cluster::Json::array()).size()) + " server(s) · global session " + s.value("global", std::string());
    if (name == "outside") {
        const std::string mode = s.value("mode", std::string("off"));
        return mode == "off" ? std::string("off (LAN only)") : mode + (s.value("remote_ok", false) ? " · reached" : " · not reached ✕");
    }
    return "";
}

_cold std::vector<cluster::ServiceLine> cluster::service_lines(const std::string& name, const cluster::Json& all)
{
    // its details, its journal (log), what the voice hears now (heard), what is wrong (error)
    const cluster::Json s = part_(name, all);
    std::vector<cluster::ServiceLine> out;
    if (name == "service") {
        out.push_back({s.value("detector", std::string()), "muted"});
        if (s.value("installed", false)) out.push_back({"wake phrase: " + s.value("wake", std::string()), "muted"});
        for (const cluster::Json& l: s.value("log", cluster::Json::array()))
            out.push_back({"› " + l.get<std::string>(), "log"});
    } else if (name == "web") {
        if (s.value("on", false)) {
            out.push_back({s.value("url", std::string()) + "   code " + s.value("pin", std::string()), "text"});
            if (s.value("mode", std::string("off")) != "off")
                out.push_back(s.value("remote_ok", false) ? cluster::ServiceLine{"outside: " + s.value("remote_url", std::string()), "ok"}
                    : cluster::ServiceLine{"outside ✕ " + s.value("remote", std::string()), "error"});
        } else {
            out.push_back({std::string("closed") + (s.value("at_start", false) ? " (opened at start)" : " (Web > Open the web access at start: off)"), "muted"});
        }
        if (s.value("on", false)) {
            const std::string cert = s.value("certificate", std::string());
            if (cert == "tailscale") out.push_back({"certificate: the real one of the tailnet (Let's Encrypt): no warning, nothing to install; the LAN address: "
                "the authority of this computer", "ok"});
            else if (cert == "authority") out.push_back({"certificate: the authority of this computer - trusted once per device (the page: Download the certificate): "
                "no warning, nobody else can make one · SHA-256 " + s.value("fingerprint", std::string()).substr(0, 23) + "…", "muted"});
            else out.push_back({"no https (Web › HTTPS off): tailscale still encrypts, the LAN does not; the microphone of the page off", "warn"});
            if (!s.value("certificate_why", std::string()).empty()) out.push_back({"✗ the real certificate of the tailnet: " + s.value("certificate_why", std::string()), "error"});
        }
        if (!s.value("error", std::string()).empty()) out.push_back({"✗ " + s.value("error", std::string()), "error"});
    } else if (name == "core") {
        out.push_back({"claude-cluster " + s.value("version", std::string()) + " · build " + s.value("build", std::string()), "muted"});
        if (!s.value("shared", true)) out.push_back({"the core is this interface (Interface > Several terminals / windows at once: off): Restart / Stop all not offered", "warn"});
    } else if (name == "voice") {
        const cluster::Json heard = s.value("heard", cluster::Json::array());
        if (heard.empty()) out.push_back({s.value("listening", false) ? "(nothing heard now)" : "(not listening)", "log"});
        for (const cluster::Json& h: heard)
            out.push_back({"» " + h.get<std::string>(), "heard"});
        for (const cluster::Json& m: s.value("missing", cluster::Json::array()))
            out.push_back({"✗ missing: " + m.get<std::string>(), "error"});
    } else if (name == "mcp") {
        const cluster::Json servers = s.value("servers", cluster::Json::array());
        if (servers.empty()) out.push_back({"none (MCP > Custom MCP servers)", "log"});
        for (const cluster::Json& server: servers)
            out.push_back({"• " + server.get<std::string>(), "text"});
    } else if (name == "outside") {
        if (s.value("mode", std::string("off")) != "off" && !s.value("remote_ok", false)) out.push_back({s.value("why", std::string()), "error"});
        out.push_back({"upnpc: " + std::string(s.value("upnpc", false) ? "installed" : "missing") + " · tailscale: " + s.value("tailscale", std::string()) + " · port outside "
            + std::to_string(s.value("port", 0)), "log"});
    }
    return out;
}

_cold std::vector<std::pair<std::string, std::string>> cluster::service_buttons(const std::string& name, const cluster::Json& all)
{
    const cluster::Json s = part_(name, all);
    using Button = std::pair<std::string, std::string>;
    if (name == "service") return {{"Start", "start"}, {"Stop", "stop"}, {"Restart", "restart"}, s.value("enabled", std::string()) == "enabled" ? Button{"Disable", "disable"}
        : Button{"Enable", "enable"}, {"Setup", "setup"}, {"Remove", "remove"}};
    if (name == "web") return {s.value("on", false) ? Button{"Close", "off"} : Button{"Open", "on"}, {"New code", "renew"}, {"Ask the router again", "reopen"}};
    if (name == "core") return s.value("shared", true) ? std::vector<Button>{{"Restart", "restart"}, {"Stop all", "stop"}} : std::vector<Button>{};
    if (name == "voice") return {s.value("listening", false) ? Button{"Stop", "stop"} : Button{"Listen", "listen"}, {"Mute", "mute"}};
    if (name == "mcp") return {{"Restart the global session", "restart_global"}};
    if (name == "outside") return {{"Ask the router again", "reopen"}, {"Remove the redirection", "unmap"}, {"Tailscale up", "tailscale_up"},
        {"Allow tailscale certificates", "tailscale_operator"}};
    return {};
}
