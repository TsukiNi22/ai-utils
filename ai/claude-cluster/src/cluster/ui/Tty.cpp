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
#define _Encapsulation
#define _SManip
#include <utils/utils.hpp>
#include "cluster/Settings.hpp"
#include "cluster/Services.hpp"
#include "cluster/Display.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Session.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Worker.hpp"
#include "cluster/Editor.hpp"
#include "cluster/Reveal.hpp"
#include "cluster/Icons.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Core.hpp"
#include "cluster/Git.hpp"
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/screen/terminal.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <termios.h>
#include <algorithm>
#include <unistd.h>
#include <optional>
#include <iostream>
#include <csignal>
#include <fstream>
#include <sstream>
#include <cerrno>
#include <atomic>
#include <chrono>
#include <deque>
#include <regex>
#include <cmath>
#include <set>
#include <map>

namespace {
//----------------------------------------------------------------//
/* TYPES */

enum class Mode {Normal, Palette, Prompt, Message, Restore, Settings, History, Search, Machine, Profile, Question, WebApprove, WebDevices};

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

_cold std::vector<std::string> wrap_(const std::string& text, const std::size_t width, std::vector<std::string>* joins = nullptr)
{
    // joins: what was between a line and the previous one ("\n", " " for a break on a space, "" else): copy back
    std::vector<std::string> lines;
    const std::size_t max = std::max<std::size_t>(width, 10);
    std::string line;
    std::size_t size = 0;
    std::string join = "\n";
    const std::function<void(std::string, const std::string&)> push = [&](std::string done, const std::string& next) {
        lines.push_back(std::move(done));
        if (joins) joins->push_back(join);
        join = next;
    };

    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            push(line, "\n");
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
                push(line.substr(0, space), " ");
                line = line.substr(space + 1);
                size = width_(line);
            } else {
                push(line, "");
                line.clear();
                size = 0;
            }
        }
        line += text[i];
        if ((c & 0xC0) != 0x80) ++size;
    }
    return lines;
}

_cold char home_end_(const ftxui::Event& event)
{
    // Home / End as the terminals send them (xterm, VT220, tmux, application mode), Shift+: selection; 0: neither
    static const std::map<std::string, char> keys = {{"\x1b[H", 'H'}, {"\x1bOH", 'H'}, {"\x1b[1~", 'H'}, {"\x1b[7~", 'H'}, {"\x1b[1;2H", 'H'}, // xstyle: ignore LU-ANSI (read)
        {"\x1b[F", 'F'}, {"\x1bOF", 'F'}, {"\x1b[4~", 'F'}, {"\x1b[8~", 'F'}, {"\x1b[1;2F", 'F'}}; // xstyle: ignore LU-ANSI (read)
    auto it = keys.find(event.input());
    return it == keys.end() ? 0 : it->second;
}

_cold std::vector<std::string> lines_(const std::string& text)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t nl = text.find('\n'); nl != std::string::npos; nl = text.find('\n', start)) {
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    lines.push_back(text.substr(start));
    return lines;
}

// The output of the interface: every frame of ftxui (and its other writes) sent in one write, inside a synchronized
// update (DEC 2026: the terminal shows it at once) with the cursor hidden while it is drawn (shown again by the frame
// at its place): no flicker, no cursor running over the screen
class FrameOut: public std::streambuf {
    private:
        std::string _buffer;

    protected:
        int overflow(const int c) override
        {
            if (c != traits_type::eof()) this->_buffer.push_back(static_cast<char>(c));
            return c;
        }

        std::streamsize xsputn(const char* data, const std::streamsize size) override
        {
            this->_buffer.append(data, static_cast<std::size_t>(size));
            return size;
        }

        int sync(void) override
        {
            if (this->_buffer.empty()) return 0;
            const std::string out = "\x1b[?2026h\x1b[?25l" + this->_buffer + "\x1b[?2026l"; // xstyle: ignore LU-ANSI (modes, no color)
            this->_buffer.clear();
            for (std::size_t sent = 0; sent < out.size();) {
                const ssize_t n = ::write(STDOUT_FILENO, out.data() + sent, out.size() - sent);
                if (n <= 0) return n < 0 && errno == EINTR ? 0 : -1;
                sent += static_cast<std::size_t>(n);
            }
            return 0;
        }
};

static constexpr std::int64_t PASTE_GAP_MS = 30; // keys closer than this after an Enter: a paste, not typing

_nodiscard inline std::int64_t now_ms_(void) {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();};

_cold std::size_t byte_at_(const std::string& line, const int column)
{
    std::size_t at = 0;
    for (int c = 0; at < line.size() && c < column; ++c) {
        ++at;
        while (at < line.size() && (static_cast<unsigned char>(line[at]) & 0xC0) == 0x80)
            ++at;
    }
    return at;
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
    if (input.size() == 2 && input[0] == 27 && std::isalnum(static_cast<unsigned char>(input[1])))
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

struct Cell {
    std::string glyph;
    std::optional<ftxui::Color> fg;
    std::optional<ftxui::Color> bg;
};

struct Preview {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;          // magick: pixels drawn with half blocks
    std::vector<std::vector<Cell>> cells;   // chafa: its symbols and colors
};

_cold std::vector<std::vector<Cell>> chafa_(const std::string& path, const int columns, const int rows)
{
    // chafa: block / border / sextant symbols in true colors (a far finer preview than half blocks)
    const cluster::Captured out = cluster::capture({"chafa", "--format", "symbols", "--colors", "full", "--animate", "off", "--polite", "on", "--work", "9",
        "--size", std::to_string(columns) + "x" + std::to_string(rows), path}, "", 10);
    std::vector<std::vector<Cell>> lines;
    if (out.code != 0) return lines;
    std::optional<ftxui::Color> fg;
    std::optional<ftxui::Color> bg;
    bool inverse = false;
    std::vector<Cell> line;
    const std::string& text = out.out;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] == '\x1b' && i + 1 < text.size() && text[i + 1] == '[') { // xstyle: ignore LU-ANSI (read, not written)
            std::size_t end = i + 2;
            while (end < text.size() && !(text[end] >= '@' && text[end] <= '~'))
                ++end;
            if (end < text.size() && text[end] == 'm') {
                const std::vector<std::string> params = cluster::split(text.substr(i + 2, end - i - 2), ';');
                for (std::size_t p = 0; p < params.size(); ++p) {
                    const int code = params[p].empty() ? 0 : std::atoi(params[p].c_str());
                    if (code == 0) {
                        fg.reset();
                        bg.reset();
                        inverse = false;
                    } else if ((code == 38 || code == 48) && p + 4 < params.size() && params[p + 1] == "2") {
                        const ftxui::Color color = ftxui::Color::RGB(static_cast<std::uint8_t>(std::atoi(params[p + 2].c_str())),
                            static_cast<std::uint8_t>(std::atoi(params[p + 3].c_str())), static_cast<std::uint8_t>(std::atoi(params[p + 4].c_str())));
                        (code == 38 ? fg : bg) = color;
                        p += 4;
                    } else if (code == 39) fg.reset();
                    else if (code == 49) bg.reset();
                    else if (code == 7) inverse = true;
                    else if (code == 27) inverse = false;
                }
            }
            i = end + 1;
            continue;
        }
        if (text[i] == '\n') {
            lines.push_back(std::move(line));
            line.clear();
            ++i;
            continue;
        }
        std::size_t next = i + 1;
        while (next < text.size() && (static_cast<unsigned char>(text[next]) & 0xC0) == 0x80)
            ++next;
        line.push_back({text.substr(i, next - i), inverse ? bg : fg, inverse ? fg : bg});
        i = next;
    }
    if (!line.empty()) lines.push_back(std::move(line));
    return lines;
}

_cold const Preview& preview_(const std::string& path, const int columns, const int rows, const bool chafa)
{
    // image -> columns x rows characters: the pixels (magick) drawn with half blocks, or the symbols of chafa
    static std::map<std::string, Preview> cache;
    const std::string key = path + "@" + std::to_string(columns) + "x" + std::to_string(rows) + (chafa ? "c" : "b");
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    Preview preview;
    if (chafa) preview.cells = chafa_(path, columns, rows);
    if (!preview.cells.empty()) return cache.emplace(key, preview).first->second;
    const cluster::Captured out = cluster::capture({"magick", path, "-resize", std::to_string(columns) + "x" + std::to_string(rows * 2), "-depth", "8", "ppm:-"}, "", 10);
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
    return cache.emplace(key, preview).first->second;
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
        bool _historyAll = false;                   // Mode::History: every session (Tab), else the one shown
        std::vector<std::size_t> _chips;            // atoms of the prompt shown as chips (images, pasted texts)
        std::vector<ftxui::Box> _chipBoxes;
        int _hoverChip = -1;
        int _hover = -1;                            // the click target under the mouse (its index in _clicks), lit
        std::map<int, std::string> _rowSessions;    // the click targets that are sessions (a right click: their menu)
        struct PromptRow {
            std::size_t from = 0;                   // first byte shown
            std::size_t end = 0;
            int offset = 3;                         // cells before the text (" › ", "…")
        };
        std::vector<PromptRow> _promptRows;         // the rows of the prompt drawn (mouse -> position)
        ftxui::Box _promptBox;
        using Hit = std::tuple<int, int, std::function<void(void)>>;   // the cells [from, to] of a line clicked: what it does
        struct View {
            ftxui::Box box;
            std::vector<std::string> lines;         // transcript lines shown (mouse selection)
            std::vector<std::string> joins;
            std::vector<std::vector<Hit>> hits;     // per line: its parts clicked (a display: tick, a button, play...)
        };
        std::map<std::string, View> _views;         // per session
        struct Selection {
            std::string session;
            int x0 = 0;                             // cells, from the top left of the transcript
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
        };
        std::optional<Selection> _selection;        // mouse selection in a transcript (copied when released)
        std::string _drag;                          // "prompt", a session id, or empty
        struct Click {
            ftxui::Box box{-1, -1, -1, -1};
            int layer = 0;                          // 0: the screen, 1: a dialog over it, 2: the settings page
            int clip = -1;                          // the box it must be in too (a list that scrolls): its index
            std::function<bool(void)> act{};        // false: not for it (the mouse selection goes on); none: a dialog that stops the click
        };
        std::deque<Click> _clicks;                  // what the mouse clicks, made again by each drawing
        std::string _title;                         // the title of the terminal set (its face)
        bool _askChat = false;                      // a question: its chat being typed ("Chat about this")
        std::string _askDraft;
        std::string _host = cluster::host_name();   // where the sessions run
        std::map<std::string, std::string> _branches;   // their git branch, read again every 5 s
        std::map<std::string, std::int64_t> _branchAt;
        std::uint64_t _raiseSeen = 0;               // the last notification clicked
        std::string _webAsk;                        // the device asking to connect shown (its popup)
        std::set<std::string> _webLater;            // the ones put off (Esc): asked again from the palette
        std::set<std::string> _webPicked;           // the web devices ticked ("d:<id>" connected / waiting, "t:<id>" trusted)
        int _webCursor = 0;
        std::string _webConfirm;                    // a dangerous action of the devices waiting for its y (ban, kick_all)
        std::int64_t _lastEscape = 0;               // Esc Esc: clear the prompt
        std::int64_t _lastInterrupt = 0;            // Ctrl+C Ctrl+C on an empty prompt, nothing running: quit
        std::uint64_t _machineSeen = 0;             // the map of this computer asked by the global session (its question shown)
        std::atomic<bool> _sendPending{false};      // Enter waits a few ms: a key right after it is a pasted line break
        std::int64_t _sendAt = 0;                   // ms of that Enter
        std::int64_t _lastKey = 0;                  // ms of the last key / character, and of the one before
        std::int64_t _keyBefore = 0;
        std::optional<std::string> _pasting;        // bracketed paste of the terminal: its text until ESC[201~
        cluster::Worker _worker;                    // the sends / commands, in their order, out of the drawing thread
        cluster::Reveal _reveal;                    // the answers typed as they come
        std::atomic<bool> _animating{false};        // typing / spinner: drawn often
        bool _expanded = false;                     // Ctrl+O: thinking and tool results in full
        std::uint64_t _focusSeen = 0;               // last ui_focus applied
        bool _machineStartup = false;               // the map of this computer asked at start (no: never asked again)
        int _machineKind = 0;                       // 1 no map yet, 2 an old one (its sync proposed)
        std::vector<cluster::ProfileQuestion> _questions;   // about the user, before a scan
        std::size_t _question = 0;
        std::string _answer;

        void profileNext_(const bool save)
        {
            // the answer kept (config.toml [user]), the next question; after the last one: the scan
            if (save) {
                try {
                    (void)cluster::settings_set(this->_manager.config(), "user", this->_questions[this->_question].key, cluster::trim(this->_answer));
                } catch (const utils::exception::IException& e) {
                    this->status_(e.info(), true);
                }
            }
            if (++this->_question < this->_questions.size()) {
                this->_answer = this->_questions[this->_question].value;
                return;
            }
            (void)this->_manager.config().reload();
            this->_manager.scanMachine();
            this->status_("mapping this computer…");
            this->_mode = this->_machineReturn == Mode::Machine || this->_machineReturn == Mode::Profile ? Mode::Normal : this->_machineReturn;
        }
        std::uint64_t _scanHidden = 0;              // the scan whose box was closed (Esc: it goes on)
        std::uint64_t _scanReported = 0;            // the scan whose end was told in the status line

        _nodiscard std::optional<cluster::MachineProgress> scanShown_(void) const
        {
            // the box of the scan: while it runs, then 4 s (unless closed)
            const std::optional<std::pair<cluster::MachineProgress, std::int64_t>> scan = this->_manager.machineProgress();
            if (!scan || scan->first.id == this->_scanHidden) return std::nullopt;
            const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            if (scan->second != 0 && now - scan->second > 4000) return std::nullopt;
            return scan->first;
        }

        _nodiscard ftxui::Element scanBox_(const cluster::MachineProgress& p) const
        {
            // the steps checked, the bar, the last lines done, the one running, the next one
            const Theme& t = this->_theme;
            // the steps on rows of 4, each one in a cell of its own (never glued to the one before)
            ftxui::Elements rows;
            ftxui::Elements row;
            for (const auto &[name, state]: p.steps) {
                const std::string mark = state == 2 ? "✓ " : state == 1 ? "◌ " : "· ";
                row.push_back(ftxui::text(" " + mark + name) | ftxui::color(state == 2 ? t.ok : state == 1 ? t.accent : t.muted) | (state == 1 ? ftxui::bold : ftxui::nothing)
                    | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 21));
                if (row.size() == 4) {
                    rows.push_back(ftxui::hbox(std::move(row)));
                    row.clear();
                }
            }
            if (!row.empty()) rows.push_back(ftxui::hbox(std::move(row)));
            ftxui::Elements lines = {
                ftxui::hbox({ftxui::text(p.done ? " This computer mapped" : " Mapping this computer") | ftxui::bold | ftxui::color(p.done ? t.ok : t.text), ftxui::filler(),
                    ftxui::text(std::to_string(static_cast<int>(p.fraction * 100)) + "% ") | ftxui::color(t.accent)}),
                ftxui::hbox({ftxui::text(" "), ftxui::gauge(static_cast<float>(p.fraction)) | ftxui::color(p.done ? t.ok : t.accent) | ftxui::flex, ftxui::text(" ")}),
                ftxui::text(""), ftxui::vbox(std::move(rows)), ftxui::text("")};
            for (const std::string& line: p.log)
                lines.push_back(ftxui::text(" ✓ " + cluster::one_line(line, 80)) | ftxui::color(t.ok));
            if (!p.now.empty()) lines.push_back(ftxui::text(" ◌ " + cluster::one_line(p.now, 80)) | ftxui::color(t.text));
            if (!p.next.empty()) lines.push_back(ftxui::text(" · next: " + p.next) | ftxui::color(t.muted) | ftxui::dim);
            lines.push_back(ftxui::text(""));
            lines.push_back(ftxui::text(p.done ? " Esc: close" : " Esc: hide (the scan goes on)") | ftxui::color(t.muted) | ftxui::dim);
            return ftxui::vbox(std::move(lines)) | ftxui::borderRounded | ftxui::color(t.border) | ftxui::bgcolor(t.surface)
                | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 88) | ftxui::center;
        }
        std::uint64_t _progressHidden = 0;          // the popup of a long operation hidden (Esc)

        ftxui::Element failureBox_(const std::vector<cluster::Failure>& failed)
        {
            // what the user did, where it failed, its error; OK (Enter, the default) / Retry (r), clickable
            const Theme& t = this->_theme;
            const cluster::Failure& f = failed.front();
            const std::string id = f.id;
            ftxui::Elements lines = {
                ftxui::hbox({ftxui::text(" ✗ ") | ftxui::bold | ftxui::color(t.error), ftxui::paragraph(f.action + " failed") | ftxui::bold | ftxui::color(t.text) | ftxui::flex,
                    failed.size() > 1 ? ftxui::text(" +" + std::to_string(failed.size() - 1) + " more ") | ftxui::color(t.muted) : ftxui::text("")}),
                ftxui::text(""),
                ftxui::hbox({ftxui::text("   source  ") | ftxui::color(t.muted), ftxui::paragraph(f.source) | ftxui::color(t.text) | ftxui::flex}),
                ftxui::hbox({ftxui::text("   error   ") | ftxui::color(t.muted), ftxui::paragraph(f.error) | ftxui::color(t.error) | ftxui::flex}),
                ftxui::text("")};
            ftxui::Elements buttons = {ftxui::text("   "), this->tap_(ftxui::text(" [Enter] OK ") | ftxui::bold | ftxui::color(t.bg) | ftxui::bgcolor(t.accent), 9,
                [this, id]() {this->_manager.failureAnswer(id, false);})};
            if (f.retry) {
                buttons.push_back(ftxui::text("   "));
                buttons.push_back(this->tap_(ftxui::text(" [r] Retry ") | ftxui::bold | ftxui::color(t.warn), 9, [this, id]() {this->_manager.failureAnswer(id, true);}));
            }
            lines.push_back(ftxui::hbox(std::move(buttons)));
            return ftxui::vbox(std::move(lines)) | ftxui::borderRounded | ftxui::color(t.error) | ftxui::bgcolor(t.surface) | ftxui::clear_under
                | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 88) | ftxui::center;
        }

        _nodiscard ftxui::Element progressBox_(const cluster::Progress& p) const
        {
            // its title and a spinner (its time), what it does now, its last lines greyed under it
            const Theme& t = this->_theme;
            static const char* spinner[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
            const std::int64_t now = cluster::now_ms();
            const std::int64_t took = ((p.done ? p.ended : now) - p.started) / 1000;
            const std::string mark = p.done ? (p.ok ? "✓" : "✗") : spinner[(now / 90) % 10];
            ftxui::Elements lines = {ftxui::hbox({ftxui::text(" " + mark + " ") | ftxui::bold | ftxui::color(p.done ? (p.ok ? t.ok : t.error) : t.accent),
                ftxui::text(p.title) | ftxui::bold | ftxui::color(t.text), ftxui::filler(), ftxui::text(std::to_string(took) + "s ") | ftxui::color(t.muted)}), ftxui::text("")};
            if (!p.now.empty()) lines.push_back(ftxui::paragraph(" " + (p.done ? "" : std::string("◌ ")) + p.now) | ftxui::color(p.done ? (p.ok ? t.ok : t.error) : t.text));
            for (const std::string& line: p.log)
                lines.push_back(ftxui::text("   " + cluster::one_line(line, 80)) | ftxui::color(t.muted) | ftxui::dim);
            lines.push_back(ftxui::text(""));
            lines.push_back(ftxui::text(p.done ? " Esc: close" : " Esc: hide (it goes on)") | ftxui::color(t.muted) | ftxui::dim);
            return ftxui::vbox(std::move(lines)) | ftxui::borderRounded | ftxui::color(t.border) | ftxui::bgcolor(t.surface) | ftxui::clear_under
                | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 88) | ftxui::center;
        }

        Mode _machineReturn = Mode::Normal;         // where its question goes back to

        void terminalModes_(void)
        {
            // bracketed paste, focus reporting: at start and after a program in the foreground (an editor turns them off)
            std::cout << "\x1b[?2004h\x1b[?1004h" << std::flush; // xstyle: ignore LU-ANSI (modes of the terminal, no color)
        }

        void settingsAction_(const std::string& key)
        {
            // an action of the settings page: the map of this computer (asked first), the system prompt of the global
            // session or of the sub-sessions
            if (key == "machine_sync") {
                this->_machineReturn = this->_mode;
                this->_mode = Mode::Machine;
            } else if (key == "edit_prompt" || key == "edit_sessions_prompt" || key == "edit_vocabulary" || key == "edit_words") {
                const bool global = key == "edit_prompt";
                const bool vocabulary = key == "edit_vocabulary" || key == "edit_words"; // a list of the voice: no restart
                const cluster::Config& config = this->_manager.config();
                const std::filesystem::path path = cluster::expand_home(global ? config.globalPrompt : key == "edit_words" ? cluster::voice_words_path(config.voice.words).string() : vocabulary ? cluster::vocabulary_path(config.voice.vocabulary).string() : config.sessionsPrompt);
                std::error_code error;
                const std::filesystem::file_time_type before = std::filesystem::last_write_time(path, error);
                std::vector<std::string> command = this->editorCommand_();
                command.push_back(path.string());
                this->_screen.WithRestoredIO([&]() {
                    utils::encapsulation::Process process;
                    try {
                        (void)process.spawn(command[0], std::vector<std::string>(command.begin() + 1, command.end()));
                        (void)process.wait();
                    } catch (const utils::exception::IException& e) {
                        std::cerr << e.info() << std::endl;
                    }
                })();
                this->terminalModes_();
                if (std::filesystem::last_write_time(path, error) == before) {
                    this->_note = vocabulary ? "vocabulary unchanged" : "system prompt unchanged";
                    return;
                }
                if (vocabulary) {
                    this->_note = "vocabulary saved: used from the next sentence heard";
                    return;
                }
                if (!global) {
                    this->_note = "system prompt of the sub-sessions saved: taken by each one at its next start (/clear, /model...)";
                    return;
                }
                try {
                    this->_manager.restartGlobal();
                    this->_note = "system prompt saved: the global session started again (same conversation)";
                } catch (const utils::exception::IException& e) {
                    this->_note = std::string("system prompt saved, the global session not restarted: ") + e.info();
                }
            }
        }
        std::string _styleSeen;                     // style of the theme
        int _historyAt = -1;                        // Up / Down: -1 the prompt being written, else an index of the history
        std::vector<cluster::Draft> _historyList;   // the history when the browsing started (newest first)
        std::string _historySession;                // the session of that list
        std::map<int, std::string> _historyEdits;   // the texts changed while browsing (forgotten when a prompt is sent)

        bool historyStep_(const int delta)
        {
            // +1 older, -1 newer, never looped; the text left (changed or not) is kept for when it comes back; the prompts
            // of the session shown only (Ctrl+R: every session), from the newest again when it changed
            if (this->_historySession != this->_active) {
                this->_historyAt = -1;
                this->_historyEdits.clear();
            }
            if (this->_historyAt == -1 && delta > 0) {
                this->_historyList = this->_manager.drafts(500, this->_active);
                this->_historySession = this->_active;
            }
            const int to = this->_historyAt + delta;
            if (to < -1 || to >= static_cast<int>(this->_historyList.size())) return false;
            this->_historyEdits[this->_historyAt] = this->_editor.text();
            this->_historyAt = to;
            const auto edited = this->_historyEdits.find(to);
            if (edited != this->_historyEdits.end()) this->_editor.setText(edited->second);
            else this->_editor.load(this->_historyList[static_cast<std::size_t>(to)]); // its images and pasted texts as blocks
            this->status_(to == -1 ? "back to the prompt being written" : "history " + std::to_string(to + 1) + "/" + std::to_string(this->_historyList.size()) + " (↓ newer)");
            return true;
        }

        void rewind_(void)
        {
            // the last turn leaves the conversation (claude started again: in the worker), its prompt comes back
            const std::string session = this->_active;
            this->status_("taking the last prompt back…");
            this->_worker.push([this, session]() {
                std::optional<cluster::Queued> prompt;
                std::string error;
                try {
                    prompt = this->_manager.rewind(session);
                } catch (const utils::exception::IException& e) {
                    error = e.info();
                }
                this->_screen.Post([this, prompt, error]() {
                    if (!prompt) {
                        this->status_(error, true);
                        return;
                    }
                    if (!cluster::trim(this->_editor.text()).empty()) this->_manager.stashPush(this->_editor.draft()); // typed meanwhile: kept
                    this->_editor.load({prompt->text, prompt->images, {}});
                    this->historyReset_();
                    this->status_("the last prompt left the conversation: back here to edit (Esc Esc on an empty prompt: the one before)");
                });
                this->_screen.PostEvent(ftxui::Event::Custom);
            });
        }

        void historyReset_(void)
        {
            // a prompt sent: the browsing and its changes are forgotten
            this->_historyAt = -1;
            this->_historyEdits.clear();
            this->_historyList.clear();
        }

        void vimSync_(void)
        {
            // [ui] vim of the config, or /vim (kept in the state)
            const bool on = this->_manager.ui("vim", this->_manager.config().vim ? "true" : "false") == "true";
            this->_editor.setVim(on);
            this->_editor.setPasteLimits(this->_manager.config().pasteLines, this->_manager.config().pasteChars);
        }
        std::string _search;                        // Ctrl+F in the transcript of the active session
        std::vector<std::size_t> _matches;          // entries matching
        int _hit = 0;                               // the current one (index in _matches)
        std::string _filter;
        std::string _prompt;
        std::string _message;
        bool _messageError = false;
        int _selected = 0;
        std::int64_t _prefixUntil = 0;              // Ctrl+B pressed: unix ms until which an arrow moves between sessions
        int _taskSel = 0;                           // the Tasks session: its item chosen by the keys
        cluster::Json _services;                    // the Services tab of the settings: its state (read by the worker)
        std::int64_t _servicesAt = 0;               // ms of its last read (0: read again)
        std::uint64_t _servicesSeen = 0;            // the version of the core it was read at (live: read again when it changes)
        std::atomic<bool> _servicesReading{false};
        int _svcButton = 0;                         // the button of the service chosen (←→)
        std::string _svcConfirm;                    // "service|action" waiting for its y / n
        std::vector<std::pair<int, std::function<void(void)>>> _taskItems; // its items drawn (their part, what Enter does)
        std::vector<cluster::SettingsPage> _pages;  // settings page
        int _page = 0;
        int _item = 0;
        bool _finding = false;                      // the search of the settings page (/)
        bool _dangerOk = false;                     // the dangerous changes confirmed (their popup at the save)
        cluster::Requirement _need;                 // what the setting chosen needs and is missing (its help, [i] install)
        bool _saveExit = true;                      // the save waiting for that popup: the page closed after
        std::string _find;
        std::vector<cluster::SettingHit> _hits;
        int _findHit = 0;
        bool _editing = false;
        std::string _edit;
        std::string _note;
        std::map<std::pair<std::string, std::string>, std::pair<cluster::Setting, std::string>> _changes;
        cluster::Action _pending;                   // action waiting for its input
        std::vector<cluster::Action> _actions;      // palette, filtered
        std::map<std::string, int> _scroll;         // lines scrolled up, per session
        std::map<std::string, std::vector<std::vector<cluster::Cell>>> _pictures;   // the pictures of the displays (chafa), per entry and width
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

        _nodiscard std::vector<std::string> ring_(void) const
        {
            // what next / previous go through: the Tasks session first, then the sessions in their order
            std::vector<std::string> ids = {TASKS_ID};
            for (const cluster::Snapshot& s: this->sessions_())
                ids.push_back(s.spec.id);
            return ids;
        }

        void move_(const int delta)
        {
            const std::vector<std::string> ids = this->ring_();
            auto it = std::find(ids.begin(), ids.end(), this->_active);
            int index = it == ids.end() ? 0 : static_cast<int>(it - ids.begin());
            index = (index + delta + static_cast<int>(ids.size())) % static_cast<int>(ids.size());
            this->_active = ids[static_cast<std::size_t>(index)];
        }

        void moveDir_(const int dx, const int dy)
        {
            // Session on the left / right / above / below in the current layout (grid: rows and columns; list, tabs: previous / next)
            const std::vector<cluster::Snapshot> list = this->sessions_();
            const int count = static_cast<int>(list.size());
            const std::string layout = this->_manager.ui("layout", this->_manager.config().layout);
            const int columns = layout == "grid" && count > 1 ? static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))) : 1;
            // the Tasks session: in the ring of next / previous (first); the rows of the grid: its sessions only
            if (count == 0 || this->_active == TASKS_ID || dx != 0 || columns == 1) {
                this->move_(dx != 0 ? dx : dy);
                return;
            }
            auto it = std::find_if(list.begin(), list.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->_active;});
            const int index = it == list.end() ? 0 : static_cast<int>(it - list.begin());
            int target = index;
            if (dx != 0 || columns == 1) target = (index + (dx != 0 ? dx : dy) + count) % count;
            else if (index + dy * columns >= 0 && index + dy * columns < count) target = index + dy * columns;
            this->_active = list[static_cast<std::size_t>(target)].spec.id;
        }

        void pick_(const std::string& prefix)
        {
            // a command typed alone (/model, /mode, /effort): the palette on its choices, the current one marked
            this->_filter = prefix;
            this->_selected = 0;
            this->filter_();
            for (std::size_t i = 0; i < this->_actions.size(); ++i)
                if (this->_actions[i].title.find("(current)") != std::string::npos) this->_selected = static_cast<int>(i);
            this->_mode = Mode::Palette;
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

        void searchUpdate_(void)
        {
            this->_matches.clear();
            const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(this->_active);
            const std::string wanted = cluster::lower(this->_search);
            if (!snap || wanted.empty()) return;
            for (std::size_t i = 0; i < snap->entries.size(); ++i)
                if (cluster::lower(snap->entries[i].text).find(wanted) != std::string::npos) this->_matches.push_back(i);
            this->_hit = this->_matches.empty() ? 0 : static_cast<int>(this->_matches.size()) - 1; // the newest first
        }

        void externalEdit_(void)
        {
            // the prompt in $VISUAL / $EDITOR (the interface comes back after)
            const std::filesystem::path path = cluster::data_dir() / "prompt-edit.md";
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            std::ofstream(path) << this->_editor.plain(); // the pasted texts in full
            std::vector<std::string> command = this->editorCommand_();
            command.push_back(path.string());
            this->_screen.WithRestoredIO([&]() {
                utils::encapsulation::Process process;
                try {
                    (void)process.spawn(command[0], std::vector<std::string>(command.begin() + 1, command.end()));
                    (void)process.wait();
                } catch (const utils::exception::IException& e) {
                    std::cerr << e.info() << std::endl;
                }
            })();
            this->terminalModes_();
            std::ifstream in(path);
            std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
            this->_editor.setText(this->_editor.collapse(text)); // a pasted text left unchanged: its block again
        }

        _nodiscard std::vector<std::string> editorCommand_(void) const
        {
            // [ui] editor, else the first installed: nvim, vim, vi, $VISUAL, $EDITOR, emacs -nw, nano
            const std::string& chosen = this->_manager.config().editor;
            if (!cluster::trim(chosen).empty()) return cluster::split(cluster::trim(chosen), ' ');
            const std::string& main = this->_manager.config().user.editor; // the main editor of the user ([user])
            if (!cluster::trim(main).empty() && cluster::has_command(cluster::split(cluster::trim(main), ' ')[0])) return cluster::split(cluster::trim(main), ' ');
            for (const char* name: {"nvim", "vim", "vi"})
                if (cluster::has_command(name)) return {name};
            for (const char* variable: {"VISUAL", "EDITOR"}) {
                const char* value = std::getenv(variable);
                if (value && *value) return cluster::split(value, ' ');
            }
            if (cluster::has_command("emacs")) return {"emacs", "-nw"};
            return {"nano"};
        }

        void historyFilter_(void)
        {
            const std::string wanted = cluster::lower(this->_filter);
            this->_historyItems.clear();
            for (const std::string& text: this->_manager.history(500, this->_historyAll ? "" : this->_active))
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
                    const std::filesystem::path path = cluster::data_dir() / "images" / ("img-" + cluster::content_hash(image.out) + extension);
                    if (!std::filesystem::exists(path, error)) std::ofstream(path, std::ios::binary) << image.out;
                    if (!this->_editor.addImage(path.string())) this->status_("the same image: one more reference to it (sent once)");
                    return;
                }
            }
            const cluster::Captured text = cluster::capture({"wl-paste", "--no-newline"}, "", 5);
            if (text.code == 0) this->_editor.paste(text.out);
            else this->status_("clipboard unreadable (wl-paste)", true);
        }

        void enter_(void)
        {
            // Enter right after a key (a paste without brackets): a new line; else sent when no key follows (Custom event)
            if (this->_keyBefore > 0 && this->_lastKey - this->_keyBefore < PASTE_GAP_MS / 3) {
                this->_editor.insert("\n");
                return;
            }
            this->_sendAt = this->_lastKey;
            this->_sendPending = true;
        }

        void submit_(void)
        {
            // the prompt leaves at once (feedback), the send / the local command runs in the worker (it may start or stop
            // a claude: seconds), its result comes back here; an error: the prompt back, nothing lost
            const std::string text = cluster::trim(this->_editor.plain());
            if (text.empty() || this->_active == TASKS_ID) return; // the Tasks session: no prompt
            const cluster::Draft draft = this->_editor.draft();
            const std::vector<std::string> images = this->_editor.images();
            const std::string session = this->_active;
            this->_editor.clear();
            this->historyReset_();
            this->_scroll[session] = 0;
            this->status_(text.starts_with("/") ? cluster::split(text, ' ')[0] + "…" : "sent");
            this->_worker.push([this, text, draft, images, session]() {
                std::optional<cluster::ActionResult> local;
                std::string error;
                try {
                    local = cluster::run_slash(this->_manager, session, text);
                    if (!local && session == TASKS_ID) local = cluster::tasks_prompt(this->_manager, text); // a new task / reminder
                    else if (!local) this->_manager.send(session, text, false, images);
                    this->_manager.addHistory(draft, session); // typed here: in the history (not the ones of the global session / MCP)
                } catch (const utils::exception::IException& e) {
                    error = std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info());
                    const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(session);
                    (void)this->_manager.fail(text.starts_with("/") ? cluster::one_line(text, 70) : "Send the prompt \"" + cluster::one_line(text, 50) + "\"",
                        snap ? snap->spec.name + " (" + session + ")" : session, error, [this, session, text, images]() {this->_manager.send(session, text, false, images);});
                }
                this->_screen.Post([this, local, error, draft]() {
                    if (!error.empty()) {
                        this->status_(error, true);
                        if (cluster::trim(this->_editor.text()).empty()) this->_editor.load(draft);
                        return;
                    }
                    if (!local) return;
                    if (!local->focus.empty()) this->_active = local->focus;
                    if (local->ui == "fill") this->_editor.load({local->fill, {}, {}}); // a :s of the last prompt, a :r: in the prompt
                    else if (local->ui == "quit") this->_screen.Exit();
                    else if (local->ui.starts_with("search:")) {
                        this->_search = local->ui.substr(7);
                        this->_matches.clear();
                        this->_hit = 0;
                        this->_mode = Mode::Search;
                        this->searchUpdate_();
                    }
                    if (local->ui == "settings") this->openSettings_();
                    else if (local->ui == "restore_menu") this->openRestore_();
                    else if (local->ui == "rewind") this->rewind_();
                    else if (local->ui.starts_with("pick:")) this->pick_(local->ui.substr(5));
                    if (!local->message.empty()) this->status_(local->message, local->error);
                    else if (local->ui.empty()) this->status_("done");
                });
                this->_screen.PostEvent(ftxui::Event::Custom);
            });
        }

        _nodiscard ftxui::Element prompt_(const int width)
        {
            // the prompt (several lines: Alt+Enter / Shift+Enter): selection inverted, cursor, 8 lines around the cursor
            this->_promptRows.clear();
            if (this->_mode == Mode::Search) {
                return ftxui::hbox({ftxui::text(" find › ") | ftxui::color(this->_theme.warn) | ftxui::bold, ftxui::text(this->_search) | ftxui::color(this->_theme.text),
                    ftxui::text("▏") | ftxui::blink, ftxui::text("  " + (this->_matches.empty() ? std::string("no match") : std::to_string(this->_hit + 1) + "/" + std::to_string(this->_matches.size()))
                    + "  Enter / ↑ older · ↓ newer · Esc close") | ftxui::color(this->_theme.muted), ftxui::filler()});
            }
            if (this->_active == TASKS_ID) // the Tasks session: no prompt, what to do instead
                return ftxui::hbox({ftxui::text(" ☰ ") | ftxui::color(this->_theme.accent) | ftxui::bold,
                    ftxui::text("Tasks: no prompt here · a click on a row: its actions · " + this->_manager.config().keys.at("next_session") + " / "
                        + this->_manager.config().keys.at("prev_session") + ", Ctrl+B + an arrow: another session") | ftxui::color(this->_theme.muted), ftxui::filler()});
            const std::string& text = this->_editor.text();
            const std::pair<std::size_t, std::size_t> sel = this->_editor.selection();
            const std::size_t cursor = this->_editor.cursor();
            // its mode: ! a command of the shell (in the folder of the session), : a command of claude-cluster (like vim)
            const char mode = text.starts_with("!") ? '!' : text.starts_with(":") ? ':' : '\0';
            std::string target = this->_active == GLOBAL_ID ? "to the global session " : this->_active == TASKS_ID ? "a new task · +30m <text>: a reminder "
                : "to " + this->_active + " ";
            if (mode == '!') {
                const std::optional<cluster::Snapshot> here = this->_manager.snapshot(this->_active);
                target = "shell in " + (here ? cluster::short_path(here->spec.cwd) : std::string("~")) + " ";
            } else if (mode == ':') {
                target = "command (:help) ";
            }
            // its rows: a line longer than the room wrapped (at a space when one is near, never an atom or a character cut),
            // never scrolled sideways: the text stays where it is while typing
            const std::size_t room = static_cast<std::size_t>(std::max(10, width - 4 - static_cast<int>(target.size()) - (this->_editor.vim() != cluster::VimMode::Off ? 16 : 0)));
            struct Row {
                std::size_t start;
                std::size_t end;
                bool last;          // the last row of its line (the cursor at its end is on it)
            };
            std::vector<Row> lines;
            std::size_t count = 1;  // its lines
            for (std::size_t start = 0;;) {
                const std::size_t nl = text.find('\n', start);
                const std::size_t end = nl == std::string::npos ? text.size() : nl;
                std::size_t from = start;
                do {
                    std::size_t at = from;
                    std::size_t used = 0;
                    std::size_t space = std::string::npos;
                    while (at < end) {
                        const std::optional<std::size_t> atom = this->_editor.atomAt(text, at);
                        std::size_t next = atom ? at + 4 : at + 1;
                        while (next < end && (static_cast<unsigned char>(text[next]) & 0xC0) == 0x80)
                            ++next;
                        const std::size_t cells = atom ? this->_editor.atom(*atom).label.size()
                            : static_cast<std::size_t>(std::max(0, ftxui::string_width(this->_editor.shown(text.substr(at, next - at)))));
                        if (used + cells > room && at > from) break;
                        if (text[at] == ' ') space = next;
                        used += cells;
                        at = next;
                    }
                    if (at < end && space != std::string::npos && space > from + room / 2) at = space; // at the last space, when not too far back
                    lines.push_back({from, at, at >= end});
                    from = at;
                } while (from < end);
                if (nl == std::string::npos) break;
                start = nl + 1;
                ++count;
            }
            std::size_t current = 0;
            for (std::size_t i = 0; i < lines.size(); ++i)
                if (cursor >= lines[i].start && (cursor < lines[i].end || (cursor == lines[i].end && lines[i].last))) current = i;
            const std::size_t first = current >= 7 ? current - 7 : 0;
            ftxui::Elements rows;
            for (std::size_t i = first; i < lines.size() && i < first + 8; ++i) {
                const std::size_t ls = lines[i].start;
                const std::size_t le = lines[i].end;
                ftxui::Elements parts = {ftxui::text(i != 0 ? "   " : mode == '!' ? " $ " : mode == ':' ? " : " : " › ")
                    | ftxui::color(mode == '!' ? this->_theme.warn : this->_theme.accent) | ftxui::bold};
                this->_promptRows.push_back({ls, le, 3});
                const std::function<void(std::size_t, std::size_t, bool)> piece = [&](std::size_t x, std::size_t y, bool selected) {
                    x = std::max(x, ls);
                    y = std::min(y, le);
                    // the atoms ([Image #N], [Pasted text #N]): their label, one block
                    for (std::size_t at = x; x < y; at = x) {
                        while (at < y && !this->_editor.atomAt(text, at))
                            ++at;
                        if (at > x) {
                            ftxui::Element element = ftxui::text(text.substr(x, at - x)) | ftxui::color(this->_theme.text);
                            parts.push_back(selected ? element | ftxui::inverted : element);
                        }
                        if (at >= y) break;
                        ftxui::Element atom = ftxui::text(this->_editor.atom(*this->_editor.atomAt(text, at)).label) | ftxui::color(this->_theme.accent) | ftxui::bold;
                        parts.push_back(selected ? atom | ftxui::inverted : atom | ftxui::bgcolor(this->_theme.surface));
                        x = at + 4;
                    }
                };
                if (sel.first != sel.second) {
                    piece(ls, sel.first, false);
                    piece(sel.first, sel.second, true);
                    piece(sel.second, le, false);
                } else if (i == current) {
                    piece(ls, cursor, false);
                    const cluster::VimMode vim = this->_editor.vim();
                    if (this->_mode == Mode::Normal && vim != cluster::VimMode::Off && vim != cluster::VimMode::Insert) {
                        // vim Normal: block cursor on the character
                        std::size_t next = cursor < le ? cursor + 1 : cursor;
                        while (next < le && (static_cast<unsigned char>(text[next]) & 0xC0) == 0x80) ++next;
                        parts.push_back(ftxui::text(cursor < le ? this->_editor.shown(text.substr(cursor, next - cursor)) : " ") | ftxui::color(this->_theme.accent) | ftxui::inverted);
                        piece(next, le, false);
                    } else if (this->_mode == Mode::Normal) {
                        // the cursor of the terminal (a bar) on the character: no cell added in the text (its end: one cell)
                        std::size_t next = cursor;
                        if (next < le) next = this->_editor.atomAt(text, next) ? next + 4 : next + 1;
                        while (next < le && (static_cast<unsigned char>(text[next]) & 0xC0) == 0x80) ++next;
                        const bool atom = cursor < le && this->_editor.atomAt(text, cursor).has_value();
                        parts.push_back(ftxui::text(cursor < le ? this->_editor.shown(text.substr(cursor, next - cursor)) : " ")
                            | ftxui::color(atom ? this->_theme.accent : this->_theme.text) | ftxui::focusCursorBarBlinking);
                        piece(next, le, false);
                    } else {
                        piece(cursor, le, false);
                    }
                } else {
                    piece(ls, le, false);
                }
                if (text.empty() && this->_mode == Mode::Normal)
                    parts.push_back(ftxui::text(" @ file · / command · ! shell · : command · " + this->_manager.config().keys.at("history") + " history · " + this->_manager.config().keys.at("help") + " keys")
                        | ftxui::color(this->_theme.muted) | ftxui::dim);
                parts.push_back(ftxui::filler());
                if (i == first && this->_editor.vim() != cluster::VimMode::Off) {
                    static const std::map<cluster::VimMode, std::string> names = {{cluster::VimMode::Insert, "INSERT"}, {cluster::VimMode::Normal, "NORMAL"},
                        {cluster::VimMode::Visual, "VISUAL"}, {cluster::VimMode::VisualLine, "V-LINE"}};
                    const bool insert = this->_editor.vim() == cluster::VimMode::Insert;
                    parts.push_back(ftxui::text(" -- " + names.at(this->_editor.vim()) + " -- " + this->_editor.vimPending() + " ")
                        | ftxui::color(insert ? this->_theme.muted : this->_theme.warn) | ftxui::bold);
                }
                if (i == first) parts.push_back(mode != '\0' ? ftxui::text(" " + target) | ftxui::color(mode == '!' ? this->_theme.warn : this->_theme.accent) | ftxui::bold
                    : ftxui::text(target) | ftxui::color(this->_theme.muted) | ftxui::dim);
                rows.push_back(ftxui::hbox(std::move(parts)));
            }
            if (lines.size() > 8)
                rows.push_back(ftxui::text("   " + std::to_string(count) + " lines, " + std::to_string(lines.size()) + " rows (row " + std::to_string(current + 1) + ")")
                    | ftxui::color(this->_theme.muted) | ftxui::dim);
            return ftxui::vbox(std::move(rows)) | ftxui::reflect(this->_promptBox);
        }

        _nodiscard std::optional<std::size_t> promptAt_(const int x, const int y) const
        {
            // a cell of the prompt -> its byte in the text (an atom is one block: its label)
            if (this->_promptRows.empty()) return std::nullopt;
            const PromptRow& row = this->_promptRows[static_cast<std::size_t>(std::clamp(y - this->_promptBox.y_min, 0, static_cast<int>(this->_promptRows.size()) - 1))];
            const std::string& text = this->_editor.text();
            int column = x - this->_promptBox.x_min - row.offset;
            std::size_t at = row.from;
            while (at < row.end && column > 0) {
                const std::optional<std::size_t> atom = this->_editor.atomAt(text, at);
                const int width = atom ? static_cast<int>(this->_editor.atom(*atom).label.size()) : 1;
                if (column < width) {
                    if (column * 2 >= width) at += 4; // the half after: after the block
                    break;
                }
                column -= width;
                at += atom ? 4 : 1;
                while (at < row.end && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) ++at;
            }
            return std::min(at, row.end);
        }

        _nodiscard std::string selectedText_(void) const
        {
            // the cells selected in a transcript, its lines joined back as they were (wrapped or not)
            if (!this->_selection || !this->_views.contains(this->_selection->session)) return "";
            const View& view = this->_views.at(this->_selection->session);
            const Selection& sel = *this->_selection;
            const bool forward = sel.y0 < sel.y1 || (sel.y0 == sel.y1 && sel.x0 <= sel.x1);
            const int ya = forward ? sel.y0 : sel.y1;
            const int xa = forward ? sel.x0 : sel.x1;
            const int yb = forward ? sel.y1 : sel.y0;
            const int xb = forward ? sel.x1 : sel.x0;
            std::string text;
            for (int y = std::max(ya, 0); y <= yb && y < static_cast<int>(view.lines.size()); ++y) {
                const std::string& line = view.lines[static_cast<std::size_t>(y)];
                const std::size_t from = y == ya ? byte_at_(line, xa) : 0;
                const std::size_t to = y == yb ? byte_at_(line, xb + 1) : line.size();
                if (y != ya) text += view.joins[static_cast<std::size_t>(y)];
                if (to > from) text += line.substr(from, to - from);
            }
            return text;
        }

        /* mouse clicks */
        ftxui::Element click_(ftxui::Element element, const int layer, const std::function<bool(void)>& act, const int clip = -1)
        {
            // drawn: its box kept, a click in it runs act (the last drawn first: what is over the rest); under the mouse: its
            // background lit, bold (hover)
            const bool hovered = static_cast<int>(this->_clicks.size()) == this->_hover;
            this->_clicks.push_back({ftxui::Box{-1, -1, -1, -1}, layer, clip, act});
            return (hovered ? element | ftxui::bold | ftxui::bgcolor(this->_theme.border) : element) | ftxui::reflect(this->_clicks.back().box);
        }

        ftxui::Element tap_(ftxui::Element element, const int layer, const std::function<void(void)>& act, const int clip = -1)
        {
            return this->click_(std::move(element), layer, [act]() {
                act();
                return true;
            }, clip);
        }

        _nodiscard int clip_(void)
        {
            // the box of a list that scrolls: its lines clicked inside it only
            this->_clicks.push_back({ftxui::Box{-1, -1, -1, -1}, -1, -1, nullptr});
            return static_cast<int>(this->_clicks.size()) - 1;
        }

        _nodiscard ftxui::Decorator clipBox_(const int clip)
        {
            return ftxui::reflect(this->_clicks[static_cast<std::size_t>(clip)].box);
        }

        _nodiscard int hoverAt_(const ftxui::Mouse& mouse) const
        {
            // the target a click would hit (the layer shown, inside its list), -1: none
            const int layer = this->_mode == Mode::Settings ? 2 : this->_mode == Mode::Normal ? 0 : 1;
            for (std::size_t i = this->_clicks.size(); i-- > 0;) {
                const Click& c = this->_clicks[i];
                if (c.layer != layer || !c.act || !c.box.Contain(mouse.x, mouse.y)) continue;
                if (c.clip >= 0 && !this->_clicks[static_cast<std::size_t>(c.clip)].box.Contain(mouse.x, mouse.y)) continue;
                return static_cast<int>(i);
            }
            return -1;
        }

        bool clickAt_(const ftxui::Mouse& mouse)
        {
            // a left click: the target under it on the layer shown (a dialog: its own targets only, outside it closes the lists)
            if (this->_mode == Mode::Message) {
                this->event_(ftxui::Event::Escape);
                return true;
            }
            const int layer = this->_mode == Mode::Settings ? 2 : this->_mode == Mode::Normal ? 0 : 1;
            for (std::size_t i = this->_clicks.size(); i-- > 0;) {
                const Click& c = this->_clicks[i];
                if (c.layer != layer || !c.act || !c.box.Contain(mouse.x, mouse.y)) continue;
                if (c.clip >= 0 && !this->_clicks[static_cast<std::size_t>(c.clip)].box.Contain(mouse.x, mouse.y)) continue;
                const std::function<bool(void)> act = c.act;
                if (act()) return true;
            }
            if (layer == 0) {
                // a display of a transcript: its line, its cells
                for (const auto &[id, view]: this->_views) {
                    if (!view.box.Contain(mouse.x, mouse.y)) continue;
                    const std::size_t y = static_cast<std::size_t>(mouse.y - view.box.y_min);
                    const int x = mouse.x - view.box.x_min;
                    if (y >= view.hits.size()) return false;
                    for (const auto &[from, to, act]: view.hits[y])
                        if (x >= from && x <= to) {
                            const std::function<void(void)> run = act;
                            run();
                            return true;
                        }
                    return false;
                }
                return false;
            }
            for (const Click& c: this->_clicks)
                if (c.layer == layer && !c.act && c.box.Contain(mouse.x, mouse.y)) return true; // in the dialog: nothing there
            if (this->_mode == Mode::Palette || this->_mode == Mode::History) this->event_(ftxui::Event::Escape); // outside: closed
            return true;
        }

        ftxui::Element block_(ftxui::Element element, const int layer)
        {
            // a dialog: a click in it that hits nothing stops there
            this->_clicks.push_back({ftxui::Box{-1, -1, -1, -1}, layer, -1, nullptr});
            return element | ftxui::reflect(this->_clicks.back().box);
        }

        void mouseSelect_(const ftxui::Mouse& mouse)
        {
            // drag in the prompt: its selection; in a transcript: cells; released: copied to the clipboard (like Claude Code)
            if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Left) {
                this->_drag.clear();
                this->_selection.reset();
                if (this->_promptBox.Contain(mouse.x, mouse.y)) {
                    const std::optional<std::size_t> at = this->promptAt_(mouse.x, mouse.y);
                    if (at) {
                        this->_editor.place(*at, false);
                        this->_drag = "prompt";
                    }
                    return;
                }
                for (const auto &[id, view]: this->_views) {
                    if (!view.box.Contain(mouse.x, mouse.y)) continue;
                    const int x = mouse.x - view.box.x_min;
                    const int y = mouse.y - view.box.y_min;
                    this->_selection = Selection{id, x, y, x, y};
                    this->_drag = id;
                    return;
                }
                return;
            }
            if (this->_drag.empty()) return;
            if (mouse.motion == ftxui::Mouse::Moved) {
                if (this->_drag == "prompt") {
                    const std::optional<std::size_t> at = this->promptAt_(mouse.x, mouse.y);
                    if (at) this->_editor.place(*at, true);
                } else if (this->_selection && this->_views.contains(this->_drag)) {
                    const ftxui::Box& box = this->_views.at(this->_drag).box;
                    this->_selection->x1 = std::clamp(mouse.x, box.x_min, box.x_max) - box.x_min;
                    this->_selection->y1 = std::clamp(mouse.y, box.y_min, box.y_max) - box.y_min;
                }
                return;
            }
            if (mouse.motion == ftxui::Mouse::Released) {
                const std::string text = this->_drag == "prompt" ? this->_editor.expand(this->_editor.selected()) : this->selectedText_();
                const bool click = this->_drag != "prompt" && this->_selection && this->_selection->x0 == this->_selection->x1 && this->_selection->y0 == this->_selection->y1;
                this->_drag.clear();
                if (click) this->_selection.reset();
                if (!text.empty() && !click) {
                    cluster::detach({"wl-copy", "--", text});
                    this->status_("copied (" + std::to_string(text.size()) + " characters)");
                }
            }
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
                const std::string prefix = this->_editor.kind() == cluster::CompletionKind::File ? "@" : this->_editor.kind() == cluster::CompletionKind::Command || this->_editor.kind() == cluster::CompletionKind::Skill ? "/" : "";
                ftxui::Element row = ftxui::hbox({ftxui::text("  " + prefix + c.label + "  "), ftxui::text(cluster::one_line(c.description, 70)) | ftxui::dim, ftxui::filler()});
                rows.push_back(i == selected ? row | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) : row | ftxui::color(this->_theme.text));
            }
            rows.push_back(ftxui::text("  Tab complete · Enter accept · ↑↓ choose · Space / Esc close (\\ + Space: a space in the name)") | ftxui::color(this->_theme.muted) | ftxui::dim);
            return ftxui::vbox(std::move(rows)) | ftxui::bgcolor(this->_theme.surface);
        }

        _nodiscard ftxui::Element images_(void)
        {
            // the atoms of the prompt (images, pasted texts): one chip each, dim when out of the text; mouse over: the
            // image (chafa) or the text, click: back in the prompt, right click: the image in the viewer of the desktop
            this->_chips = this->_editor.promptAtoms();
            this->_chipBoxes.resize(this->_chips.size());
            if (this->_chips.empty()) return ftxui::emptyElement();
            ftxui::Elements chips = {ftxui::text(" ")};
            for (std::size_t i = 0; i < this->_chips.size(); ++i) {
                const cluster::Atom& atom = this->_editor.atom(this->_chips[i]);
                const bool in = this->_editor.contains(this->_chips[i]);
                ftxui::Element chip = ftxui::text(" " + atom.label + (atom.image.empty() ? "" : " " + std::filesystem::path(atom.image).filename().string()) + " ")
                    | ftxui::color(in ? this->_theme.text : this->_theme.muted) | ftxui::bgcolor(static_cast<int>(i) == this->_hoverChip ? this->_theme.accent : this->_theme.surface);
                chips.push_back((in ? chip : chip | ftxui::dim) | ftxui::reflect(this->_chipBoxes[i]));
                chips.push_back(ftxui::text(" "));
            }
            chips.push_back(ftxui::text("(mouse over: preview · click: in the prompt · right click: open)") | ftxui::color(this->_theme.muted) | ftxui::dim);
            ftxui::Element line = ftxui::hbox(std::move(chips));
            if (this->_hoverChip < 0 || this->_hoverChip >= static_cast<int>(this->_chips.size())) return line;
            const cluster::Atom& atom = this->_editor.atom(this->_chips[static_cast<std::size_t>(this->_hoverChip)]);
            const ftxui::Dimensions size = ftxui::Terminal::Size();
            ftxui::Elements rows;
            if (atom.image.empty()) {
                // a pasted text: its first lines
                const std::vector<std::string> lines = wrap_(atom.content, static_cast<std::size_t>(std::max(20, size.dimx - 6)));
                const std::size_t shown = std::min<std::size_t>(lines.size(), static_cast<std::size_t>(std::clamp(size.dimy / 2, 6, 30)));
                for (std::size_t i = 0; i < shown; ++i)
                    rows.push_back(ftxui::text("  " + lines[i]) | ftxui::color(this->_theme.text));
                if (lines.size() > shown)
                    rows.push_back(ftxui::text("  … " + std::to_string(lines.size() - shown) + " more line(s) (" + this->_manager.config().keys.at("edit") + ": the whole text)")
                        | ftxui::color(this->_theme.muted) | ftxui::dim);
                return ftxui::vbox({ftxui::vbox(std::move(rows)) | ftxui::bgcolor(this->_theme.surface), line});
            }
            const bool chafa = this->_manager.config().imagePreview == "chafa";
            const Preview& image = chafa ? preview_(atom.image, std::clamp(size.dimx - 6, 20, 120), std::clamp(size.dimy / 2, 8, 30), true)
                : preview_(atom.image, std::clamp(size.dimx - 6, 20, 96), std::clamp(size.dimy / 2, 8, 24), false);
            if (!image.cells.empty()) {
                for (const std::vector<Cell>& cells: image.cells) {
                    ftxui::Elements parts = {ftxui::text("  ")};
                    for (const Cell& cell: cells) {
                        ftxui::Element part = ftxui::text(cell.glyph);
                        if (cell.fg) part = part | ftxui::color(*cell.fg);
                        if (cell.bg) part = part | ftxui::bgcolor(*cell.bg);
                        parts.push_back(part);
                    }
                    rows.push_back(ftxui::hbox(std::move(parts)));
                }
                rows.push_back(line);
                return ftxui::vbox(std::move(rows));
            }
            if (image.width == 0) return ftxui::vbox({ftxui::text(" (no preview: chafa or magick needed; right click: open it)") | ftxui::dim, line});
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
            this->_actions = cluster::filter_actions(cluster::actions(this->_manager, this->_active), wanted, &this->_manager); // the closest first
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
            } else if (base == "new_project") {
                this->pick_("new:"); // the projects of this computer, its name typed
            } else if (base == "setting") {
                this->openSettingsAt_(arg);
            } else if (base == "settings_search") {
                this->openSettings_();
                this->_finding = true;
                this->_find.clear();
                this->_hits.clear();
            } else if (base == "settings") {
                this->openSettings_();
            } else if (base == "machine_sync" || base == "edit_global_prompt" || base == "edit_sessions_prompt" || base == "edit_vocabulary" || base == "edit_words") {
                this->settingsAction_(base == "edit_global_prompt" ? "edit_prompt" : base);
                if (!this->_note.empty() && this->_mode == Mode::Normal) this->status_(this->_note);
            } else if (base == "restore_menu") {
                this->openRestore_();
            } else if (base == "web_devices") {
                this->_webPicked.clear();
                this->_webCursor = 0;
                this->_webConfirm.clear();
                this->_mode = Mode::WebDevices;
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
                this->terminalModes_();
            } else {
                // run in the worker: the screen goes on (its progress popup when it takes time), its result back here
                const std::string session = this->_active;
                this->status_(cluster::one_line(action.title, 60) + "…");
                this->_worker.push([this, action, input, session]() {
                    const cluster::ActionResult result = cluster::run_action(this->_manager, *this->_app.voice, action.id, session, input);
                    this->_screen.Post([this, result]() {
                        if (!result.focus.empty()) this->_active = result.focus;
                        if (!result.message.empty()) this->status_(result.message, result.error);
                        if (result.ui.starts_with("pick:")) this->pick_(result.ui.substr(5)); // a next step to pick (where it applies)
                    });
                    this->_screen.PostEvent(ftxui::Event::Custom);
                });
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
            if (action == "send_next") { // the next queued prompt now
                this->run_({"send_next", "", "", "", false}, "");
                return true;
            }
            if (action == "resume") { // retry now / continue the session shown
                this->run_({"resume", "", "", "", false}, "");
            } else if (action == "expand") {
                this->_expanded = !this->_expanded;
                this->status_(this->_expanded ? "thinking and tool results in full" : "thinking and tool results folded");
            } else if (action == "undo") {
                this->_editor.undo();
            } else if (action == "redo") {
                this->_editor.redo();
            } else if (action == "select_all") {
                this->_editor.selectAll();
            } else if (action == "stash") {
                if (cluster::trim(this->_editor.text()).empty()) {
                    this->status_("nothing to stash");
                } else {
                    this->_manager.stashPush(this->_editor.draft());
                    this->_editor.clear();
                    this->historyReset_();
                    this->status_("prompt stashed (" + std::to_string(this->_manager.stashSize()) + "), " + this->_manager.config().keys.at("unstash") + " to take it back");
                }
            } else if (action == "unstash") {
                const std::optional<cluster::Draft> text = this->_manager.stashPop();
                if (!text) {
                    this->status_("the stash is empty");
                } else {
                    if (!cluster::trim(this->_editor.text()).empty()) this->_manager.stashPush(this->_editor.draft()); // swapped, not lost
                    this->_editor.load(*text);
                }
            } else if (action == "history") {
                this->_filter.clear();
                this->_selected = 0;
                this->_historyAll = false; // the prompts of this session first (Tab: every session)
                this->historyFilter_();
                this->_mode = Mode::History;
            } else if (action == "paste") {
                this->paste_();
            } else if (action == "cut") {
                const std::string text = this->_editor.expand(this->_editor.cut());
                if (!text.empty()) cluster::detach({"wl-copy", "--", text});
            } else if (action == "interrupt") {
                // voice prompts waiting: dropped; else a turn runs: stop it; else a selection: copy it; else: clear the prompt
                const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(this->_active);
                const bool working = snap && (snap->state == cluster::State::Working || snap->state == cluster::State::Waiting);
                if (!this->_app.voice->pending().empty()) {
                    this->_app.voice->cancel();
                    this->status_("voice prompts dropped");
                } else if (this->_app.voice->speaking() && !working) {
                    this->_app.voice->mute(); // an answer read after its turn: stopped like a running one
                    this->status_("reading stopped");
                } else if (working) {
                    this->run_({"interrupt", "", "", "", false}, "");
                } else if (!this->_editor.selected().empty()) {
                    cluster::detach({"wl-copy", "--", this->_editor.expand(this->_editor.selected())});
                    this->status_("copied");
                } else if (!this->_editor.text().empty()) {
                    this->_editor.clear();
                    this->historyReset_();
                } else {
                    // nothing to stop or clear: twice within 2 s quits (like Claude Code)
                    const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    if (now - this->_lastInterrupt < 2000) this->_screen.Exit();
                    else this->status_(this->_manager.config().keys.at("interrupt") + " again to quit");
                    this->_lastInterrupt = now;
                }
            } else if (action == "mode_cycle") {
                const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(this->_active);
                if (snap) this->run_({"mode:" + cluster::next_mode(snap->spec.mode), "", "", "", false}, "");
            } else if (action == "help") {
                this->status_(cluster::help_text(this->_manager.config()));
            } else if (action == "search") {
                this->_search.clear();
                this->_matches.clear();
                this->_hit = 0;
                this->_mode = Mode::Search;
            } else if (action == "edit") {
                this->externalEdit_();
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
        void openSettingsAt_(const std::string& where)
        {
            // a setting found (the palette, a search): its page, on it
            this->openSettings_();
            for (std::size_t p = 0; p < this->_pages.size(); ++p)
                for (std::size_t i = 0; i < this->_pages[p].settings.size(); ++i)
                    if (this->_pages[p].settings[i].section + "." + this->_pages[p].settings[i].key == where) {
                        this->_page = static_cast<int>(p);
                        this->_item = static_cast<int>(i);
                    }
        }

        void openSettings_(void)
        {
            this->_pages = cluster::settings_pages(this->_manager.config());
            this->_page = 0;
            this->_item = 0;
            this->_finding = false;
            this->_editing = false;
            this->_note.clear();
            this->_changes.clear();
            this->_mode = Mode::Settings;
        }

        _nodiscard std::string servicesSetting_(const std::string& key) const
        {
            // [services] refresh / delay, their change not saved yet taken
            for (const cluster::SettingsPage& page: this->_pages)
                for (const cluster::Setting& setting: page.settings)
                    if (setting.section == "services" && setting.key == key) return this->settingValue_(setting);
            return "";
        }

        void servicesRead_(const bool force = false)
        {
            // live: when the core changes (0.3 s at most), else every 0.5 s (its slow parts kept Delay s by the core); interval:
            // every Delay s; manual: R and after each action; read by the worker (the screen never waits)
            if (this->_servicesReading) return;
            const std::string mode = this->servicesSetting_("refresh");
            const std::int64_t delay = static_cast<std::int64_t>(std::max(1, std::atoi(this->servicesSetting_("delay").c_str()))) * 1000;
            const std::int64_t now = cluster::now_ms();
            const std::uint64_t version = this->_manager.version();
            const bool due = force || this->_services.is_null() || this->_servicesAt == 0
                || (mode == "live" && ((version != this->_servicesSeen && now - this->_servicesAt >= 300) || now - this->_servicesAt >= 500))
                || (mode == "interval" && now - this->_servicesAt >= delay);
            if (!due) return;
            this->_servicesReading = true;
            this->_servicesSeen = version;
            this->_worker.push([this]() {
                cluster::Json got;
                try {
                    got = cluster::services(this->_manager);
                } catch (const utils::exception::IException& e) {
                    got = {{"error", e.info()}};
                }
                this->_screen.Post([this, got]() {
                    this->_services = got;
                    this->_servicesAt = cluster::now_ms();
                    this->_servicesReading = false;
                });
                this->_screen.PostEvent(ftxui::Event::Custom);
            });
        }

        _nodiscard std::vector<std::pair<std::string, std::string>> svcButtons_(const std::string& name) const
        {
            return cluster::service_buttons(name, this->_services); // its buttons: <label, action>
        }

        void svcRun_(const std::string& name, const std::string& action, const std::string& label)
        {
            // the core, a removal: y / n first; run by the worker (its popup), its result in the note, read again after
            if (cluster::service_risky(name, action) && this->_svcConfirm != name + "|" + action) {
                this->_svcConfirm = name + "|" + action + "|" + label;
                this->_note = "svc_confirm";
                return;
            }
            this->_svcConfirm.clear();
            this->_note = label + "…";
            this->_worker.push([this, name, action]() {
                std::string message;
                bool failed = false;
                try {
                    message = cluster::service_act(this->_manager, name, action);
                } catch (const utils::exception::IException& e) {
                    message = std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info());
                    failed = true;
                }
                this->_screen.Post([this, message, failed]() {
                    this->_note = (failed ? "✗ " : "") + message;
                    this->_servicesAt = 0; // read again
                });
                this->_screen.PostEvent(ftxui::Event::Custom);
            });
        }

        _nodiscard ftxui::Elements svcCard_(const std::string& name, const bool selected, const int index, const int clip)
        {
            // its details, its journal greyed, what the voice hears now; its buttons (the one chosen lit when it is chosen)
            const Theme& t = this->_theme;
            ftxui::Elements out;
            for (const cluster::ServiceLine& l: cluster::service_lines(name, this->_services)) {
                const ftxui::Color color = l.kind == "error" ? t.error : l.kind == "ok" ? t.ok : l.kind == "warn" ? t.warn : l.kind == "heard" ? t.accent : l.kind == "text" ? t.text
                    : t.muted;
                out.push_back(ftxui::hbox({ftxui::text("      "), l.kind == "log" ? ftxui::text(cluster::one_line(l.text, 110)) | ftxui::color(color) | ftxui::dim
                    : ftxui::paragraph(l.text) | ftxui::color(color) | ftxui::flex}));
            }
            ftxui::Elements buttons = {ftxui::text("      ")};
            const std::vector<std::pair<std::string, std::string>> list = this->svcButtons_(name);
            for (std::size_t b = 0; b < list.size(); ++b) {
                const bool lit = selected && static_cast<int>(b) == this->_svcButton;
                const std::string label = list[b].first;
                const std::string action = list[b].second;
                const int button = static_cast<int>(b);
                buttons.push_back(this->tap_(ftxui::text(" " + label + " ") | (lit ? ftxui::color(t.bg) | ftxui::bgcolor(t.accent) | ftxui::bold : ftxui::color(t.accent)), 2,
                    [this, name, action, label, index, button]() {
                        this->_item = index;
                        this->_svcButton = button;
                        this->svcRun_(name, action, label);
                    }, clip));
                buttons.push_back(ftxui::text(" "));
            }
            out.push_back(ftxui::hbox(std::move(buttons)));
            out.push_back(ftxui::text(""));
            return out;
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
            else this->_changes[{setting.section, setting.key}] = {setting, value}; // a dangerous one: asked at the save
            this->_note.clear();
        }

        _nodiscard std::vector<std::pair<std::pair<cluster::Setting, std::string>, cluster::Requirement>> missing_(void)
        {
            // the changes whose value needs something missing: listed in the popup of the save
            std::vector<std::pair<std::pair<cluster::Setting, std::string>, cluster::Requirement>> out;
            for (const auto &[where, change]: this->_changes)
                if (const cluster::Requirement need = cluster::setting_requirement(this->_manager, change.first, change.second); !need.problem.empty())
                    out.push_back({change, need});
            return out;
        }

        void install_(const std::string& package)
        {
            // a desktop: the password window and its popup here (the interface stays); else in the terminal (sudo asks its
            // password there), the interface back after
            if ((std::getenv("WAYLAND_DISPLAY") != nullptr || std::getenv("DISPLAY") != nullptr) && cluster::has_command("pkexec")) {
                this->_worker.push([this, package]() {
                    const std::string error = cluster::install_package(package, &this->_manager);
                    this->_screen.Post([this, package, error]() {this->_note = error.empty() ? package + " installed" : error;});
                    this->_screen.PostEvent(ftxui::Event::Custom);
                });
                this->_note = "installing " + package + "…";
                return;
            }
            const std::vector<std::string> command = cluster::install_command(package);
            if (command.empty()) {
                this->_note = "no package manager known here: install " + package + " yourself";
                return;
            }
            std::string line = "sudo";
            for (const std::string& part: command)
                line += " " + part;
            int code = 0;
            this->_screen.WithRestoredIO([&]() {
                std::cout << "\n  claude-cluster: installing " << package << " (" << line << ")\n\n" << std::flush;
                code = std::system(line.c_str());
                std::cout << "\n  " << (code == 0 ? "installed" : "not installed") << ": back in claude-cluster…" << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
            })();
            this->_note = code == 0 ? package + " installed" : package + " not installed (exit " + std::to_string(code) + ")";
        }

        _nodiscard std::vector<std::pair<cluster::Setting, std::string>> dangerous_(void) const
        {
            // the changes to a dangerous value: listed in the popup of the save
            std::vector<std::pair<cluster::Setting, std::string>> out;
            for (const auto &[where, change]: this->_changes)
                if (!cluster::setting_danger(change.first, change.second).empty()) out.push_back(change);
            return out;
        }

        void saveSettings_(const bool exit = true)
        {
            if (!this->_dangerOk && (!this->dangerous_().empty() || !this->missing_().empty())) { // asked first: all of them in one popup
                this->_saveExit = exit;
                this->_note = "danger_save";
                return;
            }
            if (!this->_changes.empty()) {
                try {
                    cluster::settings_write(this->_manager.config(), this->_changes);
                } catch (const utils::exception::IException& e) {
                    this->_note = std::string("not saved: ") + e.info();
                    (void)this->_manager.fail("Save the settings", this->_manager.config().path().string(), e.info());
                    return;
                }
                // the live choices of layout / style follow the saved ones
                for (const char* key: {"layout", "style", "vim"})
                    if (this->_changes.contains({"ui", key})) this->_manager.setUi(key, this->_changes.at({"ui", key}).second);
                bool restart = false;
                for (const auto &[where, change]: this->_changes)
                    restart |= change.first.restart;
                (void)this->_manager.config().reload();
                this->theme_();
                this->status_(std::to_string(this->_changes.size()) + " setting(s) saved" + (restart ? " (some at the next start)" : ""));
                this->_note = std::to_string(this->_changes.size()) + " setting(s) saved" + (restart ? " (some at the next start)" : "");
            } else {
                this->_note = "nothing to save";
            }
            this->_changes.clear();
            if (exit) this->_mode = Mode::Normal;
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

            // the search: / (or Ctrl+F) opens it, every page searched; Enter goes to the one chosen, Esc closes it
            if (this->_finding) {
                if (event == ftxui::Event::Escape) {
                    this->_finding = false;
                } else if (event == ftxui::Event::Return) {
                    if (!this->_hits.empty()) {
                        const cluster::SettingHit& hit = this->_hits[static_cast<std::size_t>(std::clamp(this->_findHit, 0, static_cast<int>(this->_hits.size()) - 1))];
                        this->_page = static_cast<int>(hit.page);
                        this->_item = static_cast<int>(hit.item);
                    }
                    this->_finding = false;
                } else if (event == ftxui::Event::ArrowDown) {
                    this->_findHit = std::min(this->_findHit + 1, std::max(0, static_cast<int>(this->_hits.size()) - 1));
                } else if (event == ftxui::Event::ArrowUp) {
                    this->_findHit = std::max(this->_findHit - 1, 0);
                } else if (event == ftxui::Event::Backspace) {
                    while (!this->_find.empty() && (static_cast<unsigned char>(this->_find.back()) & 0xC0) == 0x80) this->_find.pop_back();
                    if (!this->_find.empty()) this->_find.pop_back();
                    this->_hits = cluster::settings_search(this->_pages, this->_find);
                    this->_findHit = 0;
                } else if (event.is_character()) {
                    this->_find += event.character();
                    this->_hits = cluster::settings_search(this->_pages, this->_find);
                    this->_findHit = 0;
                }
                return true;
            }
            if (!this->_editing && this->_note.empty() && (event == ftxui::Event::Character('i') || event == ftxui::Event::Character('I')) && !this->_need.package.empty()) {
                this->install_(this->_need.package); // what the setting chosen needs
                return true;
            }
            if (!this->_editing && this->_note != "confirm" && (event == ftxui::Event::Character('/') || event.input() == std::string(1, '\x06'))) {
                this->_finding = true;
                this->_find.clear(); // a new search each time
                this->_hits.clear();
                this->_findHit = 0;
                return true;
            }
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
            // the save with dangerous values: y saves them all, n / Esc back to the page (nothing saved, the changes kept)
            if (this->_note == "danger_save") {
                this->_note.clear();
                if (event == ftxui::Event::Character('i') || event == ftxui::Event::Character('I')) { // what is missing installed, then saved
                    for (const auto &[change, need]: this->missing_())
                        if (!need.package.empty()) this->install_(need.package);
                    const std::string installed = this->_note;
                    this->_dangerOk = true;
                    this->saveSettings_(this->_saveExit);
                    this->_dangerOk = false;
                    if (!installed.empty() && this->_mode == Mode::Settings) this->_note = installed + " · " + this->_note;
                    else if (!installed.empty()) this->status_(installed);
                    return true;
                }
                if (event == ftxui::Event::Character('y') || event == ftxui::Event::Character('Y')) {
                    this->_dangerOk = true;
                    this->saveSettings_(this->_saveExit);
                    this->_dangerOk = false;
                } else {
                    this->_note = "not saved: the changes are kept (Ctrl+S to save them)";
                }
                return true;
            }
            // a service action to confirm (the core, a removal): y runs it, any other key leaves it
            if (this->_note == "svc_confirm") {
                const std::vector<std::string> parts = cluster::split(this->_svcConfirm, '|');
                this->_note.clear();
                if ((event == ftxui::Event::Character('y') || event == ftxui::Event::Character('Y')) && parts.size() == 3) {
                    this->_svcConfirm = parts[0] + "|" + parts[1];
                    this->svcRun_(parts[0], parts[1], parts[2]);
                } else {
                    this->_svcConfirm.clear();
                    this->_note = "nothing done";
                }
                return true;
            }
            if (page.section == "services" && (event == ftxui::Event::Character('R') || event == ftxui::Event::Character('r'))) {
                this->servicesRead_(true); // read again now
                return true;
            }
            if (count > 0 && page.section == "services" && page.settings[static_cast<std::size_t>(this->_item)].key.starts_with("svc_")
                && (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight || event == ftxui::Event::Return || event == ftxui::Event::Character(' '))) {
                // a service: ←→ its buttons, Enter / Space the one chosen
                const std::string name = page.settings[static_cast<std::size_t>(this->_item)].key.substr(4);
                const std::vector<std::pair<std::string, std::string>> list = this->svcButtons_(name);
                if (list.empty()) return true;
                if (event == ftxui::Event::ArrowLeft) this->_svcButton = std::max(0, this->_svcButton - 1);
                else if (event == ftxui::Event::ArrowRight) this->_svcButton = std::min(static_cast<int>(list.size()) - 1, this->_svcButton + 1);
                else {
                    const std::pair<std::string, std::string>& chosen = list[static_cast<std::size_t>(std::clamp(this->_svcButton, 0, static_cast<int>(list.size()) - 1))];
                    this->svcRun_(name, chosen.second, chosen.first);
                }
                return true;
            }
            // "Save the changes? (y / n)": y saves, any other key drops them
            if (this->_note == "confirm") {
                if (event == ftxui::Event::Character('y') || event == ftxui::Event::Character('Y')) {
                    this->saveSettings_();
                } else {
                    this->_changes.clear();
                    this->_note.clear();
                    this->_mode = Mode::Normal;
                }
                return true;
            }
            if (event.input() == std::string(1, '\x13')) { // Ctrl+S: save, the page stays
                this->saveSettings_(false);
            } else if (event == ftxui::Event::F10) {         // F10: save and exit
                this->saveSettings_();
            } else if (event == ftxui::Event::Escape) {
                if (!this->_changes.empty()) {
                    this->_note = "confirm";
                } else {
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
                this->_svcButton = 0;
            } else if (event == ftxui::Event::ArrowUp) {
                this->_item = std::max(this->_item - 1, 0);
                this->_svcButton = 0;
            } else if (count > 0 && (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight || event == ftxui::Event::Return
                || event == ftxui::Event::Character(' '))) {
                const cluster::Setting& setting = page.settings[static_cast<std::size_t>(this->_item)];
                if (setting.kind == cluster::SettingKind::Action) {
                    if (event == ftxui::Event::Return || event == ftxui::Event::Character(' ')) this->settingsAction_(setting.key);
                    return true;
                }
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
            const std::function<ftxui::Element(const std::string&, const ftxui::Event&)> keyed = [this](const std::string& label, const ftxui::Event& key) {
                return this->tap_(ftxui::text(label), 2, [this, key]() {(void)this->event_(key);});
            };
            ftxui::Elements tabs;
            for (std::size_t i = 0; i < this->_pages.size(); ++i) {
                const bool active = static_cast<int>(i) == this->_page;
                ftxui::Element tab = ftxui::text("  " + this->_pages[i].title + "  ");
                const int index = static_cast<int>(i);
                tabs.push_back(this->tap_(active ? tab | ftxui::bold | ftxui::color(t.bg) | ftxui::bgcolor(t.accent) : tab | ftxui::color(t.text), 2, [this, index]() {
                    if (this->_editing) (void)this->event_(ftxui::Event::Return);
                    this->_page = index;
                    this->_item = 0;
                }));
            }
            const cluster::SettingsPage& page = this->_pages[static_cast<std::size_t>(this->_page)];
            if (page.section == "services") { // its state read (live / every Delay / by hand)
                this->servicesRead_();
                if (this->servicesSetting_("refresh") != "manual") this->_animating = true;
            }
            ftxui::Elements rows;
            std::string help;
            std::string choiceHelp; // a choice of engine: what it handles, its limits, its link
            bool restart = false;
            const int clip = this->clip_();
            for (std::size_t i = 0; i < page.settings.size(); ++i) {
                const cluster::Setting& setting = page.settings[i];
                const bool selected = static_cast<int>(i) == this->_item;
                const bool changed = this->_changes.contains({setting.section, setting.key});
                std::string value = this->settingValue_(setting);
                if (selected && this->_editing) value = this->_edit + "▏";
                else if (setting.kind == cluster::SettingKind::Bool) value = value == "true" ? "Enabled" : "Disabled";
                else if (setting.kind == cluster::SettingKind::Choice) value = "< " + (value.empty() ? std::string("(agent default)") : value) + " >";
                else if (setting.kind == cluster::SettingKind::Action && page.section == "services") value = cluster::service_summary(value, this->_services); // a sub service: its state
                else if (setting.kind == cluster::SettingKind::Action) value = "▶ " + value;
                else if (value.empty()) value = "(empty)";
                const bool risky = static_cast<bool>(setting.danger);
                const bool dangerNow = risky && !cluster::setting_danger(setting, this->settingValue_(setting)).empty();
                const std::string label = setting.label + (risky ? " ⚠ " : " ");
                const std::size_t width = setting.label.size() + (risky ? 3 : 1);
                const std::size_t dots = width < 34 ? 34 - width : 1;
                const int index = static_cast<int>(i);
                // clicked: selected; its value clicked: changed (a switch, a choice), edited (a text), run (an action)
                const std::function<void(bool)> pick = [this, index](const bool change) {
                    if (this->_editing && this->_item != index) (void)this->event_(ftxui::Event::Return);
                    this->_item = index;
                    if (change && !this->_editing) (void)this->event_(ftxui::Event::Return);
                };
                // what its value needs and is missing: in red on its right
                const cluster::Requirement need = cluster::setting_requirement(this->_manager, setting, this->settingValue_(setting));
                ftxui::Element line = ftxui::hbox({
                    this->tap_(ftxui::text(changed ? " * " : "   ") | ftxui::color(t.warn) | ftxui::bold, 2, [pick]() {pick(false);}, clip),
                    this->tap_(ftxui::hbox({ftxui::text(setting.label) | (dangerNow ? ftxui::color(t.error) | ftxui::bold : ftxui::color(t.muted)),
                        risky ? ftxui::text(" ⚠") | ftxui::color(t.warn) | ftxui::bold : ftxui::text(""),
                        ftxui::text(" " + std::string(dots, '.') + " ") | ftxui::color(t.muted)}), 2, [pick]() {pick(false);}, clip),
                    this->tap_(ftxui::text("[" + cluster::one_line(value, 60) + "]") | (selected ? ftxui::color(t.bg) | ftxui::bgcolor(t.accent) : ftxui::color(t.text)) | ftxui::bold,
                        2, [pick]() {pick(true);}, clip),
                    need.problem.empty() ? ftxui::text("") : ftxui::text("  ✗ " + cluster::one_line(need.problem.substr(0, need.problem.find(':')), 34)) | ftxui::color(t.error) | ftxui::bold,
                    ftxui::filler(),
                });
                rows.push_back(selected ? line | ftxui::focus : line);
                if (page.section == "services" && setting.key.starts_with("svc_"))
                    for (ftxui::Element& e: this->svcCard_(setting.key.substr(4), selected, index, clip))
                        rows.push_back(std::move(e));
                if (selected) {
                    this->_need = need;
                    help = setting.description;
                    if (setting.choiceHelp) choiceHelp = setting.choiceHelp(this->settingValue_(setting));
                    restart = setting.restart;
                }
            }
            if (rows.empty()) rows.push_back(ftxui::text("   nothing here") | ftxui::color(t.muted));
            // its help: a paragraph per line, the steps / points indented, a title line (no • nor digit) in bold
            const std::function<ftxui::Elements(const std::string&)> helpLines = [&t](const std::string& text) {
                ftxui::Elements out;
                const std::vector<std::string> lines = cluster::split(text, '\n');
                for (std::size_t i = 0; i < lines.size(); ++i) {
                    const std::string line = lines[i];
                    const std::string bare = cluster::trim(line);
                    const bool point = bare.starts_with("•") || (!bare.empty() && std::isdigit(static_cast<unsigned char>(bare[0])) && bare.find(". ") < 3);
                    const bool title = i > 0 && !point && !line.starts_with(" ") && lines.size() > 1 && i + 1 < lines.size() && cluster::trim(lines[i + 1]).starts_with("1.");
                    out.push_back(ftxui::paragraph(point ? "  " + bare : line) | (title ? ftxui::bold | ftxui::color(t.accent) : ftxui::color(t.text)));
                }
                return out;
            };
            if (this->_finding) {
                // the search: every page, the closest first, where it matched (its name / its description)
                rows.clear();
                rows.push_back(ftxui::hbox({ftxui::text(" / ") | ftxui::bold | ftxui::color(t.accent), ftxui::text(this->_find + "▏") | ftxui::bold,
                    ftxui::filler(), ftxui::text(this->_find.empty() ? "type a name or words of its description " : std::to_string(this->_hits.size()) + " found ")
                    | ftxui::color(t.muted)}));
                rows.push_back(ftxui::separatorLight() | ftxui::color(t.border));
                help.clear();
                choiceHelp.clear();
                for (std::size_t h = 0; h < this->_hits.size(); ++h) {
                    const cluster::SettingHit& hit = this->_hits[h];
                    const cluster::Setting& s = this->_pages[hit.page].settings[hit.item];
                    const bool selected = static_cast<int>(h) == this->_findHit;
                    const int index = static_cast<int>(h);
                    // its name (accent when chosen), its value now, its page; where it matched: a small mark on the right
                    std::string now = this->settingValue_(s);
                    if (s.kind == cluster::SettingKind::Bool) now = now == "true" ? "on" : "off";
                    else if (s.kind == cluster::SettingKind::Action) now = "▶";
                    else if (now.empty()) now = "(empty)";
                    const bool risky = !cluster::setting_danger(s, this->settingValue_(s)).empty();
                    ftxui::Element line = ftxui::hbox({
                        ftxui::text(selected ? " ▸ " : "   ") | ftxui::color(t.accent) | ftxui::bold,
                        ftxui::text(s.label) | (selected ? ftxui::bold | ftxui::color(t.accent) : ftxui::color(t.text)),
                        s.danger ? ftxui::text(" ⚠") | ftxui::color(t.warn) : ftxui::text(""),
                        ftxui::text("  " + cluster::one_line(now, 28)) | ftxui::color(risky ? t.error : t.muted),
                        ftxui::filler(),
                        ftxui::text(this->_pages[hit.page].title) | ftxui::color(t.muted) | ftxui::dim | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 16),
                        (hit.name ? ftxui::text("● name") | ftxui::color(t.accent) : ftxui::text("○ description") | ftxui::color(t.muted)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 14),
                    });
                    rows.push_back(this->tap_(selected ? line | ftxui::bgcolor(t.surface) | ftxui::focus : line, 2, [this, index]() {
                        this->_findHit = index;
                        (void)this->event_(ftxui::Event::Return);
                    }));
                    if (selected && !s.description.empty())
                        rows.push_back(ftxui::text("     " + cluster::one_line(s.description, 110)) | ftxui::color(t.muted) | ftxui::dim | ftxui::bgcolor(t.surface));
                    if (selected) help = s.description;
                }
                if (!this->_find.empty() && this->_hits.empty()) rows.push_back(ftxui::text("   nothing found") | ftxui::color(t.muted));
            }
            std::string note = this->_note == "confirm" ? "Save the " + std::to_string(this->_changes.size()) + " change(s)? (y / n)"
                : this->_note == "danger_save" ? std::string() : this->_note == "svc_confirm"
                ? cluster::split(this->_svcConfirm, '|').back() + " (" + cluster::split(this->_svcConfirm, '|')[0] + ")? [y] yes   [n] / Esc no" : this->_note;
            if (page.section == "services" && this->_note.empty()) note = "←→ a button · Enter run it · R read again (" + this->servicesSetting_("refresh") + ")";
            ftxui::Elements choice;
            for (const std::string& line: cluster::split(choiceHelp, '\n'))
                choice.push_back(ftxui::paragraph(line) | (choice.empty() ? ftxui::bold | ftxui::color(t.accent) : ftxui::color(t.text)));
            if (!choice.empty()) choice.insert(choice.begin(), ftxui::separatorLight() | ftxui::color(t.border));
            ftxui::Element helpBox = ftxui::vbox({
                ftxui::text(" Item Specific Help") | ftxui::bold | ftxui::color(t.accent),
                ftxui::separatorLight() | ftxui::color(t.border),
                this->_need.problem.empty() || this->_finding ? ftxui::emptyElement() : ftxui::vbox({ftxui::text(""),
                    ftxui::paragraph("✗ " + this->_need.problem) | ftxui::color(t.error) | ftxui::bold,
                    this->_need.package.empty() ? ftxui::emptyElement() : keyed(" [i] install " + this->_need.package + " ", ftxui::Event::Character('i')) | ftxui::color(t.accent),
                    ftxui::text("")}),
                ftxui::vbox(helpLines(help)),
                ftxui::vbox(std::move(choice)),
                ftxui::text(""),
                restart ? ftxui::paragraph("Applied at the next start of claude-cluster") | ftxui::color(t.warn) : ftxui::emptyElement(),
                ftxui::filler(),
                ftxui::text(std::to_string(this->_changes.size()) + " change(s) not saved") | ftxui::color(this->_changes.empty() ? t.muted : t.warn),
                ftxui::paragraph(cluster::short_path(this->_manager.config().path().string())) | ftxui::color(t.muted) | ftxui::dim,
            }) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, choiceHelp.empty() ? 38 : 64);
            // the save with dangerous values: one popup, each one with its warning
            ftxui::Element popup = ftxui::emptyElement();
            if (this->_note == "danger_save") {
                ftxui::Elements lines;
                const std::vector<std::pair<std::pair<cluster::Setting, std::string>, cluster::Requirement>> missing = this->missing_();
                bool installable = false;
                if (!missing.empty()) {
                    lines.push_back(ftxui::text(" ✗ Missing for these settings ") | ftxui::bold | ftxui::color(t.error));
                    lines.push_back(ftxui::text(""));
                    for (const auto &[change, need]: missing) {
                        installable |= !need.package.empty();
                        lines.push_back(ftxui::hbox({ftxui::text(" ✗ ") | ftxui::color(t.error) | ftxui::bold, ftxui::text(change.first.label) | ftxui::bold,
                            ftxui::text(" = " + cluster::one_line(change.second, 40) + (need.package.empty() ? "" : "  (install: " + need.package + ")")) | ftxui::color(t.text)}));
                        lines.push_back(ftxui::paragraph("   " + need.problem) | ftxui::color(t.muted));
                        lines.push_back(ftxui::text(""));
                    }
                }
                if (!this->dangerous_().empty()) {
                    lines.push_back(ftxui::text(" ⚠ Dangerous settings ") | ftxui::bold | ftxui::color(t.warn));
                    lines.push_back(ftxui::text(""));
                }
                for (const auto &[setting, value]: this->dangerous_()) {
                    lines.push_back(ftxui::hbox({ftxui::text(" ⚠ ") | ftxui::color(t.warn) | ftxui::bold, ftxui::text(setting.label) | ftxui::bold | ftxui::color(t.error),
                        ftxui::text(" = " + (value.empty() ? std::string("(empty)") : cluster::one_line(value, 50))) | ftxui::color(t.text)}));
                    lines.push_back(ftxui::paragraph("   " + cluster::setting_danger(setting, value)) | ftxui::color(t.muted));
                    lines.push_back(ftxui::text(""));
                }
                lines.push_back(ftxui::hbox({installable ? keyed(" [i] install, then save ", ftxui::Event::Character('i')) | ftxui::color(t.ok) | ftxui::bold : ftxui::text(""),
                    installable ? ftxui::text("   ") : ftxui::text(""),
                    keyed(missing.empty() ? " [y] save them " : " [y] save anyway ", ftxui::Event::Character('y')) | ftxui::color(t.error) | ftxui::bold, ftxui::text("   "),
                    keyed(" [n] / Esc back (nothing saved) ", ftxui::Event::Character('n')) | ftxui::color(t.accent)}));
                popup = ftxui::vbox(std::move(lines)) | ftxui::borderRounded | ftxui::color(t.warn) | ftxui::bgcolor(t.bg) | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 90)
                    | ftxui::center;
            }
            return ftxui::dbox({ftxui::vbox({
                ftxui::hbox({ftxui::text(" claude-cluster setup ") | ftxui::bold | ftxui::color(t.bg) | ftxui::bgcolor(t.accent), ftxui::filler(),
                    ftxui::text(" config.toml ") | ftxui::color(t.muted)}),
                ftxui::hbox(std::move(tabs)) | ftxui::bgcolor(t.surface),
                ftxui::separatorHeavy() | ftxui::color(t.accent),
                ftxui::hbox({ftxui::vbox(std::move(rows)) | ftxui::yframe | this->clipBox_(clip) | ftxui::flex, ftxui::separatorLight() | ftxui::color(t.border), helpBox}) | ftxui::flex,
                ftxui::separatorHeavy() | ftxui::color(t.accent),
                this->_note == "confirm" ? ftxui::hbox({ftxui::text(" " + note + "  ") | ftxui::color(t.warn), keyed(" [y] save ", ftxui::Event::Character('y')) | ftxui::color(t.accent),
                    keyed(" [n] drop ", ftxui::Event::Character('n')) | ftxui::color(t.accent)})
                : ftxui::text(" " + note) | ftxui::color(t.warn),
                ftxui::hbox({ftxui::hbox({ftxui::text(" ↑↓ select   ←→ / Enter change   Enter edit text   Tab / Shift+Tab section   "), keyed("/ search", ftxui::Event::Character('/')),
                    ftxui::text("   "), keyed("Ctrl+S save", ftxui::Event::Special(std::string(1, '\x13'))),
                    ftxui::text("   "), keyed("F10 save & exit", ftxui::Event::F10), ftxui::text("   "), keyed("Esc exit (asks to save) ", ftxui::Event::Escape)})
                    | ftxui::color(t.bg) | ftxui::bgcolor(t.muted), ftxui::filler()}) | ftxui::bgcolor(t.muted),
            }), popup}) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, size.dimy) | ftxui::color(t.text) | ftxui::bgcolor(t.bg);
        }

        /* rendering */
        _nodiscard ftxui::Element tasksRow_(const bool active, const int clip)
        {
            // the Tasks session in the list: what runs, waits, is scheduled
            const std::vector<cluster::Task> tasks = this->_manager.tasks();
            const std::vector<cluster::Schedule> schedules = this->_manager.schedules();
            const auto running = std::count_if(tasks.begin(), tasks.end(), [](const cluster::Task& t) {return t.status == "running";});
            const auto queued = std::count_if(tasks.begin(), tasks.end(), [](const cluster::Task& t) {return t.status == "queued";});
            const auto waiting = std::count_if(schedules.begin(), schedules.end(), [](const cluster::Schedule& w) {return w.status == "waiting";});
            ftxui::Element line = ftxui::hbox({ftxui::text("☰ ") | ftxui::color(this->_theme.accent), ftxui::text("Tasks") | (active ? ftxui::bold : ftxui::nothing) | ftxui::color(this->_theme.text),
                ftxui::filler(), ftxui::text(running > 0 ? std::to_string(running) + "▶ " : "") | ftxui::color(this->_theme.ok)});
            ftxui::Element sub = ftxui::text("  " + std::to_string(queued) + " waiting · " + std::to_string(waiting) + " scheduled") | ftxui::color(this->_theme.muted) | ftxui::dim;
            ftxui::Element block = this->tap_(ftxui::vbox({line, sub}), 0, [this]() {this->_active = TASKS_ID;}, clip);
            return active ? block | ftxui::bgcolor(this->_theme.surface) : block;
        }

        bool tasksKey_(const ftxui::Event& event)
        {
            // the Tasks session without the mouse: an item chosen, its part jumped to, its actions opened
            const int count = static_cast<int>(this->_taskItems.size());
            if (count == 0) return false;
            int& sel = this->_taskSel;
            sel = std::clamp(sel, 0, count - 1);
            const std::function<int(int)> part = [this](const int index) {return this->_taskItems[static_cast<std::size_t>(index)].first;};
            if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
                sel = std::min(count - 1, sel + 1);
            } else if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
                sel = std::max(0, sel - 1);
            } else if (event == ftxui::Event::PageDown) {
                sel = std::min(count - 1, sel + 8);
            } else if (event == ftxui::Event::PageUp) {
                sel = std::max(0, sel - 8);
            } else if (event == ftxui::Event::Home || event == ftxui::Event::Character('g')) {
                sel = 0;
            } else if (event == ftxui::Event::End || event == ftxui::Event::Character('G')) {
                sel = count - 1;
            } else if (event == ftxui::Event::Tab) { // the first item of the next part (round)
                int next = sel;
                while (next < count && part(next) == part(sel)) ++next;
                sel = next < count ? next : 0;
            } else if (event == ftxui::Event::TabReverse) { // the first item of this part, else of the one before
                int first = sel;
                while (first > 0 && part(first - 1) == part(sel)) --first;
                if (first == sel && sel > 0) {
                    first = sel - 1;
                    while (first > 0 && part(first - 1) == part(sel - 1)) --first;
                }
                sel = first;
            } else if (event == ftxui::Event::Return || event == ftxui::Event::Character(' ') || event == ftxui::Event::ArrowRight || event == ftxui::Event::Character('l')) {
                const std::function<void(void)> act = this->_taskItems[static_cast<std::size_t>(sel)].second;
                if (act) act();
            } else {
                return false;
            }
            return true;
        }

        _nodiscard ftxui::Element tasksView_(const int width, const int height)
        {
            // the todo lists of the sessions, the task queue, the schedules, the archive; a click on one: its actions
            const Theme& t = this->_theme;
            const std::int64_t now = cluster::now();
            const std::function<std::string(std::int64_t)> span = [](std::int64_t s) {
                s = std::max<std::int64_t>(0, s);
                return s < 60 ? std::to_string(s) + "s" : s < 3600 ? std::to_string(s / 60) + "m" : s < 86400 ? std::to_string(s / 3600) + "h" + (s % 3600 >= 60 ? std::to_string(s % 3600 / 60) + "m" : "")
                    : std::to_string(s / 86400) + "d";
            };
            const std::function<std::string(const std::string&)> who = [this](const std::string& by) {
                if (by.empty()) return std::string("you");
                if (by == GLOBAL_ID) return std::string("global");
                const std::optional<cluster::Snapshot> s = this->_manager.snapshot(by);
                return s ? s->spec.name : by;
            };
            const std::vector<cluster::Task> tasks = this->_manager.tasks();
            const std::vector<cluster::Schedule> schedules = this->_manager.schedules();
            const int room = std::max(30, width - 4);
            // each item: chosen by the keys (lit, kept in sight) or clicked; its part (todo lists, queue, schedules, archive)
            this->_taskSel = std::clamp(this->_taskSel, 0, std::max(0, static_cast<int>(this->_taskItems.size()) - 1));
            this->_taskItems.clear();
            const std::function<ftxui::Element(const int, ftxui::Element, const std::function<void(void)>&)> item =
                [this, &t](const int part, ftxui::Element element, const std::function<void(void)>& act) {
                    const int index = static_cast<int>(this->_taskItems.size());
                    this->_taskItems.emplace_back(part, act);
                    ftxui::Element shown = this->tap_(std::move(element), 0, [this, index, act]() {
                        this->_taskSel = index;
                        act();
                    });
                    return index == this->_taskSel ? ftxui::hbox({ftxui::text("▌") | ftxui::color(t.accent), shown | ftxui::flex}) | ftxui::bgcolor(t.surface) | ftxui::bold
                        | ftxui::focus : ftxui::hbox({ftxui::text(" "), shown | ftxui::flex});
                };
            ftxui::Elements out = {ftxui::hbox({ftxui::text(" ☰ Tasks ") | ftxui::bold | ftxui::color(t.bg) | ftxui::bgcolor(t.accent),
                ftxui::text("  the todo lists of the sessions, the task queue, the schedules") | ftxui::color(t.muted), ftxui::filler()})};
            const std::function<void(const std::string&, const std::string&)> title = [&](const std::string& name, const std::string& count) {
                out.push_back(ftxui::text(""));
                out.push_back(ftxui::hbox({ftxui::text(" " + name + " ") | ftxui::bold | ftxui::color(t.accent), ftxui::text(count) | ftxui::color(t.muted), ftxui::filler()}));
                out.push_back(ftxui::separatorLight() | ftxui::color(t.border));
            };
            // the todo lists (like claude.ai): done / to do of each session
            std::size_t lists = 0;
            ftxui::Elements todos;
            for (const cluster::Snapshot& s: this->_manager.list(true)) {
                if (s.todos.empty()) continue;
                ++lists;
                const auto done = std::count_if(s.todos.begin(), s.todos.end(), [](const cluster::Todo& d) {return d.status == "completed";});
                const std::string id = s.spec.id;
                todos.push_back(item(0, ftxui::hbox({ftxui::text(" " + (s.spec.global ? std::string("global") : s.spec.name)) | ftxui::bold | ftxui::color(t.text),
                    ftxui::text("  " + std::to_string(done) + "/" + std::to_string(s.todos.size()) + " done") | ftxui::color(done == static_cast<long>(s.todos.size()) ? t.ok : t.muted),
                    ftxui::filler()}), [this, id]() {this->_active = id;}));
                std::size_t shown = 0;
                for (const cluster::Todo& d: s.todos) {
                    if (++shown > 8) break;
                    const bool active = d.status == "in_progress";
                    const bool over = d.status == "completed";
                    todos.push_back(ftxui::hbox({ftxui::text(over ? "   ☒ " : active ? "   ◼ " : "   ☐ ") | ftxui::color(over ? t.ok : active ? t.accent : t.muted),
                        ftxui::text(cluster::one_line(active && !d.activeForm.empty() ? d.activeForm : d.content, static_cast<std::size_t>(room - 6)))
                        | (over ? ftxui::color(t.muted) | ftxui::dim | ftxui::strikethrough : active ? ftxui::color(t.accent) | ftxui::bold : ftxui::color(t.text))}));
                }
                if (s.todos.size() > 8) todos.push_back(ftxui::text("   … +" + std::to_string(s.todos.size() - 8) + " more") | ftxui::color(t.muted) | ftxui::dim);
            }
            title("Todo lists", std::to_string(lists) + " session(s)");
            if (todos.empty()) out.push_back(ftxui::text("   none: a session writes its list while it works (TodoWrite)") | ftxui::color(t.muted) | ftxui::dim);
            for (ftxui::Element& e: todos)
                out.push_back(std::move(e));
            // the task queue: what runs, waits, ended (the first ones)
            std::vector<cluster::Task> shown;
            std::map<std::string, int> count;
            std::size_t archived = 0;
            for (const cluster::Task& task: tasks) {
                if (task.archived) {
                    ++archived;
                    continue;
                }
                ++count[task.status];
                shown.push_back(task);
            }
            const std::function<int(const cluster::Task&)> rank = [](const cluster::Task& x) {return x.status == "running" ? 0 : x.status == "queued" ? 1 : 2;};
            std::stable_sort(shown.begin(), shown.end(), [&rank](const cluster::Task& a, const cluster::Task& b) {
                return rank(a) != rank(b) ? rank(a) < rank(b) : rank(a) < 2 ? a.created < b.created : a.ended > b.ended;
            });
            title("Task queue", std::to_string(count["running"]) + " running · " + std::to_string(count["queued"]) + " waiting · " + std::to_string(count["done"]) + " done"
                + (count["failed"] > 0 ? " · " + std::to_string(count["failed"]) + " failed" : "") + (count["cancelled"] > 0 ? " · " + std::to_string(count["cancelled"]) + " cancelled" : ""));
            if (shown.empty()) out.push_back(ftxui::text("   empty: type a prompt below (a task for a free session; target | prompt: for it)") | ftxui::color(t.muted) | ftxui::dim);
            for (std::size_t i = 0; i < shown.size() && i < 10; ++i) {
                const cluster::Task& task = shown[i];
                const ftxui::Color tint = task.status == "running" ? t.ok : task.status == "queued" ? t.accent : task.status == "done" ? t.muted : task.status == "failed" ? t.error : t.warn;
                const std::string mark = task.status == "running" ? "▶" : task.status == "queued" ? (task.forced ? "»" : "◌") : task.status == "done" ? "✓" : task.status == "failed" ? "✗" : "⊘";
                const std::string where = !task.session.empty() ? (this->_manager.snapshot(task.session) ? this->_manager.snapshot(task.session)->spec.name : task.session)
                    : task.target.empty() ? "any free session" : task.target;
                const std::string age = task.status == "queued" ? "waits " + span(now - task.created) : task.status == "running" ? span(now - task.started) : span(now - task.ended) + " ago";
                const std::string id = task.id;
                out.push_back(item(1, ftxui::hbox({ftxui::text("  " + mark + " ") | ftxui::color(tint) | ftxui::bold, ftxui::text(task.id + "  ") | ftxui::color(t.muted),
                    ftxui::text(cluster::one_line(task.prompt, static_cast<std::size_t>(std::max(20, room - 58)))) | ftxui::color(task.status == "done" ? t.muted : t.text), ftxui::filler(),
                    ftxui::text("  → " + cluster::one_line(where, 16)) | ftxui::color(t.muted), ftxui::text("  by " + who(task.by)) | ftxui::color(t.muted) | ftxui::dim,
                    ftxui::text("  " + age + " ") | ftxui::color(t.muted) | ftxui::dim}), [this, id]() {this->pick_("task:" + id);}));
                if (!task.reason.empty()) out.push_back(ftxui::text("       ↳ " + cluster::one_line(task.reason, static_cast<std::size_t>(room - 10))) | ftxui::color(t.warn) | ftxui::dim);
            }
            if (shown.size() > 10) out.push_back(ftxui::text("     … +" + std::to_string(shown.size() - 10) + " more") | ftxui::color(t.muted) | ftxui::dim);
            // the schedules: the waiting ones by their time, then the ones run / cancelled
            std::vector<cluster::Schedule> planned;
            int waiting = 0;
            for (const cluster::Schedule& w: schedules) {
                if (w.archived) {
                    ++archived;
                    continue;
                }
                waiting += w.status == "waiting" ? 1 : 0;
                planned.push_back(w);
            }
            std::stable_sort(planned.begin(), planned.end(), [](const cluster::Schedule& a, const cluster::Schedule& b) {
                return (a.status == "waiting") != (b.status == "waiting") ? a.status == "waiting" : a.status == "waiting" ? a.at < b.at : a.last > b.last;
            });
            title("Scheduled", std::to_string(waiting) + " waiting · " + std::to_string(planned.size() - static_cast<std::size_t>(waiting)) + " run / cancelled");
            if (planned.empty()) out.push_back(ftxui::text("   none: +30m <text> or 18:00 <text> below (a prompt to the global session), or ask the global session") | ftxui::color(t.muted) | ftxui::dim);
            for (std::size_t i = 0; i < planned.size() && i < 10; ++i) {
                const cluster::Schedule& w = planned[i];
                const bool open = w.status == "waiting";
                const std::string when = open ? "in " + span(w.at - now) + " (" + cluster::clock_time(w.at) + ")" : w.status == "done" ? "run " + span(now - w.last) + " ago" : "cancelled";
                const std::string where = w.action == "notify" ? "a notification" : "→ " + (w.session == GLOBAL_ID ? std::string("global") : (this->_manager.snapshot(w.session) ? this->_manager.snapshot(w.session)->spec.name : w.session));
                const std::string id = w.id;
                out.push_back(item(2, ftxui::hbox({ftxui::text(open ? "  ⏱ " : w.status == "done" ? "  ✓ " : "  ⊘ ") | ftxui::color(open ? t.accent : w.status == "done" ? t.muted : t.warn) | ftxui::bold,
                    ftxui::text(w.id + "  ") | ftxui::color(t.muted), ftxui::text(when + (w.every > 0 ? " · every " + span(static_cast<std::int64_t>(w.every) * 60) : "") + "  ") | ftxui::color(open ? t.text : t.muted),
                    ftxui::text(cluster::one_line(w.text, static_cast<std::size_t>(std::max(20, room - 70)))) | ftxui::color(open ? t.text : t.muted), ftxui::filler(),
                    ftxui::text("  " + where) | ftxui::color(t.muted), ftxui::text("  by " + who(w.by) + " ") | ftxui::color(t.muted) | ftxui::dim}), [this, id]() {this->pick_("sched:" + id);}));
                if (!w.reason.empty()) out.push_back(ftxui::text("       ↳ " + cluster::one_line(w.reason, static_cast<std::size_t>(room - 10))) | ftxui::color(t.warn) | ftxui::dim);
            }
            if (planned.size() > 10) out.push_back(ftxui::text("     … +" + std::to_string(planned.size() - 10) + " more") | ftxui::color(t.muted) | ftxui::dim);
            if (archived > 0) {
                out.push_back(ftxui::text(""));
                out.push_back(item(3, ftxui::text("  ▸ " + std::to_string(archived) + " archived: open") | ftxui::color(t.accent), [this]() {this->pick_("archive:");}));
            }
            out.push_back(ftxui::text(""));
            out.push_back(ftxui::paragraph("  ↑↓ / j k: choose · Tab / Shift+Tab: the next / previous part · Enter / →: its actions (a todo list: its session) · "
                "PgUp / PgDn, Home / End · Alt+N / Alt+P, Ctrl+B + arrow: another session") | ftxui::color(t.muted) | ftxui::dim);
            out.push_back(ftxui::paragraph("  its actions (a click too): start / run now, cancel or delete with a reason told to the session that made it, archive · "
                "the prompt: a task (target | prompt: for it), +30m <text> / 18:00 <text>: a reminder to the global session") | ftxui::color(t.muted) | ftxui::dim);
            return ftxui::vbox(std::move(out)) | ftxui::yframe | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, height) | ftxui::flex;
        }

        _nodiscard ftxui::Element sessionRow_(const cluster::Snapshot& s, const bool active, const int clip)
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
            ftxui::Element sub = ftxui::text("  " + cluster::one_line(s.spec.summary.empty() ? backend + " · " + cluster::short_path(s.spec.cwd) : s.spec.summary, 26))
                | ftxui::color(this->_theme.muted) | ftxui::dim;
            const std::string id = s.spec.id;
            this->_rowSessions[static_cast<int>(this->_clicks.size())] = id; // a right click: its menu
            ftxui::Element block = this->tap_(ftxui::vbox({line, sub}), 0, [this, id]() {this->_active = id;}, clip); // clicked: shown
            return active ? block | ftxui::bgcolor(this->_theme.surface) : block; // no focus: it would take the cursor of the prompt
        }

        _nodiscard ftxui::Element transcript_(const cluster::Snapshot& s, const int width, const int height, const bool compact)
        {
            // Wrapped lines of the transcript, the last ones (minus the scroll); kept as text for the mouse selection
            struct Row {
                std::string text;
                ftxui::Decorator style;
                std::string join;   // before it when copied
                std::vector<cluster::Cell> cells{};     // a line of a picture: its runs of colors (text: what is copied)
                std::vector<Hit> hits{};                // its parts clicked
            };
            std::vector<Row> lines;
            const bool searching = this->_mode == Mode::Search && s.spec.id == this->_active && !this->_matches.empty();
            const std::size_t hitEntry = searching ? this->_matches[static_cast<std::size_t>(this->_hit)] : std::string::npos;
            int hitLine = -1;
            const std::size_t start = s.entries.size() > 300 && !searching ? s.entries.size() - 300 : 0;
            const cluster::Config& config = this->_manager.config();
            const cluster::Frame frame = this->_reveal.step(s, config.typewriter, config.typewriterSpeed);
            const std::size_t room = static_cast<std::size_t>(std::max(14, width - 2));
            const std::string more = " (" + (config.keys.contains("expand") ? config.keys.at("expand") : std::string("Ctrl+O")) + " to expand)";
            // a text on its lines: the first one after `first`, the next ones after `next` (its indent)
            const std::function<void(const std::string&, const std::string&, const std::string&, const ftxui::Decorator&)> add =
                [&](const std::string& text, const std::string& first, const std::string& next, const ftxui::Decorator& style) {
                    std::vector<std::string> joins;
                    const std::vector<std::string> wrapped = wrap_(text, room - std::min(room - 10, width_(first)), &joins);
                    for (std::size_t k = 0; k < wrapped.size(); ++k)
                        lines.push_back({(k == 0 ? first : next) + wrapped[k], style, joins[k]});
                };
            // a rich display (MCP session_display): text, boxed tables, bars, sparklines, pictures in colored blocks (chafa)
            const std::string sid = s.spec.id;
            // a display acted on (ticked, clicked, played): what it did in the status line
            const std::function<std::function<void(void)>(const std::string&, const cluster::Json&)> act = [this, sid](const std::string& displayId, const cluster::Json& what) {
                return [this, sid, displayId, what]() {
                    try {
                        this->status_(this->_manager.displayAct(sid, displayId, what));
                    } catch (const utils::exception::IException& e) {
                        this->status_(e.info(), true);
                    }
                };
            };
            const std::function<void(std::size_t, const std::function<void(void)>&)> clickable = [&](const std::size_t from, const std::function<void(void)>& run) {
                for (std::size_t k = from; k < lines.size(); ++k)
                    lines[k].hits.emplace_back(0, 100000, run);
            };
            const std::function<void(const cluster::Json&, const std::string&)> display = [&](const cluster::Json& d, const std::string& key) {
                const Theme& th = this->_theme;
                const std::string displayId = d.value("id", std::string());
                const std::string type = d.value("type", std::string());
                const std::string title = d.value("title", std::string());
                const std::vector<ftxui::Color> palette = {th.accent, th.ok, th.warn, th.error, th.text};
                const std::function<std::string(const cluster::Json&)> cell = [](const cluster::Json& v) {return v.is_string() ? v.get<std::string>() : v.dump();};
                const std::function<std::string(const std::string&, const std::size_t)> pad = [](const std::string& text, const std::size_t size) {
                    const std::string cut = width_(text) > size ? cluster::one_line(text, size) : text;
                    return cut + std::string(size > width_(cut) ? size - width_(cut) : 0, ' ');
                };
                if (!title.empty()) lines.push_back({"▣ " + title, ftxui::color(th.accent) | ftxui::bold, "\n"});
                if (type == "markdown" || type == "code") {
                    bool code = type == "code";
                    for (const std::string& line: lines_(d.value(type == "code" ? "code" : "text", std::string()))) {
                        if (type == "markdown" && line.starts_with("```")) {
                            code = !code;
                            continue;
                        }
                        // its marks out of the text: **bold**, `code`, [text](link) -> text (link)
                        const std::string plain = code ? line : std::regex_replace(std::regex_replace(line, std::regex(R"(\*\*|__|`)"), ""),
                            std::regex(R"(\[([^\]]+)\]\(([^)]+)\))"), "$1 ($2)");
                        if (code) add(line, "  │ ", "  │ ", ftxui::color(th.muted));
                        else if (plain.starts_with("#")) add(cluster::trim(plain.substr(plain.find_first_not_of('#'))), "  ", "  ", ftxui::color(th.accent) | ftxui::bold);
                        else if (plain.starts_with("- ") || plain.starts_with("* ")) add(plain.substr(2), "  • ", "    ", ftxui::color(th.text));
                        else add(plain, "  ", "  ", ftxui::color(th.text));
                    }
                } else if (type == "callout") {
                    const std::string level = d.value("level", std::string("info"));
                    const ftxui::Color color = level == "success" ? th.ok : level == "warning" ? th.warn : level == "error" ? th.error : th.accent;
                    const std::string icon = level == "success" ? "✔ " : level == "warning" ? "⚠ " : level == "error" ? "✗ " : "ℹ ";
                    add(d.value("text", std::string()), "  " + icon, "    ", ftxui::color(color));
                } else if (type == "progress") {
                    const double max = std::max(1e-9, d.value("max", 100.0));
                    const double ratio = std::clamp(d.value("value", 0.0) / max, 0.0, 1.0);
                    const std::size_t size = std::min<std::size_t>(40, room > 20 ? room - 20 : 10);
                    const std::size_t full = static_cast<std::size_t>(ratio * static_cast<double>(size) + 0.5);
                    std::string bar;
                    for (std::size_t k = 0; k < size; ++k)
                        bar += k < full ? "█" : "░";
                    lines.push_back({"  " + bar + " " + std::to_string(static_cast<int>(ratio * 100 + 0.5)) + "%  " + d.value("label", std::string()),
                        ftxui::color(ratio >= 1.0 ? th.ok : th.accent), "\n"});
                } else if (type == "table") {
                    // boxed, its columns as wide as their texts (the widest ones cut to the room)
                    std::vector<std::vector<std::string>> rows;
                    if (d.contains("columns") && d["columns"].is_array()) {
                        rows.emplace_back();
                        for (const cluster::Json& c: d["columns"])
                            rows.back().push_back(cell(c));
                    }
                    for (const cluster::Json& r: d["rows"]) {
                        rows.emplace_back();
                        if (r.is_array())
                            for (const cluster::Json& c: r)
                                rows.back().push_back(cell(c));
                        else rows.back().push_back(cell(r));
                    }
                    std::size_t columns = 0;
                    for (const std::vector<std::string>& r: rows)
                        columns = std::max(columns, r.size());
                    std::vector<std::size_t> widths(columns, 1);
                    for (const std::vector<std::string>& r: rows)
                        for (std::size_t c = 0; c < r.size(); ++c)
                            widths[c] = std::max(widths[c], width_(r[c]));
                    while (columns > 0 && std::accumulate(widths.begin(), widths.end(), std::size_t{0}) + 3 * columns + 3 > room) {
                        std::size_t& widest = *std::max_element(widths.begin(), widths.end());
                        if (widest <= 4) break;
                        --widest;
                    }
                    const std::function<std::string(const char*, const char*, const char*)> rule = [&](const char* left, const char* middle, const char* right) {
                        std::string line = std::string("  ") + left;
                        for (std::size_t c = 0; c < columns; ++c) {
                            for (std::size_t k = 0; k < widths[c] + 2; ++k)
                                line += "─";
                            line += c + 1 < columns ? middle : right;
                        }
                        return line;
                    };
                    const bool header = d.contains("columns");
                    lines.push_back({rule("┌", "┬", "┐"), ftxui::color(th.border), "\n"});
                    for (std::size_t r = 0; r < rows.size(); ++r) {
                        std::string line = "  │";
                        for (std::size_t c = 0; c < columns; ++c)
                            line += " " + pad(c < rows[r].size() ? rows[r][c] : "", widths[c]) + " │";
                        lines.push_back({line, header && r == 0 ? ftxui::color(th.accent) | ftxui::bold : ftxui::color(th.text), "\n"});
                        if (header && r == 0) lines.push_back({rule("├", "┼", "┤"), ftxui::color(th.border), "\n"});
                    }
                    lines.push_back({rule("└", "┴", "┘"), ftxui::color(th.border), "\n"});
                } else if (type == "chart") {
                    const std::string kind = d.value("kind", std::string("bar"));
                    const cluster::Json labels = d.value("labels", cluster::Json::array());
                    const cluster::Json& series = d["series"];
                    double low = 0.0;
                    double high = 0.0;
                    for (const cluster::Json& s: series)
                        for (const cluster::Json& v: s["values"])
                            if (v.is_number()) {
                                low = std::min(low, v.get<double>());
                                high = std::max(high, v.get<double>());
                            }
                    const std::function<std::string(double)> number = [](const double v) {
                        std::ostringstream out;
                        out << std::setprecision(4) << v;
                        return out.str();
                    };
                    if (kind == "line") {
                        // a sparkline per series
                        static const std::array<const char*, 8> levels = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
                        for (std::size_t k = 0; k < series.size(); ++k) {
                            const cluster::Json& values = series[k]["values"];
                            double a = 1e300;
                            double b = -1e300;
                            for (const cluster::Json& v: values)
                                if (v.is_number()) {
                                    a = std::min(a, v.get<double>());
                                    b = std::max(b, v.get<double>());
                                }
                            std::string spark;
                            const std::size_t step = std::max<std::size_t>(1, (values.size() + room - 30) / std::max<std::size_t>(1, room > 30 ? room - 30 : 10));
                            for (std::size_t v = 0; v < values.size(); v += step)
                                spark += levels[values[v].is_number() && b > a ? static_cast<std::size_t>((values[v].get<double>() - a) / (b - a) * 7.0 + 0.5) : 0];
                            lines.push_back({"  " + pad(series[k].value("name", std::string("series")), 12) + " " + spark + "  " + number(a) + " .. " + number(b),
                                ftxui::color(palette[k % palette.size()]), "\n"});
                        }
                    } else {
                        // bars (pie: the shares of the first series)
                        double total = 0.0;
                        if (kind == "pie")
                            for (const cluster::Json& v: series[0]["values"])
                                total += v.is_number() ? v.get<double>() : 0.0;
                        std::size_t labelWidth = 4;
                        for (const cluster::Json& l: labels)
                            labelWidth = std::min<std::size_t>(24, std::max(labelWidth, width_(cell(l))));
                        const std::size_t size = room > labelWidth + 24 ? room - labelWidth - 24 : 10;
                        const std::size_t count = series[0]["values"].size();
                        for (std::size_t i = 0; i < count; ++i)
                            for (std::size_t k = 0; k < (kind == "pie" ? 1 : series.size()); ++k) {
                                const cluster::Json& v = series[k]["values"].size() > i ? series[k]["values"][i] : cluster::Json(0);
                                const double value = v.is_number() ? v.get<double>() : 0.0;
                                const double ratio = kind == "pie" ? (total > 0 ? value / total : 0.0) : (high > 0 ? std::max(0.0, value) / high : 0.0);
                                std::string bar;
                                for (std::size_t n = 0; n < static_cast<std::size_t>(ratio * static_cast<double>(size) + 0.5); ++n)
                                    bar += "█";
                                const std::string label = k == 0 ? (i < labels.size() ? cell(labels[i]) : std::to_string(i + 1)) : "";
                                lines.push_back({"  " + pad(label, labelWidth) + " " + bar + " " + (kind == "pie" ? std::to_string(static_cast<int>(ratio * 100 + 0.5)) + "%"
                                    : number(value)) + (series.size() > 1 && kind != "pie" ? "  " + series[k].value("name", std::string()) : ""),
                                    ftxui::color(palette[(kind == "pie" ? i : k) % palette.size()]), "\n"});
                            }
                    }
                } else if (type == "diff") {
                    for (const std::string& line: lines_(d.value("diff", std::string()))) {
                        const ftxui::Decorator style = line.starts_with("+++") || line.starts_with("---") ? ftxui::color(th.muted) | ftxui::bold
                            : line.starts_with("+") ? ftxui::color(th.ok) : line.starts_with("-") ? ftxui::color(th.error) : line.starts_with("@@") ? ftxui::color(th.accent)
                            : ftxui::color(th.text);
                        add(line, "  ", "  ", style);
                    }
                } else if (type == "json") {
                    const int depth = this->_expanded ? 12 : 4;
                    const std::size_t most = this->_expanded ? 2000 : 80;
                    for (const auto &[line, kind]: cluster::display_json_lines(d["data"], depth, most))
                        lines.push_back({"  " + line.substr(0, line.find_first_not_of(' ')) + cluster::one_line(line, room - 2), ftxui::color(kind == 's' ? th.ok : kind == 'n' ? th.warn : kind == 'k' ? th.accent : th.text), "\n"});
                } else if (type == "tree") {
                    for (const auto &[line, kind]: cluster::display_tree_lines(d["root"]))
                        lines.push_back({"  " + cluster::one_line(line, room - 2), kind == 'd' ? ftxui::color(th.accent) | ftxui::bold : kind == 'm' ? ftxui::color(th.warn)
                            : kind == 'n' ? ftxui::color(th.ok) : ftxui::color(th.text), "\n"});
                    if (d.value("cut", false)) lines.push_back({"  … (400 entries shown)", ftxui::color(th.muted), "\n"});
                } else if (type == "terminal") {
                    // its own colors; the last lines (Ctrl+O: all)
                    if (!d.value("command", std::string()).empty()) lines.push_back({"  $ " + d.value("command", std::string()), ftxui::color(th.accent), "\n"});
                    const std::vector<std::vector<cluster::Cell>> output = cluster::ansi_cells(d.value("output", std::string()));
                    const std::size_t from = this->_expanded || output.size() <= 30 ? 0 : output.size() - 30;
                    if (from > 0) lines.push_back({"  … " + std::to_string(from) + " lines before" + more, ftxui::color(th.muted) | ftxui::dim, "\n"});
                    for (std::size_t k = from; k < output.size(); ++k) {
                        std::string text = "  ";
                        std::vector<cluster::Cell> cells = {{"  ", 0, 0, false, false}};
                        for (const cluster::Cell& c: output[k]) {
                            text += c.text;
                            cells.push_back(c);
                        }
                        lines.push_back({text, ftxui::color(th.text), "\n", output[k].empty() ? std::vector<cluster::Cell>{} : cells});
                    }
                } else if (type == "checklist") {
                    std::size_t ticked = 0;
                    for (std::size_t k = 0; k < d["items"].size(); ++k) {
                        const cluster::Json& item = d["items"][k];
                        const bool done = item.value("done", false);
                        ticked += done ? 1 : 0;
                        const std::size_t from = lines.size();
                        add(item.value("text", std::string()), done ? "  ☑ " : "  ☐ ", "    ", done ? ftxui::color(th.muted) | ftxui::dim : ftxui::color(th.text));
                        clickable(from, act(displayId, {{"item", k}}));
                    }
                    lines.push_back({"  " + std::to_string(ticked) + "/" + std::to_string(d["items"].size()) + " done · a click or the palette: Tick", ftxui::color(th.muted) | ftxui::dim, "\n"});
                } else if (type == "steps") {
                    for (const cluster::Json& step: d["steps"]) {
                        const std::string state = step.value("state", std::string("todo"));
                        add(step.value("text", std::string()), state == "done" ? "  ✔ " : state == "current" ? "  ● " : state == "error" ? "  ✗ " : "  ○ ", "    ",
                            state == "done" ? ftxui::color(th.ok) : state == "current" ? ftxui::color(th.accent) | ftxui::bold : state == "error" ? ftxui::color(th.error)
                            : ftxui::color(th.muted));
                    }
                } else if (type == "timeline") {
                    // a Gantt: a bar per task on the span of all of them
                    double low = 1e18;
                    double high = -1e18;
                    std::size_t name = 4;
                    for (const cluster::Json& task: d["tasks"]) {
                        low = std::min(low, task.value("from", 0.0));
                        high = std::max(high, task.value("to", 0.0));
                        name = std::min<std::size_t>(20, std::max(name, width_(task.value("name", std::string()))));
                    }
                    const std::size_t size = room > name + 30 ? room - name - 30 : 10;
                    const double span = std::max(1e-9, high - low);
                    for (std::size_t k = 0; k < d["tasks"].size(); ++k) {
                        const cluster::Json& task = d["tasks"][k];
                        const std::size_t a = static_cast<std::size_t>((task.value("from", 0.0) - low) / span * static_cast<double>(size));
                        const std::size_t b = std::max(a + 1, static_cast<std::size_t>((task.value("to", 0.0) - low) / span * static_cast<double>(size) + 0.5));
                        std::string bar;
                        for (std::size_t n = 0; n < size; ++n)
                            bar += n >= a && n < b ? "█" : "·";
                        lines.push_back({"  " + pad(task.value("name", std::string()), name) + " " + bar + "  " + cell(task["start"]) + " → " + cell(task["end"]),
                            ftxui::color(palette[k % palette.size()]), "\n"});
                    }
                } else if (type == "kanban") {
                    // its columns side by side, their cards one per line
                    const cluster::Json& columns = d["columns"];
                    const std::size_t count = std::max<std::size_t>(1, columns.size());
                    const std::size_t size = std::max<std::size_t>(8, (room > 4 ? room - 4 : 20) / count - 1);
                    std::size_t tallest = 0;
                    std::string head = "  ";
                    for (const cluster::Json& column: columns) {
                        head += pad(column.value("name", std::string()) + " (" + std::to_string(column["cards"].size()) + ")", size) + " ";
                        tallest = std::max(tallest, column["cards"].size());
                    }
                    lines.push_back({head, ftxui::color(th.accent) | ftxui::bold, "\n"});
                    for (std::size_t r = 0; r < tallest; ++r) {
                        std::string row = "  ";
                        for (const cluster::Json& column: columns)
                            row += pad(r < column["cards"].size() ? "▪ " + cell(column["cards"][r]) : "", size) + " ";
                        lines.push_back({row, ftxui::color(th.text), "\n"});
                    }
                } else if (type == "actions") {
                    // the buttons on one line, each one its cells clicked
                    Row row{"  ", ftxui::color(th.accent) | ftxui::bold, "\n"};
                    for (std::size_t k = 0; k < d["buttons"].size(); ++k) {
                        const std::string button = (d["buttons"][k].contains("clicked") ? "[✓ " : "[ ") + d["buttons"][k].value("label", std::string()) + " ]";
                        const int from = static_cast<int>(width_(row.text));
                        row.text += button;
                        row.hits.emplace_back(from, static_cast<int>(width_(row.text)) - 1, act(displayId, {{"item", k}}));
                        row.text += "  ";
                    }
                    lines.push_back(row);
                    lines.push_back({"  a click, the palette: Action" + std::string(this->_manager.config().voice.enabled ? " · voice: say its label" : ""), ftxui::color(th.muted) | ftxui::dim, "\n"});
                } else if (type == "form") {
                    const cluster::Json values = d.value("values", cluster::Json::object());
                    for (const cluster::Json& field: d["fields"]) {
                        const std::string field_name = field.value("name", std::string());
                        const std::string value = values.contains(field_name) ? cell(values[field_name]) : field.contains("value") ? cell(field["value"]) : "____";
                        std::string hint = field.value("type", std::string("text"));
                        if (field.contains("options")) hint = "";
                        if (field.contains("options"))
                            for (const cluster::Json& o: field["options"])
                                hint += (hint.empty() ? "" : " | ") + cell(o);
                        add(field.value("label", field_name) + ": " + value + "  (" + hint + ")", "  ", "    ", ftxui::color(th.text));
                    }
                    if (d.value("submitted", false)) {
                        lines.push_back({"  ✓ sent", ftxui::color(th.ok) | ftxui::dim, "\n"});
                    } else {
                        // clicked: its fields asked (name=value; ...), like the palette
                        std::string fill;
                        for (const cluster::Json& field: d["fields"])
                            fill += (fill.empty() ? "" : "; ") + field.value("name", std::string()) + "=" + (field.contains("value") ? cell(field["value"]) : "");
                        const std::string name = title.empty() ? std::string("the form") : title;
                        lines.push_back({"  ▶ Fill the form (a click, the palette)", ftxui::color(th.accent), "\n"});
                        clickable(lines.size() - 1, [this, displayId, name, fill]() {
                            this->_pending = {"display_form:" + displayId, "Fill the form: " + name, "name=value; ...", fill, false};
                            this->_prompt = fill;
                            this->_password = false;
                            this->_mode = Mode::Prompt;
                        });
                    }
                } else if (type == "details") {
                    const std::size_t from = lines.size();
                    add(d.value("summary", std::string()), this->_expanded ? "  ▾ " : "  ▸ ", "    ", ftxui::color(th.text) | ftxui::bold);
                    clickable(from, [this]() {this->_expanded = !this->_expanded;}); // clicked: opened / folded (every one, like Ctrl+O)
                    if (this->_expanded)
                        for (const std::string& line: lines_(d.value("text", std::string())))
                            add(line, "    ", "    ", ftxui::color(th.text));
                    else lines.push_back({"    " + more.substr(1), ftxui::color(th.muted) | ftxui::dim, "\n"});
                } else if (type == "compare") {
                    // two columns, the lines that differ colored
                    const std::size_t half = std::max<std::size_t>(10, (room > 6 ? room - 6 : 20) / 2);
                    const std::vector<std::string> left = lines_(d["left"].value("text", std::string()));
                    const std::vector<std::string> right = lines_(d["right"].value("text", std::string()));
                    lines.push_back({"  " + pad(d["left"].value("title", std::string("A")), half) + " │ " + d["right"].value("title", std::string("B")),
                        ftxui::color(th.accent) | ftxui::bold, "\n"});
                    for (std::size_t k = 0; k < std::max(left.size(), right.size()); ++k) {
                        const std::string a = k < left.size() ? left[k] : "";
                        const std::string b = k < right.size() ? right[k] : "";
                        lines.push_back({"  " + pad(a, half) + " │ " + cluster::one_line(b, half), a == b ? ftxui::color(th.text) : ftxui::color(th.warn), "\n"});
                    }
                } else if (type == "metrics") {
                    for (const cluster::Json& m: d["items"]) {
                        const std::string delta = m.contains("delta") ? cell(m["delta"]) : "";
                        const bool down = delta.starts_with("-");
                        add(pad(m.value("label", std::string()), 18) + "  " + cell(m.value("value", cluster::Json(""))) + m.value("unit", std::string())
                            + (delta.empty() ? "" : (down ? "  ▼ " : "  ▲ ") + delta) + (m.contains("trend") ? "  " + cluster::display_spark(m["trend"], 24) : ""),
                            "  ", "  ", ftxui::color(delta.empty() ? th.text : down ? th.error : th.ok));
                    }
                } else if (type == "heatmap") {
                    // a cell per value, its color from the surface to the accent
                    double high = 0.0;
                    for (const cluster::Json& row: d["rows"])
                        for (const cluster::Json& v: row)
                            if (v.is_number()) high = std::max(high, v.get<double>());
                    const cluster::Json ylabels = d.value("ylabels", cluster::Json::array());
                    for (std::size_t r = 0; r < d["rows"].size(); ++r) {
                        const std::string label = pad(r < ylabels.size() ? cell(ylabels[r]) : "", 4);
                        std::vector<cluster::Cell> cells = {{"  " + label + " ", 0x888888, 0, true, false}};
                        std::string text = "  " + label + " ";
                        for (const cluster::Json& v: d["rows"][r]) {
                            const double ratio = v.is_number() && high > 0 ? v.get<double>() / high : -1.0;
                            const std::uint32_t level = ratio < 0 ? 0x303030 : static_cast<std::uint32_t>(40 + ratio * 200);
                            cells.push_back({"■ ", ratio < 0 ? 0x303030u : (level << 16) | (static_cast<std::uint32_t>(level * 0.5) << 8) | 0x30u, 0, true, false});
                            text += "■ ";
                        }
                        lines.push_back({text, ftxui::nothing, "\n", cells});
                    }
                } else if (type == "audio") {
                    lines.push_back({"  ▶ ♪ " + d.value("alt", d.value("text", std::string("audio"))).substr(0, room) + "  (a click, the palette: Play)", ftxui::color(th.accent), "\n"});
                    clickable(lines.size() - 1, act(displayId, cluster::Json::object()));
                } else if (type == "math") {
                    add(d.value("unicode", std::string()), "  ", "  ", ftxui::color(th.text) | ftxui::bold);
                } else if (type == "file") {
                    // its files: a click opens it (the application of the desktop)
                    for (const cluster::Json& f: d.value("files", cluster::Json::array())) {
                        const std::string path = f.value("path", std::string());
                        const std::uintmax_t size = f.value("size", static_cast<std::uintmax_t>(0));
                        const std::string human = size >= 1048576 ? std::to_string(size / 1048576) + " MB" : std::to_string(std::max<std::uintmax_t>(1, size / 1024)) + " KB";
                        lines.push_back({"  ▤ " + f.value("name", std::string()) + "   " + human + " · " + f.value("mime", std::string()) + "   (a click: open)",
                            ftxui::color(th.accent), "\n"});
                        lines.back().hits.emplace_back(0, 100000, [this, path]() {
                            cluster::detach({"xdg-open", path});
                            this->status_("opened " + std::filesystem::path(path).filename().string());
                        });
                        lines.push_back({"    " + cluster::one_line(cluster::short_path(std::filesystem::path(path).parent_path().string()), room - 4), ftxui::color(th.muted) | ftxui::dim, "\n"});
                    }
                } else {
                    // image, diagram, svg: in blocks of colors (chafa), made once for a width
                    const std::string at = key + ":" + std::to_string(room);
                    if (!this->_pictures.contains(at)) {
                        if (this->_pictures.size() > 40) this->_pictures.clear();
                        this->_pictures[at] = cluster::display_cells(cluster::display_bytes(d), static_cast<int>(std::min<std::size_t>(room > 4 ? room - 4 : 10, 100)),
                            std::clamp(height * 2 / 3, 8, 30));
                    }
                    const std::vector<std::vector<cluster::Cell>>& picture = this->_pictures[at];
                    if (picture.empty()) lines.push_back({"  [" + type + "] " + d.value("alt", std::string()) + " (chafa not installed: the window or the web page shows it)",
                        ftxui::color(th.muted), "\n"});
                    for (const std::vector<cluster::Cell>& row: picture) {
                        std::string text = "  ";
                        for (const cluster::Cell& c: row)
                            text += c.text;
                        std::vector<cluster::Cell> cells = {{"  ", 0, 0, false, false}};
                        cells.insert(cells.end(), row.begin(), row.end());
                        lines.push_back({text, ftxui::nothing, "\n", cells});
                    }
                    const std::string caption = d.value("alt", d.value("caption", std::string()));
                    if (!caption.empty()) add(caption, "  ", "  ", ftxui::color(th.muted) | ftxui::italic);
                }
            };
            for (std::size_t i = start; i < frame.visible && i < s.entries.size(); ++i) {
                const cluster::Entry& e = s.entries[i];
                const bool matched = searching && std::find(this->_matches.begin(), this->_matches.end(), i) != this->_matches.end();
                if (i == hitEntry) hitLine = static_cast<int>(lines.size());
                const ftxui::Decorator mark = i == hitEntry ? ftxui::inverted : matched ? ftxui::bgcolor(this->_theme.surface) : ftxui::nothing;
                const std::string text = i == frame.typed ? e.text.substr(0, frame.typedBytes) : e.text;
                const bool block = e.kind != cluster::EntryKind::ToolResult && e.kind != cluster::EntryKind::User && !(compact && e.kind == cluster::EntryKind::Thinking);
                if (block && !lines.empty() && !lines.back().text.empty()) lines.push_back({"", ftxui::nothing, "\n"});
                switch (e.kind) {
                    case cluster::EntryKind::User:
                        if (!lines.empty()) lines.push_back({"", ftxui::nothing, "\n"});
                        add(text.starts_with("[voice] ") ? text.substr(8) : text, "› ", "  ", ftxui::color(this->_theme.accent) | ftxui::bold | mark);
                        break;
                    case cluster::EntryKind::Assistant:
                        add(text, "⏺ ", "  ", ftxui::color(this->_theme.text) | mark);
                        break;
                    case cluster::EntryKind::Thinking:
                        // like Claude Code: folded, Ctrl+O shows it
                        if (compact) break;
                        if (!this->_expanded) {
                            lines.push_back({"∴ Thinking" + more, ftxui::color(this->_theme.muted) | ftxui::dim | ftxui::italic | mark, "\n"});
                            break;
                        }
                        lines.push_back({"∴ Thinking…", ftxui::color(this->_theme.muted) | ftxui::italic | mark, "\n"});
                        add(text, "  ", "  ", ftxui::color(this->_theme.muted) | ftxui::dim | ftxui::italic | mark);
                        break;
                    case cluster::EntryKind::Tool:
                        if (e.tool == "TodoWrite") {
                            lines.push_back({"⏺ Update Todos", ftxui::color(this->_theme.ok) | ftxui::bold | mark, "\n"});
                            add(text, "  ⎿  ", "     ", ftxui::color(this->_theme.text) | mark);
                        } else {
                            add(e.tool + "(" + text + ")", "⏺ ", "  ", ftxui::color(this->_theme.ok) | ftxui::bold | mark);
                        }
                        break;
                    case cluster::EntryKind::ToolResult: {
                        // the first lines (Ctrl+O: all)
                        if (compact) break;
                        const std::vector<std::string> parts = lines_(text);
                        const std::size_t keep = this->_expanded ? parts.size() : std::min<std::size_t>(parts.size(), 3);
                        std::string shown;
                        for (std::size_t k = 0; k < keep; ++k)
                            shown += (k ? "\n" : "") + parts[k];
                        add(cluster::trim(shown).empty() ? "(no output)" : shown, "  ⎿  ", "     ", ftxui::color(this->_theme.muted) | mark);
                        if (parts.size() > keep)
                            lines.push_back({"     … +" + std::to_string(parts.size() - keep) + " lines" + more, ftxui::color(this->_theme.muted) | ftxui::dim, "\n"});
                        break;
                    }
                    case cluster::EntryKind::System:
                        add(text, "· ", "  ", ftxui::color(this->_theme.muted) | ftxui::dim | mark);
                        break;
                    case cluster::EntryKind::Error:
                        add(text, "✗ ", "  ", ftxui::color(this->_theme.error) | mark);
                        break;
                    case cluster::EntryKind::Display: {
                        const cluster::Json d = cluster::Json::parse(e.text, nullptr, false);
                        if (d.is_object()) display(d, s.spec.id + ":" + std::to_string(e.time) + ":" + std::to_string(i));
                        break;
                    }
                }
            }
            // the text streamed (typed), the live thinking, the state of the turn (like Claude Code)
            if (!lines.empty() && !lines.back().text.empty() && ((frame.typed == static_cast<std::size_t>(-1) && frame.partialBytes > 0) || !s.thinking.empty()
                || s.state == cluster::State::Working))
                lines.push_back({"", ftxui::nothing, "\n"});
            if (frame.typed == static_cast<std::size_t>(-1) && frame.partialBytes > 0) add(s.partial.substr(0, frame.partialBytes), "⏺ ", "  ", ftxui::color(this->_theme.text));
            if (!s.thinking.empty() && !compact) {
                if (this->_expanded) {
                    add(s.thinking, "∴ ", "  ", ftxui::color(this->_theme.muted) | ftxui::dim | ftxui::italic);
                } else {
                    const std::vector<std::string> thought = lines_(cluster::trim(s.thinking));
                    lines.push_back({"∴ " + cluster::one_line(thought.back(), room - 4), ftxui::color(this->_theme.muted) | ftxui::dim | ftxui::italic, "\n"});
                }
            }
            if (s.state == cluster::State::Working) {
                static const std::vector<std::string> spinner = {"·", "✢", "✳", "✶", "✻", "✽", "✻", "✶", "✳", "✢"};
                const std::int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                std::int64_t since = 0;
                for (auto it = s.entries.rbegin(); it != s.entries.rend() && since == 0; ++it)
                    if (it->kind == cluster::EntryKind::User) since = it->time;
                // its step (thinking, writing, a tool written, the tools running, the model awaited), the tokens as they come
                const cluster::Json turn = cluster::turn_state(s);
                const std::string tokens = turn.value("tokens", std::string());
                const std::string expand = config.keys.contains("expand") ? config.keys.at("expand") : std::string("Ctrl+O");
                lines.push_back({spinner[static_cast<std::size_t>(ms / 120) % spinner.size()] + " " + turn.value("verb", std::string()) + "… "
                    + cluster::took(turn.value("step_ms", 0LL)) + "  (turn " + cluster::took(turn.value("turn_ms", since > 0 ? (cluster::now() - since) * 1000 : 0LL))
                    + (tokens.empty() ? "" : " · ↓ " + tokens) + " · " + config.keys.at("interrupt") + " to interrupt" + (this->_expanded ? "" : " · " + expand + " details") + ")"
                    + (s.queued > 0 ? "  " + std::to_string(s.queued) + " queued" : std::string()), ftxui::color(this->_theme.accent), "\n"});
                if (!turn.value("detail", std::string()).empty()) // what it waits on (its context, the error retried, the thinking estimated)
                    lines.push_back({"  ⎿ " + cluster::one_line(turn.value("detail", std::string()), room - 22)
                        + (cluster::resumable(s) == "retry" ? " · " + config.keys.at("resume") + " retry now" : std::string()), ftxui::color(this->_theme.muted), "\n"});
                // the tools running: their input, how long
                const cluster::Json& tools = turn["tools"];
                const std::size_t most = this->_expanded ? 20 : 4;
                for (std::size_t k = 0; k < tools.size() && k < most; ++k) {
                    const std::string head = "  ⎿ " + tools[k].value("name", std::string()) + "  ";
                    const std::string age = "  " + cluster::took(tools[k].value("ms", 0LL));
                    const std::size_t left = room > width_(head) + width_(age) + 4 ? room - width_(head) - width_(age) : 10;
                    lines.push_back({head + cluster::one_line(tools[k].value("summary", std::string()), left) + age, ftxui::color(this->_theme.muted), "\n"});
                }
                if (tools.size() > most) lines.push_back({"  ⎿ … " + std::to_string(tools.size() - most) + " more", ftxui::color(this->_theme.muted) | ftxui::dim, "\n"});
                if (this->_expanded) {
                    // the rounds of the turn: one request to the model a line (its wait, its thinking, the tools it called)
                    for (const cluster::Json& round: turn["rounds"])
                        lines.push_back({"  │ " + cluster::one_line(round.value("text", std::string()), room - 6), round.value("current", false)
                            ? ftxui::color(this->_theme.accent) : ftxui::color(this->_theme.muted) | ftxui::dim, "\n"});
                    if (turn.value("hidden_thinking", false))
                        lines.push_back({"  (the text of its thinking is not sent by the model: its tokens only)", ftxui::color(this->_theme.muted) | ftxui::dim, "\n"});
                }
                this->_animating = true;
            }
            for (std::size_t q = 0; q < s.queue.size(); ++q) {
                // the first one: clicked (or F8), sent now
                const std::string now = q == 0 && !compact ? "  (" + config.keys.at("send_next") + " / a click: send it now · Esc: edit)" : compact ? "" : "  (Esc: edit)";
                Row row{"⧗ queued: " + cluster::one_line(s.queue[q], static_cast<std::size_t>(std::max(10, width - 60))) + now, ftxui::color(this->_theme.warn), "\n"};
                if (q == 0) row.hits.emplace_back(0, std::max(10, width), [this]() {(void)this->shortcut_("send_next");});
                lines.push_back(row);
            }

            int& scroll = this->_scroll[s.spec.id];
            if (hitLine >= 0) scroll = std::max(0, static_cast<int>(lines.size()) - hitLine - height / 2);
            scroll = std::clamp(scroll, 0, std::max(0, static_cast<int>(lines.size()) - height));
            const int end = static_cast<int>(lines.size()) - scroll;
            const int begin = std::max(0, end - height);
            View& view = this->_views[s.spec.id];
            view.lines.clear();
            view.joins.clear();
            view.hits.clear();
            // the mouse selection of this transcript: from (xa, ya) to (xb, yb), cells
            const bool selecting = this->_selection && this->_selection->session == s.spec.id;
            const Selection sel = selecting ? *this->_selection : Selection{};
            const bool forward = sel.y0 < sel.y1 || (sel.y0 == sel.y1 && sel.x0 <= sel.x1);
            const int ya = forward ? sel.y0 : sel.y1;
            const int xa = forward ? sel.x0 : sel.x1;
            const int yb = forward ? sel.y1 : sel.y0;
            const int xb = forward ? sel.x1 : sel.x0;
            ftxui::Elements shown;
            for (int k = begin; k < end; ++k) {
                const Row& row = lines[static_cast<std::size_t>(k)];
                const int y = k - begin;
                view.lines.push_back(row.text);
                view.joins.push_back(row.join);
                view.hits.push_back(row.hits);
                if (!row.cells.empty()) { // a line of a picture: its colors
                    ftxui::Elements runs;
                    for (const cluster::Cell& c: row.cells)
                        runs.push_back(ftxui::text(c.text) | (c.hasFg ? ftxui::color(ftxui::Color::RGB((c.fg >> 16) & 255, (c.fg >> 8) & 255, c.fg & 255)) : ftxui::nothing)
                            | (c.hasBg ? ftxui::bgcolor(ftxui::Color::RGB((c.bg >> 16) & 255, (c.bg >> 8) & 255, c.bg & 255)) : ftxui::nothing));
                    shown.push_back(ftxui::hbox(std::move(runs)));
                    continue;
                }
                if (!selecting || y < ya || y > yb || (ya == yb && xa == xb)) {
                    shown.push_back(ftxui::text(row.text) | row.style);
                    continue;
                }
                const std::size_t from = y == ya ? byte_at_(row.text, xa) : 0;
                const std::size_t to = std::max(from, y == yb ? byte_at_(row.text, xb + 1) : row.text.size());
                shown.push_back(ftxui::hbox({ftxui::text(row.text.substr(0, from)) | row.style, ftxui::text(row.text.substr(from, to - from)) | row.style | ftxui::inverted,
                    ftxui::text(row.text.substr(to)) | row.style}));
            }
            if (scroll > 0) shown.push_back(ftxui::text("↓ " + std::to_string(scroll) + " more line(s) (PageDown)") | ftxui::color(this->_theme.accent) | ftxui::dim);
            return ftxui::vbox(std::move(shown)) | ftxui::yflex | ftxui::reflect(view.box);
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

            if (s.spec.panels.contains("tokens")) {
                // the new tokens (in + written to the cache + out) apart from the ones read again from the cache at every step
                const std::function<std::string(const cluster::Usage&)> split = [](const cluster::Usage& u) {
                    return cluster::human_tokens(u.input + u.cacheCreation + u.output) + (u.cacheRead > 0 ? " +" + cluster::human_tokens(u.cacheRead) + " cache" : "");
                };
                section("Tokens", {line("last turn", split(m.last)), line("current", split(m.current)), line("total", split(m.total)),
                    line("turns", std::to_string(m.turns))});
            }
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
            if (s.spec.panels.contains("usage")) {
                // the vendor of the session over every session and run: today, 7 days, lifetime, its limits left
                ftxui::Elements body;
                for (const auto &[left, right]: this->_manager.usageLines(s))
                    body.push_back(line(left, right));
                section("Usage", std::move(body));
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
            if (s.spec.panels.contains("tasks")) {
                ftxui::Elements body;
                for (const cluster::Todo& d: s.todos)
                    body.push_back(ftxui::hbox({ftxui::text(d.status == "completed" ? "☒ " : d.status == "in_progress" ? "◼ " : "☐ ")
                        | ftxui::color(d.status == "completed" ? this->_theme.ok : d.status == "in_progress" ? this->_theme.accent : this->_theme.muted),
                        ftxui::text(cluster::one_line(d.content, 30)) | ftxui::color(d.status == "completed" ? this->_theme.muted : this->_theme.text)}));
                for (const cluster::Task& task: this->_manager.tasks())
                    if (!task.archived && (task.session == s.spec.id || task.target == s.spec.id) && (task.status == "queued" || task.status == "running"))
                        body.push_back(ftxui::text((task.status == "running" ? "▶ " : "◌ ") + task.id + " " + cluster::one_line(task.prompt, 26)) | ftxui::color(this->_theme.muted));
                if (body.empty()) body.push_back(ftxui::text("no todo list, no task") | ftxui::color(this->_theme.muted) | ftxui::dim);
                section("Tasks", std::move(body));
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

        _nodiscard std::string whereOf_(const cluster::Snapshot& s)
        {
            // " · <branch> · <computer> · pid <n>" (its branch read again every 5 s)
            const std::int64_t now = cluster::now();
            if (now - this->_branchAt[s.spec.id] >= 5) {
                this->_branches[s.spec.id] = cluster::git_branch(cluster::expand_home(s.spec.cwd));
                this->_branchAt[s.spec.id] = now;
            }
            const std::string branch = this->_branches[s.spec.id];
            return (branch.empty() ? "" : " · ⎇ " + branch) + " · " + this->_host + (s.pid > 0 ? " · pid " + std::to_string(s.pid) : "");
        }

        _nodiscard ftxui::Element header_(const cluster::Snapshot& s, const bool compact)
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
                ftxui::text(s.spec.effort.empty() || s.spec.effort == "default" ? "" : "  effort " + s.spec.effort) | ftxui::color(this->_theme.muted),
                ftxui::filler(),
                // where it runs: its folder (a click opens it), the computer, its branch, its process
                this->tap_(ftxui::text(cluster::short_path(s.spec.cwd)) | ftxui::color(this->_theme.muted), 0, [this, cwd = s.spec.cwd]() {
                    cluster::open_folder(cluster::expand_home(cwd));
                    this->status_("opened " + cluster::short_path(cwd));
                }),
                ftxui::text(this->whereOf_(s) + " ") | ftxui::color(this->_theme.muted) | ftxui::dim,
            };
            ftxui::Element head = ftxui::hbox(std::move(left));
            if (!s.warning.empty() && cluster::now() - s.warningAt < 60) // a problem of its interfaces (two prompts at once...)
                head = ftxui::vbox({head, ftxui::text(" ⚠ " + cluster::one_line(s.warning, 200)) | ftxui::color(this->_theme.warn)});
            if (!s.lastError.empty())
                return ftxui::vbox({head, ftxui::hbox({ftxui::text(" " + cluster::one_line(s.lastError, 180)) | ftxui::color(this->_theme.error),
                    cluster::resumable(s) == "continue" ? this->tap_(ftxui::text("  " + this->_manager.config().keys.at("resume") + " continue") | ftxui::color(this->_theme.accent), 0,
                        [this]() {(void)this->shortcut_("resume");}) : ftxui::emptyElement()})});
            if (!s.remoteUrl.empty() || !s.bridgeState.empty())
                return ftxui::vbox({head, ftxui::text(" Remote Control " + (s.bridgeState.empty() ? std::string("on") : s.bridgeState) + (s.remoteUrl.empty() ? std::string() : ": " + s.remoteUrl)
                    + "  (live on claude.ai and here)") | ftxui::color(this->_theme.accent)});
            return head;
        }

        _nodiscard ftxui::Element permission_(const cluster::Snapshot& s)
        {
            if (s.permissions.empty()) return ftxui::emptyElement();
            const cluster::Permission& p = s.permissions.front();
            const cluster::Config& c = this->_manager.config();
            const std::string id = s.spec.id;
            // its answers clicked (on this session, the one shown or not)
            const std::function<ftxui::Element(const std::string&, const std::string&)> answer = [&](const std::string& label, const std::string& action) {
                return this->tap_(ftxui::text(label) | ftxui::color(this->_theme.muted), 0, [this, id, action]() {
                    this->_active = id;
                    if (action == "global") (void)this->shortcut_("global");
                    else this->run_({action, "", "", "", false}, "");
                });
            };
            const std::function<ftxui::Element(void)> dot = [this]() {return ftxui::text(" · ") | ftxui::color(this->_theme.muted);}; // new each time: drawn once
            if (p.tool == "AskUserQuestion")
                return ftxui::hbox({ftxui::text(" question ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent),
                    ftxui::text(" " + cluster::one_line(p.description, 100) + " ") | ftxui::color(this->_theme.text), ftxui::filler(),
                    c.questionsToGlobal && s.spec.id != GLOBAL_ID ? answer("answer it on the global session (" + c.keys.at("global") + ")", "global")
                        : ftxui::text("its popup: shown on this session") | ftxui::color(this->_theme.muted), dot(),
                    answer(c.keys.at("deny") + " decline ", "deny")}) | ftxui::bgcolor(this->_theme.surface);
            ftxui::Element bar = ftxui::hbox({
                ftxui::text(p.safety ? " safety check " : " permission ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(p.safety ? this->_theme.error : this->_theme.warn),
                ftxui::text(" " + p.tool + ": " + cluster::one_line(p.description, 80) + " ") | ftxui::color(this->_theme.text),
                ftxui::filler(),
                answer(c.keys.at("allow") + (p.safety ? " run it anyway" : " allow"), "allow"), dot(),
                answer(c.keys.at("deny") + (p.safety ? " leave it " : " deny"), "deny"),
                p.safety ? ftxui::emptyElement() : ftxui::hbox({dot(), answer("always ", "always")}),
            }) | ftxui::bgcolor(this->_theme.surface);
            if (p.reason.empty()) return bar;
            return ftxui::vbox({bar, ftxui::text("  why: " + cluster::one_line(p.reason, 200)) | ftxui::color(this->_theme.muted) | ftxui::bgcolor(this->_theme.surface)});
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
            if (voice.listening() && !voice.ready()) label = " … loading the speech model (~10 s) ";
            ftxui::Elements row = {ftxui::text(label) | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(voice.armed() || (voice.listening() && conf.mode != "wake") ? this->_theme.ok : this->_theme.muted)};
            for (const cluster::Pending& p: voice.pending()) {
                const std::int64_t left = p.deadline == 0 ? -1 : std::max<std::int64_t>(0, (p.deadline - std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() + 999) / 1000);
                const std::string target = p.target.empty() ? "" : (p.target == GLOBAL_ID ? "global" : p.target);
                const std::string when = p.paused ? " (paused while you speak)" : left >= 0 ? " (" + std::to_string(left) + "s, palette: send / drop / correct)" : "";
                const std::string said = target.empty() ? cluster::tail_utf8(p.text, 110) : cluster::one_line(p.text, 90); // live: its end
                row.push_back(ftxui::text(" " + (target.empty() ? "" : "→ " + target + ": ") + said + when) | ftxui::color(this->_theme.text));
            }
            if (voice.speaking()) row.push_back(ftxui::text("  speaking (" + this->_manager.config().keys.at("mute") + " mute)") | ftxui::color(this->_theme.muted));
            return ftxui::hbox(std::move(row)) | ftxui::bgcolor(this->_theme.surface);
        }

        _nodiscard ftxui::Element body_(const int width, const int height)
        {
            const std::vector<cluster::Snapshot> list = this->sessions_();
            const bool tasksShown = this->_active == TASKS_ID; // the Tasks session: its view in place of a transcript
            if (list.empty() && tasksShown) return this->tasksView_(width, height); // no session yet: its tasks, its schedules still
            if (list.empty()) return ftxui::text("no session: " + this->_manager.config().keys.at("new_session") + " to start one") | ftxui::center | ftxui::flex;
            auto it = std::find_if(list.begin(), list.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->_active;});
            if (it == list.end()) {
                if (!tasksShown) this->_active = list.front().spec.id;
                it = list.begin();
            }
            const std::string layout = this->_manager.ui("layout", this->_manager.config().layout);
            if (tasksShown && layout == "grid") return this->tasksView_(width, height);

            if (layout == "grid" && list.size() > 1) {
                const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(list.size()))));
                const int rows = (static_cast<int>(list.size()) + columns - 1) / columns;
                const int cellWidth = width / columns - 2;
                const int cellHeight = height / rows - 2;
                std::vector<ftxui::Elements> grid;
                for (std::size_t i = 0; i < list.size(); ++i) {
                    if (i % static_cast<std::size_t>(columns) == 0) grid.emplace_back();
                    const bool active = list[i].spec.id == this->_active;
                    const std::string id = list[i].spec.id;
                    // clicked: shown (the one shown: its text selected as usual)
                    grid.back().push_back(this->click_(this->session_(list[i], cellWidth, cellHeight, true) | ftxui::borderStyled(active ? ftxui::HEAVY : ftxui::ROUNDED)
                        | ftxui::color(active ? this->_theme.accent : this->_theme.border), 0, [this, id]() {
                            if (this->_active == id) return false;
                            this->_active = id;
                            return true;
                        }) | ftxui::flex);
                }
                while (grid.back().size() < static_cast<std::size_t>(columns)) grid.back().push_back(ftxui::filler());
                return ftxui::gridbox(grid) | ftxui::flex;
            }
            if (layout == "tabs") {
                ftxui::Elements tabs;
                const ftxui::Element first = ftxui::text(" ☰ Tasks ");
                tabs.push_back(this->tap_(tasksShown ? first | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) : first | ftxui::color(this->_theme.muted),
                    0, [this]() {this->_active = TASKS_ID;}));
                for (const cluster::Snapshot& s: list) {
                    const bool active = s.spec.id == this->_active;
                    ftxui::Element tab = ftxui::text(" " + state_mark_(s.state) + " " + cluster::one_line(s.spec.name, 14) + (s.permissions.empty() ? "" : " !") + " ");
                    const std::string id = s.spec.id;
                    this->_rowSessions[static_cast<int>(this->_clicks.size())] = id;
                    tabs.push_back(this->tap_(active ? tab | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) : tab | ftxui::color(this->_theme.muted),
                        0, [this, id]() {this->_active = id;}));
                }
                return ftxui::vbox({ftxui::hbox(std::move(tabs)), (tasksShown ? this->tasksView_(width, height - 1) : this->session_(*it, width, height - 1, false)) | ftxui::flex});
            }
            // list + active panel
            ftxui::Elements rows = {ftxui::text(" Sessions") | ftxui::bold | ftxui::color(this->_theme.accent), ftxui::text("")};
            const int clip = this->clip_();
            rows.push_back(this->tasksRow_(tasksShown, clip)); // the Tasks session first
            // more sessions than the room: the ones around the one shown (2 lines each)
            const std::size_t room = static_cast<std::size_t>(std::max(1, (height - 3) / 2));
            const std::size_t at = static_cast<std::size_t>(it - list.begin());
            const std::size_t first = list.size() <= room || at < room ? 0 : std::min(at + 1 - room, list.size() - room);
            if (first > 0) rows.push_back(ftxui::text(" ↑ " + std::to_string(first) + " more") | ftxui::color(this->_theme.muted) | ftxui::dim);
            for (std::size_t i = first; i < list.size() && i < first + room; ++i)
                rows.push_back(this->sessionRow_(list[i], list[i].spec.id == this->_active, clip));
            if (first + room < list.size()) rows.push_back(ftxui::text(" ↓ " + std::to_string(list.size() - first - room) + " more") | ftxui::color(this->_theme.muted) | ftxui::dim);
            return ftxui::hbox({
                ftxui::vbox(std::move(rows)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 30) | ftxui::yframe | this->clipBox_(clip),
                ftxui::separatorLight() | ftxui::color(this->_theme.border),
                (tasksShown ? this->tasksView_(width - 31, height) : this->session_(*it, width - 31, height, false)) | ftxui::flex,
            });
        }

        _nodiscard ftxui::Element modal_(void)
        {
            const std::function<ftxui::Element(ftxui::Element, const int)> box = [&](ftxui::Element content, const int width) {
                return this->block_(content | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width) | ftxui::borderRounded | ftxui::color(this->_theme.accent)
                    | ftxui::bgcolor(this->_theme.surface) | ftxui::clear_under, 1) | ftxui::center;
            };
            // a key of the dialog clicked: the same as typed
            const std::function<ftxui::Element(ftxui::Element, const ftxui::Event&)> press = [&](ftxui::Element element, const ftxui::Event& key) {
                return this->tap_(std::move(element), 1, [this, key]() {(void)this->event_(key);});
            };
            if (this->_mode == Mode::Palette) {
                ftxui::Elements items;
                const int start = std::max(0, this->_selected - 12);
                for (int i = start; i < static_cast<int>(this->_actions.size()) && i < start + 24; ++i) {
                    ftxui::Element item = ftxui::text(" " + this->_actions[static_cast<std::size_t>(i)].title + " ");
                    items.push_back(this->tap_(i == this->_selected ? item | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent)
                        : item | ftxui::color(this->_theme.text), 1, [this, i]() {
                            this->_selected = i;
                            this->choose_();
                        }));
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
                    items.push_back(this->tap_(i == this->_selected ? item | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent)
                        : item | ftxui::color(this->_theme.text), 1, [this, i]() {
                            this->_selected = i;
                            (void)this->event_(ftxui::Event::Return);
                        }));
                }
                if (items.empty()) items.push_back(ftxui::text(this->_historyAll ? " no prompt yet" : " no prompt in this session yet (Tab: every session)") | ftxui::color(this->_theme.muted));
                const std::string scope = this->_historyAll ? " history (every session) > " : " history (" + this->_active + ") > ";
                return box(ftxui::vbox({ftxui::hbox({ftxui::text(scope) | ftxui::color(this->_theme.accent), ftxui::text(this->_filter), ftxui::text("▏") | ftxui::blink}),
                    ftxui::separatorLight(), ftxui::vbox(std::move(items)), ftxui::text(std::string(" Enter: put in the prompt · Tab: ")
                    + (this->_historyAll ? "this session only" : "every session") + " · Esc: close") | ftxui::dim}), 120);
            }
            if (this->_mode == Mode::Prompt) {
                const std::string shown = this->_password ? std::string(width_(this->_prompt), '*') : this->_prompt;
                return box(ftxui::vbox({ftxui::text(" " + this->_pending.title) | ftxui::bold, ftxui::text(" " + this->_pending.input + ":") | ftxui::color(this->_theme.muted),
                    ftxui::hbox({ftxui::text(" > ") | ftxui::color(this->_theme.accent), ftxui::text(shown), ftxui::text("▏") | ftxui::blink}),
                    ftxui::hbox({press(ftxui::text(" Enter: run"), ftxui::Event::Return), ftxui::text(" · "), press(ftxui::text("Escape: cancel"), ftxui::Event::Escape)}) | ftxui::dim}), 80);
            }
            if (this->_mode == Mode::Message) {
                ftxui::Elements lines;
                for (const std::string& line: cluster::split(this->_message, '\n'))
                    lines.push_back(ftxui::text(" " + line) | ftxui::color(this->_messageError ? this->_theme.error : this->_theme.text));
                lines.push_back(ftxui::text(" Escape / Enter: close") | ftxui::dim);
                return box(ftxui::vbox(std::move(lines)), 100);
            }
            if (this->_mode == Mode::Machine) {
                const bool old = this->_machineStartup && this->_machineKind == 2;
                const bool forced = this->_machineStartup && this->_machineKind == 3; // --rescan
                const std::string title = old ? " The map of this computer is " + std::to_string(cluster::machine_days_old()) + " days old: sync it?"
                    : forced && cluster::machine_days_old() >= 0 ? " Sync the map of this computer? (last " + cluster::machine_age() + ")" : " Map this computer?";
                const std::string no = !this->_machineStartup || forced ? "" : old ? " (proposed again in " + std::to_string(this->_manager.config().machineDays) + " days)"
                    : " (not asked again: settings > Global session)";
                return box(ftxui::vbox({ftxui::text(title) | ftxui::bold, ftxui::text(""),
                    ftxui::paragraph(cluster::machine_scope()) | ftxui::color(this->_theme.text), ftxui::text(""),
                    ftxui::hbox({press(ftxui::text(" [y] yes"), ftxui::Event::Character('y')), ftxui::text("   "), press(ftxui::text("[n] / Esc no" + no), ftxui::Event::Character('n'))})
                        | ftxui::color(this->_theme.accent),
                    this->_machineStartup ? ftxui::text(" [q] / Ctrl+C / " + this->_manager.config().keys.at("quit") + "  quit (asked again at the next start)")
                        | ftxui::color(this->_theme.muted) : ftxui::emptyElement()}), 90);
            }
            if (this->_mode == Mode::Profile && this->_question < this->_questions.size()) {
                // one question about the user at a time, its suggestion ready (Enter keeps it)
                const cluster::ProfileQuestion& q = this->_questions[this->_question];
                ftxui::Elements lines = {ftxui::text(" About you (for the global session) - " + std::to_string(this->_question + 1) + "/" + std::to_string(this->_questions.size())) | ftxui::bold,
                    ftxui::text(""), ftxui::text(" " + q.question) | ftxui::color(this->_theme.text),
                    ftxui::hbox({ftxui::text(" › ") | ftxui::color(this->_theme.accent), ftxui::text(this->_answer + "▏") | ftxui::color(this->_theme.text) | ftxui::bold})};
                if (!q.choices.empty()) {
                    ftxui::Elements choices = {ftxui::text(" ← → ") | ftxui::color(this->_theme.muted)};
                    for (const std::string& choice: q.choices)
                        choices.push_back(this->tap_(ftxui::text(" " + (choice.empty() ? std::string("(none)") : choice) + " ") | (choice == this->_answer
                            ? ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) : ftxui::color(this->_theme.text)), 1, [this, choice]() {
                                if (this->_answer == choice) (void)this->event_(ftxui::Event::Return); // clicked again: ok
                                else this->_answer = choice;
                            }));
                    lines.push_back(ftxui::hbox(std::move(choices)));
                }
                lines.push_back(ftxui::text(""));
                lines.push_back(ftxui::hbox({press(ftxui::text(" Enter: ok"), ftxui::Event::Return), ftxui::text(" · "), press(ftxui::text("Esc: skip it"), ftxui::Event::Escape),
                    ftxui::text(" · Ctrl+S: skip the rest (config.toml [user], F9 > User)")}) | ftxui::color(this->_theme.muted) | ftxui::dim);
                return box(ftxui::vbox(std::move(lines)), 90);
            }
            const std::optional<cluster::Ask> asked = this->_manager.asking().current();
            if (this->_mode == Mode::Question && asked) {
                // like the questions of Claude Code: one at a time, its options, other (typed); the keys, the mouse and the voice
                const cluster::Ask& a = *asked;
                const cluster::Json& q = a.questions[a.index];
                const cluster::Json options = q.value("options", cluster::Json::array());
                const bool multi = q.value("multiSelect", false);
                ftxui::Elements lines = {ftxui::hbox({ftxui::text(" " + a.name + " asks") | ftxui::bold,
                    ftxui::text(a.questions.size() > 1 ? "  (" + std::to_string(a.index + 1) + "/" + std::to_string(a.questions.size()) + ")" : "") | ftxui::color(this->_theme.muted),
                    ftxui::text(a.session != this->_active ? "  (it waits for your answer)" : "") | ftxui::color(this->_theme.warn),
                    ftxui::filler(), ftxui::text(q.value("header", std::string()) + " ") | ftxui::color(this->_theme.accent)}), ftxui::text(""),
                    ftxui::paragraph(" " + q.value("question", std::string())) | ftxui::color(this->_theme.text), ftxui::text("")};
                for (int i = 0; i <= static_cast<int>(options.size()); ++i) {
                    const bool other = i == static_cast<int>(options.size());
                    const std::string mark = multi ? (a.picked.contains(i) || (other && !a.other.empty()) ? "[x] " : "[ ] ") : (i == a.cursor ? "› " : "  ");
                    const std::string label = other ? "Other: " + a.other + (i == a.cursor ? "▏" : "") : options[static_cast<std::size_t>(i)].value("label", std::string());
                    const std::string description = other ? "" : options[static_cast<std::size_t>(i)].value("description", std::string());
                    ftxui::Element row = ftxui::hbox({ftxui::text(" " + mark + std::to_string(i + 1) + ". " + label + " "),
                        ftxui::text(description.empty() ? "" : "- " + cluster::one_line(description, 70)) | ftxui::color(this->_theme.muted)});
                    const int count = static_cast<int>(options.size());
                    // clicked: chosen (one choice: answered; several: ticked; Other: typed on)
                    lines.push_back(this->tap_(i == a.cursor ? row | ftxui::bold | ftxui::bgcolor(this->_theme.border) : row | ftxui::color(this->_theme.text), 1,
                        [this, i, count, multi]() {
                            this->_manager.asking().choose(i);
                            if (!multi && i < count) this->askNext_();
                        }));
                }
                lines.push_back(ftxui::text(""));
                lines.push_back(ftxui::hbox({ftxui::text(std::string(" ↑↓ / 1-9 choose · ") + (multi ? "Space tick · " : "")),
                    press(ftxui::text(std::string("Enter ") + (a.index + 1 < a.questions.size() ? "next" : "answer")), ftxui::Event::Return),
                    a.index > 0 ? press(ftxui::text(" · ← back"), ftxui::Event::ArrowLeft) : ftxui::emptyElement(), ftxui::text(" · type on Other · "),
                    press(ftxui::text("Esc decline"), ftxui::Event::Escape)}) | ftxui::color(this->_theme.muted) | ftxui::dim);
                // "Chat about this": a side chat about the questions, in the popup (the session untouched)
                for (const auto &[role, said]: a.chat)
                    lines.push_back(ftxui::paragraph((role == "user" ? " you › " : " ✻ ") + said) | ftxui::color(role == "user" ? this->_theme.text : this->_theme.accent));
                if (a.chatting) lines.push_back(ftxui::text(" ✻ …") | ftxui::color(this->_theme.muted));
                if (this->_askChat)
                    lines.push_back(ftxui::hbox({ftxui::text(" chat › ") | ftxui::color(this->_theme.accent) | ftxui::bold, ftxui::text(this->_askDraft),
                        ftxui::text("▏") | ftxui::blink, ftxui::filler(), ftxui::text("Enter send · Esc back ") | ftxui::color(this->_theme.muted) | ftxui::dim}));
                else
                    lines.push_back(this->tap_(ftxui::text(" Ctrl+T: chat about this (the session untouched)") | ftxui::color(this->_theme.accent), 0,
                        [this]() {this->_askChat = true;}));
                if (a.session != GLOBAL_ID)
                    lines.push_back(this->tap_(ftxui::text(" " + this->_manager.config().keys.at("global") + ": the global session answers them all for you")
                        | ftxui::color(this->_theme.accent), 0, [this]() {
                            const std::string done = this->_manager.asking().toGlobal();
                            this->status_(done, done != "the global session answers it");
                            this->_mode = Mode::Normal;
                        }));
                if (this->_manager.config().voice.enabled)
                    lines.push_back(ftxui::text(" voice: a number or an option · valide / suivant · retour · autre <answer> · la globale · annule") | ftxui::color(this->_theme.muted) | ftxui::dim);
                return box(ftxui::vbox(std::move(lines)), 96);
            }
            if (this->_mode == Mode::Normal && asked && asked->session == this->_active && !this->askShown_(*asked)) {
                const cluster::Json& q = asked->questions[asked->index];
                return box(this->tap_(ftxui::vbox({ftxui::text(" " + asked->name + " waits for your answer") | ftxui::bold, ftxui::text(""),
                    ftxui::paragraph(" " + q.value("question", std::string())) | ftxui::color(this->_theme.muted), ftxui::text(""),
                    ftxui::text(" its question is on the global session: " + this->_manager.config().keys.at("global") + " (or a click) to answer it") | ftxui::color(this->_theme.warn)})
                    | ftxui::dim, 0, [this]() {(void)this->shortcut_("global");}), 80);
            }
            const std::optional<cluster::WebStatus> webNow = this->_mode == Mode::WebApprove ? cluster::web_status(this->_manager) : std::nullopt;
            if (this->_mode == Mode::WebApprove && webNow) {
                // who it is: its name, its address, its browser, since when
                const cluster::WebStatus& status = *webNow;
                auto it = std::find_if(status.devices.begin(), status.devices.end(), [this](const cluster::WebDevice& d) {return d.id == this->_webAsk;});
                if (it == status.devices.end() || it->state != "waiting") {
                    this->_mode = Mode::Normal;
                } else {
                    const cluster::WebDevice& d = *it;
                    return box(ftxui::vbox({ftxui::text(" A device asks to connect to the web access") | ftxui::bold, ftxui::text(""),
                        ftxui::text("   name     " + d.name) | ftxui::color(this->_theme.text), ftxui::text("   address  " + d.address) | ftxui::color(this->_theme.text),
                        ftxui::paragraph("   browser  " + cluster::one_line(d.agent, 160)) | ftxui::color(this->_theme.muted),
                        ftxui::text("   asked    " + cluster::human_age(d.since / 1000) + " ago (its code was right)") | ftxui::color(this->_theme.muted), ftxui::text(""),
                        ftxui::hbox({press(ftxui::text(" [a] Allow ") | ftxui::color(this->_theme.accent), ftxui::Event::Character('a')), ftxui::text("  "),
                            press(ftxui::text(" [t] Allow and trust ") | ftxui::color(this->_theme.ok), ftxui::Event::Character('t')), ftxui::text("  "),
                            press(ftxui::text(" [d] Deny ") | ftxui::color(this->_theme.error), ftxui::Event::Character('d')), ftxui::text("  "),
                            press(ftxui::text(" Esc later ") | ftxui::color(this->_theme.muted), ftxui::Event::Escape)}),
                        ftxui::text(" trusted: no code, no approval next time (palette: Web devices, to remove it)") | ftxui::color(this->_theme.muted) | ftxui::dim}), 90);
                }
            }
            if (this->_mode == Mode::WebDevices) {
                // each device: ticked by Space or a click; kicked, its trust removed
                const std::optional<cluster::WebStatus> web = cluster::web_status(this->_manager);
                ftxui::Elements lines = {ftxui::hbox({ftxui::text(" Web devices") | ftxui::bold, ftxui::filler(),
                    ftxui::text(!web ? "no web access " : !web->on ? "closed " : web->url + " · code " + web->pin + (web->ping >= 0 ? " · ping " + std::to_string(web->ping) + " ms " : " "))
                    | ftxui::color(this->_theme.accent)}), ftxui::text("")};
                const std::vector<std::pair<std::string, cluster::WebDevice>> rows = this->webRows_();
                if (rows.empty()) lines.push_back(ftxui::text("   nobody connected, no trusted or banned device") | ftxui::color(this->_theme.muted));
                std::string title;
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    const auto &[key, d] = rows[i];
                    const std::string section = key.substr(0, 2) == "t:" ? " trusted (no code, no approval)" : key.substr(0, 2) == "b:" ? " banned (refused, its code too)" : " connected / waiting";
                    if (section != title) {
                        lines.push_back(ftxui::text(section) | ftxui::bold | ftxui::color(key.starts_with("b:") ? this->_theme.error : this->_theme.muted));
                        title = section;
                    }
                    const bool picked = this->_webPicked.contains(key);
                    const std::string state = d.state == "trusted" ? "last " + (d.seen > 0 ? cluster::human_age(d.seen / 1000) + " ago" : std::string("never"))
                        : d.state == "banned" ? "since " + cluster::human_age(d.since / 1000) + " ago"
                        : d.state + (d.trusted ? " · trusted" : "");
                    const std::string ping = d.state != "connected" ? "" : d.ping < 0 ? "  ping …" : "  ping " + std::to_string(d.ping) + " ms";
                    ftxui::Element row = ftxui::hbox({ftxui::text(std::string(picked ? " [x] " : " [ ] ") + d.name + "  "), ftxui::text(d.address + "  ") | ftxui::color(this->_theme.muted),
                        ftxui::text(state) | ftxui::color(d.state == "connected" ? this->_theme.ok : d.state == "waiting" ? this->_theme.warn : this->_theme.muted),
                        ftxui::text(ping + " ") | ftxui::color(d.ping > 300 ? this->_theme.warn : this->_theme.accent), ftxui::filler()});
                    const int index = static_cast<int>(i);
                    lines.push_back(this->tap_(index == this->_webCursor ? row | ftxui::bold | ftxui::bgcolor(this->_theme.border) : row | ftxui::color(this->_theme.text), 1,
                        [this, index, key]() {
                            this->_webCursor = index;
                            if (!this->_webPicked.erase(key)) this->_webPicked.insert(key);
                        }));
                }
                lines.push_back(ftxui::text(""));
                if (!this->_webConfirm.empty()) {
                    // a dangerous action: its y first
                    lines.push_back(ftxui::hbox({ftxui::text(this->_webConfirm == "ban" ? " Ban the device(s)? Refused from now, their code too, until unbanned  "
                        : " Kick every device? A new code too  ") | ftxui::bold | ftxui::color(this->_theme.error),
                        press(ftxui::text(" [y] yes ") | ftxui::color(this->_theme.error), ftxui::Event::Character('y')), ftxui::text(" "),
                        press(ftxui::text(" [n] / Esc no ") | ftxui::color(this->_theme.muted), ftxui::Event::Character('n'))}));
                } else {
                    lines.push_back(ftxui::hbox({press(ftxui::text(" [k] kick the ticked ") | ftxui::color(this->_theme.accent), ftxui::Event::Character('k')), ftxui::text(" "),
                        press(ftxui::text(" [K] kick all ") | ftxui::color(this->_theme.error), ftxui::Event::Character('K')), ftxui::text(" "),
                        press(ftxui::text(" [b] ban ") | ftxui::color(this->_theme.error), ftxui::Event::Character('b')), ftxui::text(" "),
                        press(ftxui::text(" [n] unban ") | ftxui::color(this->_theme.ok), ftxui::Event::Character('n')), ftxui::text(" "),
                        press(ftxui::text(" [u] trust no more ") | ftxui::color(this->_theme.warn), ftxui::Event::Character('u')), ftxui::text(" "),
                        press(ftxui::text(" [a/t/d] a waiting one ") | ftxui::color(this->_theme.muted), ftxui::Event::Character('a')), ftxui::text(" "),
                        press(ftxui::text(" Esc ") | ftxui::color(this->_theme.muted), ftxui::Event::Escape)}));
                }
                lines.push_back(ftxui::text(" ↑↓ · Space / a click: tick · none ticked: the one under the cursor · ping: the round trip of its page") | ftxui::color(this->_theme.muted) | ftxui::dim);
                return box(ftxui::vbox(std::move(lines)), 100);
            }
            if (this->_mode == Mode::Restore) {
                const std::vector<cluster::SessionSpec> previous = this->_manager.previous();
                ftxui::Elements lines = {ftxui::text(" Restore the previous run?") | ftxui::bold, ftxui::text("")};
                for (const cluster::SessionSpec& spec: previous)
                    lines.push_back(ftxui::text("   " + spec.name + "  " + spec.backend + "  " + cluster::short_path(spec.cwd)) | ftxui::color(this->_theme.muted));
                lines.push_back(ftxui::text("   global session: its conversation") | ftxui::color(this->_theme.muted));
                lines.push_back(ftxui::text(""));
                lines.push_back(press(ftxui::text(" [r] / Enter / Esc  restore") | ftxui::color(this->_theme.accent), ftxui::Event::Character('r')));
                lines.push_back(press(ftxui::text(" [n]  start new: the sessions to the trash (" + std::to_string(this->_manager.config().trashDays)
                    + " days), a new global conversation") | ftxui::color(this->_theme.accent), ftxui::Event::Character('n')));
                lines.push_back(ftxui::text(" [q] / Ctrl+C / " + this->_manager.config().keys.at("quit") + "  quit (asked again at the next start)") | ftxui::color(this->_theme.muted));
                return box(ftxui::vbox(std::move(lines)), 90);
            }
            return ftxui::emptyElement();
        }

        _nodiscard bool askShown_(const cluster::Ask& ask) const
        {
            // the question of a sub-session: on the global session ([sessions] questions_to_global), else on its own one
            if (this->_manager.config().questionsToGlobal) return this->_active == GLOBAL_ID || ask.session == GLOBAL_ID;
            return this->_active == ask.session;
        }

        void askCheck_(void)
        {
            // the question waiting (any session): its popup where it is shown; answered elsewhere (voice, MCP, web): closed
            cluster::Asking& asking = this->_manager.asking();
            (void)asking.check();
            const std::optional<cluster::Ask> ask = asking.current();
            const bool show = ask && this->askShown_(*ask);
            if (this->_mode == Mode::Question && !show) this->_mode = Mode::Normal;
            if (this->_mode != Mode::Normal) return;
            if (this->_manager.machineRequest() != this->_machineSeen) { // the global session asked to map this computer again
                this->_machineSeen = this->_manager.machineRequest();
                this->settingsAction_("machine_sync");
                return;
            }
            if (show) {
                this->_mode = Mode::Question;
                return;
            }
            // a device of the web access asks to connect: its popup (once; Esc: later, the palette)
            if (const std::optional<cluster::WebStatus> web = cluster::web_status(this->_manager))
                for (const cluster::WebDevice& d: web->devices)
                    if (d.state == "waiting" && !this->_webLater.contains(d.id)) {
                        this->_webAsk = d.id;
                        this->_mode = Mode::WebApprove;
                        return;
                    }
        }

        _nodiscard std::vector<std::pair<std::string, cluster::WebDevice>> webRows_(void) const
        {
            // the devices waiting and connected ("d:<id>"), the trusted ones ("t:<id>"), the banned ones ("b:<id>")
            std::vector<std::pair<std::string, cluster::WebDevice>> rows;
            const std::optional<cluster::WebStatus> status = cluster::web_status(this->_manager);
            if (!status) return rows;
            for (const cluster::WebDevice& d: status->devices)
                rows.emplace_back("d:" + d.id, d);
            for (const cluster::WebDevice& d: status->trusted)
                rows.emplace_back("t:" + d.id, d);
            for (const cluster::WebDevice& d: status->banned)
                rows.emplace_back("b:" + d.id, d);
            return rows;
        }

        void webDo_(const std::string& what)
        {
            // kick (the ticked ones, else the one under the cursor), all, untrust, ban / unban, a waiting one answered
            const std::vector<std::pair<std::string, cluster::WebDevice>> rows = this->webRows_();
            std::vector<std::string> devices;
            std::vector<std::string> trusted;
            std::vector<std::string> banned;
            const std::function<void(const std::string&, const std::string&)> put = [&](const std::string& key, const std::string& id) {
                (key.starts_with("t:") ? trusted : key.starts_with("b:") ? banned : devices).push_back(id);
            };
            for (const auto &[key, d]: rows)
                if (this->_webPicked.contains(key)) put(key, d.id);
            if (devices.empty() && trusted.empty() && banned.empty() && this->_webCursor < static_cast<int>(rows.size()))
                put(rows[static_cast<std::size_t>(this->_webCursor)].first, rows[static_cast<std::size_t>(this->_webCursor)].second.id);
            try {
                if (what == "kick_all") {
                    cluster::web_act(this->_manager, "kick");
                    this->status_("web: every device disconnected, a new code");
                } else if (what == "kick" && !devices.empty()) {
                    cluster::web_act(this->_manager, "kick", devices);
                    this->status_("web: " + std::to_string(devices.size()) + " disconnected");
                } else if (what == "untrust") {
                    std::vector<std::string> ids = trusted;
                    if (ids.empty()) ids = devices; // a connected trusted device: its trust
                    cluster::web_act(this->_manager, "untrust", ids);
                    this->status_("web: trusted no more");
                } else if (what == "ban") {
                    std::vector<std::string> ids = devices;
                    ids.insert(ids.end(), trusted.begin(), trusted.end());
                    if (!ids.empty()) cluster::web_act(this->_manager, "ban", ids);
                    this->status_("web: " + std::to_string(ids.size()) + " banned (refused from now: [n] unbans)");
                } else if (what == "unban" && !banned.empty()) {
                    cluster::web_act(this->_manager, "unban", banned);
                    this->status_("web: " + std::to_string(banned.size()) + " unbanned");
                } else if (what == "allow" || what == "trust" || what == "deny") {
                    for (const auto &[key, d]: rows)
                        if (d.state == "waiting" && (std::find(devices.begin(), devices.end(), d.id) != devices.end())) cluster::web_act(this->_manager, "approve", {}, d.id, what);
                }
                this->_webPicked.clear();
            } catch (const utils::exception::IException& e) {
                this->status_(e.info(), true);
            }
            this->_webPicked.clear();
        }

        void askNext_(void)
        {
            // the answer recorded: the next question, or all sent
            const std::string why = this->_manager.asking().next();
            if (!why.empty()) this->status_(why, why != "type the answer (other), or choose an option");
            else if (!this->_manager.asking().current()) this->status_("answered");
        }

        _nodiscard ftxui::Element render_(void)
        {
            if (this->_manager.config().reload()) this->theme_();
            this->askCheck_();
            this->_clicks.clear();  // made again by this drawing
            this->_rowSessions.clear();
            // the face of the app (>_< running, •_• waiting on you...): in the bar, the title of the terminal, the dock
            const cluster::AppFace face = this->_manager.appFace(this->_app.voice->listening());
            const std::string title = std::string("(") + cluster::face_text(face.face) + ") " + (face.badge > 0 ? std::to_string(face.badge) + " · " : "") + "claude-cluster";
            if (title != this->_title) {
                this->_title = title;
                std::cout << "\x1b]0;" << title << "\x07" << std::flush;
            }
            cluster::dock_badge(face.badge, face.urgent);
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
            const std::optional<cluster::WebStatus> webState = cluster::web_status(this->_manager);
            const std::string web = webState ? cluster::web_line(*webState) : "";

            // its keys clicked too
            const std::function<ftxui::Element(const std::string&, const std::string&)> key = [&](const std::string& label, const std::string& action) {
                return this->tap_(ftxui::text(label) | ftxui::color(this->_theme.muted), 0, [this, action]() {(void)this->shortcut_(action);});
            };
            const std::function<ftxui::Element(void)> dot = [this]() {return ftxui::text(" · ") | ftxui::color(this->_theme.muted);}; // new each time: drawn once
            ftxui::Element top = ftxui::hbox({
                this->tap_(ftxui::text(std::string(" ") + cluster::face_text(face.face) + " claude-cluster ") | ftxui::bold | ftxui::color(this->_theme.bg)
                    | ftxui::bgcolor(face.face == cluster::Face::Waiting ? this->_theme.warn : this->_theme.accent), 0,
                    [this]() {(void)this->shortcut_("settings");}),
                key(" " + layout, "layout"),
                ftxui::text(" · " + this->_manager.ui("style", c.style) + (this->_manager.remote() ? " · remote" : "")) | ftxui::color(this->_theme.muted),
                ftxui::filler(),
                key(c.keys.at("palette") + " palette", "palette"), dot(), key(c.keys.at("settings") + " settings", "settings"), dot(),
                key(c.keys.at("next_session") + "/" + c.keys.at("prev_session") + " session", "next_session"), dot(), key(c.keys.at("global") + " global", "global"), dot(),
                key(c.keys.at("new_session") + " new", "new_session"), dot(), ftxui::text(c.keys.at("quit") + " quit ") | ftxui::color(this->_theme.muted),
            });
            // the web access under it: its state (● someone connected, ○ open, nobody), its address, its code, the mean ping; a click: the
            // code copied; devices: their list
            const bool webUsed = !web.empty() && std::any_of(webState->devices.begin(), webState->devices.end(), [](const cluster::WebDevice& d) {return d.state == "connected";});
            ftxui::Element webRow = web.empty() ? ftxui::emptyElement() : ftxui::hbox({
                ftxui::text(webState->on ? (webUsed ? " ● " : " ○ ") : " ✕ ") | ftxui::bold | ftxui::color(!webState->on ? this->_theme.error : webUsed ? this->_theme.ok : this->_theme.accent),
                this->tap_(ftxui::text(web + " ") | ftxui::bold | ftxui::color(this->_theme.accent), 0, [this, webState]() {
                    // its code copied (ready to paste in the page): the clipboard of the desktop, the terminal's (OSC 52: over ssh too)
                    std::string digits;
                    for (const char c: webState->pin)
                        if (c >= '0' && c <= '9') digits += c;
                    const bool desktop = cluster::copy_text(digits);
                    const std::string osc = "\x1b]52;c;" + utils::smanip::codec::Base64Codec().encode(digits) + "\a";
                    (void)!::write(STDOUT_FILENO, osc.data(), osc.size());
                    this->status_("web: code " + webState->pin + " copied" + (desktop ? "" : " (terminal clipboard)"));
                }),
                !webState->on ? ftxui::emptyElement() : this->tap_(ftxui::text(" devices › ") | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent), 0,
                    [this]() {
                        this->_webPicked.clear();
                        this->_webCursor = 0;
                        this->_webConfirm.clear();
                        this->_mode = Mode::WebDevices;
                    }),
            });
            // the global session asked to show a session (ui_focus); the style may have changed (ui_style)
            if (this->_manager.raiseRequest() != this->_raiseSeen) { // a notification clicked: the bell marks this terminal
                this->_raiseSeen = this->_manager.raiseRequest();
                std::cout << "\a" << std::flush;
            }
            const std::pair<std::uint64_t, std::string> focus = this->_manager.focusRequest();
            if (focus.first != this->_focusSeen) {
                this->_focusSeen = focus.first;
                if (this->_manager.snapshot(focus.second)) this->_active = focus.second;
            }
            this->_manager.noteActive(this->_active);
            if (this->_manager.ui("style", this->_manager.config().style) != this->_styleSeen) {
                this->_styleSeen = this->_manager.ui("style", this->_manager.config().style);
                this->theme_();
            }
            this->_views.clear(); // filled again by the transcripts drawn
            this->_animating = false;
            this->_reveal.frameStart();
            const int bodyHeight = size.dimy - 4 - (this->_app.voice->listening() || !this->_app.voice->pending().empty() ? 1 : 0);
            ftxui::Element input = this->prompt_(size.dimx);
            // the bottom line: the status / the last notice on the left, cut to the room left by the web access on the right
            const int webWidth = web.empty() ? 0 : ftxui::string_width(web) + 4 + (webState->on ? 11 : 0);
            const std::size_t room = static_cast<std::size_t>(std::max(10, size.dimx - webWidth - 3));
            ftxui::Element bottom = ftxui::hbox({ftxui::text(" " + cluster::one_line(status, room)) | ftxui::color(error ? this->_theme.error : this->_theme.muted),
                ftxui::filler(), webRow});
            ftxui::Element document = ftxui::vbox({
                top,
                this->body_(size.dimx, bodyHeight) | ftxui::flex,
                this->voice_(),
                this->completion_(),
                ftxui::separatorLight() | ftxui::color(this->_theme.border),
                this->images_(),
                input,
                bottom,
            }) | ftxui::color(this->_theme.text) | ftxui::bgcolor(this->_theme.bg);
            if (this->_reveal.active()) this->_animating = true;
            // the box of the scan over any screen (started from the settings, the palette, the question at start); its end
            // in the status line too
            const std::optional<std::pair<cluster::MachineProgress, std::int64_t>> last = this->_manager.machineProgress();
            if (last && last->first.done && last->first.id != this->_scanReported) {
                this->_scanReported = last->first.id;
                this->status_("map of this computer written: " + std::to_string(last->first.repos) + " repositories");
                if (this->_note.starts_with("mapping")) this->_note = this->_status;
            }
            const std::optional<cluster::MachineProgress> scan = this->scanShown_();
            // Normal: the note of a sub-session whose question waits on the global session (modal_, empty else)
            ftxui::Element screen = this->_mode == Mode::Settings ? this->settings_(size) : ftxui::dbox({document, this->modal_()});
            // a long operation: its popup over the screen (its spinner turning)
            if (const std::optional<cluster::Progress> running = this->_manager.progress(); running && running->id != this->_progressHidden) {
                this->_animating = true;
                screen = ftxui::dbox({screen, this->progressBox_(*running)});
            }
            // what failed of what the user did: over everything (OK / Retry)
            if (const std::vector<cluster::Failure> failed = this->_manager.failures(); !failed.empty()) screen = ftxui::dbox({screen, this->failureBox_(failed)});
            if (!scan) return screen;
            this->_animating = true;
            return ftxui::dbox({screen, this->scanBox_(*scan)});
        }

        /* events */
        bool event_(ftxui::Event event)
        {
            if (event == ftxui::Event::Custom) {
                // an Enter with no key right after: a real one, sent
                if (this->_sendPending && now_ms_() - this->_sendAt >= PASTE_GAP_MS) {
                    this->_sendPending = false;
                    this->submit_();
                }
                return true;
            }
            // a failure shown: answered first (Enter / Esc / o: OK, r: Retry); the mouse: its buttons
            if (!event.is_mouse() && event.input() != "\x1b[I" && event.input() != "\x1b[O") // xstyle: ignore LU-ANSI (focus in / out)
                if (const std::vector<cluster::Failure> failed = this->_manager.failures(); !failed.empty()) {
                    if (event == ftxui::Event::Return || event == ftxui::Event::Escape || event == ftxui::Event::Character('o') || event == ftxui::Event::Character(' '))
                        this->_manager.failureAnswer(failed.front().id, false);
                    else if ((event == ftxui::Event::Character('r') || event == ftxui::Event::Character('R')) && failed.front().retry)
                        this->_manager.failureAnswer(failed.front().id, true);
                    return true;
                }
            // focus reporting of the terminal (?1004): in / out
            if (event.input() == "\x1b[I" || event.input() == "\x1b[O") { // xstyle: ignore LU-ANSI (input of the terminal, no color)
                this->_manager.setFocused(event.input() == "\x1b[I"); // xstyle: ignore LU-ANSI (focus in)
                return true;
            }
            if (event == ftxui::Event::Escape) // a long operation: its popup hidden (it goes on)
                if (const std::optional<cluster::Progress> running = this->_manager.progress(); running && running->id != this->_progressHidden) {
                    this->_progressHidden = running->id;
                    return true;
                }
            if (event == ftxui::Event::Escape && this->_mode != Mode::Machine && this->_mode != Mode::Profile && this->scanShown_()) {
                this->_scanHidden = this->scanShown_()->id; // the box of the scan closed (it goes on), on any screen
                return true;
            }
            // terminal paste (bracketed): one piece in the prompt, Enter of the text is not a send
            if (event.input() == "\x1b[200~") { // xstyle: ignore LU-ANSI (read, not written)
                this->_pasting = std::string();
                return true;
            }
            if (this->_pasting) {
                if (event.input() != "\x1b[201~") { // xstyle: ignore LU-ANSI (read, not written)
                    *this->_pasting += event.input() == "\r" ? std::string("\n") : event.input();
                    return true;
                }
                std::string text = std::move(*this->_pasting);
                this->_pasting.reset();
                if (this->_mode == Mode::Normal) {
                    this->vimSync_();
                    this->_editor.paste(text);
                } else {
                    for (const char c: text) // the other inputs (palette, search, settings): one line
                        if (c != '\n') (void)this->event_(ftxui::Event::Character(c));
                }
                return true;
            }
            if (event.is_character() || event == ftxui::Event::Return) {
                // a paste without its brackets (the mode lost): keys closer than any typing, the Enter waiting is a line
                const std::int64_t now = now_ms_();
                if (this->_sendPending) {
                    this->_sendPending = false;
                    if (now - this->_sendAt < PASTE_GAP_MS) this->_editor.insert("\n");
                    else this->submit_();
                }
                this->_keyBefore = this->_lastKey;
                this->_lastKey = now;
            }
            if (event.is_mouse()) {
                const ftxui::Mouse& mouse = event.mouse();
                this->_hover = this->hoverAt_(mouse);
                this->_hoverChip = -1;
                for (std::size_t i = 0; i < this->_chipBoxes.size(); ++i)
                    if (this->_chipBoxes[i].Contain(mouse.x, mouse.y)) this->_hoverChip = static_cast<int>(i);
                const bool list = this->_mode == Mode::Settings || this->_mode == Mode::Palette || this->_mode == Mode::History || this->_mode == Mode::Question;
                if (mouse.button == ftxui::Mouse::WheelUp) {
                    if (list) return this->event_(ftxui::Event::ArrowUp);   // the item above
                    this->_scroll[this->_active] += 3;
                    return true;
                }
                if (mouse.button == ftxui::Mouse::WheelDown) {
                    if (list) return this->event_(ftxui::Event::ArrowDown);
                    this->_scroll[this->_active] = std::max(0, this->_scroll[this->_active] - 3);
                    return true;
                }
                if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Right && this->_mode == Mode::Normal) {
                    // a session right-clicked: its menu (open, rename, archive, delete for good...)
                    if (auto it = this->_rowSessions.find(this->hoverAt_(mouse)); it != this->_rowSessions.end()) {
                        this->_active = it->second;
                        this->pick_("menu:" + it->second);
                        return true;
                    }
                }
                if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Left && this->clickAt_(mouse)) {
                    this->_selection.reset();
                    return true;
                }
                if (this->_mode != Mode::Normal) return true;
                if (mouse.motion == ftxui::Mouse::Pressed && this->_hoverChip >= 0 && this->_hoverChip < static_cast<int>(this->_chips.size())) {
                    // a chip: click puts it back in the prompt, right click opens the image (viewer of the desktop)
                    const std::size_t index = this->_chips[static_cast<std::size_t>(this->_hoverChip)];
                    const cluster::Atom& atom = this->_editor.atom(index);
                    if (mouse.button == ftxui::Mouse::Right && !atom.image.empty()) {
                        const std::string viewer = cluster::trim(this->_manager.config().imageViewer);
                        std::vector<std::string> command = viewer.empty() ? std::vector<std::string>{"xdg-open"} : cluster::split(viewer, ' ');
                        command.push_back(atom.image);
                        cluster::detach(command);
                        this->status_("opened " + std::filesystem::path(atom.image).filename().string());
                    } else if (mouse.button == ftxui::Mouse::Left) {
                        this->_editor.insertAtom(index); // an image already there: one more reference (sent once)
                    }
                    return true;
                }
                this->mouseSelect_(mouse);
                return true;
            }
            const std::string key = key_name_(event);
            this->_selection.reset(); // a key: the mouse selection (already copied) goes

            // the quit key works everywhere (a dialog open too); a question at start (restore, map of this computer): q /
            // Ctrl+C / Ctrl+D / Ctrl+Z quit too, without answering (asked again at the next start)
            const bool startup = this->_mode == Mode::Restore || (this->_mode == Mode::Machine && this->_machineStartup);
            if (key == this->_manager.config().keys.at("quit") && this->_mode == Mode::Settings) {
                // the quit key in the settings: the page closed, its changes dropped (not asked, claude-cluster goes on)
                const std::size_t dropped = this->_changes.size();
                this->_changes.clear();
                this->_note.clear();
                this->_finding = false;
                this->_editing = false;
                this->_mode = Mode::Normal;
                this->status_(dropped > 0 ? "settings closed: " + std::to_string(dropped) + " change(s) not saved" : "settings closed");
                return true;
            }
            if (key == this->_manager.config().keys.at("quit") || (startup && (event == ftxui::Event::Character('q') || event == ftxui::Event::Character('Q')
                || key == "Ctrl+C" || key == "Ctrl+D" || key == "Ctrl+Z"))) {
                this->_screen.Exit();
                return true;
            }
            switch (this->_mode) {
                case Mode::Restore:
                    // Esc keeps everything (never a loss by mistake); n: a new start
                    if (event == ftxui::Event::Character('r') || event == ftxui::Event::Character('R') || event == ftxui::Event::Return || event == ftxui::Event::Escape)
                        this->_manager.answerRestore(true);
                    else if (event == ftxui::Event::Character('n') || event == ftxui::Event::Character('N')) this->_manager.answerRestore(false);
                    else return true;
                    this->_mode = this->_machineStartup ? Mode::Machine : Mode::Normal;
                    return true;
                case Mode::Profile: {
                    if (this->_question >= this->_questions.size()) {
                        this->_mode = Mode::Normal;
                        return true;
                    }
                    const cluster::ProfileQuestion& q = this->_questions[this->_question];
                    if (event == ftxui::Event::Return) {
                        this->profileNext_(true);
                    } else if (event == ftxui::Event::Escape) {
                        this->profileNext_(false);
                    } else if (event.input() == std::string(1, '\x13')) { // Ctrl+S: the rest skipped
                        this->_question = this->_questions.size() - 1;
                        this->profileNext_(false);
                    } else if (!q.choices.empty() && (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight)) {
                        auto it = std::find(q.choices.begin(), q.choices.end(), this->_answer);
                        const long at = it == q.choices.end() ? 0 : it - q.choices.begin();
                        const long size = static_cast<long>(q.choices.size());
                        this->_answer = q.choices[static_cast<std::size_t>((at + (event == ftxui::Event::ArrowLeft ? size - 1 : 1)) % size)];
                    } else if (event == ftxui::Event::Backspace) {
                        while (!this->_answer.empty() && (static_cast<unsigned char>(this->_answer.back()) & 0xC0) == 0x80) this->_answer.pop_back();
                        if (!this->_answer.empty()) this->_answer.pop_back();
                    } else if (event.is_character()) {
                        this->_answer += event.character();
                    }
                    return true;
                }
                case Mode::Machine:
                    // y: scanned (in the background), n / Esc: not (at start: never asked again)
                    if (event == ftxui::Event::Character('y') || event == ftxui::Event::Character('Y')) {
                        // the questions about the user still empty, then the scan
                        this->_questions = cluster::machine_questions(this->_manager.config());
                        this->_machineStartup = false;
                        if (!this->_questions.empty()) {
                            this->_question = 0;
                            this->_answer = this->_questions[0].value;
                            this->_mode = Mode::Profile;
                            return true;
                        }
                        this->_manager.scanMachine();
                        this->status_("mapping this computer…");
                        this->_note = "mapping this computer…";
                    } else if (event == ftxui::Event::Character('n') || event == ftxui::Event::Character('N') || event == ftxui::Event::Escape) {
                        if (this->_machineStartup && this->_machineKind == 2) this->_manager.machineLater();
                        else if (this->_machineStartup && this->_machineKind == 1) this->_manager.machineDecline(); // --rescan (3): nothing kept
                    } else {
                        return true;
                    }
                    this->_machineStartup = false;
                    this->_mode = this->_machineReturn == Mode::Machine ? Mode::Normal : this->_machineReturn;
                    return true;
                case Mode::Message:
                    if (event == ftxui::Event::Escape || event == ftxui::Event::Return) this->_mode = Mode::Normal;
                    return true;
                case Mode::Question: {
                    cluster::Asking& asking = this->_manager.asking();
                    const std::optional<cluster::Ask> ask = asking.current();
                    if (!ask) {
                        this->_mode = Mode::Normal;
                        return true;
                    }
                    const cluster::Json& q = ask->questions[ask->index];
                    const int count = static_cast<int>(q.value("options", cluster::Json::array()).size());
                    const bool multi = q.value("multiSelect", false);
                    if (this->_askChat) { // its chat typed: Enter sent, Esc back to the options
                        if (event == ftxui::Event::Escape) {
                            this->_askChat = false;
                        } else if (event == ftxui::Event::Return) {
                            const std::string done = asking.chat(this->_askDraft);
                            if (done.empty()) this->_askDraft.clear();
                            else this->status_(done, true);
                        } else if (event == ftxui::Event::Backspace) {
                            while (!this->_askDraft.empty() && (static_cast<unsigned char>(this->_askDraft.back()) & 0xC0) == 0x80)
                                this->_askDraft.pop_back();
                            if (!this->_askDraft.empty()) this->_askDraft.pop_back();
                        } else if (event.is_character()) {
                            this->_askDraft += event.character();
                        }
                        return true;
                    }
                    if (key == "Ctrl+T") {
                        this->_askChat = true;
                        return true;
                    }
                    if (event == ftxui::Event::Escape) {
                        asking.decline();
                        this->status_("question declined");
                        this->_mode = Mode::Normal;
                    } else if (key == this->_manager.config().keys.at("global") && ask->session != GLOBAL_ID) {
                        const std::string done = asking.toGlobal(); // the whole set: the global session answers it
                        this->status_(done, done != "the global session answers it");
                        this->_mode = Mode::Normal;
                    } else if (event == ftxui::Event::ArrowUp) {
                        asking.move(-1);
                    } else if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Tab) {
                        asking.move(1);
                    } else if (event == ftxui::Event::ArrowLeft && ask->cursor != count) {
                        asking.back();
                    } else if (event == ftxui::Event::Return) {
                        this->askNext_();
                    } else if (ask->cursor == count && event == ftxui::Event::Backspace) {
                        asking.erase();
                    } else if (ask->cursor == count && event.is_character()) {
                        asking.type(event.character()); // on Other: the answer typed (digits and spaces too)
                    } else if (event.is_character() && event.character().size() == 1 && event.character()[0] >= '1' && event.character()[0] <= '9') {
                        const int pick = event.character()[0] - '1';
                        if (pick <= count) {
                            asking.choose(pick);
                            if (!multi && pick < count) this->askNext_();
                        }
                    } else if (multi && event == ftxui::Event::Character(' ')) {
                        asking.toggle();
                    }
                    return true;
                }
                case Mode::WebApprove: {
                    // a: allow, t: allow and trust, d: deny, Esc: later (the palette asks it again)
                    std::string answer;
                    if (event == ftxui::Event::Character('a') || event == ftxui::Event::Return) answer = "allow";
                    else if (event == ftxui::Event::Character('t')) answer = "trust";
                    else if (event == ftxui::Event::Character('d')) answer = "deny";
                    else if (event == ftxui::Event::Escape) {
                        this->_webLater.insert(this->_webAsk);
                        this->_mode = Mode::Normal;
                        this->status_("web: answered later (palette: Web: allow / deny)");
                        return true;
                    } else {
                        return true;
                    }
                    try {
                        cluster::web_act(this->_manager, "approve", {}, this->_webAsk, answer);
                        this->status_(answer == "deny" ? "web: denied" : answer == "trust" ? "web: allowed and trusted" : "web: allowed");
                    } catch (const utils::exception::IException& e) {
                        this->status_(e.info(), true);
                    }
                    this->_mode = Mode::Normal;
                    return true;
                }
                case Mode::WebDevices: {
                    const int count = static_cast<int>(this->webRows_().size());
                    if (!this->_webConfirm.empty()) { // y: done; anything else: not
                        const std::string what = this->_webConfirm;
                        this->_webConfirm.clear();
                        if (event == ftxui::Event::Character('y') || event == ftxui::Event::Character('Y')) this->webDo_(what);
                        return true;
                    }
                    if (event == ftxui::Event::Escape) this->_mode = Mode::Normal;
                    else if (event == ftxui::Event::ArrowDown) this->_webCursor = std::min(this->_webCursor + 1, std::max(0, count - 1));
                    else if (event == ftxui::Event::ArrowUp) this->_webCursor = std::max(0, this->_webCursor - 1);
                    else if ((event == ftxui::Event::Character(' ') || event == ftxui::Event::Return) && this->_webCursor < count) {
                        const std::string row = this->webRows_()[static_cast<std::size_t>(this->_webCursor)].first;
                        if (!this->_webPicked.erase(row)) this->_webPicked.insert(row);
                    } else if (event == ftxui::Event::Character('k')) this->webDo_("kick");
                    else if (event == ftxui::Event::Character('K')) this->_webConfirm = "kick_all";
                    else if (event == ftxui::Event::Character('b')) this->_webConfirm = "ban";
                    else if (event == ftxui::Event::Character('n')) this->webDo_("unban");
                    else if (event == ftxui::Event::Character('u')) this->webDo_("untrust");
                    else if (event == ftxui::Event::Character('a')) this->webDo_("allow");
                    else if (event == ftxui::Event::Character('t')) this->webDo_("trust");
                    else if (event == ftxui::Event::Character('d')) this->webDo_("deny");
                    return true;
                }
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
                case Mode::Search:
                    if (event == ftxui::Event::Escape) {
                        this->_mode = Mode::Normal;
                        this->_scroll[this->_active] = 0;
                    } else if (event == ftxui::Event::Return || event == ftxui::Event::ArrowUp || cluster::key_action(this->_manager.config(), key) == "search") {
                        if (!this->_matches.empty()) this->_hit = (this->_hit + static_cast<int>(this->_matches.size()) - 1) % static_cast<int>(this->_matches.size());
                    } else if (event == ftxui::Event::ArrowDown) {
                        if (!this->_matches.empty()) this->_hit = (this->_hit + 1) % static_cast<int>(this->_matches.size());
                    } else if (event == ftxui::Event::Backspace) {
                        if (!this->_search.empty()) this->_search.pop_back();
                        this->searchUpdate_();
                    } else if (event.is_character()) {
                        this->_search += event.character();
                        this->searchUpdate_();
                    }
                    return true;
                case Mode::History:
                    if (event == ftxui::Event::Escape) {
                        this->_mode = Mode::Normal;
                    } else if (event == ftxui::Event::Tab) {
                        this->_historyAll = !this->_historyAll;
                        this->_selected = 0;
                        this->historyFilter_();
                    } else if (event == ftxui::Event::ArrowDown) {
                        this->_selected = std::min(this->_selected + 1, std::max(0, static_cast<int>(this->_historyItems.size()) - 1));
                    } else if (event == ftxui::Event::ArrowUp || cluster::key_action(this->_manager.config(), key) == "history") {
                        this->_selected = std::max(0, this->_selected - 1);
                    } else if (event == ftxui::Event::Return) {
                        if (!this->_historyItems.empty()) {
                            const std::string chosen = this->_historyItems[static_cast<std::size_t>(this->_selected)];
                            const std::vector<cluster::Draft> drafts = this->_manager.drafts();
                            auto it = std::find_if(drafts.begin(), drafts.end(), [&](const cluster::Draft& d) {return d.text == chosen;});
                            this->_editor.load(it != drafts.end() ? *it : cluster::Draft{chosen, {}, {}});
                            this->historyReset_();
                        }
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
            if (event == ftxui::Event::Escape && this->_app.voice->speaking()) {
                this->_app.voice->mute();
                this->status_("reading stopped");
                return true;
            }
            if (event == ftxui::Event::Escape && !this->_app.voice->pending().empty()) {
                this->_app.voice->cancelLast();
                this->status_("voice prompt dropped (Ctrl+C: all of them)");
                return true;
            }
            if (this->_active == TASKS_ID) { // the Tasks session: no prompt; its items by the keys, its shortcuts still (next / previous, Ctrl+B...)
                if (this->tasksKey_(event)) return true;
                (void)this->shortcut_(cluster::key_action(this->_manager.config(), key));
                return true;
            }
            this->vimSync_();
            if (this->vimKey_(event, key)) return true;
            const std::string& raw = event.input();
            if (raw == "\x1b\r" || raw == "\x1b\n" || raw == "\x1b[13;2u" || raw == "\x1b[27;2;13~") { // xstyle: ignore LU-ANSI (read)
                this->_editor.insert("\n"); // Alt+Enter / Shift+Enter: new line
                return true;
            }
            if (key.size() == 5 && key.starts_with("Alt+") && key[4] >= '1' && key[4] <= '9') {
                const std::vector<cluster::Snapshot> list = this->sessions_();
                const std::size_t index = static_cast<std::size_t>(key[4] - '1');
                if (index < list.size()) this->_active = list[index].spec.id;
                return true;
            }
            if (event == ftxui::Event::Escape) {
                // a queued prompt of the session: back in the prompt to edit it (removed from the queue); else Esc Esc clears
                const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                const std::optional<cluster::Queued> queued = this->_manager.snapshot(this->_active) ? this->_manager.unqueue(this->_active) : std::nullopt;
                if (queued) {
                    if (!cluster::trim(this->_editor.text()).empty()) this->_manager.stashPush(this->_editor.draft());
                    this->_editor.setText(queued->text);
                    this->_editor.setImages(queued->images);
                    this->status_("queued prompt back to edit (Enter: queue it again)");
                    this->_lastEscape = 0;
                } else if (now - this->_lastEscape < 600 && cluster::trim(this->_editor.text()).empty()) {
                    // Esc Esc on an empty prompt (like Claude Code): the last turn leaves the conversation, its prompt comes back
                    this->_lastEscape = 0;
                    this->rewind_();
                } else if (now - this->_lastEscape < 600) {
                    this->_editor.clear();
                    this->historyReset_();
                    this->_lastEscape = 0;
                } else {
                    this->_lastEscape = now;
                }
                return true;
            }
            if (this->shortcut_(cluster::key_action(this->_manager.config(), key))) return true;
            const std::optional<Arrow> arrow = arrow_(event);
            if (event == ftxui::Event::PageUp) this->_scroll[this->_active] += 10;
            else if (event == ftxui::Event::PageDown) this->_scroll[this->_active] = std::max(0, this->_scroll[this->_active] - 10);
            else if (event == ftxui::Event::Return) this->enter_();
            else if (event == ftxui::Event::Backspace) this->_editor.backspace();
            else if (event == ftxui::Event::Delete) this->_editor.erase();
            else if (home_end_(event) == 'H') this->_editor.home(event.input() == "\x1b[1;2H"); // xstyle: ignore LU-ANSI (read)
            else if (home_end_(event) == 'F') this->_editor.end(event.input() == "\x1b[1;2F"); // xstyle: ignore LU-ANSI (read)
            else if (arrow && arrow->dy != 0 && !arrow->ctrl) {
                // Up / Down: a line of the prompt; on its first / last line, the history
                if (arrow->dy < 0 && !arrow->shift && this->_editor.onFirstLine()) (void)this->historyStep_(1);
                else if (arrow->dy > 0 && !arrow->shift && this->_editor.onLastLine()) (void)this->historyStep_(-1);
                else if (arrow->dy < 0) this->_editor.lineUp(arrow->shift);
                else this->_editor.lineDown(arrow->shift);
            } else if (arrow && arrow->dx != 0) {
                if (arrow->ctrl) arrow->dx < 0 ? this->_editor.wordLeft(arrow->shift) : this->_editor.wordRight(arrow->shift);
                else arrow->dx < 0 ? this->_editor.left(arrow->shift) : this->_editor.right(arrow->shift);
            } else if (event == ftxui::Event::Character(' ')) this->_editor.space();
            else if (event.is_character()) this->_editor.insert(event.character());
            else return false;
            return true;
        }

        bool vimKey_(const ftxui::Event& event, const std::string& key)
        {
            // vim: Esc of Insert -> Normal; in Normal / Visual the characters are commands, Enter sends, the other keys
            // (Ctrl / Alt / F) stay the shortcuts
            const cluster::VimMode vim = this->_editor.vim();
            if (vim == cluster::VimMode::Off) return false;
            if (vim == cluster::VimMode::Insert) return event == ftxui::Event::Escape && !this->_editor.completing() && this->_editor.vimKey("Escape");
            if (event == ftxui::Event::Escape) return this->_editor.vimKey("Escape"); // false: nothing waiting, Esc of the prompt
            if (event == ftxui::Event::Return) {
                (void)this->_editor.vimKey("Escape");
                this->submit_();
                return true;
            }
            // k / j (or the arrows) on the first / last line: the history, like the plain prompt
            const bool up = key == "ArrowUp" || (key == "k" && this->_editor.vimPending().empty());
            const bool down = key == "ArrowDown" || (key == "j" && this->_editor.vimPending().empty());
            if (vim == cluster::VimMode::Normal && ((up && this->_editor.onFirstLine() && this->historyStep_(1)) || (down && this->_editor.onLastLine() && this->historyStep_(-1))))
                return true;
            static const std::map<std::string, std::string> keys = {{"ArrowLeft", "h"}, {"ArrowRight", "l"}, {"ArrowUp", "k"}, {"ArrowDown", "j"},
                {"Backspace", "h"}, {"Home", "0"}, {"End", "$"}, {"Delete", "x"}};
            if (key == "Ctrl+R") return this->_editor.vimKey("Ctrl+R");
            if (keys.contains(key)) return this->_editor.vimKey(keys.at(key));
            if (event.is_character() && key.find('+') == std::string::npos) return this->_editor.vimKey(event.character());
            if (event.is_character() && key == "+") return this->_editor.vimKey("+");
            return false;
        }

    public:
        int run(void)
        {
            this->theme_();
            this->vimSync_();
            this->_machineStartup = this->_app.askMachine != 0;
            this->_machineKind = this->_app.askMachine;
            if (this->_app.askRestore) this->_mode = Mode::Restore;
            else if (this->_machineStartup) this->_mode = Mode::Machine;
            this->_editor.setProvider(cluster::completion_provider(this->_manager, [this]() {return this->_active;}));
            termios original{};
            const bool tty = ::tcgetattr(STDIN_FILENO, &original) == 0;
            if (tty) {
                termios raw = original;
                raw.c_iflag &= ~static_cast<tcflag_t>(IXON);
                ::tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            }
            FrameOut frames;
            std::streambuf* const previous = std::cout.rdbuf(&frames);
            this->terminalModes_();
            this->_screen.ForceHandleCtrlC(false);
            this->_screen.ForceHandleCtrlZ(false);
            // not stopped by a signal either (kill -TSTP, a terminal sending it): it stays in the foreground
            const sighandler_t stop = std::signal(SIGTSTP, SIG_IGN);

            // Redraw when something changed (sessions, voice, git), at least once a second (timers)
            std::atomic<bool> alive{true};
            std::thread refresher([&]() {
                std::uint64_t last = 0;
                int ticks = 0;
                while (alive) {
                    // a change, the typing / the spinner (25 images / s), else once a second (timers); a quit signal: out
                    std::this_thread::sleep_for(std::chrono::milliseconds(40));
                    if (cluster::quit_requested()) {
                        this->_screen.Exit();
                        break;
                    }
                    const std::uint64_t version = this->_manager.version() + this->_app.voice->version();
                    if (version != last || this->_animating || this->_sendPending || ++ticks >= 25) {
                        last = version;
                        ticks = 0;
                        this->_screen.PostEvent(ftxui::Event::Custom);
                    }
                }
            });
            ftxui::Component root = ftxui::Renderer([this]() {return this->render_();});
            // Ctrl+Z: its action (undo) or nothing, never the interface stopped in the background
            root = ftxui::CatchEvent(root, [this](ftxui::Event event) {return this->event_(event) || event.input() == "\x1a";});
            this->_screen.Loop(root);
            this->_worker.stop(); // the running job ends before the screen goes
            // the terminal as it was: the frames buffer out first (it hides the cursor), then the modes off, the cursor
            // shown, the synchronized update closed
            std::cout << std::flush;
            std::cout.rdbuf(previous);
            std::cout << "\x1b[?2026l\x1b[?1004l\x1b[?2004l\x1b[?25h" << std::flush; // xstyle: ignore LU-ANSI (modes of the terminal, no color)
            if (tty) ::tcsetattr(STDIN_FILENO, TCSANOW, &original);
            alive = false;
            refresher.join();
            (void)std::signal(SIGTSTP, stop);
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
