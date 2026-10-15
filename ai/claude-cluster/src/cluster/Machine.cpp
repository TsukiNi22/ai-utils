/**************************************************************\
Edition:
##  @date 11/10/2026 by @author Tsukini

File Name:
##  @file Machine.cpp

File Description:
##  Map of this computer (machine.md): read from the system files,
##  the commands of the tools, the shell (aliases, functions) and the
##  home folder (git repositories, 5 levels, hidden and generated
##  folders skipped), with what the user told; local only
\**************************************************************/

/* include */
#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Machine.hpp"
#include "cluster/Match.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Types.hpp"
#include <unordered_set>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <future>
#include <ctime>
#include <regex>
#include <map>

namespace {

struct Repository {
    std::string path;
    std::string remote;
    std::string branch;
    std::string kind;
    std::string activity;   // date of the last commit / checkout
};

_cold std::string env_(const char* name)
{
    const char* value = std::getenv(name);
    return value ? value : "";
}

_cold std::string date_(const std::filesystem::file_time_type time)
{
    const std::time_t seconds = std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::file_clock::to_sys(time)));
    std::tm local{};
    ::localtime_r(&seconds, &local);
    char out[32] = {};
    std::strftime(out, sizeof(out), "%Y-%m-%d %H:%M", &local);
    return out;
}

_cold std::string editor_(void)
{
    // the one claude-cluster opens (Ctrl+E, settings) and what $EDITOR / $VISUAL say (often a default of the system)
    std::string used;
    for (const char* name: {"nvim", "vim", "vi", "emacs", "nano"})
        if (used.empty() && cluster::has_command(name)) used = name;
    std::string text = (used.empty() ? std::string("none found") : used) + " (claude-cluster: [ui] editor, else the first of nvim, vim, vi, emacs, nano)";
    const std::string editor = env_("EDITOR");
    const std::string visual = env_("VISUAL");
    std::error_code error;
    const bool fedora = editor.ends_with("nano") && std::filesystem::exists("/etc/profile.d/nano-default-editor.sh", error);
    if (!editor.empty()) text += " · $EDITOR = " + editor + (fedora ? " (the default of the system: /etc/profile.d/nano-default-editor.sh)" : "");
    if (!visual.empty()) text += " · $VISUAL = " + visual;
    return text;
}

_cold std::string read_(const std::filesystem::path& path)
{
    std::ifstream file(path);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

_cold std::string first_line_(const std::vector<std::string>& command, const int timeout = 5)
{
    // the first line of a command (its version...), empty when it fails
    if (command.empty() || !cluster::has_command(command[0])) return "";
    const cluster::Captured out = cluster::capture(command, "", timeout);
    if (out.code != 0) return "";
    std::istringstream lines(out.out);
    std::string line;
    while (std::getline(lines, line))
        if (!cluster::trim(line).empty()) return cluster::trim(line);
    return "";
}

_cold std::vector<std::string> lines_of_(const std::vector<std::string>& command, const int timeout = 10)
{
    std::vector<std::string> lines;
    if (command.empty() || !cluster::has_command(command[0])) return lines;
    const cluster::Captured out = cluster::capture(command, "", timeout);
    if (out.code != 0) return lines;
    std::istringstream stream(out.out);
    for (std::string line; std::getline(stream, line);)
        if (!cluster::trim(line).empty()) lines.push_back(cluster::trim(line));
    return lines;
}

_cold std::string join_(const std::vector<std::string>& items, const std::string& separator, const std::size_t max = 1000)
{
    std::string text;
    for (std::size_t i = 0; i < items.size() && i < max; ++i)
        text += (i ? separator : "") + items[i];
    if (items.size() > max) text += separator + "... (" + std::to_string(items.size() - max) + " more)";
    return text;
}

_cold std::string hardware_(void)
{
    // CPU, memory, GPU (lspci), disks (df: the real file systems)
    std::string cpu;
    int cores = 0;
    std::istringstream info(read_("/proc/cpuinfo"));
    for (std::string line; std::getline(info, line);) {
        if (line.starts_with("model name") && cpu.empty()) cpu = cluster::trim(line.substr(line.find(':') + 1));
        if (line.starts_with("processor")) ++cores;
    }
    std::string memory;
    std::istringstream mem(read_("/proc/meminfo"));
    for (std::string line; std::getline(mem, line);)
        if (line.starts_with("MemTotal:")) memory = std::to_string(std::stoll(cluster::trim(line.substr(9))) / 1024 / 1024) + " GB";
    std::vector<std::string> gpus;
    for (const std::string& line: lines_of_({"lspci"}))
        if (line.find("VGA") != std::string::npos || line.find("3D controller") != std::string::npos) gpus.push_back(line.substr(line.find(": ") + 2));
    std::string text = "| | |\n|---|---|\n| CPU | " + cpu + " (" + std::to_string(cores) + " threads) |\n| Memory | " + memory + " |\n| GPU | "
        + (gpus.empty() ? std::string("?") : join_(gpus, "; ")) + " |\n\nDisks:\n\n| Mounted on | Size | Free |\n|---|---|---|\n";
    for (const std::string& line: lines_of_({"df", "-h", "--output=target,size,avail", "-x", "tmpfs", "-x", "devtmpfs", "-x", "squashfs", "-x", "overlay", "-x", "efivarfs"})) {
        const std::vector<std::string> parts = cluster::split(line, ' ');
        if (parts.size() >= 3 && parts[0] != "Mounted" && parts[0] != "Monté") text += "| `" + parts[0] + "` | " + parts[1] + " | " + parts[2] + " |\n";
    }
    return text;
}

_cold std::string tools_(std::size_t& found)
{
    // the tools of the PATH and their version
    static const std::vector<std::pair<std::string, std::vector<std::string>>> tools = {
        {"git", {"git", "--version"}}, {"gh", {"gh", "--version"}}, {"claude", {"claude", "--version"}}, {"gcc", {"gcc", "--version"}},
        {"g++", {"g++", "--version"}}, {"clang", {"clang", "--version"}}, {"clang++", {"clang++", "--version"}}, {"cmake", {"cmake", "--version"}},
        {"make", {"make", "--version"}}, {"ninja", {"ninja", "--version"}}, {"python3", {"python3", "--version"}}, {"pip3", {"pip3", "--version"}},
        {"node", {"node", "--version"}}, {"npm", {"npm", "--version"}}, {"cargo", {"cargo", "--version"}}, {"rustc", {"rustc", "--version"}},
        {"go", {"go", "version"}}, {"java", {"java", "-version"}}, {"docker", {"docker", "--version"}}, {"podman", {"podman", "--version"}},
        {"nvim", {"nvim", "--version"}}, {"vim", {"vim", "--version"}}, {"tmux", {"tmux", "-V"}}, {"zsh", {"zsh", "--version"}},
        {"xstyle", {"xstyle", "--version"}}, {"rtk", {"rtk", "--version"}}, {"voice-listen", {}}};
    std::string text = "| Tool | Version |\n|---|---|\n";
    std::vector<std::string> missing;
    found = 0;
    for (const auto &[name, command]: tools) {
        if (!cluster::has_command(name)) {
            missing.push_back(name);
            continue;
        }
        ++found;
        text += "| `" + name + "` | " + cluster::one_line(command.empty() ? std::string("installed") : first_line_(command), 90) + " |\n";
    }
    return text + "\nMissing: " + join_(missing, ", ") + "\n";
}

_cold std::pair<std::string, std::vector<std::string>> shell_once_(const std::string& shell)
{
    // the shell of the user started once (interactive, a session of its own: setsid, else it takes the terminal and
    // claude-cluster is stopped by SIGTTIN): its aliases, then its functions after a mark
    const std::string functions = shell.ends_with("zsh") ? "print -rl -- ${(ok)functions}" : "compgen -A function";
    std::vector<std::string> command = {shell, "-ic", "alias; echo __CLUSTER_FUNCTIONS__; " + functions};
    if (cluster::has_command("setsid")) command.insert(command.begin(), {"setsid", "-w"});
    const std::string out = cluster::capture(command, "", 15).out;
    const std::size_t mark = out.find("__CLUSTER_FUNCTIONS__");
    std::vector<std::string> names;
    std::istringstream lines(mark == std::string::npos ? "" : out.substr(mark + 21));
    for (std::string line; std::getline(lines, line);)
        if (!cluster::trim(line).empty()) names.push_back(cluster::trim(line));
    return {mark == std::string::npos ? out : out.substr(0, mark), names};
}

_cold std::string shell_more_(const std::vector<std::string>& functionNames)
{
    // the PATH, the functions of the shell (their names), the names of the variables (never their values)
    std::string text = "PATH:\n";
    for (const std::string& dir: cluster::split(env_("PATH"), ':'))
        text += "- `" + dir + "`\n";
    std::vector<std::string> functions;
    for (const std::string& name: functionNames)
        if (!name.starts_with("_") && name.find(' ') == std::string::npos && name.find('=') == std::string::npos) functions.push_back("`" + name + "`");
    text += "\nFunctions of the shell (" + std::to_string(functions.size()) + "): " + join_(functions, ", ", 120) + "\n";
    std::vector<std::string> names;
    for (char** env = environ; env && *env; ++env) {
        const std::string entry = *env;
        names.push_back("`" + entry.substr(0, entry.find('=')) + "`");
    }
    std::sort(names.begin(), names.end());
    return text + "\nEnvironment variables (names only): " + join_(names, ", ") + "\n";
}

_cold std::string packages_(void)
{
    // the package managers and what was installed with them (counts for the system ones)
    std::string text = "| Manager | Installed |\n|---|---|\n";
    const std::vector<std::string> rpm = lines_of_({"rpm", "-qa", "--qf", "%{NAME}\\n"}, 20);
    if (!rpm.empty()) text += "| rpm / dnf | " + std::to_string(rpm.size()) + " packages |\n";
    const std::vector<std::string> deb = lines_of_({"dpkg-query", "-f", "${Package}\\n", "-W"}, 20);
    if (!deb.empty()) text += "| dpkg / apt | " + std::to_string(deb.size()) + " packages |\n";
    const std::vector<std::string> flatpak = lines_of_({"flatpak", "list", "--app", "--columns=application"});
    if (!flatpak.empty()) text += "| flatpak | " + join_(flatpak, ", ") + " |\n";
    std::vector<std::string> pip;
    for (const std::string& line: lines_of_({"python3", "-m", "pip", "list", "--user", "--format=freeze"}, 20))
        pip.push_back(line.substr(0, line.find("==")));
    if (!pip.empty()) text += "| pip --user | " + join_(pip, ", ", 60) + " |\n";
    std::vector<std::string> npm;
    for (const std::string& line: lines_of_({"npm", "ls", "-g", "--depth=0", "--parseable"}, 20))
        if (line.find("node_modules/") != std::string::npos) npm.push_back(line.substr(line.rfind("node_modules/") + 13));
    if (!npm.empty()) text += "| npm -g | " + join_(npm, ", ") + " |\n";
    std::vector<std::string> cargo;
    for (const std::string& line: lines_of_({"cargo", "install", "--list"}))
        if (!line.starts_with(" ")) cargo.push_back(line.substr(0, line.find(' ')));
    if (!cargo.empty()) text += "| cargo install | " + join_(cargo, ", ") + " |\n";
    return text;
}

_cold std::string hosts_(const std::vector<std::string>& remotes)
{
    // the git hosts of the repositories and how many each
    std::map<std::string, int> hosts;
    for (const std::string& remote: remotes) {
        std::smatch match;
        if (std::regex_search(remote, match, std::regex(R"((?:@|://)([^/:]+))"))) ++hosts[match[1]];
    }
    std::vector<std::string> list;
    for (const auto &[host, count]: hosts)
        list.push_back(host + " (" + std::to_string(count) + ")");
    return join_(list, ", ");
}

_cold std::string development_(void)
{
    // git / GitHub identity, Claude Code (skills, plugins, MCP servers, CLAUDE.md), containers
    std::string text = "| | |\n|---|---|\n| git user | " + first_line_({"git", "config", "--global", "user.name"}) + " <" + first_line_({"git", "config", "--global", "user.email"})
        + "> |\n";
    std::string account;
    for (const std::string& line: lines_of_({"gh", "auth", "status"}))
        if (line.find("account") != std::string::npos && account.empty()) account = line;
    text += "| GitHub (gh) | " + (account.empty() ? std::string("not logged in") : cluster::one_line(account, 100)) + " |\n";
    text += "| git hosts | __GIT_HOSTS__ |\n\nClaude Code:\n\n";
    const cluster::Json settings = cluster::Json::parse(read_(cluster::expand_home("~/.claude/settings.json")), nullptr, false);
    if (settings.is_object()) text += "- model: " + settings.value("model", std::string("default")) + ", permission mode: "
        + (settings.contains("permissions") && settings["permissions"].is_object() ? settings["permissions"].value("defaultMode", std::string("default")) : std::string("default")) + "\n";
    std::vector<std::string> skills;
    std::error_code error;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(cluster::expand_home("~/.claude/skills"), error))
        if (!entry.path().filename().string().starts_with(".")) skills.push_back(entry.path().filename().string());
    std::sort(skills.begin(), skills.end());
    text += "- skills (~/.claude/skills): " + (skills.empty() ? std::string("none") : join_(skills, ", ", 80)) + "\n";
    const cluster::Json plugins = cluster::Json::parse(read_(cluster::expand_home("~/.claude/plugins/installed_plugins.json")), nullptr, false);
    std::vector<std::string> pluginList;
    if (plugins.is_object() && plugins.contains("plugins") && plugins["plugins"].is_object())
        for (const auto &[name, value]: plugins["plugins"].items())
            pluginList.push_back(name);
    text += "- plugins: " + (pluginList.empty() ? std::string("none") : join_(pluginList, ", ", 60)) + "\n";
    const cluster::Json state = cluster::Json::parse(read_(cluster::expand_home("~/.claude.json")), nullptr, false);
    std::vector<std::string> mcp;
    if (state.is_object() && state.contains("mcpServers") && state["mcpServers"].is_object())
        for (const auto &[name, value]: state["mcpServers"].items())
            mcp.push_back(name);
    text += "- MCP servers (user): " + (mcp.empty() ? std::string("none") : join_(mcp, ", ")) + "\n";
    text += "- CLAUDE.md: `~/.claude/CLAUDE.md`" + std::string(std::filesystem::exists(cluster::expand_home("~/.claude/CLAUDE.md"), error) ? "" : " (none)") + "\n";
    for (const char* engine: {"docker", "podman"}) {
        if (!cluster::has_command(engine)) continue;
        const std::vector<std::string> images = lines_of_({engine, "images", "--format", "{{.Repository}}:{{.Tag}} ({{.Size}})"});
        const std::vector<std::string> containers = lines_of_({engine, "ps", "-a", "--format", "{{.Names}} - {{.Image}} - {{.Status}}"});
        text += "\n" + std::string(engine) + ": " + std::to_string(images.size()) + " images, " + std::to_string(containers.size()) + " containers"
            + (images.empty() && containers.empty() ? " (or its daemon not reachable without root)" : "") + "\n";
        for (const std::string& image: images)
            text += "- image `" + image + "`\n";
        for (const std::string& container: containers)
            text += "- container " + container + "\n";
    }
    return text;
}

_cold std::string personal_(const std::vector<std::string>& repoPaths)
{
    // the folders of the user (XDG ones, the ones holding the projects), locale / keyboard / time zone, SSH hosts
    std::string text = "Folders:\n\n";
    std::istringstream dirs(read_(cluster::expand_home("~/.config/user-dirs.dirs")));
    for (std::string line; std::getline(dirs, line);) {
        std::smatch match;
        if (std::regex_match(line, match, std::regex(R"re(XDG_(\w+)_DIR="(.*)")re"))) {
            std::string path = match[2];
            if (path.starts_with("$HOME")) path = "~" + path.substr(5);
            text += "- " + cluster::lower(match[1].str()) + ": `" + path + "`\n";
        }
    }
    std::map<std::string, int> roots;
    for (const std::string& path: repoPaths) {
        const std::size_t slash = path.find('/', 2);
        ++roots[slash == std::string::npos ? path : path.substr(0, slash)];
    }
    for (const auto &[root, count]: roots)
        if (count > 1) text += "- `" + root + "`: " + std::to_string(count) + " git repositories\n";
    std::string keyboard;
    for (const std::string& line: lines_of_({"localectl", "status"}))
        if (line.find("Layout") != std::string::npos) keyboard += (keyboard.empty() ? "" : ", ") + line;
    std::string zone = first_line_({"timedatectl", "show", "-p", "Timezone", "--value"});
    text += "\n| | |\n|---|---|\n| Language | " + env_("LANG") + " |\n| Keyboard | " + keyboard + " |\n| Time zone | " + zone + " |\n";
    std::vector<std::string> hosts;
    std::istringstream ssh(read_(cluster::expand_home("~/.ssh/config")));
    for (std::string line; std::getline(ssh, line);) {
        line = cluster::trim(line);
        if (!cluster::lower(line).starts_with("host ")) continue;
        for (const std::string& host: cluster::split(line.substr(5), ' '))
            if (host.find('*') == std::string::npos) hosts.push_back("`" + host + "`");
    }
    return text + "\nSSH hosts (~/.ssh/config, no key read): " + (hosts.empty() ? std::string("none") : join_(hosts, ", ")) + "\n";
}

_cold std::vector<std::string> editors_(void)
{
    std::vector<std::string> found;
    for (const char* name: {"nvim", "vim", "vi", "emacs", "nano", "code", "micro", "hx", "kate", "gedit", "gnome-text-editor"})
        if (cluster::has_command(name)) found.push_back(name);
    return found;
}

_cold Repository repository_(const std::filesystem::path& folder)
{
    // what .git tells without running git: origin, branch, last activity; the kind from the files of the project
    Repository repo{cluster::short_path(folder.string()), "", "", "", ""};
    std::error_code error;
    std::filesystem::path git = folder / ".git";
    if (std::filesystem::is_regular_file(git, error)) { // worktree / submodule: "gitdir: <path>"
        const std::string line = cluster::trim(read_(git));
        if (line.starts_with("gitdir:")) git = std::filesystem::absolute(folder / cluster::trim(line.substr(7)), error);
    }
    const std::string config = read_(git / "config");
    const std::size_t origin = config.find("[remote \"origin\"]");
    if (origin != std::string::npos) {
        std::smatch match;
        const std::string rest = config.substr(origin);
        if (std::regex_search(rest, match, std::regex(R"(url\s*=\s*(\S+))"))) repo.remote = match[1];
    }
    const std::string head = cluster::trim(read_(git / "HEAD"));
    repo.branch = head.starts_with("ref: refs/heads/") ? head.substr(16) : head.substr(0, 8);
    for (const char* name: {"logs/HEAD", "index", "HEAD"}) {
        if (!std::filesystem::exists(git / name, error)) continue;
        repo.activity = date_(std::filesystem::last_write_time(git / name, error)).substr(0, 10);
        break;
    }
    static const std::vector<std::pair<std::string, std::string>> markers = {
        {"CMakeLists.txt", "C/C++ (CMake)"}, {"Cargo.toml", "Rust"}, {"package.json", "Node / JS"}, {"pyproject.toml", "Python"},
        {"setup.py", "Python"}, {"requirements.txt", "Python"}, {"go.mod", "Go"}, {"pom.xml", "Java (Maven)"}, {"build.gradle", "Java / Kotlin (Gradle)"},
        {"Gemfile", "Ruby"}, {"composer.json", "PHP"}, {"Makefile", "make"}, {"default.project.json", "Roblox (Rojo)"}};
    std::vector<std::string> kinds;
    for (const auto &[file, kind]: markers)
        if (std::filesystem::exists(folder / file, error) && std::find(kinds.begin(), kinds.end(), kind) == kinds.end()) kinds.push_back(kind);
    for (const std::string& kind: kinds)
        repo.kind += (repo.kind.empty() ? "" : ", ") + kind;
    return repo;
}

_cold std::vector<Repository> repositories_(const std::function<void(double, const std::string&, std::size_t)>& progress)
{
    // the git repositories of the home folder: 5 levels, hidden / generated folders and symlinks skipped, bounded; its
    // folders one after the other (progress: the share done, the folder, the repositories found)
    static const std::unordered_set<std::string> skipped = {"node_modules", "venv", "env", "build", "target", "dist", "__pycache__", "snap",
        "Library", "go", "vendor", "site-packages", "cmake-build-debug", "cmake-build-release"};
    std::vector<Repository> found;
    std::vector<std::filesystem::path> tops;
    std::error_code error;
    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(cluster::expand_home("~"), std::filesystem::directory_options::skip_permission_denied, error)) {
        const std::string name = entry.path().filename().string();
        if (!name.starts_with(".") && !skipped.contains(name) && !entry.is_symlink(error) && entry.is_directory(error)) tops.push_back(entry.path());
    }
    std::sort(tops.begin(), tops.end());
    std::size_t visited = 0;
    for (std::size_t top = 0; top < tops.size(); ++top) {
        progress(static_cast<double>(top) / static_cast<double>(std::max<std::size_t>(tops.size(), 1)), cluster::short_path(tops[top].string()), found.size());
        std::vector<std::pair<std::filesystem::path, int>> stack = {{tops[top], 1}};
        while (!stack.empty() && found.size() < 400 && visited < 200000) {
            const auto [folder, depth] = stack.back();
            stack.pop_back();
            ++visited;
            if (depth > 0 && std::filesystem::exists(folder / ".git", error)) found.push_back(repository_(folder)); // and its nested ones
            if (depth >= 5) continue;
            for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(folder, std::filesystem::directory_options::skip_permission_denied, error)) {
                const std::string name = entry.path().filename().string();
                if (name.starts_with(".") || skipped.contains(name) || entry.is_symlink(error) || !entry.is_directory(error)) continue;
                stack.emplace_back(entry.path(), depth + 1);
            }
        }
    }
    progress(1.0, "", found.size());
    std::sort(found.begin(), found.end(), [](const Repository& a, const Repository& b) {return a.path < b.path;});
    return found;
}

_cold std::string aliases_(const std::string& shell, const std::string& output, std::size_t& total, std::size_t& beware)
{
    // the aliases of the shell of the user (an interactive one, started once), sorted: dangerous, replacing a common
    // command, the others
    static const std::unordered_set<std::string> common = {"git", "mkdir", "cd", "ls", "rm", "cp", "mv", "cat", "grep", "find", "sed", "awk", "sudo",
        "su", "sh", "bash", "zsh", "vi", "vim", "nvim", "nano", "curl", "wget", "ssh", "python", "python3", "pip", "make", "cmake", "gcc", "clang",
        "docker", "kill", "echo", "head", "tail", "less", "man", "chmod", "chown", "touch", "ln", "tar", "diff", "which", "top", "ps", "code", "open"};
    // what makes an alias dangerous, whatever its name (whole words: "block" is not "lock")
    static const std::vector<std::pair<std::regex, std::string>> danger = {
        {std::regex(R"(\block\b|lock-session|loginctl\s+lock)"), "locks the session"},
        {std::regex(R"(\brm\s+(-[a-zA-Z]*[rR][a-zA-Z]*|--recursive))"), "deletes files (rm -r)"},
        {std::regex(R"(\b(shutdown|poweroff|reboot|halt)\b)"), "powers off / restarts"},
        {std::regex(R"(\b(kill|pkill|killall)\b)"), "kills processes"},
        {std::regex(R"(\b(logout|terminate-session|terminate-user)\b)"), "ends the session"},
        {std::regex(R"(\bsystemctl\b)"), "changes services"},
        {std::regex(R"(\b(mkfs|wipefs|shred)\b|\bdd\s+if=|>\s*/dev/sd)"), "writes a disk"},
        {std::regex(R"(:\(\)\s*\{)"), "fork bomb"}};
    std::vector<std::string> dangerous;
    std::vector<std::string> replacing;
    std::vector<std::string> others;
    std::istringstream lines(output);
    const std::regex pattern(R"(^(?:alias\s+)?([^\s=]+)=(.*)$)");
    for (std::string line; std::getline(lines, line);) {
        std::smatch match;
        if (!std::regex_match(line, match, pattern)) continue;
        const std::string name = match[1];
        std::string value = match[2];
        if (value.size() >= 2 && (value.front() == '\'' || value.front() == '"') && value.back() == value.front()) value = value.substr(1, value.size() - 2);
        const std::string first = cluster::split(value, ' ').empty() ? "" : cluster::split(value, ' ')[0];
        const std::string row = "`" + name + "` = `" + cluster::one_line(value, 120) + "`";
        std::string reasons;
        for (const auto &[rule, reason]: danger)
            if (std::regex_search(value, rule)) reasons += (reasons.empty() ? "" : ", ") + reason;
        if (!reasons.empty())
            dangerous.push_back(row + " - **" + reasons + "**" + (common.contains(name) ? ": never run `" + name + "` bare, use `\\" + name + "` or `command " + name + "`"
                : ": never run it by mistake"));
        else if (common.contains(name) && first != name && first != "\\" + name && first != "command")
            replacing.push_back(row + " - replaces `" + name + "`: `\\" + name + "` / `command " + name + "` for the real one");
        else
            others.push_back(row);
    }
    total = dangerous.size() + replacing.size() + others.size();
    beware = dangerous.size() + replacing.size();
    std::string text = "Shell: `" + shell + "` (" + std::to_string(total) + " aliases)\n";
    if (!dangerous.empty()) text += "\n**Dangerous** (what they do when run):\n";
    for (const std::string& row: dangerous)
        text += "- " + row + "\n";
    if (!replacing.empty()) text += "\n**Replacing a common command** (the real one: `\\cmd` / `command cmd`):\n";
    for (const std::string& row: replacing)
        text += "- " + row + "\n";
    if (!others.empty()) text += "\nOthers:\n";
    for (const std::string& row: others)
        text += "- " + row + "\n";
    return text;
}

} // namespace

_cold std::string cluster::machine_scope(void)
{
    return "Map this computer for the global session? It reads: the OS, the kernel and the hardware (CPU, memory, GPU, "
        "disks); your user name, home, shell, its aliases, functions and PATH (the shell is started once to list them), the "
        "names of the environment variables (never their values); the tools and their versions; the packages installed "
        "(counts for the system ones); your git / GitHub identity; the Claude Code config (skills, plugins, MCP servers); "
        "the docker / podman images and containers; your folders, language, keyboard, time zone and SSH hosts (~/.ssh/config, "
        "no key); the git repositories of your home folder (5 levels, hidden folders skipped: path, origin, branch, kind, "
        "last activity - no file content). A few questions first (nickname, email, editor, language: config.toml [user]). "
        "Written to " + cluster::short_path(cluster::machine_file().string()) + " only: nothing is sent, the global session "
        "reads it when a request is about a local project, you or the system.";
}

std::vector<cluster::ProfileQuestion> cluster::machine_questions(const cluster::Config& config)
{
    // what the system doesn't tell (or in several ways), at every sync: the current value ready (Enter keeps it), else a
    // suggestion (git identity, the editors installed, $LANG)
    std::vector<cluster::ProfileQuestion> questions;
    const std::string gitName = first_line_({"git", "config", "--global", "user.name"});
    const std::string gitEmail = first_line_({"git", "config", "--global", "user.email"});
    const std::string lang = env_("LANG");
    std::vector<std::string> editors = editors_();
    if (!config.user.editor.empty() && std::find(editors.begin(), editors.end(), config.user.editor) == editors.end()) editors.insert(editors.begin(), config.user.editor);
    questions.push_back({"nickname", "Your nickname (how the global session calls you)",
        !config.user.nickname.empty() ? config.user.nickname : gitName.empty() ? env_("USER") : gitName, {}});
    questions.push_back({"email", "Your contact email (commits, messages written for you)", !config.user.email.empty() ? config.user.email : gitEmail, {}});
    questions.push_back({"editor", "Your main editor", !config.user.editor.empty() ? config.user.editor : editors.empty() ? std::string() : editors.front(), editors});
    questions.push_back({"language", "Language of the answers (empty: the one of each message)",
        !config.user.language.empty() ? config.user.language : lang.starts_with("fr") ? "fr" : lang.starts_with("en") ? "en" : "", {"fr", "en", ""}});
    questions.push_back({"notes", "Notes for the global session (anything to know about you, empty: none)", config.user.notes, {}});
    return questions;
}

int cluster::machine_days_old(void)
{
    std::error_code error;
    if (!std::filesystem::exists(cluster::machine_file(), error)) return -1;
    const std::filesystem::file_time_type written = std::filesystem::last_write_time(cluster::machine_file(), error);
    return static_cast<int>(std::chrono::duration_cast<std::chrono::hours>(std::filesystem::file_time_type::clock::now() - written).count() / 24);
}

_cold std::string cluster::machine_age(void)
{
    std::error_code error;
    if (!std::filesystem::exists(cluster::machine_file(), error)) return "";
    return date_(std::filesystem::last_write_time(cluster::machine_file(), error));
}

_cold std::size_t cluster::machine_scan(const cluster::Config& config, const std::function<void(const cluster::MachineProgress&)>& report)
{
    // the steps, each one reported (its share of the bar: the repositories are the long one)
    static const std::vector<double> weights = {0.03, 0.1, 0.5, 0.05, 0.12, 0.1, 0.05, 0.05};
    cluster::MachineProgress progress;
    progress.steps = {{"System", 0}, {"Shell", 0}, {"Git repositories", 0}, {"Hardware", 0}, {"Tools", 0}, {"Packages", 0}, {"Development", 0}, {"machine.md", 0}};
    const std::function<double(std::size_t)> base = [](std::size_t step) {
        double sum = 0.0;
        for (std::size_t i = 0; i < step; ++i)
            sum += weights[i];
        return sum;
    };
    const std::function<void(std::size_t, const std::string&)> begin = [&](std::size_t step, const std::string& now) {
        progress.steps[step].second = 1;
        progress.now = now;
        progress.next = step + 1 < progress.steps.size() ? progress.steps[step + 1].first : "";
        progress.fraction = base(step);
        if (report) report(progress);
    };
    const std::function<void(std::size_t, const std::string&)> finish = [&](std::size_t step, const std::string& line) {
        progress.steps[step].second = 2;
        progress.log.push_back(line);
        ++progress.finished;
        if (progress.log.size() > 3) progress.log.erase(progress.log.begin());
        progress.fraction = base(step + 1);
        if (report) report(progress);
    };

    // the slow commands (lspci, rpm, pip, npm, the versions...) in the background while the shell and the home are read
    std::size_t found = 0;
    std::future<std::string> hardwareJob = std::async(std::launch::async, hardware_);
    std::future<std::string> toolsJob = std::async(std::launch::async, [&found]() {return tools_(found);});
    std::future<std::string> packagesJob = std::async(std::launch::async, packages_);
    std::future<std::string> developmentJob = std::async(std::launch::async, development_);

    begin(0, "reading /etc/os-release, uname");
    std::string os;
    std::istringstream release(read_("/etc/os-release"));
    for (std::string line; std::getline(release, line);)
        if (line.starts_with("PRETTY_NAME=")) os = cluster::trim(line.substr(12));
    if (os.size() >= 2 && os.front() == '"') os = os.substr(1, os.size() - 2);
    const std::string kernel = cluster::trim(cluster::capture({"uname", "-srm"}, "", 5).out);
    finish(0, "system: " + (os.empty() ? std::string("?") : os));

    const std::string shell = env_("SHELL").empty() ? std::string("sh") : env_("SHELL");
    begin(1, "starting " + shell + " once: its aliases, functions, PATH");
    const std::pair<std::string, std::vector<std::string>> shellOut = shell_once_(shell);
    std::size_t aliasCount = 0;
    std::size_t beware = 0;
    const std::string aliases = aliases_(shell, shellOut.first, aliasCount, beware);
    const std::string shellMore = shell_more_(shellOut.second);
    finish(1, std::to_string(aliasCount) + " aliases read (" + std::to_string(beware) + " to beware)");

    begin(2, "the folders of the home folder");
    const std::vector<Repository> repos = repositories_([&](double share, const std::string& folder, std::size_t count) {
        progress.fraction = base(2) + weights[2] * share;
        progress.now = folder.empty() ? "sorting" : folder + " (" + std::to_string(count) + " repositories so far)";
        if (report) report(progress);
    });
    std::vector<std::string> remotes;
    std::vector<std::string> paths;
    for (const Repository& r: repos) {
        remotes.push_back(r.remote);
        paths.push_back(r.path);
    }
    finish(2, std::to_string(repos.size()) + " git repositories found");

    begin(3, "CPU, memory, GPU, disks");
    const std::string hardware = hardwareJob.get();
    finish(3, "hardware read");
    begin(4, "the tools of the PATH and their versions");
    const std::string tools = toolsJob.get();
    finish(4, std::to_string(found) + " tools found");
    begin(5, "rpm / dpkg, flatpak, pip --user, npm -g, cargo");
    const std::string packages = packagesJob.get();
    finish(5, "packages listed");
    begin(6, "git / GitHub identity, Claude Code, containers, folders");
    std::string development = developmentJob.get();
    const std::size_t hostsAt = development.find("__GIT_HOSTS__");
    if (hostsAt != std::string::npos) development.replace(hostsAt, 13, hosts_(remotes));
    const std::string personal = personal_(paths);
    finish(6, "git, Claude Code, containers, folders read");
    begin(7, "writing " + cluster::short_path(cluster::machine_file().string()));

    std::ostringstream md;
    md << "# This computer (claude-cluster map, " << date_(std::filesystem::file_time_type::clock::now()) << ")\n\n"
        << "Written by `claude-cluster machine sync` (settings page: Global session > Map of this computer). Local only.\n\n"
        << "## System\n\n"
        << "| | |\n|---|---|\n"
        << "| OS | " << (os.empty() ? "?" : os) << " |\n"
        << "| Kernel | " << kernel << " |\n"
        << "| User | `" << env_("USER") << "` (home `" << cluster::expand_home("~").string() << "`) |\n"
        << "| Desktop | " << env_("XDG_CURRENT_DESKTOP") << " (" << env_("XDG_SESSION_TYPE") << ") |\n"
        << "| Editor | " << editor_() << " |\n\n"
        << "## The user\n\n"
        << "| | |\n|---|---|\n"
        << "| Nickname | " << (config.user.nickname.empty() ? "(not told)" : config.user.nickname) << " |\n"
        << "| Email | " << (config.user.email.empty() ? "(not told)" : config.user.email) << " |\n"
        << "| Main editor | " << (config.user.editor.empty() ? "(not told)" : config.user.editor) << " |\n"
        << "| Language of the answers | " << (config.user.language.empty() ? "the one of each message" : config.user.language) << " |\n"
        << "| Notes | " << config.user.notes << " |\n\n"
        << "## Hardware\n\n" << hardware << "\n"
        << "## Tools\n\n";
    md << tools << "\n"
        << "## Shell\n\n" << aliases << "\n" << shellMore << "\n"
        << "## Packages\n\n" << packages << "\n"
        << "## Development\n\n" << development << "\n"
        << "## Folders, language, SSH\n\n" << personal << "\n"
        << "## Projects (git repositories of the home folder: " << repos.size() << ")\n\n"
        << "| Path | Kind | Branch | Origin | Last activity |\n|---|---|---|---|---|\n";
    for (const Repository& r: repos)
        md << "| `" << r.path << "` | " << r.kind << " | " << r.branch << " | " << r.remote << " | " << r.activity << " |\n";
    std::error_code error;
    std::filesystem::create_directories(cluster::config_dir(), error);
    std::ofstream(cluster::machine_file()) << md.str();
    finish(7, "written: " + cluster::short_path(cluster::machine_file().string()));
    progress.done = true;
    progress.repos = repos.size();
    progress.now.clear();
    progress.next.clear();
    if (report) report(progress);
    return repos.size();
}

_cold std::vector<cluster::Project> cluster::projects(const std::string& query)
{
    // the table "Projects" of machine.md (read again when it changed); a query: its name close first, its path / kind /
    // origin far behind (6 further), the closest first
    static std::mutex mutex;
    static std::filesystem::file_time_type seen{};
    static std::vector<cluster::Project> all;
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::error_code error;
        const std::filesystem::file_time_type written = std::filesystem::last_write_time(cluster::machine_file(), error);
        if (!error && written != seen) {
            seen = written;
            all.clear();
            std::ifstream in(cluster::machine_file());
            bool table = false;
            for (std::string line; std::getline(in, line);) {
                if (line.starts_with("## ")) table = line.starts_with("## Projects");
                if (!table || !line.starts_with("| `")) continue;
                std::vector<std::string> cells = {""}; // its empty ones kept (a repository without origin)
                for (const char c: line) {
                    if (c == '|') cells.emplace_back();
                    else cells.back() += c;
                }
                for (std::string& cell: cells)
                    cell = cluster::trim(cell);
                // "", path, kind, branch, origin, activity, ""
                if (cells.size() < 6) continue;
                cluster::Project p;
                p.path = cells[1].size() > 2 ? cells[1].substr(1, cells[1].size() - 2) : cells[1];
                p.name = std::filesystem::path(p.path).filename().string();
                p.kind = cells[2];
                p.branch = cells[3];
                p.origin = cells[4];
                p.activity = cells[5];
                all.push_back(p);
            }
        }
    }
    std::lock_guard<std::mutex> lock(mutex);
    if (cluster::trim(query).empty()) return all;
    std::vector<std::pair<float, cluster::Project>> found;
    for (cluster::Project p: all) {
        // its name: word by word, or whole without its separators (bteasite: beta-site)
        std::optional<float> name = cluster::match_words(query, p.name);
        std::string joined;
        for (const char c: p.name)
            if (std::isalnum(static_cast<unsigned char>(c))) joined += c;
        if (const std::optional<float> whole = cluster::match_score(cluster::trim(query), joined); whole && (!name || *whole < *name)) name = whole;
        const std::optional<float> about = cluster::match_words(query, p.path + " " + p.kind + " " + p.origin);
        if (!name && !about) continue;
        const float described = about ? *about + 6.0F : 1e9F;
        p.byName = name && *name <= described;
        found.emplace_back(name ? std::min(*name, described) : described, p);
    }
    std::stable_sort(found.begin(), found.end(), [](const std::pair<float, cluster::Project>& a, const std::pair<float, cluster::Project>& b) {return a.first < b.first;});
    std::vector<cluster::Project> out;
    for (const auto &[score, p]: found)
        out.push_back(p);
    return out;
}

_cold std::filesystem::path cluster::resolve_folder(const std::string& text, const std::filesystem::path& base)
{
    const std::string given = cluster::trim(text);
    std::error_code error;
    const std::filesystem::path direct = given.starts_with("/") || given.starts_with("~") || base.empty() ? cluster::expand_home(given.empty() ? "." : given)
        : base / given;
    if (std::filesystem::is_directory(direct, error)) return std::filesystem::weakly_canonical(direct, error);
    // a project of the map, by its name
    const std::vector<cluster::Project> found = cluster::projects(given);
    if (!given.empty() && !found.empty() && found.front().byName) return std::filesystem::weakly_canonical(cluster::expand_home(found.front().path), error);
    std::string near;
    for (std::size_t i = 0; i < found.size() && i < 3; ++i)
        near += (near.empty() ? "" : ", ") + found[i].name;
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "not a folder nor a project: " + given
        + (near.empty() ? std::string(" (the projects come from the map of this computer: settings > Global session)") : " (close: " + near + ")"));
}
