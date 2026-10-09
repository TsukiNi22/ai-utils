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
#include "cluster/Settings.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Editor.hpp"
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
#include <termios.h>
#include <algorithm>
#include <unistd.h>
#include <optional>
#include <fstream>
#include <sstream>
#include <chrono>
#include <regex>
#include <cmath>
#include <map>

namespace {
//----------------------------------------------------------------//
/* TYPES */

enum class Mode {Normal, Palette, Prompt, Message, Restore, Settings, History};

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

    // Extended sequences (terminals that tell the modifiers apart): CSI u "ESC[<code>;<mods>u" and xterm
    // modifyOtherKeys "ESC[27;<mods>;<code>~" -> "Ctrl+Shift+T", "Alt+X"...
    static const std::regex csiU("^\x1b\\[(\\d+);(\\d+)u$");
    static const std::regex otherKeys("^\x1b\\[27;(\\d+);(\\d+)~$");
    std::smatch match;
    int code = 0;
    int mods = 0;
    if (std::regex_match(input, match, csiU)) {
        code = std::stoi(match[1].str());
        mods = std::stoi(match[2].str());
    } else if (std::regex_match(input, match, otherKeys)) {
        mods = std::stoi(match[1].str());
        code = std::stoi(match[2].str());
    }
    if (code <= 32 || code >= 127 || mods < 2) return "";
    const int bits = mods - 1;
    return std::string(bits & 4 ? "Ctrl+" : "") + (bits & 1 ? "Shift+" : "") + (bits & 2 ? "Alt+" : "")
        + static_cast<char>(std::toupper(static_cast<unsigned char>(code)));
}

struct Arrow {
    int dx = 0;
    int dy = 0;
    bool shift = false;
    bool ctrl = false;
};

_cold std::optional<Arrow> arrow_(const ftxui::Event& event)
{
    // plain arrows, and "ESC[1;<mods><A-D>" (2 Shift, 5 Ctrl, 6 Ctrl+Shift)
    if (event == ftxui::Event::ArrowLeft) return Arrow{-1, 0, false, false};
    if (event == ftxui::Event::ArrowRight) return Arrow{1, 0, false, false};
    if (event == ftxui::Event::ArrowUp) return Arrow{0, -1, false, false};
    if (event == ftxui::Event::ArrowDown) return Arrow{0, 1, false, false};
    const std::string& input = event.input();
    if (input.size() != 6 || !input.starts_with("\x1b[1;")) return std::nullopt; // xstyle: ignore LU-ANSI (read, not written)
    const int mods = input[4] - '1';
    const char key = input[5];
    Arrow arrow{key == 'D' ? -1 : key == 'C' ? 1 : 0, key == 'A' ? -1 : key == 'B' ? 1 : 0, (mods & 1) != 0, (mods & 4) != 0};
    if (arrow.dx == 0 && arrow.dy == 0) return std::nullopt;
    return arrow;
}

struct Preview {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;
};

_cold const Preview& preview_(const std::string& path)
{
    // image -> 48x48 pixels max (magick), drawn with half blocks (two pixels per character)
    static std::map<std::string, Preview> cache;
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    Preview preview;
    const cluster::Captured out = cluster::capture({"magick", path, "-resize", "48x48", "-depth", "8", "ppm:-"}, "", 10);
    std::istringstream in(out.out);
    std::string magic;
    int max = 0;
    in >> magic >> preview.width >> preview.height >> max;
    in.get();
    if (magic == "P6" && preview.width > 0 && preview.height > 0) {
        preview.rgb.resize(static_cast<std::size_t>(preview.width * preview.height * 3));
        in.read(reinterpret_cast<char*>(preview.rgb.data()), static_cast<std::streamsize>(preview.rgb.size()));
    } else {
        preview.width = 0;
    }
    return cache.emplace(path, preview).first->second;
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
        cluster::Editor _editor;                    // the prompt
        std::vector<std::string> _historyItems;     // Mode::History, filtered
        std::vector<ftxui::Box> _imageBoxes;        // pasted images in the prompt bar (mouse hover)
        int _hoverImage = -1;
        std::string _filter;
        std::string _prompt;
        std::string _message;
        bool _messageError = false;
        int _selected = 0;
        std::int64_t _prefixUntil = 0;              // Ctrl+B pressed: unix ms until which an arrow moves between sessions
        std::vector<cluster::SettingsPage> _pages;  // settings page
        int _page = 0;
        int _item = 0;
        bool _editing = false;
        std::string _edit;
        std::string _note;
        std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> _changes;
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

        void moveDir_(const int dx, const int dy)
        {
            // Session on the left / right / above / below in the current layout (grid: rows and columns; list, tabs: previous / next)
            const std::vector<cluster::Snapshot> list = this->sessions_();
            const int count = static_cast<int>(list.size());
            if (count == 0) return;
            auto it = std::find_if(list.begin(), list.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->_active;});
            const int index = it == list.end() ? 0 : static_cast<int>(it - list.begin());
            const std::string layout = this->_manager.ui("layout", this->_manager.config().layout);
            const int columns = layout == "grid" && count > 1 ? static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))) : 1;
            int target = index;
            if (dx != 0 || columns == 1) target = (index + (dx != 0 ? dx : dy) + count) % count;
            else if (index + dy * columns >= 0 && index + dy * columns < count) target = index + dy * columns;
            this->_active = list[static_cast<std::size_t>(target)].spec.id;
        }

        void openRestore_(void)
        {
            if (this->_manager.trash().empty()) {
                this->status_("the trash is empty: no closed session to reopen");
                return;
            }
            this->_filter = "restore:";
            this->_selected = 0;
            this->filter_();
            this->_mode = Mode::Palette;
        }

        void historyFilter_(void)
        {
            const std::string wanted = cluster::lower(this->_filter);
            this->_historyItems.clear();
            for (const std::string& text: this->_manager.history())
                if (wanted.empty() || cluster::lower(text).find(wanted) != std::string::npos) this->_historyItems.push_back(text);
            this->_selected = std::clamp(this->_selected, 0, std::max(0, static_cast<int>(this->_historyItems.size()) - 1));
        }

        void paste_(void)
        {
            // clipboard: an image is saved and put as [Image #N], else its text is inserted
            const std::string types = cluster::capture({"wl-paste", "--list-types"}, "", 5).out;
            const std::size_t at = types.find("image/");
            if (at != std::string::npos) {
                const std::string type = cluster::trim(types.substr(at, types.find('\n', at) - at));
                const std::string extension = type == "image/jpeg" ? ".jpg" : type == "image/gif" ? ".gif" : type == "image/webp" ? ".webp" : ".png";
                const cluster::Captured image = cluster::capture({"wl-paste", "--type", type}, "", 10);
                if (image.code == 0 && !image.out.empty()) {
                    std::error_code error;
                    std::filesystem::create_directories(cluster::data_dir() / "images", error);
                    const std::filesystem::path path = cluster::data_dir() / "images" / ("paste-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()) + extension);
                    std::ofstream(path, std::ios::binary) << image.out;
                    this->_editor.addImage(path.string());
                    return;
                }
            }
            const cluster::Captured text = cluster::capture({"wl-paste", "--no-newline"}, "", 5);
            if (text.code == 0) this->_editor.insert(text.out);
            else this->status_("clipboard unreadable (wl-paste)", true);
        }

        void submit_(void)
        {
            const std::string text = cluster::trim(this->_editor.text());
            if (text.empty()) return;
            const std::optional<cluster::ActionResult> local = cluster::run_slash(this->_manager, this->_active, text);
            if (local) {
                this->_manager.addHistory(text, this->_active);
                this->_editor.clear();
                if (!local->focus.empty()) this->_active = local->focus;
                if (local->ui == "settings") this->openSettings_();
                else if (local->ui == "restore_menu") this->openRestore_();
                if (!local->message.empty()) this->status_(local->message, local->error);
                return;
            }
            try {
                this->_manager.send(this->_active, text, false, this->_editor.images());
                this->_editor.clear();
                this->_scroll[this->_active] = 0;
            } catch (const utils::exception::IException& e) {
                this->status_(e.info(), true);
            }
        }

        _nodiscard ftxui::Element prompt_(const int width)
        {
            // the prompt line: selection inverted, cursor, the end shown when it is too long
            const std::string& text = this->_editor.text();
            const std::pair<std::size_t, std::size_t> sel = this->_editor.selection();
            const std::size_t cursor = this->_editor.cursor();
            ftxui::Elements parts = {ftxui::text(" › ") | ftxui::color(this->_theme.accent) | ftxui::bold};
            std::size_t from = 0;
            const std::size_t room = static_cast<std::size_t>(std::max(10, width - 30));
            if (text.size() > room && cursor > room / 2) from = std::min(cursor - room / 2, text.size() - std::min(text.size(), room));
            while (from > 0 && (static_cast<unsigned char>(text[from]) & 0xC0) == 0x80) --from;
            if (from > 0) parts.push_back(ftxui::text("…") | ftxui::color(this->_theme.muted));
            const std::function<void(std::size_t, std::size_t, bool)> piece = [&](std::size_t a, std::size_t b, bool selected) {
                a = std::max(a, from);
                if (b <= a) return;
                ftxui::Element element = ftxui::text(text.substr(a, b - a)) | ftxui::color(this->_theme.text);
                parts.push_back(selected ? element | ftxui::inverted : element);
            };
            if (sel.first != sel.second) {
                piece(0, sel.first, false);
                piece(sel.first, sel.second, true);
                piece(sel.second, text.size(), false);
            } else {
                piece(0, cursor, false);
                if (this->_mode == Mode::Normal) parts.push_back(ftxui::text("▏") | ftxui::color(this->_theme.accent) | ftxui::blink);
                piece(cursor, text.size(), false);
            }
            if (text.empty() && this->_mode == Mode::Normal)
                parts.push_back(ftxui::text(" @ file · / command · " + this->_manager.config().keys.at("history") + " history") | ftxui::color(this->_theme.muted) | ftxui::dim);
            parts.push_back(ftxui::filler());
            parts.push_back(ftxui::text(this->_active == GLOBAL_ID ? "to the global session " : "to " + this->_active + " ") | ftxui::color(this->_theme.muted) | ftxui::dim);
            return ftxui::hbox(std::move(parts));
        }

        _nodiscard ftxui::Element completion_(void) const
        {
            if (!this->_editor.completing() || this->_mode != Mode::Normal) return ftxui::emptyElement();
            const std::vector<cluster::Candidate>& items = this->_editor.items();
            const int selected = this->_editor.selectedItem();
            const int start = std::max(0, std::min(selected - 4, static_cast<int>(items.size()) - 8));
            ftxui::Elements rows;
            for (int i = start; i < static_cast<int>(items.size()) && i < start + 8; ++i) {
                const cluster::Candidate& c = items[static_cast<std::size_t>(i)];
                const std::string prefix = this->_editor.kind() == cluster::CompletionKind::File ? "@" : this->_editor.kind() == cluster::CompletionKind::Command ? "/" : "";
                ftxui::Element row = ftxui::hbox({ftxui::text("  " + prefix + c.label + "  "), ftxui::text(cluster::one_line(c.description, 70)) | ftxui::dim, ftxui::filler()});
                rows.push_back(i == selected ? row | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) : row | ftxui::color(this->_theme.text));
            }
            rows.push_back(ftxui::text("  Tab complete · Enter accept · ↑↓ choose · Space / Esc close (\\ + Space: a space in the name)") | ftxui::color(this->_theme.muted) | ftxui::dim);
            return ftxui::vbox(std::move(rows)) | ftxui::bgcolor(this->_theme.surface);
        }

        _nodiscard ftxui::Element images_(void)
        {
            // pasted images: one chip each; the mouse over a chip shows the image (half blocks)
            const std::vector<std::string>& images = this->_editor.images();
            this->_imageBoxes.resize(images.size());
            if (images.empty()) return ftxui::emptyElement();
            ftxui::Elements chips = {ftxui::text(" ")};
            for (std::size_t i = 0; i < images.size(); ++i) {
                ftxui::Element chip = ftxui::text(" [Image #" + std::to_string(i + 1) + "] " + std::filesystem::path(images[i]).filename().string() + " ")
                    | ftxui::color(this->_theme.text) | ftxui::bgcolor(static_cast<int>(i) == this->_hoverImage ? this->_theme.accent : this->_theme.surface);
                chips.push_back(chip | ftxui::reflect(this->_imageBoxes[i]));
                chips.push_back(ftxui::text(" "));
            }
            chips.push_back(ftxui::text("(mouse over: preview)") | ftxui::color(this->_theme.muted) | ftxui::dim);
            ftxui::Element line = ftxui::hbox(std::move(chips));
            if (this->_hoverImage < 0 || this->_hoverImage >= static_cast<int>(images.size())) return line;
            const Preview& image = preview_(images[static_cast<std::size_t>(this->_hoverImage)]);
            if (image.width == 0) return ftxui::vbox({ftxui::text(" (no preview: magick needed)") | ftxui::dim, line});
            ftxui::Elements rows;
            for (int y = 0; y + 1 < image.height + 1; y += 2) {
                ftxui::Elements cells = {ftxui::text("  ")};
                for (int x = 0; x < image.width; ++x) {
                    const std::size_t top = static_cast<std::size_t>((y * image.width + x) * 3);
                    const std::size_t bottom = static_cast<std::size_t>((std::min(y + 1, image.height - 1) * image.width + x) * 3);
                    cells.push_back(ftxui::text("▀") | ftxui::color(ftxui::Color::RGB(image.rgb[top], image.rgb[top + 1], image.rgb[top + 2]))
                        | ftxui::bgcolor(ftxui::Color::RGB(image.rgb[bottom], image.rgb[bottom + 1], image.rgb[bottom + 2])));
                }
                rows.push_back(ftxui::hbox(std::move(cells)));
            }
            rows.push_back(line);
            return ftxui::vbox(std::move(rows));
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
            } else if (base == "settings") {
                this->openSettings_();
            } else if (base == "restore_menu") {
                this->openRestore_();
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
            if (action == "undo") {
                this->_editor.undo();
            } else if (action == "redo") {
                this->_editor.redo();
            } else if (action == "select_all") {
                this->_editor.selectAll();
            } else if (action == "stash") {
                if (cluster::trim(this->_editor.text()).empty()) {
                    this->status_("nothing to stash");
                } else {
                    this->_manager.stashPush(this->_editor.text());
                    this->_editor.clear();
                    this->status_("prompt stashed (" + std::to_string(this->_manager.stashSize()) + "), " + this->_manager.config().keys.at("unstash") + " to take it back");
                }
            } else if (action == "unstash") {
                const std::optional<std::string> text = this->_manager.stashPop();
                if (!text) {
                    this->status_("the stash is empty");
                } else {
                    if (!cluster::trim(this->_editor.text()).empty()) this->_manager.stashPush(this->_editor.text()); // swapped, not lost
                    this->_editor.setText(*text);
                }
            } else if (action == "history") {
                this->_filter.clear();
                this->_selected = 0;
                this->historyFilter_();
                this->_mode = Mode::History;
            } else if (action == "paste") {
                this->paste_();
            } else if (action == "cut") {
                const std::string text = this->_editor.cut();
                if (!text.empty()) cluster::detach({"wl-copy", "--", text});
            } else if (action == "interrupt" && !this->_editor.selected().empty()) {
                cluster::detach({"wl-copy", "--", this->_editor.selected()}); // Ctrl+C on a selection: copy
                this->status_("copied");
            } else if (action == "always") {
                this->run_({"always", "", "", "", false}, "");
            } else if (action == "restore") {
                this->openRestore_();
            } else if (action == "prefix") {
                this->_prefixUntil = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() + 2000;
                this->status_(this->_manager.config().keys.at("prefix") + ": ← → ↑ ↓ to change of session");
            } else if (action == "palette") {
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
            } else if (action == "settings") {
                this->openSettings_();
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

        /* settings page */
        void openSettings_(void)
        {
            this->_pages = cluster::settings_pages(this->_manager.config());
            this->_page = 0;
            this->_item = 0;
            this->_editing = false;
            this->_note.clear();
            this->_changes.clear();
            this->_mode = Mode::Settings;
        }

        _nodiscard std::string settingValue_(const cluster::Setting& setting) const
        {
            auto it = this->_changes.find({setting.section, setting.key});
            return it != this->_changes.end() ? it->second.second : setting.value(this->_manager.config());
        }

        void setSetting_(const cluster::Setting& setting, const std::string& value)
        {
            const std::string error = cluster::setting_check(setting, value);
            if (!error.empty()) {
                this->_note = setting.label + ": " + error;
                return;
            }
            if (value == setting.value(this->_manager.config())) this->_changes.erase({setting.section, setting.key});
            else this->_changes[{setting.section, setting.key}] = {setting, value};
            this->_note.clear();
        }

        void saveSettings_(void)
        {
            if (!this->_changes.empty()) {
                try {
                    cluster::settings_write(this->_manager.config(), this->_changes);
                } catch (const utils::exception::IException& e) {
                    this->_note = std::string("not saved: ") + e.info();
                    return;
                }
                // the live choices of layout / style follow the saved ones
                for (const char* key: {"layout", "style"})
                    if (this->_changes.contains({"ui", key})) this->_manager.setUi(key, this->_changes.at({"ui", key}).second);
                bool restart = false;
                for (const auto &[where, change]: this->_changes)
                    restart |= change.first.restart;
                (void)this->_manager.config().reload();
                this->theme_();
                this->status_(std::to_string(this->_changes.size()) + " setting(s) saved" + (restart ? " (some at the next start)" : ""));
            }
            this->_changes.clear();
            this->_mode = Mode::Normal;
        }

        bool settingsEvent_(ftxui::Event event)
        {
            if (this->_pages.empty()) {
                this->_mode = Mode::Normal;
                return true;
            }
            const cluster::SettingsPage& page = this->_pages[static_cast<std::size_t>(this->_page)];
            const int count = static_cast<int>(page.settings.size());
            if (count > 0) this->_item = std::clamp(this->_item, 0, count - 1);

            if (this->_editing) {
                const cluster::Setting& setting = page.settings[static_cast<std::size_t>(this->_item)];
                if (event == ftxui::Event::Return) {
                    this->setSetting_(setting, cluster::trim(this->_edit));
                    this->_editing = false;
                } else if (event == ftxui::Event::Escape) {
                    this->_editing = false;
                } else if (event == ftxui::Event::Backspace) {
                    while (!this->_edit.empty() && (static_cast<unsigned char>(this->_edit.back()) & 0xC0) == 0x80) this->_edit.pop_back();
                    if (!this->_edit.empty()) this->_edit.pop_back();
                } else if (event.is_character()) {
                    this->_edit += event.character();
                }
                return true;
            }
            if (event == ftxui::Event::F10) {
                this->saveSettings_();
            } else if (event == ftxui::Event::Escape) {
                if (!this->_changes.empty() && this->_note != "unsaved") {
                    this->_note = "unsaved";
                } else {
                    this->_changes.clear();
                    this->_mode = Mode::Normal;
                }
            } else if (event == ftxui::Event::Tab || event == ftxui::Event::PageDown) {
                this->_page = (this->_page + 1) % static_cast<int>(this->_pages.size());
                this->_item = 0;
            } else if (event == ftxui::Event::TabReverse || event == ftxui::Event::PageUp) {
                this->_page = (this->_page + static_cast<int>(this->_pages.size()) - 1) % static_cast<int>(this->_pages.size());
                this->_item = 0;
            } else if (event == ftxui::Event::ArrowDown) {
                this->_item = std::min(this->_item + 1, std::max(0, count - 1));
            } else if (event == ftxui::Event::ArrowUp) {
                this->_item = std::max(this->_item - 1, 0);
            } else if (count > 0 && (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight || event == ftxui::Event::Return
                || event == ftxui::Event::Character(' '))) {
                const cluster::Setting& setting = page.settings[static_cast<std::size_t>(this->_item)];
                const bool cycle = setting.kind == cluster::SettingKind::Bool || setting.kind == cluster::SettingKind::Choice;
                const bool number = setting.kind == cluster::SettingKind::Int || setting.kind == cluster::SettingKind::Float;
                if (event == ftxui::Event::Return && !cycle) {
                    this->_edit = this->settingValue_(setting);
                    this->_editing = true;
                } else if (cycle || (number && event != ftxui::Event::Return && event != ftxui::Event::Character(' '))) {
                    this->setSetting_(setting, cluster::setting_next(setting, this->settingValue_(setting), event == ftxui::Event::ArrowLeft ? -1 : 1));
                }
            }
            return true;
        }

        _nodiscard ftxui::Element settings_(const ftxui::Dimensions& size)
        {
            const Theme& t = this->_theme;
            ftxui::Elements tabs;
            for (std::size_t i = 0; i < this->_pages.size(); ++i) {
                const bool active = static_cast<int>(i) == this->_page;
                ftxui::Element tab = ftxui::text("  " + this->_pages[i].title + "  ");
                tabs.push_back(active ? tab | ftxui::bold | ftxui::color(t.bg) | ftxui::bgcolor(t.accent) : tab | ftxui::color(t.text));
            }
            const cluster::SettingsPage& page = this->_pages[static_cast<std::size_t>(this->_page)];
            ftxui::Elements rows;
            std::string help;
            bool restart = false;
            for (std::size_t i = 0; i < page.settings.size(); ++i) {
                const cluster::Setting& setting = page.settings[i];
                const bool selected = static_cast<int>(i) == this->_item;
                const bool changed = this->_changes.contains({setting.section, setting.key});
                std::string value = this->settingValue_(setting);
                if (selected && this->_editing) value = this->_edit + "▏";
                else if (setting.kind == cluster::SettingKind::Bool) value = value == "true" ? "Enabled" : "Disabled";
                else if (setting.kind == cluster::SettingKind::Choice) value = "< " + (value.empty() ? std::string("(agent default)") : value) + " >";
                else if (value.empty()) value = "(empty)";
                const std::string label = setting.label + " ";
                const std::size_t dots = label.size() < 34 ? 34 - label.size() : 1;
                ftxui::Element line = ftxui::hbox({
                    ftxui::text(changed ? " * " : "   ") | ftxui::color(t.warn) | ftxui::bold,
                    ftxui::text(label + std::string(dots, '.') + " ") | ftxui::color(t.muted),
                    ftxui::text("[" + cluster::one_line(value, 60) + "]") | (selected ? ftxui::color(t.bg) | ftxui::bgcolor(t.accent) : ftxui::color(t.text)) | ftxui::bold,
                    ftxui::filler(),
                });
                rows.push_back(line);
                if (selected) {
                    help = setting.description;
                    restart = setting.restart;
                }
            }
            if (rows.empty()) rows.push_back(ftxui::text("   nothing here") | ftxui::color(t.muted));
            std::string note = this->_note == "unsaved" ? "Unsaved changes: Escape again to drop them, F10 to save" : this->_note;
            ftxui::Element helpBox = ftxui::vbox({
                ftxui::text(" Item Specific Help") | ftxui::bold | ftxui::color(t.accent),
                ftxui::separatorLight() | ftxui::color(t.border),
                ftxui::paragraph(help) | ftxui::color(t.text),
                ftxui::text(""),
                restart ? ftxui::paragraph("Applied at the next start of claude-cluster") | ftxui::color(t.warn) : ftxui::emptyElement(),
                ftxui::filler(),
                ftxui::text(std::to_string(this->_changes.size()) + " change(s) not saved") | ftxui::color(this->_changes.empty() ? t.muted : t.warn),
                ftxui::paragraph(cluster::short_path(this->_manager.config().path().string())) | ftxui::color(t.muted) | ftxui::dim,
            }) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 38);
            return ftxui::vbox({
                ftxui::hbox({ftxui::text(" claude-cluster setup ") | ftxui::bold | ftxui::color(t.bg) | ftxui::bgcolor(t.accent), ftxui::filler(),
                    ftxui::text(" config.toml ") | ftxui::color(t.muted)}),
                ftxui::hbox(std::move(tabs)) | ftxui::bgcolor(t.surface),
                ftxui::separatorHeavy() | ftxui::color(t.accent),
                ftxui::hbox({ftxui::vbox(std::move(rows)) | ftxui::yframe | ftxui::flex, ftxui::separatorLight() | ftxui::color(t.border), helpBox}) | ftxui::flex,
                ftxui::separatorHeavy() | ftxui::color(t.accent),
                ftxui::text(" " + note) | ftxui::color(t.warn),
                ftxui::hbox({ftxui::text(" ↑↓ select   ←→ / Enter change   Enter edit text   Tab / Shift+Tab section   F10 save & exit   Esc exit ")
                    | ftxui::color(t.bg) | ftxui::bgcolor(t.muted), ftxui::filler()}) | ftxui::bgcolor(t.muted),
            }) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, size.dimy) | ftxui::color(t.text) | ftxui::bgcolor(t.bg);
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
                ftxui::text("  mode " + s.spec.mode) | ftxui::color(s.spec.mode == "bypassPermissions" || s.spec.mode == "dontAsk" ? this->_theme.warn : this->_theme.muted),
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
            if (this->_mode == Mode::History) {
                ftxui::Elements items;
                const int start = std::max(0, this->_selected - 12);
                for (int i = start; i < static_cast<int>(this->_historyItems.size()) && i < start + 24; ++i) {
                    ftxui::Element item = ftxui::text(" " + cluster::one_line(this->_historyItems[static_cast<std::size_t>(i)], 110) + " ");
                    items.push_back(i == this->_selected ? item | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) : item | ftxui::color(this->_theme.text));
                }
                if (items.empty()) items.push_back(ftxui::text(" no prompt yet") | ftxui::color(this->_theme.muted));
                return box(ftxui::vbox({ftxui::hbox({ftxui::text(" history > ") | ftxui::color(this->_theme.accent), ftxui::text(this->_filter), ftxui::text("▏") | ftxui::blink}),
                    ftxui::separatorLight(), ftxui::vbox(std::move(items)), ftxui::text(" Enter: put in the prompt · Esc: close") | ftxui::dim}), 120);
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
            ftxui::Element input = this->prompt_(size.dimx);
            ftxui::Element document = ftxui::vbox({
                top,
                this->body_(size.dimx, bodyHeight) | ftxui::flex,
                this->voice_(),
                this->completion_(),
                ftxui::separatorLight() | ftxui::color(this->_theme.border),
                this->images_(),
                input,
                ftxui::text(" " + status) | ftxui::color(error ? this->_theme.error : this->_theme.muted),
            }) | ftxui::color(this->_theme.text) | ftxui::bgcolor(this->_theme.bg);
            if (this->_mode == Mode::Settings) return this->settings_(size);
            if (this->_mode == Mode::Normal) return document;
            return ftxui::dbox({document, this->modal_()});
        }

        /* events */
        bool event_(ftxui::Event event)
        {
            if (event == ftxui::Event::Custom) return true;
            if (event.is_mouse()) {
                const ftxui::Mouse& mouse = event.mouse();
                this->_hoverImage = -1;
                for (std::size_t i = 0; i < this->_imageBoxes.size(); ++i)
                    if (this->_imageBoxes[i].Contain(mouse.x, mouse.y)) this->_hoverImage = static_cast<int>(i);
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
                case Mode::Settings:
                    return this->settingsEvent_(event);
                case Mode::History:
                    if (event == ftxui::Event::Escape) {
                        this->_mode = Mode::Normal;
                    } else if (event == ftxui::Event::ArrowDown) {
                        this->_selected = std::min(this->_selected + 1, std::max(0, static_cast<int>(this->_historyItems.size()) - 1));
                    } else if (event == ftxui::Event::ArrowUp || cluster::key_action(this->_manager.config(), key) == "history") {
                        this->_selected = std::max(0, this->_selected - 1);
                    } else if (event == ftxui::Event::Return) {
                        if (!this->_historyItems.empty()) this->_editor.setText(this->_historyItems[static_cast<std::size_t>(this->_selected)]);
                        this->_mode = Mode::Normal;
                    } else if (event == ftxui::Event::Backspace) {
                        if (!this->_filter.empty()) this->_filter.pop_back();
                        this->historyFilter_();
                    } else if (event.is_character()) {
                        this->_filter += event.character();
                        this->_selected = 0;
                        this->historyFilter_();
                    }
                    return true;
                case Mode::Normal:
                    break;
            }

            // prefix (Ctrl+B) then an arrow: change of session
            if (this->_prefixUntil > 0) {
                const bool armed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() <= this->_prefixUntil;
                this->_prefixUntil = 0;
                this->_status.clear();
                if (armed && (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight || event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown)) {
                    this->moveDir_(event == ftxui::Event::ArrowLeft ? -1 : event == ftxui::Event::ArrowRight ? 1 : 0,
                        event == ftxui::Event::ArrowUp ? -1 : event == ftxui::Event::ArrowDown ? 1 : 0);
                    return true;
                }
            }
            // completion list open: Tab completes, Enter accepts, arrows choose, Space / Esc close
            if (this->_editor.completing()) {
                if (event == ftxui::Event::Tab) {
                    this->_editor.tab();
                    return true;
                } else if (event == ftxui::Event::Return) {
                    this->_editor.accept();
                    return true;
                } else if (event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown) {
                    this->_editor.select(event == ftxui::Event::ArrowUp ? -1 : 1);
                    return true;
                } else if (event == ftxui::Event::Escape) {
                    this->_editor.dismiss();
                    return true;
                }
            }
            if (this->shortcut_(cluster::key_action(this->_manager.config(), key))) return true;
            const std::optional<Arrow> arrow = arrow_(event);
            if (event == ftxui::Event::PageUp) this->_scroll[this->_active] += 10;
            else if (event == ftxui::Event::PageDown) this->_scroll[this->_active] = std::max(0, this->_scroll[this->_active] - 10);
            else if (event == ftxui::Event::Return) this->submit_();
            else if (event == ftxui::Event::Backspace) this->_editor.backspace();
            else if (event == ftxui::Event::Delete) this->_editor.erase();
            else if (event == ftxui::Event::Home || event.input() == "\x1b[1;2H") this->_editor.home(event != ftxui::Event::Home); // xstyle: ignore LU-ANSI (read)
            else if (event == ftxui::Event::End || event.input() == "\x1b[1;2F") this->_editor.end(event != ftxui::Event::End); // xstyle: ignore LU-ANSI (read)
            else if (arrow && arrow->dx != 0) {
                if (arrow->ctrl) arrow->dx < 0 ? this->_editor.wordLeft(arrow->shift) : this->_editor.wordRight(arrow->shift);
                else arrow->dx < 0 ? this->_editor.left(arrow->shift) : this->_editor.right(arrow->shift);
            } else if (event == ftxui::Event::Character(' ')) this->_editor.space();
            else if (event.is_character()) this->_editor.insert(event.character());
            else return false;
            return true;
        }

    public:
        int run(void)
        {
            this->theme_();
            if (this->_app.askRestore) this->_mode = Mode::Restore;
            this->_editor.setProvider(cluster::completion_provider(this->_manager, [this]() {return this->_active;}));
            termios original{};
            const bool tty = ::tcgetattr(STDIN_FILENO, &original) == 0;
            if (tty) {
                termios raw = original;
                raw.c_iflag &= ~static_cast<tcflag_t>(IXON);
                ::tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            }
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
            if (tty) ::tcsetattr(STDIN_FILENO, TCSANOW, &original);
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
