/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Session.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _Encapsulation
#define _SManip
#include <utils/utils.hpp>
#include "cluster/Session.hpp"
#include "cluster/Display.hpp"
#include "cluster/Models.hpp"
#include "cluster/Tools.hpp"
#include <sys/epoll.h>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <signal.h>
#include <fstream>
#include <fcntl.h>
#include <cstdio>
#include <random>
#include <regex>
#include <tuple>
#include <array>
#include <map>

/* tools */
static thread_local std::vector<cluster::Event> pending_events_; // events found while the lock is held, sent after

_cold static void cloexec_(const int fd)
{
    if (fd >= 0) ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC);
}

_cold static cluster::Usage usage_of_(const cluster::Json& usage)
{
    cluster::Usage result;
    if (!usage.is_object()) return result;
    result.input = usage.value("input_tokens", std::int64_t{0});
    result.output = usage.value("output_tokens", std::int64_t{0});
    result.cacheRead = usage.value("cache_read_input_tokens", std::int64_t{0});
    result.cacheCreation = usage.value("cache_creation_input_tokens", std::int64_t{0});
    return result;
}

_cold static void add_(cluster::Usage& to, const cluster::Usage& from)
{
    to.input += from.input;
    to.output += from.output;
    to.cacheRead += from.cacheRead;
    to.cacheCreation += from.cacheCreation;
}

_cold static std::string tool_summary_(const std::string& name, const cluster::Json& input)
{
    // One line describing a tool call: the command, the file, the pattern...; a question of the agent: its questions
    if (!input.is_object()) return "";
    if (name == "AskUserQuestion" && input.contains("questions") && input["questions"].is_array()) {
        std::string questions;
        for (const cluster::Json& q: input["questions"])
            if (q.is_object()) questions += (questions.empty() ? "" : " / ") + q.value("question", std::string());
        return cluster::one_line(questions, 160);
    }
    for (const char* key: {"command", "file_path", "filePath", "notebook_path", "path", "pattern", "url", "query", "skill", "description", "prompt"})
        if (input.contains(key) && input[key].is_string()) return cluster::one_line(input[key].get<std::string>(), 120);
    return name == "TodoWrite" ? "todo list" : cluster::one_line(input.dump(), 120);
}

_cold static std::string clip_(const std::string& text, const std::size_t max)
{
    // a tool result kept on its lines (the interfaces show the first ones, Ctrl+O all of them)
    std::string out = text.size() > max ? text.substr(0, max) + "\n…" : text;
    while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
    return out;
}

_cold static std::string todos_(const cluster::Json& input)
{
    // TodoWrite: the list as Claude Code shows it (☐ pending, ◼ in progress, ☒ done)
    std::string out;
    for (const cluster::Json& todo: input.value("todos", cluster::Json::array())) {
        const std::string status = todo.value("status", std::string());
        out += std::string(out.empty() ? "" : "\n") + (status == "completed" ? "☒ " : status == "in_progress" ? "◼ " : "☐ ")
            + todo.value("content", std::string());
    }
    return out;
}

struct Prepared {
    std::string shown;          // transcript: [Image: path]
    std::string prompt;         // per-turn agents: @path
    cluster::Json content;      // claude: text + image blocks (empty: the text alone)
};

_cold static Prepared prepare_(const cluster::Queued& queued, const bool blocks)
{
    Prepared out{queued.text, queued.text, cluster::Json::array()};
    for (std::size_t i = 0; i < queued.images.size(); ++i) {
        const std::string& path = queued.images[i];
        const std::string token = "[Image #" + std::to_string(i + 1) + "]";
        for (std::size_t at = out.shown.find(token); at != std::string::npos; at = out.shown.find(token))
            out.shown.replace(at, token.size(), "[Image: " + path + "]");
        for (std::size_t at = out.prompt.find(token); at != std::string::npos; at = out.prompt.find(token))
            out.prompt.replace(at, token.size(), "@" + path);
        if (!blocks) continue;
        std::ifstream file(path, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (bytes.empty()) continue;
        const std::string extension = cluster::lower(std::filesystem::path(path).extension().string());
        const std::string media = extension == ".jpg" || extension == ".jpeg" ? "image/jpeg" : extension == ".gif" ? "image/gif"
            : extension == ".webp" ? "image/webp" : "image/png";
        out.content.push_back(cluster::Json::object({{"type", "image"}, {"source", {{"type", "base64"}, {"media_type", media}, {"data", utils::smanip::codec::Base64Codec().encode(bytes)}}}}));
    }
    // one object (an initializer list given to insert() would add its elements: ["type", "text"], refused by the API)
    if (!out.content.empty()) out.content.insert(out.content.begin(), cluster::Json::object({{"type", "text"}, {"text", queued.text}}));
    return out;
}

_cold static std::optional<std::string> rewind_point_(const std::string& claudeId, const std::string& leaf)
{
    // the transcript of claude (~/.claude/projects/<folder>/<id>.jsonl): from the leaf of the conversation up its
    // parents to the last prompt of the user; the message before it is the cut ("": the start of the conversation)
    std::error_code error;
    std::filesystem::path transcript;
    for (const std::filesystem::directory_entry& folder: std::filesystem::directory_iterator(cluster::expand_home("~/.claude/projects"), error))
        if (std::filesystem::exists(folder.path() / (claudeId + ".jsonl"), error)) transcript = folder.path() / (claudeId + ".jsonl");
    if (transcript.empty()) return std::nullopt;
    std::map<std::string, cluster::Json> messages;
    std::string last;
    std::ifstream file(transcript);
    std::string line;
    while (std::getline(file, line)) {
        const cluster::Json json = cluster::Json::parse(line, nullptr, false);
        if (json.is_discarded() || !json.contains("uuid") || !json["uuid"].is_string() || json.value("isSidechain", false)) continue;
        last = json["uuid"].get<std::string>();
        messages[last] = json;
    }
    std::string at = messages.contains(leaf) ? leaf : last;
    while (messages.contains(at)) {
        const cluster::Json& message = messages.at(at);
        const cluster::Json parent = message.value("parentUuid", cluster::Json());
        if (message.value("isCompactSummary", false)) return std::nullopt; // not before a compaction
        const cluster::Json content = message.contains("message") ? message["message"].value("content", cluster::Json()) : cluster::Json();
        const bool prompt = message.value("type", std::string()) == "user" && !message.value("isMeta", false) && (content.is_string()
            || (content.is_array() && std::none_of(content.begin(), content.end(), [](const cluster::Json& b) {return b.value("type", std::string()) == "tool_result";})));
        if (prompt) return parent.is_string() ? parent.get<std::string>() : std::string();
        if (!parent.is_string()) return std::nullopt;
        at = parent.get<std::string>();
    }
    return std::nullopt;
}

/* constructor */
_cold cluster::Session::Session(cluster::Launch launch)
    : _launch(std::move(launch))
{
    this->_data.spec = this->_launch.spec;
    this->_data.state = cluster::State::Idle;
    this->_data.metrics.cost = this->_launch.spec.cost;
    this->_data.metrics.total.input = this->_launch.spec.tokens; // restored total (the split per kind is not kept)
    this->_data.metrics.costKnown = !this->_launch.backend.provider.local && this->_launch.backend.provider.driver != "opencode";
    if (this->_launch.backend.provider.driver == "claude" && this->_launch.backend.provider.name != "anthropic"
        && this->_launch.backend.provider.name != "anthropic-api")
        this->_data.metrics.costKnown = false; // claude prices other models as if they were Claude ones
    std::error_code error;
    std::filesystem::create_directories(this->_launch.log.parent_path(), error);
    this->_log.open(this->_launch.log, std::ios::app);
}

_cold cluster::Session::~Session()
{
    this->stop();
}

/* process */
_cold std::vector<std::string> cluster::Session::args_(const std::string& prompt) const
{
    const cluster::SessionSpec& spec = this->_data.spec;
    const std::string& driver = this->_launch.backend.provider.driver;
    const std::string& model = this->_launch.backend.model;
    const std::string& mode = spec.mode;
    const std::vector<std::string> effort = cluster::effort_args(driver, spec.effort); // thinking / reasoning
    std::vector<std::string> args = {this->_launch.command};

    if (driver == "claude") {
        args.insert(args.end(), {"-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose",
            "--include-partial-messages", "--replay-user-messages", "--permission-prompt-tool", "stdio"}); // replay: the prompts of claude.ai
        // without it claude refuses to switch to bypassPermissions later (it only makes the mode possible)
        if (this->_launch.allowBypass) args.push_back("--allow-dangerously-skip-permissions");
        if (!mode.empty()) args.insert(args.end(), {"--permission-mode", mode});
        if (!model.empty()) args.insert(args.end(), {"--model", model});
        args.insert(args.end(), effort.begin(), effort.end());
        if (!spec.claudeId.empty()) args.insert(args.end(), {"--resume", spec.claudeId});
        if (!spec.claudeId.empty() && spec.fork) args.push_back("--fork-session"); // a copy: its own id from now on
        if (!spec.claudeId.empty() && !spec.resumeAt.empty()) args.insert(args.end(), {"--resume-session-at", spec.resumeAt}); // after a rewind
    } else if (driver == "qwen") {
        const std::string approval = mode == "plan" ? "plan" : mode == "acceptEdits" || mode == "auto" ? "auto-edit"
            : mode == "bypassPermissions" ? "yolo" : "default";
        args.insert(args.end(), {"-o", "stream-json", "--approval-mode", approval, "--auth-type", "openai"});
        if (!model.empty()) args.insert(args.end(), {"-m", model});
        if (!spec.claudeId.empty()) args.insert(args.end(), {"-r", spec.claudeId});
        args.insert(args.end(), {"-p", prompt});
    } else if (driver == "opencode") {
        args.insert(args.end(), {"run", "--format", "json"});
        if (!model.empty()) args.insert(args.end(), {"-m", model});
        args.insert(args.end(), effort.begin(), effort.end());
        if (!spec.claudeId.empty()) args.insert(args.end(), {"-s", spec.claudeId});
        if (mode == "bypassPermissions") args.push_back("--auto");
        args.insert(args.end(), {"--", prompt});
    } else if (driver == "codex") {
        args.insert(args.end(), {"exec", "--json", "--skip-git-repo-check"});
        if (mode == "bypassPermissions") args.push_back("--dangerously-bypass-approvals-and-sandbox");
        else args.insert(args.end(), {"--sandbox", mode == "acceptEdits" || mode == "auto" ? "workspace-write" : "read-only"});
        if (!model.empty()) args.insert(args.end(), {"-m", model});
        args.insert(args.end(), effort.begin(), effort.end());
        if (!spec.claudeId.empty()) args.insert(args.end(), {"resume", spec.claudeId});
        args.push_back(prompt);
    }
    args.insert(args.end(), this->_launch.extraArgs.begin(), this->_launch.extraArgs.end());
    args.insert(args.end(), spec.extraArgs.begin(), spec.extraArgs.end());
    return args;
}

_cold void cluster::Session::spawn_(const std::vector<std::string>& args, const bool input)
{
    // env -C <cwd> VAR=value... <agent> <args>: the child only dup2 then exec (safe after a fork in a threaded program)
    std::vector<std::string> envArgs = {"-C", cluster::expand_home(this->_data.spec.cwd).string()};
    for (const auto &[name, value]: this->_launch.env)
        envArgs.push_back(name + "=" + value);
    envArgs.insert(envArgs.end(), args.begin(), args.end());

    utils::encapsulation::Pipe in;
    utils::encapsulation::Pipe out;
    utils::encapsulation::Pipe err;
    in.trigger();
    out.trigger();
    err.trigger();
    for (const int fd: {in.getRead(), in.getWrite(), out.getRead(), out.getWrite(), err.getRead(), err.getWrite()})
        cloexec_(fd);
    if (!input) in.closeWrite();

    this->_process = std::make_unique<utils::encapsulation::Process>();
    utils::encapsulation::Dup toIn(in.getRead(), STDIN_FILENO);
    utils::encapsulation::Dup toOut(out.getWrite(), STDOUT_FILENO);
    utils::encapsulation::Dup toErr(err.getWrite(), STDERR_FILENO);
    this->_process->dup(toIn);
    this->_process->dup(toOut);
    this->_process->dup(toErr);
    // a session of its own (setsid, same pid: exec without a fork): the signals of the terminal (its tab closed: SIGHUP)
    // never reach the agent, claude-cluster stops it itself
    if (cluster::has_command("setsid")) {
        envArgs.insert(envArgs.begin(), "env");
        this->_data.pid = static_cast<int>(this->_process->spawn("setsid", envArgs));
    } else {
        this->_data.pid = static_cast<int>(this->_process->spawn("env", envArgs));
    }

    // Parent: keep the write end of stdin and the read ends of the outputs (released from the Pipe objects)
    in.closeRead();
    out.closeWrite();
    err.closeWrite();
    this->_in = input ? in.getWrite() : -1;
    this->_outFd = out.getRead();
    this->_errFd = err.getRead();
    in.setWrite(-1);
    out.setRead(-1);
    err.setRead(-1);
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_data.lastError.clear();
    this->_stderr.clear();
    this->_costBase = this->_data.metrics.cost;
    this->_processCost = 0.0;
}

_cold void cluster::Session::read_(void)
{
    // stdout / stderr of the process read in the worker until both are closed, then the end of the process
    const int outFd = this->_outFd;
    const int errFd = this->_errFd;
    utils::encapsulation::Poll poll;
    poll.link(outFd, EPOLLIN);
    poll.link(errFd, EPOLLIN);
    std::string outBuffer;
    std::string errBuffer;
    std::array<char, 16384> chunk{};
    int open = 2;
    while (open > 0) {
        for (const epoll_event& event: poll.wait(500)) {
            const int fd = event.data.fd;
            const ssize_t size = ::read(fd, chunk.data(), chunk.size());
            if (size <= 0) {
                poll.unlink(fd);
                ::close(fd);
                --open;
                continue;
            }
            std::string& buffer = fd == outFd ? outBuffer : errBuffer;
            buffer.append(chunk.data(), static_cast<std::size_t>(size));
            for (std::size_t nl = buffer.find('\n'); nl != std::string::npos; nl = buffer.find('\n')) {
                const std::string line = buffer.substr(0, nl);
                buffer.erase(0, nl + 1);
                if (fd == outFd) {
                    this->line_(line);
                } else {
                    std::lock_guard<std::mutex> lock(this->_mutex);
                    this->_stderr = (this->_stderr + line + "\n").substr(this->_stderr.size() > 4000 ? this->_stderr.size() - 4000 : 0);
                }
            }
        }
    }
    const utils::encapsulation::Status status = this->_process->wait();
    if (this->_in != -1) {
        ::close(this->_in);
        this->_in = -1;
    }
    this->finished_(status.exited ? status.code : 128 + status.sig);
}

_cold void cluster::Session::start(void)
{
    if (!this->persistent() || this->_worker.joinable()) return;
    this->_stopping = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_data.state = cluster::State::Starting;
    }
    this->touch_();
    // The process is started here (an error goes to the caller), read by the worker
    try {
        this->spawn_(this->args_(""), true);
    } catch (const utils::exception::IException& e) {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_data.state = cluster::State::Error;
        this->_data.lastError = std::string(e.what()) + ": " + e.info();
        throw;
    }
    {
        // claude waits for the first message before its init: ready as soon as it runs
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_data.state == cluster::State::Starting) this->_data.state = cluster::State::Idle;
        // its initialize answered once it is loaded (its tools, MCP): "ready" (claude-cluster ready, its notification)
        if (this->_launch.backend.provider.driver == "claude" && this->_in != -1)
            this->write_({{"type", "control_request"}, {"request_id", "init-" + std::to_string(cluster::now())}, {"request", {{"subtype", "initialize"}}}});
        if (!this->_launch.remoteName.empty() && this->_launch.backend.provider.driver == "claude" && this->_in != -1)
            this->write_({{"type", "control_request"}, {"request_id", "rc-on-" + std::to_string(cluster::now())},
                {"request", {{"subtype", "remote_control"}, {"enabled", true}, {"name", this->_launch.remoteName}}}});
    }
    this->_worker = std::thread(&cluster::Session::read_, this);
}

_cold void cluster::Session::restart(void)
{
    // not from the reader thread: it is joined here
    if (this->_worker.joinable()) this->_worker.join();
    this->start();
}

_cold void cluster::Session::runTurns_(void)
{
    while (!this->_stopping) {
        std::string prompt;
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (this->_pending.empty()) {
                this->_busy = false;
                return;
            }
            const Prepared prepared = prepare_(this->_pending.front(), false);
            prompt = prepared.prompt;
            this->entry_(cluster::EntryKind::User, prepared.shown);
            this->_pending.pop_front();
            this->queue_();
            this->_data.state = cluster::State::Working;
            this->_turn = {};
            this->turnStart_();
            this->_data.metrics.lastCost = 0.0; // a process per turn: its cost added step by step
            this->_lastText.clear();
        }
        this->touch_();
        try {
            this->spawn_(this->args_(prompt), false);
            this->read_();
        } catch (const utils::exception::IException& e) {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_data.state = cluster::State::Error;
            this->_data.lastError = std::string(e.what()) + ": " + e.info();
        }
    }
    this->_busy = false;
}

_cold void cluster::Session::finished_(int code)
{
    std::vector<cluster::Event> events;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        const std::string error = cluster::trim(this->_stderr);
        if (this->persistent()) {
            this->_data.state = this->_stopping ? cluster::State::Stopped : cluster::State::Error;
            if (!this->_stopping) {
                this->_data.lastError = "agent exited (" + std::to_string(code) + ")" + (error.empty() ? "" : ": " + cluster::one_line(error, 300));
                events.push_back({cluster::Event::Kind::Exited, this->_data.spec.id, this->_data.lastError, true});
            }
        } else {
            // per-turn driver: the end of the process is the end of the turn
            const bool interrupted = std::exchange(this->_interrupted, false);
            const bool failed = code != 0 && !this->_stopping && !interrupted;
            if (failed) this->_data.lastError = "exit " + std::to_string(code) + (error.empty() ? "" : ": " + cluster::one_line(error, 300));
            add_(this->_data.metrics.total, this->_turn);
            this->_data.metrics.last = this->_turn;
            this->_data.metrics.current = {};
            this->_data.metrics.turns++;
            this->_data.state = this->_stopping ? cluster::State::Stopped : failed ? cluster::State::Error : cluster::State::Idle;
            if (failed) {
                cluster::Entry entry{cluster::EntryKind::Error, this->_data.lastError, "", cluster::now()};
                this->_data.entries.push_back(entry);
            }
            events.push_back({cluster::Event::Kind::TurnDone, this->_data.spec.id, failed ? this->_data.lastError : this->_lastText, failed, interrupted});
        }
    }
    this->touch_();
    for (const cluster::Event& event: events)
        if (this->_launch.listener) this->_launch.listener(event);
}

/* parsing */
_cold void cluster::Session::line_(const std::string& line)
{
    if (cluster::trim(line).empty()) return;
    cluster::Json event;
    try {
        event = cluster::Json::parse(line);
    } catch (const cluster::Json::exception&) {
        return; // not a json line (banner, warning): ignored
    }
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        const std::string& driver = this->_launch.backend.provider.driver;
        try {
            if (driver == "opencode") this->opencode_(event);
            else if (driver == "codex") this->codex_(event);
            else this->claude_(event);
        } catch (const cluster::Json::exception& e) {
            // an event of an unexpected shape (a null where a text was, a new field type): left out, never the end of claude-cluster
            this->_stderr += std::string("event left out (") + e.what() + "): " + cluster::one_line(line, 200) + "\n";
        }
    }
    this->touch_();
    std::vector<cluster::Event> events;
    events.swap(pending_events_);
    for (const cluster::Event& e: events)
        if (this->_launch.listener) this->_launch.listener(e);
}

_cold void cluster::Session::claude_(const cluster::Json& event)
{
    const std::string type = event.value("type", std::string());
    const std::string subtype = event.value("subtype", std::string());
    cluster::Metrics& metrics = this->_data.metrics;

    if (type == "system" && subtype == "init") {
        this->_data.spec.claudeId = event.value("session_id", this->_data.spec.claudeId);
        this->_data.spec.fork = false; // forked once: the copy goes on under its own id
        this->_data.modelUsed = event.value("model", std::string());
        if (event.contains("permissionMode")) this->_data.spec.mode = event.value("permissionMode", this->_data.spec.mode);
        this->_data.skillsAvailable.clear();
        this->_data.slashCommands.clear(); // the ones of initialize too: told again here
        if (event.contains("slash_commands") && event["slash_commands"].is_array())
            for (const cluster::Json& command: event["slash_commands"])
                if (command.is_string()) this->_data.slashCommands.push_back(command.get<std::string>());
        if (event.contains("skills") && event["skills"].is_array())
            for (const cluster::Json& skill: event["skills"])
                if (skill.is_string()) this->_data.skillsAvailable.push_back(skill.get<std::string>());
        if (this->_data.state == cluster::State::Starting) this->_data.state = cluster::State::Idle;
        this->emit_(cluster::Event::Kind::Init);
    } else if (type == "system" && subtype == "bridge_state") {
        const std::string state = event.value("state", std::string());
        if (state != this->_data.bridgeState && (state == "disconnected" || state == "failed" || state == "error"))
            this->entry_(cluster::EntryKind::Error, "Remote Control: " + state);
        this->_data.bridgeState = state;
    } else if (type == "system" && subtype == "compact_boundary") {
        this->entry_(cluster::EntryKind::System, "context compacted");
    } else if (type == "system" && subtype == "status") {
        // the request sent / the context being compacted (a null status: only its permission mode told)
        const std::string status = event.contains("status") && event["status"].is_string() ? event["status"].get<std::string>() : "";
        if (status == "requesting") this->step_("requesting");
        else if (status == "compacting") this->step_("compacting");
    } else if (type == "system" && subtype == "thinking_tokens") {
        // the thinking estimated while its text is not sent
        this->step_("thinking");
        this->_data.stepDetail = "~" + cluster::human_tokens(event.value("estimated_tokens", static_cast<std::int64_t>(0))) + " tokens of thinking (its text not sent by the model)";
    } else if (type == "system" && subtype == "api_retry") {
        // an error of the API: tried again after a delay
        const std::string error = event.value("error", cluster::Json("")).is_string() ? event.value("error", std::string()) : event["error"].dump();
        const cluster::Json status = event.value("error_status", cluster::Json());
        this->step_("retry");
        this->_data.stepDetail = "API error" + (status.is_number() ? " " + std::to_string(status.get<int>()) : std::string()) + (error.empty() ? "" : " (" + error + ")")
            + ", attempt " + std::to_string(event.value("attempt", 0)) + "/" + std::to_string(event.value("max_retries", 0)) + ", again in "
            + std::to_string(event.value("retry_delay_ms", 0) / 1000) + " s";
    } else if (type == "system" && subtype == "model_fallback") {
        this->entry_(cluster::EntryKind::System, "model fallback: " + event.value("original_model", std::string()) + " → " + event.value("fallback_model", std::string())
            + (event.value("content", std::string()).empty() ? "" : " (" + cluster::one_line(event.value("content", std::string()), 160) + ")"));
    } else if (type == "system" && subtype == "hook_started" && this->_data.state == cluster::State::Working) {
        this->step_("hook");
        this->_data.stepDetail = event.value("hook_name", std::string());
    } else if (type == "system" && subtype == "hook_response" && this->_data.step == "hook") {
        this->step_("model");
    } else if (type == "stream_event" && event.contains("event")) {
        const cluster::Json& e = event["event"];
        const std::string kind = e.value("type", std::string());
        if (kind == "message_start" && e.contains("message")) {
            this->_live = usage_of_(e["message"].value("usage", cluster::Json::object()));
            metrics.contextUsed = this->_live.context();
            // the model has the request: it reads it (and thinks, its text maybe not sent) before its first words
            const std::int64_t context = this->_live.context();
            this->step_("reading");
            if (context > 0)
                this->_data.stepDetail = cluster::human_tokens(context) + " tokens of context" + (this->_live.cacheRead > 0 ? " (" + std::to_string(this->_live.cacheRead * 100 / context)
                    + " % cached)" : std::string());
        } else if (kind == "message_delta" && e.contains("usage")) {
            const cluster::Usage delta = usage_of_(e["usage"]);
            this->_live.output = delta.output;
            if (delta.input + delta.cacheRead + delta.cacheCreation > 0) {
                this->_live.input = delta.input;
                this->_live.cacheRead = delta.cacheRead;
                this->_live.cacheCreation = delta.cacheCreation;
            }
            metrics.contextUsed = this->_live.context();
        } else if (kind == "message_stop") {
            add_(this->_turn, this->_live);
            this->_live = {};
            const bool running = std::any_of(this->_data.tools.begin(), this->_data.tools.end(), [](const cluster::ToolRun& t) {return !t.done;});
            this->step_(running ? "tools" : "model");
        } else if (kind == "content_block_start" && e.contains("content_block")) {
            // the step of the turn: thinking, writing, a tool being written
            const cluster::Json& block = e["content_block"];
            const std::string block_type = block.value("type", std::string());
            if (block_type == "thinking" || block_type == "redacted_thinking") this->step_("thinking");
            else if (block_type == "text") this->step_("writing");
            else if (block_type == "tool_use" || block_type == "server_tool_use") this->step_(block.value("name", std::string("tool")));
        } else if (kind == "content_block_delta" && e.contains("delta") && e["delta"].value("type", std::string()) == "text_delta") {
            this->_data.partial += e["delta"].value("text", std::string());
            this->_data.streamed += e["delta"].value("text", std::string()).size();
        } else if (kind == "content_block_delta" && e.contains("delta") && e["delta"].value("type", std::string()) == "thinking_delta") {
            this->_data.thinking += e["delta"].value("thinking", std::string());
            this->_data.streamed += e["delta"].value("thinking", std::string()).size();
        } else if (kind == "content_block_delta" && e.contains("delta") && e["delta"].value("type", std::string()) == "input_json_delta") {
            this->_data.streamed += e["delta"].value("partial_json", std::string()).size();
        }
        if (kind == "message_delta" && e.contains("usage") && e["usage"].contains("output_tokens_details"))
            this->_data.thinkingTokens += e["usage"]["output_tokens_details"].value("thinking_tokens", static_cast<std::int64_t>(0));
        metrics.current = this->_turn;
        add_(metrics.current, this->_live);
    } else if (type == "assistant" && event.contains("message")) {
        if (!event.value("parent_tool_use_id", cluster::Json()).is_null()) return; // inside a sub-agent
        if (event.contains("uuid") && event["uuid"].is_string()) this->_data.spec.leaf = event["uuid"].get<std::string>();
        for (const cluster::Json& block: event["message"].value("content", cluster::Json::array())) {
            const std::string kind = block.value("type", std::string());
            if (kind == "text") {
                this->_lastText = block.value("text", std::string());
                this->entry_(cluster::EntryKind::Assistant, this->_lastText);
                this->_data.partial.clear();
            } else if (kind == "thinking") {
                this->entry_(cluster::EntryKind::Thinking, block.value("thinking", std::string()));
                this->_data.thinking.clear();
            } else if (kind == "tool_use") {
                this->tool_(block.value("id", std::string()), block.value("name", std::string()), block.value("input", cluster::Json::object()));
                this->step_("tools");
                if (this->_inputs.size() > 200) this->_inputs.erase(this->_inputs.begin());
                this->_inputs[block.value("id", std::string())] = {{"name", block.value("name", std::string())}, {"input", block.value("input", cluster::Json::object())}};
            }
        }
        if (this->persistent() && this->_data.state != cluster::State::Waiting) this->_data.state = cluster::State::Working;
        // per-turn drivers (qwen) don't stream: the usage of the message is the one of the turn so far
        if (!this->persistent()) {
            add_(this->_turn, usage_of_(event["message"].value("usage", cluster::Json::object())));
            metrics.current = this->_turn;
        }
    } else if (type == "user" && event.contains("message")) {
        if (event.contains("uuid") && event["uuid"].is_string() && event.value("parent_tool_use_id", cluster::Json()).is_null())
            this->_data.spec.leaf = event["uuid"].get<std::string>();
        const cluster::Json content = event["message"].value("content", cluster::Json());
        // the replay of a prompt: written here (its uuid known: already shown) or sent from claude.ai (Remote Control)
        const std::string uuid = event.value("uuid", std::string());
        if (event.value("isReplay", false) && this->_sent.erase(uuid) > 0) return;
        if (event.value("isReplay", false) && event.value("parent_tool_use_id", cluster::Json()).is_null() && !event.value("isSynthetic", false)
            && !event.value("isMeta", false)) {
            std::string text;
            if (content.is_string()) text = content.get<std::string>();
            else if (content.is_array())
                for (const cluster::Json& block: content)
                    if (block.value("type", std::string()) == "text") text += block.value("text", std::string());
            const bool tool = content.is_array() && std::any_of(content.begin(), content.end(), [](const cluster::Json& b) {return b.value("type", std::string()) == "tool_result";});
            if (!tool && !cluster::trim(text).empty() && !cluster::trim(text).starts_with("<") && text.find("[Request interrupted") == std::string::npos) {
                this->entry_(cluster::EntryKind::User, text);
                this->_data.state = cluster::State::Working;
                this->_turn = {};
                this->turnStart_();
                this->emit_(cluster::Event::Kind::Prompt, text); // the history of the prompts too
            }
            return;
        }
        if (!content.is_array()) return;
        for (const cluster::Json& block: content) {
            if (block.value("type", std::string()) != "tool_result") continue;
            const std::string id = block.value("tool_use_id", std::string());
            const bool error = block.value("is_error", false);
            for (cluster::ToolRun& run: this->_data.tools)
                if (run.id == id) {
                    run.done = true;
                    run.error = error;
                }
            if (std::none_of(this->_data.tools.begin(), this->_data.tools.end(), [](const cluster::ToolRun& t) {return !t.done;}) && this->_data.step == "tools")
                this->step_("model");
            std::string text;
            if (block.contains("content") && block["content"].is_string()) text = block["content"].get<std::string>();
            else if (block.contains("content") && block["content"].is_array())
                for (const cluster::Json& part: block["content"])
                    text += part.value("text", std::string());
            if (error && text.find("auto mode classifier gave no verdict") != std::string::npos) {
                this->noVerdict_(this->_inputs.contains(id) ? this->_inputs[id].value("name", std::string("a tool")) : std::string("a tool"));
                continue;
            }
            if (!error) this->_noVerdict = 0;
            this->entry_(error ? cluster::EntryKind::Error : cluster::EntryKind::ToolResult, clip_(text, 6000));
            if (error && text.find("denied by a built-in Claude Code safety check") != std::string::npos && this->_inputs.contains(id)) {
                // a mode that never asks (auto, dontAsk): the safety check denied it at once; asked here instead, allowed it runs again
                const cluster::Json& tool = this->_inputs[id];
                cluster::Permission permission;
                permission.requestId = "safety-" + id;
                permission.tool = tool.value("name", std::string());
                permission.input = tool.value("input", cluster::Json::object());
                permission.description = "stopped by the safety check of Claude Code: " + tool_summary_(permission.tool, permission.input);
                const std::size_t flagged = text.find("What was flagged:");
                permission.reason = flagged == std::string::npos ? "only a person may approve it" : cluster::one_line(text.substr(flagged + 18), 240);
                permission.time = cluster::now();
                permission.safety = true;
                this->_data.permissions.push_back(permission);
                this->emit_(cluster::Event::Kind::Permission, permission.tool + ": " + cluster::one_line(permission.description, 120));
            }
            this->_inputs.erase(id);
        }
    } else if (type == "control_request" && event.contains("request")) {
        const cluster::Json& request = event["request"];
        if (request.value("subtype", std::string()) != "can_use_tool") return;
        cluster::Permission permission;
        permission.requestId = event.value("request_id", std::string());
        permission.tool = request.value("tool_name", std::string());
        permission.description = request.value("description", tool_summary_(permission.tool, request.value("input", cluster::Json::object())));
        permission.input = request.value("input", cluster::Json::object());
        permission.suggestions = request.value("permission_suggestions", cluster::Json::array());
        permission.reason = request.value("decision_reason", std::string());
        permission.time = cluster::now();
        if (!this->_approved.empty() && permission.input.dump() == this->_approved) {
            this->write_({{"type", "control_response"}, {"response", {{"subtype", "success"}, {"request_id", permission.requestId},
                {"response", {{"behavior", "allow"}, {"updatedInput", permission.input}}}}}});
            this->entry_(cluster::EntryKind::System, "allowed " + permission.tool + " (the user allowed it after the safety check)");
            this->_approved.clear();
            return;
        }
        this->_data.permissions.push_back(permission);
        this->_data.state = cluster::State::Waiting;
        this->emit_(cluster::Event::Kind::Permission, permission.tool + ": " + cluster::one_line(permission.description, 120));
    } else if (type == "control_response" && event.contains("response")) {
        const cluster::Json& response = event["response"];
        if (response.value("request_id", std::string()).starts_with("init-")) {
            // its commands and skills (the user's, its plugins'), with their description: completed from the start
            const cluster::Json answer = response.value("response", cluster::Json::object());
            if (answer.contains("commands") && answer["commands"].is_array()) {
                for (const cluster::Json& command: answer["commands"]) {
                    const std::string name = command.value("name", std::string());
                    if (name.empty()) continue;
                    if (std::find(this->_data.slashCommands.begin(), this->_data.slashCommands.end(), name) == this->_data.slashCommands.end())
                        this->_data.slashCommands.push_back(name);
                    this->_data.commandHelp[name] = cluster::one_line(command.value("description", std::string()), 140);
                }
            }
            if (response.value("subtype", std::string()) != "error") this->emit_(cluster::Event::Kind::Init);
            return;
        }
        if (response.value("subtype", std::string()) == "error") {
            this->entry_(cluster::EntryKind::Error, response.value("error", std::string("request refused by the agent")));
            if (response.value("request_id", std::string()).starts_with("rc-on-")) this->_launch.remoteName.clear();
        } else if (response.contains("response") && response["response"].is_object() && response["response"].contains("session_url")) {
            this->_data.remoteUrl = response["response"].value("session_url", std::string());
            this->entry_(cluster::EntryKind::System, "Remote Control on: " + this->_data.remoteUrl);
        } else if (response.contains("response") && response["response"].is_object() && response["response"].contains("mode")) {
            this->_data.spec.mode = response["response"].value("mode", this->_data.spec.mode);
            this->entry_(cluster::EntryKind::System, "permission mode: " + this->_data.spec.mode);
        }
    } else if (type == "control_cancel_request") {
        const std::string id = event.value("request_id", std::string());
        const std::size_t before = this->_data.permissions.size();
        std::erase_if(this->_data.permissions, [&](const cluster::Permission& p) {return p.requestId == id;});
        if (this->_data.permissions.size() != before) this->entry_(cluster::EntryKind::System, "permission request cancelled by the agent");
        if (this->_data.permissions.empty() && this->_data.state == cluster::State::Waiting) this->_data.state = cluster::State::Working;
    } else if (type == "rate_limit_event" && event.contains("rate_limit_info")) {
        const cluster::Json windows = event["rate_limit_info"].value("unifiedWindows", cluster::Json::object());
        // utilization 0..1 and the reset (unix seconds, or milliseconds) when it is told
        const std::function<std::int64_t(const cluster::Json&)> reset = [](const cluster::Json& window) -> std::int64_t {
            for (const char* key: {"resets_at", "resetsAt", "reset_at"}) {
                if (!window.contains(key) || !window[key].is_number()) continue;
                const std::int64_t value = window[key].get<std::int64_t>();
                return value > 100000000000 ? value / 1000 : value;
            }
            return 0;
        };
        if (windows.contains("five_hour")) {
            metrics.limit5h = windows["five_hour"].value("utilization", -1.0);
            if (const std::int64_t at = reset(windows["five_hour"]); at > 0) metrics.reset5h = at;
        }
        if (windows.contains("seven_day")) {
            metrics.limit7d = windows["seven_day"].value("utilization", -1.0);
            if (const std::int64_t at = reset(windows["seven_day"]); at > 0) metrics.reset7d = at;
        }
    } else if (type == "result") {
        const cluster::Usage usage = usage_of_(event.value("usage", cluster::Json::object()));
        metrics.last = usage.total() > 0 ? usage : this->_turn;
        if (this->persistent()) add_(metrics.total, metrics.last);
        else this->_turn = metrics.last; // added to the total at the end of the process
        metrics.current = {};
        this->_turn = this->persistent() ? cluster::Usage{} : this->_turn;
        this->_live = {};
        metrics.turns += this->persistent() ? 1 : 0;
        this->step_("");
        this->_noVerdict = 0;
        if (!this->_modeBack.empty() && metrics.turns >= this->_restoreAfter) {
            // the command allowed after the safety check ran (or was not asked again): its mode back
            this->write_({{"type", "control_request"}, {"request_id", "mode-back-" + std::to_string(cluster::now())},
                {"request", {{"subtype", "set_permission_mode"}, {"mode", this->_modeBack}}}});
            this->_modeBack.clear();
            this->_approved.clear();
        }
        // total_cost_usd is cumulative over the life of the process (a restart begins again at 0)
        const double cost = event.value("total_cost_usd", 0.0);
        metrics.lastCost = std::max(0.0, cost - this->_processCost);
        this->_processCost = cost;
        metrics.cost = this->_costBase + cost;
        if (event.contains("modelUsage") && event["modelUsage"].is_object())
            for (const auto &[model, data]: event["modelUsage"].items())
                metrics.contextWindow = std::max<std::int64_t>(data.value("contextWindow", std::int64_t{0}), 1000);
        const bool interrupted = std::exchange(this->_interrupted, false); // its end is not an error
        const bool error = event.value("is_error", false) && !interrupted;
        const std::string text = event.contains("result") && event["result"].is_string() ? event["result"].get<std::string>() : this->_lastText;
        if (error) this->entry_(cluster::EntryKind::Error, text.empty() ? subtype : text);
        this->_data.partial.clear();
        if (this->persistent()) {
            // a stop of the safety check shown to the user is no request claude waits for: the turn is over
            const bool asked = std::any_of(this->_data.permissions.begin(), this->_data.permissions.end(), [](const cluster::Permission& p) {return !p.safety;});
            this->_data.state = asked ? cluster::State::Waiting : cluster::State::Idle;
            this->emit_(cluster::Event::Kind::TurnDone, text, error, interrupted);
            if (!this->_pending.empty() && this->_data.state == cluster::State::Idle) {
                const cluster::Queued next = this->_pending.front();
                this->_pending.pop_front();
                this->queue_();
                this->dispatch_(next);
            }
        } else {
            this->_lastText = text;
        }
    }
}

_cold void cluster::Session::opencode_(const cluster::Json& event)
{
    const std::string type = event.value("type", std::string());
    const cluster::Json part = event.value("part", cluster::Json::object());

    if (event.contains("sessionID")) this->_data.spec.claudeId = event.value("sessionID", this->_data.spec.claudeId);
    if (type == "text") {
        this->_lastText = part.value("text", std::string());
        this->entry_(cluster::EntryKind::Assistant, this->_lastText);
    } else if (type == "reasoning") {
        this->entry_(cluster::EntryKind::Thinking, part.value("text", std::string()));
    } else if (type == "tool_use") {
        const cluster::Json state = part.value("state", cluster::Json::object());
        const std::string id = part.value("callID", std::string());
        this->tool_(id, part.value("tool", std::string()), state.value("input", cluster::Json::object()));
        const bool error = state.value("status", std::string()) == "error";
        for (cluster::ToolRun& run: this->_data.tools)
            if (run.id == id) {
                run.done = true;
                run.error = error;
            }
        const cluster::Json output = state.value("output", cluster::Json());
        this->entry_(error ? cluster::EntryKind::Error : cluster::EntryKind::ToolResult, clip_(output.is_string() ? output.get<std::string>() : output.dump(), 6000));
    } else if (type == "step_finish") {
        const cluster::Json tokens = part.value("tokens", cluster::Json::object());
        cluster::Usage usage;
        usage.input = tokens.value("input", std::int64_t{0});
        usage.output = tokens.value("output", std::int64_t{0}) + tokens.value("reasoning", std::int64_t{0});
        usage.cacheRead = tokens.contains("cache") ? tokens["cache"].value("read", std::int64_t{0}) : 0;
        usage.cacheCreation = tokens.contains("cache") ? tokens["cache"].value("write", std::int64_t{0}) : 0;
        add_(this->_turn, usage);
        this->_data.metrics.current = this->_turn;
        this->_data.metrics.contextUsed = usage.context();
        const double cost = part.value("cost", 0.0);
        if (cost > 0) this->_data.metrics.costKnown = true; // a paid provider of opencode gives its real cost
        this->_data.metrics.cost += cost;
        this->_data.metrics.lastCost += cost;
    } else if (type == "error") {
        const cluster::Json error = event.value("error", cluster::Json::object());
        const std::string message = error.contains("data") ? error["data"].value("message", error.dump()) : error.dump();
        this->entry_(cluster::EntryKind::Error, message);
    }
}

_cold void cluster::Session::codex_(const cluster::Json& event)
{
    const std::string type = event.value("type", std::string());

    if (type == "thread.started") {
        this->_data.spec.claudeId = event.value("thread_id", this->_data.spec.claudeId);
    } else if (type == "item.started" || type == "item.completed") {
        const cluster::Json item = event.value("item", cluster::Json::object());
        const std::string kind = item.value("type", std::string());
        const std::string id = item.value("id", std::string());
        if (kind == "agent_message" && type == "item.completed") {
            this->_lastText = item.value("text", std::string());
            this->entry_(cluster::EntryKind::Assistant, this->_lastText);
        } else if (kind == "reasoning" && type == "item.completed") {
            this->entry_(cluster::EntryKind::Thinking, item.value("text", std::string()));
        } else if (kind == "command_execution") {
            if (type == "item.started") {
                this->tool_(id, "shell", {{"command", item.value("command", std::string())}});
                return;
            }
            const bool error = item.value("exit_code", 0) != 0;
            for (cluster::ToolRun& run: this->_data.tools)
                if (run.id == id) {
                    run.done = true;
                    run.error = error;
                }
            this->entry_(error ? cluster::EntryKind::Error : cluster::EntryKind::ToolResult, clip_(item.value("aggregated_output", std::string()), 6000));
        } else if (kind == "file_change" && type == "item.completed") {
            for (const cluster::Json& change: item.value("changes", cluster::Json::array())) {
                const std::string path = change.value("path", std::string());
                if (std::find(this->_data.files.begin(), this->_data.files.end(), path) == this->_data.files.end()) this->_data.files.push_back(path);
                this->entry_(cluster::EntryKind::Tool, change.value("kind", std::string("edit")) + " " + path, "file_change");
            }
        } else if (kind == "mcp_tool_call" && type == "item.started") {
            this->tool_(id, item.value("server", std::string()) + "." + item.value("tool", std::string()), item.value("arguments", cluster::Json::object()));
        }
    } else if (type == "turn.completed") {
        const cluster::Json usage = event.value("usage", cluster::Json::object());
        cluster::Usage turn;
        turn.input = usage.value("input_tokens", std::int64_t{0});
        turn.cacheRead = usage.value("cached_input_tokens", std::int64_t{0});
        turn.output = usage.value("output_tokens", std::int64_t{0});
        this->_turn = turn;
        this->_data.metrics.contextUsed = turn.context();
    } else if (type == "turn.failed" || type == "error") {
        const cluster::Json error = event.value("error", cluster::Json::object());
        this->entry_(cluster::EntryKind::Error, error.is_object() ? error.value("message", event.dump()) : event.value("message", event.dump()));
    }
}

_cold void cluster::Session::tool_(const std::string& id, const std::string& name, const cluster::Json& input)
{
    const std::string summary = tool_summary_(name, input);
    this->_data.tools.push_back({id, name, summary, false, false, cluster::now()});
    if (this->_data.tools.size() > 200) this->_data.tools.erase(this->_data.tools.begin());
    this->entry_(cluster::EntryKind::Tool, name == "TodoWrite" ? todos_(input) : summary, name);

    // Skills loaded and files modified, for their panels
    if (name == "Skill") {
        const std::string skill = input.value("skill", input.value("command", std::string()));
        if (!skill.empty() && std::find(this->_data.skillsLoaded.begin(), this->_data.skillsLoaded.end(), skill) == this->_data.skillsLoaded.end())
            this->_data.skillsLoaded.push_back(skill);
    }
    static const std::array<const char*, 7> editors = {"Edit", "Write", "MultiEdit", "NotebookEdit", "edit", "write", "patch"};
    if (std::find_if(editors.begin(), editors.end(), [&](const char* e) {return name == e;}) != editors.end()) {
        for (const char* key: {"file_path", "filePath", "notebook_path", "path"}) {
            if (!input.contains(key) || !input[key].is_string()) continue;
            const std::string path = input[key].get<std::string>();
            if (std::find(this->_data.files.begin(), this->_data.files.end(), path) == this->_data.files.end()) this->_data.files.push_back(path);
            break;
        }
    }
}

_cold void cluster::Session::append(const std::vector<cluster::Entry>& entries)
{
    // entries of a transcript from elsewhere (another agent, the session forked), shown and kept in its log
    std::lock_guard<std::mutex> lock(this->_mutex);
    for (const cluster::Entry& entry: entries) {
        this->_data.entries.push_back(entry);
        if (this->_log) this->_log << cluster::Json{{"time", entry.time}, {"kind", static_cast<int>(entry.kind)}, {"text", entry.text}, {"tool", entry.tool}}.dump() << "\n";
    }
    if (this->_data.entries.size() > 2000) this->_data.entries.erase(this->_data.entries.begin(), this->_data.entries.end() - 1500);
    if (this->_log) this->_log.flush();
    this->touch_();
}

static std::int64_t ms_(void)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void cluster::Session::step_(const std::string& name)
{
    if (name == this->_data.step) return;
    if (name != "thinking" || this->_data.step != "reading") this->_data.stepDetail.clear(); // the thinking keeps the context told
    const std::int64_t now = ms_();
    if (!this->_data.step.empty() && !(this->_data.step == "tools" && now - this->_data.stepSince < 300)) { // between two tools of a message: left out
        this->_data.steps.emplace_back(this->_data.step, now - this->_data.stepSince);
        if (this->_data.steps.size() > 60) this->_data.steps.erase(this->_data.steps.begin());
    }
    this->_data.step = name;
    this->_data.stepSince = now;
}

void cluster::Session::noVerdict_(const std::string& tool)
{
    // the classifier of the auto mode answered nothing (a failure on its side, not a refusal): one line counted; two in a
    // row: the rest of the turn in manual mode (the tool asked to the user instead of failing again), auto put back after
    ++this->_noVerdict;
    const std::string line = "auto mode: the safety classifier gave no verdict for " + tool + " (a transient failure on its side, retried)";
    if (!this->_data.entries.empty() && this->_data.entries.back().kind == cluster::EntryKind::Error && this->_data.entries.back().text.starts_with(line))
        this->_data.entries.back().text = line + " ×" + std::to_string(this->_noVerdict);
    else
        this->entry_(cluster::EntryKind::Error, line);
    if (this->_noVerdict < 2 || !this->_modeBack.empty() || this->_data.spec.mode != "auto") return;
    this->_modeBack = "auto";
    this->_restoreAfter = this->_data.metrics.turns + 1;
    this->write_({{"type", "control_request"}, {"request_id", "mode-verdict-" + std::to_string(cluster::now())},
        {"request", {{"subtype", "set_permission_mode"}, {"mode", "default"}}}});
    this->entry_(cluster::EntryKind::System, "the classifier of the auto mode does not answer: manual mode for the rest of this turn (its tools asked to "
        "you instead of failing), auto put back after it");
}

void cluster::Session::turnStart_(void)
{
    this->_data.steps.clear();
    this->_data.streamed = 0;
    this->_data.thinkingTokens = 0;
    this->_data.step = "model";
    this->_data.stepSince = this->_data.turnSince = ms_();
}

_cold void cluster::Session::updateDisplay(const std::string& displayId, const std::string& text)
{
    // the last display of this id replaced; the log written again with it (the next start shows it so)
    std::lock_guard<std::mutex> lock(this->_mutex);
    const std::string mark = "\"id\":\"" + displayId + "\"";
    for (auto it = this->_data.entries.rbegin(); it != this->_data.entries.rend(); ++it)
        if (it->kind == cluster::EntryKind::Display && it->text.find(mark) != std::string::npos) {
            it->text = text;
            break;
        }
    std::ifstream in(this->_launch.log);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line))
        lines.push_back(line);
    in.close();
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        if (it->find(mark) == std::string::npos) continue;
        cluster::Json json = cluster::Json::parse(*it, nullptr, false);
        if (!json.is_object() || json.value("kind", 0) != static_cast<int>(cluster::EntryKind::Display)) continue;
        json["text"] = text;
        *it = json.dump();
        break;
    }
    this->_log.close();
    std::ofstream out(this->_launch.log, std::ios::trunc);
    for (const std::string& kept: lines)
        out << kept << "\n";
    out.close();
    this->_log.open(this->_launch.log, std::ios::app);
    this->touch_();
}

_cold static std::vector<std::string> files_named_(const std::string& text, const std::string& cwd)
{
    // the files an answer names that exist (made by the agent: pictures, documents, archives, apps...), 6 at most
    static const std::regex named(R"((?:^|[\s`'"(\[])((?:~|\.{1,2})?/?[\w@%+=:,./-]*[\w-]\.(?:png|jpe?g|gif|webp|svg|pdf|zip|tar\.gz|tgz|7z|apk|mp3|wav|ogg|mp4|webm|csv|docx|xlsx|pptx|epub))(?=$|[\s`'")\].,;:!?]))",
        std::regex::icase);
    std::vector<std::string> found;
    for (std::sregex_iterator it(text.begin(), text.end(), named), end; it != end && found.size() < 6; ++it) {
        std::filesystem::path path = cluster::expand_home((*it)[1].str());
        if (path.is_relative()) path = std::filesystem::path(cluster::expand_home(cwd)) / path;
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error)) continue;
        const std::string full = std::filesystem::weakly_canonical(path, error).string();
        if (std::find(found.begin(), found.end(), full) == found.end()) found.push_back(full);
    }
    return found;
}

_cold void cluster::Session::entry_(const cluster::EntryKind kind, const std::string& text, const std::string& tool)
{
    if (text.empty() && kind != cluster::EntryKind::Tool) return;
    cluster::Entry entry{kind, text, tool, cluster::now()};
    this->_data.entries.push_back(entry);
    if (kind == cluster::EntryKind::Assistant) { // the files it names: their card under it (open, download)
        const std::vector<std::string> files = files_named_(text, this->_data.spec.cwd);
        if (!files.empty()) {
            try {
                const cluster::Json card = cluster::display_prepare({{"type", "file"}, {"paths", files}, {"auto", true}});
                this->entry_(cluster::EntryKind::Display, card.dump(-1, ' ', false, cluster::Json::error_handler_t::replace));
            } catch (const std::exception&) {}
        }
    }
    if (this->_data.entries.size() > 2000) this->_data.entries.erase(this->_data.entries.begin(), this->_data.entries.begin() + 500);
    if (this->_log) {
        this->_log << cluster::Json{{"time", entry.time}, {"kind", static_cast<int>(kind)}, {"text", text}, {"tool", tool}}.dump() << "\n";
        this->_log.flush();
    }
}

_cold void cluster::Session::emit_(const cluster::Event::Kind kind, const std::string& text, const bool error, const bool interrupted)
{
    pending_events_.push_back({kind, this->_data.spec.id, text, error, interrupted});
}

/* actions */
_cold void cluster::Session::write_(const cluster::Json& message)
{
    if (this->_in == -1) throw utils::exception::ErrorException(utils::exception::InternalCode::NotRunning, "session " + this->_data.spec.id + " is not running");
    const std::string line = message.dump() + "\n";
    std::size_t written = 0;
    while (written < line.size()) {
        const ssize_t size = ::write(this->_in, line.data() + written, line.size() - written);
        if (size <= 0) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, "stdin of session " + this->_data.spec.id);
        written += static_cast<std::size_t>(size);
    }
}

_cold void cluster::Session::send(const std::string& text, const std::vector<std::string>& images)
{
    const cluster::Queued prompt{text, images};
    if (this->persistent()) {
        if (!this->_worker.joinable() || this->_data.state == cluster::State::Stopped || this->_data.state == cluster::State::Error) {
            if (this->_worker.joinable()) this->_worker.join();
            this->start();
            for (int i = 0; i < 100 && this->_in == -1; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::lock_guard<std::mutex> lock(this->_mutex);
        // a turn runs: kept here (editable) until its end, then written
        if (this->_data.state == cluster::State::Working || this->_data.state == cluster::State::Waiting) {
            this->_pending.push_back(prompt);
            this->queue_();
        } else {
            this->dispatch_(prompt);
        }
    } else {
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->_pending.push_back(prompt);
            this->queue_();
            if (this->_data.state != cluster::State::Working) this->_data.state = cluster::State::Working;
        }
        if (!this->_busy) {
            if (this->_worker.joinable()) this->_worker.join();
            this->_busy = true;
            this->_stopping = false;
            this->_worker = std::thread(&cluster::Session::runTurns_, this);
        }
    }
    this->touch_();
}

_cold static std::string uuid_(void)
{
    // a random uuid (version 4): the id of a prompt written here, carried by its replay
    static std::mt19937_64 random{std::random_device{}()};
    const std::uint64_t high = random();
    const std::uint64_t low = random();
    char out[37] = {};
    std::snprintf(out, sizeof(out), "%08x-%04x-4%03x-%04x-%012llx", static_cast<unsigned>(high >> 32), static_cast<unsigned>((high >> 16) & 0xFFFF),
        static_cast<unsigned>(high & 0x0FFF), static_cast<unsigned>(0x8000 | ((low >> 48) & 0x3FFF)), static_cast<unsigned long long>(low & 0xFFFFFFFFFFFFULL));
    return out;
}

_cold void cluster::Session::dispatch_(const cluster::Queued& prompt)
{
    const Prepared prepared = prepare_(prompt, true);
    this->_lastPrompt = prompt;
    this->_data.spec.resumeAt.clear(); // the conversation goes on from the cut: its new messages are the latest
    this->entry_(cluster::EntryKind::User, prepared.shown);
    const std::string uuid = uuid_();
    this->_sent.insert(uuid);
    this->write_({{"type", "user"}, {"uuid", uuid}, {"message", {{"role", "user"}, {"content", prepared.content.empty() ? cluster::Json(prompt.text) : prepared.content}}}});
    this->_data.state = cluster::State::Working;
    this->_turn = {};
    this->turnStart_();
}

_cold void cluster::Session::queue_(void)
{
    this->_data.queue.clear();
    for (const cluster::Queued& queued: this->_pending)
        this->_data.queue.push_back(queued.text);
    this->_data.queued = static_cast<int>(this->_pending.size());
}

_cold std::optional<cluster::Queued> cluster::Session::unqueue(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_pending.empty()) return std::nullopt;
    const cluster::Queued last = this->_pending.back();
    this->_pending.pop_back();
    this->queue_();
    this->touch_();
    return last;
}

_cold void cluster::Session::answer(const std::string& requestId, const std::string& behavior)
{
    std::unique_lock<std::mutex> lock(this->_mutex);
    auto it = std::find_if(this->_data.permissions.begin(), this->_data.permissions.end(),
        [&](const cluster::Permission& p) {return requestId.empty() || p.requestId == requestId;});
    if (it == this->_data.permissions.end())
        throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no pending permission '" + requestId + "' in " + this->_data.spec.id);
    if (it->safety) {
        // the command the safety check stopped: allowed, the agent runs it again, out of a mode that never asks (its request
        // then allowed here at once, the mode put back after the turn); denied, it stays not run
        const cluster::Permission stopped = *it;
        this->_data.permissions.erase(it);
        if (behavior == "deny") {
            this->entry_(cluster::EntryKind::System, "not run (the safety check stopped it, the user denied it): " + cluster::one_line(stopped.description, 160));
            this->touch_();
            return;
        }
        this->_approved = stopped.input.dump();
        const std::string mode = this->_data.spec.mode;
        if (mode == "auto" || mode == "dontAsk") {
            this->_modeBack = mode;
            this->write_({{"type", "control_request"}, {"request_id", "mode-ask-" + std::to_string(cluster::now())},
                {"request", {{"subtype", "set_permission_mode"}, {"mode", "default"}}}});
        }
        this->_restoreAfter = this->_data.metrics.turns + (this->_data.state == cluster::State::Idle ? 1 : 2);
        this->entry_(cluster::EntryKind::System, "allowed by the user after the safety check: " + cluster::one_line(stopped.description, 160));
        this->touch_();
        lock.unlock();
        const std::string command = stopped.input.contains("command") ? stopped.input.value("command", std::string()) : stopped.input.dump();
        this->send("I approve, myself, the command the safety check stopped (I am the person it asked for). Run it again now, exactly unchanged: "
            "it goes through a permission prompt that I accept.\n" + command);
        return;
    }
    if (it->tool == "AskUserQuestion" && behavior != "deny") // allowed without its answers, the agent goes on blind
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction,
            "a question of the agent: answer it in its popup (or session_allow with answers {question: answer})");

    cluster::Json response = {{"behavior", behavior == "deny" ? "deny" : "allow"}};
    if (behavior == "deny") response["message"] = "Denied by the user (claude-cluster)";
    else response["updatedInput"] = it->input;
    if (behavior == "always" && it->suggestions.is_array()) {
        cluster::Json rules = cluster::Json::array();
        for (const cluster::Json& suggestion: it->suggestions)
            if (suggestion.value("type", std::string()) == "addRules" || suggestion.value("type", std::string()) == "addDirectories") rules.push_back(suggestion);
        if (!rules.empty()) response["updatedPermissions"] = rules;
    }
    this->write_({{"type", "control_response"}, {"response", {{"subtype", "success"}, {"request_id", it->requestId}, {"response", response}}}});
    this->entry_(cluster::EntryKind::System, std::string(behavior == "deny" ? "denied " : behavior == "always" ? "always allowed " : "allowed ") + it->tool);
    this->_data.permissions.erase(it);
    if (this->_data.permissions.empty() && this->_data.state == cluster::State::Waiting) this->_data.state = cluster::State::Working;
    this->touch_();
}

_cold void cluster::Session::answerQuestions(const std::string& requestId, const cluster::Json& answers)
{
    // AskUserQuestion: allowed with its input and the answers {question: label, or labels joined by ", "}
    std::lock_guard<std::mutex> lock(this->_mutex);
    auto it = std::find_if(this->_data.permissions.begin(), this->_data.permissions.end(),
        [&](const cluster::Permission& p) {return p.tool == "AskUserQuestion" && (requestId.empty() || p.requestId == requestId);});
    if (it == this->_data.permissions.end())
        throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no pending question '" + requestId + "' in " + this->_data.spec.id);
    if (!answers.is_object()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "answers: {question: answer}");
    cluster::Json input = it->input;
    input["answers"] = answers;
    this->write_({{"type", "control_response"}, {"response", {{"subtype", "success"}, {"request_id", it->requestId},
        {"response", {{"behavior", "allow"}, {"updatedInput", input}}}}}});
    std::string said;
    for (const auto &[question, answer]: answers.items())
        said += (said.empty() ? "" : " · ") + question + " → " + (answer.is_string() ? answer.get<std::string>() : answer.dump());
    this->entry_(cluster::EntryKind::System, "answered: " + said);
    this->_data.permissions.erase(it);
    if (this->_data.permissions.empty() && this->_data.state == cluster::State::Waiting) this->_data.state = cluster::State::Working;
    this->touch_();
}

_cold void cluster::Session::setMode(const std::string& mode)
{
    const std::vector<std::pair<std::string, std::string>>& modes = cluster::permission_modes();
    if (std::none_of(modes.begin(), modes.end(), [&](const std::pair<std::string, std::string>& m) {return m.first == mode;}))
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "'" + mode + "': auto | manual | acceptEdits | plan | dontAsk | bypassPermissions");
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->persistent() && this->_in != -1) {
        this->write_({{"type", "control_request"}, {"request_id", "mode-" + std::to_string(cluster::now())}, {"request", {{"subtype", "set_permission_mode"}, {"mode", mode}}}});
    } else {
        this->_data.spec.mode = mode; // next start / next turn
        this->entry_(cluster::EntryKind::System, "permission mode: " + mode);
    }
    this->touch_();
}

_cold void cluster::Session::interrupt(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->persistent()) {
        if (this->_in != -1)
            this->write_({{"type", "control_request"}, {"request_id", "int-" + std::to_string(cluster::now())}, {"request", {{"subtype", "interrupt"}}}});
    } else {
        this->_pending.clear();
        if (this->_process && this->_process->is()) ::kill(this->_process->getPid(), SIGINT);
    }
    if (this->_data.state == cluster::State::Working || this->_data.state == cluster::State::Waiting) this->_interrupted = true;
    this->entry_(cluster::EntryKind::System, "interrupted");
    this->touch_();
}

_cold cluster::Queued cluster::Session::rewind(void)
{
    // like Esc Esc in Claude Code: the last turn leaves the conversation (claude goes on from the message before its
    // prompt: --resume-session-at, kept until the next prompt) and the transcript; its prompt comes back to be edited
    std::string claudeId;
    std::string leaf;
    cluster::Queued prompt;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_launch.backend.provider.driver != "claude" || !this->persistent())
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "rewind: only for the sessions of Claude Code");
        auto user = std::find_if(this->_data.entries.rbegin(), this->_data.entries.rend(), [](const cluster::Entry& e) {return e.kind == cluster::EntryKind::User;});
        if (user == this->_data.entries.rend() || this->_data.spec.claudeId.empty())
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "rewind: no prompt to take back");
        claudeId = this->_data.spec.claudeId;
        leaf = this->_data.spec.leaf;
        prompt = this->_lastPrompt.text.empty() ? cluster::Queued{user->text, {}} : this->_lastPrompt;
    }
    const std::optional<std::string> point = rewind_point_(claudeId, leaf);
    if (!point) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "rewind: the last prompt is not in the transcript of claude (compacted?)");
    this->stop();
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        auto user = std::find_if(this->_data.entries.rbegin(), this->_data.entries.rend(), [](const cluster::Entry& e) {return e.kind == cluster::EntryKind::User;});
        if (user != this->_data.entries.rend()) this->_data.entries.erase(std::next(user).base(), this->_data.entries.end());
        this->_data.partial.clear();
        this->_data.thinking.clear();
        this->_data.permissions.clear();
        // the log of the session: from its last prompt on, removed
        this->_log.close();
        std::vector<std::string> lines;
        {
            std::ifstream in(this->_launch.log);
            for (std::string line; std::getline(in, line);)
                lines.push_back(line);
        }
        std::size_t keep = lines.size();
        for (std::size_t i = lines.size(); i-- > 0;) {
            const cluster::Json json = cluster::Json::parse(lines[i], nullptr, false);
            if (!json.is_discarded() && json.value("kind", -1) == static_cast<int>(cluster::EntryKind::User)) {
                keep = i;
                break;
            }
        }
        {
            std::ofstream out(this->_launch.log, std::ios::trunc);
            for (std::size_t i = 0; i < keep; ++i)
                out << lines[i] << "\n";
        }
        this->_log.open(this->_launch.log, std::ios::app);
        this->_data.spec.leaf = *point;
        this->_data.spec.resumeAt = *point;
        if (point->empty()) this->_data.spec.claudeId.clear(); // its first prompt: a new conversation
        this->_lastPrompt = {};
    }
    this->start();
    return prompt;
}

_cold void cluster::Session::stop(void)
{
    this->_stopping = true;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_pending.clear();
        if (this->_in != -1) {
            ::close(this->_in);
            this->_in = -1;
        }
        if (this->_process && this->_process->is()) ::kill(this->_process->getPid(), SIGTERM);
    }
    if (this->_worker.joinable()) {
        // a stubborn agent is killed after 3 s
        for (int i = 0; i < 30 && this->_process && this->_process->is(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (this->_process && this->_process->is()) ::kill(this->_process->getPid(), SIGKILL);
        this->_worker.join();
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_data.state = cluster::State::Stopped;
    this->touch_();
}

_cold void cluster::Session::mark(const std::string& text)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->entry_(cluster::EntryKind::System, text);
    this->touch_();
}

_cold void cluster::Session::setClaudeId(const std::string& id)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_data.spec.claudeId = id;
    this->_data.spec.leaf.clear();
    this->_data.spec.resumeAt.clear();
    this->touch_();
}

_cold void cluster::Session::remoteControl(const bool on, const std::string& name)
{
    // the session itself on claude.ai (control_request remote_control of the SDK): the prompts of both sides go to this
    // claude, its events come here (live); kept over the restarts of the process (Launch::remoteName)
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_launch.backend.provider.driver != "claude" || !this->persistent())
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "Remote Control: only for the sessions of Claude Code");
    this->_launch.remoteName = on ? name : "";
    if (this->_in != -1) {
        cluster::Json request = {{"subtype", "remote_control"}, {"enabled", on}};
        if (on) request["name"] = name;
        this->write_({{"type", "control_request"}, {"request_id", std::string(on ? "rc-on-" : "rc-off-") + std::to_string(cluster::now())}, {"request", request}});
    }
    if (!on) {
        this->_data.remoteUrl.clear();
        this->_data.bridgeState.clear();
    }
    this->entry_(cluster::EntryKind::System, on ? "Remote Control: connecting…" : "Remote Control off");
    this->touch_();
}

_cold void cluster::Session::rename(const std::string& name)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_data.spec.name = name;
    this->touch_();
}

_cold void cluster::Session::describe(const std::string& text, const bool given)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!given && !this->_data.spec.summaryAuto) return;
    this->_data.spec.summary = text;
    this->_data.spec.summaryAuto = !given || text.empty();
    this->_data.spec.summaryTurns = this->_data.spec.turns;
    this->touch_();
}

_cold std::int64_t cluster::Session::countTurn(void)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return ++this->_data.spec.turns;
}

_cold void cluster::Session::togglePanel(const std::string& panel)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (!this->_data.spec.panels.erase(panel)) this->_data.spec.panels.insert(panel);
    this->touch_();
}

_cold void cluster::Session::loadHistory(const std::size_t max)
{
    std::ifstream file(this->_launch.log);
    std::deque<cluster::Entry> entries;
    std::string line;

    while (std::getline(file, line)) {
        try {
            const cluster::Json json = cluster::Json::parse(line);
            entries.push_back({static_cast<cluster::EntryKind>(json.value("kind", 0)), json.value("text", std::string()), json.value("tool", std::string()), json.value("time", std::int64_t{0})});
            if (entries.size() > max) entries.pop_front();
        } catch (const cluster::Json::exception&) {}
    }
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_data.entries.assign(entries.begin(), entries.end());
    this->touch_();
}

cluster::Snapshot cluster::Session::snapshot(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_data;
}

cluster::SessionSpec cluster::Session::spec(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    cluster::SessionSpec spec = this->_data.spec;
    spec.cost = this->_data.metrics.cost;
    spec.tokens = this->_data.metrics.total.total();
    return spec;
}

std::tuple<cluster::State, std::size_t, bool> cluster::Session::brief(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    const bool safety = std::any_of(this->_data.permissions.begin(), this->_data.permissions.end(), [](const cluster::Permission& p) {return p.safety;});
    return {this->_data.state, this->_data.permissions.size(), safety};
}

cluster::State cluster::Session::state(void) const
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_data.state;
}

/* state name */
const char* cluster::state_name(cluster::State state)
{
    switch (state) {
        case cluster::State::Starting: return "starting";
        case cluster::State::Idle: return "idle";
        case cluster::State::Working: return "working";
        case cluster::State::Waiting: return "permission";
        case cluster::State::Remote: return "remote";
        case cluster::State::Error: return "error";
        case cluster::State::Stopped: return "stopped";
    }
    return "?";
}
