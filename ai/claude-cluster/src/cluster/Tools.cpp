/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Tools.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _Encapsulation
#include <utils/utils.hpp>
#include "cluster/Tools.hpp"
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <algorithm>
#include <sys/un.h>
#include <unistd.h>
#include <csignal>
#include <fcntl.h>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <array>
#include <ctime>
#include <set>

/* paths */
_cold std::filesystem::path cluster::expand_home(const std::string& path)
{
    const char* home = std::getenv("HOME");
    if (home && (path == "~" || path.starts_with("~/"))) return std::filesystem::path(home) / path.substr(path.size() > 1 ? 2 : 1);
    return path;
}

_cold std::filesystem::path cluster::config_dir(void)
{
    const char* own = std::getenv("CLAUDE_CLUSTER_CONFIG_DIR");
    if (own && *own) return own;
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    return (xdg && *xdg ? std::filesystem::path(xdg) : cluster::expand_home("~/.config")) / "claude-cluster";
}

_cold std::filesystem::path cluster::data_dir(void)
{
    const char* own = std::getenv("CLAUDE_CLUSTER_DATA_DIR");
    if (own && *own) return own;
    const char* xdg = std::getenv("XDG_DATA_HOME");
    return (xdg && *xdg ? std::filesystem::path(xdg) : cluster::expand_home("~/.local/share")) / "claude-cluster";
}

_cold std::filesystem::path cluster::runtime_dir(void)
{
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && *xdg) return std::filesystem::path(xdg) / "claude-cluster";
    return std::filesystem::temp_directory_path() / ("claude-cluster-" + std::to_string(::getuid()));
}

_cold std::filesystem::path cluster::socket_path(void)
{
    const char* env = std::getenv("CLAUDE_CLUSTER_SOCKET");
    if (env && *env) return env;
    return cluster::runtime_dir() / "control.sock";
}

_cold std::filesystem::path cluster::self_exe(void)
{
    std::error_code error;
    std::string exe = std::filesystem::read_symlink("/proc/self/exe", error).string();
    // replaced while running (reinstalled): "<path> (deleted)", the new binary is at <path>
    if (exe.ends_with(" (deleted)")) exe.erase(exe.size() - 10);
    return error ? std::filesystem::path("claude-cluster") : std::filesystem::path(exe);
}

_cold std::string cluster::short_path(const std::string& path)
{
    const char* home = std::getenv("HOME");
    if (home && *home && path.starts_with(home)) return "~" + path.substr(std::string(home).size());
    return path;
}

/* text */
std::vector<std::string> cluster::split(const std::string& text, const char sep)
{
    std::vector<std::string> parts;
    std::string part;
    std::istringstream stream(text);

    while (std::getline(stream, part, sep))
        if (!cluster::trim(part).empty()) parts.push_back(cluster::trim(part));
    return parts;
}

std::string cluster::trim(const std::string& text)
{
    const std::size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
}

std::string cluster::lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {return std::tolower(c);});
    return text;
}

std::string cluster::one_line(const std::string& text, const std::size_t max)
{
    std::string line;
    for (const char c: text) {
        if (c == '\n' || c == '\r' || c == '\t') {
            if (!line.empty() && line.back() != ' ') line += ' ';
        } else {
            line += c;
        }
    }
    line = cluster::trim(line);
    if (line.size() <= max) return line;
    std::size_t cut = max > 3 ? max - 3 : max;
    while (cut > 0 && (static_cast<unsigned char>(line[cut]) & 0xC0) == 0x80) --cut; // never cut an UTF-8 character
    return line.substr(0, cut) + "...";
}

std::string cluster::language_of(const std::string& text)
{
    // Count of frequent words + accents: enough to pick the voice of an answer; French unless another one is clear (its
    // own words only: "de", "la", "que"... shared by the Romance languages are not counted for the others)
    struct Language {
        const char* code;
        std::vector<std::string> words;
        std::vector<std::string> marks;
    };
    static const std::vector<Language> languages = {
        {"fr", {"le", "la", "les", "des", "est", "une", "un", "et", "pour", "pas", "que", "qui", "dans", "sur", "avec", "je", "tu", "vous",
            "nous", "ce", "c'est", "du", "au", "mais"}, {"é", "è", "à", "ç", "ê", "ù"}},
        {"en", {"the", "is", "are", "and", "for", "not", "that", "which", "in", "on", "with", "i", "you", "we", "this", "it's", "of", "to",
            "a", "an", "but", "was", "be", "has"}, {}},
        {"es", {"el", "los", "las", "es", "está", "pero", "por", "para", "y", "muy", "también", "como", "qué", "hay", "son", "del"}, {"ñ", "¿", "¡"}},
        {"it", {"il", "gli", "è", "che", "non", "sono", "della", "per", "anche", "questo", "perché", "ho", "hai", "molto", "sei"}, {"ò", "ì"}},
        {"de", {"der", "die", "das", "und", "ist", "nicht", "ich", "sie", "mit", "auf", "ein", "eine", "auch", "es", "wir", "zu"}, {"ß", "ü", "ö", "ä"}},
        {"pt", {"não", "você", "os", "uma", "com", "também", "muito", "são", "isso", "está", "mas", "em"}, {"ã", "õ", "ção"}},
        {"nl", {"het", "een", "niet", "van", "zijn", "dat", "ik", "je", "wel", "ook", "naar", "maar", "wij"}, {"ij"}},
    };
    std::vector<int> scores(languages.size(), 0);

    for (const std::string& word: cluster::split(cluster::lower(text), ' ')) {
        std::string w = word;
        w.erase(std::remove_if(w.begin(), w.end(), [](char c) {return c == ',' || c == '.' || c == '!' || c == '?' || c == ':';}), w.end());
        for (std::size_t i = 0; i < languages.size(); ++i)
            scores[i] += std::find(languages[i].words.begin(), languages[i].words.end(), w) != languages[i].words.end();
    }
    for (std::size_t i = 0; i < languages.size(); ++i)
        for (const std::string& mark: languages[i].marks)
            if (text.find(mark) != std::string::npos) scores[i] += 2;
    // English against French as before; another language: clearly ahead of both (3 hits at least)
    std::size_t best = scores[1] > scores[0] ? 1 : 0;
    for (std::size_t i = 2; i < languages.size(); ++i)
        if (scores[i] >= 3 && scores[i] > scores[best] && scores[i] > scores[0] && scores[i] > scores[1]) best = i;
    return languages[best].code;
}

std::string cluster::summary_of(const std::string& text, const std::size_t max)
{
    // Spoken summary: no code blocks, no markdown marks, the first sentences up to max characters
    std::string clean;
    bool code = false;

    for (const std::string& line: cluster::split(text, '\n')) {
        if (line.starts_with("```")) {
            code = !code;
            continue;
        }
        if (code || line.starts_with("|")) continue;
        std::string l = line;
        l.erase(std::remove_if(l.begin(), l.end(), [](char c) {return c == '*' || c == '`' || c == '#' || c == '>' || c == '_';}), l.end());
        clean += cluster::trim(l) + " ";
    }
    clean = cluster::trim(clean);
    if (clean.size() <= max) return clean;
    std::size_t cut = clean.rfind(". ", max);
    if (cut == std::string::npos || cut < max / 3) return cluster::one_line(clean, max);
    return clean.substr(0, cut + 1);
}

std::string cluster::clean_label(const std::string& text)
{
    // a label: lower case, letters / digits / - / _ / . only (a space becomes -), a leading # dropped
    std::string out;
    for (const char c: cluster::lower(cluster::trim(text))) {
        if (std::isalnum(static_cast<unsigned char>(c)) || (static_cast<unsigned char>(c) >= 0x80) || c == '-' || c == '_' || c == '.') out += c;
        else if ((c == ' ' || c == '/') && !out.empty() && out.back() != '-') out += '-';
    }
    while (!out.empty() && (out.back() == '-' || out.back() == '.')) out.pop_back();
    while (!out.empty() && out.front() == '-') out.erase(0, 1);
    return out.substr(0, 24);
}

std::vector<std::string> cluster::clean_tags(const std::vector<std::string>& tags)
{
    std::vector<std::string> out;
    for (const std::string& tag: tags) {
        const std::string clean = cluster::clean_label(tag);
        if (!clean.empty() && std::find(out.begin(), out.end(), clean) == out.end() && out.size() < 8) out.push_back(clean);
    }
    return out;
}

std::vector<std::string> cluster::split_tags(const std::string& text)
{
    std::string spaced = text;
    std::replace(spaced.begin(), spaced.end(), ',', ' ');
    std::vector<std::string> words;
    for (const std::string& word: cluster::split(spaced, ' '))
        if (!cluster::trim(word).empty()) words.push_back(word);
    return cluster::clean_tags(words);
}

std::string cluster::content_hash(const std::string& bytes)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char c: bytes) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

/* format */
std::string cluster::human_tokens(const std::int64_t tokens)
{
    std::ostringstream out;
    if (tokens < 1000) return std::to_string(tokens);
    if (tokens < 1000000) out << std::fixed << std::setprecision(tokens < 10000 ? 1 : 0) << tokens / 1000.0 << "k";
    else out << std::fixed << std::setprecision(1) << tokens / 1000000.0 << "M";
    return out.str();
}

std::string cluster::human_cost(const double usd)
{
    std::ostringstream out;
    out << "$" << std::fixed << std::setprecision(usd < 1.0 ? 3 : 2) << usd;
    return out.str();
}

std::string cluster::human_age(const std::int64_t time)
{
    const std::int64_t age = std::max<std::int64_t>(0, cluster::now() - time);
    if (age < 60) return std::to_string(age) + "s";
    if (age < 3600) return std::to_string(age / 60) + "m";
    if (age < 86400) return std::to_string(age / 3600) + "h";
    return std::to_string(age / 86400) + "d";
}

std::string cluster::clock_time(const std::int64_t time)
{
    const std::time_t t = static_cast<std::time_t>(time);
    std::tm tm{};
    ::localtime_r(&t, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%H:%M");
    return out.str();
}

/* processes */
_cold bool cluster::has_command(const std::string& name)
{
    if (name.find('/') != std::string::npos) return ::access(cluster::expand_home(name).c_str(), X_OK) == 0;
    const char* path = std::getenv("PATH");
    if (!path) return false;
    for (const std::string& dir: cluster::split(path, ':'))
        if (::access((std::filesystem::path(dir) / name).c_str(), X_OK) == 0) return true;
    return false;
}

// the commands running (their pid): stopped at the quit, waited for (no process, pipe or poll left at the end)
static std::mutex captures_mutex_;
static std::set<pid_t> captures_;
static std::atomic<int> captures_running_{0};

_cold void cluster::end_captures(const int ms)
{
    {
        std::lock_guard<std::mutex> lock(captures_mutex_);
        for (const pid_t pid: captures_)
            cluster::stop_group(pid, false);
    }
    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (captures_running_ > 0 && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

_cold void cluster::stop_group(const int pid, const bool force)
{
    // the session of a command (setsid): the group leader is the command, its children are in the group
    if (pid <= 0) return;
    (void)::kill(-pid, force ? SIGKILL : SIGTERM);
    (void)::kill(pid, force ? SIGKILL : SIGTERM);
}

_cold cluster::Captured cluster::capture(const std::vector<std::string>& args, const std::string& cwd, const int timeout, std::atomic<int>* pidOut)
{
    cluster::Captured result;
    if (args.empty()) return result;
    ++captures_running_;
    struct Running {
        pid_t pid = 0;
        ~Running()
        {
            if (this->pid > 0) {
                std::lock_guard<std::mutex> lock(captures_mutex_);
                captures_.erase(this->pid);
            }
            --captures_running_;
        }
    } running;
    utils::encapsulation::Pipe out;
    utils::encapsulation::Process process;

    // stdout + stderr of the child into the pipe, stdin from /dev/null; env -C for the folder (only dup2 + exec in the child)
    out.trigger();
    for (const int fd: {out.getRead(), out.getWrite()})
        ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC);
    const int null = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    process.dup(out.getWrite(), STDOUT_FILENO);
    process.dup(out.getWrite(), STDERR_FILENO);
    if (null != -1) process.dup(null, STDIN_FILENO);
    std::vector<std::string> envArgs;
    if (!cwd.empty()) envArgs = {"-C", cwd};
    envArgs.insert(envArgs.end(), args.begin(), args.end());
    // setsid: no controlling terminal for the child (an interactive shell asked for its aliases would take the terminal
    // of the interface - tcsetpgrp - and its keyboard would be lost); the output is a pipe anyway
    static const bool setsid = cluster::has_command("setsid");
    if (setsid) {
        envArgs.insert(envArgs.begin(), "env");
        envArgs.insert(envArgs.begin(), "-w"); // waited for: its exit code kept whatever setsid does (a fork when it leads a group)
        running.pid = process.spawn("setsid", envArgs);
    } else {
        running.pid = process.spawn("env", envArgs);
    }
    {
        std::lock_guard<std::mutex> lock(captures_mutex_);
        captures_.insert(running.pid);
    }
    if (pidOut != nullptr) *pidOut = running.pid;
    out.closeWrite();
    if (null != -1) ::close(null);

    // Read until the end of the output or the timeout (the child is then killed)
    utils::encapsulation::Poll poll;
    poll.link(out.getRead(), EPOLLIN);
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout);
    std::array<char, 4096> buffer{};
    while (std::chrono::steady_clock::now() < deadline) {
        const int left = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count());
        if (poll.wait(std::max(left, 1)).empty()) break;
        const ssize_t size = ::read(out.getRead(), buffer.data(), buffer.size());
        if (size <= 0) break;
        result.out.append(buffer.data(), static_cast<std::size_t>(size));
    }
    if (process.is() && std::chrono::steady_clock::now() >= deadline) {
        cluster::stop_group(running.pid, true); // the command and what it started: nothing left spinning
        process.kill();
        result.code = -1;
        return result;
    }
    const utils::encapsulation::Status status = process.wait();
    result.code = status.exited ? status.code : -1;
    return result;
}

static bool claude_driver_(const std::string& provider)
{
    return provider == "claude" || provider.starts_with("anthropic");
}

static cluster::Json claude_settings_(void)
{
    static std::mutex lock;
    const std::lock_guard<std::mutex> guard(lock);
    static cluster::Json cached;
    static std::int64_t at = 0;
    if (cluster::now() - at > 20) {
        at = cluster::now();
        cached = cluster::read_json(cluster::expand_home("~/.claude/settings.json")).value_or(cluster::Json::object());
    }
    return cached;
}

_cold static std::string claude_default_model_read_(void)
{
    // the "model" of ~/.claude/settings.json; an alias (sonnet, opus...) resolved to the id of the last turn run on it (the usage ledger), else the alias
    const cluster::Json settings = claude_settings_();
    const std::string model = settings.is_object() ? settings.value("model", std::string()) : std::string();
    if (model.empty() || std::any_of(model.begin(), model.end(), [](const unsigned char c) {return std::isdigit(c) != 0;})) return model;
    std::ifstream ledger(cluster::data_dir() / "usage.jsonl");
    std::string line;
    std::string found;
    while (std::getline(ledger, line)) {
        const std::size_t at = line.find("\"model\":\"");
        if (at == std::string::npos) continue;
        const std::string id = line.substr(at + 9, line.find('"', at + 9) - at - 9);
        if (id.find(model) != std::string::npos) found = id;
    }
    if (found.empty() && settings.contains("modelSettings") && settings["modelSettings"].is_object()) // never run here: a model of its modelSettings that the alias names
        for (const auto &[name, value]: settings["modelSettings"].items())
            if (name.find(model) != std::string::npos && name > found) found = name;
    return found.empty() ? model : found;
}

_cold void cluster::keep_awake(const bool want)
{
    // an inhibitor of 120 s renewed every 100 s while wanted: nothing to stop (it ends alone, even when this process dies)
    static std::mutex lock;
    static std::int64_t last = 0;
    const std::lock_guard<std::mutex> guard(lock);
    if (!want || cluster::now() - last < 100) return;
    last = cluster::now();
    std::thread([]() {
        (void)std::system("systemd-inhibit --what=idle --who=claude-cluster --why='sessions working' --mode=block sleep 120 >/dev/null 2>&1");
    }).detach();
}

_cold std::string cluster::claude_default_model(void)
{
    // drawn at every frame by every interface: read at most once a minute (the ledger is scanned), from any thread
    static std::mutex lock;
    static std::string cached;
    static std::int64_t at = 0;
    const std::lock_guard<std::mutex> guard(lock);
    if (at == 0 || cluster::now() - at > 60) {
        cached = claude_default_model_read_();
        at = cluster::now();
    }
    return cached;
}

_cold std::string cluster::claude_default_effort(const std::string& model)
{
    const cluster::Json settings = claude_settings_();
    if (model.empty() || !settings.is_object() || !settings.contains("modelSettings") || !settings["modelSettings"].is_object()) return "";
    std::string best;
    for (const auto &[name, value]: settings["modelSettings"].items())
        if (value.is_object() && value.contains("effortLevel") && (model.find(name) == 0 || name.find(model) != std::string::npos) && name.size() >= best.size()) best = value.value("effortLevel", std::string());
    return best;
}

_cold std::string cluster::model_label(const std::string& backend, const std::string& modelUsed)
{
    // "claude/opus-5-5" when a model is chosen (in the backend, claude/opus) or loaded; else the one Claude Code starts on, marked (default)
    const std::size_t slash = backend.find('/');
    const std::string provider = backend.substr(0, slash);
    const bool chosen = slash != std::string::npos;
    std::string model = chosen ? backend.substr(slash + 1) : modelUsed;
    if (model.empty() && claude_driver_(provider)) model = claude_default_model();
    if (model.empty()) return provider + " (default)";
    return provider + "/" + (modelUsed.empty() ? model : modelUsed) + (chosen ? "" : " (default)");
}

_cold std::string cluster::effort_label(const std::string& effort, const std::string& configured, const std::string& backend, const std::string& modelUsed)
{
    if (!effort.empty() && effort != "default") return "think " + effort;
    // not chosen: the level of [agent] effort, else the one of the model in the settings of Claude Code - always shown, even before a first message
    if (!configured.empty()) return "think " + configured + " (default)";
    const std::string provider = backend.substr(0, backend.find('/'));
    const std::string model = !modelUsed.empty() ? modelUsed : backend.find('/') != std::string::npos ? backend.substr(backend.find('/') + 1) : claude_driver_(provider) ? claude_default_model() : std::string();
    const std::string level = claude_driver_(provider) ? claude_default_effort(model) : std::string();
    return "think " + (level.empty() ? std::string("default") : level + " (default)");
}

std::string cluster::tail_utf8(const std::string& text, const std::size_t max)
{
    // the end of a text (the live speech: what is being said), cut on a character
    if (text.size() <= max) return text;
    std::size_t from = text.size() - max;
    while (from < text.size() && (static_cast<unsigned char>(text[from]) & 0xC0) == 0x80) ++from;
    return "…" + text.substr(from);
}

std::string cluster::head_utf8(const std::string& text, const std::size_t max)
{
    // the start of a text, cut on a character (a cut inside one made an invalid string: a JSON dump threw)
    if (text.size() <= max) return text;
    std::size_t to = max;
    while (to > 0 && (static_cast<unsigned char>(text[to]) & 0xC0) == 0x80) --to;
    return text.substr(0, to) + "\n[...]";
}

_cold void cluster::detach(const std::vector<std::string>& args)
{
    if (args.empty() || !cluster::has_command(args[0])) return;
    std::thread([args]() {
        try {
            (void)cluster::capture(args, "", 30);
        } catch (...) {} // a notification that fails is not an error of the tool
    }).detach();
}

_cold void cluster::complete_path(void)
{
    // started by systemd (the service), a desktop file...: PATH without the folders where claude, qwen, opencode live
    const char* current = std::getenv("PATH");
    std::string path = current ? current : "/usr/local/bin:/usr/bin:/bin";
    for (const char* folder: {"~/.local/bin", "~/.npm-global/bin", "~/.opencode/bin", "~/.bun/bin", "~/.cargo/bin", "~/go/bin", "~/.deno/bin", "/usr/local/bin"}) {
        const std::string dir = cluster::expand_home(folder).string();
        const std::vector<std::string> parts = cluster::split(path, ':');
        std::error_code error;
        if (std::find(parts.begin(), parts.end(), dir) == parts.end() && std::filesystem::is_directory(dir, error)) path += ":" + dir;
    }
    ::setenv("PATH", path.c_str(), 1);
}

/* the hot upgrade */
static std::vector<std::string> program_args_;     // NOLINT: this run, as started
static std::atomic<bool> upgrade_wanted_{false};    // NOLINT
static std::atomic<bool> upgrade_restore_{false};   // NOLINT

// xstyle: ignore-next CPP-SINGLE-STATEMENT (its state is private to Tools.cpp)
_cold void cluster::set_program_args(const int argc, char** argv)
{
    program_args_.assign(argv, argv + argc);
}

_cold void cluster::upgrade_request(const bool restore)
{
    upgrade_restore_ = restore;
    upgrade_wanted_ = true;
}

// xstyle: ignore-next CPP-SINGLE-STATEMENT (its state is private to Tools.cpp)
_cold bool cluster::upgrade_requested(void)
{
    return upgrade_wanted_;
}

_cold void cluster::upgrade_exec(void)
{
    // the binary on disk (the new one) in place of this process: the same arguments; a core: its sessions restored at once
    if (program_args_.empty()) return;
    std::vector<std::string> args = program_args_;
    args[0] = cluster::self_exe().string();
    if (upgrade_restore_ && std::find(args.begin(), args.end(), "--restore") == args.end()) args.push_back("--restore");
    ::setenv("CLAUDE_CLUSTER_UPGRADED_FROM", CLUSTER_VERSION, 1);
    std::vector<char*> argv;
    for (std::string& arg: args)
        argv.push_back(arg.data());
    argv.push_back(nullptr);
    ::execv(args[0].c_str(), argv.data());
    std::cerr << "claude-cluster: the update failed (" << std::strerror(errno) << "): start it again" << std::endl;
}

_cold int cluster::version_compare(const std::string& a, const std::string& b)
{
    const std::vector<std::string> x = cluster::split(a, '.');
    const std::vector<std::string> y = cluster::split(b, '.');
    for (std::size_t i = 0; i < std::max(x.size(), y.size()); ++i) {
        const long u = i < x.size() ? std::strtol(x[i].c_str(), nullptr, 10) : 0;
        const long v = i < y.size() ? std::strtol(y[i].c_str(), nullptr, 10) : 0;
        if (u != v) return u < v ? -1 : 1;
    }
    return 0;
}

_cold std::string cluster::upgrade_selftest(const std::filesystem::path& binary)
{
    // the new binary run as a core of its own (own config, data and socket, no global session) and asked its status: an update to a binary
    // that does not start must never replace the running one
    std::error_code error;
    const std::filesystem::path dir = cluster::data_dir() / ("upgrade-test-" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir / "cfg", error);
    std::filesystem::create_directories(dir / "data", error);
    std::ofstream(dir / "cfg" / "config.toml") << "[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = \"t\"\nemail = \"t@t.t\"\n[usage]\naccount = false\n";
    const std::string socket = (dir / "sock").string();
    std::string problem;
    const pid_t child = ::fork(); // xstyle: ignore LU-SYSCALL (the new binary tried alone: a process of its own)
    if (child < 0) return "fork failed (" + std::string(std::strerror(errno)) + ")";
    if (child == 0) {
        ::setsid();
        const int null = ::open("/dev/null", O_RDWR);
        if (null >= 0) {
            ::dup2(null, 0); // xstyle: ignore LU-SYSCALL (the new binary tried alone: a process of its own)
            ::dup2(null, 1); // xstyle: ignore LU-SYSCALL (the new binary tried alone: a process of its own)
            ::dup2(null, 2); // xstyle: ignore LU-SYSCALL (the new binary tried alone: a process of its own)
        }
        ::setenv("CLAUDE_CLUSTER_CONFIG_DIR", (dir / "cfg").c_str(), 1);
        ::setenv("CLAUDE_CLUSTER_DATA_DIR", (dir / "data").c_str(), 1);
        ::setenv("CLAUDE_CLUSTER_SOCKET", socket.c_str(), 1);
        ::unsetenv("CLAUDE_CLUSTER_UPGRADED_FROM");
        ::execl(binary.c_str(), binary.c_str(), "serve", static_cast<char*>(nullptr));
        ::_exit(127);
    }
    bool answered = false;
    for (int i = 0; i < 100 && !answered; ++i) { // 10 s
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int status = 0;
        if (::waitpid(child, &status, WNOHANG) == child) {
            problem = WIFSIGNALED(status) ? "it crashed at the start (signal " + std::to_string(WTERMSIG(status)) + ")" : "it stopped at the start (exit " + std::to_string(WEXITSTATUS(status)) + ")";
            std::filesystem::remove_all(dir, error);
            return problem;
        }
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0); // xstyle: ignore LU-SOCKET (the new binary tried alone: a process of its own)
        if (fd < 0) continue;
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, socket.c_str(), sizeof(address.sun_path) - 1);
        answered = ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
        ::close(fd);
    }
    if (!answered) problem = "it did not answer within 10 s";
    ::kill(-child, SIGTERM);
    for (int i = 0; i < 30; ++i) { // 3 s to stop, then killed
        int status = 0;
        if (::waitpid(child, &status, WNOHANG) == child) {
            std::filesystem::remove_all(dir, error);
            return problem;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ::kill(-child, SIGKILL);
    (void)::waitpid(child, nullptr, 0);
    std::filesystem::remove_all(dir, error);
    return problem;
}

_cold std::string cluster::binary_version(const std::filesystem::path& binary)
{
    const cluster::Captured got = cluster::capture({binary.string(), "--version"}, "", 10);
    const std::size_t at = got.out.find("claude-cluster ");
    if (got.code != 0 || at == std::string::npos) return "";
    const std::string version = cluster::trim(got.out.substr(at + 15, got.out.find('\n', at) == std::string::npos ? std::string::npos : got.out.find('\n', at) - at - 15));
    return version.find_first_not_of("0123456789.") == std::string::npos && !version.empty() ? version : "";
}

_cold void cluster::catch_quit_signals(void)
{
    // only a flag here (async signal safe): the interface sees it and quits the normal way (terminal restored, saved)
    for (const int signal: {SIGINT, SIGTERM, SIGHUP, SIGQUIT})
        (void)std::signal(signal, [](int) {cluster::quit_signal = true;});
}

/* json files */
_cold std::optional<cluster::Json> cluster::read_json(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) return std::nullopt;
    try {
        return cluster::Json::parse(file);
    } catch (const cluster::Json::exception&) {
        return std::nullopt;
    }
}

_cold void cluster::private_dir(const std::filesystem::path& dir)
{
    // the conversations, the history, the config and the keys of the web: this user only (the home folder is often 0755)
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    ::chmod(dir.c_str(), S_IRWXU);
}

_cold bool cluster::hidden_path(const std::string& path)
{
    // a component starting with a dot (the keys, the configs, the credentials of everything), the folders of claude-cluster
    const std::filesystem::path full = std::filesystem::absolute(cluster::expand_home(path)).lexically_normal();
    for (const std::filesystem::path& part: full)
        if (part.string().size() > 1 && part.string().starts_with(".")) return true;
    const std::string text = full.string();
    return text.starts_with(cluster::config_dir().string()) || text.starts_with(cluster::data_dir().string());
}

_cold void cluster::write_json(const std::filesystem::path& path, const cluster::Json& data)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    const std::filesystem::path tmp = path.string() + ".tmp";
    {
        std::ofstream file(tmp);
        if (!file) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, tmp.string());
        file << data.dump(2, ' ', false, cluster::JSON_LENIENT) << "\n";
    }
    std::filesystem::rename(tmp, path, error);
    if (error) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, path.string() + ": " + error.message());
}

_cold std::string cluster::host_name(void)
{
    std::array<char, 256> name{};
    if (::gethostname(name.data(), name.size() - 1) != 0) return "this computer";
    return name.data();
}

_cold std::string cluster::git_branch(const std::filesystem::path& folder)
{
    // .git/HEAD of the folder or of one above (a worktree: its gitdir file)
    std::error_code error;
    for (std::filesystem::path dir = std::filesystem::weakly_canonical(folder, error); !dir.empty(); dir = dir.parent_path()) {
        const std::filesystem::path git = dir / ".git";
        std::filesystem::path head = git / "HEAD";
        if (std::filesystem::is_regular_file(git, error)) { // a worktree: "gitdir: <path>"
            std::ifstream in(git);
            std::string line;
            std::getline(in, line);
            if (line.starts_with("gitdir: ")) head = std::filesystem::path(cluster::trim(line.substr(8))) / "HEAD";
        }
        if (std::filesystem::is_regular_file(head, error)) {
            std::ifstream in(head);
            std::string line;
            std::getline(in, line);
            if (line.starts_with("ref: refs/heads/")) return cluster::trim(line.substr(16));
            return line.size() >= 7 ? "detached " + line.substr(0, 7) : "";
        }
        if (dir == dir.root_path()) break;
    }
    return "";
}

_cold void cluster::open_folder(const std::filesystem::path& folder)
{
    cluster::detach({"xdg-open", folder.string()});
}

_cold std::string cluster::open_terminal(const std::filesystem::path& folder)
{
    // the first terminal installed, opened in this folder
    const std::string dir = folder.string();
    const std::vector<std::pair<std::string, std::vector<std::string>>> terminals = {
        {"ptyxis", {"ptyxis", "--new-window", "--working-directory=" + dir}}, {"gnome-terminal", {"gnome-terminal", "--working-directory=" + dir}},
        {"kgx", {"kgx", "--working-directory=" + dir}}, {"kitty", {"kitty", "--directory", dir}}, {"konsole", {"konsole", "--workdir", dir}},
        {"alacritty", {"alacritty", "--working-directory", dir}}, {"wezterm", {"wezterm", "start", "--cwd", dir}}, {"foot", {"foot", "--working-directory=" + dir}},
        {"xterm", {"xterm", "-e", "sh", "-c", "cd \"$0\" && exec \"${SHELL:-sh}\"", dir}}};
    for (const auto &[name, command]: terminals)
        if (cluster::has_command(name)) {
            cluster::detach(command);
            return name;
        }
    return "";
}

_cold bool cluster::copy_text(const std::string& text)
{
    // the clipboard of the desktop: Wayland (wl-copy), else X11 (xclip, xsel: the text given on their input)
    if (std::getenv("WAYLAND_DISPLAY") != nullptr && cluster::has_command("wl-copy")) {
        cluster::detach({"wl-copy", "--", text});
        return true;
    }
    for (const char* tool: {"xclip -selection clipboard", "xsel --clipboard --input"})
        if (std::getenv("DISPLAY") != nullptr && cluster::has_command(cluster::split(tool, ' ')[0])) {
            cluster::detach({"sh", "-c", std::string("printf %s \"$1\" | ") + tool, "sh", text});
            return true;
        }
    return false;
}

_cold std::string cluster::build_id(void)
{
    // read once, at the start: the binary installed again meanwhile is another build (its date changed)
    static const std::string id = []() {
        struct stat info{};
        const std::int64_t seconds = ::stat(cluster::self_exe().c_str(), &info) == 0 ? static_cast<std::int64_t>(info.st_mtime) : 0; // unix time
        return std::string(CLUSTER_VERSION) + "+" + std::to_string(seconds);
    }();
    return id;
}

/* debug mode */
namespace {
    std::mutex debugMutex;
    std::atomic<bool> debugAll{false};
    std::set<std::string> debugTopics;
    std::string debugRole = "process";
    bool debugHeader = false;
}

_cold std::string cluster::platform_summary(void)
{
    // what changes the behaviour: the OS, the graphic session, the desktop, the terminal
    struct utsname name{};
    const std::string os = ::uname(&name) == 0 ? std::string(name.sysname) + " " + name.release : std::string("unknown OS");
    const std::function<std::string(const char*)> env = [](const char* key) {return std::getenv(key) != nullptr ? std::string(std::getenv(key)) : std::string();};
    const std::string session = !env("XDG_SESSION_TYPE").empty() ? env("XDG_SESSION_TYPE") : !env("WAYLAND_DISPLAY").empty() ? "wayland" : !env("DISPLAY").empty() ? "x11" : "no display";
    std::string out = os + " · " + session;
    if (!env("XDG_CURRENT_DESKTOP").empty()) out += " · " + env("XDG_CURRENT_DESKTOP");
    if (!env("TERM").empty()) out += " · TERM " + env("TERM") + (env("COLORTERM").empty() ? "" : " (" + env("COLORTERM") + ")");
    if (!env("TERM_PROGRAM").empty()) out += " · " + env("TERM_PROGRAM");
    return out;
}

_cold void cluster::debug_configure(const bool on, const std::vector<std::string>& topics)
{
    {
        std::lock_guard<std::mutex> lock(debugMutex);
        debugTopics.clear();
        for (const std::string& topic: topics)
            if (!cluster::trim(topic).empty()) debugTopics.insert(cluster::trim(topic));
    }
    const bool was = debugAll.exchange(on);
    if (on && !was) cluster::debug("platform", "debug on (" + cluster::platform_summary() + ")");
}

_cold void cluster::debug_role(const std::string& role)
{
    std::lock_guard<std::mutex> lock(debugMutex);
    debugRole = role;
}

_cold bool cluster::debug_on(const std::string& topic)
{
    if (!debugAll) return false;
    std::lock_guard<std::mutex> lock(debugMutex);
    return debugTopics.empty() || debugTopics.contains("all") || debugTopics.contains(topic);
}

_cold void cluster::debug(const std::string& topic, const std::string& text)
{
    if (!cluster::debug_on(topic)) return;
    std::lock_guard<std::mutex> lock(debugMutex);
    const std::filesystem::path path = cluster::debug_log_path();
    std::error_code error;
    if (std::filesystem::exists(path, error) && std::filesystem::file_size(path, error) > 4 * 1024 * 1024) // kept small: the last 4 MB + the previous ones
        std::filesystem::rename(path, path.string() + ".1", error);
    std::ofstream out(path, std::ios::app);
    if (!out) return;
    const std::int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const std::time_t seconds = static_cast<std::time_t>(ms / 1000);
    std::tm local{};
    (void)::localtime_r(&seconds, &local);
    char stamp[32] = {};
    (void)std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
    if (!debugHeader) { // the first line of this process: what it runs on
        debugHeader = true;
        out << stamp << " [" << debugRole << " " << ::getpid() << "] platform: " << cluster::platform_summary() << " · claude-cluster " CLUSTER_VERSION "\n";
    }
    out << stamp << "." << std::setw(3) << std::setfill('0') << ms % 1000 << " [" << debugRole << " " << ::getpid() << "] " << topic << ": " << text << "\n";
}

/* limits of the files that grow */
_cold bool cluster::keep_newest(const std::filesystem::path& path, const std::size_t maxLines, const std::uintmax_t maxBytes)
{
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || (maxBytes > 0 && size <= maxBytes && maxLines == 0)) return false;
    std::ifstream in(path, std::ios::binary);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line))
        if (!line.empty()) lines.push_back(line);
    in.close();
    std::size_t first = maxLines > 0 && lines.size() > maxLines ? lines.size() - maxLines : 0;
    if (maxBytes > 0) {
        std::uintmax_t kept = 0;
        for (std::size_t i = lines.size(); i-- > first;) {
            kept += lines[i].size() + 1;
            if (kept > maxBytes) {
                first = i + 1;
                break;
            }
        }
    }
    if (first == 0) return false;
    const std::filesystem::path temporary = path.string() + ".cut";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        for (std::size_t i = first; i < lines.size(); ++i)
            out << lines[i] << "\n";
    }
    std::filesystem::rename(temporary, path, error);
    return !error;
}

_cold std::string cluster::interactive_program(const std::string& command)
{
    // the first program of each part of a shell command (; && || |) that needs a terminal: an editor, a pager, a monitor, a client
    // that talks to you, a REPL without a script; empty: nothing that could not run without one
    static const std::set<std::string> always = {"vim", "vi", "nvim", "view", "vimdiff", "ex", "nano", "pico", "emacs", "micro", "hx", "helix", "kak", "joe", "mcedit",
        "less", "more", "most", "man", "info", "top", "htop", "btop", "atop", "nmon", "glances", "iotop", "iftop", "nethogs", "watch", "tmux", "screen", "byobu", "zellij",
        "mc", "ranger", "nnn", "lf", "vifm", "fzf", "lazygit", "tig", "gitui", "k9s", "lazydocker", "ncdu", "cmatrix", "telnet", "ftp", "sftp", "mysql", "psql", "sqlite3",
        "redis-cli", "mongosh", "mongo", "gdb", "lldb", "irb", "pry", "ipython", "bpython", "cfdisk", "alsamixer", "pulsemixer", "newsboat", "mutt", "neomutt", "weechat", "irssi"};
    static const std::set<std::string> repl = {"python", "python3", "node", "ruby", "php", "lua", "luajit", "julia", "R", "ghci", "bash", "zsh", "sh", "fish", "dash", "ksh", "nu"};
    static const std::set<std::string> skip = {"sudo", "doas", "exec", "env", "time", "nice", "command", "nohup", "stdbuf", "xargs"};
    std::string part;
    std::vector<std::string> parts;
    for (std::size_t i = 0; i < command.size(); ++i) {
        const char c = command[i];
        if (c == ';' || c == '|' || c == '&' || c == '\n') {
            parts.push_back(part);
            part.clear();
            if ((c == '|' || c == '&') && i + 1 < command.size() && command[i + 1] == c) ++i;
        } else {
            part += c;
        }
    }
    parts.push_back(part);
    for (const std::string& each: parts) {
        std::vector<std::string> words;
        for (const std::string& w: cluster::split(cluster::trim(each), ' '))
            if (!w.empty()) words.push_back(w);
        std::size_t k = 0;
        while (k < words.size() && (skip.contains(std::filesystem::path(words[k]).filename().string()) || words[k].find('=') != std::string::npos || (k > 0 && words[k].starts_with("-"))))
            ++k;
        if (k >= words.size()) continue;
        const std::string program = std::filesystem::path(words[k]).filename().string();
        std::size_t arguments = 0; // the words after it that are not options
        for (std::size_t j = k + 1; j < words.size(); ++j)
            if (!words[j].starts_with("-")) ++arguments;
        if (always.contains(program)) return program;
        if (repl.contains(program) && arguments == 0 && std::none_of(words.begin() + static_cast<long>(k) + 1, words.end(), [](const std::string& w) {return w == "-c" || w == "-e" || w == "--version" || w == "-V" || w == "-h" || w == "--help";}))
            return program; // a REPL (a script, -c, -e, a version: not)
        if (program == "ssh" && arguments == 1) return program; // a login (a command after the host: not)
        if (program == "docker" || program == "podman" || program == "kubectl")
            for (const std::string& w: words)
                if (w == "-it" || w == "-ti" || w == "-i" || w == "-t") return program; // an interactive container
    }
    return "";
}

_cold std::string cluster::open_terminal_command(const std::filesystem::path& folder, const std::string& command)
{
    // a terminal emulator running the command in the folder, kept open at its end (Enter closes it)
    const std::string dir = folder.string();
    const std::string run = command + "; printf '\\n[done: Enter closes this terminal] '; read answer";
    const std::vector<std::pair<std::string, std::vector<std::string>>> terminals = {
        {"ptyxis", {"ptyxis", "--new-window", "--working-directory=" + dir, "--", "sh", "-c", run}}, {"gnome-terminal", {"gnome-terminal", "--working-directory=" + dir, "--", "sh", "-c", run}},
        {"kgx", {"kgx", "--working-directory=" + dir, "-e", "sh -c '" + run + "'"}}, {"kitty", {"kitty", "--directory", dir, "sh", "-c", run}},
        {"konsole", {"konsole", "--workdir", dir, "-e", "sh", "-c", run}}, {"alacritty", {"alacritty", "--working-directory", dir, "-e", "sh", "-c", run}},
        {"wezterm", {"wezterm", "start", "--cwd", dir, "--", "sh", "-c", run}}, {"foot", {"foot", "--working-directory=" + dir, "sh", "-c", run}},
        {"xterm", {"xterm", "-e", "sh", "-c", "cd \"$0\" && " + run, dir}}};
    for (const auto &[name, argv]: terminals)
        if (cluster::has_command(name)) {
            cluster::detach(argv);
            return name;
        }
    return "";
}
