/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Settings.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Orchestrator.hpp"
#include "cluster/VoiceEngines.hpp"
#include "cluster/Vocabulary.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Plugins.hpp"
#include "cluster/Display.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Method.hpp"
#include "cluster/Match.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Types.hpp"
#include "cluster/Auth.hpp"
#include "cluster/Mcp.hpp"
#include <toml++/toml.hpp>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <set>

/* tools */
_cold static std::string join_(const std::vector<std::string>& list)
{
    std::string text;
    for (const std::string& item: list)
        text += (text.empty() ? "" : ", ") + item;
    return text;
}

_cold static std::string number_(const double value)
{
    std::ostringstream out;
    out << std::setprecision(10) << value;
    return out.str();
}

_cold static std::string quote_(const std::string& text)
{
    std::string out = "\"";
    for (const char c: text) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

_cold static std::string encode_(const cluster::Setting& setting, const std::string& value)
{
    // The value shown in the page -> its TOML form
    switch (setting.kind) {
        case cluster::SettingKind::Bool: return value == "true" ? "true" : "false";
        case cluster::SettingKind::Int: return std::to_string(std::stol(value));
        case cluster::SettingKind::Float: {
            const std::string number = number_(std::stod(value));
            return number.find_first_of(".e") == std::string::npos ? number + ".0" : number;
        }
        case cluster::SettingKind::List: {
            std::string out = "[";
            for (const std::string& item: cluster::split(value, ','))
                out += (out.size() > 1 ? ", " : "") + quote_(item);
            return out + "]";
        }
        default: return quote_(value);
    }
}

_cold static std::size_t comment_start_(const std::string& line)
{
    // first # outside a string ("..." or '...'), npos when there is none
    char quote = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quote) {
            if (c == '\\' && quote == '"') ++i;
            else if (c == quote) quote = 0;
        } else if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '#') {
            return i;
        }
    }
    return std::string::npos;
}

/* pages */
_cold std::vector<cluster::SettingsPage> cluster::settings_pages(const cluster::Config& config)
{
    using cluster::SettingKind;
    std::vector<std::string> modes = {""};
    const std::vector<std::string> efforts = {"", "low", "medium", "high", "xhigh", "max", "minimal"}; // claude; codex / opencode: minimal...
    for (const auto &[mode, description]: cluster::permission_modes())
        modes.push_back(mode);
    std::vector<std::string> styles;
    for (const auto &[name, style]: config.styles)
        styles.push_back(name);
    const cluster::Config& c = config;

    std::vector<cluster::SettingsPage> pages = {
        {"ui", "Interface", {
            {"ui", "frontend", "Front-end", SettingKind::Choice, {"auto", "tty", "gui"}, "auto: terminal when in a terminal, else a window (--tty / --gui win); "
                "also what the wake phrase opens when claude-cluster is closed (auto: a window, a terminal on its failure)", true,
                [](const cluster::Config& cf) {return cf.frontend;}},
            {"ui", "terminal", "Terminal emulator", SettingKind::Text, {}, "the terminal opened when one is needed (the wake phrase with Front-end tty, or "
                "a window that failed), with its option to run a command (kitty, alacritty -e...); empty: the first installed", false,
                [](const cluster::Config& cf) {return cf.service.terminal;}},
            {"ui", "matching", "Matching of the lists", SettingKind::Choice, {"c2dmp", "raw"}, "the completions (/command, @file, !command, :command, "
                "models...), the palette and the search of the settings: c2dmp (c2dmp-hsm of libutils: typos and misplaced letters found, the "
                "closest first) or raw (what starts with the text typed, then what contains it)", false,
                [](const cluster::Config& cf) {return cf.matching;},
                [](const std::string& v) {return v == "c2dmp" ? std::string("fuzzy, ranked by distance (\"modle\" finds model)") : std::string("a prefix, then contained");}},
            {"ui", "shell_context", "!command given to the agent", SettingKind::Choice, {"ask", "always", "never"}, "a !command typed in the prompt: its "
                "command and its output given to the agent of the session with your next prompt (like Claude Code); ask: chosen after it ran "
                "(this time, always, not this time, never: always / never are written here, ask again from here)", false,
                [](const cluster::Config& cf) {return cf.shellContext;},
                [](const std::string& v) {return v == "ask" ? std::string("asked after each !command") : v == "always" ? std::string("always given, never asked")
                    : std::string("never given (only shown in the conversation)");}},
            {"ui", "shared_core", "Several terminals / windows at once", SettingKind::Bool, {}, "claude-cluster opened again: attached to the one running, every "
                "terminal and window in sync (the core of the sessions in a process of its own, until the last one closes); off: opened again is refused", true,
                [](const cluster::Config& cf) {return cf.sharedCore ? std::string("true") : std::string("false");}},
            {"ui", "quit_background", "Quit with sessions running", SettingKind::Choice, {"ask", "always", "never"}, "the last terminal / window closed while a "
                "session works (or has prompts queued):\n• ask: a popup - keep them running in background (the core goes on without interface, its web access closed, "
                "stopped by itself once every session is idle; claude-cluster opened again meanwhile shows them) / stop everything / back\n• always: kept without asking\n"
                "• never: stopped (the turns interrupted, the sessions kept for the next start)", false,
                [](const cluster::Config& cf) {return cf.quitBackground;}},
            {"ui", "gui_mode", "Window mode", SettingKind::Choice, {"gui", "tty"}, "gui: the window and its own interface; tty: the window shows the terminal "
                "interface (a real terminal inside it, every key of it; -g --tty-mode for one run)", false,
                [](const cluster::Config& cf) {return cf.guiMode;}},
            {"ui", "layout", "Layout", SettingKind::Choice, {"list", "grid", "tabs", "hub"}, "list + active panel, every session in a grid, tabs, or the hub "
                "(tabs and the hub: the window and the web page only - the hub is a graph of the sessions around the global one; the terminal shows the list)", false,
                [](const cluster::Config& cf) {return cf.layout;}},
            {"ui", "style", "Style", SettingKind::Choice, styles, "colors of the interface (own ones: [styles.<name>] in the file)", false,
                [](const cluster::Config& cf) {return cf.style;}},
            {"ui", "prompt_origin", "Prompts coloured by origin", SettingKind::Bool, {}, "the prompts of a transcript by where they come from: you, mcp (the global session, replies, "
                "answers), task (tasks and schedules); with a label in the header. Off: all prompts as before", false,
                [](const cluster::Config& cf) {return cf.promptOrigin ? std::string("true") : std::string("false");}},
            {"ui", "prompt_color_user", "Color of your prompts", SettingKind::Text, {}, "#rrggbb, empty: the accent of the style (as before)", false,
                [](const cluster::Config& cf) {return cf.promptColorUser;}},
            {"ui", "prompt_color_mcp", "Color of the mcp prompts", SettingKind::Text, {}, "#rrggbb, empty: violet (#8a3fb5 light, #bb8cf2 dark)", false,
                [](const cluster::Config& cf) {return cf.promptColorMcp;}},
            {"ui", "prompt_color_task", "Color of the task prompts", SettingKind::Text, {}, "#rrggbb, empty: magenta (#b3267a light, #f08ccb dark); tasks and schedules", false,
                [](const cluster::Config& cf) {return cf.promptColorTask;}},
            {"ui", "panels", "Panels of a new session", SettingKind::List, {}, "skills, tokens, context, cost, git, diff, tools, files", false,
                [](const cluster::Config& cf) {return join_(cf.panels);}},
            {"ui", "restore", "Previous sessions at start", SettingKind::Choice, {"ask", "always", "never"}, "ask at every start (default, like Firefox: r restore, n new, q quit), always restore, never (to the trash)", true,
                [](const cluster::Config& cf) {return cf.restore;}},
            {"ui", "vim", "Vim mode of the prompt", SettingKind::Bool, {}, "Esc: Normal mode (hjkl w b e 0 $ x dd dw cw yy p u v V...), i / a / o: Insert", false,
                [](const cluster::Config& cf) {return cf.vim ? std::string("true") : std::string("false");}},
            {"ui", "group_by_category", "Group the session list by category", SettingKind::Bool, {}, "the sessions under the category the global session or you gave them (terminal, window, web page; the palette: Group by category)", false,
                [](const cluster::Config& cf) {return cf.groupByCategory ? std::string("true") : std::string("false");}},
            {"ui", "paste_lines", "Paste collapsed above (lines)", SettingKind::Int, {}, "a longer paste is one [Pasted text #N +L lines] in the prompt (0: never)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.pasteLines);}},
            {"ui", "stash_max", "Stash: prompts kept", SettingKind::Int, {}, "the prompts put aside (Ctrl+S, the list: Ctrl+L); the prompts copied by themselves "
                "(a prompt replaced by another one) never are refused", false, [](const cluster::Config& cf) {return std::to_string(cf.stashMax);}},
            {"ui", "stash_full", "Stash: when it is full", SettingKind::Choice, {"refuse", "drop_oldest"}, "refuse: the prompt stays in the editor, nothing is lost; "
                "drop_oldest: the oldest stashed prompt makes room (with a notice)", false, [](const cluster::Config& cf) {return cf.stashFull;}},
            {"ui", "stash_rewound", "Stash: the prompts a rewind drops", SettingKind::Bool, {}, "Esc (the chooser of the points of the conversation): the prompts it removes "
                "are put in the stash too", false, [](const cluster::Config& cf) {return cf.stashRewound ? std::string("true") : std::string("false");}},
            {"ui", "paste_chars", "Paste collapsed above (characters)", SettingKind::Int, {}, "a longer paste is one [Pasted text #N] in the prompt (0: never)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.pasteChars);}},
            {"ui", "editor", "Editor of the prompt (Ctrl+E)", SettingKind::Text, {}, "empty: nvim, vim, vi, $VISUAL, $EDITOR, emacs -nw, nano (the first installed)", false,
                [](const cluster::Config& cf) {return cf.editor;}},
            {"ui", "image_viewer", "Image viewer (right click on a chip)", SettingKind::Text, {}, "empty: xdg-open (the viewer of the desktop)", false,
                [](const cluster::Config& cf) {return cf.imageViewer;}},
            {"ui", "image_preview", "Image preview (terminal)", SettingKind::Choice, {"blocks", "chafa"}, "blocks: the pixels as half blocks; chafa: its symbols", false,
                [](const cluster::Config& cf) {return cf.imagePreview;}},
            {"ui", "typewriter", "Answers typed as they come", SettingKind::Bool, {}, "the text appears a few characters at a time, also when the agent sends it at once", false,
                [](const cluster::Config& cf) {return cf.typewriter ? std::string("true") : std::string("false");}},
            {"ui", "typewriter_speed", "Typing speed (characters / s)", SettingKind::Int, {}, "faster by itself when a long text is behind (never more than ~1.5 s late)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.typewriterSpeed);}},
        }},
        {"notify", "Notifications", {
            {"ui", "notify", "Desktop notifications", SettingKind::Bool, {}, "all of them (off: none); their face, their buttons (Allow / Deny, the options of a question, Open), one per session", false,
                [](const cluster::Config& cf) {return cf.notify ? std::string("true") : std::string("false");}},
            {"ui", "notify_when", "When", SettingKind::Choice, {"unfocused", "always"}, "unfocused: only when claude-cluster is not in front (its terminal / window "
                "not focused: its own popups are in front of you); always: also when it is", false,
                [](const cluster::Config& cf) {return cf.notifyWhen;}},
            {"ui", "notify_global", "The global session answers", SettingKind::Bool, {}, "^_^ the end of its answers (their first words)", false,
                [](const cluster::Config& cf) {return cf.notifyGlobal ? std::string("true") : std::string("false");}},
            {"ui", "notify_done", "A session finishes", SettingKind::Bool, {}, ">‿< the end of a turn of a sub-session (its tokens)", false,
                [](const cluster::Config& cf) {return cf.notifyDone ? std::string("true") : std::string("false");}},
            {"ui", "notify_permission", "A permission waits", SettingKind::Bool, {}, "•_• a tool waits for your yes (Allow / Deny from the notification); o_o: the safety check", false,
                [](const cluster::Config& cf) {return cf.notifyPermission ? std::string("true") : std::string("false");}},
            {"ui", "notify_question", "A question of an agent", SettingKind::Bool, {}, "•_• its options as buttons when there is one question", false,
                [](const cluster::Config& cf) {return cf.notifyQuestion ? std::string("true") : std::string("false");}},
            {"ui", "notify_error", "An error, a crash", SettingKind::Bool, {}, "o_o a turn failed, a session stopped", false,
                [](const cluster::Config& cf) {return cf.notifyError ? std::string("true") : std::string("false");}},
            {"ui", "notify_limits", "A limit close or reached", SettingKind::Bool, {}, "o_o 80 % / 95 % of the 5 h / 7 d windows; -_- reached (until its reset)", false,
                [](const cluster::Config& cf) {return cf.notifyLimits ? std::string("true") : std::string("false");}},
            {"ui", "notify_web", "Web devices", SettingKind::Bool, {}, "•_• a device asks to connect (Allow / Allow and trust / Deny), ^_^ a device connected", false,
                [](const cluster::Config& cf) {return cf.notifyWeb ? std::string("true") : std::string("false");}},
            {"ui", "notify_tasks", "The task queue done", SettingKind::Bool, {}, ">‿< one summary when every task is over", false,
                [](const cluster::Config& cf) {return cf.notifyTasks ? std::string("true") : std::string("false");}},
            {"ui", "notify_task_each", "Each task done", SettingKind::Bool, {}, "a notification at the end of each task (task_add notify: true asks it for one)", false,
                [](const cluster::Config& cf) {return cf.notifyTaskEach ? std::string("true") : std::string("false");}},
            {"ui", "notify_reminders", "Reminders", SettingKind::Bool, {}, "•_• schedule add --notify, at its time", false,
                [](const cluster::Config& cf) {return cf.notifyReminders ? std::string("true") : std::string("false");}},
            {"ui", "notify_ready", "Something slow is ready", SettingKind::Bool, {}, "^_^ claude-cluster started (its global session), the speech "
                "model loaded, the map of this computer written: come back when it is there", false,
                [](const cluster::Config& cf) {return cf.notifyReady ? std::string("true") : std::string("false");}},
            {"ui", "notify_app", "Android app", SettingKind::Bool, {}, "the same notifications published to the Android app (the kinds above apply; shown when the app is not in front, "
                "each phone chooses its own kinds in its This device page; off: none sent)", false,
                [](const cluster::Config& cf) {return cf.notifyApp ? std::string("true") : std::string("false");}},
            {"ui", "notify_sound", "With a sound", SettingKind::Bool, {}, "a short sound of the desktop theme, one per kind (done, question, error, web)", false,
                [](const cluster::Config& cf) {return cf.notifySound ? std::string("true") : std::string("false");}},
        }},
        {"agent", "Agent", {
            {"agent", "provider", "Provider of every session", SettingKind::Text, {}, "the global session, every sub-session (existing, new, imported conversations): "
                "<provider>[/<model>] - claude, ollama/<model>, openai/<model>, opencode/<p>/<m>, codex; the global session needs a claude-driver one (claude, ollama, anthropic-api...)", true,
                [](const cluster::Config& cf) {return cf.provider;}},
            {"agent", "permission_mode", "Permission mode", SettingKind::Choice, modes, "mode of the new sessions (empty: the default of the agent)", false,
                [](const cluster::Config& cf) {return cf.mode;}},
            {"agent", "effort", "Thinking effort", SettingKind::Choice, efforts, "of the new sessions (empty: the default of the agent; per session: /effort, the palette)", false,
                [](const cluster::Config& cf) {return cf.effort;}},
            {"agent", "allow_bypass", "bypassPermissions possible", SettingKind::Bool, {}, "claude launched with --allow-dangerously-skip-permissions (needed to switch to bypass)", true,
                [](const cluster::Config& cf) {return cf.allowBypass ? std::string("true") : std::string("false");}},
            {"agent", "orchestration", "Orchestration mode", SettingKind::Choice, {"compact", "hybrid", "ultra"},
                "how readily the global session splits work into sub-sessions (told to it in its system prompt, taken at its next start): compact - the fewest tokens, "
                "medium tasks go in ONE session (one per project), it asks you before splitting; hybrid - several sub-sessions when it pays, sub-globals rarely; ultra - "
                "a sub-session (and sub-globals) for each different task", false,
                [](const cluster::Config& cf) {return cf.orchestration;},
                [](const std::string& value) {
                    return value == "compact" ? std::string("one session per project, asks before splitting") : value == "ultra" ? std::string("a sub-session or sub-global per task")
                        : std::string("several sub-sessions when it pays, sub-globals rarely");
                }},
            {"agent", "work_method", "Work method", SettingKind::Choice, {"free", "list-review", "one-task", "plan-first", "custom"},
                "how the work is organised: injected, short, in the system prompt of the sub-sessions and of the global session, which splits the requests accordingly; "
                "taken by each session at its next start", false,
                [](const cluster::Config& cf) {return cf.workMethod;},
                [](const std::string& value) {
                    const cluster::MethodPreset* preset = cluster::method_preset(value);
                    return preset == nullptr ? std::string() : value == "free" ? std::string("no method") : value == "custom" ? std::string("only the text below")
                        : preset->label + ": " + preset->sub;
                }},
            {"agent", "work_method_text", "Work method (free text)", SettingKind::Text, {}, "your own words, added to the preset (alone with custom): short, 400 characters at most", false,
                [](const cluster::Config& cf) {return cf.workMethodText;}},
            {"agent", "work_method_detect", "Detect the work method", SettingKind::Bool, {}, "the global session may PROPOSE a method from how you phrase your requests (a small sample of your "
                "prompts, no model): a popup you answer Allow / Deny - never applied alone", false,
                [](const cluster::Config& cf) {return cf.workMethodDetect ? std::string("true") : std::string("false");}},
            {"agent", "work_method_every", "Detection: every N sessions", SettingKind::Int, {}, "sessions started by the global session between two checks (0: only when asked)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.workMethodEvery);}},
            {"agent", "sub_read", "Sub-sessions: see their tasks", SettingKind::Bool, {}, "a sub-session lists its own tasks and schedules (MCP task_list, "
                "schedule_list: the ones it made; never the others). Off: it has no such tool", false, [](const cluster::Config& cf) {return cf.subRead ? std::string("true") : std::string("false");}},
            {"agent", "sub_tasks", "Sub-sessions: queue tasks", SettingKind::Bool, {}, "a sub-session queues tasks for itself or another session (MCP "
                "task_add, task_cancel, task_act on its own tasks)", false, [](const cluster::Config& cf) {return cf.subTasks ? std::string("true") : std::string("false");}},
            {"agent", "sub_schedules", "Sub-sessions: create schedules", SettingKind::Bool, {}, "a sub-session schedules prompts, notifications or tasks "
                "(MCP schedule_add, schedule_edit): off by default - a repeated schedule runs by itself, long after", false, [](const cluster::Config& cf) {return cf.subSchedules ? std::string("true") : std::string("false");}},
            {"agent", "sub_spawn", "Sub-sessions: tasks that start sessions", SettingKind::Bool, {}, "a task or a schedule of a sub-session may start "
                "sessions of its own (each costs tokens): off by default", false, [](const cluster::Config& cf) {return cf.subSpawn ? std::string("true") : std::string("false");}},
            {"agent", "global_pin", "Global session can pin", SettingKind::Bool, {}, "the global session may pin a message or a display block at the bottom of the view of "
                "any session (MCP session_pin, session_unpin); you can always pin by hand (right click, Ctrl+K: Pin / Unpin)", false,
                [](const cluster::Config& cf) {return cf.pinGlobal ? std::string("true") : std::string("false");}},
            {"agent", "taxonomy_edit", "Global session can edit the taxonomy", SettingKind::Bool, {}, "the global session may set the parent of a category / tag, rename, delete "
                "and merge them (MCP taxonomy), which moves the sessions in the graph and the grouped list; you can always edit it by hand (graph, palette, `claude-cluster taxonomy`)", false,
                [](const cluster::Config& cf) {return cf.taxonomyEdit ? std::string("true") : std::string("false");}},
            {"agent", "sub_pin", "Sub-sessions can pin", SettingKind::Bool, {}, "a sub-session may pin an item on its own view (a task list, a form you keep seeing); "
                "never on another session", false, [](const cluster::Config& cf) {return cf.subPin ? std::string("true") : std::string("false");}},
            {"agent", "sub_spawn_cap", "Sub-sessions: sessions started at most", SettingKind::Int, {}, "the sessions of its tasks / schedules a sub-session "
                "may have waiting or open at once (past it a new one is refused); the global session has no cap", false,
                [](const cluster::Config& cf) {return std::to_string(cf.subSpawnCap);}},
            {"agent", "extra_args", "Extra arguments (claude)", SettingKind::List, {}, "added to every claude session, comma separated", true,
                [](const cluster::Config& cf) {return join_(cf.extraArgs);}},
        }},
        {"commands", "Commands", {}},
        {"global", "Global session", {
            {"global", "enabled", "Enabled", SettingKind::Bool, {}, "the session that manages the others (MCP)", true,
                [](const cluster::Config& cf) {return cf.globalEnabled ? std::string("true") : std::string("false");}},
            {"global", "model", "Model", SettingKind::Text, {}, "empty: the default model of the provider ([agent] provider)", true,
                [](const cluster::Config& cf) {return cf.globalModel;}},
            {"global", "permission_mode", "Permission mode", SettingKind::Choice, modes, "applied at every start (bypassPermissions: it never asks)", true,
                [](const cluster::Config& cf) {return cf.globalMode;}},
            {"global", "effort", "Thinking effort", SettingKind::Choice, efforts, "applied at every start (empty: its own, /effort, else the one of [agent])", true,
                [](const cluster::Config& cf) {return cf.globalEffort;}},
            {"global", "cwd", "Folder", SettingKind::Text, {}, "working folder of the global session", true,
                [](const cluster::Config& cf) {return cf.globalCwd;}},
            {"global", "prompt", "System prompt file", SettingKind::Text, {}, "its own system prompt, added to CLAUDE.md", true,
                [](const cluster::Config& cf) {return cf.globalPrompt;}},
            {"global", "remote_name", "Remote Control name", SettingKind::Text, {}, "name of the Remote Control session", false,
                [](const cluster::Config& cf) {return cf.remoteName;}},
            {"global", "edit_prompt", "System prompt (edit)", SettingKind::Action, {},
                "opens it in the editor ([ui] editor, else nvim, vim, vi, $VISUAL, $EDITOR...); saved: the global session restarted on the same conversation", false,
                [](const cluster::Config& cf) {return cluster::short_path(cluster::expand_home(cf.globalPrompt).string());}},
            {"global", "machine_sync", "Map of this computer (sync)", SettingKind::Action, {},
                "scans again what the global session knows of this computer (asks first): OS, user, shell aliases, tools, git repositories", false,
                [](const cluster::Config&) {
                    const std::string age = cluster::machine_age();
                    return age.empty() ? std::string("never: Enter to scan") : "last " + age;
                }},
        }},
        {"sessions", "Sessions", {
            {"sessions", "describe", "Short descriptions", SettingKind::Choice, {"model", "prompt", "off"}, "a few words under the name of each session (the trash too), "
                "to find it again: model: the title Claude Code made, else written by a model (Model of the descriptions; its own process, nothing kept) after its 1st, "
                "3rd, 9th... turn; prompt: its first prompt; off: none written (the ones given kept)", false,
                [](const cluster::Config& cf) {return cf.describe;}},
            {"sessions", "describe_model", "Model of the descriptions", SettingKind::Text, {}, "<provider>/<model> writing them, any provider (claude, "
                "codex / OpenAI, opencode, qwen, a local one: ollama/llama3.2...); empty: the provider of each session (haiku for Anthropic)", false,
                [](const cluster::Config& cf) {return cf.describeModel;}},
            {"sessions", "trash_days", "Trash kept (days)", SettingKind::Int, {}, "a closed session stays restorable this long", false,
                [](const cluster::Config& cf) {return std::to_string(cf.trashDays);}},
            {"sessions", "max_parallel", "Parallel sessions", SettingKind::Int, {}, "sub-sessions working at the same time: past it, the tasks (task_add, the "
                "groups of the global session) wait in the task queue and are sent as soon as a place is free", false,
                [](const cluster::Config& cf) {return std::to_string(cf.maxParallel);}},
            {"sessions", "queue_given", "Queued prompts", SettingKind::Choice, {"step", "end"}, "a prompt typed while the turn runs (Claude Code sessions):\n"
                "• step: given to claude after 1.5 s (Esc edits it before), taken at its next step - between two tool calls - like the queue of Claude Code\n"
                "• end: held here until the end of the turn (editable until then), then sent", false,
                [](const cluster::Config& cf) {return cf.queueGiven;}},
            {"sessions", "history_limit", "Prompt history (prompts)", SettingKind::Int, {}, "the prompts kept for ↑ / Ctrl+R (history.jsonl): the oldest "
                "dropped beyond it; 0: all", false, [](const cluster::Config& cf) {return std::to_string(cf.historyLimit);}},
            {"sessions", "transcript_limit_mb", "Transcript size (MB)", SettingKind::Int, {}, "each conversation kept here (logs/<session>.jsonl, the "
                "pictures and display blocks in it): its oldest entries dropped beyond this size (the conversation of the agent itself untouched); 0: no "
                "limit", false, [](const cluster::Config& cf) {return std::to_string(cf.transcriptLimitMb);}},
            {"sessions", "task_queue", "Task queue", SettingKind::Int, {}, "tasks waiting for a free place, at most (a new one past it is refused)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.taskQueue);}},
            {"sessions", "task_result_chars", "Task result size", SettingKind::Int, {}, "the answer kept in a task (its result, chained into the prompt of the "
                "next one with {{tN.result}}): its first characters, at most", false, [](const cluster::Config& cf) {return std::to_string(cf.taskResultChars);}},
            {"sessions", "budget_usd", "Budget per session (USD)", SettingKind::Float, {}, "0: none; reached: the session refuses new prompts", false,
                [](const cluster::Config& cf) {return number_(cf.budget);}},
            {"sessions", "questions_to_global", "Questions on the global session", SettingKind::Bool, {}, "a question of the agent of a "
                "sub-session (AskUserQuestion) shown on the global session, its own one greyed while it waits; answered with the keys, the mouse "
                "or the voice (a number or an option, valide / suivant, retour, autre <answer>, annule); off: on its own session only", false,
                [](const cluster::Config& cf) {return cf.questionsToGlobal ? std::string("true") : std::string("false");}},
            {"sessions", "global_answers", "Global session answers questions", SettingKind::Bool, {}, "the global session is told the questions "
                "of the sub-sessions and may choose their answer (MCP session_allow answers) when it knows what you want; you can answer first", false,
                [](const cluster::Config& cf) {return cf.globalAnswers ? std::string("true") : std::string("false");}},
            {"sessions", "bypass_to_global", "Bypass: global decides", SettingKind::Bool, {}, "a sub-session in bypassPermissions still asks for "
                "some commands (the safety check of Claude Code: a removal it cannot resolve...): the global session decides them (it asks you when "
                "unsure); off: you answer them. In the other modes such a command is asked to you (auto mode: after the check stopped it)", false,
                [](const cluster::Config& cf) {return cf.bypassToGlobal ? std::string("true") : std::string("false");}},
            {"sessions", "global_fork", "Global session can fork", SettingKind::Bool, {}, "the global session may copy a sub-session of Claude Code "
                "(MCP session_fork: its conversation so far, each one goes on apart) to give a new one its context; you can always (palette, fork)", false,
                [](const cluster::Config& cf) {return cf.globalFork ? std::string("true") : std::string("false");}},
            {"sessions", "global_edit", "Global session can edit", SettingKind::Bool, {}, "the global session may rewrite the conversation of a sub-session "
                "of Claude Code (MCP session_edit: texts replaced, messages deleted, its end cut; a new conversation, the old one kept)", false,
                [](const cluster::Config& cf) {return cf.globalEdit ? std::string("true") : std::string("false");}},
            {"sessions", "archive_one_shot", "Archive one-shot sessions", SettingKind::Bool, {}, "a session the global session started for ONE task "
                "(session_spawn one_shot) is closed to the trash (restorable) once its answer is relayed; kept when you typed in it, look at it or it waits for you. "
                "Off: the flag is ignored", false, [](const cluster::Config& cf) {return cf.archiveOneShot ? std::string("true") : std::string("false");}},
            {"sessions", "rewrite_global_prompt", "Global session can rewrite its own system prompt", SettingKind::Bool, {}, "the global session may rewrite "
                "its own system prompt (MCP config_write global_prompt): what it obeys at every start, it is started again on it. Off: it asks you (Ctrl+K: edit the system prompt)",
                false, [](const cluster::Config& cf) {return cf.globalPromptWrite ? std::string("true") : std::string("false");}},
            {"sessions", "rewrite_sessions_prompt", "Global session can rewrite the system prompt of the sub-sessions", SettingKind::Bool, {}, "the global session may "
                "rewrite the system prompt of the sub-sessions (MCP config_write sessions_prompt): each one takes it at its next start. Off: it asks you (Ctrl+K: edit the system prompt)",
                false, [](const cluster::Config& cf) {return cf.sessionsPromptWrite ? std::string("true") : std::string("false");}},
            {"sessions", "prompt", "System prompt file", SettingKind::Text, {}, "system prompt of every sub-session (claude driver), added to CLAUDE.md; empty file: none", false,
                [](const cluster::Config& cf) {return cf.sessionsPrompt;}},
            {"sessions", "edit_sessions_prompt", "System prompt (edit)", SettingKind::Action, {},
                "opens it in the editor ([ui] editor, else nvim, vim, vi, $VISUAL, $EDITOR...); taken by each sub-session at its next start", false,
                [](const cluster::Config& cf) {return cluster::short_path(cluster::expand_home(cf.sessionsPrompt).string());}},
        }},
        {"voice", "Voice", {
            {"voice", "enabled", "Enabled", SettingKind::Bool, {}, "voice of the global session (voice-listen / voice-say)", true,
                [](const cluster::Config& cf) {return cf.voice.enabled ? std::string("true") : std::string("false");}},
            {"voice", "mode", "Listening", SettingKind::Choice, {"push", "auto", "wake"}, "push-to-talk key, always, or after the wake phrase", true,
                [](const cluster::Config& cf) {return cf.voice.mode;}},
            {"voice", "source", "Microphone", SettingKind::Text, {}, "PulseAudio / PipeWire source of the wake phrase heard while claude-cluster is closed "
                "(Services > Service), empty: the default one", false,
                [](const cluster::Config& cf) {return cf.service.source;}},
            {"voice", "wake_word", "Wake phrase", SettingKind::Text, {}, "wake mode, variants separated by commas (ok claude, okay claude)", false,
                [](const cluster::Config& cf) {return cf.voice.wakeWord;}},
            {"voice", "wake_seconds", "Armed (seconds)", SettingKind::Int, {}, "the phrase alone: the next sentence within this time is the command", false,
                [](const cluster::Config& cf) {return std::to_string(cf.voice.wakeSeconds);}},
            {"voice", "wake_reply", "Spoken when armed", SettingKind::Text, {}, "ex: Oui ? (empty: silent)", false,
                [](const cluster::Config& cf) {return cf.voice.wakeReply;}},
            {"voice", "only_me", "Only my voice", SettingKind::Bool, {}, "voice-listen --only-me (needs voice-enroll)", true,
                [](const cluster::Config& cf) {return cf.voice.onlyMe ? std::string("true") : std::string("false");}},
            {"voice", "confirm_seconds", "Confirmation (seconds)", SettingKind::Int, {}, "the transcription is shown this long before it is sent", false,
                [](const cluster::Config& cf) {return std::to_string(cf.voice.confirmSeconds);}},
            {"voice", "speak", "Spoken answers", SettingKind::Bool, {}, "short summary of the answers of the global session", false,
                [](const cluster::Config& cf) {return cf.voice.speak ? std::string("true") : std::string("false");}},
            {"voice", "summary_chars", "Summary length", SettingKind::Int, {}, "characters of the spoken summary", false,
                [](const cluster::Config& cf) {return std::to_string(cf.voice.summaryChars);}},
            {"voice", "say_engine", "Speech engine", SettingKind::Choice, {"piper", "kokoro", "elevenlabs", "cartesia"},
                "text to speech of the answers (←→: what each one handles); an engine failing (no key, quota, model missing): Piper", false,
                [](const cluster::Config& cf) {return cf.voice.sayEngine;}},
            {"voice", "say_voices", "Voices of the engine", SettingKind::Text, {},
                "its voice per language: fr=ff_siwis, en=bf_emma (voice-tts --list <engine>); another engine for a language: "
                "es=kokoro:ef_dora; a language missing: the default voice of the engine", false,
                [](const cluster::Config& cf) {return cf.voice.sayVoices;}},
            {"voice", "voice_fr", "French voice (Piper)", SettingKind::Text, {}, "Piper voice of the French answers (engine piper, the fallback of the others)", false,
                [](const cluster::Config& cf) {return cf.voice.voiceFr;}},
            {"voice", "voice_en", "English voice (Piper)", SettingKind::Text, {}, "Piper voice of the English answers (engine piper, the fallback of the others)", false,
                [](const cluster::Config& cf) {return cf.voice.voiceEn;}},
            {"voice", "listen_command", "Listen command", SettingKind::Text, {}, "speech to text (one JSON per sentence)", true,
                [](const cluster::Config& cf) {return cf.voice.listenCommand;}},
            {"voice", "say_command", "Say command", SettingKind::Text, {}, "text to speech", false,
                [](const cluster::Config& cf) {return cf.voice.sayCommand;}},
            {"voice", "listen_model", "Speech recognition", SettingKind::Choice, {"", "parakeet", "whisper", "kroko-fr", "groq", "deepgram"},
                "speech to text (←→: what each one handles): local models, or the free-tier APIs groq / deepgram (their key: "
                "claude-cluster auth login groq); an API failing: the local model", false,
                [](const cluster::Config& cf) {return cf.voice.listenModel;}},
            {"voice", "listen_language", "Language spoken", SettingKind::Text, {}, "for whisper / groq / deepgram: fr, en...; empty: found by the model (deepgram: multi, languages mixed)", false,
                [](const cluster::Config& cf) {return cf.voice.listenLanguage;}},
            {"voice", "end_silence", "End of a sentence (seconds)", SettingKind::Float, {}, "the silence that ends a sentence (0: 1.0); a short pause goes on the same one", false,
                [](const cluster::Config& cf) {return number_(cf.voice.endSilence);}},
            {"voice", "words", "Voice words file", SettingKind::Text, {}, "the cancel / send words said alone while a transcription waits; empty: voice-words.txt", false,
                [](const cluster::Config& cf) {return cf.voice.words;}},
            {"voice", "edit_words", "Voice words (edit)", SettingKind::Action, {}, "opens it in the editor: one word per line under [cancel] / [send]", false,
                [](const cluster::Config& cf) {return cluster::short_path(cluster::voice_words_path(cf.voice.words).string());}},
            {"voice", "vocabulary", "Vocabulary file", SettingKind::Text, {}, "proper names written back in what is heard (+ sessions, projects, your names); empty: vocabulary.txt of the config folder", false,
                [](const cluster::Config& cf) {return cf.voice.vocabulary;}},
            {"voice", "edit_vocabulary", "Vocabulary (edit)", SettingKind::Action, {}, "opens it in the editor: one name per line, \"name = how it is heard, ...\"", false,
                [](const cluster::Config& cf) {return cluster::short_path(cluster::vocabulary_path(cf.voice.vocabulary).string());}},
        }},
        {"user", "User", {
            {"user", "nickname", "Nickname", SettingKind::Text, {}, "how the global session calls you", false,
                [](const cluster::Config& cf) {return cf.user.nickname;}},
            {"user", "email", "Contact email", SettingKind::Text, {}, "for commits, messages the global session writes for you", false,
                [](const cluster::Config& cf) {return cf.user.email;}},
            {"user", "editor", "Main editor", SettingKind::Text, {}, "nvim, vim, code... (Ctrl+E too when [ui] editor is empty)", false,
                [](const cluster::Config& cf) {return cf.user.editor;}},
            {"user", "language", "Language of the answers", SettingKind::Choice, {"", "fr", "en"}, "empty: the one of each message", false,
                [](const cluster::Config& cf) {return cf.user.language;}},
            {"user", "notes", "Notes", SettingKind::Text, {}, "anything the global session should know about you", false,
                [](const cluster::Config& cf) {return cf.user.notes;}},
            {"ui", "machine_days", "Map: sync proposed after (days)", SettingKind::Int, {}, "the map of this computer older: a sync proposed at start (0: never)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.machineDays);}},
        }},
        {"web", "Web", {
            {"web", "enabled", "Open the web access at start", SettingKind::Bool, {}, "a browser or the app of a phone / another computer drives claude-cluster.\n"
                "• who: the devices of this network (same LAN); beyond it: Beyond the LAN\n"
                "• how: the address and the 6-digit code shown at the top (a click copies the code), then your yes here (a popup)\n"
                "• several at once; trusted: no code next time; banned: refused (devices › at the top, the palette: Web devices)\n"
                "• opened / closed live: the palette, Web access", false,
                [](const cluster::Config& cf) {return cf.web.enabled ? std::string("true") : std::string("false");}},
            {"web", "approve", "Approve each new device", SettingKind::Bool, {}, "after its code, a new device waits for your yes here.\n"
                "• a popup: its name, its address, its browser; allow, allow and trust, deny\n"
                "• off: the code is enough on the LAN (from outside the LAN: always asked)", false,
                [](const cluster::Config& cf) {return cf.web.approve ? std::string("true") : std::string("false");}},
            {"web", "port", "Port", SettingKind::Int, {}, "the address shown: https://<this computer on the LAN>:<port>", false,
                [](const cluster::Config& cf) {return std::to_string(cf.web.port);}},
            {"web", "remote_port", "Port outside", SettingKind::Int, {}, "router: the port opened on the router (0: the same as here)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.web.remotePort);}},
            {"web", "tty_port", "Terminal page port", SettingKind::Int, {}, "the terminal interface of claude-cluster in a browser (its own port, the same code and "
                "approval): a real terminal of this computer, its keys, its mouse; the browser only (never the app); 0: off", false,
                [](const cluster::Config& cf) {return std::to_string(cf.web.ttyPort);}},
            {"web", "tty_remote_port", "Terminal page port outside", SettingKind::Int, {}, "router: the port opened outside for the terminal page (0: the same)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.web.ttyRemotePort);}},
            {"web", "remote", "Beyond the LAN", SettingKind::Choice, {"off", "tailscale", "router"}, "the page reached away from home (the app goes there by itself when the LAN is not reached), encrypted end to end.\n"
                "tailscale (recommended: nothing open to the Internet)\n"
                "  1. install Tailscale here and on the phone, log in both with the same account\n"
                "  2. choose tailscale here: the address shown at the top becomes its tailnet one\n"
                "router (open to the Internet)\n"
                "  1. choose router: the port is opened on the router by UPnP (upnpc), else forward it yourself (TCP, Port outside → Port)\n"
                "  2. a changing public address: a dynamic DNS name in Name from outside\n"
                "Always from outside: each new device approved here, 5 wrong codes block its address 15 min, the bans apply", false,
                [](const cluster::Config& cf) {return cf.web.remote;}},
            {"web", "remote_host", "Name from outside", SettingKind::Text, {}, "router: a dynamic DNS name pointing at your router (duckdns, no-ip...).\n"
                "• empty: its public address\n• put in the certificate (the browser checks it)", false,
                [](const cluster::Config& cf) {return cf.web.remoteHost;}},
            {"web", "shell", "Shell from the page", SettingKind::Bool, {}, "!command (and :r !command) typed in the web page or the app run on this computer.\n"
                "• in the folder of the session, like in the terminal\n"
                "• off (default): refused there - on, every device connected gets a shell of this computer, from outside the LAN too", false,
                [](const cluster::Config& cf) {return cf.web.shell ? std::string("true") : std::string("false");}},
            {"web", "settings", "Settings from the page", SettingKind::List, {}, "the sections of the settings a device of the web page (the app) may change; "
                "empty (default): none, the settings read only there (This device stays its own).\n"
                "• sections: ui, notify, agent, commands, global, sessions, voice, user, service, web, mcp, services (the actions of the Services tab: "
                "start / stop, open / close, restart...)\n• all: every one (web itself: never this option from the page)", false,
                [](const cluster::Config& cf) {
                    std::string out;
                    for (const std::string& s: cf.web.settings)
                        out += (out.empty() ? "" : ", ") + s;
                    return out;
                }},
            {"web", "https", "HTTPS", SettingKind::Bool, {}, "TLS with a certificate of its own, made once.\n"
                "• the conversation and the code encrypted, the microphone of the page allowed\n"
                "• the browser asks to accept it the first time (or: the palette, Web access: trust its certificate)\n"
                "• off: plain http, no microphone", false,
                [](const cluster::Config& cf) {return cf.web.https ? std::string("true") : std::string("false");}},
        }},
        {"attachments", "Attachments", {
            {"attachments", "enabled", "Attachments", SettingKind::Bool, {}, "files, images and photos attached to a prompt (the paperclip, a drop, a paste; the app: the camera)", false,
                [](const cluster::Config& cf) {return cf.attach.enabled ? std::string("true") : std::string("false");}},
            {"attachments", "max_mb", "Largest file (MB)", SettingKind::Int, {}, "a bigger file is refused", false,
                [](const cluster::Config& cf) {return std::to_string(cf.attach.maxMb);}},
            {"attachments", "max_files", "Files per prompt", SettingKind::Int, {}, "at most this many attached to one prompt", false,
                [](const cluster::Config& cf) {return std::to_string(cf.attach.maxFiles);}},
            {"attachments", "keep", "Kept per session", SettingKind::Int, {}, "the oldest attachments of a session go past this many (0: all kept); all of them go when the session is deleted", false,
                [](const cluster::Config& cf) {return std::to_string(cf.attach.keep);}},
            {"attachments", "max_pixels", "Downscale images above (px)", SettingKind::Int, {}, "a photo larger than this side is reduced before it is stored and sent (0: as it is); needs ImageMagick (magick)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.attach.maxPixels);}},
            {"attachments", "strip_exif", "Strip the EXIF of images", SettingKind::Bool, {}, "photos re-encoded without their metadata (GPS position, camera, date); needs ImageMagick (magick)", false,
                [](const cluster::Config& cf) {return cf.attach.stripExif ? std::string("true") : std::string("false");}},
            {"attachments", "dir", "Stored in", SettingKind::Text, {}, "the folder of the attachments, one folder per session (empty: <data>/attachments)", false,
                [](const cluster::Config& cf) {return cf.attach.dir;}},
            {"attachments", "link", "Allow link mode", SettingKind::Bool, {}, "a file of this computer linked by its path, nothing copied (window, terminal, a page opened on this computer); "
                "a phone or another computer always uploads", false,
                [](const cluster::Config& cf) {return cf.attach.link ? std::string("true") : std::string("false");}},
            {"attachments", "global", "The global session may attach", SettingKind::Bool, {}, "it may attach a local file to a prompt it sends (MCP session_send attachments): "
                "the project folder of the session and the home of the user only (no hidden folder of the home, nothing of claude-cluster itself)", false,
                [](const cluster::Config& cf) {return cf.attach.globalAttach ? std::string("true") : std::string("false");}},
            {"attachments", "global_max", "Files per prompt of the global session", SettingKind::Int, {}, "at most this many", false,
                [](const cluster::Config& cf) {return std::to_string(cf.attach.globalMax);}},
        }},
        {"mcp", "MCP", {
            {"mcp", "servers", "Custom MCP servers", SettingKind::List, {}, "more tools for the global session (MCP servers), comma separated.\n"
                "• a local program: name: command args (stdio; env VAR=value command... for its variables)\n"
                "  e.g. files: npx -y @modelcontextprotocol/server-filesystem ~/notes\n"
                "• a site: name: https://url (HTTP)\n"
                "• its tools: mcp__<name>__*, on / off one by one in this MCP page\n"
                "Applied at the next start of the global session", true,
                [](const cluster::Config& cf) {return join_(cf.mcpServers);}},
        }},
        {"usage", "Usage", {
            {"usage", "account", "Account windows", SettingKind::Bool, {}, "the 5 h / 7 d windows of the Claude account logged in here (the usage endpoint of claude.ai, "
                "the token of Claude Code: ~/.claude/.credentials.json, never shown), read every 5 min and after each turn; off: the ones the last turn told only", false,
                [](const cluster::Config& cf) {return cf.usage.account ? std::string("true") : std::string("false");}},
            {"usage", "transcripts", "Claude Code transcripts", SettingKind::List, {}, "the folders of the transcripts of Claude Code (~/.claude/projects): every answer's usage "
                "(the API's own numbers: input, cache written / read, output, thinking) summed by day and model, every session of this computer - in claude-cluster or not; "
                "empty: the ledger of claude-cluster's own sessions only.\nThe prices: [usage.prices] in config.toml (model = $ per million input tokens), else the ones learned "
                "from the cost Claude Code tells, else the public table", false,
                [](const cluster::Config& cf) {
                    std::string out;
                    for (const std::string& s: cf.usage.transcripts)
                        out += (out.empty() ? "" : ", ") + s;
                    return out;
                }},
        }},
        {"debug", "Debug", {
            {"debug", "showcase_all", "Show everything", SettingKind::Action, {}, "everything below in a row, on this interface: the display blocks in the "
                "showcase session, its states, a progress popup, a failure popup, a notification, a permission, the questions (QCM, Q&A, chat, review)", false,
                [](const cluster::Config&) {return std::string("run");}},
            {"debug", "showcase_displays", "Display blocks", SettingKind::Action, {}, "markdown, callouts, code, table, charts (bar, line, pie), progress, "
                "metrics, heatmap, image, svg, diagram, diff, json, tree, terminal, checklist, steps, timeline, kanban, buttons, form, details, compare, math, "
                "audio, files - and how this platform draws each one", false, [](const cluster::Config&) {return std::string("show");}},
            {"debug", "showcase_questions", "Questions popup", SettingKind::Action, {}, "the questions of an agent: a single choice, a multiple choice, "
                "a free answer (Other), the chat about them, back / next, the review before Submit / Cancel - answered here, sent to nobody", false,
                [](const cluster::Config&) {return std::string("show");}},
            {"debug", "showcase_permission", "Permission popup", SettingKind::Action, {}, "a permission asked by an agent (Allow / Always / Deny): "
                "nothing is run", false, [](const cluster::Config&) {return std::string("show");}},
            {"debug", "showcase_states", "Session states", SettingKind::Action, {}, "the showcase session working, waiting, in error, idle: its icon, "
                "the title of the terminal / window / tab, the face", false, [](const cluster::Config&) {return std::string("show");}},
            {"debug", "showcase_progress", "Progress popup", SettingKind::Action, {}, "a long action: its steps, its log scrolling, then done", false,
                [](const cluster::Config&) {return std::string("show");}},
            {"debug", "showcase_failure", "Failure popup", SettingKind::Action, {}, "an action that failed: what went wrong, OK / Retry", false,
                [](const cluster::Config&) {return std::string("show");}},
            {"debug", "showcase_notify", "Notifications", SettingKind::Action, {}, "a desktop / phone notification and the notices (information, error)",
                false, [](const cluster::Config&) {return std::string("show");}},
        }},
        {"services", "Services", {
            {"services", "refresh", "Refresh", SettingKind::Choice, {"live", "interval", "manual"},
                "live: what changes shown at once (the voice heard, the web, the sessions), the slow parts (systemd, its journal, tailscale) read every Delay;\n"
                "interval: everything every Delay; manual: R reads it again (and after each action)", false,
                [](const cluster::Config& cf) {return cf.services.refresh;}},
            {"services", "delay", "Delay (s)", SettingKind::Int, {}, "seconds between two reads (interval), the slow parts kept so long (live)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.services.delay);}},
            {"services", "svc_service", "Service (systemd)", SettingKind::Action, {},
                "the service of your session (systemd --user): it listens for the wake phrase while claude-cluster is closed and opens it when heard.\n"
                "• Setup: its unit written · Enable: started now and with your session · Disable: stopped, not started any more\n"
                "• Start / Stop / Restart: now only · Remove: its unit and its detector removed\n"
                "Its detector is paused while claude-cluster runs (the microphone is the voice's of the core): what is heard now is under Voice", false,
                [](const cluster::Config&) {return std::string("service");}},
            {"services", "svc_web", "Web access", SettingKind::Action, {},
                "the web page and the app of the phone: open / close it now (Open the web access at start: at the start), a new code (the devices "
                "connected stay), the router asked again (Beyond the LAN)", false, [](const cluster::Config&) {return std::string("web");}},
            {"services", "svc_core", "Core", SettingKind::Action, {},
                "the process that runs the sessions, shared by the terminals and windows (Several terminals / windows at once).\n"
                "• Restart: it stops, its terminals / windows start it again, the sessions resumed (their conversations kept)\n"
                "• Stop all: the core and every terminal / window closed", false, [](const cluster::Config&) {return std::string("core");}},
            {"services", "svc_voice", "Voice", SettingKind::Action, {},
                "the voice of the core: listening or not, what it hears now (what is being said, the transcriptions waiting), what it misses", false,
                [](const cluster::Config&) {return std::string("voice");}},
            {"services", "svc_mcp", "MCP servers", SettingKind::Action, {},
                "the MCP servers given to the global session (MCP > Custom MCP servers): Restart the global session to load them again", false,
                [](const cluster::Config&) {return std::string("mcp");}},
            {"services", "svc_outside", "Outside (router / tailscale)", SettingKind::Action, {},
                "how the web access is reached from outside (Web > Beyond the LAN): ask the router again (UPnP), remove its redirection, "
                "log this computer in to tailscale", false, [](const cluster::Config&) {return std::string("outside");}},
        }},
        {"keys", "Keys", {}},
    };

    // the dangerous values: confirmed first (the terminal, the window, the web page, :set!, the MCP)
    static const std::map<std::string, std::function<std::string(const std::string&)>> dangers = {
        {"agent.permission_mode", [](const std::string& v) {return v == "bypassPermissions" ? std::string("every new session runs any tool and "
            "command without asking you (files changed or deleted, commands run)") : std::string();}},
        {"global.permission_mode", [](const std::string& v) {return v == "bypassPermissions" ? std::string("the global session runs anything without "
            "asking you, the sub-sessions too through it") : std::string();}},
        {"agent.allow_bypass", [](const std::string& v) {return v == "true" ? std::string("a session may be switched to bypassPermissions: no "
            "permission asked any more") : std::string();}},
        {"agent.extra_args", [](const std::string& v) {return v.find("dangerously") != std::string::npos ? std::string("these arguments skip the "
            "permissions of claude") : std::string();}},
        {"sessions.bypass_to_global", [](const std::string& v) {return v == "true" ? std::string("the global session decides the commands the "
            "safety check of Claude Code stopped (a removal...), without you") : std::string();}},
        {"web.approve", [](const std::string& v) {return v == "false" ? std::string("a device of the network giving the code is in at once, without "
            "your yes (from outside the LAN: still asked)") : std::string();}},
        {"web.remote", [](const std::string& v) {return v == "router" ? std::string("a port of your router opened to the Internet: anyone may reach "
            "the page (the code, your approval and the bans still apply). Tailscale opens nothing") : std::string();}},
        {"web.shell", [](const std::string& v) {return v == "true" ? std::string("every device connected runs commands on this computer "
            "(!command), from outside the LAN too") : std::string();}},
        {"sessions.rewrite_global_prompt", [](const std::string& v) {return v == "true" ? std::string("the global session rewrites what it obeys "
            "at its start: a prompt injected once (a web page read) could stay for good") : std::string();}},
        {"sessions.rewrite_sessions_prompt", [](const std::string& v) {return v == "true" ? std::string("the global session rewrites what every sub-session obeys "
            "at its start: a prompt injected once (a web page read) could spread to them") : std::string();}},
        {"web.settings", [](const std::string& v) {return cluster::trim(v).empty() ? std::string() : std::string("every device connected to the page changes "
            "these settings of this computer (" + cluster::trim(v) + "): a phone lost or unlocked changes them too");}},
        {"web.https", [](const std::string& v) {return v == "false" ? std::string("the conversation, the code and the cookies go in clear on the "
            "network (anyone on it can read them); no microphone") : std::string();}},
        {"mcp.servers", [](const std::string& v) {return cluster::trim(v).empty() ? std::string() : std::string("an MCP server runs a program (or "
            "reaches a site) with the rights of the global session: only the ones you trust");}},
        // a program run with your rights: the one you mean only (a path, a name of the PATH)
        {"ui.editor", [](const std::string& v) {return cluster::trim(v).empty() ? std::string() : std::string("this program is run with your rights (Ctrl+E)");}},
        {"ui.image_viewer", [](const std::string& v) {return cluster::trim(v).empty() ? std::string() : std::string("this program is run with your rights (a picture opened)");}},
        {"service.terminal", [](const std::string& v) {return cluster::trim(v).empty() ? std::string() : std::string("this program is run with your rights by the service");}},
        {"service.python", [](const std::string& v) {return cluster::trim(v).empty() ? std::string() : std::string("this program runs the wake word detector with your rights");}},
        {"voice.listen_command", [](const std::string& v) {return cluster::trim(v).empty() ? std::string() : std::string("this program hears the microphone, with your rights");}},
        {"voice.say_command", [](const std::string& v) {return cluster::trim(v).empty() ? std::string() : std::string("this program is run with your rights (the voice)");}},
    };
    for (cluster::SettingsPage& page: pages)
        for (cluster::Setting& setting: page.settings)
            if (auto it = dangers.find(setting.section + "." + setting.key); it != dangers.end()) setting.danger = it->second;

    // the engines of the voice: what the value chosen handles
    for (cluster::SettingsPage& page: pages)
        for (cluster::Setting& setting: page.settings)
            if (setting.section == "voice" && (setting.key == "listen_model" || setting.key == "say_engine")) {
                const std::string kind = setting.key == "listen_model" ? "listen" : "say";
                setting.choiceHelp = [kind](const std::string& value) {return cluster::voice_engine_help(kind, value);};
            }

    // pages built from the config itself
    for (const auto &[driver, command]: c.commands) {
        const std::string name = driver;
        std::find_if(pages.begin(), pages.end(), [](const cluster::SettingsPage& page) {return page.section == "commands";})->settings.push_back({"commands", name, name, SettingKind::Text, {}, "program of the " + name + " driver", true,
            [name](const cluster::Config& cf) {return cf.command(name);}, {},
            [name](const std::string& v) {return cluster::trim(v).empty() ? std::string() : "this program is run as the " + name + " agent, with your rights, in your projects";}});
    }
    // the tools of the global session (claude-cluster MCP): each one on / off
    for (cluster::SettingsPage& page: pages) {
        if (page.section != "mcp") continue;
        for (const cluster::Json& tool: cluster::mcp_tools()) {
            const std::string name = tool.value("name", std::string());
            std::string description = tool.value("description", std::string());
            if (const std::size_t end = description.find(". "); end != std::string::npos && end < 220) description = description.substr(0, end + 1);
            if (name == "session_fork" || name == "session_edit")
                description += std::string(" Also needs Sessions > Global session can ") + (name == "session_fork" ? "fork." : "edit.");
            page.settings.push_back({"mcp_tools", name, name, SettingKind::Bool, {}, description + " Off: refused at once, out of its tool list at its "
                "next start (mcp__claude-cluster__" + name + ")", false,
                [name](const cluster::Config& cf) {return cf.mcpTool(name) ? std::string("true") : std::string("false");}});
        }
    }
    // plugins / skills of repositories: the links, then a tick per plugin or skill found (and one for all of a repository)
    {
        std::string state;
        const std::vector<cluster::PluginRepo> repos = cluster::plugins_index();
        for (const cluster::PluginRepo& repo: repos)
            state += "\n• " + repo.url + ": " + (repo.error.empty() ? std::to_string(repo.items.size()) + (repo.marketplace.empty() ? " skill(s)" : " plugin(s) of the marketplace "
                + repo.marketplace) : repo.error);
        cluster::SettingsPage page{"plugins", "Plugins", {
            {"plugins", "repos", "Sources", SettingKind::List, {}, "where plugins / skills are offered below from, comma separated: a git repository "
                "(GitHub owner/repo, an https or git URL), a folder of this computer (~/... or /...), a .zip (a path; dropped on the terminal: its path).\n• a Claude Code marketplace (.claude-plugin/marketplace.json): its plugins (claude plugin install)\n"
                "• else its skills (the folders with a SKILL.md): linked in ~/.claude/skills\nA repository cloned / a .zip unpacked in the data folder when the "
                "list changes (pulled at each start of the core); tick what to install: done at the save, for the sessions started after it. The palette, "
                "Plugins: the browser (add with a path completed, on / off, install / remove, information, the tree of its files and their preview). Code of these repositories runs as you: only "
                "repositories you trust" + (state.empty() ? std::string() : "\nListed:" + state), false,
                [](const cluster::Config& cf) {return join_(cf.pluginRepos);}},
        }};
        page.settings.front().danger = [](const std::string& v) {
            return cluster::trim(v).empty() ? std::string() : "the plugins / skills of these sources run as you in every session (their commands, hooks, MCP "
                "servers): only sources you trust";
        };
        for (const cluster::PluginRepo& repo: repos) {
            if (repo.items.empty()) continue;
            const std::string all = repo.slug + ":*";
            page.settings.push_back({"plugins.select", "\"" + all + "\"", "All of " + repo.slug, SettingKind::Bool, {}, "every plugin / skill of " + repo.url
                + " (the new ones too)", false, [all](const cluster::Config& cf) {return cf.pluginSelect.contains(all) && cf.pluginSelect.at(all) ? std::string("true")
                    : std::string("false");}});
            for (const cluster::PluginItem& item: repo.items) {
                const std::string id = item.id;
                page.settings.push_back({"plugins.select", "\"" + id + "\"", "  " + (item.kind == "skill" ? "/" + repo.slug + ":" + item.name : item.name) + " (" + item.kind + (item.installed ? item.enabled ? ", installed" : ", off" : "") + ")",
                    SettingKind::Bool, {}, (item.description.empty() ? std::string("no description") : item.description) + "\nfrom " + repo.url, false,
                    [id](const cluster::Config& cf) {return cf.pluginSelect.contains(id) && cf.pluginSelect.at(id) ? std::string("true") : std::string("false");}});
                if (item.installed) // installed: turned off / on (a plugin disabled, a skill unlinked), kept installed
                    page.settings.push_back({"plugins.off", "\"" + id + "\"", "    " + item.name + ": off", SettingKind::Bool, {}, "turned off: kept installed, not "
                        "loaded by the sessions started after it", false,
                        [id](const cluster::Config& cf) {return cf.pluginOff.contains(id) && cf.pluginOff.at(id) ? std::string("true") : std::string("false");}});
            }
        }
        const auto mcp = std::find_if(pages.begin(), pages.end(), [](const cluster::SettingsPage& p) {return p.section == "mcp";});
        pages.insert(mcp == pages.end() ? pages.end() : mcp + 1, page);
    }
    // accounts & limits: the providers (store / remove a key, log in) and what the sessions may spend together
    {
        const std::function<std::string(const std::string&)> state = [](const std::string& name) {
            for (const cluster::Json& p: accounts_status_get().value("providers", cluster::Json::array()))
                if (p.value("name", std::string()) == name)
                    return std::string(p.value("ready", false) ? "ready" : "not ready") + " · " + p.value("detail", std::string());
            return std::string("(the core has not answered yet)");
        };
        const std::function<std::string(const std::string&, double)> spent = [](const std::string& window, const double limit) {
            const double now = accounts_status_get().value("spent", cluster::Json::object()).value(window, 0.0);
            return cluster::human_cost(now) + (limit > 0 ? " of " + cluster::human_cost(limit) + " (" + std::to_string(static_cast<int>(now / limit * 100.0 + 0.5)) + "%)" : std::string(" (no limit)"));
        };
        cluster::SettingsPage page{"accounts", "Accounts & limits", {}};
        for (const cluster::Provider& p: cluster::Auth(c).providers()) {
            const std::string name = p.name;
            if (p.auth == cluster::AuthKind::None) continue;
            page.settings.push_back({"accounts", "account_" + name, name + " (" + p.driver + ")", SettingKind::Action, {}, p.auth == cluster::AuthKind::Key
                ? "an API key for " + name + ": Enter stores / replaces it (asked without echo; the keyring of the system, else credentials.json 0600; never in config.toml). "
                  "Use it: [agent] provider = \"" + name + "\", or /model, or a profile's backend"
                : name + " logs in with its own tool (" + cluster::one_line(cluster::Auth(c).loginCommand(p).empty() ? std::string() : cluster::Auth(c).loginCommand(p)[0], 40)
                  + "): Enter runs it in a terminal; claude-cluster stores nothing", false,
                [name, state](const cluster::Config&) {return state(name);}});
            if (p.auth == cluster::AuthKind::Key)
                page.settings.push_back({"accounts", "account_rm_" + name, "  remove the key of " + name, SettingKind::Action, {}, "Enter removes the stored key of " + name, false,
                    [](const cluster::Config&) {return std::string("remove");}});
        }
        page.settings.push_back({"limits", "daily_usd", "Limit per day (USD)", SettingKind::Float, {}, "what all the sessions may spend since the local midnight; 0: none. Reached: the prompts "
            "are refused (Block) and you are notified; warned at the share of Warn at", false, [](const cluster::Config& cf) {return number_(cf.limits.dailyUsd);}});
        page.settings.push_back({"accounts", "spent_day", "  spent today", SettingKind::Action, {}, "the spending of today (the usage ledger, the scope below)", false,
            [spent](const cluster::Config& cf) {return spent("day", cf.limits.dailyUsd);}});
        page.settings.push_back({"limits", "weekly_usd", "Limit per week (USD)", SettingKind::Float, {}, "the last 7 days; 0: none", false,
            [](const cluster::Config& cf) {return number_(cf.limits.weeklyUsd);}});
        page.settings.push_back({"accounts", "spent_week", "  spent this week", SettingKind::Action, {}, "the last 7 days", false,
            [spent](const cluster::Config& cf) {return spent("week", cf.limits.weeklyUsd);}});
        page.settings.push_back({"limits", "monthly_usd", "Limit per month (USD)", SettingKind::Float, {}, "since the 1st of the local month; 0: none", false,
            [](const cluster::Config& cf) {return number_(cf.limits.monthlyUsd);}});
        page.settings.push_back({"accounts", "spent_month", "  spent this month", SettingKind::Action, {}, "since the 1st of the month", false,
            [spent](const cluster::Config& cf) {return spent("month", cf.limits.monthlyUsd);}});
        page.settings.push_back({"limits", "scope", "Counted", SettingKind::Choice, {"keys", "all"}, "what the limits count.\n• keys: only the turns billed through an API key "
            "(openrouter, anthropic-api...): a subscription costs nothing more per turn\n• all: every turn, the notional cost of a subscription too (the price of its tokens)", false,
            [](const cluster::Config& cf) {return cf.limits.scope;}});
        page.settings.push_back({"limits", "block", "Block at the limit", SettingKind::Bool, {}, "true: a limit reached refuses the new prompts (send --force passes); false: only warned",
            false, [](const cluster::Config& cf) {return cf.limits.block ? std::string("true") : std::string("false");}});
        page.settings.push_back({"limits", "warn_percent", "Warn at (%)", SettingKind::Int, {}, "warned (a notice and a notification) at this share of a limit; 0: only when it is reached",
            false, [](const cluster::Config& cf) {return std::to_string(cf.limits.warnPercent);}});
        page.settings.push_back({"sessions", "budget_usd", "Budget per session (USD)", SettingKind::Float, {}, "0: none; reached: the session refuses new prompts (also in Sessions)",
            false, [](const cluster::Config& cf) {return number_(cf.budget);}});
        const auto usage = std::find_if(pages.begin(), pages.end(), [](const cluster::SettingsPage& p) {return p.section == "usage";});
        pages.insert(usage == pages.end() ? pages.end() : usage + 1, page);
    }
    // the model / thinking policy of the global session
    {
        const std::vector<std::string> policyModes = {"free", "advise", "force"};
        const std::function<std::string(const std::string&)> modeHelp = [](const std::string& value) {
            return value == "free" ? "no default: the global session chooses by the difficulty of each task"
                : value == "force" ? "the default is the only one the global session may use (anything else is refused)"
                : "the default is the start: the global session may deviate for a reason (cheaper for easy tasks, stronger for hard ones)";
        };
        cluster::SettingsPage page{"policy", "Model policy", {}};
        page.settings.push_back({"global", "model", "Model of the global session", SettingKind::Text, {}, "the model the global session itself runs on (empty: the default of "
            "the agent). Also in Global session", true, [](const cluster::Config& cf) {return cf.globalModel;}});
        page.settings.push_back({"global", "effort", "Thinking of the global session", SettingKind::Choice, efforts, "its thinking effort (empty: the default of the agent). "
            "Also in Global session", true, [](const cluster::Config& cf) {return cf.globalEffort;}});
        page.settings.push_back({"policy", "default_model", "Default model of the sub-sessions", SettingKind::Text, {}, "the model the sub-sessions start from (opus, sonnet, haiku, "
            "an id...); empty: none. The global session chooses the model of each sub-session by the difficulty of its task, from this one", false,
            [](const cluster::Config& cf) {return cf.policy.defaultModel;}});
        page.settings.back().choiceHelp = nullptr;
        page.settings.push_back({"policy", "model_mode", "Model: how the default applies", SettingKind::Choice, policyModes, "free: no default | advise: the default is the start, "
            "deviate for a reason | force: nothing else is allowed (refused, with the reason)", false, [](const cluster::Config& cf) {return cf.policy.modelMode;}, modeHelp});
        page.settings.push_back({"policy", "default_effort", "Default thinking of the sub-sessions", SettingKind::Choice, efforts, "the thinking effort they start from (empty: the "
            "default of the agent)", false, [](const cluster::Config& cf) {return cf.policy.defaultEffort;}});
        page.settings.push_back({"policy", "effort_mode", "Thinking: how the default applies", SettingKind::Choice, policyModes, "free | advise | force, like the model", false,
            [](const cluster::Config& cf) {return cf.policy.effortMode;}, modeHelp});
        page.settings.push_back({"policy", "allowed_models", "Allowed models", SettingKind::List, {}, "the only models the global session may choose, comma separated (a name or a "
            "part: haiku, sonnet); empty: every one", false, [](const cluster::Config& cf) {return join_(cf.policy.allowedModels);}});
        page.settings.push_back({"policy", "allowed_efforts", "Allowed thinking efforts", SettingKind::List, {}, "the only efforts it may choose, comma separated; empty: every one",
            false, [](const cluster::Config& cf) {return join_(cf.policy.allowedEfforts);}});
        page.settings.push_back({"policy", "ask_costly", "Ask before a costly choice", SettingKind::Bool, {}, "true: the global session asks you (a popup: Allow / Deny, on every "
            "interface and a notification) before running a session on a costly model or thinking effort (below); false: it chooses alone inside the limits above", false,
            [](const cluster::Config& cf) {return cf.policy.askCostly ? std::string("true") : std::string("false");}});
        page.settings.push_back({"policy", "costly_models", "Costly models", SettingKind::List, {}, "models that cost as much or more than these (a name or a part: opus, fable)", false,
            [](const cluster::Config& cf) {return join_(cf.policy.costlyModels);}});
        page.settings.push_back({"policy", "costly_efforts", "Costly thinking efforts", SettingKind::List, {}, "efforts of this level or above (xhigh, max)", false,
            [](const cluster::Config& cf) {return join_(cf.policy.costlyEfforts);}});
        const auto accounts = std::find_if(pages.begin(), pages.end(), [](const cluster::SettingsPage& p) {return p.section == "accounts";});
        pages.insert(accounts == pages.end() ? pages.end() : accounts + 1, page);
    }
    for (const auto &[action, key]: c.keys) {
        const std::string name = action;
        pages.back().settings.push_back({"keys", name, name, SettingKind::Text, {}, "Ctrl+<letter>, Ctrl+Shift+<letter>, Alt+<letter>, F1..F12, Tab, Escape; alternatives separated by |", false,
            [name](const cluster::Config& cf) {return cf.keys.contains(name) ? cf.keys.at(name) : std::string();}});
    }
    pages.push_back(cluster::orchestrator_settings_page()); // Orchestrator-Settings.cpp
    return pages;
}

/* editing */
_cold std::string cluster::setting_next(const cluster::Setting& setting, const std::string& value, const int step)
{
    switch (setting.kind) {
        case cluster::SettingKind::Bool: return value == "true" ? "false" : "true";
        case cluster::SettingKind::Choice: {
            if (setting.choices.empty()) return value;
            auto it = std::find(setting.choices.begin(), setting.choices.end(), value);
            const long size = static_cast<long>(setting.choices.size());
            const long index = it == setting.choices.end() ? 0 : ((it - setting.choices.begin()) + step % size + size) % size;
            return setting.choices[static_cast<std::size_t>(index)];
        }
        case cluster::SettingKind::Int:
            try {
                return std::to_string(std::max(0L, std::stol(value) + step));
            } catch (const std::exception&) {
                return value;
            }
        case cluster::SettingKind::Float:
            try {
                return number_(std::max(0.0, std::stod(value) + step * 0.5));
            } catch (const std::exception&) {
                return value;
            }
        default: return value;
    }
}

_cold std::string cluster::setting_check(const cluster::Setting& setting, const std::string& value)
{
    try {
        std::size_t used = 0;
        if (setting.kind == cluster::SettingKind::Int) {
            const long number = std::stol(value, &used);
            if (used != cluster::trim(value).size()) return "not an integer";
            if (setting.key.ends_with("port") && (number < 0 || number > 65535)) return "a port: 0 to 65535";
            if (number < 0) return "0 or more";
        } else if (setting.kind == cluster::SettingKind::Float) {
            (void)std::stod(value, &used);
            if (used != cluster::trim(value).size()) return "not a number";
        }
    } catch (const std::exception&) {
        return "not a number";
    }
    if (setting.kind == cluster::SettingKind::Choice && std::find(setting.choices.begin(), setting.choices.end(), value) == setting.choices.end())
        return "one of the choices only";
    return "";
}

_cold bool cluster::settings_set(const cluster::Config& config, const std::string& section, const std::string& key, const std::string& value)
{
    // one option of the settings page by its section / key: written; true when applied at the next start only
    for (const cluster::SettingsPage& page: cluster::settings_pages(config))
        for (const cluster::Setting& setting: page.settings)
            if (setting.section == section && setting.key == key && setting.kind != cluster::SettingKind::Action) {
                cluster::settings_write(config, {{{section, key}, {setting, value}}});
                return setting.restart;
            }
    throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, "no option " + section + "." + key);
}

_cold std::vector<cluster::SettingHit> cluster::settings_search(const std::vector<cluster::SettingsPage>& pages, const std::string& query)
{
    // the name: its label (every word typed close to one of its words), its key, section.key; the description: the same,
    // 6 further (a word of a description never before a name close to it)
    constexpr float DESCRIPTION = 6.0F;
    std::vector<cluster::SettingHit> hits;
    if (cluster::trim(query).empty()) return hits;
    for (std::size_t p = 0; p < pages.size(); ++p)
        for (std::size_t i = 0; i < pages[p].settings.size(); ++i) {
            const cluster::Setting& s = pages[p].settings[i];
            std::optional<float> name = cluster::match_words(query, s.label);
            for (const std::string& key: {s.key, s.section + "." + s.key}) {
                if (const std::optional<float> k = cluster::match_score(cluster::trim(query), key); k && (!name || *k < *name)) name = k;
                std::string spaced = key; // "shell_context" found by "shell context"
                std::replace_if(spaced.begin(), spaced.end(), [](const char c) {return c == '_' || c == '.';}, ' ');
                if (const std::optional<float> k = cluster::match_words(query, spaced); k && (!name || *k < *name)) name = k;
            }
            const std::optional<float> about = cluster::match_words(query, s.description);
            if (!name && !about) continue;
            const float described = about ? *about + DESCRIPTION : 1e9F;
            hits.push_back({p, i, name ? std::min(*name, described) : described, name && *name <= described});
        }
    std::stable_sort(hits.begin(), hits.end(), [](const cluster::SettingHit& a, const cluster::SettingHit& b) {return a.score < b.score;});
    if (hits.size() > 50) hits.resize(50);
    return hits;
}

_cold void cluster::settings_write(const cluster::Config& config,
    const std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>>& changes)
{
    // Line edit of config.toml: the value of "key = value" in its [section] is replaced (comment kept), a missing key
    // is added at the end of its section, a missing section at the end of the file
    std::vector<std::string> lines;
    {
        std::ifstream in(config.path());
        std::string line;
        while (std::getline(in, line))
            lines.push_back(line);
    }
    for (const auto &[where, change]: changes) {
        const auto &[section, key] = where;
        const std::string value = encode_(change.first, change.second);
        std::size_t start = lines.size();
        std::size_t end = lines.size();
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const std::string line = cluster::trim(lines[i]);
            if (line == "[" + section + "]") start = i;
            else if (start != lines.size() && i > start && line.starts_with("[")) {
                end = i;
                break;
            }
        }
        if (start == lines.size()) {
            lines.push_back("");
            lines.push_back("[" + section + "]");
            lines.push_back(key + " = " + value);
            continue;
        }
        bool done = false;
        for (std::size_t i = start + 1; i < end && !done; ++i) {
            const std::string line = cluster::trim(lines[i]);
            if (line.starts_with("#") || line.find('=') == std::string::npos) continue;
            if (cluster::trim(line.substr(0, line.find('='))) != key) continue;
            const std::size_t comment = comment_start_(lines[i]);
            const std::string before = key + " = " + value;
            if (comment == std::string::npos) lines[i] = before;
            else lines[i] = before + std::string(before.size() < comment ? comment - before.size() : 1, ' ') + lines[i].substr(comment);
            done = true;
        }
        if (!done) {
            std::size_t at = end;
            while (at > start + 1 && cluster::trim(lines[at - 1]).empty()) --at;
            lines.insert(lines.begin() + static_cast<long>(at), key + " = " + value);
        }
    }
    const std::filesystem::path tmp = config.path().string() + ".tmp";
    {
        std::ofstream out(tmp);
        if (!out) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, tmp.string());
        for (const std::string& line: lines)
            out << line << "\n";
    }
    std::error_code error;
    std::filesystem::rename(tmp, config.path(), error);
    if (error) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, config.path().string() + ": " + error.message());
}

/* the accounts */
static std::mutex accounts_mutex_;     // NOLINT: the cache of the core's answer
static cluster::Json accounts_json_;   // NOLINT

_cold void cluster::accounts_status_set(const cluster::Json& status)
{
    std::lock_guard<std::mutex> lock(accounts_mutex_);
    accounts_json_ = status;
}

_cold cluster::Json cluster::accounts_status_get(void)
{
    std::lock_guard<std::mutex> lock(accounts_mutex_);
    return accounts_json_.is_object() ? accounts_json_ : cluster::Json::object(); // nothing yet: no state
}

/* the keys */
_cold static const std::vector<std::string>& key_names_(void)
{
    static const std::vector<std::string> names = {"Enter", "Tab", "Escape", "Space", "Backspace", "Delete", "Insert", "Home", "End", "PageUp", "PageDown", "Up", "Down", "Left", "Right",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"};
    return names;
}

_cold static bool key_valid_(const std::string& key)
{
    // modifiers (Ctrl, Alt, Shift, each once) joined by +, then a key: one character, a letter / digit / sign, or a name of the list
    const std::vector<std::string> parts = cluster::split(key, '+');
    if (parts.empty()) return false;
    std::set<std::string> used;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        if ((parts[i] != "Ctrl" && parts[i] != "Alt" && parts[i] != "Shift") || !used.insert(parts[i]).second) return false;
    }
    const std::string& last = parts.back();
    if (last.empty() && parts.size() >= 2) return true; // "Ctrl++": the plus itself
    if (last.size() == 1 && static_cast<unsigned char>(last[0]) > 32) return true;
    return std::find(key_names_().begin(), key_names_().end(), last) != key_names_().end();
}

_cold std::vector<std::string> cluster::setting_invalid_parts(const cluster::Setting& setting, const std::string& value)
{
    std::vector<std::string> bad;
    if (setting.section != "keys") return bad;
    for (const std::string& part: cluster::split(value, '|')) {
        const std::string key = cluster::trim(part);
        if (!key.empty() && !key_valid_(key)) bad.push_back(key);
    }
    return bad;
}

/* the suggestions of a text value */
_cold std::vector<std::string> cluster::setting_suggestions(const cluster::Config& config, const cluster::Setting& setting, const std::string& typed, const bool local)
{
    if (setting.kind != SettingKind::Text && setting.kind != SettingKind::List) return {};
    // a list: the item being typed is the one completed, the others kept
    std::string head;
    std::string word = typed;
    if (setting.kind == SettingKind::List) {
        const std::size_t comma = typed.rfind(',');
        if (comma != std::string::npos) {
            head = typed.substr(0, comma + 1) + (typed.size() > comma + 1 && typed[comma + 1] == ' ' ? " " : "");
            word = cluster::trim(typed.substr(comma + 1));
        }
    }
    if (setting.section == "keys") { // the last alternative: its modifiers kept, then the modifiers left and the keys
        const std::size_t bar = typed.rfind('|');
        const std::string before = bar == std::string::npos ? std::string() : typed.substr(0, bar + 1);
        const std::string alternative = cluster::trim(bar == std::string::npos ? typed : typed.substr(bar + 1));
        const std::size_t plus = alternative.rfind('+');
        const std::string modifiers = plus == std::string::npos ? std::string() : alternative.substr(0, plus + 1);
        const std::string last = plus == std::string::npos ? alternative : alternative.substr(plus + 1);
        std::vector<std::string> pool;
        for (const char* modifier: {"Ctrl", "Alt", "Shift"})
            if (modifiers.find(std::string(modifier) + "+") == std::string::npos) pool.push_back(modifier);
        for (const std::string& name: key_names_())
            pool.push_back(name);
        std::vector<std::pair<float, std::string>> found;
        for (const std::string& name: pool) {
            if (last.empty()) found.emplace_back(0.0F, name);
            else if (const std::optional<float> score = cluster::match_score(last, name)) found.emplace_back(*score, name);
        }
        std::stable_sort(found.begin(), found.end(), [](const std::pair<float, std::string>& a, const std::pair<float, std::string>& b) {return a.first < b.first;});
        std::vector<std::string> out;
        for (std::size_t i = 0; i < found.size() && out.size() < 8; ++i) {
            const bool modifier = found[i].second == "Ctrl" || found[i].second == "Alt" || found[i].second == "Shift";
            const std::string whole = before + (before.empty() || before.back() != '|' ? "" : "") + modifiers + found[i].second + (modifier ? "+" : "");
            if (whole != typed) out.push_back(whole);
        }
        return out;
    }
    const std::string key = setting.key;
    const std::string lowered = [&key]() {std::string s = key; std::transform(s.begin(), s.end(), s.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));}); return s;}();
    const std::function<bool(const char*)> ends = [&lowered](const char* tail) {return lowered.size() >= std::string(tail).size() && lowered.compare(lowered.size() - std::string(tail).size(), std::string::npos, tail) == 0;};
    std::vector<std::string> pool;
    bool names = true; // ranked by the matching; a path or a program: the entries of the folder / PATH starting with the word
    if (ends("cwd") || ends("dir") || ends("folder") || ends("path") || ends("file") || ends("editor") || setting.section == "commands" || ends("command") || ends("terminal")) {
        if (!local) return {};
        const bool program = setting.section == "commands" || ends("command") || ends("editor") || ends("terminal");
        const bool pathy = word.find('/') != std::string::npos || word.starts_with("~") || word.starts_with(".");
        if (!program || pathy) { // a path: the folder typed, its entries
            std::string dirPart = word.substr(0, word.rfind('/') == std::string::npos ? 0 : word.rfind('/') + 1);
            const std::string base = word.substr(dirPart.size());
            std::error_code error;
            const std::filesystem::path folder = dirPart.empty() ? std::filesystem::path(".") : cluster::expand_home(dirPart);
            for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(folder, std::filesystem::directory_options::skip_permission_denied, error)) {
                const std::string name = entry.path().filename().string();
                if (!base.starts_with(".") && name.starts_with(".")) continue;
                if (name.compare(0, base.size(), base) != 0) continue;
                const bool directory = entry.is_directory(error);
                if (!directory && (ends("cwd") || ends("dir") || ends("folder"))) continue; // a folder wanted
                pool.push_back(dirPart + name + (directory ? "/" : ""));
                if (pool.size() >= 200) break;
            }
            std::sort(pool.begin(), pool.end());
            names = false;
        } else { // a program: the executables of the PATH
            const char* path = std::getenv("PATH");
            std::set<std::string> seen;
            for (const std::string& dir: cluster::split(path != nullptr ? path : "", ':')) {
                std::error_code error;
                for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, error))
                    if (seen.size() < 6000 && entry.is_regular_file(error) && ::access(entry.path().c_str(), X_OK) == 0) seen.insert(entry.path().filename().string());
            }
            pool.assign(seen.begin(), seen.end());
        }
    } else if (lowered.find("model") != std::string::npos) {
        pool = {"opus", "sonnet", "haiku", "fable", "claude-opus-5-5", "claude-sonnet-5-5", "claude-haiku-5-5", "claude-fable-5-1"};
    } else if (lowered.find("provider") != std::string::npos || lowered.find("backend") != std::string::npos) {
        pool = {"claude"}; // the drivers, then every provider known: the logins and the API ones (anthropic-api, openrouter, deepseek...), the own ones of config.toml too
        for (const cluster::Provider& provider: cluster::Auth(config).providers())
            if (provider.driver != "voice" && std::find(pool.begin(), pool.end(), provider.name) == pool.end()) pool.push_back(provider.name);
    } else if (ends("effort") || ends("efforts")) {
        pool = {"low", "medium", "high", "xhigh", "max"};
    } else if (ends("shell") || ends("frontend")) {
        pool = {"tty", "gui", "web"};
    }
    if (pool.empty()) return {};
    std::vector<std::pair<float, std::string>> ranked;
    for (const std::string& item: pool) {
        if (word.empty()) {
            ranked.emplace_back(0.0f, item);
        } else if (!names) {
            ranked.emplace_back(0.0f, item);
        } else if (const std::optional<float> score = cluster::match_score(word, item)) {
            ranked.emplace_back(*score, item);
        }
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const std::pair<float, std::string>& a, const std::pair<float, std::string>& b) {return a.first < b.first;});
    std::vector<std::string> out;
    for (std::size_t i = 0; i < ranked.size() && i < 8; ++i)
        if (head + ranked[i].second != typed) out.push_back(head + ranked[i].second);
    return out;
}

/* the protected settings */
_cold bool cluster::setting_protected(const std::string& section, const std::string& key)
{
    // a shell / a program run as the user, the web opened wider, the permissions of the agents, the tools of the global session
    static const std::set<std::string> keys = {"web.shell", "web.settings", "web.tty_port", "web.tty_remote_port", "usage.transcripts", "web.remote", "web.remote_host", "web.remote_port", "web.approve", "web.https", "web.enabled", "web.port",
        "agent.permission_mode", "agent.allow_bypass", "agent.extra_args", "agent.work_method", "agent.orchestration", "agent.work_method_text", "agent.work_method_detect", "agent.work_method_every", "global.permission_mode", "sessions.bypass_to_global", "sessions.global_prompts", "sessions.rewrite_global_prompt", "sessions.rewrite_sessions_prompt", "global.answers", "global.fork", "global.edit",
        "mcp.servers", "orchestrator.sub_add", "orchestrator.sub_kill_own", "orchestrator.sub_see_others", "orchestrator.kill_on_oom", "ui.editor", "ui.image_viewer", "service.terminal", "service.python", "service.frontend", "voice.listen_command", "voice.say_command"};
    return section == "commands" || section == "mcp_tools" || section == "plugins" || section.starts_with("plugins.") || section == "limits" || section == "policy"
        || (section == "sessions" && key == "budget_usd") || keys.contains(section + "." + key);
}

_cold std::vector<std::string> cluster::protected_changes(const std::string& oldToml, const std::string& newToml)
{
    // the protected settings whose value differs between two config.toml texts (unparsable: everything counts as changed)
    std::vector<std::string> changed;
    toml::table before;
    toml::table after;
    try {
        before = toml::parse(oldToml);
        after = toml::parse(newToml);
    } catch (const toml::parse_error&) {
        return {"config.toml (unreadable)"};
    }
    const std::function<std::string(const toml::table&, const std::string&, const std::string&)> value = [](const toml::table& table, const std::string& section, const std::string& key) {
        const toml::node* node = table.at_path(section + "." + key).node();
        if (node == nullptr) return std::string();
        std::ostringstream out;
        out << toml::node_view<const toml::node>(node);
        return out.str();
    };
    std::set<std::string> keys;
    for (const char* section: {"web", "agent", "global", "sessions", "mcp", "ui", "service", "voice", "usage", "limits", "policy", "orchestrator"})
        for (const toml::table* table: {before.at_path(section).as_table(), after.at_path(section).as_table()})
            if (table != nullptr)
                for (const auto &[key, node]: *table)
                    if (cluster::setting_protected(section, std::string(key.str()))) keys.insert(std::string(section) + "." + std::string(key.str()));
    for (const char* section: {"commands", "mcp_tools"})
        for (const toml::table* table: {before.at_path(section).as_table(), after.at_path(section).as_table()})
            if (table != nullptr)
                for (const auto &[key, node]: *table)
                    keys.insert(std::string(section) + "." + std::string(key.str()));
    {
        // the plugins: code of repositories run as the user (their links, their ticks)
        std::ostringstream a;
        std::ostringstream b;
        if (const toml::table* t = before.at_path("plugins").as_table()) a << *t;
        if (const toml::table* t = after.at_path("plugins").as_table()) b << *t;
        if (a.str() != b.str()) changed.push_back("plugins");
    }
    for (const std::string& path: keys) {
        const std::size_t dot = path.find('.');
        if (value(before, path.substr(0, dot), path.substr(dot + 1)) != value(after, path.substr(0, dot), path.substr(dot + 1))) changed.push_back(path);
    }
    return changed;
}

_cold bool cluster::web_may(const cluster::Config& config, const std::string& section)
{
    // a device of the web page: the sections it may change ([web] settings: "all", or the sections)
    for (const std::string& s: config.web.settings)
        if (s == "all" || s == section) return true;
    return false;
}
