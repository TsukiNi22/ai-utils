/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Core-Completion.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Control.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Auth.hpp"
#include "cluster/Core.hpp"
#include <functional>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <map>

/* tools */
_cold static std::string replace_(std::string text, const std::string& from, const std::string& to)
{
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
    return text;
}

_cold static std::string zsh_desc_(const std::string& text)
{
    // inside '...[desc]': no single quote, brackets escaped
    return replace_(replace_(replace_(text, "'", "'\\''"), "[", "\\["), "]", "\\]");
}

_cold static std::string fish_desc_(const std::string& text)
{
    return replace_(replace_(text, "\\", "\\\\"), "'", "\\'");
}

_cold static std::vector<std::pair<std::string, std::string>> values_(const std::string& kind)
{
    // Fixed values with their description
    static const std::map<std::string, std::vector<std::pair<std::string, std::string>>> fixed = {
        {"layout", {{"list", "list + active panel"}, {"grid", "every session in a grid"}, {"tabs", "one tab per session"}}},
        {"behavior", {{"allow", "allow this time"}, {"always", "allow and keep the rule"}, {"deny", "refuse"}}},
        {"onoff", {{"on", "start"}, {"off", "stop"}}},
        {"task_sub", {{"add", "queue a task"}, {"list", "tasks and their status"}, {"cancel", "cancel a task"}}},
        {"auth_sub", {{"list", "every provider and its state"}, {"login", "log in / store an API key"}, {"logout", "log out / remove the key"}}},
        {"shell", {{"bash", "bash completion"}, {"zsh", "zsh completion"}, {"fish", "fish completion"}}},
    };
    auto it = fixed.find(kind);
    return it == fixed.end() ? std::vector<std::pair<std::string, std::string>>{} : it->second;
}

/* commands */
_cold const std::vector<cluster::Command>& cluster::commands(void)
{
    static const std::vector<cluster::Command> list = {
        {"list", "", "sessions: state, backend, tokens, context, cost, permissions", {}},
        {"status", "<s>", "details and the last messages of a session (id, name or name prefix)", {"session"}},
        {"spawn", "[folder]", "new session (--name, --backend, --mode, --profile, --prompt)", {"folder"}},
        {"send", "<s> <text...>", "prompt to a session (queued when working, --force: ignore the budget)", {"session", "text"}},
        {"allow", "<s> [allow|always|deny]", "answer the pending permission", {"session", "behavior"}},
        {"mode", "<s> <mode>", "permission mode: auto | manual | acceptEdits | plan | dontAsk | bypassPermissions", {"session", "mode"}},
        {"interrupt", "<s>", "stop the running turn", {"session"}},
        {"rename", "<s> <name>", "rename a session", {"session", "text"}},
        {"cd", "<s> <folder>", "move a session to another project", {"session", "folder"}},
        {"close", "<s>", "close a session (to the trash)", {"session"}},
        {"trash", "", "closed sessions that can still be restored", {}},
        {"restore", "<s>", "restore a closed session with its conversation", {"trash"}},
        {"purge", "[s|*]", "delete for good from the trash (default: the expired ones, *: all)", {"trash"}},
        {"task", "add|list|cancel", "task queue (add: --target, --after t1,t2)", {"task_sub", "task"}},
        {"search", "<text>", "full-text search in every history", {"text"}},
        {"export", "<s> [file]", "conversation as Markdown", {"session", "file"}},
        {"remote", "on|off", "Remote Control of the global session", {"onoff"}},
        {"providers", "", "backends and their state", {}},
        {"notices", "", "last events", {}},
        {"auth", "list|login|logout <provider>", "providers and their credentials", {"auth_sub", "provider"}},
        {"serve", "", "run the cluster without interface (until Ctrl+C)", {}},
        {"mcp", "", "MCP server of the global session (started by it)", {}},
        {"help", "", "this help", {}},
    };
    return list;
}

_cold std::string cluster::Core::flagKind_(const std::string& id) const
{
    static const std::map<std::string, std::string> kinds = {
        {"backend", "backend"}, {"mode", "mode"}, {"layout", "layout"}, {"style", "style"}, {"profile", "profile"},
        {"completion", "shell"}, {"target", "session"}, {"after", "task"}, {"name", "text"}, {"prompt", "text"},
    };
    auto it = kinds.find(id);
    return it == kinds.end() ? "" : it->second;
}

/* values: __complete <kind> -> "value<TAB>description" lines */
_cold void cluster::Core::complete_(const std::string& kind) const
{
    std::vector<std::pair<std::string, std::string>> out = kind == "mode" ? cluster::permission_modes() : values_(kind);
    const std::function<cluster::Json(const std::string&)> ask = [](const std::string& cmd) -> cluster::Json { // the running instance, nothing when there is none
        try {
            const cluster::Json response = cluster::Control::request({{"cmd", cmd}});
            if (response.value("ok", false)) return response.value("data", cluster::Json::array());
        } catch (const utils::exception::IException&) {}
        return cluster::Json::array();
    };

    if (kind == "command") {
        for (const cluster::Command& command: cluster::commands())
            out.emplace_back(command.name, command.description);
    } else if (kind == "session") {
        if (cluster::Control::alive())
            for (const cluster::Json& s: ask("list"))
                out.emplace_back(s.value("name", ""), s.value("id", "") + " " + s.value("state", "") + " " + s.value("backend", "") + " " + cluster::short_path(s.value("cwd", "")));
    } else if (kind == "trash") {
        if (cluster::Control::alive())
            for (const cluster::Json& s: ask("trash"))
                out.emplace_back(s.value("name", ""), "closed " + cluster::human_age(s.value("deleted", std::int64_t{0})) + " ago, " + cluster::short_path(s.value("cwd", "")));
    } else if (kind == "task") {
        if (cluster::Control::alive())
            for (const cluster::Json& t: ask("task_list"))
                out.emplace_back(t.value("id", ""), t.value("status", "") + ": " + cluster::one_line(t.value("prompt", ""), 60));
    } else if (kind == "provider" || kind == "backend" || kind == "profile" || kind == "style") {
        cluster::Config config;
        config.load();
        if (kind == "profile") {
            for (const auto &[name, profile]: config.profiles)
                out.emplace_back(name, cluster::short_path(profile.cwd) + (profile.backend.empty() ? "" : " · " + profile.backend));
        } else if (kind == "style") {
            for (const auto &[name, style]: config.styles)
                out.emplace_back(name, style.dark ? "dark" : "light");
        } else {
            const cluster::Auth auth(config);
            for (const cluster::Provider& p: auth.providers())
                out.emplace_back(p.name, p.driver + ": " + p.description);
        }
        if (kind == "backend") {
            // the models known on this computer: Claude aliases, local Ollama models, opencode models
            for (const char* model: {"opus", "sonnet", "haiku"})
                out.emplace_back(std::string("claude/") + model, std::string("Claude Code, model ") + model);
            if (cluster::has_command("ollama")) {
                const std::vector<std::string> lines = cluster::split(cluster::capture({"ollama", "list"}, "", 3).out, '\n');
                for (std::size_t i = 1; i < lines.size(); ++i) {
                    const std::vector<std::string> fields = cluster::split(lines[i], ' ');
                    if (fields.empty()) continue;
                    out.emplace_back("ollama/" + fields[0], "local model through Claude Code");
                    out.emplace_back("ollama-openai/" + fields[0], "local model through Qwen Code");
                }
            }
            if (cluster::has_command(config.command("opencode"))) {
                const std::vector<std::string> lines = cluster::split(cluster::capture({config.command("opencode"), "models"}, "", 5).out, '\n');
                for (std::size_t i = 0; i < lines.size() && i < 80; ++i)
                    if (lines[i].find('/') != std::string::npos && lines[i].find(' ') == std::string::npos)
                        out.emplace_back("opencode/" + lines[i], "opencode");
            }
        }
    }
    for (const auto &[value, description]: out)
        std::cout << value << "\t" << cluster::one_line(description, 90) << "\n";
}

/* scripts */
_cold void cluster::Core::completion_(void) const
{
    // Flags (from the parser, with their description and the kind of their value) and commands (table above)
    struct Flag {std::string shortName; std::string longName; std::string description; std::string kind; bool value = false;};
    std::vector<Flag> flags = {{"h", "help", "Display the help and exit", "", false}};
    for (const auto &[id, flag]: this->_parser.getFlags()) {
        const auto &[shortName, flagName, longName, env] = flag.flag;
        flags.push_back({shortName, longName, flag.description, this->flagKind_(id), !flag.options.empty()});
    }
    std::sort(flags.begin() + 1, flags.end(), [](const Flag& a, const Flag& b) {return a.longName < b.longName;});
    const std::vector<cluster::Command>& commands = cluster::commands();

    if (this->_completion == "bash") {
        std::string names;
        std::string valued;
        std::ostringstream cases;
        for (const Flag& f: flags) {
            names += (names.empty() ? "" : " ") + std::string(f.shortName.empty() ? "" : "-" + f.shortName + " ") + "--" + f.longName;
            if (!f.value) continue;
            const std::string pattern = (f.shortName.empty() ? "" : "-" + f.shortName + "|") + "--" + f.longName;
            valued += (valued.empty() ? "" : "|") + pattern;
            cases << "        " << pattern << ") kind=" << (f.kind.empty() ? "text" : f.kind) << " ;;\n";
        }
        std::string commandNames;
        std::ostringstream positions;
        for (const cluster::Command& c: commands) {
            commandNames += (commandNames.empty() ? "" : " ") + c.name;
            for (std::size_t i = 0; i < c.args.size(); ++i)
                positions << "            " << c.name << ":" << i << ") kind=" << c.args[i] << " ;;\n";
        }
        std::cout << "# claude-cluster completion (bash) - generated by: claude-cluster --completion bash\n\n"
            << "_claude_cluster_values()\n{\n    claude-cluster __complete \"$1\" 2> /dev/null | cut -f1\n}\n\n"
            << "_claude_cluster()\n{\n"
            << "    local cur prev words cword kind=\"\" cmd=\"\" sub=\"\" pos=0 i\n"
            << "    if declare -F _get_comp_words_by_ref > /dev/null; then\n"
            << "        _get_comp_words_by_ref -n : cur prev words cword # keeps ollama/model:tag as one word\n"
            << "    else\n"
            << "        cur=\"${COMP_WORDS[COMP_CWORD]}\"; prev=\"${COMP_WORDS[COMP_CWORD-1]}\"; words=(\"${COMP_WORDS[@]}\"); cword=$COMP_CWORD\n"
            << "    fi\n"
            << "    COMPREPLY=()\n"
            << "    case \"$prev\" in\n" << cases.str() << "    esac\n"
            << "    if [[ -z $kind ]]; then\n"
            << "        if [[ $cur == -* ]]; then\n"
            << "            COMPREPLY=($(compgen -W \"" << names << "\" -- \"$cur\"))\n"
            << "            return\n"
            << "        fi\n"
            << "        for ((i = 1; i < cword; i++)); do # command and position, the flags (and their value) skipped\n"
            << "            case \"${words[i]}\" in\n"
            << "                " << valued << ") ((i++)) ;;\n"
            << "                -*) ;;\n"
            << "                *) if [[ -z $cmd ]]; then cmd=\"${words[i]}\"; else [[ $pos -eq 0 ]] && sub=\"${words[i]}\"; ((pos++)); fi ;;\n"
            << "            esac\n"
            << "        done\n"
            << "        if [[ -z $cmd ]]; then\n"
            << "            COMPREPLY=($(compgen -W \"" << commandNames << "\" -- \"$cur\"))\n"
            << "            return\n"
            << "        fi\n"
            << "        case \"$cmd:$pos\" in\n"
            << "            task:1) [[ $sub == cancel ]] && kind=task ;;\n"
            << "            auth:1) [[ $sub == login || $sub == logout ]] && kind=provider ;;\n"
            << positions.str()
            << "        esac\n"
            << "    fi\n"
            << "    case \"$kind\" in\n"
            << "        \"\"|text) ;;\n"
            << "        folder) COMPREPLY=($(compgen -d -- \"$cur\")) ;;\n"
            << "        file) COMPREPLY=($(compgen -f -- \"$cur\")) ;;\n"
            << "        *) COMPREPLY=($(compgen -W \"$(_claude_cluster_values \"$kind\")\" -- \"$cur\")) ;;\n"
            << "    esac\n"
            << "    if declare -F __ltrim_colon_completions > /dev/null; then __ltrim_colon_completions \"$cur\"; fi\n"
            << "}\n\n"
            << "complete -F _claude_cluster claude-cluster\n";
    } else if (this->_completion == "zsh") {
        std::cout << "#compdef claude-cluster\n# claude-cluster completion (zsh) - generated by: claude-cluster --completion zsh\n\n"
            << "_claude_cluster_values()\n{\n"
            << "    # \"value<TAB>description\" lines of the running instance / config -> _describe\n"
            << "    local -a items\n    local line\n"
            << "    for line in ${(f)\"$(claude-cluster __complete $1 2> /dev/null)\"}; do\n"
            << "        items+=(\"${${line%%$'\\t'*}//:/\\\\:}:${line#*$'\\t'}\")\n"
            << "    done\n"
            << "    _describe -t $1 $1 items\n"
            << "}\n\n"
            << "local curcontext=\"$curcontext\" state line\ntypeset -A opt_args\n\n"
            << "_arguments -s -S \\\n";
        for (const Flag& f: flags) {
            const std::string desc = "[" + zsh_desc_(f.description) + "]";
            std::string action;
            if (f.value) {
                if (f.kind == "folder") action = ":folder:_files -/";
                else if (f.kind.empty() || f.kind == "text") action = ":" + f.longName + ": ";
                else action = ":" + f.kind + ":_claude_cluster_values " + f.kind;
            }
            if (f.longName == "help") std::cout << "    '(- *)'{-h,--help}'" << desc << "' \\\n";
            else if (f.shortName.empty()) std::cout << "    '--" << f.longName << desc << action << "' \\\n";
            else std::cout << "    '(-" << f.shortName << " --" << f.longName << ")'{-" << f.shortName << ",--" << f.longName << "}'" << desc << action << "' \\\n";
        }
        std::cout << "    '1:command:->command' \\\n    '*::argument:->argument'\n\n"
            << "case $state in\n"
            << "    command) _claude_cluster_values command ;;\n"
            << "    argument)\n"
            << "        local kind=\"\"\n"
            << "        case \"$words[1]:$((CURRENT - 1))\" in\n"
            << "            task:2) [[ $words[2] == cancel ]] && kind=task ;;\n"
            << "            auth:2) [[ $words[2] == login || $words[2] == logout ]] && kind=provider ;;\n";
        for (const cluster::Command& c: commands)
            for (std::size_t i = 0; i < c.args.size(); ++i)
                if (!(c.name == "task" && i == 1) && !(c.name == "auth" && i == 1))
                    std::cout << "            " << c.name << ":" << i + 1 << ") kind=" << c.args[i] << " ;;\n";
        std::cout << "        esac\n"
            << "        case $kind in\n"
            << "            folder) _files -/ ;;\n"
            << "            file) _files ;;\n"
            << "            ''|text) ;;\n"
            << "            *) _claude_cluster_values $kind ;;\n"
            << "        esac ;;\n"
            << "esac\n";
    } else if (this->_completion == "fish") {
        std::cout << "# claude-cluster completion (fish) - generated by: claude-cluster --completion fish\n\n"
            << "function __claude_cluster_position --description 'number of the words after claude-cluster, flags excepted'\n"
            << "    set -l count 0\n"
            << "    for token in (commandline -opc)[2..-1]\n"
            << "        string match -q -- '-*' $token; or set count (math $count + 1)\n"
            << "    end\n"
            << "    echo $count\n"
            << "end\n\n"
            << "complete -c claude-cluster -f\n"
            << "complete -c claude-cluster -n '__fish_use_subcommand' -a '(claude-cluster __complete command 2> /dev/null)'\n";
        for (const Flag& f: flags) {
            std::cout << "complete -c claude-cluster" << (f.shortName.empty() ? "" : " -s " + f.shortName) << " -l " << f.longName
                << " -d '" << fish_desc_(f.description) << "'";
            if (f.value && f.kind == "folder") std::cout << " -x -a '(__fish_complete_directories)'";
            else if (f.value && !f.kind.empty() && f.kind != "text") std::cout << " -x -a '(claude-cluster __complete " << f.kind << " 2> /dev/null)'";
            else if (f.value) std::cout << " -x";
            std::cout << "\n";
        }
        for (const cluster::Command& c: commands) {
            for (std::size_t i = 0; i < c.args.size(); ++i) {
                const std::string& kind = c.args[i];
                if (kind == "text") continue;
                std::string condition = "__fish_seen_subcommand_from " + c.name + "; and test (__claude_cluster_position) -eq " + std::to_string(i + 1);
                std::string kindNow = kind;
                if (c.name == "task" && i == 1) condition += "; and __fish_seen_subcommand_from cancel";
                if (c.name == "auth" && i == 1) condition += "; and __fish_seen_subcommand_from login logout";
                std::cout << "complete -c claude-cluster -n '" << condition << "'";
                if (kindNow == "folder") std::cout << " -x -a '(__fish_complete_directories)'\n";
                else if (kindNow == "file") std::cout << " -F\n";
                else std::cout << " -x -a '(claude-cluster __complete " << kindNow << " 2> /dev/null)'\n";
            }
        }
    }
}
