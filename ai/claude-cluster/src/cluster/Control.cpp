/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Control.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _Encapsulation
#include <utils/utils.hpp>
#include "cluster/VoiceEngines.hpp"
#include "cluster/Orchestrator.hpp"
#include "cluster/Attachments.hpp"
#include "cluster/External.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Services.hpp"
#include "cluster/Showcase.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Control.hpp"
#include "cluster/Docs.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Service.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Display.hpp"
#include "cluster/Policy.hpp"
#include "cluster/Editor.hpp"
#include "cluster/Stats.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Wire.hpp"
#include "cluster/Web.hpp"
#include "cluster/Simple.hpp"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <algorithm>
#include <sys/un.h>
#include <unistd.h>
#include <iterator>
#include <fstream>
#include <cstring>
#include <chrono>
#include <thread>
#include <array>

/* tools */
_cold static sockaddr_un address_(const std::filesystem::path& path)
{
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.string().size() >= sizeof(addr.sun_path))
        throw utils::exception::ErrorException(utils::exception::InternalCode::SocketInit, "socket path too long: " + path.string());
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    return addr;
}

_cold static int connect_(const std::filesystem::path& path)
{
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); // xstyle: ignore LU-SOCKET (unix socket: libutils only has TCP)
    if (fd == -1) return -1;
    const sockaddr_un addr = address_(path);
    if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1) {
        ::close(fd);
        return -1;
    }
    return fd;
}

_cold int cluster::Control::corePid(void)
{
    // the process at the other end of the socket (this user's: a socket of the runtime folder, 0700)
    const int fd = connect_(cluster::socket_path());
    if (fd == -1) return -1;
    ucred credentials{};
    socklen_t size = sizeof(credentials);
    const int pid = ::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &size) == 0 ? credentials.pid : -1;
    ::close(fd);
    return pid;
}

_cold static bool send_line_(const int fd, const std::string& line)
{
    const std::string data = line + "\n";
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t size = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (size <= 0) return false;
        sent += static_cast<std::size_t>(size);
    }
    return true;
}

_cold static std::optional<std::string> read_line_(const int fd, std::string& buffer, const int timeout, bool& closed)
{
    utils::encapsulation::Poll poll;
    poll.link(fd, EPOLLIN);
    std::array<char, 8192> chunk{};
    closed = false;
    while (buffer.find('\n') == std::string::npos) {
        if (poll.wait(timeout).empty()) return std::nullopt;
        const ssize_t size = ::recv(fd, chunk.data(), chunk.size(), 0);
        if (size <= 0) {
            closed = true;
            return std::nullopt;
        }
        buffer.append(chunk.data(), static_cast<std::size_t>(size));
    }
    const std::size_t nl = buffer.find('\n');
    std::string line = buffer.substr(0, nl);
    buffer.erase(0, nl + 1);
    return line;
}

_cold static std::string arg_(const cluster::Json& request, const char* key, const std::string& fallback = "")
{
    if (!request.contains(key) || request[key].is_null()) return fallback;
    return request[key].is_string() ? request[key].get<std::string>() : request[key].dump(-1, ' ', false, cluster::JSON_LENIENT);
}

#define WEB_READ_ONLY "read only from the web page: the computer allows it in its settings (Web > Settings from the page)"

/* the heavy jobs (Orchestrator.hpp): the commands, and the wrapper cluster-run whose connection is its job's until it ends */
_cold static bool job_command_(const std::string& cmd)
{
    return cmd.starts_with("job_") || cmd == "heavy_run" || cmd == "hardware_scan" || cmd.starts_with("orch_");
}

_cold static bool job_serve_(cluster::Manager& manager, const int fd, const std::string& line)
{
    cluster::Json request;
    try {
        request = cluster::Json::parse(line);
    } catch (const cluster::Json::exception&) {
        return false;
    }
    if (request.value("cmd", std::string()) != "job_run") return false;
    if (manager.orchestrator() == nullptr) {
        (void)send_line_(fd, cluster::Json{{"ok", false}, {"error", "the orchestrator is not running here"}}.dump(-1, ' ', false, cluster::JSON_LENIENT));
        return true;
    }
    manager.orchestrator()->serve(fd, request);
    return true;
}

/* constructor */
_cold cluster::Control::Control(cluster::Manager& manager)
    : _manager(manager), _path(cluster::socket_path())
{
}

_cold cluster::Control::~Control()
{
    this->stop();
}

/* server */
_cold bool cluster::Control::alive(void)
{
    const int fd = connect_(cluster::socket_path());
    if (fd == -1) return false;
    ::close(fd);
    return true;
}

_cold void cluster::Control::start(void)
{
    std::error_code error;
    std::filesystem::create_directories(this->_path.parent_path(), error);
    ::chmod(this->_path.parent_path().c_str(), S_IRWXU);
    if (cluster::Control::alive())
        throw utils::exception::ErrorException(utils::exception::InternalCode::AlreadyRunning,
            "claude-cluster is already running (" + this->_path.string() + "): use the headless commands (claude-cluster list...)");
    std::filesystem::remove(this->_path, error); // left by a crash

    this->_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); // xstyle: ignore LU-SOCKET (unix socket: libutils only has TCP)
    if (this->_fd == -1) throw utils::exception::ErrorException(utils::exception::InternalCode::Socket, std::strerror(errno));
    const sockaddr_un addr = address_(this->_path);
    if (::bind(this->_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1 || ::listen(this->_fd, 16) == -1) {
        const std::string reason = std::strerror(errno);
        ::close(this->_fd);
        this->_fd = -1;
        throw utils::exception::ErrorException(utils::exception::InternalCode::SocketInit, this->_path.string() + ": " + reason);
    }
    ::chmod(this->_path.c_str(), S_IRUSR | S_IWUSR);
    this->_running = true;
    this->_thread = std::thread(&cluster::Control::serve_, this);
}

_cold void cluster::Control::stop(void)
{
    if (!this->_running) return;
    this->_running = false;
    if (this->_thread.joinable()) this->_thread.join();
    if (this->_fd != -1) ::close(this->_fd);
    this->_fd = -1;
    std::error_code error;
    std::filesystem::remove(this->_path, error);
}

_cold void cluster::Control::serve_(void)
{
    utils::encapsulation::Poll poll;
    poll.link(this->_fd, EPOLLIN);
    while (this->_running) {
        if (poll.wait(300).empty()) continue;
        const int client = ::accept4(this->_fd, nullptr, nullptr, SOCK_CLOEXEC);
        if (client == -1) continue;
        std::thread(&cluster::Control::client_, this, client).detach();
    }
}

_cold void cluster::Control::client_(const int fd)
{
    // One connection = any number of requests (the MCP server keeps it open)
    std::string buffer;
    bool closed = false;
    while (this->_running && !closed) {
        const std::optional<std::string> line = read_line_(fd, buffer, 500, closed);
        if (!line) continue;
        if (job_serve_(this->_manager, fd, *line)) break; // a cluster-run: this connection is its job until it ends
        cluster::Json response;
        try {
            response = cluster::Control::handle(this->_manager, cluster::Json::parse(*line));
        } catch (const std::exception& e) { // not JSON, or anything a request may throw: told, the core goes on
            response = {{"ok", false}, {"error", std::string("bad request: ") + e.what()}};
        }
        if (!send_line_(fd, response.dump(-1, ' ', false, cluster::JSON_LENIENT))) break;
    }
    ::close(fd);
}

/* client */
_cold cluster::Json cluster::Control::request(const cluster::Json& request, std::atomic<int>* current)
{
    const int fd = connect_(cluster::socket_path());
    if (fd == -1) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning,
        "claude-cluster is not running (start it: claude-cluster)");
    if (current) *current = fd;
    std::string buffer;
    bool closed = false;
    const bool sent = send_line_(fd, request.dump(-1, ' ', false, cluster::JSON_LENIENT));
    const std::optional<std::string> line = sent ? read_line_(fd, buffer, 120000, closed) : std::nullopt;
    if (current) *current = -1;
    ::close(fd);
    if (!line) throw utils::exception::ErrorException(utils::exception::InternalCode::Communication, "no answer from claude-cluster");
    return cluster::Json::parse(*line);
}

/* editable files */
struct ConfigFile {
    std::string name;
    std::filesystem::path path;
    std::string what;
    std::string applied;            // when a change is taken
    bool editable = true;
    bool globalMay = true;          // the global session may write it (the settings of the prompts)
};

_cold static std::vector<ConfigFile> config_files_(cluster::Manager& manager)
{
    const cluster::Config& c = manager.config();
    return {
        {"global_prompt", cluster::expand_home(c.globalPrompt), "the system prompt of the global session (added to CLAUDE.md)",
            "the global session is started again on it (same conversation) after its turn", true, c.globalPromptWrite},
        {"sessions_prompt", cluster::expand_home(c.sessionsPrompt), "the system prompt of every sub-session (claude driver; empty: none)",
            "each sub-session takes it at its next start (/clear, /model...)", true, c.sessionsPromptWrite},
        {"vocabulary", cluster::vocabulary_path(c.voice.vocabulary), "proper names written back in what the voice hears: \"name = how it is heard, ...\"",
            "the next sentence heard", true},
        {"voice_words", cluster::voice_words_path(c.voice.words), "words said alone while a voice transcription waits: [cancel] / [send], one per line",
            "the next sentence heard", true},
        {"config", c.path(), "every option of claude-cluster (TOML)", "reloaded at once (the options marked at_next_start: at the next start)", true},
        {"machine_map", cluster::config_dir() / "machine.md", "the map of this computer (written by its scan: machine_sync)", "read only", false},
    };
}

_cold static ConfigFile config_file_(cluster::Manager& manager, const std::string& name)
{
    for (const ConfigFile& file: config_files_(manager))
        if (file.name == name) return file;
    throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId,
        "no file '" + name + "' (global_prompt, sessions_prompt, vocabulary, voice_words, config, machine_map)");
}

/* everything about this claude-cluster */
_cold static std::string resolve_command_(const std::string& name)
{
    // a command: its file in PATH (completed at the start: ~/.local/bin...), "" not found
    if (name.find('/') != std::string::npos) return std::filesystem::exists(cluster::expand_home(name)) ? cluster::expand_home(name).string() : "";
    const char* path = std::getenv("PATH");
    for (const std::string& dir: cluster::split(path ? path : "", ':'))
        if (::access((std::filesystem::path(dir) / name).c_str(), X_OK) == 0) return (std::filesystem::path(dir) / name).string();
    return "";
}

_cold static cluster::Json cluster_info_(cluster::Manager& manager)
{
    // what an assistant would otherwise look for in the files: where things are, how they are set, what is missing
    const cluster::Config& c = manager.config();
    const std::function<cluster::Json(const std::filesystem::path&)> file = [](const std::filesystem::path& path) {
        std::error_code error;
        return cluster::Json{{"path", path.string()}, {"exists", std::filesystem::exists(path, error)}};
    };
    cluster::Json info;
    info["version"] = CLUSTER_VERSION;
    info["binary"] = cluster::self_exe().string();
    #ifdef CLUSTER_GUI
        info["window_built"] = true;
    #else
        info["window_built"] = false;
    #endif
    info["paths"] = {{"config_dir", cluster::config_dir().string()}, {"config", file(c.path())}, {"data_dir", cluster::data_dir().string()},
        {"session_logs", (cluster::data_dir() / "logs").string()}, {"archived_transcripts", (cluster::data_dir() / "logs" / "archive").string()},
        {"history", file(cluster::data_dir() / "history.jsonl")}, {"usage_ledger", file(cluster::data_dir() / "usage.jsonl")},
        {"state", file(cluster::data_dir() / "state.json")}, {"socket", cluster::socket_path().string()},
        {"global_prompt", file(cluster::expand_home(c.globalPrompt))}, {"sessions_prompt", file(cluster::expand_home(c.sessionsPrompt))},
        {"vocabulary", file(cluster::vocabulary_path(c.voice.vocabulary))}, {"voice_words", file(cluster::voice_words_path(c.voice.words))},
        {"machine_map", file(cluster::config_dir() / "machine.md")}};
    info["machine_map_age"] = cluster::machine_age();
    info["config_error"] = c.error();
    info["global"] = {{"enabled", c.globalEnabled}, {"backend", c.globalBackend}, {"model", c.globalModel}, {"permission_mode", c.globalMode},
        {"effort", c.globalEffort}, {"cwd", c.globalCwd.empty() ? (cluster::data_dir() / "global").string() : c.globalCwd}, {"remote_control", manager.remote()}};
    cluster::Json commands = cluster::Json::object();
    for (const auto &[driver, command]: c.commands) {
        const std::string found = resolve_command_(command);
        commands[driver] = found.empty() ? command + " (not found)" : found;
    }
    info["agents"] = {{"default_backend", c.backend}, {"permission_mode", c.mode}, {"effort", c.effort}, {"commands", commands}};
    // the voice: its config, the model voice-listen uses, the models installed, what is missing
    const std::filesystem::path models = cluster::expand_home("~/.local/share/voice/models");
    std::string chosen = c.voice.listenModel;
    if (chosen.empty()) {
        std::ifstream in(cluster::expand_home("~/.config/voice/listen-model"));
        in >> chosen;
    }
    cluster::Json installed = cluster::Json::object();
    for (const auto &[name, folder]: std::vector<std::pair<std::string, std::string>>{{"parakeet", "sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8"},
        {"whisper", "sherpa-onnx-whisper-small"}, {"kroko-fr", "sherpa-onnx-streaming-zipformer-fr-kroko-2025-08-06"}})
        installed[name] = std::filesystem::exists(models / folder);
    if (chosen.empty()) chosen = installed["parakeet"].get<bool>() ? "parakeet" : "kroko-fr";
    info["voice"] = {{"enabled", c.voice.enabled}, {"mode", c.voice.mode}, {"wake_word", c.voice.wakeWord}, {"confirm_seconds", c.voice.confirmSeconds},
        {"end_silence", c.voice.endSilence > 0.0 ? c.voice.endSilence : 1.0}, {"only_me", c.voice.onlyMe}, {"speech_model", chosen},
        {"models_installed", installed}, {"listen_command", resolve_command_(c.voice.listenCommand)}, {"say_command", resolve_command_(c.voice.sayCommand)},
        {"say_engine", c.voice.sayEngine}, {"say_voices", c.voice.sayVoices}, {"listen_language", c.voice.listenLanguage},
        {"missing", cluster::voice_missing(c)}};
    // the background service
    std::ifstream unit(cluster::service_unit());
    const std::string text((std::istreambuf_iterator<char>(unit)), std::istreambuf_iterator<char>());
    info["service"] = {{"unit", file(cluster::service_unit())}, {"active", cluster::trim(cluster::capture({"systemctl", "--user", "is-active", "claude-cluster.service"}, "", 5).out)},
        {"enabled", cluster::trim(cluster::capture({"systemctl", "--user", "is-enabled", "claude-cluster.service"}, "", 5).out)},
        {"debug", text.find("service run --debug") != std::string::npos}, {"opens", c.service.frontend}, {"voice_mode", c.service.voiceMode},
        {"logs", "claude-cluster service logs (journalctl --user -u claude-cluster.service)"}};
    info["interface"] = {{"frontend", c.frontend}, {"layout", manager.ui("layout", c.layout)}, {"style", manager.ui("style", c.style)}, {"keys", c.keys}};
    info["sessions"] = manager.list(false).size();
    info["trash"] = manager.trash().size();
    return info;
}

/* commands */
/* the MCP of a sub-session: its own tasks and schedules (the ones it made) */
_cold static bool task_mine_(const cluster::Task& t, const std::string& owner, const bool sub)
{
    return !sub || t.by == owner;
}

_cold static bool schedule_mine_(const cluster::Schedule& w, const std::string& owner, const bool sub)
{
    return !sub || w.by == owner;
}

_cold static void ensure_task_(cluster::Manager& manager, const std::string& id, const std::string& owner, const bool sub)
{
    if (!sub) return;
    for (const cluster::Task& t: manager.tasks())
        if (t.id == id && task_mine_(t, owner, sub)) return;
    throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no task '" + id + "' of yours");
}

_cold static void ensure_schedule_(cluster::Manager& manager, const std::string& id, const std::string& owner, const bool sub)
{
    if (!sub) return;
    for (const cluster::Schedule& w: manager.schedules())
        if (w.id == id && schedule_mine_(w, owner, sub)) return;
    throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no schedule '" + id + "' of yours");
}

// the tools a sub-session gets (the settings [agent] sub_*), and what they may do
_cold static cluster::Json sub_tools_(const cluster::Config& config)
{
    cluster::Json tools = cluster::Json::array();
    if (config.subRead) {
        tools.push_back("task_list");
        tools.push_back("schedule_list");
    }
    if (config.subTasks) {
        for (const char* tool: {"task_add", "task_cancel", "task_act"})
            tools.push_back(tool);
    } else if (config.subRead) {
        tools.push_back("task_cancel");
    }
    if (config.subPin) {
        for (const char* tool: {"session_pin", "session_unpin", "session_pin_get"})
            tools.push_back(tool);
    }
    if (config.subSchedules) {
        for (const char* tool: {"schedule_add", "schedule_edit", "schedule_cancel", "schedule_act"})
            tools.push_back(tool);
    } else if (config.subRead) {
        tools.push_back("schedule_cancel");
    }
    for (const std::string& tool: cluster::orchestrator_sub_tools(config.orch)) // heavy_run, jobs_list, job_status, job_wait, job_kill...
        tools.push_back(tool);
    return {{"tools", tools}, {"spawn", config.subSpawn}, {"schedules", config.subSchedules}};
}

// a sub-session asks: allowed only what the settings open, never a local command, never a prompt into another session
_cold static void subGate_(cluster::Manager& manager, const std::string& owner, const cluster::Json& request)
{
    const std::function<void(const std::string&)> refuse = [](const std::string& why) {
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, why);
    };
    const std::string tool = request.value("tool", std::string());
    const std::string cmd = arg_(request, "cmd");
    if (owner.empty() || owner == GLOBAL_ID || !manager.snapshot(owner)) refuse("not allowed: a sub-session only");
    if (cmd == "sub_tools") return;
    const cluster::Json allowed = sub_tools_(manager.config()).at("tools");
    if (tool.empty() || std::find(allowed.begin(), allowed.end(), tool) == allowed.end())
        refuse("the tool " + tool + " is off for the sub-sessions ("
            + std::string(tool == "heavy_run" || tool == "jobs_add" ? "[orchestrator] sub_add: its permissions are not asked on this path; in a shell, cluster-run works as usual" : "[agent] sub_read / sub_tasks / sub_schedules")
            + " in the settings): ask the user");
    const bool spawn = manager.config().subSpawn;
    const std::function<bool(const cluster::Json&)> spawns = [&](const cluster::Json& o) {return o.contains("spawn") && o["spawn"].is_object() && !arg_(o["spawn"], "cwd").empty();};
    const std::function<bool(const cluster::Json&)> local = [&](const cluster::Json& on) {return on.is_object() && arg_(on, "type") == "command";};
    if (tool == "task_add") {
        if (spawns(request) && !spawn) refuse("not allowed: a task that starts a session is off for the sub-sessions ([agent] sub_spawn)");
        if (local(request.value("on_event", cluster::Json())))
            refuse("not allowed: a sub-session waits for session_idle / git_push / file events; a local command is the user's");
        const std::string target = arg_(request, "target");
        if (!target.empty() && manager.resolve(target) != owner) refuse("not allowed: a sub-session queues tasks for itself (or for a session of its own, spawn)");
    } else if (tool == "schedule_add" || tool == "schedule_edit") {
        if (!arg_(request, "session").empty() && manager.resolve(arg_(request, "session")) != owner) refuse("not allowed: a schedule of a sub-session wakes that sub-session only");
        const std::string action = arg_(request, "action", tool == "schedule_edit" ? "" : "prompt");
        if (action == "command" || (request.contains("if") && request["if"].is_object() && !arg_(request["if"], "command").empty()))
            refuse("not allowed: a schedule that runs a local command is the user's");
        if ((action == "spawn" || spawns(request)) && !spawn) refuse("not allowed: a schedule that starts sessions is off for the sub-sessions ([agent] sub_spawn)");
        if (local(request.value("on_event", cluster::Json()))) refuse("not allowed: a local command is the user's");
        if (action == "task" && !arg_(request, "target").empty() && manager.resolve(arg_(request, "target")) != owner) refuse("not allowed: a sub-session queues tasks for itself");
    } else if (tool == "task_act" && arg_(request, "action") == "force") {
        refuse("not allowed: the limit of parallel sessions is not for a sub-session to lift");
    }
}

_cold cluster::Json cluster::Control::handle(cluster::Manager& manager, const cluster::Json& request)
{
    const std::string cmd = arg_(request, "cmd");
    cluster::failure_popups() = request.value("ui", false); // its failures: told by its answer (an interface: its popup too)
    const std::function<std::string()> session = [&]() {return manager.resolve(arg_(request, "session"));};
    const bool from_global = request.value("origin", std::string()) == "mcp"; // a tool of the global session (its limits)
    const bool from_web = request.value("origin", std::string()) == "web";    // a device of the web page (set by the web access itself)
    const bool from_sub = request.value("origin", std::string()) == "sub";    // the MCP of a sub-session: itself only (its id in "owner")
    const std::string owner = from_sub ? arg_(request, "owner") : std::string();
    cluster::Json data;

    try {
        if (from_global && request.contains("tool") && !manager.config().mcpTool(request.value("tool", std::string())))
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the tool " + request.value("tool", std::string())
                + " is off in the settings of claude-cluster (MCP page): tell the user, they turn it on there if they want");
        if (from_sub) subGate_(manager, owner, request);
        // an interface of another process: seen alive (the core stops when none is left)
        // (its detach never: an interface closing is not one coming back, which would end the background of the core)
        if (cmd != "detach" && request.contains("client") && request["client"].is_string()) manager.clientSeen(request["client"].get<std::string>());
        if (cmd == "hello") {
            // who this core is: an interface of another build asks before attaching
            return {{"ok", true}, {"data", {{"version", CLUSTER_VERSION}, {"build", cluster::build_id()}, {"pid", ::getpid()}, {"binary", cluster::self_exe().string()}}}};
        }
        if (job_command_(cmd)) { // the heavy jobs: the user, the global session, a sub-session (its own jobs); a web device reads unless the settings let it act
            if (manager.orchestrator() == nullptr) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "the orchestrator is not running here");
            if (from_web && cmd != "job_list" && cmd != "job_status" && cmd != "orch_state" && !cluster::web_may(manager.config(), "orchestrator"))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, WEB_READ_ONLY);
            cluster::Orchestrator::Caller caller;
            caller.kind = request.value("origin", std::string()) == "sub" ? "sub" : from_global ? "global" : "user";
            caller.session = arg_(request, "owner");
            return {{"ok", true}, {"data", manager.orchestrator()->handle(cmd, request, caller)}};
        }
        if (cmd == "sync" && request.value("build", std::string()) != cluster::build_id()) // an interface of another build: not mixed
            return {{"ok", false}, {"error", "another version of claude-cluster runs the core (" + cluster::build_id() + "): close this one and open it again"}};
        if (cmd == "sync") {
            // its state, waited for up to 10 s until something changes
            const std::uint64_t seen = request.value("v", std::uint64_t{0});
            const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            const std::function<std::uint64_t()> now = [&manager]() {return manager.syncVersion();};
            while (seen != 0 && now() == seen && std::chrono::steady_clock::now() < end) {
                (void)manager.asking().check();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            data = manager.syncState(request);
        } else if (cmd == "detach") {
            manager.clientGone(arg_(request, "client"));
        } else if (cmd == "client_focus") {
            manager.clientFocus(arg_(request, "client"), request.value("focused", false));
        } else if (cmd == "active") {
            manager.noteActive(arg_(request, "session"));
        } else if (cmd == "simple_words") { // the words of the simple mode, once per language (the page)
            const std::string lang = arg_(request, "lang");
            data = cluster::simple_words(lang.empty() ? cluster::simple_lang(manager) : lang);
        } else if (cmd == "ui_set") {
            manager.setUi(arg_(request, "key"), request.value("value", std::string()));
        } else if (cmd == "unqueue") {
            const std::optional<cluster::Queued> queued = manager.unqueue(session());
            if (queued) data = *queued;
        } else if (cmd == "rewind") {
            data = manager.rewind(session(), request.value("back", 1));
        } else if (cmd == "restore_answer") {
            manager.answerRestore(request.value("restore", false));
        } else if (cmd == "restart_global") {
            manager.restartGlobal();
        } else if (cmd == "machine_scan") {
            if (from_web) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the computer is mapped from the computer only");
            manager.scanMachine();
        } else if (cmd == "voice") {
            // the voice keys of an interface of another process (F5, send, drop, correct, mute): the voice of this core
            if (from_web) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the page has its own voice");
            if (from_global) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the voice is driven by the user only");
            cluster::Voice* voice = manager.voice();
            if (voice == nullptr) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "no core here");
            const std::string action = arg_(request, "action");
            const std::string text = request.value("text", std::string());
            if (action == "toggle") {
                if (!manager.config().voice.enabled) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "voice disabled: [voice] enabled = true in the config");
                voice->toggle();
            } else if (action == "confirm") {
                voice->confirm();
            } else if (action == "cancel") {
                voice->cancel();
            } else if (action == "cancel_last") {
                voice->cancelLast();
            } else if (action == "edit") {
                voice->edit(text);
            } else if (action == "mute") {
                voice->mute();
            } else if (action == "speak") {
                voice->speak(text);
            } else {
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "voice: toggle | confirm | cancel | cancel_last | edit | mute | speak");
            }
            data = {{"listening", voice->listening()}};
        } else if (cmd == "action") {
            // the palette of an interface of another process, run here (its voice, its web access, its sessions)
            if (from_global || from_web) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the palette is the user's (its keys, its web access)");
            if (manager.voice() == nullptr) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "no core here");
            const cluster::ActionResult result = cluster::run_action(manager, *manager.voice(), arg_(request, "id"), arg_(request, "session", GLOBAL_ID),
                request.value("input", std::string()));
            data = {{"message", result.message}, {"error", result.error}, {"focus", result.focus}, {"ui", result.ui}};
        } else if (cmd == "actions") {
            data = cluster::Json::array();
            for (const cluster::Action& action: cluster::actions(manager, arg_(request, "session", GLOBAL_ID)))
                data.push_back({{"id", action.id}, {"title", action.title}, {"input", action.input}, {"fill", action.fill}, {"ui", action.ui}});
        } else if (cmd == "slash") {
            const std::optional<cluster::ActionResult> local = cluster::run_slash(manager, arg_(request, "session", GLOBAL_ID), arg_(request, "text"));
            data = local ? cluster::Json{{"local", true}, {"message", local->message}, {"error", local->error}, {"focus", local->focus}, {"ui", local->ui},
                {"fill", local->fill}} : cluster::Json{{"local", false}};
        } else if (cmd == "complete") {
            const std::string id = arg_(request, "session", GLOBAL_ID);
            data = cluster::Json::array();
            for (const cluster::Candidate& c: cluster::completion_provider(manager, [id]() {return id;})(static_cast<cluster::CompletionKind>(request.value("kind", 0)),
                    request.value("query", std::string())))
                data.push_back({{"value", c.value}, {"label", c.label}, {"description", c.description}, {"directory", c.directory}, {"source", c.source}});
        } else if (cmd == "face") {
            // the face of the app now (the web page, claude-cluster icon, a status bar)
            const cluster::AppFace face = manager.appFace(false);
            data = {{"name", cluster::face_name(face.face)}, {"text", cluster::face_text(face.face)}, {"badge", face.badge}, {"urgent", face.urgent},
                {"svg", cluster::face_svg(face.face, face.badge, face.urgent)}};
        } else if (cmd == "mcp_off") {
            data = cluster::Json::array();
            for (const auto &[tool, on]: manager.config().mcpTools)
                if (!on) data.push_back(tool);
        } else if (cmd == "list") {
            // filters: category, tag (several: all of them), state (open: the live sessions, default | archived / trash | all | a state: working, idle...)
            const std::string category = cluster::clean_label(request.value("category", std::string()));
            std::vector<std::string> wantedTags = cluster::split_tags(request.value("tag", std::string()));
            for (const std::string& tag: request.value("tags", std::vector<std::string>()))
                wantedTags.push_back(cluster::clean_label(tag));
            const std::string state = cluster::lower(request.value("state", std::string("open")));
            // the taxonomy: a category / tag also matches the sessions under the labels below it (descendants: false, only itself);
            // under: every session below a node of the taxonomy
            const cluster::Taxonomy taxonomy = manager.taxonomy();
            const bool below = request.value("descendants", true);
            const std::string under = cluster::clean_label(request.value("under", std::string()));
            const std::function<bool(const cluster::SessionSpec&)> matches = [&](const cluster::SessionSpec& spec) {
                if (!under.empty() && !taxonomy.under(spec, under, below)) return false;
                if (!category.empty() && !taxonomy.scope(category, below).contains(spec.category)) return false;
                return std::all_of(wantedTags.begin(), wantedTags.end(), [&](const std::string& tag) {
                    const std::set<std::string> accepted = taxonomy.scope(tag, below);
                    return std::any_of(spec.tags.begin(), spec.tags.end(), [&accepted](const std::string& own) {return accepted.contains(own);});
                });
            };
            data = cluster::Json::array();
            if (state != "archived" && state != "trash")
                for (const cluster::Snapshot& snap: manager.list(request.value("global", true)))
                    if (matches(snap.spec) && (state == "open" || state == "all" || state == cluster::state_name(snap.state)))
                        data.push_back(manager.toJson(snap, false));
            if (state == "archived" || state == "trash" || state == "all")
                for (const cluster::SessionSpec& spec: manager.trash())
                    if (matches(spec)) data.push_back({{"id", spec.id}, {"name", spec.name}, {"cwd", spec.cwd}, {"state", "archived"}, {"summary", spec.summary}, {"category", spec.category}, {"tags", spec.tags}});
        } else if (cmd == "status" || cmd == "read") {
            const std::optional<cluster::Snapshot> snap = manager.snapshot(session());
            if (snap) data = manager.toJson(*snap, true);
        } else if (cmd == "spawn") {
            std::string backend = arg_(request, "backend");
            std::string effort = arg_(request, "effort");
            std::string wanted;         // the costly model / effort asked (applied once the user allows it)
            std::string wantedEffort;
            if (from_global) {
                // the policy: its default when nothing is asked, refused / asked when what is asked is not allowed / costly
                const cluster::PolicyConf& policy = manager.config().policy;
                const std::size_t slash = backend.find('/');
                std::string model = slash == std::string::npos ? std::string() : backend.substr(slash + 1);
                const std::string provider = slash == std::string::npos ? backend : backend.substr(0, slash);
                if (model.empty() && !policy.defaultModel.empty() && policy.modelMode != "free" && backend != "claude/" && provider.find('/') == std::string::npos) {
                    const std::string base = provider.empty() ? manager.config().backend.substr(0, manager.config().backend.find('/')) : provider;
                    if (base == "claude" || base == "anthropic" || base == "anthropic-api") model = policy.defaultModel;
                    if (!model.empty()) backend = base + "/" + model;
                }
                if (effort.empty() && !policy.defaultEffort.empty() && policy.effortMode != "free") effort = policy.defaultEffort;
                const cluster::PolicyVerdict verdict = cluster::policy_check(manager.config(), model, effort, "", "");
                if (verdict.kind == "refuse") throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "refused by the policy: " + verdict.reason);
                if (verdict.kind == "ask") { // the session starts as the policy says, the costly part asked after
                    wanted = model;
                    wantedEffort = effort;
                    if (cluster::policy_costly_model(manager.config(), model)) {
                        backend = backend.substr(0, backend.find('/')); // not on the costly model yet: on the default of the policy, else the one of its agent
                        if (!policy.defaultModel.empty() && policy.modelMode != "free" && !cluster::policy_costly_model(manager.config(), policy.defaultModel))
                            backend += "/" + policy.defaultModel;
                    }
                    if (cluster::policy_costly_effort(manager.config(), effort)) effort.clear();
                    if (!cluster::policy_costly_model(manager.config(), model)) wanted.clear();
                    if (!cluster::policy_costly_effort(manager.config(), wantedEffort)) wantedEffort.clear();
                }
            }
            cluster::SpawnRequest made{arg_(request, "cwd"), arg_(request, "name"), backend, arg_(request, "mode"), arg_(request, "profile"), arg_(request, "prompt"), effort, false, "", false, {}, "", from_global && request.value("reply", true), from_global && request.value("one_shot", false) && manager.config().archiveOneShot};
            made.origin = "mcp";
            data = {{"id", manager.spawn(made)}};
            if (from_global && manager.methodSpawned()) data["work_method_due"] = "the user enabled the detection of their work method: call work_method (action sample), then propose if their habits differ from the current one";
            if (!wanted.empty() || !wantedEffort.empty()) {
                const std::string asked = manager.modelChange(data["id"].get<std::string>(), wanted, wantedEffort, true);
                if (!asked.empty()) data["asked"] = asked + " (the session runs meanwhile on the policy default; the user answers in their interface)";
            }
        } else if (cmd == "send" && arg_(request, "session") == TASKS_ID) {
            // the Tasks session (the web page): a /command, else a new task / reminder
            const std::string text = cluster::trim(arg_(request, "text"));
            std::optional<cluster::ActionResult> local = cluster::run_slash(manager, TASKS_ID, text);
            if (!local) local = cluster::tasks_prompt(manager, text);
            if (local->error) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, local->message);
            data = {{"id", TASKS_ID}, {"local", true}, {"message", local->message}, {"ui", local->ui}, {"focus", local->focus}, {"fill", local->fill}};
        } else if (cmd == "send") {
            // a local /command (/clear, /model, /mode...) runs here like typed in the prompt, the others go to the agent
            const std::string id = session();
            const std::string text = arg_(request, "text", arg_(request, "prompt")); // "prompt": the name used by session_spawn
            if (cluster::trim(text).empty() && request.value("images", std::vector<std::string>()).empty())
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "empty prompt: give the text to send in 'text'");
            if (const std::string typed = cluster::trim(text); from_web && !manager.config().web.shell && (typed.starts_with("!")
                || (typed.starts_with(":") && typed.find('!') != std::string::npos))) // the page and its terminal: the shell only when the computer allows it
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the shell from the page is off (settings of the computer: Web > Shell from the page)");
            if (request.value("raw", false)) { // an interface of another process: its /commands already run, its images
                manager.noteSender(id, arg_(request, "client"));
                if (!from_global) manager.userTouched(id);
                manager.send(id, text, request.value("force", false), request.value("images", std::vector<std::string>()), request.value("prompt_origin", std::string()));
                return {{"ok", true}, {"data", {{"id", id}, {"queued", true}}}};
            }
            if (from_global) {
                // /model and /effort typed by the global session go through the same policy as session_set_model
                const std::string typed = cluster::trim(text);
                for (const char* command: {"/model", "/effort"}) {
                    const std::string head = std::string(command) + " ";
                    if (!typed.starts_with(head) || cluster::trim(typed.substr(head.size())).empty()) continue;
                    const std::string value = cluster::trim(typed.substr(head.size()));
                    const std::string asked = manager.modelChange(id, std::string(command) == "/model" ? value : "", std::string(command) == "/effort" ? value : "", true);
                    return {{"ok", true}, {"data", {{"id", id}, {"local", true}, {"message", asked.empty() ? std::string("done") : asked + " (asked to the user)"}}}};
                }
            }
            const std::optional<cluster::ActionResult> local = cluster::run_slash(manager, id, cluster::trim(text));
            if (local) {
                if (local->error) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, local->message);
                std::string message = local->message.empty() ? "done" : local->message;
                if (!local->ui.empty() && local->ui != "fill" && local->ui != "pick:share:" && !local->ui.starts_with("pick:delete:")) message += " (" + local->ui + ": only in the interface, not run)";
                return {{"ok", true}, {"data", {{"id", id}, {"local", true}, {"message", message}, {"ui", local->ui}, {"fill", local->fill}}}};
            }
            if (!from_global) manager.userTouched(id);
            const bool reply = request.value("reply", false) && id != GLOBAL_ID;
            if (reply) manager.replyTo(id); // before: a short turn could end before it is known
            // from the global session (MCP): marked, the sub-session answers it compact (sessions.md), not for a human
            const bool global = request.value("from", std::string()) == "global" && id != GLOBAL_ID;
            // the images of the prompt: the attachments of this session only (never a path given by a page); the files the global session attaches
            std::string sent = text;
            std::vector<std::string> images;
            for (const std::string& path: request.value("images", std::vector<std::string>()))
                if (const std::string mine = cluster::attach_resolve(manager.config(), id, path); !mine.empty()) images.push_back(mine);
            if (from_global && request.contains("attachments") && request["attachments"].is_array() && !request["attachments"].empty()) {
                const cluster::AttachConf& limits = manager.config().attach;
                if (!limits.globalAttach) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the global session may not attach files (settings: Attachments > The global session may attach)");
                if (static_cast<int>(request["attachments"].size()) > limits.globalMax)
                    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "at most " + std::to_string(limits.globalMax) + " attachments per prompt (settings)");
                const std::optional<cluster::Snapshot> target = manager.snapshot(id);
                std::vector<cluster::Attachment> linked;
                for (const cluster::Json& path: request["attachments"])
                    if (path.is_string()) linked.push_back(cluster::attach_link(manager.config(), manager.resolve(id), path.get<std::string>(), cluster::AttachScope::Global, target ? target->spec.cwd : ""));
                sent = cluster::attach_prompt(text, linked, images);
            }
            manager.send(id, global ? "[from the global session] " + sent : sent, request.value("force", false), images, from_global ? "mcp" : "");   // typed by the user (CLI, page): no origin
            data = {{"id", id}, {"queued", true}, {"reply", reply ? "its answer comes back to you as a message [reply from ...] when done: no need to poll it" : "no"}};
        } else if (cmd == "wait") {
            // until the session is done (idle, nothing queued) or the time is over: its last answer only, not its context
            const std::string id = session();
            const int seconds = std::clamp(request.value("timeout", 100), 1, 110);
            const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
            std::optional<cluster::Snapshot> snap = manager.snapshot(id);
            const std::function<bool(void)> busy = [&snap]() {
                return snap && (snap->state == cluster::State::Working || snap->state == cluster::State::Waiting || snap->state == cluster::State::Starting || snap->queued > 0);
            };
            while (busy() && std::chrono::steady_clock::now() < end) {
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
                snap = manager.snapshot(id);
            }
            if (!snap) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no session " + id);
            std::string answer;
            for (auto it = snap->entries.rbegin(); it != snap->entries.rend() && answer.empty(); ++it)
                if (it->kind == cluster::EntryKind::Assistant) answer = it->text;
            // how its last turn ended: finished, cancelled (interrupted before its answer), error, closed - only finished is done for good
            const std::string outcome = busy() ? std::string("running") : snap->state == cluster::State::Stopped ? std::string("closed")
                : snap->state == cluster::State::Error || !snap->lastError.empty() ? std::string("error") : snap->stopped ? std::string("cancelled") : std::string("finished");
            data = {{"id", id}, {"state", cluster::state_name(snap->state)}, {"done", !busy()}, {"outcome", outcome}, {"finished", outcome == "finished"},
                {"answer", cluster::head_utf8(answer, 6000)}};
            if (outcome == "cancelled") data["note"] = "its last turn was interrupted before it finished: not done (send it again or ask what is left)";
            else if (outcome == "error") data["note"] = "its last turn ended with an error: not done (" + cluster::one_line(snap->lastError, 200) + ")";
            if (busy()) data["note"] = "still working: wait again, or send with reply and go on (its answer will come back)";
            if (!snap->permissions.empty()) data["permission"] = snap->permissions.front().tool + ": " + cluster::one_line(snap->permissions.front().description, 200);
        } else if (cmd == "close") {
            manager.close(session());
        } else if (cmd == "restore") {
            data = {{"id", manager.restore(arg_(request, "session"))}};
        } else if (cmd == "projects") {
            // the projects of the map of this computer matching a text (its name first): a new session on one
            data = cluster::Json::array();
            for (const cluster::Project& p: cluster::projects(arg_(request, "query"))) {
                data.push_back({{"path", p.path}, {"name", p.name}, {"kind", p.kind}, {"branch", p.branch}, {"origin", p.origin}, {"by_name", p.byName}});
                if (data.size() >= 40) break;
            }
        } else if (cmd == "delete") {
            // a session deleted for good (closed, out of the trash): only confirmed
            if (!(request.contains("confirmed") && (request["confirmed"] == true || request["confirmed"] == "true")))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "confirm: delete it for good (its conversation gone)");
            const std::string id = manager.resolve(session());
            manager.close(id);
            manager.purge(id);
        } else if (cmd == "purge") {
            manager.purge(arg_(request, "session"));
        } else if (cmd == "rename") {
            manager.rename(session(), arg_(request, "name"));
        } else if (cmd == "apply") {
            // a model / mode / effort where asked (here, global, subs, all, new, a session)
            data = {{"done", manager.applyTo(arg_(request, "kind"), arg_(request, "value"), arg_(request, "scope"), arg_(request, "session", GLOBAL_ID))}};
        } else if (cmd == "shell_share") {
            data = {{"done", manager.shareShell(session(), arg_(request, "choice"))}};
        } else if (cmd == "shell") {
            if (from_web && !manager.config().web.shell) // the page (its terminal too): only when the computer allows it
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the shell from the page is off (settings of the computer: Web > Shell from the page)");
            data = manager.shell(session(), arg_(request, "command"));
        } else if (cmd == "agents") {
            // the sub-agents of a session (with their activity), or of every session (without it)
            const std::string id = arg_(request, "session");
            data = cluster::Json::array();
            for (const cluster::Snapshot& s: manager.list(true)) {
                if (!id.empty() && s.spec.id != manager.resolve(id)) continue;
                for (const cluster::SubAgent& a: s.agents) {
                    cluster::Json item = a;
                    item["session"] = s.spec.id;
                    item["session_name"] = s.spec.global ? std::string("global") : s.spec.name;
                    if (id.empty()) item.erase("entries");
                    if (!id.empty() && !a.outputFile.empty()) { // a shell: the end of its output
                        const std::string out = cluster::read_small(a.outputFile);
                        item["output"] = cluster::clean_utf8(out.size() > 6000 ? out.substr(out.size() - 6000) : out);
                    }
                    data.push_back(item);
                }
            }
        } else if (cmd == "agent_stop") {
            manager.stopAgent(arg_(request, "session"), arg_(request, "task"));
            data = {{"stopping", true}};
        } else if (cmd == "send_next") {
            manager.sendNext(session());
        } else if (cmd == "resume") {
            data = {{"done", manager.resume(arg_(request, "session"))}};
        } else if (cmd == "describe") {
            // the description (text; none given with labels: kept) and / or the category and tags (given by the global session or the user)
            const bool labels = request.contains("category") || request.contains("tags") || request.contains("add_tags") || request.contains("remove_tags");
            if (request.contains("text") || !labels) manager.describe(arg_(request, "session"), request.value("text", std::string())); // the trash too
            if (labels) {
                const std::function<std::vector<std::string>(const char*)> words = [&request](const char* key) {
                    if (!request.contains(key)) return std::vector<std::string>();
                    if (request[key].is_array()) return request[key].get<std::vector<std::string>>();
                    return cluster::split_tags(request[key].is_string() ? request[key].get<std::string>() : std::string());
                };
                manager.label(arg_(request, "session"), request.contains("category") ? std::optional<std::string>(request.value("category", std::string())) : std::nullopt,
                    request.contains("tags") ? std::optional<std::vector<std::string>>(words("tags")) : std::nullopt, words("add_tags"), words("remove_tags"));
            }
        } else if (cmd == "taxonomy") {
            // the forest of the categories / tags: list (default) | set_parent (node, parent: none = under global) | rename (node, to) | delete (node, strip)
            // | merge (node, into); the global session only when the setting allows it
            const std::string action = arg_(request, "action", "list");
            if (action != "list") {
                if (from_global && !manager.config().taxonomyEdit)
                    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the global session may not edit the taxonomy (setting "
                        "\"Global session can edit the taxonomy\" is off): tell the user, they set the parent by hand (graph, palette, claude-cluster taxonomy)");
                std::string other = arg_(request, "to");
                if (other.empty()) other = arg_(request, "parent");
                if (other.empty()) other = arg_(request, "into");
                manager.taxonomyEdit(action, arg_(request, "node"), other, request.value("strip", false));
            }
            data = manager.taxonomyJson();
            cluster::Json labels = cluster::Json::object(); // every label used by a live session, with its number of sessions
            for (const cluster::Snapshot& snap: manager.list(false)) {
                std::set<std::string> own(snap.spec.tags.begin(), snap.spec.tags.end());
                if (!snap.spec.category.empty()) own.insert(snap.spec.category);
                for (const std::string& label: own)
                    labels[label] = labels.value(label, 0) + 1;
            }
            data["labels"] = labels;
        } else if (cmd == "work_method") {
            // read (default), sample (the habits of the user, for the detection) or propose (the user answers Allow / Deny); never written here
            const std::string action = arg_(request, "action", "read");
            if (action == "read") {
                data = manager.methodJson();
            } else if (action == "sample") {
                data = manager.methodSample(static_cast<std::size_t>(request.value("max", 10)));
            } else if (action == "propose") {
                data = {{"asked", manager.methodPropose(arg_(request, "preset"), arg_(request, "text"), arg_(request, "reason"))},
                    {"note", "the user answers in their interface; the answer comes back to you as a message"}};
            } else {
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "work_method: action read | sample | propose");
            }
        } else if (cmd == "mode") {
            manager.setMode(session(), arg_(request, "mode"));
        } else if (cmd == "model") {
            // the model and / or the effort of a session (the same conversation started again); none: the current ones and the choices
            const std::string id = session();
            // from the global session: the policy of the user (refused, allowed, or asked first when costly)
            const std::string asked = manager.modelChange(id, arg_(request, "model"), arg_(request, "effort"), from_global);
            const cluster::ModelInfo info = cluster::model_info(manager, id);
            data = {{"id", id}, {"driver", info.driver}, {"model", info.model}, {"effort", info.effort.empty() ? "default" : info.effort},
                {"models", cluster::Json::array()}, {"efforts", info.efforts}};
            if (!asked.empty()) data["asked"] = asked + " (the user answers in their interface; the answer comes back to you as a message)";
            for (const auto &[model, description]: info.models)
                data["models"].push_back({{"model", model}, {"description", description}});
        } else if (cmd == "usage") {
            manager.usageRefresh(); // what grew since, the account (in the background: the next read has it)
            data = manager.usageJson(arg_(request, "vendor"));
        } else if (cmd == "cluster_info") {
            data = cluster_info_(manager);
        } else if (cmd == "config_files") {
            data = cluster::Json::array();
            for (const ConfigFile& file: config_files_(manager)) {
                std::error_code error;
                data.push_back({{"name", file.name}, {"path", file.path.string()}, {"what", file.what}, {"applied", file.applied},
                    {"editable", file.editable && (!from_global || file.globalMay)}, {"exists", std::filesystem::exists(file.path, error)}});
            }
        } else if (cmd == "config_read") {
            if (from_web && !cluster::web_may(manager.config(), "config")) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, WEB_READ_ONLY);
            const ConfigFile file = config_file_(manager, arg_(request, "name"));
            std::ifstream in(file.path);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (content.size() > 200000) content = cluster::head_utf8(content, 200000);
            data = {{"name", file.name}, {"path", file.path.string()}, {"content", content}};
        } else if (cmd == "config_write") {
            // the whole file replaced (read it first); config.toml checked before; the global prompt: started again after the turn
            if (from_web && !cluster::web_may(manager.config(), "config")) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, WEB_READ_ONLY);
            const ConfigFile file = config_file_(manager, arg_(request, "name"));
            if (!file.editable) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, file.name + " is read only (" + file.what + ")");
            if (from_global && !file.globalMay)
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not allowed: the user did not let the global session rewrite "
                    + std::string(file.name == "global_prompt" ? "its own system prompt (settings page > Sessions > Global session can rewrite its own system prompt)"
                    : "the system prompt of the sub-sessions (settings page > Sessions > Global session can rewrite the system prompt of the sub-sessions)")
                    + ": give them the text, they edit it (Ctrl+K, edit the system prompt)");
            const std::string content = arg_(request, "content");
            if (file.name == "config") {
                const std::string error = cluster::Config::check(content);
                if (!error.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "config.toml not written, invalid TOML: " + error);
                if (from_global) { // the protected settings: the user alone
                    std::ifstream old(file.path);
                    const std::string before((std::istreambuf_iterator<char>(old)), std::istreambuf_iterator<char>());
                    const std::vector<std::string> changed = cluster::protected_changes(before, content);
                    if (!changed.empty()) {
                        std::string list;
                        for (const std::string& key: changed)
                            list += (list.empty() ? "" : ", ") + key;
                        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "config.toml not written: " + list
                            + " changed by the user only, in the settings page of claude-cluster: ask them (settings_set for the other options)");
                    }
                }
            }
            std::error_code error;
            std::filesystem::create_directories(file.path.parent_path(), error);
            std::ofstream(file.path) << content;
            if (file.name == "global_prompt") manager.restartGlobalAfterTurn();
            manager.notice(GLOBAL_ID, cluster::short_path(file.path.string()) + " written by the global session");
            data = {{"name", file.name}, {"path", file.path.string()}, {"applied", file.applied}};
        } else if (cmd == "machine_sync") {
            if (from_web) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the computer is mapped from the computer only");
            manager.requestMachineSync();
            data = {{"asked", "the question is shown in the interface: the user answers it (the scan then runs, its progress shown)"}};
        } else if (cmd == "allow") {
            if (from_global && arg_(request, "request_id").starts_with("core-"))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not allowed: this question is the user's (the global session never answers it)");
            if (request.contains("answers") && request["answers"].is_object()) manager.answerQuestions(session(), arg_(request, "request_id"), request["answers"]);
            else manager.answer(session(), arg_(request, "request_id"), arg_(request, "behavior", "allow"));
        } else if (cmd == "interrupt") {
            manager.interrupt(session());
        } else if (cmd == "cd") {
            manager.cd(session(), arg_(request, "cwd"));
        } else if (cmd == "trash") {
            data = cluster::Json::array();
            for (const cluster::SessionSpec& spec: manager.trash())
                data.push_back({{"id", spec.id}, {"name", spec.name}, {"cwd", spec.cwd}, {"backend", spec.backend}, {"deleted", spec.deleted}, {"summary", spec.summary}, {"category", spec.category}, {"tags", spec.tags}});
        } else if (cmd == "task_add") {
            const std::string by = from_sub ? owner : request.value("origin", std::string()) == "mcp" ? std::string(GLOBAL_ID) : arg_(request, "by"); // a sub-session, the global session (MCP), else you
            cluster::Json options = request;
            if (from_sub && arg_(request, "target").empty() && !(request.contains("spawn") && request["spawn"].is_object())) options["target"] = owner; // for itself: after its turn
            data = {{"id", manager.addTaskJson(options, by)}};
        } else if (cmd == "fanout") {
            data = manager.fanout(request);
        } else if (cmd == "group_status") {
            data = manager.groupStatus(request.value("group", std::string()));
        } else if (cmd == "group_cancel") {
            manager.groupCancel(arg_(request, "group"), request.value("interrupt", false));
        } else if (cmd == "task_list") {
            data = cluster::Json::array();
            const bool all = request.value("archived", false); // the finished ones leave the list after a while (like the interfaces)
            const std::string wanted = arg_(request, "status");
            for (const cluster::Task& t: manager.tasks()) {
                if ((t.archived && !all) || !task_mine_(t, owner, from_sub) || (!wanted.empty() && t.status != wanted) || (!arg_(request, "id").empty() && t.id != arg_(request, "id"))) continue;
                data.push_back(manager.taskJson(t));
            }
        } else if (cmd == "background") {
            if (from_web || from_global) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the core is kept in background by its interfaces only");
            manager.setBackground(true);
            data = {{"busy", manager.busy()}};
        } else if (cmd == "notify") {
            const bool sent = manager.notifyDesktop(arg_(request, "title", "claude-cluster"), arg_(request, "text"), request.value("urgent", false));
            data = {{"sent", sent}};
            if (!sent) data["note"] = "desktop notifications are off ([ui] notify)";
        } else if (cmd == "schedule_add") {
            const std::string by = from_sub ? owner : request.value("origin", std::string()) == "mcp" ? std::string(GLOBAL_ID) : arg_(request, "by");
            cluster::Json options = request;
            if (from_sub && (arg_(request, "action", "prompt") == "prompt" || arg_(request, "action") == "command")) options["session"] = owner; // wakes itself only
            data = manager.addScheduleJson(options, by, request.value("dry_run", false));
        } else if (cmd == "schedule_list") {
            data = cluster::Json::array();
            const bool all = request.value("archived", false);
            for (const cluster::Schedule& w: manager.schedules())
                if ((all || !w.archived) && schedule_mine_(w, owner, from_sub) && (arg_(request, "id").empty() || w.id == arg_(request, "id"))) data.push_back(manager.scheduleJson(w));
        } else if (cmd == "schedule_edit") {
            ensure_schedule_(manager, arg_(request, "id"), owner, from_sub);
            data = manager.scheduleEdit(arg_(request, "id"), request);
        } else if (cmd == "schedule_cancel") {
            ensure_schedule_(manager, arg_(request, "id"), owner, from_sub);
            data = {{"done", manager.scheduleAct(arg_(request, "id"), "cancel", arg_(request, "reason"))}};
        } else if (cmd == "task_cancel") {
            ensure_task_(manager, arg_(request, "id"), owner, from_sub);
            data = {{"done", manager.taskAct(arg_(request, "id"), "cancel", arg_(request, "reason"))}};
        } else if (cmd == "task_act") {
            ensure_task_(manager, arg_(request, "id"), owner, from_sub);
            data = {{"done", arg_(request, "action") == "set" ? manager.taskSet(arg_(request, "id"), request) : manager.taskAct(arg_(request, "id"), arg_(request, "action"), arg_(request, "reason"))}};
        } else if (cmd == "schedule_act") {
            ensure_schedule_(manager, arg_(request, "id"), owner, from_sub);
            data = {{"done", manager.scheduleAct(arg_(request, "id"), arg_(request, "action"), arg_(request, "action") == "snooze" && request.contains("minutes") ? arg_(request, "minutes") : arg_(request, "reason"))}};
        } else if (cmd == "sub_tools") {
            data = sub_tools_(manager.config());
        } else if (cmd == "todos") {
            // the todo list of each session (its TodoWrite now)
            data = cluster::Json::array();
            for (const cluster::Snapshot& s: manager.list(true))
                if (!s.todos.empty()) data.push_back({{"session", s.spec.id}, {"name", s.spec.global ? std::string("global") : s.spec.name}, {"todos", s.todos}});
        } else if (cmd == "search") {
            data = cluster::Json::array();
            for (const cluster::Hit& hit: manager.search(arg_(request, "text")))
                data.push_back({{"session", hit.session}, {"name", hit.name}, {"time", hit.time}, {"text", hit.text}});
        } else if (cmd == "export") {
            const std::string markdown = manager.exportMarkdown(arg_(request, "session"));
            const std::string path = arg_(request, "path");
            // the global session: a .md / .txt file out of the hidden folders only (never ~/.bashrc, ~/.ssh/authorized_keys...)
            if ((from_global || from_web) && !path.empty() && (cluster::hidden_path(path) || !(path.ends_with(".md") || path.ends_with(".txt"))))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not allowed: the global session and the page write .md / .txt files out of the "
                    "hidden folders only (" + path + "): give the markdown to the user, or another path");
            if (path.empty()) {
                data = {{"markdown", markdown}};
            } else {
                std::ofstream(cluster::expand_home(path)) << markdown;
                data = {{"path", cluster::expand_home(path).string()}};
            }
        } else if (cmd == "remote") {
            manager.setRemote(request.value("on", true));
            data = {{"remote", manager.remote()}};
        } else if (cmd == "ui_state") {
            const cluster::Config& config = manager.config();
            data = {{"shown", manager.active()}, {"layout", manager.ui("layout", config.layout)}, {"style", manager.ui("style", config.style)},
                {"remote", manager.remote()}, {"vim", manager.ui("vim", config.vim ? "true" : "false") == "true"},
                {"layouts", {"list", "grid", "tabs", "hub"}}, {"panels", {"skills", "tokens", "context", "cost", "usage", "git", "diff", "tools", "files"}}};
            data["styles"] = cluster::Json::array();
            for (const auto &[name, style]: config.styles)
                data["styles"].push_back(name);
        } else if (cmd == "ui_focus") {
            manager.requestFocus(arg_(request, "session", "g"));
        } else if (cmd == "ui_layout") {
            const std::string layout = arg_(request, "layout");
            if (layout != "list" && layout != "grid" && layout != "tabs" && layout != "hub")
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "layout: list | grid | tabs");
            manager.setUi("layout", layout);
        } else if (cmd == "ui_style") {
            const std::string style = arg_(request, "style");
            if (!manager.config().styles.contains(style)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown style " + style);
            manager.setUi("style", style);
        } else if (cmd == "ui_panel") {
            manager.togglePanel(session(), arg_(request, "panel"));
        } else if (cmd == "settings_search") {
            // the settings matching a text: by their name far before their description, where it matched
            const std::vector<cluster::SettingsPage> pages = cluster::settings_pages(manager.config());
            data = cluster::Json::array();
            for (const cluster::SettingHit& hit: cluster::settings_search(pages, arg_(request, "query"))) {
                const cluster::Setting& s = pages[hit.page].settings[hit.item];
                data.push_back({{"section", s.section}, {"key", s.key}, {"label", s.label}, {"page", pages[hit.page].title}, {"help", s.description},
                    {"matched", hit.name ? "name" : "description"}});
            }
        } else if (cmd == "stats") {
            // the Stats session: built / refreshed, shown on request
            data = {{"id", cluster::stats_open(manager, request.value("focus", false))}};
        } else if (cmd == "showcase") {
            // the Debug page: everything claude-cluster can show, on the interface asking (its platform)
            data = {{"message", cluster::showcase(manager, arg_(request, "what", "all"), arg_(request, "platform"))}};
        } else if (cmd == "services") {
            // its state, and the card of each one made here (the web page draws them as they are)
            data = cluster::services(manager);
            for (const char* name: {"service", "web", "core", "voice", "mcp", "outside", "processes"}) {
                cluster::Json lines = cluster::Json::array();
                for (const cluster::ServiceLine& l: cluster::service_lines(name, data))
                    lines.push_back({{"text", l.text}, {"kind", l.kind}});
                cluster::Json buttons = cluster::Json::array();
                for (const auto &[label, action]: cluster::service_buttons(name, data))
                    buttons.push_back({{"label", label}, {"action", action}, {"risky", cluster::service_risky(name, action)}});
                data["cards"][name] = {{"summary", cluster::service_summary(name, data)}, {"lines", lines}, {"buttons", buttons}};
            }
        } else if (cmd == "service_act") {
            if (from_web && !cluster::web_may(manager.config(), "services")) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, WEB_READ_ONLY);
            if (from_web && arg_(request, "service") == "core" && (arg_(request, "action") == "log" || arg_(request, "action") == "debug_log"))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the logs are opened on the computer only");
            data = {{"message", cluster::service_act(manager, arg_(request, "service"), arg_(request, "action"))}};
        } else if (cmd == "failure") {
            // a failure answered: OK (gone), retry (run again)
            const std::string answer = request.value("answer", std::string("ok"));
            manager.failureAnswer(arg_(request, "id"), answer == "retry" || answer == "other" ? answer : std::string("ok"));
            data = {{"done", true}};
        } else if (cmd == "failures") {
            data = manager.failures();
        } else if (cmd == "install_package") {
            // what a setting needs: installed here (the password window of the desktop)
            if (from_web && !cluster::web_may(manager.config(), "services")) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, WEB_READ_ONLY);
            const std::string error = cluster::install_package(arg_(request, "package"), &manager);
            if (!error.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, error);
            data = {{"done", arg_(request, "package") + " installed"}};
        } else if (cmd == "settings_get") {
            // every option of the settings page: its section, key, value, choices and help
            const std::string section = arg_(request, "section");
            data = cluster::Json::array();
            cluster::accounts_status_set(manager.accountsJson()); // the Accounts page: the providers and the spending as they are now
            for (const cluster::SettingsPage& page: cluster::settings_pages(manager.config()))
                for (const cluster::Setting& setting: page.settings)
                    if (section.empty() || section == setting.section)
                        data.push_back({{"section", setting.section}, {"key", setting.key}, {"label", setting.label}, {"value", setting.value(manager.config())},
                            {"problem", cluster::setting_requirement(manager, setting, setting.value(manager.config())).problem},
                            {"package", cluster::setting_requirement(manager, setting, setting.value(manager.config())).package},
                            {"web_editable", cluster::web_may(manager.config(), setting.section) && !(setting.section == "web" && setting.key == "settings")},
                            {"danger", [&setting]() {
                                cluster::Json d = cluster::Json::object(); // the dangerous values of a choice / a switch: their warning
                                std::vector<std::string> values = setting.choices;
                                if (setting.kind == cluster::SettingKind::Bool) values = {"true", "false"};
                                for (const std::string& v: values)
                                    if (const std::string w = cluster::setting_danger(setting, v); !w.empty()) d[v] = w;
                                return d;
                            }()},
                            {"choices", setting.choices}, {"help", setting.description}, {"at_next_start", setting.restart}, {"page", page.title},
                            {"kind", setting.kind == cluster::SettingKind::Bool ? "bool" : setting.kind == cluster::SettingKind::Int ? "int"
                                : setting.kind == cluster::SettingKind::Float ? "float" : setting.kind == cluster::SettingKind::Choice ? "choice"
                                : setting.kind == cluster::SettingKind::List ? "list" : setting.kind == cluster::SettingKind::Action ? "action" : "text"}});
        } else if (cmd == "settings_suggest") {
            // the values a text setting takes for what is typed (the page shows them as a list); the folders and programs of the computer only for its own interfaces
            data = cluster::Json::array();
            for (const cluster::SettingsPage& page: cluster::settings_pages(manager.config()))
                for (const cluster::Setting& setting: page.settings)
                    if (setting.section == arg_(request, "section") && setting.key == arg_(request, "key"))
                        for (const std::string& value: cluster::setting_suggestions(manager.config(), setting, arg_(request, "typed"), !from_web))
                            data.push_back(value);
        } else if (cmd == "settings_set") {
            // one option written in config.toml (its comments kept), loaded again; the live ones of the interface follow
            const std::string section = arg_(request, "section");
            const std::string key = arg_(request, "key");
            const std::string value = arg_(request, "value");
            const bool confirmed = request.contains("confirmed") && (request["confirmed"] == true || request["confirmed"] == "true");
            if (from_web && (!cluster::web_may(manager.config(), section) || (section == "web" && key == "settings")))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, WEB_READ_ONLY);
            if (from_global && cluster::setting_protected(section, key)) // the user alone (the tools of the global session never ask them)
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not allowed: " + section + "." + key
                    + " is changed by the user only, in the settings page of claude-cluster (F9, Ctrl+K settings): ask them");
            for (const cluster::SettingsPage& page: cluster::settings_pages(manager.config()))
                for (const cluster::Setting& setting: page.settings)
                    if (setting.section == section && setting.key == key && value != setting.value(manager.config()) && !confirmed)
                        if (const std::string warning = cluster::setting_danger(setting, value); !warning.empty())
                            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "confirm: " + warning);
            const bool restart = cluster::settings_set(manager.config(), section, key, value);
            if (section == "ui" && (key == "layout" || key == "style" || key == "vim" || key == "group_by_category")) manager.setUi(key, value);
            (void)manager.config().reload();
            data = {{"value", value}, {"at_next_start", restart}};
        } else if (cmd == "notices") {
            data = cluster::Json::array();
            for (const cluster::Notice& n: manager.notices(30))
                data.push_back({{"time", n.time}, {"session", n.session}, {"text", n.text}, {"error", n.error}});
        } else if (cmd == "display") {
            // a rich display put in the transcript of a session (no prompt): {type, ...} or a text (markdown)
            cluster::Json display = request.contains("display") && request["display"].is_object() ? request["display"]
                : cluster::Json{{"type", "markdown"}, {"text", arg_(request, "text")}};
            if (request.contains("title") && !display.contains("title")) display["title"] = arg_(request, "title");
            if ((from_global || from_web) && display.contains("files") && display["files"].is_array()) // never a hidden file offered to the page
                for (const cluster::Json& f: display["files"])
                    if (f.is_object() && cluster::hidden_path(f.value("path", std::string())))
                        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not allowed: a hidden file is never offered to download ("
                            + f.value("path", std::string()) + ")");
            data = {{"id", manager.display(session(), display)}};
        } else if (cmd == "display_update") {
            // a display replaced (the steps of a long work, a progress, a checklist...), its id kept
            if (!request.contains("display") || !request["display"].is_object())
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "display_update: session, id, display {type, ...}");
            manager.displayUpdate(session(), arg_(request, "id"), request["display"]);
            data = {{"id", arg_(request, "id")}};
        } else if (cmd == "upgrade") {
            // a newer claude-cluster installed: update / ignore / quit (the user, in an interface of this computer)
            if (from_web || from_global)
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the update is chosen by the user, in a terminal or a window of claude-cluster");
            data = {{"done", manager.upgradeAct(arg_(request, "action"))}};
        } else if (cmd == "shells") {
            data = manager.shells();
        } else if (cmd == "shell_kill") {
            data = {{"done", manager.shellKill(arg_(request, "id", "all"))}};
        } else if (cmd == "models_policy") {
            data = manager.modelsPolicyJson();
        } else if (cmd == "plugins_refresh") { // a source (empty: every one) pulled now
            if (from_web && !cluster::web_may(manager.config(), "plugins"))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, WEB_READ_ONLY);
            manager.pluginsRefresh(arg_(request, "source"));
        } else if (cmd == "plugins") {
            data = manager.pluginsJson();
        } else if (cmd == "plugins_set") { // several ticks / sources at once (code of these sources runs as you: a web device only with [web] settings plugins)
            if (from_web && !cluster::web_may(manager.config(), "plugins"))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, WEB_READ_ONLY);
            manager.pluginsSet(request.value("changes", cluster::Json::object()));
        } else if (cmd == "display_act") {
            data = {{"done", manager.displayAct(session(), arg_(request, "id"), request)}};
        } else if (cmd == "display_get") {
            const std::optional<cluster::Json> d = manager.displayOf(session(), arg_(request, "id"));
            if (!d) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no display " + arg_(request, "id"));
            data = *d;
        } else if (cmd == "ask_preview") {
            // the picture of the preview of an option (a path, ![](path) or {"type": "image", "path"}): read here, the browser cannot reach the file; only a file
            // the question being asked names itself
            const std::optional<cluster::Ask> current = manager.asking().current();
            if (!current || request.value("question", -1) < 0 || request.value("question", -1) >= static_cast<int>(current->questions.size()))
                throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "no question");
            const cluster::Json options = current->questions[static_cast<std::size_t>(request.value("question", 0))].value("options", cluster::Json::array());
            const int at = request.value("option", -1);
            if (at < 0 || at >= static_cast<int>(options.size())) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no such option");
            std::string source = cluster::trim(options[static_cast<std::size_t>(at)].value("preview", std::string()));
            const cluster::Json block = source.starts_with("{") ? cluster::Json::parse(source, nullptr, false) : cluster::Json();
            if (block.is_object() && block.contains("path")) source = block.value("path", std::string());
            else if (source.starts_with("![") && source.find("](") != std::string::npos) source = source.substr(source.find("](") + 2, source.rfind(')') - source.find("](") - 2);
            const cluster::Json prepared = cluster::display_prepare({{"type", "image"}, {"path", cluster::trim(source)}});
            data = {{"mime", prepared.value("mime", std::string())}, {"data", prepared.value("data", std::string())}};
        } else if (cmd == "ask") {
            // the question of an agent being answered (shared by the interfaces and the voice): choose, toggle, other, next, back, decline
            cluster::Asking& asking = manager.asking();
            (void)asking.check();
            if (from_global)
                if (const std::optional<cluster::Ask> current = asking.current(); current && current->requestId.starts_with("core-"))
                    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not allowed: this question is the user's (the global session never answers it)");
            const std::string action = arg_(request, "action");
            if (action == "choose") asking.choose(request.value("option", 0));
            else if (action == "move") asking.move(request.value("delta", 1));
            else if (action == "type") asking.type(arg_(request, "text"));
            else if (action == "erase") asking.erase();
            else if (action == "toggle") asking.toggle();
            else if (action == "other") asking.setOther(arg_(request, "text"));
            else if (action == "back") asking.back();
            else if (action == "forward") data = {{"moved", asking.forward()}};
            else if (action == "decline") asking.decline();
            else if (action == "global") data = {{"done", asking.toGlobal()}};
            else if (action == "chat") data = {{"done", asking.chat(arg_(request, "text"))}}; // "Chat about this": its answer in the question
            else if (action == "say") data = {{"done", asking.spoken(arg_(request, "text"))}}; // as said to the voice
            else if (action == "next") {
                const std::string why = asking.next();
                if (!why.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, why);
            } else if (!action.empty()) {
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "ask: choose | toggle | other | next | back | decline | global | chat");
            }
            if (action != "say" && action != "global" && action != "chat" && action != "forward") data = asking.json();
        } else if (cmd == "external") {
            // the conversations of the agents opened outside claude-cluster, not the ones opened here
            data = cluster::Json::array();
            for (const cluster::Conversation& c: cluster::external_conversations(arg_(request, "agent"), std::clamp(request.value("limit", 30), 1, 200),
                manager.conversationIds()))
                data.push_back(cluster::external_json(c));
        } else if (cmd == "import") {
            data = {{"id", manager.importConversation(arg_(request, "agent"), arg_(request, "id"), arg_(request, "name"), request.value("fork", false))}};
        } else if (cmd == "fork") {
            if (from_global && !manager.config().globalFork)
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction,
                    "not allowed: the user did not let the global session fork (settings page > Sessions > Global session can fork)");
            data = {{"id", manager.forkSession(session(), arg_(request, "name"))}};
        } else if (cmd == "keep_warm") { // a session kept warm or not: on | off | toggle
            data = {{"message", manager.setKeepWarm(session(), request.value("value", std::string("toggle")))}};
        } else if (cmd == "older") {
            // scrolled to the top of a transcript: its older entries read again from its log
            data = manager.loadOlder(session(), static_cast<std::size_t>(std::clamp(request.value("count", 300), 1, 5000)));
        } else if (cmd == "transcript") {
            data = manager.transcript(session(), request.value("from", -50L), static_cast<std::size_t>(std::clamp(request.value("count", 50), 1, 500)));
        } else if (cmd == "context") {
            // the global session: never a hidden file (keys, credentials, configs) given to a session (and to its API)
            if (const std::string source = arg_(request, "source"); (from_global || from_web) && !source.starts_with("session:") && (source.starts_with("/") || source.starts_with("~")
                || source.starts_with(".") || source.find(':') == std::string::npos) && cluster::hidden_path(source))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "not allowed: a hidden file is never given as a context (" + source + ")");
            manager.loadContext(session(), arg_(request, "source"));
            data = {{"sent", true}};
        } else if (cmd == "messages") {
            data = manager.conversationMessages(session());
        } else if (cmd == "edit") {
            if (from_global && !manager.config().globalEdit)
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction,
                    "not allowed: the user did not let the global session edit (settings page > Sessions > Global session can edit)");
            data = {{"result", manager.editConversation(session(), request)}};
        } else if (cmd == "web") {
            // the web access of this instance: on / off for this run, kick (the browser out, a new code), pin (a new code)
            cluster::Web* web = manager.web();
            if (web == nullptr) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no web access in this instance");
            // devices: approve <id> allow | trust | deny, kick [ids], untrust [ids] (ids: separated by commas, none: all)
            static const std::set<std::string> actions = {"", "on", "off", "kick", "pin", "devices", "approve", "untrust", "ban", "unban"};
            const std::string action = arg_(request, "action");
            if (!actions.contains(action))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown action '" + action
                    + "' (web [on|off|pin|devices|kick [ids]|approve <id> allow|trust|deny|untrust [ids]|ban <ids>|unban [ids]])");
            if ((action == "kick" || action == "pin" || action == "approve") && !web->status().on)
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the web access is closed: claude-cluster web on");
            std::vector<std::string> ids;
            for (const std::string& id: cluster::split(arg_(request, "ids"), ','))
                if (!cluster::trim(id).empty() && cluster::trim(id) != "all") ids.push_back(cluster::trim(id));
            const bool kicked = action == "kick" && !web->status().client.empty();
            if (action == "on" || action == "off") web->setOn(action == "on");
            else if (action == "kick") web->kick(ids);
            else if (action == "pin") web->renewPin();
            else if (action == "approve") web->approve(arg_(request, "id"), arg_(request, "answer", "allow"));
            else if (action == "untrust") web->untrust(ids);
            else if (action == "ban") web->ban(ids);
            else if (action == "unban") web->unban(ids);
            if (action == "on" || action == "off") web->tick(); // applied now, not at the next second
            const cluster::WebStatus status = web->status();
            const std::function<cluster::Json(const std::vector<cluster::WebDevice>&)> list = [](const std::vector<cluster::WebDevice>& devices) {
                cluster::Json out = cluster::Json::array();
                for (const cluster::WebDevice& d: devices)
                    out.push_back({{"id", d.id}, {"name", d.name}, {"address", d.address}, {"agent", d.agent}, {"state", d.state}, {"trusted", d.trusted},
                        {"since", d.since / 1000}, {"seen", d.seen / 1000}, {"ping", d.ping}});
                return out;
            };
            data = {{"on", status.on}, {"url", status.url}, {"pin", status.pin}, {"client", status.client}, {"error", status.error},
                {"https", status.https}, {"tty_port", status.ttyPort}, {"fingerprint", status.fingerprint}, {"certificate", status.certificate}, {"certificate_why", status.certificateWhy}, {"remote_url", status.remoteUrl}, {"remote", status.remoteState}, {"remote_ok", status.remoteOk},
                {"action", action}, {"kicked", kicked}, {"devices", list(status.devices)},
                {"trusted", list(status.trusted)}, {"banned", list(status.banned)}, {"ping", status.ping}};
        } else if (cmd == "docs") {
            // the documentation embedded in this binary: searched, the page of a screen of the app, or opened in the browser of the desktop (a web device opens its own panel)
            if (!cluster::docs_built())
                throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "the documentation is not in this build of claude-cluster (python3 3.11+ is needed to build it)");
            const std::string action = arg_(request, "action").empty() ? "search" : arg_(request, "action");
            const std::string lang = arg_(request, "lang") == "fr" ? "fr" : "en";
            const std::string target = arg_(request, "page").empty() ? arg_(request, "screen") : arg_(request, "page");
            if (action == "search") {
                data = cluster::docs_search(arg_(request, "query"), lang, static_cast<std::size_t>(std::clamp(request.value("limit", 8), 1, 20)));
            } else if (action == "page") {
                data = {{"page", cluster::docs_page(target)}};
            } else if (action == "open") {
                if (from_web) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the page opens the documentation in its own panel (/docs/)");
                const cluster::Web* web = manager.web();
                const cluster::DocOpened opened = cluster::docs_open(target, web != nullptr && web->status().on ? web->status().url : std::string());
                data = {{"url", opened.url}, {"opened", opened.opened}};
            } else {
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "docs: action search | page | open");
            }
        } else if (cmd == "voice_engines") {
            // the engines of the voice: what each one handles, its limits, its free use, its links, ready, used
            data = cluster::voice_engines_json(manager.config());
        } else if (cmd == "providers") {
            data = cluster::Json::array();
            for (const cluster::Provider& p: manager.auth().providers()) {
                const auto [ready, detail] = manager.auth().status(p);
                data.push_back({{"name", p.name}, {"driver", p.driver}, {"ready", ready}, {"detail", detail}, {"local", p.local}, {"description", p.description}});
            }
        } else if (cmd == "pin" || cmd == "unpin" || cmd == "pin_get") {
            // the pinned item of a session's view: the user (any), the global session, a sub-session (its own view only), per the settings
            const bool bySub = request.value("origin", std::string()) == "sub";   // the MCP of a sub-session: its own view only
            const std::string target = bySub ? manager.resolve(arg_(request, "owner")) : session();
            const std::string by = from_global ? "global" : bySub ? "sub" : "user";
            if (cmd == "pin") data = manager.pin(target, request, by);
            else if (cmd == "unpin") manager.unpin(target, by);
            else data = manager.pinOf(target);
        } else {
            return {{"ok", false}, {"error", "unknown command '" + cmd + "'"}};
        }
    } catch (const utils::exception::IException& e) {
        return {{"ok", false}, {"error", std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info())}};
    } catch (const std::exception& e) {
        // a number out of range, a wrong type...: told, the core goes on (a request never stops every session)
        return {{"ok", false}, {"error", std::string("bad request: ") + e.what()}};
    }
    return {{"ok", true}, {"data", data}};
}
