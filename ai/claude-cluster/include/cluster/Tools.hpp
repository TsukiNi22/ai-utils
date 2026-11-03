/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Tools.hpp

File Description:
##  Small helpers: paths, time, formatting, sub-processes, json
##  files, language detection
\**************************************************************/

#ifndef CLUSTER_TOOLS_H
    #define CLUSTER_TOOLS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Json
    #include <filesystem>           // std::filesystem::path
    #include <algorithm>            // std::min
    #include <functional>           // std::invoke
    #include <iosfwd>               // std::istream
    #include <optional>             // std::optional
    #include <exception>            // std::exception
    #include <thread>               // std::thread
    #include <csignal>              // std::signal, SIGHUP, SIG_IGN
    #include <cstdint>              // std::int64_t
    #include <atomic>               // std::atomic
    #include <chrono>               // std::chrono
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Captured {
    int code = -1;
    std::string out;
};

//----------------------------------------------------------------//
/* FUNCTION */

/* paths */
_cold _nodiscard std::filesystem::path expand_home(const std::string& path);
_cold _nodiscard std::filesystem::path config_dir(void);     // ~/.config/claude-cluster
_cold _nodiscard std::filesystem::path data_dir(void);       // ~/.local/share/claude-cluster
_cold _nodiscard std::filesystem::path runtime_dir(void);    // $XDG_RUNTIME_DIR/claude-cluster (or /tmp)
_cold _nodiscard std::filesystem::path socket_path(void);
_cold _nodiscard std::filesystem::path private_temp(const std::string& name); // a temporary file in a folder of this user alone
_cold void private_dir(const std::filesystem::path& dir);     // made, this user only (0700): the config, the data, the runtime folder
_cold _nodiscard bool hidden_path(const std::string& path);    // a hidden file / folder (~/.ssh, ~/.config...), the folders of claude-cluster: never given to an agent by the global session
// the MCP config of the global session, next to the socket: an instance of its own (CLAUDE_CLUSTER_SOCKET) never
// overwrites the one of another
_cold _nodiscard inline std::filesystem::path mcp_config_path(void) {return cluster::socket_path().parent_path() / "mcp.json";};
_cold _nodiscard std::filesystem::path self_exe(void);
_cold _nodiscard std::string build_id(void);    // its version + the date of its binary when it started ("1.3.0+1791467670")
_cold _nodiscard std::string short_path(const std::string& path); // ~ for the home

/* text */
_nodiscard std::vector<std::string> split(const std::string& text, const char sep);
_nodiscard std::string trim(const std::string& text);
_nodiscard std::string lower(std::string text);
_nodiscard std::string one_line(const std::string& text, const std::size_t max);
_nodiscard std::string slash_tag(const std::string& id);    // an id written in invisible tag characters (U+E0020-E007E, U+E007F at its end)
_nodiscard std::string slash_untag(const std::string& text, std::string* first = nullptr); // without them; first: the id after the first /word
_nodiscard std::string paste_keep(const std::string& text);    // every hidden id dropped but the marks of the pastes
_nodiscard std::string paste_reveal(const std::string& text);  // for the agent: each paste between <pasted> and </pasted>, nothing hidden left
_nodiscard std::string language_of(const std::string& text);      // "fr" | "en" | "es" | "it" | "de" | "pt" | "nl"
_nodiscard std::string summary_of(const std::string& text, const std::size_t max);
_nodiscard std::string clean_label(const std::string& text);                    // a category / a tag: lower case, one word (- and _ kept), 24 characters at most
_nodiscard std::vector<std::string> clean_tags(const std::vector<std::string>& tags);   // each cleaned, no duplicate, 8 at most
_nodiscard std::vector<std::string> split_tags(const std::string& text);        // "a, #b c": the words (commas and spaces), cleaned
_nodiscard std::string content_hash(const std::string& bytes);   // FNV-1a 64, hexadecimal (same content, same name)

/* format */
_nodiscard inline std::int64_t now(void) {return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();};
// a paste between its two invisible marks (tag characters): told to the agent only, the user sees the text alone
_nodiscard bool pasted_first(const std::string& text);    // it starts by what was pasted: a prompt, never a !command, a :command, a /command (a block hides what it starts with)
_nodiscard inline std::string paste_mark(const std::string& text) {return text.empty() ? text : slash_tag("paste{") + text + slash_tag("}paste");};
_nodiscard inline std::int64_t now_ms(void)                       {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();};
// seconds this computer was awake (a monotonic clock: a sleep / suspend never counts, unlike now())
_nodiscard inline std::int64_t awake_s(void) {return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();};
// a program run, each line of its output (and its errors) given as it comes; its exit code (-1: not run)
_cold int run_logged(const std::vector<std::string>& args, const std::function<void(const std::string&)>& line);
// the operation running in this thread (a Busy): its step now (the one before done), where this step is (its text, its percent), a line of its log; none: nothing
_cold void busy_step(const std::string& text);
_cold void busy_update(const std::string& text, const int percent = -1);
_cold void busy_log(const std::string& line);
// what fails in this thread: a popup of failure (an interface acting: true; a request of the control socket: its answer
// tells it, unless an interface sent it: "ui": true)
_cold _nodiscard bool& failure_popups(void);

class Manager;
// A user action taking time: its progress popup (shown after 0.4 s) while it runs in this thread, its steps told by
// busy_step; inside another one: a step of it
class Busy {
    private:
        cluster::Manager& _manager;
        std::uint64_t _id = 0;
        bool _nested = false;
        bool _closed = false;

    public:
        Busy(cluster::Manager& manager, const std::string& title, const std::string& session = "");
        ~Busy();
        Busy(const Busy&) = delete;
        Busy& operator=(const Busy&) = delete;
        void end(const bool ok, const std::string& text = "");    // done (its result a moment) / failed (its failure popup instead)
        void drop(void);
};
_nodiscard std::string tail_utf8(const std::string& text, const std::size_t max); // its last max bytes (whole characters, "…" before)
_nodiscard std::string size_text(const std::uint64_t bytes);                      // 812 B, 34 KB, 3.2 MB
_nodiscard std::string head_utf8(const std::string& text, const std::size_t max); // its first max bytes (whole characters, "[...]" after)
_nodiscard std::string clean_utf8(const std::string& text);    // valid UTF-8 only (a broken byte: U+FFFD), C0 / C1 control characters out (\n \t kept)
_nodiscard std::string read_small(const std::string& path);    // a small file (/proc/<pid>/stat...), empty on any error (a process gone meanwhile)
_nodiscard std::string read_end(const std::string& path, const std::size_t size);   // the last bytes of a regular file of any size; empty on any error or for anything else
_nodiscard std::string model_label(const std::string& backend, const std::string& modelUsed);   // provider/model loaded, '(default)' when none was chosen
_nodiscard std::string effort_label(const std::string& effort, const std::string& configured, const std::string& backend, const std::string& modelUsed);   // think <level>, '(default)' when it is the default one
void keep_awake(bool want);                                           // want: a systemd idle inhibitor kept alive (renewed before it ends); not: it ends by itself
_nodiscard std::string claude_default_model(void);                    // the model Claude Code starts on (its settings.json, an alias resolved by the models used before)
_nodiscard std::string claude_default_effort(const std::string& model);   // its thinking level for a model (settings.json modelSettings), empty: unknown
_nodiscard std::string human_tokens(const std::int64_t tokens);   // 12.3k, 1.2M
_nodiscard std::string human_cost(const double usd);              // $0.0123
_nodiscard std::string human_age(const std::int64_t time);        // 3m, 2h, 5d
_nodiscard std::string left_time(const std::int64_t at);          // what is left until then, its two biggest units: 3d5h, 4h40m, 40m12s, 12s; "" past or not known
_nodiscard std::string clock_time(const std::int64_t time);       // 14:03

/* processes */
_cold _nodiscard bool has_command(const std::string& name);
_cold _nodiscard cluster::Captured capture(const std::vector<std::string>& args, const std::string& cwd = "", const int timeout = 10, std::atomic<int>* pidOut = nullptr);
// a big output read whole (a clipboard): its size told as it comes (10 times a second at most), stopped past limit bytes (its program killed, code -2)
_cold _nodiscard cluster::Captured capture_big(const std::vector<std::string>& args, const int timeout, const std::size_t limit, const std::function<void(std::size_t)>& told);
_cold _nodiscard cluster::Captured capture_env(const std::vector<std::string>& args, const std::vector<std::pair<std::string, std::string>>& env, const std::string& cwd = "", const int timeout = 10); // with these variables, never on its command line // pidOut: its process group (kill(-pid))
/* command triggers (task on_event command, schedule if.command) */
enum class CommandVerdict {Met, NotYet, Failed, GaveUp};
_cold _nodiscard cluster::CommandMatch command_match(const cluster::Json& options);          // stdout / contains / regex / code / every / fail_after (throws on a bad one)
_cold _nodiscard cluster::CommandVerdict command_check(const std::string& command, cluster::CommandMatch& match); // run once: met, not yet, failed, given up
_cold _nodiscard cluster::Json command_match_json(const cluster::CommandMatch& match);    // its options as given, and its state (failures, last)
_cold _nodiscard std::string command_label(const std::string& text);  // dangerous (it changes things outside: push, deploy, remove, sudo...) | safe
// a regex built from what someone typed or sent (:s, the output wanted from a command): 512 characters and 64 levels of parentheses at most (a utils exception),
// a wrong one or too much work on this text: std::regex_error; never a stack gone in its compiler, never a search that does not end
_cold void regex_check(const std::string& pattern);
_cold _nodiscard bool regex_found(const std::string& pattern, const std::string& text);
_cold _nodiscard std::string regex_changed(const std::string& pattern, const std::string& text, const std::string& format, const bool icase, const bool all);
_cold void stop_group(const int pid, const bool force);             // a command and everything it started (its process group): TERM, KILL (force)
_cold _nodiscard std::string interactive_program(const std::string& command);   // vim, less, top, ssh, a REPL...: the program that needs a terminal in a shell command ("" none)
_cold _nodiscard std::string open_terminal_command(const std::filesystem::path& folder, const std::string& command); // an emulator running it there: its name ("" none found)
_cold void detach(const std::vector<std::string>& args);            // run without waiting (notify-send...)
_cold bool copy_text(const std::string& text);                      // the clipboard: wl-copy, else xclip / xsel; false: none of them
_cold void end_captures(const int ms);                              // the quit: the commands still running stopped, waited for (ms at most)
_cold _nodiscard std::string host_name(void);                          // the name of this computer
_cold _nodiscard std::string git_branch(const std::filesystem::path& folder); // its branch (.git/HEAD up the tree), "" none
_cold void open_folder(const std::filesystem::path& folder);           // the file manager of the desktop on it
_cold _nodiscard std::string open_terminal(const std::filesystem::path& folder); // a terminal there: the one opened ("" none found)
inline std::atomic<bool> quit_signal{false}; // set by catch_quit_signals
_cold void complete_path(void);         // PATH + the folders of the user's tools (~/.local/bin...) it lacks
// A JSONL file kept to its newest lines: at most maxLines (0: any) and maxBytes (0: any); true when it was cut
_cold bool keep_newest(const std::filesystem::path& path, const std::size_t maxLines, const std::uintmax_t maxBytes);
// Debug mode ([debug] enabled / topics): lines of <data>/debug.log, the process named (core, terminal, window, web terminal)
_cold void debug_configure(const bool on, const std::vector<std::string>& topics); // set by the config (its reload too)
_cold void debug_role(const std::string& role);                                     // what this process is (core, terminal...)
_cold _nodiscard bool debug_on(const std::string& topic);                           // core, interfaces, render, voice, service, web, platform
// where a value is between two others, from 0 to 1: the numbers of a display are anything (1e308 - -1e308 is inf, inf / inf no number at all, an index out of it or a bar without end)
_nodiscard inline double between(const double value, const double low, const double high)
{
    const double at = (value / 2.0 - low / 2.0) / (high / 2.0 - low / 2.0);
    return at >= 0.0 ? std::min(at, 1.0) : 0.0;
}
// Json::parse with a limit of depth: nlohmann parses any depth, its copies and its dumps recurse (a line of 200 000 [, from an agent, a device, a file: the stack gone, the core ended by a
// SIGSEGV); too deep: its parse error, or discarded when exceptions are off, like any malformed text
_nodiscard cluster::Json json_parse(const std::string& text, std::nullptr_t callback = nullptr, const bool exceptions = true);
_nodiscard cluster::Json json_parse(std::istream& in, std::nullptr_t callback = nullptr, const bool exceptions = true);
_cold void thread_failed(const char* why);                                          // what ended a thread (guarded): in the debug log
// a thread whose exception is told and ends it alone (out of a std::thread: std::terminate, the core and its sessions gone)
template <typename F, typename... A>
_nodiscard std::thread guarded(F&& work, A&&... args)
{
    return std::thread([work = std::forward<F>(work), ...args = std::forward<A>(args)]() mutable {
        try {
            std::invoke(std::move(work), std::move(args)...);
        } catch (const std::exception& e) {
            cluster::thread_failed(e.what());
        } catch (...) {
            cluster::thread_failed("an unknown exception");
        }
    });
}
_cold void debug(const std::string& topic, const std::string& text);                // one line, only when its topic is on
_cold _nodiscard std::string platform_summary(void);                                // OS, session (wayland / x11), desktop, terminal
_cold _nodiscard inline std::filesystem::path debug_log_path(void) {return cluster::data_dir() / "debug.log";};
_cold inline void ignore_hangup(void)                              {(void)std::signal(SIGHUP, SIG_IGN);}; // the terminal closed leaves the process running (the core kept in background)
_cold void catch_quit_signals(void);    // SIGINT, SIGTERM, SIGHUP (terminal closed), SIGQUIT: quit_requested() (reset by exec)
_cold void set_program_args(const int argc, char** argv);   // the arguments of this run (an update runs them again)
_cold void upgrade_request(const bool restore);             // after the quit: the binary on disk run again, its arguments (+ --restore)
_cold _nodiscard bool upgrade_requested(void);
_cold void upgrade_exec(void);                              // the exec of it (main, after the quit); back only when it failed
_cold void restart_exec(const std::string& crashes);        // the watcher again from the binary on disk after a crash of its core (back: failed)
_cold _nodiscard int version_compare(const std::string& a, const std::string& b);    // "1.4.18" vs "1.4.17": > 0
_cold _nodiscard std::string upgrade_selftest(const std::filesystem::path& binary);   // the binary started alone (own folders, no global session): empty when it runs, else why not
_cold _nodiscard std::string binary_version(const std::filesystem::path& binary);   // its --version ("1.4.18"); empty: not one
_nodiscard inline bool quit_requested(void) {return quit_signal;}; // a signal asked to quit: the interface closes as with its quit key

/* json files */
_cold _nodiscard std::optional<cluster::Json> read_json(const std::filesystem::path& path);
_cold void write_json(const std::filesystem::path& path, const cluster::Json& data); // atomic (tmp + rename), this user only
_cold void write_text(const std::filesystem::path& path, const std::string& text);   // atomic too, the mode of the file it replaces kept

} // namespace end

#endif /* CLUSTER_TOOLS_H */
