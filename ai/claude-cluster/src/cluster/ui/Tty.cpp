/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Tty.cpp

File Description:
##  ftxui::Terminal front-end (FTXUI): layouts list / grid / tabs, session
##  view with its panels, permission and voice bars, palette
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Actions.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Core.hpp"
#include "cluster/Git.hpp"
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/screen/terminal.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <algorithm>
#include <cmath>
#include <map>

namespace {
//----------------------------------------------------------------//
/* TYPES */

enum class Mode {Normal, Palette, Prompt, Message, Restore};

struct Theme {
    ftxui::Color bg;
    ftxui::Color surface;
    ftxui::Color text;
    ftxui::Color muted;
    ftxui::Color accent;
    ftxui::Color border;
    ftxui::Color ok;
    ftxui::Color warn;
    ftxui::Color error;
};

//----------------------------------------------------------------//
/* TOOLS */

_cold ftxui::Color hex_(const std::string& hex)
{
    if (hex.size() != 7 || hex[0] != '#') return ftxui::Color::Default;
    const std::function<std::uint8_t(const std::size_t)> byte = [&](const std::size_t i) {return static_cast<std::uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));};
    return ftxui::Color::RGB(byte(1), byte(3), byte(5));
}

_cold Theme make_theme_(const cluster::Style& style)
{
    return {hex_(style.bg), hex_(style.surface), hex_(style.text), hex_(style.muted), hex_(style.accent), hex_(style.border),
        hex_(style.ok), hex_(style.warn), hex_(style.error)};
}

_cold std::size_t width_(const std::string& text)
{
    // display width ~ number of UTF-8 code points
    return static_cast<std::size_t>(std::count_if(text.begin(), text.end(), [](char c) {return (static_cast<unsigned char>(c) & 0xC0) != 0x80;}));
}

_cold std::vector<std::string> wrap_(const std::string& text, const std::size_t width)
{
    std::vector<std::string> lines;
    const std::size_t max = std::max<std::size_t>(width, 10);
    std::string line;
    std::size_t size = 0;

    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            lines.push_back(line);
            line.clear();
            size = 0;
            continue;
        }
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '\t') {
            line += "    ";
            size += 4;
            continue;
        }
        if ((c & 0xC0) != 0x80 && size >= max) {
            // break on the last space when there is one
            const std::size_t space = line.rfind(' ');
            if (space != std::string::npos && space > line.size() / 2) {
                lines.push_back(line.substr(0, space));
                line = line.substr(space + 1);
                size = width_(line);
            } else {
                lines.push_back(line);
                line.clear();
                size = 0;
            }
        }
        line += text[i];
        if ((c & 0xC0) != 0x80) ++size;
    }
    return lines;
}

_cold std::string key_name_(const ftxui::Event& event)
{
    // ftxui::Event -> "Ctrl+K", "Alt+X", "F5", "Escape", "Tab"
    static const std::vector<std::pair<ftxui::Event, std::string>> specials = {
        {ftxui::Event::F1, "F1"}, {ftxui::Event::F2, "F2"}, {ftxui::Event::F3, "F3"}, {ftxui::Event::F4, "F4"}, {ftxui::Event::F5, "F5"}, {ftxui::Event::F6, "F6"},
        {ftxui::Event::F7, "F7"}, {ftxui::Event::F8, "F8"}, {ftxui::Event::F9, "F9"}, {ftxui::Event::F10, "F10"}, {ftxui::Event::F11, "F11"}, {ftxui::Event::F12, "F12"},
        {ftxui::Event::Escape, "Escape"}, {ftxui::Event::Tab, "Tab"}, {ftxui::Event::TabReverse, "Shift+Tab"},
    };
    for (const auto &[special, name]: specials)
        if (event == special) return name;
    const std::string& input = event.input();
    if (input.size() == 1 && input[0] >= 1 && input[0] <= 26 && input[0] != 9 && input[0] != 10 && input[0] != 13)
        return std::string("Ctrl+") + static_cast<char>('A' + input[0] - 1);
    if (input.size() == 2 && input[0] == 27 && std::isalpha(static_cast<unsigned char>(input[1])))
        return std::string("Alt+") + static_cast<char>(std::toupper(static_cast<unsigned char>(input[1])));
    return "";
}

_cold std::string state_mark_(const cluster::State state)
{
    switch (state) {
        case cluster::State::Working: return "●";
        case cluster::State::Waiting: return "!";
        case cluster::State::Error: return "x";
        case cluster::State::Remote: return "R";
        case cluster::State::Stopped: return "-";
        case cluster::State::Starting: return "○";
        case cluster::State::Idle: return "○";
    }
    return "?";
}

//----------------------------------------------------------------//
/* CLASS */

class TtyUi {
    private:
        cluster::App& _app;
        cluster::Manager& _manager;
        ftxui::ScreenInteractive _screen = ftxui::ScreenInteractive::Fullscreen();
        Theme _theme{};
        Mode _mode = Mode::Normal;
        std::string _active = GLOBAL_ID;
        std::string _input;
        std::string _filter;
        std::string _prompt;
        std::string _message;
        bool _messageError = false;
        int _selected = 0;
        cluster::Action _pending;                   // action waiting for its input
        std::vector<cluster::Action> _actions;      // palette, filtered
        std::map<std::string, int> _scroll;         // lines scrolled up, per session
        bool _password = false;
        std::string _status;
        bool _statusError = false;
        std::int64_t _statusTime = 0;

        // ---------- Pre-Function -------- //
        _nodiscard std::vector<cluster::Snapshot> sessions_(void) const
        {
            return this->_manager.list(true);
        }

        void status_(const std::string& text, const bool error = false)
        {
            if (text.find('\n') != std::string::npos) {
                this->_message = text;
                this->_messageError = error;
                this->_mode = Mode::Message;
                return;
            }
            this->_status = text;
            this->_statusError = error;
            this->_statusTime = cluster::now();
        }

        void move_(const int delta)
        {
            const std::vector<cluster::Snapshot> list = this->sessions_();
            if (list.empty()) return;
            auto it = std::find_if(list.begin(), list.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->_active;});
            int index = it == list.end() ? 0 : static_cast<int>(it - list.begin());
            index = (index + delta + static_cast<int>(list.size())) % static_cast<int>(list.size());
            this->_active = list[static_cast<std::size_t>(index)].spec.id;
        }

        void filter_(void)
        {
            const std::string wanted = cluster::lower(this->_filter);
            this->_actions.clear();
            for (const cluster::Action& action: cluster::actions(this->_manager, this->_active))
                if (wanted.empty() || cluster::lower(action.title).find(wanted) != std::string::npos || cluster::lower(action.id).find(wanted) != std::string::npos)
                    this->_actions.push_back(action);
            this->_selected = std::clamp(this->_selected, 0, std::max(0, static_cast<int>(this->_actions.size()) - 1));
        }

        void run_(const cluster::Action& action, const std::string& input)
        {
            const std::string base = action.id.substr(0, action.id.find(':'));
            const std::string arg = action.id.find(':') == std::string::npos ? "" : action.id.substr(action.id.find(':') + 1);
            this->_mode = Mode::Normal;

            // Actions of the front-end
            if (base == "layout") {
                this->_manager.setUi("layout", arg);
            } else if (base == "style") {
                this->_manager.setUi("style", arg);
                this->theme_();
            } else if (base == "quit") {
                this->_screen.Exit();
            } else if (base == "auth_login" || base == "auth_logout") {
                // the login of an agent is interactive: run in the terminal, the interface comes back after
                this->_screen.WithRestoredIO([&]() {
                    try {
                        if (base == "auth_login") this->_manager.auth().login(arg);
                        else this->_manager.auth().logout(arg);
                    } catch (const utils::exception::IException& e) {
                        std::cerr << e.info() << std::endl;
                    }
                    std::cout << "\n[Enter] back to claude-cluster" << std::flush;
                    std::string line;
                    std::getline(std::cin, line);
                })();
            } else {
                const cluster::ActionResult result = cluster::run_action(this->_manager, *this->_app.voice, action.id, this->_active, input);
                if (!result.focus.empty()) this->_active = result.focus;
                if (!result.message.empty()) this->status_(result.message, result.error);
            }
        }

        void choose_(void)
        {
            if (this->_actions.empty()) return;
            const cluster::Action action = this->_actions[static_cast<std::size_t>(this->_selected)];
            if (action.input.empty()) {
                this->run_(action, "");
                return;
            }
            this->_pending = action;
            this->_prompt = action.fill;
            this->_password = action.id.starts_with("auth_key:");
            this->_mode = Mode::Prompt;
        }

        bool shortcut_(const std::string& action)
        {
            if (action.empty()) return false;
            if (action == "palette") {
                this->_filter.clear();
                this->_selected = 0;
                this->filter_();
                this->_mode = Mode::Palette;
            } else if (action == "next_session") {
                this->move_(1);
            } else if (action == "prev_session") {
                this->move_(-1);
            } else if (action == "global") {
                this->_active = GLOBAL_ID;
            } else if (action == "new_session") {
                this->_pending = {"new_session", "New session", "folder [backend]", ".", false};
                this->_prompt = ".";
                this->_password = false;
                this->_mode = Mode::Prompt;
            } else if (action == "close_session") {
                this->run_({"close_session", "", "", "", false}, "");
            } else if (action == "allow" || action == "deny" || action == "interrupt" || action == "mute") {
                this->run_({action, "", "", "", false}, "");
            } else if (action == "push_to_talk") {
                this->run_({"voice_toggle", "", "", "", false}, "");
            } else if (action == "layout") {
                const std::string layout = this->_manager.ui("layout", this->_manager.config().layout);
                this->_manager.setUi("layout", layout == "list" ? "grid" : layout == "grid" ? "tabs" : "list");
            } else if (action == "quit") {
                this->_screen.Exit();
            } else {
                return false;
            }
            return true;
        }

        void theme_(void)
        {
            cluster::Config& config = this->_manager.config();
            const std::string name = this->_manager.ui("style", config.style);
            auto it = config.styles.find(name);
            this->_theme = make_theme_(it != config.styles.end() ? it->second : config.currentStyle());
        }

        /* rendering */
        _nodiscard ftxui::Element sessionRow_(const cluster::Snapshot& s, const bool active) const
        {
            const ftxui::Color mark = s.state == cluster::State::Waiting ? this->_theme.warn : s.state == cluster::State::Error ? this->_theme.error
                : s.state == cluster::State::Working ? this->_theme.ok : this->_theme.muted;
            ftxui::Elements row = {
                ftxui::text(state_mark_(s.state) + " ") | ftxui::color(mark),
                ftxui::text(cluster::one_line(s.spec.name, 16)) | (active ? ftxui::bold : ftxui::nothing) | ftxui::color(this->_theme.text),
                ftxui::filler(),
                ftxui::text(cluster::human_tokens(s.metrics.total.total()) + " ") | ftxui::color(this->_theme.muted),
            };
            if (!s.permissions.empty()) row.push_back(ftxui::text("[" + std::to_string(s.permissions.size()) + "]") | ftxui::color(this->_theme.warn) | ftxui::bold);
            ftxui::Element line = ftxui::hbox(std::move(row));
            const std::string backend = s.spec.backend.substr(0, s.spec.backend.find('/'));
            ftxui::Element sub = ftxui::text("  " + cluster::one_line(backend + " · " + cluster::short_path(s.spec.cwd), 26)) | ftxui::color(this->_theme.muted) | ftxui::dim;
            ftxui::Element block = ftxui::vbox({line, sub});
            return active ? block | ftxui::bgcolor(this->_theme.surface) : block;
        }

        _nodiscard ftxui::Element transcript_(const cluster::Snapshot& s, const int width, const int height, const bool compact)
        {
            // Wrapped lines of the transcript, the last ones (minus the scroll)
            std::vector<ftxui::Element> lines;
            const std::size_t start = s.entries.size() > 300 ? s.entries.size() - 300 : 0;
            for (std::size_t i = start; i < s.entries.size(); ++i) {
                const cluster::Entry& e = s.entries[i];
                std::string prefix;
                ftxui::Color tint = this->_theme.text;
                ftxui::Decorator deco = ftxui::nothing;
                switch (e.kind) {
                    case cluster::EntryKind::User: prefix = "› "; tint = this->_theme.accent; deco = ftxui::bold; break;
                    case cluster::EntryKind::Assistant: prefix = ""; break;
                    case cluster::EntryKind::Thinking: if (compact) continue; prefix = "∴ "; tint = this->_theme.muted; deco = ftxui::dim; break;
                    case cluster::EntryKind::Tool: prefix = "⏺ " + e.tool + " "; tint = this->_theme.muted; break;
                    case cluster::EntryKind::ToolResult: if (compact) continue; prefix = "  ⎿ "; tint = this->_theme.muted; deco = ftxui::dim; break;
                    case cluster::EntryKind::System: prefix = "· "; tint = this->_theme.muted; deco = ftxui::dim; break;
                    case cluster::EntryKind::Error: prefix = "✗ "; tint = this->_theme.error; break;
                }
                const std::string body = e.kind == cluster::EntryKind::Thinking ? cluster::one_line(e.text, 300) : e.text;
                if (e.kind == cluster::EntryKind::User && !lines.empty()) lines.push_back(ftxui::text(""));
                for (const std::string& line: wrap_(prefix + body, static_cast<std::size_t>(std::max(10, width - 2))))
                    lines.push_back(ftxui::text(line) | ftxui::color(tint) | deco);
            }
            if (!s.partial.empty())
                for (const std::string& line: wrap_(s.partial, static_cast<std::size_t>(std::max(10, width - 2))))
                    lines.push_back(ftxui::text(line) | ftxui::color(this->_theme.text) | ftxui::dim);
            if (s.state == cluster::State::Working) lines.push_back(ftxui::text("… working" + (s.queued > 0 ? " (" + std::to_string(s.queued) + " queued)" : std::string())) | ftxui::color(this->_theme.muted) | ftxui::dim);

            int& scroll = this->_scroll[s.spec.id];
            scroll = std::clamp(scroll, 0, std::max(0, static_cast<int>(lines.size()) - height));
            const int end = static_cast<int>(lines.size()) - scroll;
            const int begin = std::max(0, end - height);
            ftxui::Elements shown(lines.begin() + begin, lines.begin() + end);
            if (scroll > 0) shown.push_back(ftxui::text("↓ " + std::to_string(scroll) + " more line(s) (PageDown)") | ftxui::color(this->_theme.accent) | ftxui::dim);
            return ftxui::vbox(std::move(shown)) | ftxui::yflex;
        }

        _nodiscard ftxui::Element panels_(const cluster::Snapshot& s)
        {
            static cluster::Git git; // lives as long as the program (its refresh threads)
            const cluster::Metrics& m = s.metrics;
            ftxui::Elements blocks;
            const std::function<void(const std::string&, ftxui::Elements)> section = [&](const std::string& name, ftxui::Elements body) {
                blocks.push_back(ftxui::vbox({ftxui::text(name) | ftxui::bold | ftxui::color(this->_theme.accent), ftxui::vbox(std::move(body))}));
                blocks.push_back(ftxui::text(""));
            };
            const std::function<ftxui::Element(const std::string&, const std::string&)> line = [&](const std::string& left, const std::string& right) {
                return ftxui::hbox({ftxui::text(left) | ftxui::color(this->_theme.muted), ftxui::filler(), ftxui::text(right) | ftxui::color(this->_theme.text)});
            };

            if (s.spec.panels.contains("tokens"))
                section("Tokens", {line("last prompt", cluster::human_tokens(m.last.total())), line("current", cluster::human_tokens(m.current.total())),
                    line("total", cluster::human_tokens(m.total.total())), line("turns", std::to_string(m.turns))});
            if (s.spec.panels.contains("context")) {
                const float ratio = m.contextWindow > 0 ? static_cast<float>(m.contextUsed) / static_cast<float>(m.contextWindow) : 0.0f;
                section("Context", {line(cluster::human_tokens(m.contextUsed) + " / " + cluster::human_tokens(m.contextWindow), std::to_string(static_cast<int>(ratio * 100)) + "%"),
                    ftxui::gauge(ratio) | ftxui::color(ratio > 0.8f ? this->_theme.warn : this->_theme.accent)});
            }
            if (s.spec.panels.contains("cost")) {
                ftxui::Elements body = {line("session", m.costKnown ? cluster::human_cost(m.cost) : "local / unknown"),
                    line("last turn", m.costKnown ? cluster::human_cost(m.lastCost) : "-")};
                if (this->_manager.config().budget > 0) body.push_back(line("budget", cluster::human_cost(this->_manager.config().budget)));
                if (m.limit5h >= 0) body.push_back(line("5h limit", std::to_string(static_cast<int>(m.limit5h * 100)) + "%"));
                if (m.limit7d >= 0) body.push_back(line("7d limit", std::to_string(static_cast<int>(m.limit7d * 100)) + "%"));
                section("Cost", std::move(body));
            }
            if (s.spec.panels.contains("skills")) {
                ftxui::Elements body;
                for (const std::string& skill: s.skillsLoaded)
                    body.push_back(ftxui::text("● " + skill) | ftxui::color(this->_theme.ok));
                if (s.skillsLoaded.empty()) body.push_back(ftxui::text("none loaded") | ftxui::color(this->_theme.muted) | ftxui::dim);
                body.push_back(ftxui::text(std::to_string(s.skillsAvailable.size()) + " available") | ftxui::color(this->_theme.muted) | ftxui::dim);
                section("Skills", std::move(body));
            }
            if (s.spec.panels.contains("git") || s.spec.panels.contains("diff")) {
                const cluster::GitInfo info = git.info(s.spec.cwd, s.spec.panels.contains("diff"));
                if (info.repository && s.spec.panels.contains("git")) {
                    ftxui::Elements body = {ftxui::text(" " + info.branch) | ftxui::color(this->_theme.ok) | ftxui::bold};
                    for (std::size_t i = 0; i < info.graph.size() && i < 14; ++i)
                        body.push_back(ftxui::text(cluster::one_line(info.graph[i], 34)) | ftxui::color(this->_theme.muted));
                    section("Git", std::move(body));
                } else if (s.spec.panels.contains("git") && info.time > 0) {
                    section("Git", {ftxui::text("not a repository") | ftxui::color(this->_theme.muted) | ftxui::dim});
                }
                if (info.repository && s.spec.panels.contains("diff")) {
                    ftxui::Elements body;
                    for (const std::string& stat: info.diffStat)
                        body.push_back(ftxui::text(cluster::one_line(stat, 34)) | ftxui::color(this->_theme.muted));
                    for (std::size_t i = 0; i < info.diff.size() && i < 40; ++i) {
                        const std::string& d = info.diff[i];
                        const ftxui::Color tint = d.starts_with("+") ? this->_theme.ok : d.starts_with("-") ? this->_theme.error : this->_theme.muted;
                        body.push_back(ftxui::text(cluster::one_line(d, 34)) | ftxui::color(tint));
                    }
                    if (body.empty()) body.push_back(ftxui::text("no change") | ftxui::color(this->_theme.muted) | ftxui::dim);
                    section("Diff", std::move(body));
                }
            }
            if (s.spec.panels.contains("tools")) {
                ftxui::Elements body;
                const std::size_t start = s.tools.size() > 10 ? s.tools.size() - 10 : 0;
                for (std::size_t i = start; i < s.tools.size(); ++i) {
                    const cluster::ToolRun& t = s.tools[i];
                    const ftxui::Color tint = !t.done ? this->_theme.accent : t.error ? this->_theme.error : this->_theme.ok;
                    body.push_back(ftxui::hbox({ftxui::text(t.done ? (t.error ? "✗ " : "✓ ") : "… ") | ftxui::color(tint), ftxui::text(cluster::one_line(t.name + " " + t.summary, 30)) | ftxui::color(this->_theme.muted)}));
                }
                if (body.empty()) body.push_back(ftxui::text("none yet") | ftxui::color(this->_theme.muted) | ftxui::dim);
                section("Tools", std::move(body));
            }
            if (s.spec.panels.contains("files")) {
                ftxui::Elements body;
                for (const std::string& file: s.files)
                    body.push_back(ftxui::text(cluster::one_line(cluster::short_path(file), 32)) | ftxui::color(this->_theme.text));
                if (body.empty()) body.push_back(ftxui::text("none") | ftxui::color(this->_theme.muted) | ftxui::dim);
                section("Files", std::move(body));
            }
            if (blocks.empty()) return ftxui::emptyElement();
            return ftxui::vbox(std::move(blocks)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 36) | ftxui::yframe;
        }

        _nodiscard ftxui::Element header_(const cluster::Snapshot& s, const bool compact) const
        {
            if (compact) {
                const ftxui::Color mark = s.state == cluster::State::Waiting ? this->_theme.warn : s.state == cluster::State::Error ? this->_theme.error : this->_theme.muted;
                return ftxui::hbox({ftxui::text(" " + s.spec.name + " ") | ftxui::bold | ftxui::color(this->_theme.text), ftxui::text(state_mark_(s.state) + " " + cluster::state_name(s.state)) | ftxui::color(mark),
                    ftxui::filler(), ftxui::text(s.spec.backend.substr(0, s.spec.backend.find('/')) + " ") | ftxui::color(this->_theme.muted) | ftxui::dim});
            }
            const std::string model = s.modelUsed.empty() ? s.spec.backend : s.spec.backend.substr(0, s.spec.backend.find('/')) + "/" + s.modelUsed;
            ftxui::Elements left = {
                ftxui::text(" " + s.spec.name + " ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent),
                ftxui::text(" " + std::string(cluster::state_name(s.state)) + " ") | ftxui::color(s.state == cluster::State::Error ? this->_theme.error : this->_theme.text),
                ftxui::text(model) | ftxui::color(this->_theme.muted),
                ftxui::text("  mode " + s.spec.mode) | ftxui::color(s.spec.mode == "bypassPermissions" ? this->_theme.warn : this->_theme.muted),
                ftxui::filler(),
                ftxui::text(cluster::short_path(s.spec.cwd) + " ") | ftxui::color(this->_theme.muted) | ftxui::dim,
            };
            ftxui::Element head = ftxui::hbox(std::move(left));
            if (!s.lastError.empty()) return ftxui::vbox({head, ftxui::text(" " + cluster::one_line(s.lastError, 200)) | ftxui::color(this->_theme.error)});
            if (s.state == cluster::State::Remote) return ftxui::vbox({head, ftxui::text(" Remote Control: continue on claude.ai / the Claude app (session " + s.remoteId + ")") | ftxui::color(this->_theme.accent)});
            return head;
        }

        _nodiscard ftxui::Element permission_(const cluster::Snapshot& s) const
        {
            if (s.permissions.empty()) return ftxui::emptyElement();
            const cluster::Permission& p = s.permissions.front();
            const cluster::Config& c = this->_manager.config();
            return ftxui::hbox({
                ftxui::text(" permission ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.warn),
                ftxui::text(" " + p.tool + ": " + cluster::one_line(p.description, 80) + " ") | ftxui::color(this->_theme.text),
                ftxui::filler(),
                ftxui::text(c.keys.at("allow") + " allow · " + c.keys.at("deny") + " deny · palette: always ") | ftxui::color(this->_theme.muted),
            }) | ftxui::bgcolor(this->_theme.surface);
        }

        _nodiscard ftxui::Element session_(const cluster::Snapshot& s, const int width, const int height, const bool compact)
        {
            ftxui::Element panels = compact ? ftxui::emptyElement() : this->panels_(s);
            const int panelWidth = compact || s.spec.panels.empty() ? 0 : 37;
            const int bodyHeight = std::max(3, height - (s.permissions.empty() ? 2 : 3) - (s.lastError.empty() ? 0 : 1));
            return ftxui::vbox({
                this->header_(s, compact),
                ftxui::separatorLight() | ftxui::color(this->_theme.border),
                ftxui::hbox({this->transcript_(s, width - panelWidth, bodyHeight, compact) | ftxui::flex, panelWidth ? ftxui::separatorLight() | ftxui::color(this->_theme.border) : ftxui::emptyElement(), panels}) | ftxui::flex,
                this->permission_(s),
            });
        }

        _nodiscard ftxui::Element voice_(void) const
        {
            const cluster::Voice& voice = *this->_app.voice;
            if (!voice.listening() && voice.pending().empty() && !voice.speaking()) return ftxui::emptyElement();
            const cluster::VoiceConf& conf = this->_manager.config().voice;
            std::string label = voice.listening() ? " ◉ listening " : " ○ voice ";
            const std::vector<std::string> wake = cluster::split(conf.wakeWord, ',');
            if (voice.listening() && conf.mode == "wake")
                label = voice.armed() ? " ◉ armed: say the command " : " ○ waiting for \"" + (wake.empty() ? std::string("?") : wake.front()) + "\" ";
            ftxui::Elements row = {ftxui::text(label) | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(voice.armed() || (voice.listening() && conf.mode != "wake") ? this->_theme.ok : this->_theme.muted)};
            for (const cluster::Pending& p: voice.pending()) {
                const std::int64_t left = p.deadline == 0 ? -1 : std::max<std::int64_t>(0, (p.deadline - std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() + 999) / 1000);
                const std::string target = p.target.empty() ? "" : (p.target == GLOBAL_ID ? "global" : p.target);
                row.push_back(ftxui::text(" " + (target.empty() ? "" : "→ " + target + ": ") + cluster::one_line(p.text, 90) + (left >= 0 ? " (" + std::to_string(left) + "s, palette: send / drop / correct)" : "")) | ftxui::color(this->_theme.text));
            }
            if (voice.speaking()) row.push_back(ftxui::text("  speaking (" + this->_manager.config().keys.at("mute") + " mute)") | ftxui::color(this->_theme.muted));
            return ftxui::hbox(std::move(row)) | ftxui::bgcolor(this->_theme.surface);
        }

        _nodiscard ftxui::Element body_(const int width, const int height)
        {
            const std::vector<cluster::Snapshot> list = this->sessions_();
            if (list.empty()) return ftxui::text("no session: " + this->_manager.config().keys.at("new_session") + " to start one") | ftxui::center | ftxui::flex;
            auto it = std::find_if(list.begin(), list.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->_active;});
            if (it == list.end()) {
                this->_active = list.front().spec.id;
                it = list.begin();
            }
            const std::string layout = this->_manager.ui("layout", this->_manager.config().layout);

            if (layout == "grid" && list.size() > 1) {
                const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(list.size()))));
                const int rows = (static_cast<int>(list.size()) + columns - 1) / columns;
                const int cellWidth = width / columns - 2;
                const int cellHeight = height / rows - 2;
                std::vector<ftxui::Elements> grid;
                for (std::size_t i = 0; i < list.size(); ++i) {
                    if (i % static_cast<std::size_t>(columns) == 0) grid.emplace_back();
                    const bool active = list[i].spec.id == this->_active;
                    grid.back().push_back(this->session_(list[i], cellWidth, cellHeight, true) | ftxui::borderStyled(active ? ftxui::HEAVY : ftxui::ROUNDED)
                        | ftxui::color(active ? this->_theme.accent : this->_theme.border) | ftxui::flex);
                }
                while (grid.back().size() < static_cast<std::size_t>(columns)) grid.back().push_back(ftxui::filler());
                return ftxui::gridbox(grid) | ftxui::flex;
            }
            if (layout == "tabs") {
                ftxui::Elements tabs;
                for (const cluster::Snapshot& s: list) {
                    const bool active = s.spec.id == this->_active;
                    ftxui::Element tab = ftxui::text(" " + state_mark_(s.state) + " " + cluster::one_line(s.spec.name, 14) + (s.permissions.empty() ? "" : " !") + " ");
                    tabs.push_back(active ? tab | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) : tab | ftxui::color(this->_theme.muted));
                }
                return ftxui::vbox({ftxui::hbox(std::move(tabs)), this->session_(*it, width, height - 1, false) | ftxui::flex});
            }
            // list + active panel
            ftxui::Elements rows = {ftxui::text(" Sessions") | ftxui::bold | ftxui::color(this->_theme.accent), ftxui::text("")};
            for (const cluster::Snapshot& s: list)
                rows.push_back(this->sessionRow_(s, s.spec.id == this->_active));
            const std::vector<cluster::Task> tasks = this->_manager.tasks();
            const auto open = std::count_if(tasks.begin(), tasks.end(), [](const cluster::Task& t) {return t.status == "queued" || t.status == "running";});
            if (open > 0) rows.push_back(ftxui::text(" " + std::to_string(open) + " task(s) in the queue") | ftxui::color(this->_theme.muted));
            return ftxui::hbox({
                ftxui::vbox(std::move(rows)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 30) | ftxui::yframe,
                ftxui::separatorLight() | ftxui::color(this->_theme.border),
                this->session_(*it, width - 31, height, false) | ftxui::flex,
            });
        }

        _nodiscard ftxui::Element modal_(void)
        {
            const std::function<ftxui::Element(ftxui::Element, const int)> box = [&](ftxui::Element content, const int width) {
                return content | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width) | ftxui::borderRounded | ftxui::color(this->_theme.accent) | ftxui::bgcolor(this->_theme.surface) | ftxui::clear_under | ftxui::center;
            };
            if (this->_mode == Mode::Palette) {
                ftxui::Elements items;
                const int start = std::max(0, this->_selected - 12);
                for (int i = start; i < static_cast<int>(this->_actions.size()) && i < start + 24; ++i) {
                    ftxui::Element item = ftxui::text(" " + this->_actions[static_cast<std::size_t>(i)].title + " ");
                    items.push_back(i == this->_selected ? item | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) : item | ftxui::color(this->_theme.text));
                }
                if (items.empty()) items.push_back(ftxui::text(" nothing matches") | ftxui::color(this->_theme.muted));
                return box(ftxui::vbox({ftxui::hbox({ftxui::text(" > ") | ftxui::color(this->_theme.accent), ftxui::text(this->_filter) | ftxui::color(this->_theme.text), ftxui::text("▏") | ftxui::blink}),
                    ftxui::separatorLight(), ftxui::vbox(std::move(items))}), 80);
            }
            if (this->_mode == Mode::Prompt) {
                const std::string shown = this->_password ? std::string(width_(this->_prompt), '*') : this->_prompt;
                return box(ftxui::vbox({ftxui::text(" " + this->_pending.title) | ftxui::bold, ftxui::text(" " + this->_pending.input + ":") | ftxui::color(this->_theme.muted),
                    ftxui::hbox({ftxui::text(" > ") | ftxui::color(this->_theme.accent), ftxui::text(shown), ftxui::text("▏") | ftxui::blink}), ftxui::text(" Enter: run · Escape: cancel") | ftxui::dim}), 80);
            }
            if (this->_mode == Mode::Message) {
                ftxui::Elements lines;
                for (const std::string& line: cluster::split(this->_message, '\n'))
                    lines.push_back(ftxui::text(" " + line) | ftxui::color(this->_messageError ? this->_theme.error : this->_theme.text));
                lines.push_back(ftxui::text(" Escape / Enter: close") | ftxui::dim);
                return box(ftxui::vbox(std::move(lines)), 100);
            }
            if (this->_mode == Mode::Restore) {
                const std::vector<cluster::SessionSpec> previous = this->_manager.previous();
                ftxui::Elements lines = {ftxui::text(" Restore the previous sessions?") | ftxui::bold, ftxui::text("")};
                for (const cluster::SessionSpec& spec: previous)
                    lines.push_back(ftxui::text("   " + spec.name + "  " + spec.backend + "  " + cluster::short_path(spec.cwd)) | ftxui::color(this->_theme.muted));
                lines.push_back(ftxui::text(""));
                lines.push_back(ftxui::text(" [r] restore   [n] start clean (they stay in the trash " + std::to_string(this->_manager.config().trashDays) + " days)") | ftxui::color(this->_theme.accent));
                return box(ftxui::vbox(std::move(lines)), 90);
            }
            return ftxui::emptyElement();
        }

        _nodiscard ftxui::Element render_(void)
        {
            if (this->_manager.config().reload()) this->theme_();
            const ftxui::Dimensions size = ftxui::Terminal::Size();
            const std::string layout = this->_manager.ui("layout", this->_manager.config().layout);
            const std::vector<cluster::Notice> notices = this->_manager.notices(1);
            std::string status = this->_status;
            bool error = this->_statusError;
            if (!notices.empty() && notices.back().time > this->_statusTime) {
                status = cluster::one_line(notices.back().text, 160);
                error = notices.back().error;
            }
            const cluster::Config& c = this->_manager.config();

            ftxui::Element top = ftxui::hbox({
                ftxui::text(" claude-cluster ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent),
                ftxui::text(" " + layout + " · " + this->_manager.ui("style", c.style) + (this->_manager.remote() ? " · remote" : "")) | ftxui::color(this->_theme.muted),
                ftxui::filler(),
                ftxui::text(c.keys.at("palette") + " palette · " + c.keys.at("next_session") + "/" + c.keys.at("prev_session") + " session · "
                    + c.keys.at("global") + " global · " + c.keys.at("new_session") + " new · " + c.keys.at("quit") + " quit ") | ftxui::color(this->_theme.muted),
            });
            const int bodyHeight = size.dimy - 4 - (this->_app.voice->listening() || !this->_app.voice->pending().empty() ? 1 : 0);
            ftxui::Element input = ftxui::hbox({ftxui::text(" › ") | ftxui::color(this->_theme.accent) | ftxui::bold, ftxui::text(this->_input) | ftxui::color(this->_theme.text),
                ftxui::text(this->_mode == Mode::Normal ? "▏" : "") | ftxui::blink, ftxui::filler(),
                ftxui::text(this->_active == GLOBAL_ID ? "to the global session " : "to " + this->_active + " ") | ftxui::color(this->_theme.muted) | ftxui::dim});
            ftxui::Element document = ftxui::vbox({
                top,
                this->body_(size.dimx, bodyHeight) | ftxui::flex,
                this->voice_(),
                ftxui::separatorLight() | ftxui::color(this->_theme.border),
                input,
                ftxui::text(" " + status) | ftxui::color(error ? this->_theme.error : this->_theme.muted),
            }) | ftxui::color(this->_theme.text) | ftxui::bgcolor(this->_theme.bg);
            if (this->_mode == Mode::Normal) return document;
            return ftxui::dbox({document, this->modal_()});
        }

        /* events */
        bool event_(ftxui::Event event)
        {
            if (event == ftxui::Event::Custom) return true;
            if (event.is_mouse()) {
                const ftxui::Mouse& mouse = event.mouse();
                if (mouse.button == ftxui::Mouse::WheelUp) this->_scroll[this->_active] += 3;
                else if (mouse.button == ftxui::Mouse::WheelDown) this->_scroll[this->_active] = std::max(0, this->_scroll[this->_active] - 3);
                return true;
            }
            const std::string key = key_name_(event);

            switch (this->_mode) {
                case Mode::Restore:
                    if (event == ftxui::Event::Character('r') || event == ftxui::Event::Character('R') || event == ftxui::Event::Return) this->_manager.restorePrevious(true);
                    else if (event == ftxui::Event::Character('n') || event == ftxui::Event::Character('N') || event == ftxui::Event::Escape) this->_manager.restorePrevious(false);
                    else return true;
                    this->_mode = Mode::Normal;
                    return true;
                case Mode::Message:
                    if (event == ftxui::Event::Escape || event == ftxui::Event::Return) this->_mode = Mode::Normal;
                    return true;
                case Mode::Palette:
                    if (event == ftxui::Event::Escape) this->_mode = Mode::Normal;
                    else if (event == ftxui::Event::ArrowDown) this->_selected = std::min(this->_selected + 1, std::max(0, static_cast<int>(this->_actions.size()) - 1));
                    else if (event == ftxui::Event::ArrowUp) this->_selected = std::max(0, this->_selected - 1);
                    else if (event == ftxui::Event::Return) this->choose_();
                    else if (event == ftxui::Event::Backspace) {
                        if (!this->_filter.empty()) this->_filter.pop_back();
                        this->filter_();
                    } else if (event.is_character()) {
                        this->_filter += event.character();
                        this->_selected = 0;
                        this->filter_();
                    }
                    return true;
                case Mode::Prompt:
                    if (event == ftxui::Event::Escape) this->_mode = Mode::Normal;
                    else if (event == ftxui::Event::Return) this->run_(this->_pending, this->_prompt);
                    else if (event == ftxui::Event::Backspace) {
                        while (!this->_prompt.empty() && (static_cast<unsigned char>(this->_prompt.back()) & 0xC0) == 0x80) this->_prompt.pop_back();
                        if (!this->_prompt.empty()) this->_prompt.pop_back();
                    } else if (event.is_character()) this->_prompt += event.character();
                    return true;
                case Mode::Normal:
                    break;
            }

            if (this->shortcut_(cluster::key_action(this->_manager.config(), key))) return true;
            if (event == ftxui::Event::PageUp) this->_scroll[this->_active] += 10;
            else if (event == ftxui::Event::PageDown) this->_scroll[this->_active] = std::max(0, this->_scroll[this->_active] - 10);
            else if (event == ftxui::Event::Return) {
                const std::string text = cluster::trim(this->_input);
                if (text.empty()) return true;
                try {
                    this->_manager.send(this->_active, text);
                    this->_input.clear();
                    this->_scroll[this->_active] = 0;
                } catch (const utils::exception::IException& e) {
                    this->status_(e.info(), true);
                }
            } else if (event == ftxui::Event::Backspace) {
                while (!this->_input.empty() && (static_cast<unsigned char>(this->_input.back()) & 0xC0) == 0x80) this->_input.pop_back();
                if (!this->_input.empty()) this->_input.pop_back();
            } else if (event.is_character()) {
                this->_input += event.character();
            } else {
                return false;
            }
            return true;
        }

    public:
        int run(void)
        {
            this->theme_();
            if (this->_app.askRestore) this->_mode = Mode::Restore;
            this->_screen.ForceHandleCtrlC(false);
            this->_screen.ForceHandleCtrlZ(false);

            // Redraw when something changed (sessions, voice, git), at least once a second (timers)
            std::atomic<bool> alive{true};
            std::thread refresher([&]() {
                std::uint64_t last = 0;
                int ticks = 0;
                while (alive) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(120));
                    const std::uint64_t version = this->_manager.version() + this->_app.voice->version();
                    if (version != last || ++ticks >= 8) {
                        last = version;
                        ticks = 0;
                        this->_screen.PostEvent(ftxui::Event::Custom);
                    }
                }
            });
            ftxui::Component root = ftxui::Renderer([this]() {return this->render_();});
            root = ftxui::CatchEvent(root, [this](ftxui::Event event) {return this->event_(event);});
            this->_screen.Loop(root);
            alive = false;
            refresher.join();
            return 0;
        }

        TtyUi(cluster::App& app)
            : _app(app), _manager(*app.manager)
        {
        }
};

} // namespace

/* entry */
_cold int cluster::run_tty(cluster::App& app)
{
    TtyUi ui(app);
    return ui.run();
}
