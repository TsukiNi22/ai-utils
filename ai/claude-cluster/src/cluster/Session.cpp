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
#include <utils/utils.hpp>
#include "cluster/Session.hpp"
#include "cluster/Tools.hpp"
#include <sys/epoll.h>
#include <algorithm>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <array>

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
    // One line describing a tool call: the command, the file, the pattern...
    if (!input.is_object()) return "";
    for (const char* key: {"command", "file_path", "filePath", "notebook_path", "path", "pattern", "url", "query", "skill", "description", "prompt"})
        if (input.contains(key) && input[key].is_string()) return cluster::one_line(input[key].get<std::string>(), 120);
    return name == "TodoWrite" ? "todo list" : cluster::one_line(input.dump(), 120);
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
    std::vector<std::string> args = {this->_launch.command};

    if (driver == "claude") {
        args.insert(args.end(), {"-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose",
            "--include-partial-messages", "--permission-prompt-tool", "stdio", "--permission-mode", mode});
        if (!model.empty()) args.insert(args.end(), {"--model", model});
        if (!spec.claudeId.empty()) args.insert(args.end(), {"--resume", spec.claudeId});
    } else if (driver == "qwen") {
        const std::string approval = mode == "plan" ? "plan" : mode == "acceptEdits" ? "auto-edit" : mode == "bypassPermissions" ? "yolo" : "default";
        args.insert(args.end(), {"-o", "stream-json", "--approval-mode", approval, "--auth-type", "openai"});
        if (!model.empty()) args.insert(args.end(), {"-m", model});
        if (!spec.claudeId.empty()) args.insert(args.end(), {"-r", spec.claudeId});
        args.insert(args.end(), {"-p", prompt});
    } else if (driver == "opencode") {
        args.insert(args.end(), {"run", "--format", "json"});
        if (!model.empty()) args.insert(args.end(), {"-m", model});
        if (!spec.claudeId.empty()) args.insert(args.end(), {"-s", spec.claudeId});
        if (mode == "bypassPermissions") args.push_back("--auto");
        args.insert(args.end(), {"--", prompt});
    } else if (driver == "codex") {
        args.insert(args.end(), {"exec", "--json", "--skip-git-repo-check"});
        if (mode == "bypassPermissions") args.push_back("--dangerously-bypass-approvals-and-sandbox");
        else args.insert(args.end(), {"--sandbox", mode == "acceptEdits" ? "workspace-write" : "read-only"});
        if (!model.empty()) args.insert(args.end(), {"-m", model});
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
    (void)this->_process->spawn("env", envArgs);

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
    }
    this->_worker = std::thread(&cluster::Session::read_, this);
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
            prompt = this->_pending.front();
            this->_pending.pop_front();
            this->_data.queued = static_cast<int>(this->_pending.size());
            this->_data.state = cluster::State::Working;
            this->_turn = {};
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
            const bool failed = code != 0 && !this->_stopping;
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
            events.push_back({cluster::Event::Kind::TurnDone, this->_data.spec.id, failed ? this->_data.lastError : this->_lastText, failed});
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
        if (driver == "opencode") this->opencode_(event);
        else if (driver == "codex") this->codex_(event);
        else this->claude_(event);
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
        this->_data.modelUsed = event.value("model", std::string());
        if (event.contains("permissionMode")) this->_data.spec.mode = event.value("permissionMode", this->_data.spec.mode);
        this->_data.skillsAvailable.clear();
        if (event.contains("skills") && event["skills"].is_array())
            for (const cluster::Json& skill: event["skills"])
                if (skill.is_string()) this->_data.skillsAvailable.push_back(skill.get<std::string>());
        if (this->_data.state == cluster::State::Starting) this->_data.state = cluster::State::Idle;
        this->emit_(cluster::Event::Kind::Init);
    } else if (type == "system" && subtype == "compact_boundary") {
        this->entry_(cluster::EntryKind::System, "context compacted");
    } else if (type == "stream_event" && event.contains("event")) {
        const cluster::Json& e = event["event"];
        const std::string kind = e.value("type", std::string());
        if (kind == "message_start" && e.contains("message")) {
            this->_live = usage_of_(e["message"].value("usage", cluster::Json::object()));
            metrics.contextUsed = this->_live.context();
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
        } else if (kind == "content_block_delta" && e.contains("delta") && e["delta"].value("type", std::string()) == "text_delta") {
            this->_data.partial += e["delta"].value("text", std::string());
        }
        metrics.current = this->_turn;
        add_(metrics.current, this->_live);
    } else if (type == "assistant" && event.contains("message")) {
        if (!event.value("parent_tool_use_id", cluster::Json()).is_null()) return; // inside a sub-agent
        for (const cluster::Json& block: event["message"].value("content", cluster::Json::array())) {
            const std::string kind = block.value("type", std::string());
            if (kind == "text") {
                this->_lastText = block.value("text", std::string());
                this->entry_(cluster::EntryKind::Assistant, this->_lastText);
                this->_data.partial.clear();
            } else if (kind == "thinking") {
                this->entry_(cluster::EntryKind::Thinking, block.value("thinking", std::string()));
            } else if (kind == "tool_use") {
                this->tool_(block.value("id", std::string()), block.value("name", std::string()), block.value("input", cluster::Json::object()));
            }
        }
        if (this->persistent() && this->_data.state != cluster::State::Waiting) this->_data.state = cluster::State::Working;
        // per-turn drivers (qwen) don't stream: the usage of the message is the one of the turn so far
        if (!this->persistent()) {
            add_(this->_turn, usage_of_(event["message"].value("usage", cluster::Json::object())));
            metrics.current = this->_turn;
        }
    } else if (type == "user" && event.contains("message")) {
        const cluster::Json content = event["message"].value("content", cluster::Json());
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
            std::string text;
            if (block.contains("content") && block["content"].is_string()) text = block["content"].get<std::string>();
            else if (block.contains("content") && block["content"].is_array())
                for (const cluster::Json& part: block["content"])
                    text += part.value("text", std::string());
            this->entry_(error ? cluster::EntryKind::Error : cluster::EntryKind::ToolResult, cluster::one_line(text, 300));
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
        permission.time = cluster::now();
        this->_data.permissions.push_back(permission);
        this->_data.state = cluster::State::Waiting;
        this->emit_(cluster::Event::Kind::Permission, permission.tool + ": " + cluster::one_line(permission.description, 120));
    } else if (type == "rate_limit_event" && event.contains("rate_limit_info")) {
        const cluster::Json windows = event["rate_limit_info"].value("unifiedWindows", cluster::Json::object());
        if (windows.contains("five_hour")) metrics.limit5h = windows["five_hour"].value("utilization", -1.0);
        if (windows.contains("seven_day")) metrics.limit7d = windows["seven_day"].value("utilization", -1.0);
    } else if (type == "result") {
        const cluster::Usage usage = usage_of_(event.value("usage", cluster::Json::object()));
        metrics.last = usage.total() > 0 ? usage : this->_turn;
        if (this->persistent()) add_(metrics.total, metrics.last);
        else this->_turn = metrics.last; // added to the total at the end of the process
        metrics.current = {};
        this->_turn = this->persistent() ? cluster::Usage{} : this->_turn;
        this->_live = {};
        metrics.turns += this->persistent() ? 1 : 0;
        // total_cost_usd is cumulative over the life of the process (a restart begins again at 0)
        const double cost = event.value("total_cost_usd", 0.0);
        metrics.lastCost = std::max(0.0, cost - this->_processCost);
        this->_processCost = cost;
        metrics.cost = this->_costBase + cost;
        if (event.contains("modelUsage") && event["modelUsage"].is_object())
            for (const auto &[model, data]: event["modelUsage"].items())
                metrics.contextWindow = std::max<std::int64_t>(data.value("contextWindow", std::int64_t{0}), 1000);
        const bool error = event.value("is_error", false);
        const std::string text = event.contains("result") && event["result"].is_string() ? event["result"].get<std::string>() : this->_lastText;
        if (error) this->entry_(cluster::EntryKind::Error, text.empty() ? subtype : text);
        this->_data.partial.clear();
        if (this->persistent()) {
            this->_data.state = this->_data.permissions.empty() ? cluster::State::Idle : cluster::State::Waiting;
            this->_data.queued = std::max(0, this->_data.queued - 1);
            this->emit_(cluster::Event::Kind::TurnDone, text, error);
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
        this->entry_(error ? cluster::EntryKind::Error : cluster::EntryKind::ToolResult, cluster::one_line(output.is_string() ? output.get<std::string>() : output.dump(), 300));
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
        this->_data.metrics.lastCost = cost;
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
            this->entry_(error ? cluster::EntryKind::Error : cluster::EntryKind::ToolResult, cluster::one_line(item.value("aggregated_output", std::string()), 300));
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
    this->entry_(cluster::EntryKind::Tool, summary, name);

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

_cold void cluster::Session::entry_(const cluster::EntryKind kind, const std::string& text, const std::string& tool)
{
    if (text.empty() && kind != cluster::EntryKind::Tool) return;
    cluster::Entry entry{kind, text, tool, cluster::now()};
    this->_data.entries.push_back(entry);
    if (this->_data.entries.size() > 2000) this->_data.entries.erase(this->_data.entries.begin(), this->_data.entries.begin() + 500);
    if (this->_log) {
        this->_log << cluster::Json{{"time", entry.time}, {"kind", static_cast<int>(kind)}, {"text", text}, {"tool", tool}}.dump() << "\n";
        this->_log.flush();
    }
}

_cold void cluster::Session::emit_(const cluster::Event::Kind kind, const std::string& text, const bool error)
{
    pending_events_.push_back({kind, this->_data.spec.id, text, error});
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

_cold void cluster::Session::send(const std::string& text)
{
    if (this->persistent()) {
        if (!this->_worker.joinable() || this->_data.state == cluster::State::Stopped || this->_data.state == cluster::State::Error) {
            if (this->_worker.joinable()) this->_worker.join();
            this->start();
            for (int i = 0; i < 100 && this->_in == -1; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->entry_(cluster::EntryKind::User, text);
        this->write_({{"type", "user"}, {"message", {{"role", "user"}, {"content", text}}}});
        if (this->_data.state == cluster::State::Working) this->_data.queued++;
        else this->_data.state = cluster::State::Working;
        this->_turn = {};
    } else {
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            this->entry_(cluster::EntryKind::User, text);
            this->_pending.push_back(text);
            this->_data.queued = static_cast<int>(this->_pending.size()) - (this->_busy ? 0 : 1);
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

_cold void cluster::Session::answer(const std::string& requestId, const std::string& behavior)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    auto it = std::find_if(this->_data.permissions.begin(), this->_data.permissions.end(),
        [&](const cluster::Permission& p) {return requestId.empty() || p.requestId == requestId;});
    if (it == this->_data.permissions.end())
        throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no pending permission '" + requestId + "' in " + this->_data.spec.id);

    cluster::Json response = {{"behavior", behavior == "deny" ? "deny" : "allow"}};
    if (behavior == "deny") response["message"] = "Denied by the user (claude-cluster)";
    else response["updatedInput"] = it->input;
    if (behavior == "always" && it->suggestions.is_array() && !it->suggestions.empty()) response["updatedPermissions"] = it->suggestions;
    this->write_({{"type", "control_response"}, {"response", {{"subtype", "success"}, {"request_id", it->requestId}, {"response", response}}}});
    this->entry_(cluster::EntryKind::System, std::string(behavior == "deny" ? "denied " : behavior == "always" ? "always allowed " : "allowed ") + it->tool);
    this->_data.permissions.erase(it);
    if (this->_data.permissions.empty() && this->_data.state == cluster::State::Waiting) this->_data.state = cluster::State::Working;
    this->touch_();
}

_cold void cluster::Session::setMode(const std::string& mode)
{
    static const std::array<const char*, 4> modes = {"default", "acceptEdits", "plan", "bypassPermissions"};
    if (std::find_if(modes.begin(), modes.end(), [&](const char* m) {return mode == m;}) == modes.end())
        throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "'" + mode + "': default | acceptEdits | plan | bypassPermissions");
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_data.spec.mode = mode;
    if (this->persistent() && this->_in != -1)
        this->write_({{"type", "control_request"}, {"request_id", "mode-" + std::to_string(cluster::now())}, {"request", {{"subtype", "set_permission_mode"}, {"mode", mode}}}});
    this->entry_(cluster::EntryKind::System, "permission mode: " + mode);
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
    this->entry_(cluster::EntryKind::System, "interrupted");
    this->touch_();
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

_cold void cluster::Session::rename(const std::string& name)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_data.spec.name = name;
    this->touch_();
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
