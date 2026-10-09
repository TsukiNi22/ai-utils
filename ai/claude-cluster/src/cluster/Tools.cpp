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
#include <sys/epoll.h>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <array>
#include <ctime>

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
    const std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::path("claude-cluster") : exe;
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
    // Count of frequent words + accents: enough to pick the voice of an answer
    static const std::array<const char*, 24> fr = {"le", "la", "les", "des", "est", "une", "un", "et", "pour", "pas", "que", "qui",
        "dans", "sur", "avec", "je", "tu", "vous", "nous", "ce", "c'est", "du", "au", "mais"};
    static const std::array<const char*, 24> en = {"the", "is", "are", "and", "for", "not", "that", "which", "in", "on", "with",
        "i", "you", "we", "this", "it's", "of", "to", "a", "an", "but", "was", "be", "has"};
    int scoreFr = 0;
    int scoreEn = 0;

    for (const std::string& word: cluster::split(cluster::lower(text), ' ')) {
        std::string w = word;
        w.erase(std::remove_if(w.begin(), w.end(), [](char c) {return c == ',' || c == '.' || c == '!' || c == '?' || c == ':';}), w.end());
        scoreFr += std::find_if(fr.begin(), fr.end(), [&](const char* f) {return w == f;}) != fr.end();
        scoreEn += std::find_if(en.begin(), en.end(), [&](const char* e) {return w == e;}) != en.end();
    }
    for (const char* accent: {"é", "è", "à", "ç", "ê", "ù"})
        if (text.find(accent) != std::string::npos) scoreFr += 2;
    return scoreEn > scoreFr ? "en" : "fr";
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

_cold cluster::Captured cluster::capture(const std::vector<std::string>& args, const std::string& cwd, const int timeout)
{
    cluster::Captured result;
    if (args.empty()) return result;
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
    (void)process.spawn("env", envArgs);
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
        process.kill();
        result.code = -1;
        return result;
    }
    const utils::encapsulation::Status status = process.wait();
    result.code = status.exited ? status.code : -1;
    return result;
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

_cold void cluster::write_json(const std::filesystem::path& path, const cluster::Json& data)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    const std::filesystem::path tmp = path.string() + ".tmp";
    {
        std::ofstream file(tmp);
        if (!file) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, tmp.string());
        file << data.dump(2) << "\n";
    }
    std::filesystem::rename(tmp, path, error);
    if (error) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, path.string() + ": " + error.message());
}
