/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Prompt.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Settings.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Editor.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>
#include <fstream>
#include <thread>
#include <regex>
#include <set>

/* the commands of the shell (completion of !): the ones of the user's shell (its aliases, functions, builtins), read once */
static std::mutex shell_mutex_;
static std::vector<cluster::Candidate> shell_commands_;
static bool shell_read_ = false;

_cold static std::vector<cluster::Candidate> shell_scan_(void)
{
    // the shell of the user asked (zsh: commands, aliases, functions, builtins; bash: compgen), else the PATH
    std::vector<cluster::Candidate> found;
    std::set<std::string> seen;
    const char* env = std::getenv("SHELL");
    const std::string shell = env != nullptr ? env : "/bin/sh";
    const std::string name = std::filesystem::path(shell).filename().string();
    std::string listing;
    if (name == "zsh") {
        listing = cluster::capture({shell, "-ic", "for k in ${(k)aliases}; do print -r -- \"$k\talias\"; done; for k in ${(k)functions}; do print -r -- \"$k\tfunction\"; "
            "done; for k in ${(k)builtins}; do print -r -- \"$k\tbuiltin\"; done; for k in ${(k)commands}; do print -r -- \"$k\tcommand\"; done"}, "", 15).out;
    } else if (name == "bash") {
        listing = cluster::capture({shell, "-ic", "compgen -A alias | sed 's/$/\talias/'; compgen -A function | sed 's/$/\tfunction/'; "
            "compgen -b | sed 's/$/\tbuiltin/'; compgen -c | sed 's/$/\tcommand/'"}, "", 15).out;
    }
    for (const std::string& line: cluster::split(listing, '\n')) {
        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos || tab == 0) continue;
        const std::string command = line.substr(0, tab);
        if (command.find_first_of(" /:=") != std::string::npos || !seen.insert(command).second) continue;
        found.push_back({command, command, cluster::trim(line.substr(tab + 1)), false});
    }
    if (found.empty()) { // no shell answer: the programs of the PATH
        const char* path = std::getenv("PATH");
        std::error_code error;
        for (const std::string& dir: cluster::split(path != nullptr ? path : "/usr/bin:/bin", ':'))
            for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(dir, error))
                if (entry.is_regular_file(error) && seen.insert(entry.path().filename().string()).second)
                    found.push_back({entry.path().filename().string(), entry.path().filename().string(), "command", false});
    }
    std::sort(found.begin(), found.end(), [](const cluster::Candidate& a, const cluster::Candidate& b) {return a.value < b.value;});
    return found;
}

_cold std::vector<cluster::Candidate> cluster::shell_commands(const std::string& prefix)
{
    // read once in the background (a shell started: ~1 s), the PATH meanwhile
    {
        std::lock_guard<std::mutex> lock(shell_mutex_);
        if (!shell_read_) {
            shell_read_ = true;
            std::thread([]() {
                std::vector<cluster::Candidate> all = shell_scan_();
                std::lock_guard<std::mutex> hold(shell_mutex_);
                shell_commands_ = std::move(all);
            }).detach();
        }
    }
    std::vector<cluster::Candidate> items;
    std::lock_guard<std::mutex> lock(shell_mutex_);
    for (const cluster::Candidate& c: shell_commands_) {
        if (!c.value.starts_with(prefix)) continue;
        items.push_back(c);
        if (items.size() >= 80) break;
    }
    return items;
}

/* the :commands (like vim / nvim) */
_cold const std::vector<std::pair<std::string, std::string>>& cluster::ex_commands(void)
{
    static const std::vector<std::pair<std::string, std::string>> list = {
        {"s/", "s/old/new/[g][i]: your last prompt changed (a regex), back in the prompt to send"},
        {"!", "!<command>: run in the shell, in the folder of the session (= !<command>)"},
        {"r ", "r <file> | r !<command>: the file / the output of the command in the prompt"},
        {"w ", "w [file]: the conversation to Markdown (~/<name>.md)"},
        {"e ", "e <folder>: the session in another folder (= /cd)"},
        {"q", "q: close the session (to the trash)"},
        {"qa", "qa: quit claude-cluster"},
        {"set ", "set <key>=<value>: a setting (:set layout=grid, :set ui.notify=false)"},
        {"g/", "g/<text>/: search the conversation"},
        {"new ", "new <folder> [backend]: a new session (= /new)"},
        {"model ", "model [model]: the model of the session (= /model)"},
        {"mode ", "mode [mode]: its permission mode (= /mode)"},
        {"effort ", "effort [level]: its thinking effort (= /effort)"},
        {"clear", "clear: a new conversation (= /clear)"},
        {"help", "help: these commands"},
    };
    return list;
}

_cold static cluster::ActionResult substitute_(cluster::Manager& manager, const std::string& session, const std::string& command)
{
    // s/old/new/flags (any delimiter after s), on the last prompt of the session; the result back in the prompt
    cluster::ActionResult result;
    const std::size_t s = command.starts_with("%") ? 1 : 0;
    if (command.size() < s + 2) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, ":s/old/new/[g]");
    const char delimiter = command[s + 1];
    std::vector<std::string> parts = {""};
    for (std::size_t i = s + 2; i < command.size(); ++i) {
        if (command[i] == '\\' && i + 1 < command.size() && command[i + 1] == delimiter) {
            parts.back() += delimiter;
            ++i;
        } else if (command[i] == delimiter) {
            parts.emplace_back();
        } else {
            parts.back() += command[i];
        }
    }
    if (parts.size() < 2) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, ":s/old/new/[g]: its new text missing");
    const std::string flags = parts.size() > 2 ? parts[2] : "";
    // the last prompt of its conversation (sent from here, the web, the global session, the MCP)
    std::vector<std::string> history;
    if (const std::optional<cluster::Snapshot> snap = manager.snapshot(session))
        for (auto it = snap->entries.rbegin(); it != snap->entries.rend() && history.empty(); ++it)
            if (it->kind == cluster::EntryKind::User) history.push_back(it->text);
    if (history.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidAction, "no prompt sent in this session yet");
    const std::regex pattern(parts[0], flags.find('i') != std::string::npos ? std::regex::ECMAScript | std::regex::icase : std::regex::ECMAScript);
    const std::string changed = std::regex_replace(history.front(), pattern, parts[1],
        flags.find('g') != std::string::npos || s == 1 ? std::regex_constants::format_default : std::regex_constants::format_first_only);
    if (changed == history.front()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "nothing matches " + parts[0] + " in your last prompt");
    result.fill = changed;
    result.ui = "fill";
    result.message = "your last prompt changed: Enter sends it";
    return result;
}

_cold static cluster::ActionResult ex_(cluster::Manager& manager, const std::string& session, const std::string& command)
{
    // a :command: its prompt, the conversation, the session, a setting, claude-cluster
    cluster::ActionResult result;
    const std::size_t space = command.find(' ');
    const std::string name = command.substr(0, space);
    const std::string arg = space == std::string::npos ? "" : cluster::trim(command.substr(space + 1));
    const std::function<cluster::ActionResult(const std::string&)> slash = [&manager, &session](const std::string& line) {
        const std::optional<cluster::ActionResult> done = cluster::run_slash(manager, session, line);
        return done ? *done : cluster::ActionResult{"", false, "", "", ""};
    };
    const std::optional<cluster::Snapshot> snap = manager.snapshot(session);
    const std::string cwd = snap ? cluster::expand_home(snap->spec.cwd).string() : cluster::expand_home("~").string();
    if ((name.starts_with("s") || name.starts_with("%s")) && command.size() > (name.starts_with("%") ? 2 : 1)
        && !std::isalnum(static_cast<unsigned char>(command[name.starts_with("%") ? 2 : 1]))) return substitute_(manager, session, command);
    if (command.starts_with("!")) return cluster::run_shell(manager, session, cluster::trim(command.substr(1)));
    if (name == "r" || name == "read") {
        if (arg.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, ":r <file> | :r !<command>");
        if (arg.starts_with("!")) {
            const char* env = std::getenv("SHELL");
            result.fill = cluster::trim(cluster::capture({env != nullptr ? env : "/bin/sh", "-c", arg.substr(1)}, cwd, 120).out);
        } else {
            const std::filesystem::path file = arg.starts_with("/") || arg.starts_with("~") ? cluster::expand_home(arg) : std::filesystem::path(cwd) / arg;
            std::ifstream in(file, std::ios::binary);
            if (!in) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no file " + file.string());
            result.fill.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            if (result.fill.size() > 200000) result.fill.resize(200000);
        }
        result.ui = "fill";
        result.message = std::to_string(std::count(result.fill.begin(), result.fill.end(), '\n') + 1) + " line(s) in the prompt";
        return result;
    }
    if (name == "w" || name == "write") return slash("/export" + (arg.empty() ? "" : " " + arg));
    if (name == "e" || name == "edit" || name == "cd") return slash("/cd " + arg);
    if (name == "q" || name == "q!" || name == "close") return slash("/close");
    if (name == "qa" || name == "qa!" || name == "quit" || name == "wqa") return {"", false, "", "quit", ""};
    if (name == "g" || name.starts_with("g/")) {
        std::string pattern = command.substr(1);
        while (!pattern.empty() && pattern.front() == '/') pattern.erase(0, 1);
        while (!pattern.empty() && pattern.back() == '/') pattern.pop_back();
        return {"", false, "", "search:" + pattern, ""};
    }
    if (name == "set" || name == "se") {
        // key=value: a key of the settings, alone or with its section (ui.layout)
        const std::size_t equal = arg.find('=');
        if (equal == std::string::npos) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, ":set <key>=<value>");
        const std::string key = cluster::trim(arg.substr(0, equal));
        const std::string value = cluster::trim(arg.substr(equal + 1));
        std::vector<cluster::Setting> matches;
        for (const cluster::SettingsPage& page: cluster::settings_pages(manager.config()))
            for (const cluster::Setting& setting: page.settings)
                if (setting.key == key || setting.section + "." + setting.key == key) matches.push_back(setting);
        if (matches.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no setting " + key);
        if (matches.size() > 1) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, key + ": several settings, say its section (" +
            matches[0].section + "." + key + ")");
        if (const std::string why = cluster::setting_check(matches[0], value); !why.empty())
            throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, why);
        (void)cluster::settings_set(manager.config(), matches[0].section, matches[0].key, value);
        result.message = matches[0].section + "." + matches[0].key + " = " + value + (matches[0].restart ? " (at the next start)" : "");
        return result;
    }
    if (name == "h" || name == "help") {
        result.message = "the :commands:";
        for (const auto &[ex, description]: cluster::ex_commands())
            result.message += "\n  :" + description;
        return result;
    }
    // a /command of the same name (:new, :model, :mode, :effort, :clear...)
    const std::vector<std::pair<std::string, std::string>>& local = cluster::slash_commands();
    if (std::any_of(local.begin(), local.end(), [&name](const std::pair<std::string, std::string>& c) {return c.first == name;}))
        return slash("/" + command);
    throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "no :" + name + " (:help)");
}

_cold cluster::ActionResult cluster::run_shell(cluster::Manager& manager, const std::string& session, const std::string& command)
{
    // !command: run by the shell of the user in the folder of the session; its output in the conversation and given to the
    // agent with the next prompt (like Claude Code)
    if (command.empty()) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "!<command>");
    const cluster::Json done = manager.shell(session, command);
    cluster::ActionResult result;
    result.message = "exit " + std::to_string(done.value("code", -1)) + " · " + std::to_string(done.value("lines", 0)) + " line(s): in the conversation, "
        "given to the agent with your next prompt";
    if (!done.value("shown", true)) // no session: its output here
        result.message = "exit " + std::to_string(done.value("code", -1)) + (done.value("output", std::string()).empty() ? "" : "\n" + cluster::tail_utf8(done.value("output", std::string()), 4000));
    result.error = done.value("code", -1) != 0;
    return result;
}

_cold std::optional<cluster::ActionResult> cluster::run_prompt_mode(cluster::Manager& manager, const std::string& session, const std::string& line)
{
    // !<shell command>, :<command>; anything else: not for it
    const std::string text = cluster::trim(line);
    if (text.size() < 2 || (text[0] != '!' && text[0] != ':')) return std::nullopt;
    cluster::ActionResult result;
    try {
        result = text[0] == '!' ? cluster::run_shell(manager, session, cluster::trim(text.substr(1))) : ex_(manager, session, cluster::trim(text.substr(1)));
    } catch (const utils::exception::IException& e) {
        result = {std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info()), true, "", "", ""};
    } catch (const std::regex_error& e) {
        result = {std::string("a wrong regex: ") + e.what(), true, "", "", ""};
    }
    return result;
}
