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
#include "cluster/VoiceEngines.hpp"
#include "cluster/Vocabulary.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Types.hpp"
#include "cluster/Mcp.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

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
            {"ui", "frontend", "Front-end", SettingKind::Choice, {"auto", "tty", "gui"}, "auto: terminal when in a terminal, else a window (--tty / --gui win)", true,
                [](const cluster::Config& cf) {return cf.frontend;}},
            {"ui", "layout", "Layout", SettingKind::Choice, {"list", "grid", "tabs"}, "list + active panel, every session in a grid, or tabs", false,
                [](const cluster::Config& cf) {return cf.layout;}},
            {"ui", "style", "Style", SettingKind::Choice, styles, "colors of the interface (own ones: [styles.<name>] in the file)", false,
                [](const cluster::Config& cf) {return cf.style;}},
            {"ui", "panels", "Panels of a new session", SettingKind::List, {}, "skills, tokens, context, cost, git, diff, tools, files", false,
                [](const cluster::Config& cf) {return join_(cf.panels);}},
            {"ui", "restore", "Previous sessions at start", SettingKind::Choice, {"ask", "always", "never"}, "ask (like Firefox), always restore, never (to the trash)", true,
                [](const cluster::Config& cf) {return cf.restore;}},
            {"ui", "notify", "Desktop notifications", SettingKind::Bool, {}, "all of them (off: none, the global session's too)", false,
                [](const cluster::Config& cf) {return cf.notify ? std::string("true") : std::string("false");}},
            {"ui", "notify_global", "  when the global session answers", SettingKind::Bool, {}, "the end of its answers (its first words)", false,
                [](const cluster::Config& cf) {return cf.notifyGlobal ? std::string("true") : std::string("false");}},
            {"ui", "notify_done", "  when a session finishes", SettingKind::Bool, {}, "the end of a turn of a sub-session", false,
                [](const cluster::Config& cf) {return cf.notifyDone ? std::string("true") : std::string("false");}},
            {"ui", "notify_permission", "  when a permission waits", SettingKind::Bool, {}, "a session waits for your yes", false,
                [](const cluster::Config& cf) {return cf.notifyPermission ? std::string("true") : std::string("false");}},
            {"ui", "notify_sound", "  with a sound", SettingKind::Bool, {}, "a short sound of the desktop theme, one per kind (done, question, error)", false,
                [](const cluster::Config& cf) {return cf.notifySound ? std::string("true") : std::string("false");}},
            {"ui", "vim", "Vim mode of the prompt", SettingKind::Bool, {}, "Esc: Normal mode (hjkl w b e 0 $ x dd dw cw yy p u v V...), i / a / o: Insert", false,
                [](const cluster::Config& cf) {return cf.vim ? std::string("true") : std::string("false");}},
            {"ui", "paste_lines", "Paste collapsed above (lines)", SettingKind::Int, {}, "a longer paste is one [Pasted text #N +L lines] in the prompt (0: never)", false,
                [](const cluster::Config& cf) {return std::to_string(cf.pasteLines);}},
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
        {"agent", "Agent", {
            {"agent", "default", "Backend of new sessions", SettingKind::Text, {}, "<provider>[/<model>]: claude, claude/opus, ollama/<model>, openai/<model>, opencode/<p>/<m>, codex", false,
                [](const cluster::Config& cf) {return cf.backend;}},
            {"agent", "permission_mode", "Permission mode", SettingKind::Choice, modes, "mode of the new sessions (empty: the default of the agent)", false,
                [](const cluster::Config& cf) {return cf.mode;}},
            {"agent", "effort", "Thinking effort", SettingKind::Choice, efforts, "of the new sessions (empty: the default of the agent; per session: /effort, the palette)", false,
                [](const cluster::Config& cf) {return cf.effort;}},
            {"agent", "allow_bypass", "bypassPermissions possible", SettingKind::Bool, {}, "claude launched with --allow-dangerously-skip-permissions (needed to switch to bypass)", true,
                [](const cluster::Config& cf) {return cf.allowBypass ? std::string("true") : std::string("false");}},
            {"agent", "extra_args", "Extra arguments (claude)", SettingKind::List, {}, "added to every claude session, comma separated", true,
                [](const cluster::Config& cf) {return join_(cf.extraArgs);}},
        }},
        {"commands", "Commands", {}},
        {"global", "Global session", {
            {"global", "enabled", "Enabled", SettingKind::Bool, {}, "the session that manages the others (MCP)", true,
                [](const cluster::Config& cf) {return cf.globalEnabled ? std::string("true") : std::string("false");}},
            {"global", "backend", "Backend", SettingKind::Text, {}, "a claude-driver provider (claude, ollama, anthropic-api, openrouter...)", true,
                [](const cluster::Config& cf) {return cf.globalBackend;}},
            {"global", "model", "Model", SettingKind::Text, {}, "empty: the default model of the backend", true,
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
            {"sessions", "trash_days", "Trash kept (days)", SettingKind::Int, {}, "a closed session stays restorable this long", false,
                [](const cluster::Config& cf) {return std::to_string(cf.trashDays);}},
            {"sessions", "max_parallel", "Tasks in parallel", SettingKind::Int, {}, "task queue: sessions working at the same time", false,
                [](const cluster::Config& cf) {return std::to_string(cf.maxParallel);}},
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
        {"service", "Service", {
            {"service", "frontend", "Opens", SettingKind::Choice, {"auto", "gui", "tty"}, "auto: a window, else / on its failure a terminal; gui: a window (a terminal on its failure); tty: a terminal", false,
                [](const cluster::Config& cf) {return cf.service.frontend;}},
            {"service", "terminal", "Terminal", SettingKind::Text, {}, "with its option to run a command (kitty, alacritty -e...), empty: the first installed", false,
                [](const cluster::Config& cf) {return cf.service.terminal;}},
            {"service", "voice_mode", "Voice of the opened one", SettingKind::Choice, {"auto", "wake", "push"}, "auto: every sentence, at once", false,
                [](const cluster::Config& cf) {return cf.service.voiceMode;}},
            {"service", "wake_word", "Wake phrase", SettingKind::Text, {}, "empty: the one of the voice; variants separated by commas", false,
                [](const cluster::Config& cf) {return cf.service.wakeWord;}},
            {"service", "source", "Microphone", SettingKind::Text, {}, "PulseAudio / PipeWire source, empty: the default one", false,
                [](const cluster::Config& cf) {return cf.service.source;}},
        }},
        {"web", "Web", {
            {"web", "enabled", "Web access", SettingKind::Bool, {}, "a browser of this network (same LAN, private address) drives claude-cluster after "
                "the 6-digit code shown at the top; one browser at a time, the others refused while it is there (30 s without it: free); "
                "plain HTTP: on a network you trust only. Open / closed live (palette: Web access)", false,
                [](const cluster::Config& cf) {return cf.web.enabled ? std::string("true") : std::string("false");}},
            {"web", "port", "Port", SettingKind::Int, {}, "the address shown: https://<this computer on the LAN>:<port>", false,
                [](const cluster::Config& cf) {return std::to_string(cf.web.port);}},
            {"web", "https", "HTTPS", SettingKind::Bool, {}, "TLS with a certificate of its own (made once, the browser asks to accept it the first time): "
                "the conversation and the code encrypted, the microphone of the page allowed; off: plain http, no microphone", false,
                [](const cluster::Config& cf) {return cf.web.https ? std::string("true") : std::string("false");}},
        }},
        {"mcp", "MCP", {
            {"mcp", "servers", "Custom MCP servers", SettingKind::List, {}, "MCP servers given to the global session, comma separated: "
                "\"name: command args\" (a local program, stdio; env VAR=value command... for its variables) or \"name: https://url\" "
                "(HTTP); its tools are mcp__<name>__*", true,
                [](const cluster::Config& cf) {return join_(cf.mcpServers);}},
        }},
        {"keys", "Keys", {}},
    };

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
        pages[2].settings.push_back({"commands", name, name, SettingKind::Text, {}, "program of the " + name + " driver", true,
            [name](const cluster::Config& cf) {return cf.command(name);}});
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
    for (const auto &[action, key]: c.keys) {
        const std::string name = action;
        pages.back().settings.push_back({"keys", name, name, SettingKind::Text, {}, "Ctrl+<letter>, Ctrl+Shift+<letter>, Alt+<letter>, F1..F12, Tab, Escape; alternatives separated by |", false,
            [name](const cluster::Config& cf) {return cf.keys.contains(name) ? cf.keys.at(name) : std::string();}});
    }
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
            (void)std::stol(value, &used);
            if (used != cluster::trim(value).size()) return "not an integer";
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
