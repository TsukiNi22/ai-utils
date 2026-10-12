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
#include "cluster/Actions.hpp"
#include "cluster/Session.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Worker.hpp"
#include "cluster/Editor.hpp"
#include "cluster/Reveal.hpp"
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
#include <cerrno>
#include <atomic>
#include <chrono>
#include <regex>
#include <cmath>
#include <map>

namespace {
//----------------------------------------------------------------//
/* TYPES */

enum class Mode {Normal, Palette, Prompt, Message, Restore, Settings, History, Search, Machine, Profile};

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
        struct View {
            ftxui::Box box;
            std::vector<std::string> lines;         // transcript lines shown (mouse selection)
            std::vector<std::string> joins;
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
        std::int64_t _lastEscape = 0;               // Esc Esc: clear the prompt
        std::int64_t _lastInterrupt = 0;            // Ctrl+C Ctrl+C on an empty prompt, nothing running: quit
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
            } else if (key == "edit_prompt" || key == "edit_sessions_prompt" || key == "edit_vocabulary") {
                const bool global = key == "edit_prompt";
                const bool vocabulary = key == "edit_vocabulary";
                const cluster::Config& config = this->_manager.config();
                const std::filesystem::path path = cluster::expand_home(global ? config.globalPrompt : vocabulary ? cluster::vocabulary_path(config.voice.vocabulary).string() : config.sessionsPrompt);
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
            std::vector<std::pair<std::size_t, std::size_t>> lines; // <start, end> of each line
            for (std::size_t start = 0;;) {
                const std::size_t nl = text.find('\n', start);
                lines.emplace_back(start, nl == std::string::npos ? text.size() : nl);
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            std::size_t current = 0;
            for (std::size_t i = 0; i < lines.size(); ++i)
                if (cursor >= lines[i].first && cursor <= lines[i].second) current = i;
            const std::size_t first = current >= 7 ? current - 7 : 0;
            const std::size_t room = static_cast<std::size_t>(std::max(10, width - 30));
            ftxui::Elements rows;
            for (std::size_t i = first; i < lines.size() && i < first + 8; ++i) {
                const auto [ls, le] = lines[i];
                ftxui::Elements parts = {ftxui::text(i == 0 ? " › " : "   ") | ftxui::color(this->_theme.accent) | ftxui::bold};
                std::size_t from = ls;
                if (le - ls > room && cursor > ls + room / 2 && i == current) from = std::min(cursor - room / 2, le - room);
                while (from > ls && (static_cast<unsigned char>(text[from]) & 0xC0) == 0x80) --from;
                if (from > ls) parts.push_back(ftxui::text("…") | ftxui::color(this->_theme.muted));
                this->_promptRows.push_back({from, le, from > ls ? 4 : 3});
                const std::function<void(std::size_t, std::size_t, bool)> piece = [&](std::size_t x, std::size_t y, bool selected) {
                    x = std::max(x, from);
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
                        // the cursor of the terminal (a bar) on the character: no cell added in the text
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
                if (i == first) parts.push_back(ftxui::text(this->_active == GLOBAL_ID ? "to the global session " : "to " + this->_active + " ") | ftxui::color(this->_theme.muted) | ftxui::dim);
                rows.push_back(ftxui::hbox(std::move(parts)));
            }
            if (lines.size() > 8) rows.push_back(ftxui::text("   " + std::to_string(lines.size()) + " lines") | ftxui::color(this->_theme.muted) | ftxui::dim);
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
                const std::string prefix = this->_editor.kind() == cluster::CompletionKind::File ? "@" : this->_editor.kind() == cluster::CompletionKind::Command ? "/" : "";
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
            } else if (base == "machine_sync" || base == "edit_global_prompt" || base == "edit_sessions_prompt" || base == "edit_vocabulary") {
                this->settingsAction_(base == "edit_global_prompt" ? "edit_prompt" : base);
                if (!this->_note.empty() && this->_mode == Mode::Normal) this->status_(this->_note);
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
                if (!this->_app.voice->pending().empty()) {
                    this->_app.voice->cancel();
                    this->status_("voice prompts dropped");
                } else if (snap && (snap->state == cluster::State::Working || snap->state == cluster::State::Waiting)) {
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
                else if (setting.kind == cluster::SettingKind::Action) value = "▶ " + value;
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
            std::string note = this->_note == "confirm" ? "Save the " + std::to_string(this->_changes.size()) + " change(s)? (y / n)" : this->_note;
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
                ftxui::hbox({ftxui::text(" ↑↓ select   ←→ / Enter change   Enter edit text   Tab / Shift+Tab section   Ctrl+S save   F10 save & exit   Esc exit (asks to save) ")
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
            // Wrapped lines of the transcript, the last ones (minus the scroll); kept as text for the mouse selection
            struct Row {
                std::string text;
                ftxui::Decorator style;
                std::string join;   // before it when copied
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
                const bool tool = !s.entries.empty() && s.entries.back().kind == cluster::EntryKind::Tool;
                const std::string verb = !s.thinking.empty() ? "Thinking" : (!s.partial.empty() || frame.typed != static_cast<std::size_t>(-1)) ? "Writing"
                    : tool ? "Running " + s.entries.back().tool : "Working";
                const std::int64_t output = s.metrics.current.output;
                lines.push_back({spinner[static_cast<std::size_t>(ms / 120) % spinner.size()] + " " + verb + "… (" + (since > 0 ? std::to_string(cluster::now() - since) + "s" : std::string("…"))
                    + (output > 0 ? " · ↓ " + cluster::human_tokens(output) + " tokens" : std::string()) + " · " + config.keys.at("interrupt") + " to interrupt)"
                    + (s.queued > 0 ? "  " + std::to_string(s.queued) + " queued" : std::string()), ftxui::color(this->_theme.accent), "\n"});
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
                ftxui::text(s.spec.effort.empty() || s.spec.effort == "default" ? "" : "  effort " + s.spec.effort) | ftxui::color(this->_theme.muted),
                ftxui::filler(),
                ftxui::text(cluster::short_path(s.spec.cwd) + " ") | ftxui::color(this->_theme.muted) | ftxui::dim,
            };
            ftxui::Element head = ftxui::hbox(std::move(left));
            if (!s.lastError.empty()) return ftxui::vbox({head, ftxui::text(" " + cluster::one_line(s.lastError, 200)) | ftxui::color(this->_theme.error)});
            if (!s.remoteUrl.empty() || !s.bridgeState.empty())
                return ftxui::vbox({head, ftxui::text(" Remote Control " + (s.bridgeState.empty() ? std::string("on") : s.bridgeState) + (s.remoteUrl.empty() ? std::string() : ": " + s.remoteUrl)
                    + "  (live on claude.ai and here)") | ftxui::color(this->_theme.accent)});
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
            if (voice.listening() && !voice.ready()) label = " … loading the speech model (~10 s) ";
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
                if (items.empty()) items.push_back(ftxui::text(this->_historyAll ? " no prompt yet" : " no prompt in this session yet (Tab: every session)") | ftxui::color(this->_theme.muted));
                const std::string scope = this->_historyAll ? " history (every session) > " : " history (" + this->_active + ") > ";
                return box(ftxui::vbox({ftxui::hbox({ftxui::text(scope) | ftxui::color(this->_theme.accent), ftxui::text(this->_filter), ftxui::text("▏") | ftxui::blink}),
                    ftxui::separatorLight(), ftxui::vbox(std::move(items)), ftxui::text(std::string(" Enter: put in the prompt · Tab: ")
                    + (this->_historyAll ? "this session only" : "every session") + " · Esc: close") | ftxui::dim}), 120);
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
            if (this->_mode == Mode::Machine) {
                const bool old = this->_machineStartup && this->_machineKind == 2;
                const bool forced = this->_machineStartup && this->_machineKind == 3; // --rescan
                const std::string title = old ? " The map of this computer is " + std::to_string(cluster::machine_days_old()) + " days old: sync it?"
                    : forced && cluster::machine_days_old() >= 0 ? " Sync the map of this computer? (last " + cluster::machine_age() + ")" : " Map this computer?";
                const std::string no = !this->_machineStartup || forced ? "" : old ? " (proposed again in " + std::to_string(this->_manager.config().machineDays) + " days)"
                    : " (not asked again: settings > Global session)";
                return box(ftxui::vbox({ftxui::text(title) | ftxui::bold, ftxui::text(""),
                    ftxui::paragraph(cluster::machine_scope()) | ftxui::color(this->_theme.text), ftxui::text(""),
                    ftxui::text(" [y] yes   [n] / Esc no" + no) | ftxui::color(this->_theme.accent),
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
                        choices.push_back(ftxui::text(" " + (choice.empty() ? std::string("(none)") : choice) + " ") | (choice == this->_answer
                            ? ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) : ftxui::color(this->_theme.text)));
                    lines.push_back(ftxui::hbox(std::move(choices)));
                }
                lines.push_back(ftxui::text(""));
                lines.push_back(ftxui::text(" Enter: ok · Esc: skip it · Ctrl+S: skip the rest (config.toml [user], F9 > User)") | ftxui::color(this->_theme.muted) | ftxui::dim);
                return box(ftxui::vbox(std::move(lines)), 90);
            }
            if (this->_mode == Mode::Restore) {
                const std::vector<cluster::SessionSpec> previous = this->_manager.previous();
                ftxui::Elements lines = {ftxui::text(" Restore the previous run?") | ftxui::bold, ftxui::text("")};
                for (const cluster::SessionSpec& spec: previous)
                    lines.push_back(ftxui::text("   " + spec.name + "  " + spec.backend + "  " + cluster::short_path(spec.cwd)) | ftxui::color(this->_theme.muted));
                lines.push_back(ftxui::text("   global session: its conversation") | ftxui::color(this->_theme.muted));
                lines.push_back(ftxui::text(""));
                lines.push_back(ftxui::text(" [r] / Enter / Esc  restore") | ftxui::color(this->_theme.accent));
                lines.push_back(ftxui::text(" [n]  start new: the sessions to the trash (" + std::to_string(this->_manager.config().trashDays)
                    + " days), a new global conversation") | ftxui::color(this->_theme.accent));
                lines.push_back(ftxui::text(" [q] / Ctrl+C / " + this->_manager.config().keys.at("quit") + "  quit (asked again at the next start)") | ftxui::color(this->_theme.muted));
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
            // the global session asked to show a session (ui_focus); the style may have changed (ui_style)
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
            ftxui::Element screen = this->_mode == Mode::Settings ? this->settings_(size) : this->_mode == Mode::Normal ? document
                : ftxui::dbox({document, this->modal_()});
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
                if (mouse.button == ftxui::Mouse::WheelUp) {
                    this->_scroll[this->_active] += 3;
                    return true;
                }
                if (mouse.button == ftxui::Mouse::WheelDown) {
                    this->_scroll[this->_active] = std::max(0, this->_scroll[this->_active] - 3);
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
