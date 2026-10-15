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
#include <utils/utils.hpp>
#include "cluster/Settings.hpp"
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
        std::string _host = cluster::host_name();   // where the sessions run
        std::map<std::string, std::string> _branches;   // their git branch, read again every 5 s
        std::map<std::string, std::int64_t> _branchAt;
        std::uint64_t _raiseSeen = 0;               // the last notification clicked
        std::string _webAsk;                        // the device asking to connect shown (its popup)
        std::set<std::string> _webLater;            // the ones put off (Esc): asked again from the palette
        std::set<std::string> _webPicked;           // the web devices ticked ("d:<id>" connected / waiting, "t:<id>" trusted)
        int _webCursor = 0;
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
            if (text.empty()) return;
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
                    if (!local) this->_manager.send(session, text, false, images);
                    this->_manager.addHistory(draft, session); // typed here: in the history (not the ones of the global session / MCP)
                } catch (const utils::exception::IException& e) {
                    error = std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info());
                }
                this->_screen.Post([this, local, error, draft]() {
                    if (!error.empty()) {
                        this->status_(error, true);
                        if (cluster::trim(this->_editor.text()).empty()) this->_editor.load(draft);
                        return;
                    }
                    if (!local) return;
                    if (!local->focus.empty()) this->_active = local->focus;
                    if (local->ui == "settings") this->openSettings_();
                    else if (local->ui == "restore_menu") this->openRestore_();
                    else if (local->ui == "rewind") this->rewind_();
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
            const std::string& text = this->_editor.text();
            const std::pair<std::size_t, std::size_t> sel = this->_editor.selection();
            const std::size_t cursor = this->_editor.cursor();
            const std::string target = this->_active == GLOBAL_ID ? "to the global session " : "to " + this->_active + " ";
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
                ftxui::Elements parts = {ftxui::text(i == 0 ? " › " : "   ") | ftxui::color(this->_theme.accent) | ftxui::bold};
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
                    parts.push_back(ftxui::text(" @ file · / command · " + this->_manager.config().keys.at("history") + " history · " + this->_manager.config().keys.at("help") + " keys")
                        | ftxui::color(this->_theme.muted) | ftxui::dim);
                parts.push_back(ftxui::filler());
                if (i == first && this->_editor.vim() != cluster::VimMode::Off) {
                    static const std::map<cluster::VimMode, std::string> names = {{cluster::VimMode::Insert, "INSERT"}, {cluster::VimMode::Normal, "NORMAL"},
                        {cluster::VimMode::Visual, "VISUAL"}, {cluster::VimMode::VisualLine, "V-LINE"}};
                    const bool insert = this->_editor.vim() == cluster::VimMode::Insert;
                    parts.push_back(ftxui::text(" -- " + names.at(this->_editor.vim()) + " -- " + this->_editor.vimPending() + " ")
                        | ftxui::color(insert ? this->_theme.muted : this->_theme.warn) | ftxui::bold);
                }
                if (i == first) parts.push_back(ftxui::text(target) | ftxui::color(this->_theme.muted) | ftxui::dim);
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
            // drawn: its box kept, a click in it runs act (the last drawn first: what is over the rest)
            this->_clicks.push_back({ftxui::Box{-1, -1, -1, -1}, layer, clip, act});
            return element | ftxui::reflect(this->_clicks.back().box);
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
            } else if (base == "machine_sync" || base == "edit_global_prompt" || base == "edit_sessions_prompt" || base == "edit_vocabulary" || base == "edit_words") {
                this->settingsAction_(base == "edit_global_prompt" ? "edit_prompt" : base);
                if (!this->_note.empty() && this->_mode == Mode::Normal) this->status_(this->_note);
            } else if (base == "restore_menu") {
                this->openRestore_();
            } else if (base == "web_devices") {
                this->_webPicked.clear();
                this->_webCursor = 0;
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
            if (action == "expand") {
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

        void saveSettings_(const bool exit = true)
        {
            if (!this->_changes.empty()) {
                try {
                    cluster::settings_write(this->_manager.config(), this->_changes);
                } catch (const utils::exception::IException& e) {
                    this->_note = std::string("not saved: ") + e.info();
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
            } else if (event == ftxui::Event::ArrowUp) {
                this->_item = std::max(this->_item - 1, 0);
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
                else if (setting.kind == cluster::SettingKind::Action) value = "▶ " + value;
                else if (value.empty()) value = "(empty)";
                const std::string label = setting.label + " ";
                const std::size_t dots = label.size() < 34 ? 34 - label.size() : 1;
                const int index = static_cast<int>(i);
                // clicked: selected; its value clicked: changed (a switch, a choice), edited (a text), run (an action)
                const std::function<void(bool)> pick = [this, index](const bool change) {
                    if (this->_editing && this->_item != index) (void)this->event_(ftxui::Event::Return);
                    this->_item = index;
                    if (change && !this->_editing) (void)this->event_(ftxui::Event::Return);
                };
                ftxui::Element line = ftxui::hbox({
                    this->tap_(ftxui::text(changed ? " * " : "   ") | ftxui::color(t.warn) | ftxui::bold, 2, [pick]() {pick(false);}, clip),
                    this->tap_(ftxui::text(label + std::string(dots, '.') + " ") | ftxui::color(t.muted), 2, [pick]() {pick(false);}, clip),
                    this->tap_(ftxui::text("[" + cluster::one_line(value, 60) + "]") | (selected ? ftxui::color(t.bg) | ftxui::bgcolor(t.accent) : ftxui::color(t.text)) | ftxui::bold,
                        2, [pick]() {pick(true);}, clip),
                    ftxui::filler(),
                });
                rows.push_back(selected ? line | ftxui::focus : line);
                if (selected) {
                    help = setting.description;
                    if (setting.choiceHelp) choiceHelp = setting.choiceHelp(this->settingValue_(setting));
                    restart = setting.restart;
                }
            }
            if (rows.empty()) rows.push_back(ftxui::text("   nothing here") | ftxui::color(t.muted));
            std::string note = this->_note == "confirm" ? "Save the " + std::to_string(this->_changes.size()) + " change(s)? (y / n)" : this->_note;
            ftxui::Elements choice;
            for (const std::string& line: cluster::split(choiceHelp, '\n'))
                choice.push_back(ftxui::paragraph(line) | (choice.empty() ? ftxui::bold | ftxui::color(t.accent) : ftxui::color(t.text)));
            if (!choice.empty()) choice.insert(choice.begin(), ftxui::separatorLight() | ftxui::color(t.border));
            ftxui::Element helpBox = ftxui::vbox({
                ftxui::text(" Item Specific Help") | ftxui::bold | ftxui::color(t.accent),
                ftxui::separatorLight() | ftxui::color(t.border),
                ftxui::paragraph(help) | ftxui::color(t.text),
                ftxui::vbox(std::move(choice)),
                ftxui::text(""),
                restart ? ftxui::paragraph("Applied at the next start of claude-cluster") | ftxui::color(t.warn) : ftxui::emptyElement(),
                ftxui::filler(),
                ftxui::text(std::to_string(this->_changes.size()) + " change(s) not saved") | ftxui::color(this->_changes.empty() ? t.muted : t.warn),
                ftxui::paragraph(cluster::short_path(this->_manager.config().path().string())) | ftxui::color(t.muted) | ftxui::dim,
            }) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, choiceHelp.empty() ? 38 : 64);
            return ftxui::vbox({
                ftxui::hbox({ftxui::text(" claude-cluster setup ") | ftxui::bold | ftxui::color(t.bg) | ftxui::bgcolor(t.accent), ftxui::filler(),
                    ftxui::text(" config.toml ") | ftxui::color(t.muted)}),
                ftxui::hbox(std::move(tabs)) | ftxui::bgcolor(t.surface),
                ftxui::separatorHeavy() | ftxui::color(t.accent),
                ftxui::hbox({ftxui::vbox(std::move(rows)) | ftxui::yframe | this->clipBox_(clip) | ftxui::flex, ftxui::separatorLight() | ftxui::color(t.border), helpBox}) | ftxui::flex,
                ftxui::separatorHeavy() | ftxui::color(t.accent),
                this->_note == "confirm" ? ftxui::hbox({ftxui::text(" " + note + "  ") | ftxui::color(t.warn), keyed(" [y] save ", ftxui::Event::Character('y')) | ftxui::color(t.accent),
                    keyed(" [n] drop ", ftxui::Event::Character('n')) | ftxui::color(t.accent)}) : ftxui::text(" " + note) | ftxui::color(t.warn),
                ftxui::hbox({ftxui::hbox({ftxui::text(" ↑↓ select   ←→ / Enter change   Enter edit text   Tab / Shift+Tab section   "), keyed("Ctrl+S save", ftxui::Event::Special(std::string(1, '\x13'))),
                    ftxui::text("   "), keyed("F10 save & exit", ftxui::Event::F10), ftxui::text("   "), keyed("Esc exit (asks to save) ", ftxui::Event::Escape)})
                    | ftxui::color(t.bg) | ftxui::bgcolor(t.muted), ftxui::filler()}) | ftxui::bgcolor(t.muted),
            }) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, size.dimy) | ftxui::color(t.text) | ftxui::bgcolor(t.bg);
        }

        /* rendering */
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
            ftxui::Element sub = ftxui::text("  " + cluster::one_line(backend + " · " + cluster::short_path(s.spec.cwd), 26)) | ftxui::color(this->_theme.muted) | ftxui::dim;
            const std::string id = s.spec.id;
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
                    lines.push_back({"  ⎿ " + cluster::one_line(turn.value("detail", std::string()), room - 6), ftxui::color(this->_theme.muted), "\n"});
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
            for (const std::string& queued: s.queue)
                lines.push_back({"⧗ queued: " + cluster::one_line(queued, static_cast<std::size_t>(std::max(10, width - 30))) + (compact ? "" : "  (Esc: edit)"), ftxui::color(this->_theme.warn), "\n"});

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
            if (!s.lastError.empty()) return ftxui::vbox({head, ftxui::text(" " + cluster::one_line(s.lastError, 200)) | ftxui::color(this->_theme.error)});
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
                for (const cluster::Snapshot& s: list) {
                    const bool active = s.spec.id == this->_active;
                    ftxui::Element tab = ftxui::text(" " + state_mark_(s.state) + " " + cluster::one_line(s.spec.name, 14) + (s.permissions.empty() ? "" : " !") + " ");
                    const std::string id = s.spec.id;
                    tabs.push_back(this->tap_(active ? tab | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) : tab | ftxui::color(this->_theme.muted),
                        0, [this, id]() {this->_active = id;}));
                }
                return ftxui::vbox({ftxui::hbox(std::move(tabs)), this->session_(*it, width, height - 1, false) | ftxui::flex});
            }
            // list + active panel
            ftxui::Elements rows = {ftxui::text(" Sessions") | ftxui::bold | ftxui::color(this->_theme.accent), ftxui::text("")};
            const int clip = this->clip_();
            // more sessions than the room: the ones around the one shown (2 lines each)
            const std::size_t room = static_cast<std::size_t>(std::max(1, (height - 3) / 2));
            const std::size_t at = static_cast<std::size_t>(it - list.begin());
            const std::size_t first = list.size() <= room || at < room ? 0 : std::min(at + 1 - room, list.size() - room);
            if (first > 0) rows.push_back(ftxui::text(" ↑ " + std::to_string(first) + " more") | ftxui::color(this->_theme.muted) | ftxui::dim);
            for (std::size_t i = first; i < list.size() && i < first + room; ++i)
                rows.push_back(this->sessionRow_(list[i], list[i].spec.id == this->_active, clip));
            if (first + room < list.size()) rows.push_back(ftxui::text(" ↓ " + std::to_string(list.size() - first - room) + " more") | ftxui::color(this->_theme.muted) | ftxui::dim);
            const std::vector<cluster::Task> tasks = this->_manager.tasks();
            const auto open = std::count_if(tasks.begin(), tasks.end(), [](const cluster::Task& t) {return t.status == "queued" || t.status == "running";});
            if (open > 0) rows.push_back(ftxui::text(" " + std::to_string(open) + " task(s) in the queue") | ftxui::color(this->_theme.muted));
            return ftxui::hbox({
                ftxui::vbox(std::move(rows)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 30) | ftxui::yframe | this->clipBox_(clip),
                ftxui::separatorLight() | ftxui::color(this->_theme.border),
                this->session_(*it, width - 31, height, false) | ftxui::flex,
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
                if (this->_manager.config().voice.enabled)
                    lines.push_back(ftxui::text(" voice: a number or an option · valide / suivant · retour · autre <answer> · annule") | ftxui::color(this->_theme.muted) | ftxui::dim);
                return box(ftxui::vbox(std::move(lines)), 96);
            }
            if (this->_mode == Mode::Normal && asked && asked->session == this->_active && !this->askShown_(*asked)) {
                const cluster::Json& q = asked->questions[asked->index];
                return box(this->tap_(ftxui::vbox({ftxui::text(" " + asked->name + " waits for your answer") | ftxui::bold, ftxui::text(""),
                    ftxui::paragraph(" " + q.value("question", std::string())) | ftxui::color(this->_theme.muted), ftxui::text(""),
                    ftxui::text(" its question is on the global session: " + this->_manager.config().keys.at("global") + " (or a click) to answer it") | ftxui::color(this->_theme.warn)})
                    | ftxui::dim, 0, [this]() {(void)this->shortcut_("global");}), 80);
            }
            if (this->_mode == Mode::WebApprove && this->_app.web) {
                // who it is: its name, its address, its browser, since when
                const cluster::WebStatus status = this->_app.web->status();
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
                ftxui::Elements lines = {ftxui::text(" Web devices") | ftxui::bold, ftxui::text("")};
                const std::vector<std::pair<std::string, cluster::WebDevice>> rows = this->webRows_();
                if (rows.empty()) lines.push_back(ftxui::text("   nobody connected, no trusted device") | ftxui::color(this->_theme.muted));
                bool trustedTitle = false;
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    const auto &[key, d] = rows[i];
                    if (key.starts_with("t:") && !trustedTitle) {
                        lines.push_back(ftxui::text(" trusted (no code, no approval)") | ftxui::color(this->_theme.muted));
                        trustedTitle = true;
                    }
                    const bool picked = this->_webPicked.contains(key);
                    const std::string state = d.state == "trusted" ? "last " + (d.seen > 0 ? cluster::human_age(d.seen / 1000) + " ago" : std::string("never"))
                        : d.state + (d.trusted ? " · trusted" : "");
                    ftxui::Element row = ftxui::text(std::string(picked ? " [x] " : " [ ] ") + d.name + "  " + d.address + "  " + state + " ");
                    const int index = static_cast<int>(i);
                    lines.push_back(this->tap_(index == this->_webCursor ? row | ftxui::bold | ftxui::bgcolor(this->_theme.border) : row | ftxui::color(this->_theme.text), 1,
                        [this, index, key]() {
                            this->_webCursor = index;
                            if (!this->_webPicked.erase(key)) this->_webPicked.insert(key);
                        }));
                }
                lines.push_back(ftxui::text(""));
                lines.push_back(ftxui::hbox({press(ftxui::text(" [k] kick the ticked ") | ftxui::color(this->_theme.accent), ftxui::Event::Character('k')), ftxui::text(" "),
                    press(ftxui::text(" [K] kick all ") | ftxui::color(this->_theme.error), ftxui::Event::Character('K')), ftxui::text(" "),
                    press(ftxui::text(" [u] trust no more ") | ftxui::color(this->_theme.warn), ftxui::Event::Character('u')), ftxui::text(" "),
                    press(ftxui::text(" [a/t/d] a waiting one ") | ftxui::color(this->_theme.muted), ftxui::Event::Character('a')), ftxui::text(" "),
                    press(ftxui::text(" Esc ") | ftxui::color(this->_theme.muted), ftxui::Event::Escape)}));
                lines.push_back(ftxui::text(" ↑↓ · Space / a click: tick · none ticked: the one under the cursor") | ftxui::color(this->_theme.muted) | ftxui::dim);
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
            if (this->_app.web)
                for (const cluster::WebDevice& d: this->_app.web->status().devices)
                    if (d.state == "waiting" && !this->_webLater.contains(d.id)) {
                        this->_webAsk = d.id;
                        this->_mode = Mode::WebApprove;
                        return;
                    }
        }

        _nodiscard std::vector<std::pair<std::string, cluster::WebDevice>> webRows_(void) const
        {
            // the devices waiting and connected ("d:<id>"), then the trusted ones ("t:<id>")
            std::vector<std::pair<std::string, cluster::WebDevice>> rows;
            if (!this->_app.web) return rows;
            const cluster::WebStatus status = this->_app.web->status();
            for (const cluster::WebDevice& d: status.devices)
                rows.emplace_back("d:" + d.id, d);
            for (const cluster::WebDevice& d: status.trusted)
                rows.emplace_back("t:" + d.id, d);
            return rows;
        }

        void webDo_(const std::string& what)
        {
            // kick (the ticked ones, else the one under the cursor), all, untrust, a waiting one answered
            if (!this->_app.web) return;
            const std::vector<std::pair<std::string, cluster::WebDevice>> rows = this->webRows_();
            std::vector<std::string> devices;
            std::vector<std::string> trusted;
            for (const auto &[key, d]: rows)
                if (this->_webPicked.contains(key)) (key.starts_with("t:") ? trusted : devices).push_back(d.id);
            if (devices.empty() && trusted.empty() && this->_webCursor < static_cast<int>(rows.size())) {
                const auto &[key, d] = rows[static_cast<std::size_t>(this->_webCursor)];
                (key.starts_with("t:") ? trusted : devices).push_back(d.id);
            }
            try {
                if (what == "kick_all") {
                    this->_app.web->kick();
                    this->status_("web: every device disconnected, a new code");
                } else if (what == "kick" && !devices.empty()) {
                    this->_app.web->kick(devices);
                    this->status_("web: " + std::to_string(devices.size()) + " disconnected");
                } else if (what == "untrust") {
                    std::vector<std::string> ids = trusted;
                    if (ids.empty()) ids = devices; // a connected trusted device: its trust
                    this->_app.web->untrust(ids);
                    this->status_("web: trusted no more");
                } else if (what == "allow" || what == "trust" || what == "deny") {
                    for (const auto &[key, d]: rows)
                        if (d.state == "waiting" && (std::find(devices.begin(), devices.end(), d.id) != devices.end())) this->_app.web->approve(d.id, what);
                }
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
            const std::string web = this->_app.web ? cluster::web_line(this->_app.web->status()) : "";

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
                web.empty() ? ftxui::emptyElement() : this->tap_(ftxui::text("  " + web + " ") | ftxui::bold | ftxui::color(this->_theme.accent), 0, [this]() {
                    // its code copied (ready to paste in the page), the devices dialog when one waits
                    const cluster::WebStatus status = this->_app.web->status();
                    std::string digits;
                    for (const char c: status.pin)
                        if (c >= '0' && c <= '9') digits += c;
                    cluster::detach({"wl-copy", "--", digits});
                    this->status_("web: code " + status.pin + " copied");
                }),
                ftxui::filler(),
                key(c.keys.at("palette") + " palette", "palette"), dot(), key(c.keys.at("settings") + " settings", "settings"), dot(),
                key(c.keys.at("next_session") + "/" + c.keys.at("prev_session") + " session", "next_session"), dot(), key(c.keys.at("global") + " global", "global"), dot(),
                key(c.keys.at("new_session") + " new", "new_session"), dot(), ftxui::text(c.keys.at("quit") + " quit ") | ftxui::color(this->_theme.muted),
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
            // focus reporting of the terminal (?1004): in / out
            if (event.input() == "\x1b[I" || event.input() == "\x1b[O") { // xstyle: ignore LU-ANSI (input of the terminal, no color)
                this->_manager.setFocused(event.input() == "\x1b[I"); // xstyle: ignore LU-ANSI (focus in)
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
                    if (event == ftxui::Event::Escape) {
                        asking.decline();
                        this->status_("question declined");
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
                        if (this->_app.web) this->_app.web->approve(this->_webAsk, answer);
                        this->status_(answer == "deny" ? "web: denied" : answer == "trust" ? "web: allowed and trusted" : "web: allowed");
                    } catch (const utils::exception::IException& e) {
                        this->status_(e.info(), true);
                    }
                    this->_mode = Mode::Normal;
                    return true;
                }
                case Mode::WebDevices: {
                    const int count = static_cast<int>(this->webRows_().size());
                    if (event == ftxui::Event::Escape) this->_mode = Mode::Normal;
                    else if (event == ftxui::Event::ArrowDown) this->_webCursor = std::min(this->_webCursor + 1, std::max(0, count - 1));
                    else if (event == ftxui::Event::ArrowUp) this->_webCursor = std::max(0, this->_webCursor - 1);
                    else if ((event == ftxui::Event::Character(' ') || event == ftxui::Event::Return) && this->_webCursor < count) {
                        const std::string row = this->webRows_()[static_cast<std::size_t>(this->_webCursor)].first;
                        if (!this->_webPicked.erase(row)) this->_webPicked.insert(row);
                    } else if (event == ftxui::Event::Character('k')) this->webDo_("kick");
                    else if (event == ftxui::Event::Character('K')) this->webDo_("kick_all");
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
            root = ftxui::CatchEvent(root, [this](ftxui::Event event) {return this->event_(event);});
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
