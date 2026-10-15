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
#include "cluster/External.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Control.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Service.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Web.hpp"
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
    return request[key].is_string() ? request[key].get<std::string>() : request[key].dump();
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
        cluster::Json response;
        try {
            response = cluster::Control::handle(this->_manager, cluster::Json::parse(*line));
        } catch (const cluster::Json::exception& e) {
            response = {{"ok", false}, {"error", std::string("bad request: ") + e.what()}};
        }
        if (!send_line_(fd, response.dump())) break;
    }
    ::close(fd);
}

/* client */
_cold cluster::Json cluster::Control::request(const cluster::Json& request)
{
    const int fd = connect_(cluster::socket_path());
    if (fd == -1) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning,
        "claude-cluster is not running (start it: claude-cluster)");
    std::string buffer;
    bool closed = false;
    const bool sent = send_line_(fd, request.dump());
    const std::optional<std::string> line = sent ? read_line_(fd, buffer, 120000, closed) : std::nullopt;
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
};

_cold static std::vector<ConfigFile> config_files_(cluster::Manager& manager)
{
    const cluster::Config& c = manager.config();
    return {
        {"global_prompt", cluster::expand_home(c.globalPrompt), "the system prompt of the global session (added to CLAUDE.md)",
            "the global session is started again on it (same conversation) after its turn", true},
        {"sessions_prompt", cluster::expand_home(c.sessionsPrompt), "the system prompt of every sub-session (claude driver; empty: none)",
            "each sub-session takes it at its next start (/clear, /model...)", true},
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
_cold cluster::Json cluster::Control::handle(cluster::Manager& manager, const cluster::Json& request)
{
    const std::string cmd = arg_(request, "cmd");
    const std::function<std::string()> session = [&]() {return manager.resolve(arg_(request, "session"));};
    const bool from_global = request.value("origin", std::string()) == "mcp"; // a tool of the global session (its limits)
    cluster::Json data;

    try {
        if (from_global && request.contains("tool") && !manager.config().mcpTool(request.value("tool", std::string())))
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "the tool " + request.value("tool", std::string())
                + " is off in the settings of claude-cluster (MCP page): tell the user, they turn it on there if they want");
        if (cmd == "face") {
            // the face of the app now (the web page, claude-cluster icon, a status bar)
            const cluster::AppFace face = manager.appFace(false);
            data = {{"name", cluster::face_name(face.face)}, {"text", cluster::face_text(face.face)}, {"badge", face.badge}, {"urgent", face.urgent},
                {"svg", cluster::face_svg(face.face, face.badge, face.urgent)}};
        } else if (cmd == "mcp_off") {
            data = cluster::Json::array();
            for (const auto &[tool, on]: manager.config().mcpTools)
                if (!on) data.push_back(tool);
        } else if (cmd == "list") {
            data = cluster::Json::array();
            for (const cluster::Snapshot& snap: manager.list(request.value("global", true)))
                data.push_back(manager.toJson(snap, false));
        } else if (cmd == "status" || cmd == "read") {
            const std::optional<cluster::Snapshot> snap = manager.snapshot(session());
            if (snap) data = manager.toJson(*snap, true);
        } else if (cmd == "spawn") {
            data = {{"id", manager.spawn({arg_(request, "cwd"), arg_(request, "name"), arg_(request, "backend"),
                arg_(request, "mode"), arg_(request, "profile"), arg_(request, "prompt"), arg_(request, "effort")})}};
        } else if (cmd == "send") {
            // a local /command (/clear, /model, /mode...) runs here like typed in the prompt, the others go to the agent
            const std::string id = session();
            const std::string text = arg_(request, "text");
            const std::optional<cluster::ActionResult> local = cluster::run_slash(manager, id, cluster::trim(text));
            if (local) {
                if (local->error) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, local->message);
                std::string message = local->message.empty() ? "done" : local->message;
                if (!local->ui.empty()) message += " (" + local->ui + ": only in the interface, not run)";
                return {{"ok", true}, {"data", {{"id", id}, {"local", true}, {"message", message}}}};
            }
            const bool reply = request.value("reply", false) && id != GLOBAL_ID;
            if (reply) manager.replyTo(id); // before: a short turn could end before it is known
            // from the global session (MCP): marked, the sub-session answers it compact (sessions.md), not for a human
            const bool global = request.value("from", std::string()) == "global" && id != GLOBAL_ID;
            manager.send(id, global ? "[from the global session] " + text : text, request.value("force", false));
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
            data = {{"id", id}, {"state", cluster::state_name(snap->state)}, {"done", !busy()}, {"answer", answer.size() > 6000 ? answer.substr(0, 6000) + "\n[...]" : answer}};
            if (busy()) data["note"] = "still working: wait again, or send with reply and go on (its answer will come back)";
            if (!snap->permissions.empty()) data["permission"] = snap->permissions.front().tool + ": " + cluster::one_line(snap->permissions.front().description, 200);
        } else if (cmd == "close") {
            manager.close(session());
        } else if (cmd == "restore") {
            data = {{"id", manager.restore(arg_(request, "session"))}};
        } else if (cmd == "purge") {
            manager.purge(arg_(request, "session"));
        } else if (cmd == "rename") {
            manager.rename(session(), arg_(request, "name"));
        } else if (cmd == "describe") {
            manager.describe(arg_(request, "session"), request.value("text", std::string())); // the trash too
        } else if (cmd == "mode") {
            manager.setMode(session(), arg_(request, "mode"));
        } else if (cmd == "model") {
            // the model and / or the effort of a session (the same conversation started again); none: the current ones and the choices
            const std::string id = session();
            if (!arg_(request, "model").empty()) manager.setModel(id, arg_(request, "model"));
            if (!arg_(request, "effort").empty()) manager.setEffort(id, arg_(request, "effort"));
            const cluster::ModelInfo info = cluster::model_info(manager, id);
            data = {{"id", id}, {"driver", info.driver}, {"model", info.model}, {"effort", info.effort.empty() ? "default" : info.effort},
                {"models", cluster::Json::array()}, {"efforts", info.efforts}};
            for (const auto &[model, description]: info.models)
                data["models"].push_back({{"model", model}, {"description", description}});
        } else if (cmd == "usage") {
            data = manager.usageJson(arg_(request, "vendor"));
        } else if (cmd == "cluster_info") {
            data = cluster_info_(manager);
        } else if (cmd == "config_files") {
            data = cluster::Json::array();
            for (const ConfigFile& file: config_files_(manager)) {
                std::error_code error;
                data.push_back({{"name", file.name}, {"path", file.path.string()}, {"what", file.what}, {"applied", file.applied},
                    {"editable", file.editable}, {"exists", std::filesystem::exists(file.path, error)}});
            }
        } else if (cmd == "config_read") {
            const ConfigFile file = config_file_(manager, arg_(request, "name"));
            std::ifstream in(file.path);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (content.size() > 200000) content = content.substr(0, 200000) + "\n[... cut at 200 000 characters]";
            data = {{"name", file.name}, {"path", file.path.string()}, {"content", content}};
        } else if (cmd == "config_write") {
            // the whole file replaced (read it first); config.toml checked before; the global prompt: started again after the turn
            const ConfigFile file = config_file_(manager, arg_(request, "name"));
            if (!file.editable) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, file.name + " is read only (" + file.what + ")");
            const std::string content = arg_(request, "content");
            if (file.name == "config") {
                const std::string error = cluster::Config::check(content);
                if (!error.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "config.toml not written, invalid TOML: " + error);
            }
            std::error_code error;
            std::filesystem::create_directories(file.path.parent_path(), error);
            std::ofstream(file.path) << content;
            if (file.name == "global_prompt") manager.restartGlobalAfterTurn();
            manager.notice(GLOBAL_ID, cluster::short_path(file.path.string()) + " written by the global session");
            data = {{"name", file.name}, {"path", file.path.string()}, {"applied", file.applied}};
        } else if (cmd == "machine_sync") {
            manager.requestMachineSync();
            data = {{"asked", "the question is shown in the interface: the user answers it (the scan then runs, its progress shown)"}};
        } else if (cmd == "allow") {
            if (request.contains("answers") && request["answers"].is_object()) manager.answerQuestions(session(), arg_(request, "request_id"), request["answers"]);
            else manager.answer(session(), arg_(request, "request_id"), arg_(request, "behavior", "allow"));
        } else if (cmd == "interrupt") {
            manager.interrupt(session());
        } else if (cmd == "cd") {
            manager.cd(session(), arg_(request, "cwd"));
        } else if (cmd == "trash") {
            data = cluster::Json::array();
            for (const cluster::SessionSpec& spec: manager.trash())
                data.push_back({{"id", spec.id}, {"name", spec.name}, {"cwd", spec.cwd}, {"backend", spec.backend}, {"deleted", spec.deleted}, {"summary", spec.summary}});
        } else if (cmd == "task_add") {
            data = {{"id", manager.addTask(arg_(request, "prompt"), arg_(request, "target"), request.value("after", std::vector<std::string>()))}};
        } else if (cmd == "task_list") {
            data = cluster::Json::array();
            for (const cluster::Task& t: manager.tasks())
                data.push_back({{"id", t.id}, {"prompt", cluster::one_line(t.prompt, 200)}, {"target", t.target}, {"after", t.after}, {"status", t.status}, {"session", t.session}});
        } else if (cmd == "notify") {
            const bool sent = manager.notifyDesktop(arg_(request, "title", "claude-cluster"), arg_(request, "text"), request.value("urgent", false));
            data = {{"sent", sent}};
            if (!sent) data["note"] = "desktop notifications are off ([ui] notify)";
        } else if (cmd == "schedule_add") {
            data = {{"id", manager.addSchedule(arg_(request, "when"), arg_(request, "action", "prompt"), arg_(request, "session"), arg_(request, "text"),
                request.value("every", 0))}};
        } else if (cmd == "schedule_list") {
            data = cluster::Json::array();
            for (const cluster::Schedule& w: manager.schedules())
                data.push_back({{"id", w.id}, {"at", cluster::clock_time(w.at)}, {"in_minutes", (w.at - cluster::now()) / 60}, {"every_minutes", w.every},
                    {"action", w.action}, {"session", w.session}, {"text", cluster::one_line(w.text, 200)}});
        } else if (cmd == "schedule_cancel") {
            manager.cancelSchedule(arg_(request, "id"));
        } else if (cmd == "task_cancel") {
            manager.cancelTask(arg_(request, "id"));
        } else if (cmd == "search") {
            data = cluster::Json::array();
            for (const cluster::Hit& hit: manager.search(arg_(request, "text")))
                data.push_back({{"session", hit.session}, {"name", hit.name}, {"time", hit.time}, {"text", hit.text}});
        } else if (cmd == "export") {
            const std::string markdown = manager.exportMarkdown(arg_(request, "session"));
            const std::string path = arg_(request, "path");
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
                {"layouts", {"list", "grid", "tabs"}}, {"panels", {"skills", "tokens", "context", "cost", "usage", "git", "diff", "tools", "files"}}};
            data["styles"] = cluster::Json::array();
            for (const auto &[name, style]: config.styles)
                data["styles"].push_back(name);
        } else if (cmd == "ui_focus") {
            manager.requestFocus(arg_(request, "session", "g"));
        } else if (cmd == "ui_layout") {
            const std::string layout = arg_(request, "layout");
            if (layout != "list" && layout != "grid" && layout != "tabs")
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "layout: list | grid | tabs");
            manager.setUi("layout", layout);
        } else if (cmd == "ui_style") {
            const std::string style = arg_(request, "style");
            if (!manager.config().styles.contains(style)) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown style " + style);
            manager.setUi("style", style);
        } else if (cmd == "ui_panel") {
            manager.togglePanel(session(), arg_(request, "panel"));
        } else if (cmd == "settings_get") {
            // every option of the settings page: its section, key, value, choices and help
            const std::string section = arg_(request, "section");
            data = cluster::Json::array();
            for (const cluster::SettingsPage& page: cluster::settings_pages(manager.config()))
                for (const cluster::Setting& setting: page.settings)
                    if (section.empty() || section == setting.section)
                        data.push_back({{"section", setting.section}, {"key", setting.key}, {"label", setting.label}, {"value", setting.value(manager.config())},
                            {"choices", setting.choices}, {"help", setting.description}, {"at_next_start", setting.restart}, {"page", page.title},
                            {"kind", setting.kind == cluster::SettingKind::Bool ? "bool" : setting.kind == cluster::SettingKind::Int ? "int"
                                : setting.kind == cluster::SettingKind::Float ? "float" : setting.kind == cluster::SettingKind::Choice ? "choice"
                                : setting.kind == cluster::SettingKind::List ? "list" : setting.kind == cluster::SettingKind::Action ? "action" : "text"}});
        } else if (cmd == "settings_set") {
            // one option written in config.toml (its comments kept), loaded again; the live ones of the interface follow
            const std::string section = arg_(request, "section");
            const std::string key = arg_(request, "key");
            const std::string value = arg_(request, "value");
            const bool restart = cluster::settings_set(manager.config(), section, key, value);
            if (section == "ui" && (key == "layout" || key == "style" || key == "vim")) manager.setUi(key, value);
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
            data = {{"id", manager.display(session(), display)}};
        } else if (cmd == "display_update") {
            // a display replaced (the steps of a long work, a progress, a checklist...), its id kept
            if (!request.contains("display") || !request["display"].is_object())
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "display_update: session, id, display {type, ...}");
            manager.displayUpdate(session(), arg_(request, "id"), request["display"]);
            data = {{"id", arg_(request, "id")}};
        } else if (cmd == "display_act") {
            data = {{"done", manager.displayAct(session(), arg_(request, "id"), request)}};
        } else if (cmd == "display_get") {
            const std::optional<cluster::Json> d = manager.displayOf(session(), arg_(request, "id"));
            if (!d) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no display " + arg_(request, "id"));
            data = *d;
        } else if (cmd == "ask") {
            // the question of an agent being answered (shared by the interfaces and the voice): choose, toggle, other, next, back, decline
            cluster::Asking& asking = manager.asking();
            (void)asking.check();
            const std::string action = arg_(request, "action");
            if (action == "choose") asking.choose(request.value("option", 0));
            else if (action == "toggle") asking.toggle();
            else if (action == "other") asking.setOther(arg_(request, "text"));
            else if (action == "back") asking.back();
            else if (action == "decline") asking.decline();
            else if (action == "say") data = {{"done", asking.spoken(arg_(request, "text"))}}; // as said to the voice
            else if (action == "next") {
                const std::string why = asking.next();
                if (!why.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, why);
            } else if (!action.empty()) {
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "ask: choose | toggle | other | next | back | decline");
            }
            if (action != "say") data = asking.json();
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
        } else if (cmd == "transcript") {
            data = manager.transcript(session(), request.value("from", -50L), static_cast<std::size_t>(std::clamp(request.value("count", 50), 1, 500)));
        } else if (cmd == "context") {
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
            static const std::set<std::string> actions = {"", "on", "off", "kick", "pin", "devices", "approve", "untrust"};
            const std::string action = arg_(request, "action");
            if (!actions.contains(action))
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "unknown action '" + action
                    + "' (web [on|off|pin|devices|kick [ids]|approve <id> allow|trust|deny|untrust [ids]])");
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
            if (action == "on" || action == "off") web->tick(); // applied now, not at the next second
            const cluster::WebStatus status = web->status();
            const std::function<cluster::Json(const std::vector<cluster::WebDevice>&)> list = [](const std::vector<cluster::WebDevice>& devices) {
                cluster::Json out = cluster::Json::array();
                for (const cluster::WebDevice& d: devices)
                    out.push_back({{"id", d.id}, {"name", d.name}, {"address", d.address}, {"agent", d.agent}, {"state", d.state}, {"trusted", d.trusted},
                        {"since", d.since / 1000}, {"seen", d.seen / 1000}});
                return out;
            };
            data = {{"on", status.on}, {"url", status.url}, {"pin", status.pin}, {"client", status.client}, {"error", status.error},
                {"https", status.https}, {"fingerprint", status.fingerprint}, {"action", action}, {"kicked", kicked}, {"devices", list(status.devices)},
                {"trusted", list(status.trusted)}};
        } else if (cmd == "voice_engines") {
            // the engines of the voice: what each one handles, its limits, its free use, its links, ready, used
            data = cluster::voice_engines_json(manager.config());
        } else if (cmd == "providers") {
            data = cluster::Json::array();
            for (const cluster::Provider& p: manager.auth().providers()) {
                const auto [ready, detail] = manager.auth().status(p);
                data.push_back({{"name", p.name}, {"driver", p.driver}, {"ready", ready}, {"detail", detail}, {"local", p.local}, {"description", p.description}});
            }
        } else {
            return {{"ok", false}, {"error", "unknown command '" + cmd + "'"}};
        }
    } catch (const utils::exception::IException& e) {
        return {{"ok", false}, {"error", std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info())}};
    }
    return {{"ok", true}, {"data", data}};
}
