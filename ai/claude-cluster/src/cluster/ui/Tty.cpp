/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Tty.cpp

File Description:
##  ftxui::Terminal front-end (FTXUI): layouts list / grid, session
##  view with its panels, permission and voice bars, palette
\**************************************************************/

#define _Exception
#define _Attribute
#define _Encapsulation
#define _SManip
#include <utils/utils.hpp>
#include "cluster/RewindChooser.hpp"
#include "cluster/Attachments.hpp"
#include "cluster/LineEditor.hpp"
#include "cluster/StashPopup.hpp"
#include "cluster/PromptNav.hpp"
#include "cluster/Settings.hpp"
#include "cluster/Services.hpp"
#include "cluster/Showcase.hpp"
#include "cluster/TtyGraph.hpp"
#include "cluster/Plugins.hpp"
#include "cluster/Display.hpp"
#include "cluster/Actions.hpp"
#include "cluster/Docs.hpp"
#include "cluster/Session.hpp"
#include "cluster/Machine.hpp"
#include "cluster/Origin.hpp"
#include "cluster/Worker.hpp"
#include "cluster/Editor.hpp"
#include "cluster/Reveal.hpp"
#include "cluster/Icons.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Demo.hpp"
#include "cluster/Cron.hpp"
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
#include <limits>
#include <deque>
#include <regex>
#include <cmath>
#include <set>
#include <map>

namespace {
//----------------------------------------------------------------//
/* TYPES */

enum class Mode {Normal, Palette, Prompt, Message, Restore, Settings, History, Search, Machine, Profile, Question, WebApprove, WebDevices, QuitAsk, Form, Plugins, Graph, Rewind, Stash,
    Tree};

struct FormField {                     // a field of the form of a display, filled in its popup
    std::string name;
    std::string label;
    std::string type;                  // text | number | choice | bool | date
    std::vector<std::string> options;
    std::string value;
};

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
    bool dark = true;
    ftxui::Color okInk = ftxui::Color::Default;     // the text on a plate of the ok colour (bg or text: the more contrasted)
};

//----------------------------------------------------------------//
/* TOOLS */

_cold ftxui::Color hex_(const std::string& hex)
{
    if (hex.size() != 7 || hex[0] != '#') return ftxui::Color::Default;
    const std::function<std::uint8_t(const std::size_t)> byte = [&](const std::size_t i) {return static_cast<std::uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));};
    return ftxui::Color::RGB(byte(1), byte(3), byte(5));
}

struct CodePalette {                  // the colours of the code boxes (from the theme)
    std::uint32_t text = 0xe6e7ea;
    std::uint32_t comment = 0x7f848e;
    std::uint32_t keyword = 0x7aa2f7;
    std::uint32_t string = 0x9ece6a;
    std::uint32_t number = 0xe0af68;
    std::uint32_t type = 0xe0af68;
    std::uint32_t function = 0x7dcfff;
    std::uint32_t surface = 0x24283b;   // the slots of a form
};

_cold std::uint32_t rgb_(const std::string& hex, const std::uint32_t fallback)
{
    if (hex.size() != 7 || hex[0] != '#') return fallback;
    try {
        return static_cast<std::uint32_t>(std::stoul(hex.substr(1), nullptr, 16));
    } catch (const std::exception&) {
        return fallback;
    }
}

_cold double audio_seconds_(const std::string& bytes)
{
    // the length of a WAV (its fmt byte rate, its data size); another format: unknown (-1)
    if (bytes.size() < 44 || bytes.compare(0, 4, "RIFF") != 0 || bytes.compare(8, 4, "WAVE") != 0) return -1.0;
    const std::function<std::uint32_t(std::size_t)> u32 = [&bytes](const std::size_t at) {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at])) | static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 1])) << 8
            | static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 2])) << 16 | static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 3])) << 24;
    };
    std::uint32_t rate = 0;
    for (std::size_t at = 12; at + 8 <= bytes.size();) {
        const std::string id = bytes.substr(at, 4);
        const std::uint32_t size = u32(at + 4);
        if (id == "fmt " && at + 16 <= bytes.size()) rate = u32(at + 16);
        if (id == "data") return rate > 0 ? static_cast<double>(std::min<std::size_t>(size, bytes.size() - at - 8)) / rate : -1.0;
        at += 8 + size + (size % 2);
    }
    return -1.0;
}

_cold std::vector<cluster::Cell> highlight_(const std::string& line, const std::string& language, const CodePalette& p, bool& inComment)
{
    // a light tokenizer: comments, strings, numbers, keywords, types (C-like: capitalized), calls; by family of language
    static const std::set<std::string> clike = {"if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue", "return", "goto",
        "class", "struct", "enum", "union", "namespace", "using", "typedef", "template", "typename", "public", "private", "protected", "virtual", "override",
        "final", "static", "const", "constexpr", "consteval", "inline", "extern", "volatile", "mutable", "explicit", "friend", "operator", "new", "delete",
        "try", "catch", "throw", "noexcept", "auto", "void", "bool", "char", "int", "long", "short", "float", "double", "unsigned", "signed", "true", "false",
        "nullptr", "this", "sizeof", "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast", "co_await", "co_return", "co_yield", "concept",
        "requires", "import", "export", "package", "interface", "implements", "extends", "function", "var", "let", "fn", "pub", "impl", "trait", "mod",
        "use", "match", "mut", "ref", "self", "Self", "loop", "where", "async", "await", "yield", "func", "go", "defer", "chan", "select", "type", "null",
        "undefined", "instanceof", "typeof", "in", "of", "from", "as", "is", "super", "#include", "#define", "#ifndef", "#ifdef", "#endif", "#pragma", "#if", "#else"};
    static const std::set<std::string> python = {"def", "class", "if", "elif", "else", "for", "while", "return", "import", "from", "as", "with", "try", "except",
        "finally", "raise", "pass", "break", "continue", "lambda", "yield", "global", "nonlocal", "in", "is", "not", "and", "or", "None", "True", "False",
        "async", "await", "assert", "del", "match", "case", "self"};
    static const std::set<std::string> shell = {"if", "then", "else", "elif", "fi", "for", "while", "until", "do", "done", "case", "esac", "in", "function",
        "return", "local", "export", "readonly", "declare", "set", "unset", "source", "echo", "cd", "exit", "shift", "true", "false"};
    std::string lang = language;
    std::transform(lang.begin(), lang.end(), lang.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});
    const bool py = lang == "python" || lang == "py";
    const bool sh = lang == "sh" || lang == "bash" || lang == "zsh" || lang == "shell" || lang == "console" || lang == "fish";
    const bool data = lang == "json" || lang == "yaml" || lang == "yml" || lang == "toml" || lang == "ini";
    const bool hashComment = py || sh || lang == "yaml" || lang == "yml" || lang == "toml" || lang == "ini" || lang == "cmake" || lang == "make" || lang == "ruby";
    const bool plain = lang.empty() || lang == "text" || lang == "txt" || lang == "md" || lang == "markdown" || lang == "diff" || lang == "log";
    const std::set<std::string>& keywords = py ? python : sh ? shell : clike;
    std::vector<cluster::Cell> out;
    const std::function<void(const std::string&, std::uint32_t)> put = [&out](const std::string& text, const std::uint32_t color) {
        if (text.empty()) return;
        if (!out.empty() && out.back().fg == color) out.back().text += text;
        else out.push_back({text, color, 0, true, false});
    };
    if (plain) {
        const std::uint32_t color = lang == "diff" && line.starts_with("+") ? p.string : lang == "diff" && line.starts_with("-") ? 0xf7768e
            : lang == "diff" && line.starts_with("@@") ? p.keyword : p.text;
        put(line, color);
        return out;
    }
    std::size_t i = 0;
    while (i < line.size()) {
        if (inComment) { // inside /* ... */
            const std::size_t end = line.find("*/", i);
            put(line.substr(i, end == std::string::npos ? std::string::npos : end + 2 - i), p.comment);
            if (end == std::string::npos) return out;
            inComment = false;
            i = end + 2;
            continue;
        }
        const char c = line[i];
        if (!hashComment && !data && line.compare(i, 2, "/*") == 0) {
            inComment = true;
            continue;
        }
        if ((!hashComment && !data && line.compare(i, 2, "//") == 0) || (hashComment && c == '#' && !(sh && i > 0 && line[i - 1] == '$'))) {
            put(line.substr(i), p.comment);
            return out;
        }
        if (c == '"' || c == '\'' || c == '`') {
            std::size_t end = i + 1;
            while (end < line.size() && line[end] != c) end += line[end] == '\\' ? 2 : 1;
            end = std::min(line.size(), end + 1);
            put(line.substr(i, end - i), data && line.find(':', end) != std::string::npos && c == '"' ? p.keyword : p.string);
            i = end;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) && (i == 0 || !std::isalnum(static_cast<unsigned char>(line[i - 1])))) {
            std::size_t end = i;
            while (end < line.size() && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '.' || line[end] == '_')) ++end;
            put(line.substr(i, end - i), p.number);
            i = end;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || (c == '#' && !hashComment) || (c == '$' && sh)) {
            std::size_t end = i + 1;
            while (end < line.size() && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '_' || (sh && line[end] == '-'))) ++end;
            const std::string word = line.substr(i, end - i);
            std::size_t after = end;
            while (after < line.size() && line[after] == ' ') ++after;
            const std::uint32_t color = keywords.contains(word) ? p.keyword : c == '$' ? p.number
                : (data && (word == "true" || word == "false" || word == "null")) ? p.number
                : after < line.size() && line[after] == '(' ? p.function
                : !py && !sh && std::isupper(static_cast<unsigned char>(c)) ? p.type : p.text;
            put(word, color);
            i = end;
            continue;
        }
        put(std::string(1, c), p.text);
        ++i;
    }
    return out;
}

_nodiscard double luminance_(const std::string& hex)
{
    // WCAG relative luminance of "#rrggbb"
    if (hex.size() != 7 || hex[0] != '#') return 0.0;
    double sum = 0.0;
    const double weight[3] = {0.2126, 0.7152, 0.0722};
    for (int k = 0; k < 3; ++k) {
        const double v = std::stoi(hex.substr(1 + 2 * static_cast<std::size_t>(k), 2), nullptr, 16) / 255.0;
        sum += weight[k] * (v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4));
    }
    return sum;
}

_nodiscard double contrast_(const std::string& a, const std::string& b)
{
    const double la = luminance_(a);
    const double lb = luminance_(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

_cold Theme make_theme_(const cluster::Style& style)
{
    return {hex_(style.bg), hex_(style.surface), hex_(style.text), hex_(style.muted), hex_(style.accent), hex_(style.border),
        hex_(style.ok), hex_(style.warn), hex_(style.error), style.dark,
        hex_(contrast_(style.ok, style.bg) >= contrast_(style.ok, style.text) ? style.bg : style.text)};
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
// at its place): no flicker, no cursor running over the screen. A frame identical to the one before is not written at
// all: the refresh timers redraw it every second (or every 40 ms while animated), and the hide / show of the cursor
// around each of them restarted its blink
class FrameOut: public std::streambuf {
    private:
        static constexpr std::size_t FRAME_MIN = 64; // under this size: a control sequence, always written

        std::string _buffer;
        std::string _last;                  // the last frame written

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
            if (this->_buffer.size() >= FRAME_MIN && this->_buffer == this->_last) {
                this->_buffer.clear();
                return 0;
            }
            this->_last = this->_buffer.size() >= FRAME_MIN ? this->_buffer : std::string();
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

static constexpr int GRID_MIN_WIDTH = 38;   // a cell of the grid: its smallest size, the rows that do not fit scroll
static constexpr int GRID_MIN_HEIGHT = 8;
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

_cold std::string code_slice_(const std::string& raw, const int from, const int to)
{
    // the part of a code line (tabs kept) drawn on the cells [from, to) (a tab is drawn as 4 cells, a character as 1)
    std::string out;
    int column = 0;
    for (std::size_t at = 0; at < raw.size() && column < to;) {
        std::size_t end = at + 1;
        while (end < raw.size() && (static_cast<unsigned char>(raw[end]) & 0xC0) == 0x80)
            ++end;
        const int size = raw[at] == '\t' ? 4 : 1;
        if (column + size > from) out += raw.substr(at, end - at);
        column += size;
        at = end;
    }
    return out;
}

_cold std::string cut_(const std::string& line, const std::size_t columns)
{
    // a line cut to its first columns, its spaces kept (an indentation, an alignment), … when cut
    if (width_(line) <= columns || columns == 0) return line;
    return line.substr(0, byte_at_(line, static_cast<int>(columns) - 1)) + "…";
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
    static const std::map<std::string, std::string> pages = {{"\x1b[5;2~", "Shift+PageUp"}, {"\x1b[6;2~", "Shift+PageDown"}, {"\x1b[1;5H", "Ctrl+Home"}, {"\x1b[1;5F", "Ctrl+End"}, // xstyle: ignore LU-ANSI (read)
        {"\x1b[7;5~", "Ctrl+Home"}, {"\x1b[8;5~", "Ctrl+End"}, // xstyle: ignore LU-ANSI (read)
        {"\x1b[13;2u", "Shift+Enter"}, {"\x1b[27;2;13~", "Shift+Enter"}, // xstyle: ignore LU-ANSI (read)
        {"\x1b[1;3A", "Alt+Up"}, {"\x1b[1;3B", "Alt+Down"}, {"\x1b[1;3C", "Alt+Right"}, {"\x1b[1;3D", "Alt+Left"}, {"\x1b\x1b[A", "Alt+Up"}, {"\x1b\x1b[B", "Alt+Down"}, // xstyle: ignore LU-ANSI (read)
        {"\x1b\x1b[C", "Alt+Right"}, {"\x1b\x1b[D", "Alt+Left"}, {"\x1b[5;3~", "Alt+PageUp"}, {"\x1b[6;3~", "Alt+PageDown"}, {"\x1b\x1b[5~", "Alt+PageUp"}, // xstyle: ignore LU-ANSI (read)
        {"\x1b\x1b[6~", "Alt+PageDown"}, {"\x1b[1;3H", "Alt+Home"}, {"\x1b[1;3F", "Alt+End"}, {"\x1b[7;3~", "Alt+Home"}, {"\x1b[8;3~", "Alt+End"}, // xstyle: ignore LU-ANSI (read)
        {"\x1b\x1b[H", "Alt+Home"}, {"\x1b\x1b[F", "Alt+End"}}; // xstyle: ignore LU-ANSI (read)
    if (pages.contains(input)) return pages.at(input);
    if (input.size() == 1 && input[0] >= 1 && input[0] <= 26 && input[0] != 9 && input[0] != 10 && input[0] != 13)
        return std::string("Ctrl+") + static_cast<char>('A' + input[0] - 1);
    if (input.size() == 2 && input[0] == 27 && std::isalnum(static_cast<unsigned char>(input[1])))
        return std::string("Alt+") + static_cast<char>(std::toupper(static_cast<unsigned char>(input[1])));

    // the function keys with modifiers: "ESC[21;3~" (Alt+F10), "ESC[1;5P" (Ctrl+F1), and Alt sent as a leading ESC ("ESC ESC[21~")
    {
        static const std::map<int, int> tilde = {{11, 1}, {12, 2}, {13, 3}, {14, 4}, {15, 5}, {17, 6}, {18, 7}, {19, 8}, {20, 9}, {21, 10}, {23, 11}, {24, 12}};
        static const std::regex modTilde("^\x1b\\[(\\d+);(\\d+)~$");
        static const std::regex modSs3("^\x1b\\[1;(\\d+)([PQRS])$");
        static const std::regex altTilde("^\x1b\x1b\\[(\\d+)~$");
        static const std::regex altSs3("^\x1b\x1bO([PQRS])$");
        std::smatch found;
        int number = 0;
        int modifier = 1;
        if (std::regex_match(input, found, modTilde) && tilde.contains(std::stoi(found[1].str()))) {
            number = tilde.at(std::stoi(found[1].str()));
            modifier = std::stoi(found[2].str());
        } else if (std::regex_match(input, found, modSs3)) {
            number = 1 + (found[2].str()[0] - 'P');
            modifier = std::stoi(found[1].str());
        } else if (std::regex_match(input, found, altTilde) && tilde.contains(std::stoi(found[1].str()))) {
            number = tilde.at(std::stoi(found[1].str()));
            modifier = 3;
        } else if (std::regex_match(input, found, altSs3)) {
            number = 1 + (found[1].str()[0] - 'P');
            modifier = 3;
        }
        if (number > 0 && modifier >= 2) {
            const int bits = modifier - 1;
            return std::string(bits & 4 ? "Ctrl+" : "") + (bits & 1 ? "Shift+" : "") + (bits & 2 ? "Alt+" : "") + "F" + std::to_string(number);
        }
    }

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
        const bool _webTty = std::getenv("CLAUDE_CLUSTER_WEB_TTY") != nullptr; // run for the terminal page: its rules (the shell, the settings, the services, the keys)
        cluster::Manager& _manager;
        ftxui::ScreenInteractive _screen = ftxui::ScreenInteractive::Fullscreen();
        std::set<std::string> _known;                                   // the /words of the session shown (coloured in the prompt)
        std::pair<std::string, std::uint64_t> _knownKey;                // the session and the version of the sessions they were read at
        Theme _theme{};
        Mode _mode = Mode::Normal;
        cluster::GraphView _graph;                  // the graph view (Mode::Graph): the sessions around global
        bool _graphHub = false;                     // Mode::Graph as the hub: every session around global (else the graph: the taxonomy between)
        int _treeSel = 0;                           // Mode::Tree: the session under the cursor (the sessions by folder)
        std::vector<std::string> _treeIds;          // its rows that are sessions, in their order
        std::string _active = GLOBAL_ID;
        cluster::Editor _editor;                    // the prompt
        std::vector<std::string> _historyItems;     // Mode::History, filtered
        bool _historyAll = false;                   // Mode::History: every session (Tab), else the one shown
        std::vector<std::size_t> _chips;            // atoms of the prompt shown as chips (images, pasted texts)
        std::vector<ftxui::Box> _chipBoxes;
        int _hoverChip = -1;
        int _hover = -1;                            // the click target under the mouse (its index in _clicks), lit
        std::string _hitView;                       // the part of a display under the mouse: its transcript, line, first cell
        int _hitY = -1;
        int _hitFrom = -1;
        std::string _pressView;                     // the part of a display clicked last: flashed a moment
        int _pressY = -1;
        int _pressFrom = -1;
        std::int64_t _pressAt = 0;
        std::string _formSession;                   // the form of a display in its popup: where, its fields, the one focused
        std::string _formDisplay;
        std::string _formTitle;
        std::vector<FormField> _form;
        std::size_t _formFocus = 0;                 // a field, then Submit (size), Cancel (size + 1)
        std::map<std::string, std::int64_t> _audioStart; // an audio playing here: when it started (its display id)
        std::string _upgradeLater;                  // the newer version put off (Esc): not asked again in this run
        std::vector<cluster::PluginRepo> _plug;     // the plugins browser: the sources listed (the index of the core)
        std::int64_t _plugRead = 0;
        std::size_t _plugSel = 0;                   // its row (a source, an item)
        bool _plugTree = false;                     // the focus in the tree of the files of the item
        std::set<std::string> _plugOpen;            // the folders opened in the tree
        std::size_t _plugNode = 0;
        std::string _plugFile;                      // the file previewed
        int _plugScroll = 0;
        bool _plugAdd = false;                      // a source typed (a path completed, a link)
        std::vector<std::string> _plugHints;
        std::map<int, std::string> _rowSessions;    // the click targets that are sessions (a right click: their menu)
        struct PromptRow {
            std::size_t from = 0;                   // first byte shown
            std::size_t end = 0;
            int offset = 3;                         // cells before the text (" › ", "…")
        };
        std::vector<PromptRow> _promptRows;         // the rows of the prompt drawn (mouse -> position)
        std::vector<cluster::VisualRow> _visualRows;    // every row of the prompt as wrapped at the last frame (Up / Down by row)
        cluster::VisualGoal _visualGoal;            // the column kept by Up / Down from one row to the other
        ftxui::Box _promptBox;
        ftxui::Box _panelBox;                       // the side panels of the session shown (the wheel over them scrolls them)
        int _panelScroll = 0;                       // lines hidden at their top
        ftxui::Box _listBox{0, -1, 0, -1};          // the list of sessions (list layout): the wheel over it scrolls it
        std::size_t _listFirst = 0;                 // its first session shown
        std::size_t _listRoom = 1;                  // the sessions it shows (a page)
        ftxui::Box _gridBox{0, -1, 0, -1};          // the grid of sessions: the wheel over it scrolls its rows when they do not all fit
        int _gridFirst = 0;                         // its first row shown
        int _gridColumns = 1;                       // its columns, rows shown (a page), rows in all, as drawn last
        int _gridVisible = 1;
        int _gridRows = 1;
        std::string _listSeen;                      // the session shown when the list / the grid last followed it (it moved: its row is kept in view)
        std::string _gridSeen;
        CodePalette _code;                          // the colours of the code boxes (from the theme)
        ftxui::Box _askChatBox;                     // the chat of a question popup (the wheel over it scrolls it)
        int _askChatScroll = 0;                     // its lines hidden below (0: the newest shown)
        std::size_t _askChatSeen = 0;               // its messages when drawn last (a new one: back to the newest)
        int _panelHidden = 0;                       // lines below the room at the last drawing
        using Hit = std::tuple<int, int, std::function<void(void)>>;   // the cells [from, to] of a line clicked: what it does
        struct CodeRow {
            // a row of a fenced code block: what the selection copies from it (the box and its padding are not text)
            int block = -1;                         // the block (-1: not a row of a block)
            bool text = false;                      // a line of code (false: the border / header of the box)
            bool button = false;                    // the header with the copy button
            int from = 0;                           // the cells of the code text [from, to)
            int to = 0;
            std::string raw;                        // the code of this row as written (tabs kept; a wrapped line: its part)
        };
        struct View {
            ftxui::Box box;
            std::vector<std::string> lines;         // transcript lines shown (mouse selection)
            std::vector<std::string> joins;
            std::vector<CodeRow> code;              // per line: the code block it belongs to
            std::vector<std::vector<Hit>> hits;     // per line: its parts clicked (a display: tick, a button, play...)
            std::vector<std::size_t> entries;       // per line: its entry of the transcript (npos: none), for the right click (Pin)
        };
        std::map<std::string, View> _views;         // per session
        std::set<std::string> _pinFolded;           // the sessions whose pinned item is collapsed
        std::size_t _transcriptLines = 0;           // the lines of the transcript made last (the height the pinned item needs)
        struct Selection {
            std::string session;
            int x0 = 0;                             // cells, from the top left of the transcript
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            int block = -1;                         // started in a code block: stays inside its code text
        };
        std::string _copiedBlock;                   // the code block just copied (its button says copied for a while)
        std::int64_t _copiedAt = 0;
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
        // a line typed in a popup, edited like the prompt (cursor, selection, words, kill ring, undo, mouse): the answer on Other, the chat draft
        using Line = cluster::LineEditor;
        // the text fields that are plain strings (filters, names, settings, forms...): one editor follows the field being typed in
        mutable cluster::LineEditor _field;
        mutable const void* _fieldOf = nullptr;
        mutable std::vector<cluster::LineEditor*> _fieldsDrawn;     // the fields drawn focused by this frame (the mouse)
        Line _askDraft{true};           // the chat about the questions: several lines (Alt+Enter)
        Line _askOther;                 // the answer on Other, edited here, pushed to the question at each change
        Line _plugInput;                // the source typed in the plugins browser
        std::string _askOtherKey;       // the question it belongs to (request:index)
        std::string _host = cluster::host_name();   // where the sessions run
        std::map<std::string, std::string> _branches;   // their git branch, read again every 5 s
        std::map<std::string, std::int64_t> _branchAt;
        std::uint64_t _raiseSeen = 0;               // the last notification clicked
        std::string _webAsk;                        // the device asking to connect shown (its popup)
        std::set<std::string> _webLater;            // the ones put off (Esc): asked again from the palette
        std::set<std::string> _webPicked;           // the web devices ticked ("d:<id>" connected / waiting, "t:<id>" trusted)
        int _webCursor = 0;
        std::string _webConfirm;                    // a dangerous action of the devices waiting for its y (ban, kick_all)
        std::int64_t _lastEscapeAt = 0;             // takeBack_: ms of the Esc that asked for a second one
        cluster::RewindChooser _rewind;             // Mode::Rewind: the chooser of the point of the conversation to go back to (Esc)
        cluster::StashPopup _stash;                 // Mode::Stash: the list of the stashed prompts (Ctrl+L)
        std::int64_t _lastInterrupt = 0;            // Ctrl+C Ctrl+C on an empty prompt, nothing running: quit
        std::uint64_t _machineSeen = 0;             // the map of this computer asked by the global session (its question shown)
        std::atomic<bool> _sendPending{false};      // Enter waits a few ms: a key right after it is a pasted line break
        std::int64_t _sendAt = 0;                   // ms of that Enter
        std::optional<std::string> _pasting;        // bracketed paste of the terminal: its text until ESC[201~
        cluster::Worker _worker;                    // the sends / commands, in their order, out of the drawing thread
        cluster::Reveal _reveal;                    // the answers typed as they come
        std::atomic<bool> _animating{false};        // typing / text reveal: drawn at 25 images / s
        std::atomic<bool> _spinning{false};         // spinners / popups: drawn at their pace (8 images / s)
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
        int _upgradeFocus = 0;                      // the choice of the update popup under the cursor: 0 Update, 1 Ignore, 2 Quit (arrows, Tab, Enter)
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
            std::cout << "\x1b[?2004h\x1b[?1004h\x1b[>4;1m" << std::flush; // + Shift+Enter told apart from Enter (modifyOtherKeys level 1: only the keys that would be ambiguous) // xstyle: ignore LU-ANSI (modes of the terminal, no color)
        }

        void settingsAction_(const std::string& key)
        {
            // an action of the settings page: the map of this computer (asked first), the system prompt of the global
            // session or of the sub-sessions, the showcase of the Debug page
            if (key.starts_with("account_")) {
                // a provider: its key stored / removed, its login run (the settings left for the prompt, the key asked without echo)
                if (!this->_changes.empty()) {
                    this->_note = "save (Ctrl+S) or drop your changes first";
                    return;
                }
                const bool remove = key.starts_with("account_rm_");
                const std::string name = key.substr(remove ? 11 : 8);
                const std::optional<cluster::Provider> provider = this->_manager.auth().find(name);
                if (!provider) return;
                if (remove) {
                    this->run_({"auth_logout:" + name, "", "", "", false}, "");
                } else if (provider->auth == cluster::AuthKind::Key) {
                    this->_pending = {"auth_key:" + name, "Provider " + name + ": store the API key", "API key", "", false};
                    this->_prompt.clear();
                    this->_password = true;
                    this->_mode = Mode::Prompt;
                } else {
                    this->run_({"auth_login:" + name, "", "", "", false}, "");
                }
                return;
            }
            if (key.starts_with("showcase_")) {
                if (!this->_changes.empty()) {
                    this->_note = "save (Ctrl+S) or drop your changes first: the showcase leaves the settings";
                    return;
                }
                const std::string platform = std::getenv("CLAUDE_CLUSTER_WEB_TTY") != nullptr ? "web terminal" : "terminal";
                this->_mode = Mode::Normal; // the settings closed: the showcase seen where it happens
                std::thread([this, what = key.substr(9), platform]() {
                    try {
                        const std::string done = cluster::showcase(this->_manager, what, platform);
                        this->_screen.Post([this, done]() {this->status_(done);});
                    } catch (const utils::exception::IException& e) {
                        const std::string error = e.info();
                        this->_screen.Post([this, error]() {this->status_(error, true);});
                    }
                    this->_screen.PostEvent(ftxui::Event::Custom);
                }).detach();
                return;
            }
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

        void rewind_(const int back = 1)
        {
            // the last `back` prompts leave the conversation (claude started again: in the worker), the oldest of them comes back
            const std::string session = this->_active;
            this->status_("going back in the conversation…");
            this->_worker.push([this, session, back]() {
                std::optional<cluster::Queued> prompt;
                std::string error;
                try {
                    prompt = this->_manager.rewind(session, back);
                } catch (const utils::exception::IException& e) {
                    error = e.info();
                }
                this->_screen.Post([this, prompt, error, session]() {
                    if (!prompt) {
                        this->status_(error, true);
                        return;
                    }
                    if (!cluster::trim(this->_editor.text()).empty()) (void)this->_manager.stashPush(this->_editor.draft(), session); // typed meanwhile: kept
                    this->_editor.load({prompt->text, prompt->images, {}});
                    this->historyReset_();
                    this->status_("back in the conversation: the prompt is here to edit (Esc on an empty prompt: an earlier point)");
                });
                this->_screen.PostEvent(ftxui::Event::Custom);
            });
        }

        void takeBackRunning_(void)
        {
            // the turn runs: it is interrupted and its prompt comes back in the editor, the conversation goes on from before it
            // (rewind); a session that cannot rewind (not Claude Code, not in its transcript yet): interrupted, its text back
            const std::string session = this->_active;
            std::string last;
            if (const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(session)) {
                const auto user = std::find_if(snap->entries.rbegin(), snap->entries.rend(), [](const cluster::Entry& e) {return e.kind == cluster::EntryKind::User;});
                if (user != snap->entries.rend()) last = user->text;
            }
            this->status_("taking the prompt back…");
            this->_worker.push([this, session, last]() {
                std::optional<cluster::Queued> prompt;
                std::string error;
                try {
                    prompt = this->_manager.rewind(session, 1);
                } catch (const utils::exception::IException& e) {
                    error = e.info();
                }
                if (!prompt) this->_manager.interrupt(session);
                this->_screen.Post([this, prompt, error, last, session]() {
                    if (!prompt && last.empty()) {
                        this->status_(error, true);
                        return;
                    }
                    if (!cluster::trim(this->_editor.text()).empty()) (void)this->_manager.stashPush(this->_editor.draft(), session);
                    if (prompt) this->_editor.load({prompt->text, prompt->images, {}});
                    else this->_editor.setText(last);
                    this->historyReset_();
                    this->status_(prompt ? "prompt taken back" : "prompt taken back (interrupted; it stays in the conversation)");
                });
                this->_screen.PostEvent(ftxui::Event::Custom);
            });
        }

        void takeBack_(void)
        {
            // Esc. A queued prompt: back in the editor. A turn running: interrupted, its prompt back in the editor. Nothing running and
            // the editor empty: the chooser of the point of the conversation to go back to. Nothing running and a prompt being
            // written: Esc Esc clears it (the first one tells so)
            const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(this->_active);
            const std::optional<cluster::Queued> queued = snap ? this->_manager.unqueue(this->_active) : std::nullopt;
            const bool empty = cluster::trim(this->_editor.text()).empty();
            if (queued) {
                if (!empty) (void)this->_manager.stashPush(this->_editor.draft(), this->_active);
                this->_editor.setText(queued->text);
                this->_editor.setImages(queued->images);
                this->status_("prompt taken back (Enter: queue it again)");
            } else if (snap && (snap->state == cluster::State::Working || snap->state == cluster::State::Waiting)) {
                this->takeBackRunning_();
            } else if (!empty) {
                if (now - this->_lastEscapeAt < 600) {
                    this->_lastEscapeAt = 0;
                    this->_editor.clear();
                    this->historyReset_();
                } else {
                    this->_lastEscapeAt = now;
                    this->status_("Esc again: clear the prompt (" + this->_manager.config().keys.at("stash") + ": stash it instead)");
                }
                return;
            } else if (snap) {
                this->openRewind_();
            }
            this->_lastEscapeAt = 0;
        }

        void openRewind_(void)
        {
            // the chooser of the points of the conversation (its prompts, newest first) to go back to
            const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(this->_active);
            if (snap && this->_rewind.open(snap->entries)) this->_mode = Mode::Rewind;
            else this->status_("no earlier prompt to go back to");
        }

        bool rewindKey_(const ftxui::Event& event)
        {
            const cluster::RewindResult result = this->_rewind.key(event);
            if (!result.status.empty()) this->status_(result.status);
            if (!this->_rewind.active() && this->_mode == Mode::Rewind) this->_mode = Mode::Normal;
            if (result.back > 0) this->rewind_(result.back);
            return true;
        }

        bool stashKey_(const ftxui::Event& event)
        {
            const cluster::StashResult result = this->_stash.key(event, cluster::trim(this->_editor.text()).empty());
            if (!result.status.empty()) this->status_(result.status, result.error);
            if (result.load) {
                if (result.stashCurrent && !cluster::trim(this->_editor.text()).empty()) (void)this->_manager.stashPush(this->_editor.draft(), this->_active);
                if (result.append) cluster::append_draft(this->_editor, *result.load);
                else this->_editor.load(*result.load);
                this->historyReset_();
            }
            if (!this->_stash.active() && this->_mode == Mode::Stash) this->_mode = Mode::Normal;
            return true;
        }

        _nodiscard bool rowsKnown_(void) const {return cluster::visual_rows_valid(this->_visualRows, this->_editor.text());};

        _nodiscard bool onFirstRow_(void) const {return this->rowsKnown_() ? cluster::visual_row_of(this->_visualRows, this->_editor.cursor()) == 0 : this->_editor.onFirstLine();};

        _nodiscard bool onLastRow_(void) const {return this->rowsKnown_() ? cluster::visual_row_of(this->_visualRows, this->_editor.cursor()) + 1 >= this->_visualRows.size() : this->_editor.onLastLine();};

        bool visualStep_(const int dy, const bool select)
        {
            // Up / Down: the cursor to the row above / below as the prompt is wrapped on the screen, the column kept; false: not
            // possible (first / last row: the history takes it, or the rows of the last frame do not match the text)
            if (!this->rowsKnown_()) return false;
            const std::optional<std::size_t> to = cluster::visual_move(this->_editor, this->_visualRows, dy, this->_visualGoal);
            if (!to) return false;
            this->_editor.place(*to, select);
            return true;
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
        struct Anchor {
            bool valid = false;
            int scroll = 0;                         // the scroll kept at the last drawing (another value: the user moved, the anchor is dropped)
            std::size_t entry = 0;                  // the entry of the first line shown (npos: the live tail after the entries)
            int offset = 0;                         // its line inside that entry
            int fresh = 0;                          // lines added below the view since it left the end
        };
        std::map<std::string, Anchor> _anchors;     // per session: the first line shown stays where it is when text is added or resized
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
        std::vector<cluster::Action> _actionBase;       // the palette: every action, made once per opening
        std::string _actionBaseFor;                     // the session it was made for
        std::int64_t _actionBaseAt = 0;                 // when (ms)
        bool _suggestOpen = false;                      // the settings: the suggestions of the value typed, one chosen with the arrows
        int _suggest = 0;                               // the one chosen
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
        _nodiscard bool grouped_(void) const {return this->_manager.ui("group_by_category", this->_manager.config().groupByCategory ? "true" : "false") == "true";};
        _nodiscard std::vector<cluster::Snapshot> sessions_(void) const {return this->_manager.shown();};

        // a chip of a label: a color made from its text (the same label, the same color everywhere)
        _nodiscard static ftxui::Color chip_color_(const std::string& label)
        {
            std::uint32_t hash = 2166136261U;
            for (const char c: label)
                hash = (hash ^ static_cast<unsigned char>(c)) * 16777619U;
            return ftxui::Color::HSV(static_cast<std::uint8_t>(hash % 256), 140, 235);
        }

        void quit_(void)
        {
            // sessions running (a turn, prompts queued): asked what to do with them ([ui] quit_background), else the quit
            const std::string& mode = this->_manager.config().quitBackground;
            if (mode != "never" && this->_manager.busy()) {
                if (mode == "ask") {
                    this->_mode = Mode::QuitAsk;
                    return;
                }
                this->_manager.requestBackground();
            }
            this->_screen.Exit();
        }

        void openPicture_(const std::string& path)
        {
            // the viewer of the desktop (ui.image_viewer, else xdg-open)
            const std::string viewer = cluster::trim(this->_manager.config().imageViewer);
            std::vector<std::string> command = viewer.empty() ? std::vector<std::string>{"xdg-open"} : cluster::split(viewer, ' ');
            command.push_back(path);
            cluster::detach(command);
            this->status_("opened " + std::filesystem::path(path).filename().string());
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
            // Session on the left / right / above / below in the current layout (grid: rows and columns; list: previous / next)
            const std::vector<cluster::Snapshot> list = this->sessions_();
            const int count = static_cast<int>(list.size());
            const std::string layout = this->_manager.ui("layout", this->_manager.config().layout);
            const int columns = layout == "grid" && count >= 1 ? this->_gridColumns : 1;
            if (count == 0 || columns == 1) {
                this->move_(dx != 0 ? dx : dy);
                return;
            }
            // the grid: the sessions, then the Tasks as the last cell; left / right turn inside the row (the last one has fewer cells), up / down inside
            // the column, from the top to the bottom and back; under a shorter last row, its last cell
            const int cellCount = count + 1;
            auto it = std::find_if(list.begin(), list.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->_active;});
            const int index = this->_active == TASKS_ID ? count : it == list.end() ? 0 : static_cast<int>(it - list.begin());
            const int rows = (cellCount + columns - 1) / columns;
            int row = index / columns;
            int column = index % columns;
            if (dx != 0) {
                const int cells = std::min(columns, cellCount - row * columns);
                column = (column + dx + cells) % cells;
            } else {
                row = (row + dy + rows) % rows;
                column = std::min(column, cellCount - row * columns - 1);
            }
            const int target = row * columns + column;
            this->_active = target == count ? std::string(TASKS_ID) : list[static_cast<std::size_t>(target)].spec.id;
        }

        bool gridArrow_(const Arrow& arrow)
        {
            // grid, empty prompt: the arrows move between the cells (a prompt being written: they edit it as ever; Alt + arrows always move)
            if (arrow.shift || arrow.ctrl || !this->_editor.text().empty() || this->_editor.completing()) return false;
            if (this->_manager.ui("layout", this->_manager.config().layout) != "grid" || this->sessions_().empty()) return false;
            this->moveDir_(arrow.dx, arrow.dy);
            return true;
        }

        void sessionMove_(const std::string& action)
        {
            // Alt+arrows: the session around; Alt+PageUp / PageDown: a page of the list / the grid; Alt+Home / End: the first / the last
            if (action == "session_up" || action == "session_down" || action == "session_left" || action == "session_right") {
                this->moveDir_(action == "session_left" ? -1 : action == "session_right" ? 1 : 0, action == "session_up" ? -1 : action == "session_down" ? 1 : 0);
                return;
            }
            const std::vector<cluster::Snapshot> list = this->sessions_();
            if (list.empty()) return;
            const int count = static_cast<int>(list.size());
            const bool grid = this->_manager.ui("layout", this->_manager.config().layout) == "grid" && count >= 1;
            const int page = grid ? this->_gridVisible * this->_gridColumns : static_cast<int>(this->_listRoom);
            auto it = std::find_if(list.begin(), list.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->_active;});
            const int at = it == list.end() ? 0 : static_cast<int>(it - list.begin());
            const int target = action == "session_first" ? 0 : action == "session_last" ? count - 1 : action == "session_page_up" ? std::max(0, at - page) : std::min(count - 1, at + page);
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

        _nodiscard cluster::LineColors lineColors_(void) const {return {this->_theme.text, this->_theme.accent, this->_theme.bg, this->_theme.muted};};

        cluster::LineEditor& field_(const std::string& value) const
        {
            // the editor of a string field: kept while it is the same field and nobody else changed it, else loaded with it (the cursor at its end)
            if (this->_fieldOf != &value || this->_field.text() != value) {
                this->_field.set(value);
                this->_fieldOf = &value;
            }
            return this->_field;
        }

        bool fieldKey_(std::string& value, const ftxui::Event& event)
        {
            // an editing key on a string field (cursor, words, kill ring, undo, selection, clipboard, characters): true when it was one
            cluster::LineEditor& editor = this->field_(value);
            if (!editor.key(event)) return false;
            value = editor.text();
            return true;
        }

        _nodiscard ftxui::Element lineView_(cluster::LineEditor& editor, const int width, const int rows, const bool focused = true) const
        {
            // drawn in the frame (the cursor is a cell, never the one of the terminal); focused: the mouse can place the cursor in it
            if (focused) this->_fieldsDrawn.push_back(&editor);
            return editor.render(width, rows, this->lineColors_(), focused);
        }

        _nodiscard ftxui::Element fieldView_(const std::string& value, const int width, const bool focused = true) const {return this->lineView_(this->field_(value), width, 1, focused);};

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
                    try { // stored with the attachments of the session (caps, EXIF, purge): the same model as the window and the page
                        const cluster::Attachment item = cluster::attach_store(this->_manager.config(), this->_active, "clipboard", image.out);
                        if (!this->_editor.addImage(item.path)) this->status_("the same image: one more reference to it (sent once)");
                    } catch (const utils::exception::IException& e) {
                        this->status_(std::string(e.info()).empty() ? std::string(e.what()) : std::string(e.info()), true);
                    }
                    return;
                }
            }
            const cluster::Captured text = cluster::capture({"wl-paste", "--no-newline"}, "", 5);
            if (text.code == 0) this->_editor.paste(text.out);
            else this->status_("clipboard unreadable (wl-paste)", true);
        }

        void enter_(void)
        {
            // sent when no key follows it (Custom event); a key right after it (a paste without brackets): a new line. Never
            // judged by the keys before it: typed keys handled in a burst (the interface busy a moment) look like a paste
            this->_sendAt = now_ms_();
            this->_sendPending = true;
        }

        void submit_(void)
        {
            // the prompt leaves at once (feedback), the send / the local command runs in the worker (it may start or stop
            // a claude: seconds), its result comes back here; an error: the prompt back, nothing lost
            const std::string text = cluster::trim(this->_editor.plain());
            if (text.empty()) { // an empty prompt: the voice prompt waiting sent now
                const std::vector<cluster::Pending> voiced = this->_app.voice->pending();
                if (std::any_of(voiced.begin(), voiced.end(), [](const cluster::Pending& p) {return !p.target.empty();})) {
                    this->run_({"voice_confirm", "", "", "", false}, "");
                    this->status_("voice prompt sent");
                }
                return;
            }
            if (this->_active == TASKS_ID) return; // the Tasks session: no prompt
            if (this->_webTty && (text.starts_with("!") || (text.starts_with(":") && !text.starts_with("::") && text.find('!') != std::string::npos) || text.starts_with(":r ")
                || text.starts_with(":read ")) && !this->_manager.config().web.shell) {
                this->status_("the shell from the page is off (settings of the computer: Web > Shell from the page)", true);
                return;
            }
            if (this->_webTty && (text.starts_with(":set") || text.starts_with(":se "))) {
                const std::size_t equal = text.find('=');
                const std::string key = equal == std::string::npos ? "" : cluster::trim(text.substr(text.find(' ') + 1, equal - text.find(' ') - 1));
                const std::string section = key.find('.') != std::string::npos ? key.substr(0, key.find('.')) : "";
                if (equal != std::string::npos && (section.empty() || !cluster::web_may(this->_manager.config(), section) || key == "web.settings")) {
                    this->status_("read only from the web page (its section, e.g. ui.layout; the computer allows it: Web > Settings from the page)", true);
                    return;
                }
            }
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
                    else if (local->ui == "quit") this->quit_();
                    else if (local->ui.starts_with("search:")) {
                        this->_search = local->ui.substr(7);
                        this->_matches.clear();
                        this->_hit = 0;
                        this->_mode = Mode::Search;
                        this->searchUpdate_();
                    }
                    if (local->ui == "settings") this->openSettings_();
                    else if (local->ui == "plugins") this->openPlugins_();
                    else if (local->ui == "graph") this->openGraph_(false);
                    else if (local->ui.starts_with("term:")) {
                        this->handOver_(local->ui.substr(5));
                        return;
                    } else if (local->ui == "message") { // a local command shown (/usage, /skills...): never in the conversation
                        this->_message = local->message;
                        this->_messageError = local->error;
                        this->_mode = Mode::Message;
                        return;
                    } else if (local->ui == "restore_menu") this->openRestore_();
                    else if (local->ui == "rewind") this->openRewind_();
                    else if (local->ui.starts_with("pick:")) this->pick_(local->ui.substr(5));
                    if (!local->message.empty()) this->status_(local->message, local->error);
                    else if (local->ui.empty()) this->status_("done");
                });
                this->_screen.PostEvent(ftxui::Event::Custom);
            });
        }

        _nodiscard const std::set<std::string>& slashKnown_(const std::string& text)
        {
            // the /words that exist for the session shown, read again when the sessions changed (never for a prompt with no /word)
            static const std::set<std::string> none;
            if (text.find('/') == std::string::npos) return none;
            const std::pair<std::string, std::uint64_t> key{this->_active, this->_manager.version()};
            if (key != this->_knownKey) {
                this->_knownKey = key;
                this->_known = cluster::slash_known(this->_manager, this->_active);
            }
            return this->_known;
        }

        _nodiscard ftxui::Element prompt_(const int width)
        {
            // the prompt (several lines: Alt+Enter / Shift+Enter): selection inverted, cursor, 8 lines around the cursor
            this->_promptRows.clear();
            this->_visualRows.clear();
            if (this->_mode == Mode::Search) {
                return ftxui::hbox({ftxui::text(" find › ") | ftxui::color(this->_theme.warn) | ftxui::bold, this->fieldView_(this->_search, std::max(10, width - 60)),
                    ftxui::text("  " + (this->_matches.empty() ? std::string("no match") : std::to_string(this->_hit + 1) + "/" + std::to_string(this->_matches.size()))
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
            const char mode = text.starts_with("!") ? '!' : text.starts_with(":") && !text.starts_with("::") ? ':' : '\0';
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
            for (const Row& row: lines)
                this->_visualRows.push_back({row.start, row.end, row.last});
            std::size_t current = 0;
            for (std::size_t i = 0; i < lines.size(); ++i)
                if (cursor >= lines[i].start && (cursor < lines[i].end || (cursor == lines[i].end && lines[i].last))) current = i;
            const std::size_t first = current >= 7 ? current - 7 : 0;
            const std::vector<cluster::PromptMark> marks = cluster::prompt_marks(text, this->slashKnown_(text));
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
                        // the text: the @files, ::sessions and the /commands / skills that exist in the accent colour, like the atoms;
                        // a first /word that does not exist in the colour of the errors
                        for (std::size_t k = x; k < at;) {
                            const cluster::PromptMark* mark = nullptr;
                            std::size_t end = at;
                            for (const cluster::PromptMark& m: marks) {
                                if (m.start <= k && k < m.end) mark = &m;
                                else if (m.start > k) end = std::min(end, m.start);
                            }
                            if (mark != nullptr) end = std::min(mark->end, at);
                            if (end == k) ++end;
                            ftxui::Element element = ftxui::text(text.substr(k, end - k));
                            if (mark == nullptr) element = element | ftxui::color(this->_theme.text);
                            else if (mark->kind == cluster::MarkKind::Unknown) element = element | ftxui::color(this->_theme.error) | ftxui::bold | ftxui::underlined;
                            else element = element | ftxui::color(this->_theme.accent) | ftxui::bold | ftxui::bgcolor(this->_theme.surface);
                            parts.push_back(selected ? element | ftxui::inverted : element);
                            k = end;
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
                        const std::optional<std::size_t> atom = cursor < le ? this->_editor.atomAt(text, cursor) : std::nullopt;
                        if (atom) {
                            // a block ([Image #N], [Pasted text #N]): the cursor on its first cell (the terminal puts it at
                            // the end of the element it is on: on its "]" before)
                            const std::string label = this->_editor.atom(*atom).label;
                            parts.push_back(ftxui::text(label.substr(0, 1)) | ftxui::color(this->_theme.accent) | ftxui::bold | ftxui::bgcolor(this->_theme.surface)
                                | ftxui::focusCursorBarBlinking);
                            parts.push_back(ftxui::text(label.substr(1)) | ftxui::color(this->_theme.accent) | ftxui::bold | ftxui::bgcolor(this->_theme.surface));
                        } else {
                            parts.push_back(ftxui::text(cursor < le ? this->_editor.shown(text.substr(cursor, next - cursor)) : " ") | ftxui::color(this->_theme.text)
                                | ftxui::focusCursorBarBlinking);
                        }
                        piece(next, le, false);
                    } else {
                        piece(cursor, le, false);
                    }
                } else {
                    piece(ls, le, false);
                }
                if (text.empty() && this->_mode == Mode::Normal)
                    parts.push_back(ftxui::text(" @ file · / command · ! shell · : command · " + this->_manager.config().keys.at("history") + " history · Esc back · " + this->_manager.config().keys.at("stashes") + " stash · " + this->_manager.config().keys.at("help") + " keys")
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

        struct CodeSpan {
            int first = -1;     // the rows of code of a block around a row, the cells of its text
            int last = -1;
            int xMin = 0;
            int xMax = 0;
        };

        _nodiscard static CodeSpan codeSpan_(const View& view, const int y)
        {
            // the block drawn on the row y: its first / last row of code and its widest text (none: first < 0)
            CodeSpan span;
            if (y < 0 || y >= static_cast<int>(view.code.size()) || view.code[static_cast<std::size_t>(y)].block < 0) return span;
            const int block = view.code[static_cast<std::size_t>(y)].block;
            int top = y;
            int bottom = y;
            while (top > 0 && view.code[static_cast<std::size_t>(top - 1)].block == block)
                --top;
            while (bottom + 1 < static_cast<int>(view.code.size()) && view.code[static_cast<std::size_t>(bottom + 1)].block == block)
                ++bottom;
            for (int k = top; k <= bottom; ++k) {
                const CodeRow& row = view.code[static_cast<std::size_t>(k)];
                if (!row.text) continue;
                if (span.first < 0) {
                    span.first = k;
                    span.xMin = row.from;
                }
                span.last = k;
                span.xMax = std::max(span.xMax, row.to);
            }
            return span;
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
            bool started = false;    // something copied: the next row is joined to it
            for (int y = std::max(ya, 0); y <= yb && y < static_cast<int>(view.lines.size()); ++y) {
                const CodeRow& code = view.code[static_cast<std::size_t>(y)];
                if (code.block >= 0) {
                    // a code block: its code as written, without the box, its padding or its header
                    if (!code.text) continue;
                    const int a = std::max(code.from, y == ya ? xa : 0);
                    const int b = std::min(code.to, y == yb ? xb + 1 : code.to);
                    if (started) text += view.joins[static_cast<std::size_t>(y)];
                    started = true;
                    if (b > a) text += code_slice_(code.raw, a - code.from, b - code.from);
                    continue;
                }
                const std::string& line = view.lines[static_cast<std::size_t>(y)];
                const std::size_t from = y == ya ? byte_at_(line, xa) : 0;
                const std::size_t to = y == yb ? byte_at_(line, xb + 1) : line.size();
                if (started) text += view.joins[static_cast<std::size_t>(y)];
                started = true;
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

        _nodiscard ftxui::Decorator clipBox_(const int clip) {return ftxui::reflect(this->_clicks[static_cast<std::size_t>(clip)].box);};

        bool plugEvent_(const ftxui::Event& event)
        {
            if (now_ms_() - this->_plugRead > 1000) { // the index of the core again (a scan, an install done)
                this->_plug = cluster::plugins_index();
                this->_plugRead = now_ms_();
            }
            if (this->_plugAdd) { // a source typed: Tab completes, Enter adds, Esc back
                if (event == ftxui::Event::Escape) {
                    this->_plugAdd = false;
                    this->_plugHints.clear();
                } else if (event == ftxui::Event::Return) {
                    this->_plugAdd = false;
                    this->_plugHints.clear();
                    this->plugAddSource_(this->_plugInput.text());
                } else if (event == ftxui::Event::Tab) {
                    this->plugComplete_();
                } else {
                    (void)this->_plugInput.key(event);
                    this->_plugHints.clear();
                }
                return true;
            }
            const std::vector<std::pair<std::size_t, int>> rows = this->plugRows_();
            const bool any = !rows.empty();
            const std::pair<std::size_t, int> row = any ? rows[std::min(this->_plugSel, rows.size() - 1)] : std::pair<std::size_t, int>{0, -1};
            const cluster::PluginRepo* repo = any ? &this->_plug[row.first] : nullptr;
            const cluster::PluginItem* item = repo && row.second >= 0 ? &repo->items[static_cast<std::size_t>(row.second)] : nullptr;
            const std::filesystem::path root = item ? std::filesystem::path(item->path.empty() ? repo->dir : item->path) : repo ? std::filesystem::path(repo->dir) : "";
            if (event == ftxui::Event::Escape || event == ftxui::Event::Character('q')) {
                if (this->_plugTree) this->_plugTree = false;
                else this->_mode = Mode::Normal;
                return true;
            }
            if (this->_plugTree) { // the tree: ↑↓, Enter / → opens a folder or previews a file, ← closes / back to the list
                const std::vector<std::tuple<int, std::filesystem::path, bool>> nodes = this->plugNodes_(root);
                if (nodes.empty()) {
                    this->_plugTree = false;
                    return true;
                }
                this->_plugNode = std::min(this->_plugNode, nodes.size() - 1);
                const auto &[depth, path, dir] = nodes[this->_plugNode];
                if (event == ftxui::Event::ArrowUp) this->_plugNode = this->_plugNode > 0 ? this->_plugNode - 1 : 0;
                else if (event == ftxui::Event::ArrowDown) this->_plugNode = std::min(this->_plugNode + 1, nodes.size() - 1);
                else if (event == ftxui::Event::PageDown) this->_plugScroll += 10;
                else if (event == ftxui::Event::PageUp) this->_plugScroll = std::max(0, this->_plugScroll - 10);
                else if (event == ftxui::Event::Return || event == ftxui::Event::ArrowRight) {
                    if (dir) {
                        if (this->_plugOpen.contains(path.string())) this->_plugOpen.erase(path.string());
                        else this->_plugOpen.insert(path.string());
                    } else {
                        this->_plugFile = path.string();
                        this->_plugScroll = 0;
                    }
                } else if (event == ftxui::Event::ArrowLeft) {
                    if (dir && this->_plugOpen.contains(path.string())) this->_plugOpen.erase(path.string());
                    else this->_plugTree = false;
                }
                return true;
            }
            if (event == ftxui::Event::ArrowUp) {
                this->_plugSel = this->_plugSel > 0 ? this->_plugSel - 1 : 0;
                this->_plugFile.clear();
            } else if (event == ftxui::Event::ArrowDown) {
                this->_plugSel = any ? std::min(this->_plugSel + 1, rows.size() - 1) : 0;
                this->_plugFile.clear();
            } else if ((event == ftxui::Event::Return || event == ftxui::Event::ArrowRight || event == ftxui::Event::Tab) && repo) {
                this->_plugTree = true;
                this->_plugNode = 0;
                // its SKILL.md / README previewed at once
                for (const char* first: {"SKILL.md", "README.md", ".claude-plugin/plugin.json"})
                    if (std::filesystem::exists(root / first)) {
                        this->_plugFile = (root / first).string();
                        break;
                    }
            } else if (event == ftxui::Event::Character('a') || (event.is_character() && (event.character() == "/" || event.character() == "~"
                || event.character() == "'" || event.character() == "\""))) {
                // a source typed, or a file / folder dropped on the terminal (its path pasted)
                this->_plugAdd = true;
                this->_plugInput.clear();
                if (event != ftxui::Event::Character('a')) this->_plugInput.insert(event.character());
            } else if (event == ftxui::Event::Character('u')) {
                this->_manager.pluginsRefresh();
                this->status_("plugins: the sources pulled / listed again");
            } else if (event == ftxui::Event::Character('d') && repo && !item) {
                std::vector<std::string> repos = this->_manager.config().pluginRepos;
                repos.erase(std::remove(repos.begin(), repos.end(), repo->url), repos.end());
                std::string list;
                for (const std::string& r: repos)
                    list += (list.empty() ? "" : ",") + r;
                this->plugWrite_("plugins", "repos", cluster::SettingKind::List, list);
                this->status_("source dropped: " + repo->url + " (what it installed stays until removed: r)");
            } else if ((event == ftxui::Event::Character('i') || event == ftxui::Event::Character('r') || event == ftxui::Event::Character(' ')) && repo) {
                // installed / removed: the item, or every item of a source (its "*")
                const std::string id = item ? item->id : repo->slug + ":*";
                const bool install = event == ftxui::Event::Character('i') || (event == ftxui::Event::Character(' ') && !(item && item->installed));
                this->plugWrite_("plugins.select", "\"" + id + "\"", cluster::SettingKind::Bool, install ? "true" : "false");
                this->status_(std::string(install ? "installing " : "removing ") + (item ? item->name : "everything of " + repo->slug) + "… (for the sessions started after it)");
            } else if (event == ftxui::Event::Character('e') && item) {
                if (!item->installed) {
                    this->status_("not installed: i first", true);
                } else {
                    this->plugWrite_("plugins.off", "\"" + item->id + "\"", cluster::SettingKind::Bool, item->enabled ? "true" : "false");
                    this->status_(std::string(item->enabled ? "turning off " : "turning on ") + item->name + "…");
                }
            }
            return true;
        }

        _nodiscard ftxui::Elements plugPreview_(const std::string& file, const int height, const int width)
        {
            // a file of a plugin / skill: its lines coloured by its language; Markdown drawn (headings, bullets, its code blocks)
            ftxui::Elements out;
            std::ifstream in(file);
            if (!in) return {ftxui::text(" (unreadable)") | ftxui::color(this->_theme.muted)};
            static const std::map<std::string, std::string> languages = {{".py", "python"}, {".sh", "bash"}, {".bash", "bash"}, {".zsh", "bash"}, {".js", "js"},
                {".ts", "ts"}, {".json", "json"}, {".yaml", "yaml"}, {".yml", "yaml"}, {".toml", "toml"}, {".cpp", "cpp"}, {".hpp", "cpp"}, {".c", "c"}, {".h", "c"},
                {".rs", "rust"}, {".go", "go"}, {".lua", "lua"}, {".md", "md"}, {".txt", "text"}, {".html", "html"}, {".css", "css"}, {".cmake", "cmake"}};
            const std::string ext = std::filesystem::path(file).extension().string();
            const std::string lang = languages.contains(ext) ? languages.at(ext) : std::string(std::filesystem::path(file).filename() == "CMakeLists.txt" ? "cmake" : "text");
            const bool markdown = lang == "md";
            std::string line;
            int n = 0;
            bool fence = false;
            bool front = false;
            std::string fenceLang;
            bool comment = false;
            const std::function<ftxui::Element(const std::vector<cluster::Cell>&)> cells = [](const std::vector<cluster::Cell>& list) {
                ftxui::Elements runs;
                for (const cluster::Cell& c: list)
                    runs.push_back(ftxui::text(c.text) | ftxui::color(ftxui::Color::RGB((c.fg >> 16) & 255, (c.fg >> 8) & 255, c.fg & 255)));
                return ftxui::hbox(std::move(runs));
            };
            while (std::getline(in, line) && static_cast<int>(out.size()) < height) {
                for (std::size_t tab = line.find('\t'); tab != std::string::npos; tab = line.find('\t'))
                    line.replace(tab, 1, "    ");
                ++n;
                if (n <= this->_plugScroll) {
                    if (markdown && cluster::trim(line).starts_with("```")) fence = !fence;
                    continue;
                }
                const std::string shown = cut_(line, static_cast<std::size_t>(std::max(10, width - 6)));
                const ftxui::Element number = ftxui::text(std::string(4 - std::min<std::size_t>(4, std::to_string(n).size()), ' ') + std::to_string(n) + " ")
                    | ftxui::color(this->_theme.border);
                ftxui::Element body;
                if (!markdown) {
                    body = cells(highlight_(shown, lang, this->_code, comment));
                } else if (n == 1 && cluster::trim(line) == "---") {
                    front = true;
                    body = ftxui::text(shown) | ftxui::color(this->_theme.muted);
                } else if (front) {
                    if (cluster::trim(line) == "---") front = false;
                    const std::size_t colon = shown.find(':');
                    body = colon == std::string::npos ? ftxui::text(shown) | ftxui::color(this->_theme.muted)
                        : ftxui::hbox({ftxui::text(shown.substr(0, colon + 1)) | ftxui::color(this->_theme.accent), ftxui::text(shown.substr(colon + 1)) | ftxui::color(this->_theme.muted)});
                } else if (cluster::trim(line).starts_with("```")) {
                    fence = !fence;
                    fenceLang = fence ? cluster::trim(cluster::trim(line).substr(3)) : std::string();
                    body = ftxui::text(fence ? "╭─ " + (fenceLang.empty() ? std::string("code") : fenceLang) : "╰─") | ftxui::color(this->_theme.border);
                } else if (fence) {
                    body = ftxui::hbox({ftxui::text("│ ") | ftxui::color(this->_theme.border), cells(highlight_(shown, fenceLang, this->_code, comment))});
                } else if (line.starts_with("#")) {
                    const std::size_t at = shown.find_first_not_of('#');
                    body = ftxui::text(at == std::string::npos ? std::string() : cluster::trim(shown.substr(at))) | ftxui::bold | ftxui::color(this->_theme.accent);
                } else if (cluster::trim(line).starts_with("- ") || cluster::trim(line).starts_with("* ")) {
                    const std::size_t at = line.find_first_not_of(' ');
                    body = ftxui::hbox({ftxui::text(std::string(at, ' ') + "• ") | ftxui::color(this->_theme.accent), ftxui::text(cluster::one_line(line.substr(at + 2),
                        static_cast<std::size_t>(std::max(10, width - 8)))) | ftxui::color(this->_theme.text)});
                } else {
                    body = ftxui::text(shown) | ftxui::color(this->_theme.text);
                }
                out.push_back(ftxui::hbox({number, body}));
            }
            return out;
        }

        void openForm_(const std::string& session, const std::string& display, const std::string& title, const cluster::Json& fields, const cluster::Json& values,
            const std::size_t focus)
        {
            // the popup of a form: a slot per field, the values given or sent before, the field clicked focused
            this->_form.clear();
            for (const cluster::Json& field: fields) {
                FormField f{field.value("name", std::string()), field.value("label", field.value("name", std::string())),
                    field.value("type", std::string(field.contains("options") ? "choice" : "text")), {}, ""};
                for (const cluster::Json& o: field.value("options", cluster::Json::array()))
                    f.options.push_back(o.is_string() ? o.get<std::string>() : o.dump(-1, ' ', false, cluster::JSON_LENIENT));
                const cluster::Json value = values.contains(f.name) ? values[f.name] : field.value("value", cluster::Json(""));
                f.value = value.is_string() ? value.get<std::string>() : value.is_boolean() ? (value.get<bool>() ? "yes" : "no") : value.dump(-1, ' ', false, cluster::JSON_LENIENT);
                if (f.type == "bool" && f.value != "yes" && f.value != "true") f.value = "no";
                if (f.type == "bool" && f.value == "true") f.value = "yes";
                if (f.type == "choice" && f.value.empty() && !f.options.empty()) f.value = f.options.front();
                this->_form.push_back(f);
            }
            this->_formSession = session;
            this->_formDisplay = display;
            this->_formTitle = title;
            this->_formFocus = std::min(focus, this->_form.size());
            this->_mode = Mode::Form;
        }

        void formSubmit_(void)
        {
            // its values typed (a number, yes / no) to the display: sent to the global session
            cluster::Json values = cluster::Json::object();
            for (const FormField& f: this->_form) {
                if (f.type == "bool") {
                    values[f.name] = f.value == "yes";
                    continue;
                }
                if (f.type == "number") {
                    try {
                        std::size_t used = 0;
                        const double number = std::stod(f.value, &used);
                        if (used == f.value.size()) {
                            values[f.name] = number == std::floor(number) && std::abs(number) < 1e15 ? cluster::Json(static_cast<long long>(number)) : cluster::Json(number);
                            continue;
                        }
                    } catch (const std::exception&) { // xstyle: ignore G-EMPTY-BLOCK (not a number: sent as typed)
                    }
                }
                values[f.name] = f.value;
            }
            this->_mode = Mode::Normal;
            try {
                this->status_(this->_manager.displayAct(this->_formSession, this->_formDisplay, {{"values", values}}));
            } catch (const utils::exception::IException& e) {
                this->status_(e.info(), true);
            }
        }

        bool formEvent_(const ftxui::Event& event)
        {
            const std::size_t count = this->_form.size();
            FormField* f = this->_formFocus < count ? &this->_form[this->_formFocus] : nullptr;
            const bool typed = f && (f->type == "text" || f->type == "number" || f->type == "date");
            if (event == ftxui::Event::Escape) {
                this->_mode = Mode::Normal;
            } else if (event.input() == std::string(1, '\x13')) { // Ctrl+S
                this->formSubmit_();
            } else if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Tab) {
                this->_formFocus = (this->_formFocus + 1) % (count + 2);
            } else if (event == ftxui::Event::ArrowUp || event == ftxui::Event::TabReverse) {
                this->_formFocus = (this->_formFocus + count + 1) % (count + 2);
            } else if (event == ftxui::Event::Return) {
                if (this->_formFocus == count + 1) this->_mode = Mode::Normal;
                else if (this->_formFocus + 1 >= count) this->formSubmit_();     // the last field or Submit
                else ++this->_formFocus;
            } else if (!f) {
                if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight) this->_formFocus = this->_formFocus == count ? count + 1 : count;
            } else if (f->type == "choice" && (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight) && !f->options.empty()) {
                const auto at = std::find(f->options.begin(), f->options.end(), f->value);
                const std::size_t index = at == f->options.end() ? 0 : static_cast<std::size_t>(at - f->options.begin());
                const std::size_t size = f->options.size();
                f->value = f->options[(index + (event == ftxui::Event::ArrowRight ? 1 : size - 1)) % size];
            } else if (f->type == "bool" && (event == ftxui::Event::Character(' ') || event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight)) {
                f->value = f->value == "yes" ? "no" : "yes";
            } else if (typed) {
                // a number field takes digits only (the editing keys work as in a text field)
                if (f->type != "number" || !event.is_character() || event.character().find_first_not_of("0123456789.-+eE") == std::string::npos)
                    (void)this->fieldKey_(f->value, event);
            }
            return true;
        }

        void hitHover_(const ftxui::Mouse& mouse)
        {
            // the clickable part of a display under the mouse (a transcript of the main layer, nothing over it)
            this->_hitView.clear();
            this->_hitY = -1;
            if (this->_mode != Mode::Normal || this->_hover >= 0) return;
            for (const auto &[id, view]: this->_views) {
                if (!view.box.Contain(mouse.x, mouse.y)) continue;
                const std::size_t y = static_cast<std::size_t>(mouse.y - view.box.y_min);
                const int x = mouse.x - view.box.x_min;
                if (y >= view.hits.size()) return;
                for (const auto &[from, to, act]: view.hits[y])
                    if (x >= from && x <= to) {
                        this->_hitView = id;
                        this->_hitY = static_cast<int>(y);
                        this->_hitFrom = from;
                    }
                return;
            }
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
                            this->_pressView = id;
                            this->_pressY = static_cast<int>(y);
                            this->_pressFrom = from;
                            this->_pressAt = now_ms_();
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
            if (this->_mode == Mode::Palette || this->_mode == Mode::History || this->_mode == Mode::Rewind || this->_mode == Mode::Stash) this->event_(ftxui::Event::Escape); // outside: closed
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
                    const CodeSpan span = this->codeSpan_(view, y);
                    this->_selection = Selection{id, x, y, x, y};
                    if (span.first >= 0) {
                        // started in a code block: it stays in its text
                        this->_selection->block = view.code[static_cast<std::size_t>(y)].block;
                        this->_selection->y0 = this->_selection->y1 = std::clamp(y, span.first, span.last);
                        this->_selection->x0 = this->_selection->x1 = std::clamp(x, span.xMin, std::max(span.xMin, span.xMax - 1));
                    }
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
                    if (this->_selection->block >= 0) {
                        const CodeSpan span = this->codeSpan_(this->_views.at(this->_drag), this->_selection->y0);
                        if (span.first >= 0) {
                            this->_selection->y1 = std::clamp(this->_selection->y1, span.first, span.last);
                            this->_selection->x1 = std::clamp(this->_selection->x1, span.xMin, std::max(span.xMin, span.xMax - 1));
                        }
                    }
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
            const std::string prefix = this->_editor.kind() == cluster::CompletionKind::Session ? "::" : this->_editor.kind() == cluster::CompletionKind::File ? "@" : this->_editor.kind() == cluster::CompletionKind::Command || this->_editor.kind() == cluster::CompletionKind::Skill ? "/" : "";
            int labelWidth = 0; // the columns (name, source, description) aligned over the rows shown
            int sourceWidth = 0;
            for (int i = start; i < static_cast<int>(items.size()) && i < start + 8; ++i) {
                labelWidth = std::max(labelWidth, ftxui::string_width(prefix + items[static_cast<std::size_t>(i)].label));
                sourceWidth = std::max(sourceWidth, ftxui::string_width(items[static_cast<std::size_t>(i)].source));
            }
            for (int i = start; i < static_cast<int>(items.size()) && i < start + 8; ++i) {
                const cluster::Candidate& c = items[static_cast<std::size_t>(i)];
                ftxui::Elements cells = {ftxui::text("  " + prefix + c.label) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, labelWidth + 4)};
                if (sourceWidth > 0) // a /command or a skill: where it comes from (the colour of its source, no background)
                    cells.push_back((c.source.empty() ? ftxui::text("") : i == selected ? ftxui::text(c.source) : ftxui::text(c.source) | ftxui::color(hex_(cluster::source_color(c.source)))) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, sourceWidth + 2));
                cells.push_back(ftxui::text(cluster::one_line(c.description, 70)) | ftxui::dim);
                cells.push_back(ftxui::filler());
                ftxui::Element row = ftxui::hbox(std::move(cells));
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
            // the list of every action (the models, the trash, the projects...) is long to make: made when the palette opens, then only filtered at each letter
            if (wanted.empty() || this->_actionBase.empty() || this->_actionBaseFor != this->_active || now_ms_() - this->_actionBaseAt > 8000) {
                this->_actionBase = cluster::actions(this->_manager, this->_active);
                this->_actionBaseFor = this->_active;
                this->_actionBaseAt = now_ms_();
            }
            this->_actions = cluster::filter_actions(this->_actionBase, wanted, &this->_manager); // the closest first
            std::erase_if(this->_actions, [](const cluster::Action& action) {return action.id == "layout:tabs";}); // no tabs in the terminal
            this->_selected = std::clamp(this->_selected, 0, std::max(0, static_cast<int>(this->_actions.size()) - 1));
        }

        void run_(const cluster::Action& action, const std::string& input)
        {
            const std::string base = action.id.substr(0, action.id.find(':'));
            if (this->_webTty && (base.starts_with("auth_") || base == "web_trust_ca" || base == "quit_all" || base == "docs")) { // the keys and the computer: from it only
                this->status_("from the computer only", true);
                this->_mode = Mode::Normal;
                return;
            }
            const std::string arg = action.id.find(':') == std::string::npos ? "" : action.id.substr(action.id.find(':') + 1);
            this->_mode = Mode::Normal;

            // Actions of the front-end
            if (base == "transcript_top" || base == "transcript_bottom") {
                this->scrollEdge_(base == "transcript_top");
            } else if (base == "layout") {
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
            } else if (base == "plugins") {
                this->openPlugins_();
            } else if (base == "graph") {
                this->openGraph_(false);
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
                this->quit_();
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
                        if (result.ui == "message" && !result.message.empty()) { // a popup (/info...): never in the conversation
                            this->_message = result.message;
                            this->_messageError = result.error;
                            this->_mode = Mode::Message;
                        } else if (!result.message.empty()) {
                            this->status_(result.message, result.error);
                        }
                        if (result.ui.starts_with("term:")) this->handOver_(result.ui.substr(5));
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

        void scrollEdge_(const bool top)
        {
            // the first message (clamped at the drawing: the whole transcript is kept), or the end, followed again
            this->_scroll[this->_active] = top ? std::numeric_limits<int>::max() / 2 : 0;
            this->status_(top ? "start of the transcript" : "end of the transcript");
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
                    const std::size_t before = this->_manager.stashSize();
                    const cluster::StashAdd added = this->_manager.stashPush(this->_editor.draft(), this->_active, false);
                    if (added == cluster::StashAdd::Full) {
                        this->status_("the stash is full (" + std::to_string(before) + "): load or remove one (" + this->_manager.config().keys.at("stashes")
                            + ") or raise [ui] stash_max; the prompt stays here", true);
                    } else if (added == cluster::StashAdd::TooBig) {
                        this->status_("that prompt is too big for the stash; it stays here", true);
                    } else {
                        this->_editor.clear();
                        this->historyReset_();
                        this->status_("prompt stashed (" + std::to_string(this->_manager.stashSize()) + "/" + std::to_string(this->_manager.config().stashMax) + (before >= static_cast<std::size_t>(this->_manager.config().stashMax) ? ", the oldest one dropped" : "")
                            + "), " + this->_manager.config().keys.at("stashes") + ": the list, " + this->_manager.config().keys.at("unstash") + ": the last one back");
                    }
                }
            } else if (action == "stashes") {
                this->_stash.open(this->_active, this->_manager.config().stashMax);
                this->_mode = Mode::Stash;
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
                this->status_(this->_manager.config().keys.at("prefix") + ": ← → ↑ ↓ to change of session, PageUp / PageDown / Home / End: a page / the first / the last");
            } else if (action == "palette") {
                this->_filter.clear();
                this->_selected = 0;
                this->filter_();
                this->_mode = Mode::Palette;
            } else if (action.starts_with("session_")) {
                this->sessionMove_(action);
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
            } else if (action == "graph") {
                this->openGraph_(false);
            } else if (action == "stats") {
                this->run_({"stats", "", "", "", false}, "");
            } else if (action == "push_to_talk") {
                this->run_({"voice_toggle", "", "", "", false}, "");
            } else if (action == "grid_open") { // the session of the focused cell: opened in the list
                if (this->_manager.ui("layout", this->_manager.config().layout) != "grid") return false; // else Shift+Enter stays a new line
                this->_manager.setUi("layout", "list");
                this->status_("opened in the list: " + this->_active);
            } else if (action == "layout") {
                const std::string layout = this->_manager.ui("layout", this->_manager.config().layout);
                // list -> grid -> graph -> hub -> tree -> list (the last three are views of their own, closed back on the list); a stored tabs: the list here
                if (layout == "grid") {
                    this->_manager.setUi("layout", "list");
                    this->openGraph_(false);
                } else {
                    this->_manager.setUi("layout", layout == "list" ? "grid" : "list");
                }
            } else if (action == "settings") {
                this->openSettings_();
            } else if (action == "quit") {
                this->quit_();
            } else if (action == "transcript_top" || action == "transcript_bottom") {
                this->scrollEdge_(action == "transcript_top");
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
            const cluster::Style& style = it != config.styles.end() ? it->second : config.currentStyle();
            this->_theme = make_theme_(style);
            this->_code = {rgb_(style.text, 0xe6e7ea), rgb_(style.muted, 0x7f848e), rgb_(style.accent, 0x7aa2f7), rgb_(style.ok, 0x9ece6a),
                rgb_(style.warn, 0xe0af68), rgb_(style.warn, 0xe0af68), rgb_(style.accent, 0x7dcfff), rgb_(style.surface, 0x24283b)};
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

        void openGraph_(const bool hub)
        {
            this->_graphHub = hub;
            this->_graph.title = hub ? "Hub" : "Graph";
            this->_graph.onOpen = [this](const std::string& id) { // a session opened: its view, the graph closed
                if (!this->_manager.snapshot(id)) return;
                this->_active = id;
                this->_mode = Mode::Normal;
            };
            this->_graph.onMenu = [this](const std::string& id) { // the menu of a session (its right click in the lists)
                this->_mode = Mode::Normal;
                this->_active = id;
                this->pick_("menu:" + id);
            };
            this->_mode = Mode::Graph;
        }

        _nodiscard ftxui::Element graph_(const ftxui::Dimensions& size)
        {
            // the sessions around global: its state colours (permission waiting, working, error, idle), read only
            std::vector<cluster::GraphNode> nodes;
            bool global = false;
            std::set<std::string> labels;
            std::map<std::string, int> under;   // the sessions below a label
            for (const cluster::Snapshot& s: this->sessions_()) {
                cluster::GraphNode node;
                node.id = s.spec.id;
                node.name = s.spec.global ? "global" : s.spec.name;
                node.kind = s.spec.id == GLOBAL_ID ? cluster::GraphKind::Global : cluster::GraphKind::Session;
                node.tone = !s.permissions.empty() ? cluster::GraphTone::Waiting : s.state == cluster::State::Working || s.state == cluster::State::Starting ? cluster::GraphTone::Working
                    : s.state == cluster::State::Error ? cluster::GraphTone::Error : cluster::GraphTone::Idle;
                node.detail = !s.permissions.empty() ? "waiting for you" : cluster::state_name(s.state);
                node.info = cluster::short_path(s.spec.cwd);
                node.tokens = s.spec.tokens;
                node.parent = node.kind == cluster::GraphKind::Global ? "" : GLOBAL_ID;
                global = global || node.kind == cluster::GraphKind::Global;
                if (!this->_graphHub && node.kind == cluster::GraphKind::Session) {
                    // the graph: the session under its category (else its first tag), the levels of the taxonomy between it and the global session
                    std::string up = GLOBAL_ID;
                    for (const std::string& level: this->_manager.groupPath(s.spec)) {
                        const std::string id = "label:" + level;
                        if (labels.insert(id).second) {
                            cluster::GraphNode label;
                            label.id = id;
                            label.name = level;
                            label.kind = cluster::GraphKind::Label;
                            label.detail = "category / tag";
                            label.parent = up;
                            nodes.push_back(label);
                        }
                        ++under[id];
                        up = id;
                    }
                    node.parent = up;
                }
                nodes.push_back(node);
            }
            for (cluster::GraphNode& node: nodes)
                if (node.kind == cluster::GraphKind::Label) node.count = under[node.id];
            if (!global) { // none: the app itself in the middle
                cluster::GraphNode root;
                root.id = GLOBAL_ID;
                root.name = "claude-cluster";
                root.kind = cluster::GraphKind::Global;
                nodes.insert(nodes.begin(), root);
            }
            this->_graph.setNodes(nodes);
            const Theme& t = this->_theme;
            return this->_graph.render(size.dimx, size.dimy, {t.bg, t.surface, t.text, t.muted, t.accent, t.ok, t.warn, t.error}, this->_active, this->_manager.config().keys);
        }

        void openTree_(void)
        {
            this->_mode = Mode::Tree;
            const std::vector<std::string> ids = this->_treeIds;
            const auto at = std::find(ids.begin(), ids.end(), this->_active);
            this->_treeSel = at == ids.end() ? 0 : static_cast<int>(at - ids.begin());
        }

        _nodiscard ftxui::Element tree_(const ftxui::Dimensions& size)
        {
            // the sessions by project folder: a folder, then its sessions (state, name, labels, what it does); a click opens one
            std::map<std::string, std::vector<cluster::Snapshot>> folders;
            for (const cluster::Snapshot& s: this->sessions_())
                folders[cluster::short_path(s.spec.cwd)].push_back(s);
            this->_treeIds.clear();
            ftxui::Elements rows = {ftxui::hbox({ftxui::text(" ⑂ Tree ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent),
                ftxui::text(" the sessions by project folder · " + std::to_string(folders.size()) + " folder" + (folders.size() == 1 ? "" : "s")) | ftxui::color(this->_theme.muted),
                ftxui::filler(), ftxui::text("↑ ↓ choose · Enter open · F2 the list · Esc close ") | ftxui::color(this->_theme.muted)}), ftxui::text("")};
            int focusRow = 0;
            for (const auto &[folder, sessions]: folders) {
                rows.push_back(ftxui::text(" ▾ " + folder) | ftxui::bold | ftxui::color(this->_theme.accent));
                for (std::size_t i = 0; i < sessions.size(); ++i) {
                    const cluster::Snapshot& s = sessions[i];
                    const int index = static_cast<int>(this->_treeIds.size());
                    this->_treeIds.push_back(s.spec.id);
                    const bool on = index == this->_treeSel;
                    if (on) focusRow = static_cast<int>(rows.size());
                    const std::string branch = i + 1 == sessions.size() ? "   └─ " : "   ├─ ";
                    const std::string labels = (s.spec.category.empty() ? std::string() : "  " + s.spec.category) + [&]() {
                        std::string tags;
                        for (const std::string& tag: s.spec.tags)
                            tags += " #" + tag;
                        return tags;
                    }();
                    const std::string id = s.spec.id;
                    ftxui::Element row = ftxui::hbox({ftxui::text(branch) | ftxui::color(this->_theme.border), ftxui::text(!s.permissions.empty() ? "!" : s.state == cluster::State::Working ? "●" : s.state == cluster::State::Error ? "✖" : "○")
                        | ftxui::color(!s.permissions.empty() ? this->_theme.warn : s.state == cluster::State::Working ? this->_theme.ok : s.state == cluster::State::Error ? this->_theme.error : this->_theme.muted),
                        ftxui::text(" "),
                        ftxui::text(s.spec.global ? "global" : s.spec.name) | ftxui::bold, ftxui::text(std::string("  ") + cluster::state_name(s.state)) | ftxui::color(this->_theme.muted),
                        ftxui::text(labels) | ftxui::color(this->_theme.accent), ftxui::text(s.spec.summary.empty() ? "" : "  · " + cluster::one_line(s.spec.summary, 60)) | ftxui::color(this->_theme.muted) | ftxui::dim});
                    rows.push_back(this->tap_(on ? row | ftxui::inverted : row, 0, [this, id]() {
                        this->_active = id;
                        this->_manager.setUi("layout", "list");
                        this->_mode = Mode::Normal;
                    }));
                }
            }
            if (this->_treeIds.empty()) rows.push_back(ftxui::text("   no session yet") | ftxui::color(this->_theme.muted));
            (void)size;
            (void)focusRow;
            return ftxui::vbox(std::move(rows)) | ftxui::yframe | ftxui::flex;
        }

        void openPlugins_(void)
        {
            this->_plug = cluster::plugins_index();
            this->_plugRead = now_ms_();
            this->_plugSel = std::min(this->_plugSel, this->plugRows_().empty() ? 0 : this->plugRows_().size() - 1);
            this->_plugTree = false;
            this->_plugAdd = false;
            this->_mode = Mode::Plugins;
        }

        _nodiscard std::vector<std::pair<std::size_t, int>> plugRows_(void) const
        {
            // a source, then its items (-1: the source itself)
            std::vector<std::pair<std::size_t, int>> rows;
            for (std::size_t r = 0; r < this->_plug.size(); ++r) {
                rows.emplace_back(r, -1);
                for (std::size_t i = 0; i < this->_plug[r].items.size(); ++i)
                    rows.emplace_back(r, static_cast<int>(i));
            }
            return rows;
        }

        _nodiscard std::vector<std::tuple<int, std::filesystem::path, bool>> plugNodes_(const std::filesystem::path& root) const
        {
            // the tree of a folder: its opened folders unfolded (folders first, .git skipped), 600 rows at most
            std::vector<std::tuple<int, std::filesystem::path, bool>> nodes;
            std::function<void(const std::filesystem::path&, int)> walk = [&](const std::filesystem::path& dir, const int depth) {
                std::error_code error;
                std::vector<std::filesystem::path> dirs;
                std::vector<std::filesystem::path> files;
                for (const std::filesystem::directory_entry& e: std::filesystem::directory_iterator(dir, error)) {
                    if (e.path().filename() == ".git" || e.path().filename() == ".claude-cluster-unpacked") continue;
                    (e.is_directory(error) ? dirs : files).push_back(e.path());
                }
                std::sort(dirs.begin(), dirs.end());
                std::sort(files.begin(), files.end());
                for (const std::filesystem::path& d: dirs) {
                    if (nodes.size() >= 600) return;
                    nodes.emplace_back(depth, d, true);
                    if (this->_plugOpen.contains(d.string())) walk(d, depth + 1);
                }
                for (const std::filesystem::path& f: files) {
                    if (nodes.size() >= 600) return;
                    nodes.emplace_back(depth, f, false);
                }
            };
            if (std::filesystem::is_directory(root)) walk(root, 0);
            return nodes;
        }

        void plugWrite_(const std::string& section, const std::string& key, const cluster::SettingKind kind, const std::string& value)
        {
            // config.toml written ([plugins] / [plugins.select] / [plugins.off]): the core applies it at its reload
            cluster::Setting setting;
            setting.section = section;
            setting.key = key;
            setting.kind = kind;
            try {
                cluster::settings_write(this->_manager.config(), {{{section, key}, {setting, value}}});
                (void)this->_manager.config().reload();
            } catch (const utils::exception::IException& e) {
                this->status_(e.info(), true);
            }
        }

        void plugAddSource_(std::string source)
        {
            // a link, a folder, a .zip (dropped on the terminal: its path, quoted or file://)
            source = cluster::trim(source);
            if (source.size() >= 2 && (source.front() == '\'' || source.front() == '"') && source.back() == source.front()) source = source.substr(1, source.size() - 2);
            if (source.starts_with("file://")) source = source.substr(7);
            for (std::size_t at = source.find("\\ "); at != std::string::npos; at = source.find("\\ ", at)) source.erase(at, 1);
            if (source.empty()) return;
            std::vector<std::string> repos = this->_manager.config().pluginRepos;
            if (std::find(repos.begin(), repos.end(), source) != repos.end()) {
                this->status_("already a source: " + source);
                return;
            }
            repos.push_back(source);
            std::string list;
            for (const std::string& r: repos)
                list += (list.empty() ? "" : ",") + r;
            this->plugWrite_("plugins", "repos", cluster::SettingKind::List, list);
            this->status_("source added: " + source + " (listed in a moment: cloned / unpacked by the core)");
        }

        void plugComplete_(void)
        {
            // Tab on a path: completed (folders, .zip), the choices shown when several
            std::string typed = this->_plugInput.text();
            if (cluster::plugin_kind(typed) == "git" && !typed.starts_with("/") && !typed.starts_with("~")) return;
            const std::filesystem::path full = cluster::expand_home(typed.empty() ? std::string("~/") : typed);
            const bool inside = typed.ends_with("/");
            const std::filesystem::path dir = inside ? full : full.parent_path();
            const std::string prefix = inside ? std::string() : full.filename().string();
            std::vector<std::string> names;
            std::error_code error;
            for (const std::filesystem::directory_entry& e: std::filesystem::directory_iterator(dir, error)) {
                const std::string name = e.path().filename().string();
                if (!name.starts_with(prefix) || (name.starts_with(".") && !prefix.starts_with("."))) continue;
                if (e.is_directory(error)) names.push_back(name + "/");
                else if (name.ends_with(".zip")) names.push_back(name);
            }
            std::sort(names.begin(), names.end());
            this->_plugHints.assign(names.begin(), names.begin() + static_cast<long>(std::min<std::size_t>(names.size(), 12)));
            if (names.empty()) return;
            std::string common = names.front();
            for (const std::string& n: names)
                while (!n.starts_with(common)) common.pop_back();
            const std::string base = typed.substr(0, inside ? typed.size() : typed.rfind('/') == std::string::npos ? 0 : typed.rfind('/') + 1);
            this->_plugInput.set(base + common);
            if (names.size() == 1) this->_plugHints.clear();
        }

        void handOver_(const std::string& command)
        {
            // a program that needs a terminal (vim, less, top, ssh...): the interface leaves the screen, the program has the terminal until it ends, then it is back
            const std::optional<cluster::Snapshot> snap = this->_manager.snapshot(this->_active);
            const std::string cwd = snap ? cluster::expand_home(snap->spec.cwd).string() : std::filesystem::current_path().string();
            const char* env = std::getenv("SHELL");
            int code = -1;
            this->_screen.WithRestoredIO([&]() {
                utils::encapsulation::Process process;
                try {
                    (void)process.spawn("env", {"-C", cwd, env != nullptr ? env : "/bin/sh", "-c", command});
                    const utils::encapsulation::Status status = process.wait();
                    code = status.exited ? status.code : -1;
                } catch (const utils::exception::IException& e) {
                    std::cerr << e.info() << std::endl;
                }
            })();
            this->status_("exit " + std::to_string(code) + " · " + cluster::one_line(command, 50), code != 0);
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
            if (this->_webTty && !cluster::web_may(this->_manager.config(), "services")) {
                this->_note = "read only from the web page: the computer allows it (Web > Settings from the page: services)";
                return;
            }
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
            if (this->_webTty && (!cluster::web_may(this->_manager.config(), setting.section) || (setting.section == "web" && setting.key == "settings"))) {
                this->_note = "read only from the web page: the computer allows it in its settings (Web > Settings from the page)";
                return;
            }
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
            if (this->_webTty) {
                this->_note = "installed from the computer only";
                return;
            }
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
                for (const char* key: {"layout", "style", "vim", "group_by_category"})
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
                } else {
                    const std::string before = this->_find;
                    if (this->fieldKey_(this->_find, event) && this->_find != before) {
                        this->_hits = cluster::settings_search(this->_pages, this->_find);
                        this->_findHit = 0;
                    }
                }
                return true;
            }
            if (!this->_editing && this->_note.empty() && (event == ftxui::Event::Character('i') || event == ftxui::Event::Character('I')) && !this->_need.package.empty()) {
                this->install_(this->_need.package); // what the setting chosen needs
                return true;
            }
            if (!this->_editing && !this->_finding && this->_note != "confirm" && !this->_webTty && event == ftxui::Event::Character('?')) { // the help of this page: its docs page
                const cluster::DocOpened docs = cluster::docs_open("settings");
                this->status_(docs.url.empty() ? "the documentation is not in this build" : (docs.opened ? "docs: " : "docs, open it in a browser: ") + docs.url, docs.url.empty());
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
                const std::vector<std::string> suggestions = cluster::setting_suggestions(this->_manager.config(), setting, this->_edit);
                this->_suggest = std::min(this->_suggest, std::max(0, static_cast<int>(suggestions.size()) - 1));
                if (event == ftxui::Event::Return) {
                    if (this->_suggestOpen && !suggestions.empty()) { // a suggestion chosen with the arrows: taken, the field goes on
                        this->_edit = suggestions[static_cast<std::size_t>(this->_suggest)];
                        this->_suggestOpen = false;
                        this->_suggest = 0;
                        return true;
                    }
                    this->setSetting_(setting, cluster::trim(this->_edit));
                    this->_editing = false;
                    this->_suggestOpen = false;
                    this->_suggest = 0;
                } else if (event == ftxui::Event::Escape) {
                    if (this->_suggestOpen) this->_suggestOpen = false; // the list closes first
                    else this->_editing = false;
                } else if (event == ftxui::Event::Tab && !suggestions.empty()) {
                    this->_edit = suggestions[static_cast<std::size_t>(this->_suggest)]; // Tab takes the first / the chosen one
                    this->_suggestOpen = false;
                    this->_suggest = 0;
                } else if (!suggestions.empty() && (event == ftxui::Event::ArrowDown || event == ftxui::Event::ArrowUp)) {
                    this->_suggestOpen = true;
                    const int total = static_cast<int>(suggestions.size());
                    this->_suggest = (this->_suggest + (event == ftxui::Event::ArrowDown ? 1 : total - 1)) % total;
                } else {
                    (void)this->fieldKey_(this->_edit, event);
                    this->_suggest = 0;
                    this->_suggestOpen = false;
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

        _nodiscard ftxui::Element keysValue_(const cluster::Setting& setting, const std::string& value, const bool typed, const bool selected) const
        {
            // a value between brackets; in the Keys page the alternatives that are no key shown in red (the others as usual)
            const Theme& t = this->_theme;
            const ftxui::Decorator normal = selected ? ftxui::color(t.bg) | ftxui::bgcolor(t.accent) : ftxui::color(t.text);
            const std::vector<std::string> bad = cluster::setting_invalid_parts(setting, value);
            if (bad.empty()) return ftxui::text((typed ? "[" : " ") + cluster::one_line(value, 60) + (typed ? "]" : " ")) | normal | ftxui::bold;
            ftxui::Elements parts = {ftxui::text("[") | normal | ftxui::bold};
            const std::vector<std::string> items = cluster::split(value, '|');
            for (std::size_t i = 0; i < items.size(); ++i) {
                const bool invalid = std::find(bad.begin(), bad.end(), cluster::trim(items[i])) != bad.end();
                if (i > 0) parts.push_back(ftxui::text("|") | normal | ftxui::bold);
                parts.push_back(ftxui::text(items[i]) | (invalid ? ftxui::color(t.error) | ftxui::bgcolor(t.bg) | ftxui::bold | ftxui::underlined : normal | ftxui::bold));
            }
            parts.push_back(ftxui::text("]") | normal | ftxui::bold);
            return ftxui::hbox(std::move(parts));
        }

        _nodiscard ftxui::Element askPreview_(const std::string& source, const int width, const int maxRows)
        {
            // the preview of an option of a question (Claude Desktop): markdown with its code blocks, tables and diffs, an image (a path or ![](path)), a
            // display block ({"type": "image" | "table" | "chart", ...}), a plain mock-up; the colours of the theme
            const Theme& t = this->_theme;
            ftxui::Elements out;
            int more = 0;
            const std::function<void(ftxui::Element)> add = [&out, &more, maxRows](ftxui::Element element) {
                if (static_cast<int>(out.size()) < maxRows) out.push_back(std::move(element));
                else ++more;
            };
            const std::set<std::string> pictures = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".svg"};
            const std::function<bool(const std::string&)> image = [&](const std::string& raw) {
                const std::filesystem::path path = cluster::expand_home(cluster::trim(raw));
                std::error_code error;
                std::string extension = path.extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});
                if (!pictures.contains(extension) || !std::filesystem::is_regular_file(path, error)) return false;
                const bool chafa = this->_manager.config().imagePreview == "chafa";
                const int cols = std::clamp(width - 2, 12, 100);
                const int rows = std::clamp(maxRows - 1, 6, 22);
                const Preview& shown = chafa ? preview_(path.string(), cols, rows, true) : preview_(path.string(), cols, rows, false);
                if (!shown.cells.empty()) {
                    for (const std::vector<Cell>& cells: shown.cells) {
                        ftxui::Elements parts;
                        for (const Cell& cell: cells) {
                            ftxui::Element part = ftxui::text(cell.glyph);
                            if (cell.fg) part = part | ftxui::color(*cell.fg);
                            if (cell.bg) part = part | ftxui::bgcolor(*cell.bg);
                            parts.push_back(part);
                        }
                        add(ftxui::hbox(std::move(parts)));
                    }
                } else if (shown.width > 0) {
                    for (int y = 0; y + 1 < shown.height + 1; y += 2) {
                        ftxui::Elements parts;
                        for (int x = 0; x < shown.width; ++x) {
                            const std::size_t top = static_cast<std::size_t>((y * shown.width + x) * 3);
                            const std::size_t bottom = static_cast<std::size_t>((std::min(y + 1, shown.height - 1) * shown.width + x) * 3);
                            parts.push_back(ftxui::text("▀") | ftxui::color(ftxui::Color::RGB(shown.rgb[top], shown.rgb[top + 1], shown.rgb[top + 2]))
                                | ftxui::bgcolor(ftxui::Color::RGB(shown.rgb[bottom], shown.rgb[bottom + 1], shown.rgb[bottom + 2])));
                        }
                        add(ftxui::hbox(std::move(parts)));
                    }
                } else {
                    add(ftxui::text("(no preview of " + path.filename().string() + ": chafa or magick needed)") | ftxui::color(t.muted) | ftxui::dim);
                }
                return true;
            };
            const std::string text = cluster::trim(source);
            const cluster::Json block = text.starts_with("{") ? cluster::Json::parse(text, nullptr, false) : cluster::Json();
            if (block.is_object() && block.contains("type")) {
                const std::string type = block.value("type", std::string());
                if (type == "image" && image(block.value("path", std::string()))) return ftxui::vbox(std::move(out));
                if (type == "table" && block.contains("rows") && block["rows"].is_array()) { // a table: its columns padded to the widest cell
                    std::vector<std::vector<std::string>> grid;
                    if (block.contains("columns") && block["columns"].is_array()) {
                        std::vector<std::string> head;
                        for (const cluster::Json& c: block["columns"]) head.push_back(c.is_string() ? c.get<std::string>() : c.dump());
                        grid.push_back(head);
                    }
                    for (const cluster::Json& r: block["rows"]) {
                        std::vector<std::string> cells;
                        if (r.is_array()) for (const cluster::Json& c: r) cells.push_back(c.is_string() ? c.get<std::string>() : c.dump());
                        grid.push_back(cells);
                    }
                    std::vector<std::size_t> widths;
                    for (const std::vector<std::string>& r: grid)
                        for (std::size_t c = 0; c < r.size(); ++c) {
                            if (widths.size() <= c) widths.resize(c + 1, 0);
                            widths[c] = std::max(widths[c], width_(r[c]));
                        }
                    for (std::size_t r = 0; r < grid.size(); ++r) {
                        std::string row;
                        for (std::size_t c = 0; c < grid[r].size(); ++c)
                            row += grid[r][c] + std::string(widths[c] - width_(grid[r][c]) + 2, ' ');
                        add(ftxui::text(row) | (r == 0 && block.contains("columns") ? ftxui::bold | ftxui::color(t.accent) : ftxui::color(t.text)));
                    }
                    return ftxui::vbox(std::move(out));
                }
                if (type == "chart" && block.contains("series") && block["series"].is_array() && !block["series"].empty()) { // bars of the first series
                    const cluster::Json& values = block["series"][0].value("values", cluster::Json::array());
                    const cluster::Json labels = block.value("labels", cluster::Json::array());
                    double biggest = 0.0;
                    for (const cluster::Json& v: values) biggest = std::max(biggest, v.is_number() ? v.get<double>() : 0.0);
                    if (block.contains("title")) add(ftxui::text(block.value("title", std::string())) | ftxui::bold | ftxui::color(t.accent));
                    for (std::size_t i = 0; i < values.size(); ++i) {
                        const double v = values[i].is_number() ? values[i].get<double>() : 0.0;
                        const int bar = biggest > 0 ? static_cast<int>(std::round(v / biggest * std::max(6, width - 22))) : 0;
                        const std::string label = i < labels.size() && labels[i].is_string() ? labels[i].get<std::string>() : std::to_string(i + 1);
                        add(ftxui::hbox({ftxui::text(label.substr(0, 10) + std::string(11 - std::min<std::size_t>(10, width_(label)), ' ')) | ftxui::color(t.muted),
                            ftxui::text(std::string(static_cast<std::size_t>(std::max(0, bar)), '#')) | ftxui::color(t.accent), ftxui::text(" " + cluster::human_tokens(static_cast<std::int64_t>(v)))}));
                    }
                    return ftxui::vbox(std::move(out));
                }
            }
            // markdown: fenced blocks (diff coloured, the others in the code colours), tables, headings, bullets, pictures, text wrapped
            std::vector<std::string> lines;
            {
                // the markdown tables (consecutive | lines) padded to their columns: the header marked \x01, the separator row dropped
                const std::vector<std::string> raw = cluster::split(text, '\n');
                bool fence = false;
                for (std::size_t i = 0; i < raw.size();) {
                    if (raw[i].starts_with("```")) fence = !fence;
                    if (fence || !raw[i].starts_with("|")) {
                        lines.push_back(raw[i++]);
                        continue;
                    }
                    std::vector<std::vector<std::string>> grid;
                    for (; i < raw.size() && raw[i].starts_with("|"); ++i) {
                        std::vector<std::string> cells = cluster::split(raw[i], '|');
                        if (!cells.empty() && cluster::trim(cells.front()).empty()) cells.erase(cells.begin());
                        if (!cells.empty() && cluster::trim(cells.back()).empty()) cells.pop_back();
                        const bool separator = !cells.empty() && std::all_of(cells.begin(), cells.end(), [](const std::string& c) {return cluster::trim(c).find_first_not_of("-: ") == std::string::npos;});
                        if (separator) continue;
                        for (std::string& c: cells)
                            c = cluster::trim(c);
                        grid.push_back(cells);
                    }
                    std::vector<std::size_t> widths;
                    for (const std::vector<std::string>& r: grid)
                        for (std::size_t c = 0; c < r.size(); ++c) {
                            if (widths.size() <= c) widths.resize(c + 1, 0);
                            widths[c] = std::max(widths[c], width_(r[c]));
                        }
                    for (std::size_t r = 0; r < grid.size(); ++r) {
                        std::string row = r == 0 ? "\x01" : "";
                        for (std::size_t c = 0; c < grid[r].size(); ++c)
                            row += grid[r][c] + std::string(widths[c] - width_(grid[r][c]) + 2, ' ');
                        lines.push_back(row);
                    }
                }
            }
            std::size_t signs = 0; // a whole text of + / - lines: a diff
            std::size_t counted = 0;
            for (const std::string& line: lines) {
                if (cluster::trim(line).empty()) continue;
                ++counted;
                signs += (line.starts_with("+") || line.starts_with("-") || line.starts_with("@@") || line.starts_with("diff ") || line.starts_with(" ")) && !line.starts_with("- ") ? 1 : 0;
            }
            const bool wholeDiff = counted >= 2 && signs * 10 >= counted * 8 && text.find("```") == std::string::npos;
            const std::function<ftxui::Element(const std::string&)> diffLine = [&t](const std::string& line) {
                if (line.starts_with("+++") || line.starts_with("---") || line.starts_with("diff ") || line.starts_with("index ")) return ftxui::text(line) | ftxui::bold | ftxui::color(t.muted);
                if (line.starts_with("@@")) return ftxui::text(line) | ftxui::color(t.accent);
                if (line.starts_with("+")) return ftxui::text(line) | ftxui::color(t.ok) | ftxui::bgcolor(ftxui::Color::RGB(0x14, 0x2b, 0x1d));
                if (line.starts_with("-")) return ftxui::text(line) | ftxui::color(t.error) | ftxui::bgcolor(ftxui::Color::RGB(0x33, 0x18, 0x1d));
                return ftxui::text(line) | ftxui::color(t.text);
            };
            bool fenced = false;
            std::string language;
            bool comment = false;
            for (const std::string& raw: lines) {
                const std::string line = raw;
                if (line.starts_with("```")) {
                    fenced = !fenced;
                    language = fenced ? cluster::trim(line.substr(3)) : std::string();
                    comment = false;
                    continue;
                }
                if (wholeDiff || (fenced && (language == "diff" || language == "patch"))) {
                    add(diffLine(line));
                } else if (fenced) {
                    ftxui::Elements parts = {ftxui::text(" ")};
                    for (const cluster::Cell& cell: highlight_(line, language, this->_code, comment)) {
                        ftxui::Element part = ftxui::text(cell.text);
                        if (cell.hasFg) part = part | ftxui::color(ftxui::Color::RGB((cell.fg >> 16) & 0xff, (cell.fg >> 8) & 0xff, cell.fg & 0xff));
                        parts.push_back(part);
                    }
                    add(ftxui::hbox(std::move(parts)) | ftxui::bgcolor(t.surface));
                } else if (line.starts_with("![") && line.find("](") != std::string::npos && image(line.substr(line.find("](") + 2, line.rfind(')') - line.find("](") - 2))) {
                    continue;
                } else if (image(line)) {
                    continue;
                } else if (line.starts_with("\x01")) {
                    add(ftxui::text(line.substr(1)) | ftxui::bold | ftxui::color(t.accent)); // a table header
                } else if (line.starts_with("#")) {
                    add(ftxui::text(cluster::trim(line.substr(line.find_first_not_of('#')))) | ftxui::bold | ftxui::color(t.accent));
                } else if (line.starts_with("- ") || line.starts_with("* ")) {
                    std::vector<std::string> wrapped = wrap_(line.substr(2), static_cast<std::size_t>(std::max(10, width - 2)));
                    for (std::size_t k = 0; k < wrapped.size(); ++k)
                        add(ftxui::text((k == 0 ? "• " : "  ") + wrapped[k]) | ftxui::color(t.text));
                } else if (cluster::trim(line).empty()) {
                    add(ftxui::text(""));
                } else if (line.starts_with("    ") || line.starts_with("\t") || line.find("  ") != std::string::npos) {
                    add(ftxui::text(line) | ftxui::color(t.text)); // a mock-up: its spaces kept
                } else {
                    for (const std::string& part: wrap_(line, static_cast<std::size_t>(std::max(10, width))))
                        add(ftxui::text(part) | ftxui::color(t.text));
                }
            }
            if (more > 0) out.push_back(ftxui::text("… " + std::to_string(more) + " more line(s)") | ftxui::color(t.muted) | ftxui::dim);
            return ftxui::vbox(std::move(out));
        }

        _nodiscard ftxui::Element settings_(const ftxui::Dimensions& size)
        {
            const Theme& t = this->_theme;
            const std::function<ftxui::Element(const std::string&, const ftxui::Event&)> keyed = [this](const std::string& label, const ftxui::Event& key) {
                return this->tap_(ftxui::text(label), 2, [this, key]() {(void)this->event_(key);});
            };
            ftxui::Elements tabs;
            std::vector<int> tabWidths;
            for (std::size_t i = 0; i < this->_pages.size(); ++i) {
                const bool active = static_cast<int>(i) == this->_page;
                ftxui::Element tab = ftxui::text("  " + this->_pages[i].title + "  ");
                tabWidths.push_back(ftxui::string_width(this->_pages[i].title) + 4);
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
                if (this->servicesSetting_("refresh") != "manual") this->_spinning = true;
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
                const bool typing = selected && this->_editing;  // its value edited: a field with its cursor (the page keeps it at Enter / Esc)
                if (typing) value = this->_edit;
                else if (setting.kind == cluster::SettingKind::Bool) value = value == "true" ? "● Enabled" : "○ Disabled";
                else if (setting.kind == cluster::SettingKind::Choice) value = "‹ " + (value.empty() ? std::string("(agent default)") : value) + " ›";
                else if (setting.kind == cluster::SettingKind::Action && page.section == "services") value = cluster::service_summary(value, this->_services); // a sub service: its state
                else if (setting.kind == cluster::SettingKind::Action) value = "▶ " + value;
                else if (value.empty()) value = "(empty)";
                const bool typed = setting.kind != cluster::SettingKind::Bool && setting.kind != cluster::SettingKind::Choice; // between brackets; a switch and a choice have their own marks
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
                    typing ? ftxui::hbox({ftxui::text("[") | ftxui::bold | ftxui::color(t.accent), this->fieldView_(this->_edit, 58) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, std::clamp(ftxui::string_width(this->_edit) + 2, 4, 58)), // the ] right after the text, one space more
                        ftxui::text("]") | ftxui::bold | ftxui::color(t.accent)})
                    : this->tap_(this->keysValue_(setting, value, typed, selected), 2, [pick]() {pick(true);}, clip),
                    need.problem.empty() ? ftxui::text("") : ftxui::text("  ✗ " + cluster::one_line(need.problem.substr(0, need.problem.find(':')), 34)) | ftxui::color(t.error) | ftxui::bold,
                    ftxui::filler(),
                });
                rows.push_back(selected ? line | ftxui::focus : line);
                if (typing) { // the suggestions of what is typed, under the field (Tab takes the first, ↑↓ choose, Enter takes the chosen one)
                    const std::vector<std::string> suggestions = cluster::setting_suggestions(this->_manager.config(), setting, this->_edit);
                    for (std::size_t s = 0; s < suggestions.size(); ++s) {
                        const bool chosen = this->_suggestOpen && static_cast<int>(s) == this->_suggest;
                        rows.push_back(ftxui::text("      ↳ " + cluster::one_line(suggestions[s], 80)) | (chosen ? ftxui::color(t.bg) | ftxui::bgcolor(t.accent) : s == 0 && !this->_suggestOpen ? ftxui::color(t.accent) : ftxui::color(t.muted)));
                    }
                    if (!suggestions.empty()) rows.push_back(ftxui::text("      Tab take the first · ↑↓ choose · Enter take it") | ftxui::color(t.muted) | ftxui::dim);
                    const std::vector<std::string> bad = cluster::setting_invalid_parts(setting, this->_edit);
                    if (!bad.empty()) { // the keys that are none, named under the field
                        std::string names;
                        for (const std::string& b: bad)
                            names += (names.empty() ? "" : ", ") + b;
                        rows.push_back(ftxui::text("      ✗ not a key: " + names + "  (Ctrl / Alt / Shift + a letter, F1..F12, Tab, Enter, Escape, PageUp...)") | ftxui::color(t.error) | ftxui::bold);
                    }
                }
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
                rows.push_back(ftxui::hbox({ftxui::text(" / ") | ftxui::bold | ftxui::color(t.accent), this->fieldView_(this->_find, 50) | ftxui::bold,
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
            std::string note = this->_note == "confirm" || this->_note == "danger_save" || this->_note == "svc_confirm" ? std::string() : this->_note; // those: a popup
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
                        for (const std::string& line: wrap_(need.problem, 80)) // text lines: a paragraph lets the page show through its spaces
                            lines.push_back(ftxui::text("   " + line + " ") | ftxui::color(t.muted));
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
                    for (const std::string& line: wrap_(cluster::setting_danger(setting, value), 80)) // text lines: a paragraph lets the page show through its spaces
                        lines.push_back(ftxui::text("   " + line + " ") | ftxui::color(t.muted));
                    lines.push_back(ftxui::text(""));
                }
                lines.push_back(ftxui::hbox({installable ? keyed(" [i] install, then save ", ftxui::Event::Character('i')) | ftxui::color(t.ok) | ftxui::bold : ftxui::text(""),
                    installable ? ftxui::text("   ") : ftxui::text(""),
                    keyed(missing.empty() ? " [y] save them " : " [y] save anyway ", ftxui::Event::Character('y')) | ftxui::color(t.error) | ftxui::bold, ftxui::text("   "),
                    keyed(" [n] / Esc back (nothing saved) ", ftxui::Event::Character('n')) | ftxui::color(t.accent)}));
                popup = ftxui::vbox(std::move(lines)) | ftxui::borderRounded | ftxui::color(t.warn) | ftxui::bgcolor(t.bg) | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 90)
                    | ftxui::center;
            } else if (this->_note == "confirm") {
                // leaving with changes not saved: each one listed, saved or dropped
                ftxui::Elements lines = {ftxui::text(" Save the " + std::to_string(this->_changes.size()) + " change(s)? ") | ftxui::bold | ftxui::color(t.accent), ftxui::text("")};
                std::size_t shown = 0;
                for (const auto &[key, change]: this->_changes) {
                    if (++shown > 12) {
                        lines.push_back(ftxui::text("   … " + std::to_string(this->_changes.size() - 12) + " more") | ftxui::color(t.muted));
                        break;
                    }
                    lines.push_back(ftxui::hbox({ftxui::text("   " + change.first.label) | ftxui::bold | ftxui::color(t.text),
                        ftxui::text(" = " + (change.second.empty() ? std::string("(empty)") : cluster::one_line(change.second, 46))) | ftxui::color(t.muted)}));
                }
                lines.push_back(ftxui::text(""));
                lines.push_back(ftxui::hbox({keyed(" [y] save ", ftxui::Event::Character('y')) | ftxui::color(t.ok) | ftxui::bold, ftxui::text("   "),
                    keyed(" [n] drop them ", ftxui::Event::Character('n')) | ftxui::color(t.error)}));
                popup = ftxui::vbox(std::move(lines)) | ftxui::borderRounded | ftxui::color(t.accent) | ftxui::bgcolor(t.bg) | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 80)
                    | ftxui::center;
            } else if (this->_note == "svc_confirm") {
                // a risky action of the Services tab (the core, a removal): what it does, y runs it
                const std::vector<std::string> parts = cluster::split(this->_svcConfirm, '|');
                const std::string service = parts.empty() ? std::string() : parts[0];
                const std::string action = parts.size() > 1 ? parts[1] : std::string();
                const std::string label = parts.size() > 2 ? parts[2] : action;
                const std::string what = service == "core" && action == "restart" ? "the core stops and its terminals / windows start it again: the turns running are interrupted, the sessions resumed"
                    : service == "core" && action == "stop" ? "the core and every terminal / window of it stop: the turns running are interrupted (the sessions kept for the next start)"
                    : service == "service" && action == "remove" ? "the systemd service of the wake phrase removed (Setup writes it again)" : label;
                ftxui::Elements lines = {ftxui::text(" " + label + " (" + service + ")? ") | ftxui::bold | ftxui::color(t.warn), ftxui::text("")};
                for (const std::string& line: wrap_(what, 70)) // text lines (a paragraph lets the page show through its spaces)
                    lines.push_back(ftxui::text(" " + line + " ") | ftxui::color(t.text));
                lines.push_back(ftxui::text(""));
                lines.push_back(ftxui::hbox({keyed(" [y] yes ", ftxui::Event::Character('y')) | ftxui::color(t.error) | ftxui::bold, ftxui::text("   "),
                    keyed(" [n] / Esc no ", ftxui::Event::Character('n')) | ftxui::color(t.accent), ftxui::filler()}));
                popup = ftxui::vbox(std::move(lines)) | ftxui::borderRounded | ftxui::color(t.warn) | ftxui::bgcolor(t.bg) | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 80)
                    | ftxui::center;
            }
            // the tabs: on as many rows as the width needs (none cut off on the right when the pages are many)
            ftxui::Elements tabRows;
            {
                ftxui::Elements current;
                int used = 0;
                for (std::size_t i = 0; i < tabs.size(); ++i) {
                    if (!current.empty() && used + tabWidths[i] > size.dimx) {
                        tabRows.push_back(ftxui::hbox(std::move(current)) | ftxui::bgcolor(t.surface));
                        current.clear();
                        used = 0;
                    }
                    used += tabWidths[i];
                    current.push_back(std::move(tabs[i]));
                }
                if (!current.empty()) tabRows.push_back(ftxui::hbox(std::move(current)) | ftxui::bgcolor(t.surface));
            }
            const ftxui::Element tabBar = ftxui::vbox(std::move(tabRows));
            return ftxui::dbox({ftxui::vbox({
                ftxui::hbox({ftxui::text(" claude-cluster setup ") | ftxui::bold | ftxui::color(t.bg) | ftxui::bgcolor(t.accent), ftxui::filler(),
                    ftxui::text(" config.toml ") | ftxui::color(t.muted)}),
                tabBar,
                ftxui::separatorHeavy() | ftxui::color(t.accent),
                ftxui::hbox({ftxui::vbox(std::move(rows)) | ftxui::yframe | this->clipBox_(clip) | ftxui::flex, ftxui::separatorLight() | ftxui::color(t.border), helpBox}) | ftxui::flex,
                ftxui::separatorHeavy() | ftxui::color(t.accent),
                ftxui::text(" " + note) | ftxui::color(t.warn),
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
            const bool grid = this->_manager.ui("layout", this->_manager.config().layout) == "grid" && !this->sessions_().empty();
            // the grid: the Tasks is a cell; left leaves it, up / down leave it from the first / the last item (nothing to choose: always)
            if (grid && event == ftxui::Event::ArrowLeft) {
                this->moveDir_(-1, 0);
                return true;
            }
            if (grid && ((event == ftxui::Event::ArrowUp && this->_taskSel <= 0) || (event == ftxui::Event::ArrowDown && this->_taskSel >= count - 1))) {
                this->moveDir_(0, event == ftxui::Event::ArrowUp ? -1 : 1);
                return true;
            }
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
                if (const std::string traits = cluster::task_traits(task); !traits.empty()) out.push_back(ftxui::text("       · " + cluster::one_line(traits, static_cast<std::size_t>(room - 10))) | ftxui::color(t.muted) | ftxui::dim);
                if (task.status == "done" && !task.result.empty() && i < 4) out.push_back(ftxui::text("       = " + cluster::one_line(task.result, static_cast<std::size_t>(room - 10))) | ftxui::color(t.muted) | ftxui::dim);
            }
            if (shown.size() > 10) out.push_back(ftxui::text("     … +" + std::to_string(shown.size() - 10) + " more") | ftxui::color(t.muted) | ftxui::dim);
            // the heavy jobs of the resource orchestrator: the gauges of the computer, what runs, waits, is stopped (a click: kill / stop / resume / priority)
            {
                const cluster::Json jobs = this->_manager.jobsState();
                if (jobs.is_object() && !jobs.empty()) {
                    const cluster::Json host = jobs.value("host", cluster::Json::object());
                    const int level = jobs.value("level", 0);
                    title("Heavy jobs", std::to_string(jobs.value("running", 0)) + "/" + std::to_string(jobs.value("slots", 0)) + " running · " + std::to_string(jobs.value("waiting", 0)) + " waiting"
                        + (jobs.value("stopped", 0) > 0 ? " · " + std::to_string(jobs.value("stopped", 0)) + " stopped" : "") + " · profile " + jobs.value("profile", std::string("low"))
                        + (jobs.value("enabled", true) ? "" : " · OFF"));
                    char gauge[200];
                    std::snprintf(gauge, sizeof(gauge), "   load %.1f/core · %ld MB free · swap %ld MB%s", host.value("load_per_core", 0.0), host.value("mem_avail_mb", 0L), host.value("swap_used_mb", 0L),
                        host.value("psi_mem", -1.0) >= 0 ? (" · stall cpu " + std::to_string(static_cast<int>(host.value("psi_cpu", 0.0))) + "% mem " + std::to_string(static_cast<int>(host.value("psi_mem", 0.0)))
                        + "% io " + std::to_string(static_cast<int>(host.value("psi_io", 0.0))) + "%").c_str() : "");
                    out.push_back(ftxui::hbox({ftxui::text(gauge) | ftxui::color(t.muted), ftxui::text(level == 0 ? "  ● calm" : level == 1 ? "  ● hold: nothing new starts" : "  ● reduce: jobs slowed")
                        | ftxui::color(level == 0 ? t.ok : level == 1 ? t.warn : t.error), ftxui::filler()}));
                    if (level > 0 && !jobs.value("pressure", std::string()).empty()) out.push_back(ftxui::text("       ↳ " + jobs.value("pressure", std::string())) | ftxui::color(t.warn) | ftxui::dim);
                    std::size_t listed = 0;
                    for (const cluster::Json& job: jobs.value("jobs", cluster::Json::array())) {
                        if (++listed > 12) break;
                        const std::string state = job.value("state", std::string());
                        const ftxui::Color tint = state == "running" ? t.ok : state == "queued" ? t.accent : state == "stopped" ? t.warn : state == "done" ? t.muted : state == "failed" ? t.error : t.warn;
                        const std::string mark = state == "running" ? "▶" : state == "queued" ? "◌" : state == "stopped" ? "⏸" : state == "done" ? "✓" : state == "failed" ? "✗" : "⊘";
                        const std::string session = job.value("session", std::string());
                        const std::string age = state == "queued" ? "#" + std::to_string(job.value("position", 0)) + " ETA " + span(job.value("eta_s", std::int64_t{0})) : state == "running" ? span(now - job.value("started", now))
                            : state == "stopped" ? "stopped" : span(now - job.value("ended", now)) + " ago";
                        const std::string id = job.value("id", std::string());
                        out.push_back(item(4, ftxui::hbox({ftxui::text("  " + mark + " ") | ftxui::color(tint) | ftxui::bold, ftxui::text(id + "  ") | ftxui::color(t.muted),
                            ftxui::text(job.value("class", std::string()) + "  ") | ftxui::color(t.accent) | ftxui::dim,
                            ftxui::text(cluster::one_line(job.value("name", std::string()).empty() ? job.value("command", std::string()) : job.value("name", std::string()), static_cast<std::size_t>(std::max(16, room - 60))))
                            | ftxui::color(state == "done" ? t.muted : t.text), ftxui::filler(), ftxui::text("  → " + cluster::one_line(session.empty() ? std::string("you") : who(session), 14)) | ftxui::color(t.muted),
                            ftxui::text("  " + age + " ") | ftxui::color(t.muted) | ftxui::dim}), [this, id]() {this->pick_("job:" + id);}));
                        if (state == "queued" && !job.value("waiting_for", std::string()).empty()) out.push_back(ftxui::text("       ↳ waits for " + cluster::one_line(job.value("waiting_for", std::string()), static_cast<std::size_t>(room - 20))) | ftxui::color(t.muted) | ftxui::dim);
                        if (state == "stopped") out.push_back(ftxui::text("       ↳ " + cluster::one_line(job.value("message", std::string()), static_cast<std::size_t>(room - 10))) | ftxui::color(t.warn) | ftxui::dim);
                        if (state == "killed" || state == "failed") out.push_back(ftxui::text("       ↳ " + cluster::one_line(job.value("message", std::string()), static_cast<std::size_t>(room - 10))) | ftxui::color(t.warn) | ftxui::dim);
                    }
                    for (const cluster::Json& d: jobs.value("detected", cluster::Json::array()))
                        out.push_back(ftxui::text("  ! outside the queue: " + cluster::one_line(d.value("command", std::string()), static_cast<std::size_t>(std::max(20, room - 40))) + " (" + std::to_string(d.value("cpu_seconds", 0))
                            + " s CPU, " + who(d.value("session", std::string())) + ")") | ftxui::color(t.warn) | ftxui::dim);
                    if (jobs.value("jobs", cluster::Json::array()).empty()) out.push_back(ftxui::text("   none: heavy commands go through cluster-run / heavy_run and wait here for a slot") | ftxui::color(t.muted) | ftxui::dim);
                }
            }
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
                const std::string when = open ? "in " + span(w.at - now) + " (" + cluster::clock_time(w.at) + ")" : w.status == "paused" ? "paused" : w.status == "done" ? "run " + span(now - w.last) + " ago" : "cancelled";
                const std::string where = w.action == "notify" ? "a notification" : w.action == "spawn" ? "a new session" : w.action == "task" ? "a task" : w.action == "command" ? "a command" : "→ " + (w.session == GLOBAL_ID ? std::string("global") : (this->_manager.snapshot(w.session) ? this->_manager.snapshot(w.session)->spec.name : w.session));
                const std::string id = w.id;
                out.push_back(item(2, ftxui::hbox({ftxui::text(open ? "  ⏱ " : w.status == "done" ? "  ✓ " : "  ⊘ ") | ftxui::color(open ? t.accent : w.status == "done" ? t.muted : t.warn) | ftxui::bold,
                    ftxui::text(w.id + "  ") | ftxui::color(t.muted), ftxui::text(when + (w.every > 0 ? " · every " + span(static_cast<std::int64_t>(w.every) * 60) : "") + "  ") | ftxui::color(open ? t.text : t.muted),
                    ftxui::text(cluster::one_line(w.text, static_cast<std::size_t>(std::max(20, room - 70)))) | ftxui::color(open ? t.text : t.muted), ftxui::filler(),
                    ftxui::text("  " + where) | ftxui::color(t.muted), ftxui::text("  by " + who(w.by) + " ") | ftxui::color(t.muted) | ftxui::dim}), [this, id]() {this->pick_("sched:" + id);}));
                if (!w.reason.empty()) out.push_back(ftxui::text("       ↳ " + cluster::one_line(w.reason, static_cast<std::size_t>(room - 10))) | ftxui::color(t.warn) | ftxui::dim);
                if (const std::string rule = cluster::schedule_rule(w); !rule.empty()) out.push_back(ftxui::text("       · " + cluster::one_line(rule, static_cast<std::size_t>(room - 10))) | ftxui::color(t.muted) | ftxui::dim);
                if (!w.note.empty() && w.runs + w.skipped > 0) out.push_back(ftxui::text("       ↻ " + cluster::one_line(w.note, static_cast<std::size_t>(room - 10))) | ftxui::color(t.muted) | ftxui::dim);
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
                "the prompt: a task (target | prompt: for it), +30m <text> / 18:00 <text>: a reminder to the global session; a schedule: pause, resume, snooze, run now, edit") | ftxui::color(t.muted) | ftxui::dim);
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
            ftxui::Elements lines = {line, sub};
            if (!s.spec.category.empty() || !s.spec.tags.empty()) {
                // its category (a filled chip) and its tags (colored #words) on a line of their own, as many as fit
                ftxui::Elements chips = {ftxui::text(" ")};
                std::size_t used = 1;
                if (!s.spec.category.empty()) {
                    chips.push_back(ftxui::text(" " + cluster::one_line(s.spec.category, 14) + " ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->chip_color_(s.spec.category)));
                    used += width_(cluster::one_line(s.spec.category, 14)) + 3;
                }
                for (const std::string& tag: s.spec.tags) {
                    const std::string text = "#" + cluster::one_line(tag, 12);
                    if (used + width_(text) + 1 > 28) break;
                    chips.push_back(ftxui::text(" " + text) | ftxui::color(this->chip_color_(tag)));
                    used += width_(text) + 1;
                }
                lines.push_back(ftxui::hbox(std::move(chips)));
            }
            ftxui::Element block = this->tap_(ftxui::vbox(std::move(lines)), 0, [this, id]() {this->_active = id;}, clip); // clicked: shown
            return active ? block | ftxui::bgcolor(this->_theme.surface) : block; // no focus: it would take the cursor of the prompt
        }

        _nodiscard ftxui::Element transcript_(const cluster::Snapshot& s, const int width, const int height, const bool compact, const std::string& owner = "")
        {
            // Wrapped lines of the transcript, the last ones (minus the scroll); kept as text for the mouse selection
            struct Row {
                std::string text;
                ftxui::Decorator style;
                std::string join;   // before it when copied
                std::vector<cluster::Cell> cells{};     // a line of a picture: its runs of colors (text: what is copied)
                std::vector<Hit> hits{};                // its parts clicked
                CodeRow code{};                         // a row of a code block: its text as written, its cells
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
            const std::string sid = owner.empty() ? s.spec.id : owner;   // the pinned item: the session it is pinned on
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
            // a fenced code block: boxed, its language and the ⧉ copy button (a click) on its top line, its lines coloured; each
            // row keeps its block, its cells of text and its code as written (the selection copies that, not the box)
            const std::function<void(const std::string&, const std::vector<std::string>&, const std::string&)> codeBox =
                [&](const std::string& language, const std::vector<std::string>& code, const std::string& indent) {
                    const std::size_t inner = room > width_(indent) + 6 ? room - width_(indent) - 4 : 20;
                    const std::string label = language.empty() ? std::string("code") : language;
                    const std::string copy = "  ⧉ copy  ";     // 10 cells, like " ✓ copied "
                    std::string all;
                    for (const std::string& line: code)
                        all += (all.empty() ? "" : "\n") + line;
                    const std::string id = std::to_string(std::hash<std::string>{}(language + "\n" + all) & 0x7fffffff);
                    const int block = static_cast<int>(std::stoll(id));
                    const bool copied = this->_copiedBlock == id && now_ms_() - this->_copiedAt < 1500;
                    if (copied) this->_animating = true; // drawn again when "copied" goes
                    const std::string shown = copied ? " ✓ copied " : copy;
                    const int textFrom = static_cast<int>(width_(indent)) + 2;
                    std::string top = indent + "╭─ " + label + " ";
                    const std::size_t used = width_(top) - width_(indent);
                    for (std::size_t k = used + width_(shown) + 1; k < inner + 4; ++k)
                        top += "─";
                    const int copyAt = static_cast<int>(width_(top));
                    top += shown + "╮";
                    lines.push_back({top, ftxui::color(this->_theme.border), "\n"});
                    lines.back().code = {block, false, true, copyAt, copyAt + static_cast<int>(width_(shown)), ""};
                    lines.back().hits.emplace_back(copyAt, copyAt + static_cast<int>(width_(shown)) - 1, [this, all, id]() {
                        cluster::detach({"wl-copy", "--", all});
                        this->_copiedBlock = id;
                        this->_copiedAt = now_ms_();
                        this->status_("code copied (" + std::to_string(all.size()) + " characters)");
                    });
                    bool comment = false;
                    for (const std::string& raw: code) {
                        // a line longer than the box goes on the next rows (a part of the raw line each, joined back when copied)
                        std::vector<std::string> parts;
                        std::string part;
                        std::size_t size = 0;
                        for (std::size_t at = 0; at < raw.size();) {
                            std::size_t end = at + 1;
                            while (end < raw.size() && (static_cast<unsigned char>(raw[end]) & 0xC0) == 0x80)
                                ++end;
                            const std::size_t cells = raw[at] == '\t' ? 4 : 1;
                            if (size + cells > inner && !part.empty()) {
                                parts.push_back(part);
                                part.clear();
                                size = 0;
                            }
                            part += raw.substr(at, end - at);
                            size += cells;
                            at = end;
                        }
                        parts.push_back(part);
                        for (std::size_t n = 0; n < parts.size(); ++n) {
                            std::string line = parts[n];
                            for (std::size_t tab = line.find('\t'); tab != std::string::npos; tab = line.find('\t'))
                                line.replace(tab, 1, "    ");
                            std::vector<cluster::Cell> cells = {{indent + "│ ", this->_code.comment, 0, true, false}};
                            for (const cluster::Cell& cell: highlight_(line, language, this->_code, comment))
                                cells.push_back(cell);
                            const std::size_t drawn = width_(line);
                            cells.push_back({std::string(inner > drawn ? inner - drawn : 0, ' ') + " │", this->_code.comment, 0, true, false});
                            lines.push_back({indent + "│ " + line, ftxui::color(this->_theme.text), n == 0 ? "\n" : "", cells});
                            lines.back().code = {block, true, false, textFrom, textFrom + static_cast<int>(drawn), parts[n]};
                        }
                    }
                    std::string bottom = indent + "╰";
                    for (std::size_t k = 0; k < inner + 2; ++k)
                        bottom += "─";
                    lines.push_back({bottom + "╯", ftxui::color(this->_theme.border), "\n"});
                    lines.back().code = {block, false, false, 0, 0, ""};
                };
            const std::function<void(std::size_t, const std::function<void(void)>&)> clickable = [&](const std::size_t from, const std::function<void(void)>& run) {
                for (std::size_t k = from; k < lines.size(); ++k)
                    lines[k].hits.emplace_back(0, 100000, run);
            };
            // Markdown (an answer, a markdown display): its first line after `head`, the next ones indented by 2
            std::function<void(const std::string&, const std::string&, const ftxui::Decorator&, const std::string&)> markdown;
            const std::function<void(const cluster::Json&, const std::string&)> display = [&](const cluster::Json& d, const std::string& key) {
                const Theme& th = this->_theme;
                const std::string displayId = d.value("id", std::string());
                const std::string type = d.value("type", std::string());
                const std::string title = d.value("title", std::string());
                const std::vector<ftxui::Color> palette = {th.accent, th.ok, th.warn, th.error, th.text};
                const std::function<std::string(const cluster::Json&)> cell = [](const cluster::Json& v) {return v.is_string() ? v.get<std::string>() : v.dump(-1, ' ', false, cluster::JSON_LENIENT);};
                const std::function<std::string(const std::string&, const std::size_t)> pad = [](const std::string& text, const std::size_t size) {
                    const std::string cut = width_(text) > size ? cluster::one_line(text, size) : text;
                    return cut + std::string(size > width_(cut) ? size - width_(cut) : 0, ' ');
                };
                if (!title.empty()) lines.push_back({"▣ " + title, ftxui::color(th.accent) | ftxui::bold, "\n"});
                if (type == "code") {
                    codeBox(d.value("language", std::string()), lines_(d.value("code", std::string())), "  ");
                } else if (type == "markdown") {
                    markdown(d.value("text", std::string()), "  ", ftxui::nothing, key + ":md");
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
                    // a form: a line per field (its label, a slot / its choices / a yes-no), a click opens it in its popup there
                    const cluster::Json values = d.value("values", cluster::Json::object());
                    const cluster::Json fields = d.value("fields", cluster::Json::array());
                    const bool sent = d.value("submitted", false);
                    const std::string name = title.empty() ? std::string("the form") : title;
                    std::size_t labelWidth = 4;
                    for (const cluster::Json& field: fields)
                        labelWidth = std::min<std::size_t>(24, std::max(labelWidth, width_(field.value("label", field.value("name", std::string())))));
                    const std::size_t slot = room > labelWidth + 12 ? std::min<std::size_t>(48, room - labelWidth - 8) : 16;
                    for (std::size_t k = 0; k < fields.size(); ++k) {
                        const cluster::Json& field = fields[k];
                        const std::string fieldName = field.value("name", std::string());
                        const std::string kind = field.value("type", std::string(field.contains("options") ? "choice" : "text"));
                        const cluster::Json given = values.contains(fieldName) ? values[fieldName] : field.value("value", cluster::Json(""));
                        const std::string value = given.is_string() ? given.get<std::string>() : given.is_boolean() ? (given.get<bool>() ? "yes" : "no") : given.dump(-1, ' ', false, cluster::JSON_LENIENT);
                        Row row{"", ftxui::color(th.text), "\n"};
                        const std::string label = "  " + pad(field.value("label", fieldName), labelWidth) + "  ";
                        row.cells.push_back({label, this->_code.text, 0, true, false});
                        row.text = label;
                        if (kind == "choice") {
                            for (const cluster::Json& o: field.value("options", cluster::Json::array())) {
                                const std::string option = o.is_string() ? o.get<std::string>() : o.dump(-1, ' ', false, cluster::JSON_LENIENT);
                                const bool on = option == value || (value.empty() && option == cell(field["options"][0]));
                                const std::string chip = std::string(on ? "(•) " : "( ) ") + option;
                                const int from = static_cast<int>(width_(row.text));
                                row.cells.push_back({chip, on ? this->_code.keyword : this->_code.text, 0, true, false});
                                row.text += chip;
                                if (!sent)
                                    row.hits.emplace_back(from, static_cast<int>(width_(row.text)) - 1, [this, sid, displayId, name, fields, values, k, option]() {
                                        this->openForm_(sid, displayId, name, fields, values, k);
                                        this->_form[k].value = option;
                                    });
                                row.cells.push_back({"   ", 0, 0, false, false});
                                row.text += "   ";
                            }
                        } else if (kind == "bool") {
                            const bool yes = value == "yes" || value == "true";
                            const std::string box = yes ? "☑ yes" : "☐ no";
                            const int from = static_cast<int>(width_(row.text));
                            row.cells.push_back({box, yes ? this->_code.string : this->_code.text, 0, true, false});
                            row.text += box;
                            if (!sent)
                                row.hits.emplace_back(from, static_cast<int>(width_(row.text)) - 1, [this, sid, displayId, name, fields, values, k, yes]() {
                                    this->openForm_(sid, displayId, name, fields, values, k);
                                    this->_form[k].value = yes ? "no" : "yes";
                                });
                        } else {
                            const std::string hint = value.empty() ? (kind == "number" ? "a number" : kind == "date" ? "YYYY-MM-DD" : "click to type") : "";
                            std::string shown = " " + cut_(value.empty() ? hint : value, slot - 2);
                            shown += std::string(slot > width_(shown) ? slot - width_(shown) : 0, ' ');
                            const int from = static_cast<int>(width_(row.text));
                            row.cells.push_back({shown, value.empty() ? this->_code.comment : this->_code.text, this->_code.surface, true, true});
                            row.text += shown;
                            if (!sent)
                                row.hits.emplace_back(from, static_cast<int>(width_(row.text)) - 1, [this, sid, displayId, name, fields, values, k]() {
                                    this->openForm_(sid, displayId, name, fields, values, k);
                                });
                        }
                        lines.push_back(row);
                    }
                    if (sent) {
                        lines.push_back({"  ✓ sent", ftxui::color(th.ok) | ftxui::dim, "\n"});
                    } else {
                        Row buttons{"  [ ▶ Fill the form ]  [ ✓ Submit ]   a click on a field: there · the palette too", ftxui::color(th.accent) | ftxui::bold, "\n"};
                        buttons.hits.emplace_back(2, 20, [this, sid, displayId, name, fields, values]() {this->openForm_(sid, displayId, name, fields, values, 0);});
                        buttons.hits.emplace_back(23, 36, [this, sid, displayId, name, fields, values]() {
                            this->openForm_(sid, displayId, name, fields, values, 0);
                            this->formSubmit_(); // the values shown sent as they are
                        });
                        lines.push_back(buttons);
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
                    // ▶ / ⏸, its name, the time played and its length, a bar of its progress; a click plays / stops it here
                    const double length = audio_seconds_(cluster::display_bytes(d));
                    const auto it = this->_audioStart.find(displayId);
                    const double played = it == this->_audioStart.end() ? -1.0 : static_cast<double>(now_ms_() - it->second) / 1000.0;
                    const bool playing = played >= 0 && played < (length > 0 ? length + 0.3 : 30.0);
                    if (!playing && it != this->_audioStart.end()) this->_audioStart.erase(it);
                    if (playing) this->_animating = true;
                    const std::function<std::string(double)> clock = [](const double s) {
                        const int t = static_cast<int>(std::max(0.0, s));
                        return std::to_string(t / 60) + ":" + (t % 60 < 10 ? "0" : "") + std::to_string(t % 60);
                    };
                    std::string bar;
                    const int size = 24;
                    const int done = playing && length > 0 ? static_cast<int>(std::min(1.0, played / length) * size) : 0;
                    for (int k = 0; k < size; ++k)
                        bar += k < done ? "━" : k == done && playing ? "●" : "─";
                    const std::string name = d.value("alt", d.value("text", std::string("audio")));
                    const std::string line = std::string("  ") + (playing ? "⏸" : "▶") + " ♪ " + cut_(name, std::min<std::size_t>(40, room / 3)) + "  " + clock(playing ? played : 0)
                        + " " + bar + " " + (length > 0 ? clock(length) : std::string("?:??")) + "  (a click: " + (playing ? "stop" : "play") + ")";
                    lines.push_back({line, playing ? ftxui::color(th.ok) : ftxui::color(th.accent), "\n"});
                    lines.back().hits.emplace_back(0, 100000, [this, sid, displayId, playing]() {
                        try {
                            if (playing) this->_audioStart.erase(displayId);
                            else this->_audioStart[displayId] = now_ms_();
                            this->status_(this->_manager.displayAct(sid, displayId, playing ? cluster::Json{{"stop", true}} : cluster::Json::object()));
                        } catch (const utils::exception::IException& e) {
                            this->_audioStart.erase(displayId);
                            this->status_(e.info(), true);
                        }
                    });
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
            markdown = [&](const std::string& source, const std::string& head, const ftxui::Decorator& mark, const std::string& key) {
                // code blocks boxed and coloured (⧉ copy), tables boxed, headings, bullets, rules, the marks out
                const std::vector<std::string> all = lines_(source);
                bool first = true;
                const std::function<std::string(void)> lead = [&first, head]() {
                    const std::string out = first ? head : "  ";
                    first = false;
                    return out;
                };
                const std::function<bool(const std::string&)> separator = [](const std::string& line) {
                    const std::string t = cluster::trim(line);
                    return t.starts_with("|") && t.find("---") != std::string::npos && t.find_first_not_of("|-: ") == std::string::npos;
                };
                const std::function<std::vector<std::string>(const std::string&)> cells = [](const std::string& line) {
                    std::string t = cluster::trim(line);
                    if (t.starts_with("|")) t = t.substr(1);
                    if (t.ends_with("|")) t.pop_back();
                    std::vector<std::string> out;
                    for (const std::string& c: cluster::split(t, '|'))
                        out.push_back(std::regex_replace(cluster::trim(c), std::regex(R"(\*\*|__|`)"), ""));
                    return out;
                };
                for (std::size_t k = 0; k < all.size(); ++k) {
                    const std::string trimmed = cluster::trim(all[k]);
                    if (trimmed.starts_with("```")) {
                        const std::string language = cluster::trim(trimmed.substr(3));
                        std::vector<std::string> fenced;
                        std::size_t j = k + 1;
                        while (j < all.size() && !cluster::trim(all[j]).starts_with("```")) fenced.push_back(all[j++]);
                        if (first && head != "  ") lines.push_back({lead(), ftxui::color(this->_theme.text) | mark, "\n"});
                        first = false;
                        codeBox(language, fenced, "  ");
                        k = j;
                        continue;
                    }
                    if (trimmed.starts_with("|") && k + 1 < all.size() && separator(all[k + 1])) {
                        cluster::Json table = {{"type", "table"}, {"columns", cells(all[k])}, {"rows", cluster::Json::array()}};
                        std::size_t j = k + 2;
                        while (j < all.size() && cluster::trim(all[j]).starts_with("|")) table["rows"].push_back(cells(all[j++]));
                        if (first && head != "  ") lines.push_back({lead(), ftxui::color(this->_theme.text) | mark, "\n"});
                        first = false;
                        display(table, key + ":" + std::to_string(k));
                        k = j - 1;
                        continue;
                    }
                    const std::string plain = std::regex_replace(std::regex_replace(all[k], std::regex(R"(\*\*|__|`)"), ""),
                        std::regex(R"(\[([^\]]+)\]\(([^)]+)\))"), "$1 ($2)");
                    const std::string p = cluster::trim(plain);
                    if (p.starts_with("#") && p.find_first_not_of('#') != std::string::npos && p[p.find_first_not_of('#')] == ' ') {
                        add(cluster::trim(p.substr(p.find_first_not_of('#'))), lead(), "  ", ftxui::color(this->_theme.accent) | ftxui::bold | mark);
                    } else if ((p.starts_with("- ") || p.starts_with("* ")) && plain.find_first_not_of(' ') < 8) {
                        const std::string indent(2 + plain.find_first_not_of(' '), ' ');
                        const std::string lead1 = lead();
                        add(p.substr(2), (lead1 != "  " ? lead1 : indent) + "• ", indent + "  ", ftxui::color(this->_theme.text) | mark);
                    } else if (p == "---" || p == "***") {
                        lines.push_back({lead() + std::string(std::min<std::size_t>(room - 4, 60), '-'), ftxui::color(this->_theme.border), "\n"});
                    } else {
                        add(plain, lead(), "  ", ftxui::color(this->_theme.text) | mark);
                    }
                }
                if (first && head != "  ") add("", head, "  ", ftxui::color(this->_theme.text) | mark);
            };
            std::vector<std::pair<std::size_t, std::size_t>> starts;   // the first line of each entry
            for (std::size_t i = start; i < frame.visible && i < s.entries.size(); ++i) {
                const cluster::Entry& e = s.entries[i];
                const bool matched = searching && std::find(this->_matches.begin(), this->_matches.end(), i) != this->_matches.end();
                if (i == hitEntry) hitLine = static_cast<int>(lines.size());
                const ftxui::Decorator mark = i == hitEntry ? ftxui::inverted : matched ? ftxui::bgcolor(this->_theme.surface) : ftxui::nothing;
                const std::string text = i == frame.typed ? e.text.substr(0, frame.typedBytes) : e.text;
                starts.emplace_back(lines.size(), i);
                const bool block = e.kind != cluster::EntryKind::ToolResult && e.kind != cluster::EntryKind::User && !(compact && e.kind == cluster::EntryKind::Thinking);
                if (block && !lines.empty() && !lines.back().text.empty()) lines.push_back({"", ftxui::nothing, "\n"});
                switch (e.kind) {
                    case cluster::EntryKind::User:
                        if (!lines.empty()) lines.push_back({"", ftxui::nothing, "\n"});
                        {
                            // coloured by its origin (mcp / task), with its label; the user: the accent, as before
                            ftxui::Color ink = this->_theme.accent;
                            std::string head = "› ";
                            if (cluster::origin_colors().enabled) {
                                const std::string origin = cluster::prompt_origin(e.origin, e.text);
                                const std::string color = cluster::origin_color(origin, this->_theme.dark);
                                if (!color.empty()) ink = hex_(color);
                                if (const std::string label = cluster::origin_label(origin); !label.empty()) head += label + " · ";
                            }
                            add(text.starts_with("[voice] ") ? text.substr(8) : text, head, "  ", ftxui::color(ink) | ftxui::bold | mark);
                        }
                        break;
                    case cluster::EntryKind::Assistant:
                        markdown(text, "⏺ ", mark, s.spec.id + ":md" + std::to_string(i));
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
            const std::size_t entriesEnd = lines.size();
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
                    for (const cluster::Json& round: turn["rounds"]) {
                        lines.push_back({"  │ " + cluster::one_line(round.value("text", std::string()), room - 6), round.value("current", false)
                            ? ftxui::color(this->_theme.accent) : ftxui::color(this->_theme.muted) | ftxui::dim, "\n"});
                        // what it did, in a sentence; a picture read or made: a click opens it
                        if (const std::string did = round.value("did", std::string()); !did.empty())
                            lines.push_back({"  │    ↳ " + cluster::one_line(did, room - 9), ftxui::color(this->_theme.muted) | ftxui::dim, "\n"});
                        for (const cluster::Json& picture: round.value("pictures", cluster::Json::array())) {
                            const std::string path = picture.get<std::string>();
                            lines.push_back({"  │    📷 " + cluster::one_line(cluster::short_path(path), room - 28) + "  (a click: open)", ftxui::color(this->_theme.accent), "\n"});
                            lines.back().hits.emplace_back(0, 100000, [this, path]() {this->openPicture_(path);});
                        }
                    }
                    if (turn.value("hidden_thinking", false))
                        lines.push_back({"  (the text of its thinking is not sent by the model: its tokens only)", ftxui::color(this->_theme.muted) | ftxui::dim, "\n"});
                }
                this->_spinning = true;
            }
            for (std::size_t q = 0; q < s.queue.size(); ++q) {
                // the first one: clicked (or F8), sent now; given to claude already: taken at its next step
                const bool given = static_cast<int>(q) < s.queueGiven;
                const std::string now = given ? (compact ? "" : "  (given to claude: taken at its next step)")
                    : q == 0 && !compact ? "  (" + config.keys.at("send_next") + " / a click: send it now · Esc: edit)" : compact ? "" : "  (Esc: edit)";
                Row row{std::string(given ? "↳ given: " : "⧗ queued: ") + cluster::one_line(s.queue[q], static_cast<std::size_t>(std::max(10, width - 60))) + now,
                    ftxui::color(given ? this->_theme.muted : this->_theme.warn), "\n"};
                if (q == 0) row.hits.emplace_back(0, std::max(10, width), [this]() {(void)this->shortcut_("send_next");});
                lines.push_back(row);
            }

            int& scroll = this->_scroll[s.spec.id];
            Anchor& anchor = this->_anchors[s.spec.id];
            const int total = static_cast<int>(lines.size());
            if (hitLine >= 0) {
                scroll = std::max(0, total - hitLine - height / 2);
                anchor.valid = false;
            } else if (anchor.valid && scroll > 0 && scroll == anchor.scroll) {
                // scrolled up and not moved by the user: the first line shown stays on its entry (text added below, an entry above that grew or folded)
                int top = -1;
                if (anchor.entry == std::string::npos) {
                    top = static_cast<int>(entriesEnd) + anchor.offset;
                } else {
                    const auto found = std::find_if(starts.begin(), starts.end(), [&](const std::pair<std::size_t, std::size_t>& p) {return p.second == anchor.entry;});
                    if (found != starts.end()) {
                        const std::size_t next = std::next(found) != starts.end() ? std::next(found)->first : entriesEnd;
                        top = static_cast<int>(std::min(found->first + static_cast<std::size_t>(anchor.offset), std::max(found->first, next) - (next > found->first ? 1 : 0)));
                    }
                }
                if (top >= 0) {
                    const int before = scroll;
                    scroll = std::max(0, total - top - height);
                    anchor.fresh += std::max(0, scroll - before); // the lines added below the view
                }
            }
            scroll = std::clamp(scroll, 0, std::max(0, total - height));
            if (scroll == 0) {
                anchor = Anchor{}; // at the end: followed again
            } else {
                const std::size_t top = static_cast<std::size_t>(std::max(0, total - scroll - height));
                const auto owned = std::upper_bound(starts.begin(), starts.end(), std::make_pair(top, std::numeric_limits<std::size_t>::max()));
                anchor.valid = true;
                anchor.scroll = scroll;
                if (top >= entriesEnd || owned == starts.begin()) {
                    anchor.entry = std::string::npos;
                    anchor.offset = static_cast<int>(top) - static_cast<int>(std::min(top, entriesEnd));
                } else {
                    anchor.entry = std::prev(owned)->second;
                    anchor.offset = static_cast<int>(top - std::prev(owned)->first);
                }
            }
            const int end = static_cast<int>(lines.size()) - scroll;
            this->_transcriptLines = lines.size();
            const int begin = std::max(0, end - height);
            View& view = this->_views[s.spec.id];
            view.lines.clear();
            view.joins.clear();
            view.code.clear();
            view.hits.clear();
            // the mouse selection of this transcript: from (xa, ya) to (xb, yb), cells
            const bool selecting = this->_selection && this->_selection->session == s.spec.id;
            const Selection sel = selecting ? *this->_selection : Selection{};
            const bool forward = sel.y0 < sel.y1 || (sel.y0 == sel.y1 && sel.x0 <= sel.x1);
            view.entries.clear();
            const int ya = forward ? sel.y0 : sel.y1;
            const int xa = forward ? sel.x0 : sel.x1;
            const int yb = forward ? sel.y1 : sel.y0;
            const int xb = forward ? sel.x1 : sel.x0;
            ftxui::Elements shown;
            const bool flash = now_ms_() - this->_pressAt < 300;
            if (flash) this->_animating = true; // drawn again when the flash ends
            for (int k = begin; k < end; ++k) {
                const Row& row = lines[static_cast<std::size_t>(k)];
                const int y = k - begin;
                view.lines.push_back(row.text);
                view.joins.push_back(row.join);
                view.code.push_back(row.code);
                view.hits.push_back(row.hits);
                // a clickable part under the mouse: lit like a button; just clicked: flashed
                const bool pressed = flash && this->_pressView == s.spec.id && this->_pressY == y;
                const bool hovered = this->_hitView == s.spec.id && this->_hitY == y;
                const Hit* lit = nullptr;
                const auto owned = std::upper_bound(starts.begin(), starts.end(), std::make_pair(static_cast<std::size_t>(k), std::numeric_limits<std::size_t>::max()));
                view.entries.push_back(static_cast<std::size_t>(k) >= entriesEnd || owned == starts.begin() ? std::string::npos : std::prev(owned)->second);
                for (const Hit& hit: row.hits)
                    if (std::get<0>(hit) == (pressed ? this->_pressFrom : this->_hitFrom) && (pressed || hovered)) lit = &hit;
                if (row.code.button) {
                    // the copy button: a soft pill (the accent on the surface), filled with the accent under the mouse, inverted when pressed, green once copied
                    const bool copied = this->_copiedBlock == std::to_string(row.code.block) && now_ms_() - this->_copiedAt < 1500;
                    ftxui::Decorator plate = copied ? ftxui::color(this->_theme.okInk) | ftxui::bgcolor(this->_theme.ok) | ftxui::bold
                        : ftxui::color(this->_theme.accent) | ftxui::bgcolor(this->_theme.surface);
                    if (lit && pressed) plate = ftxui::color(this->_theme.accent) | ftxui::bgcolor(this->_theme.bg) | ftxui::bold;
                    else if (lit && !copied) plate = ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) | ftxui::bold;
                    const std::size_t x0 = byte_at_(row.text, row.code.from);
                    const std::size_t x1 = byte_at_(row.text, row.code.to);
                    shown.push_back(ftxui::hbox({ftxui::text(row.text.substr(0, x0)) | row.style, ftxui::text(row.text.substr(x0, x1 - x0)) | plate,
                        ftxui::text(row.text.substr(x1)) | row.style}));
                    continue;
                }
                if (row.code.text && selecting && y >= ya && y <= yb && !(ya == yb && xa == xb)) {
                    // a line of code: only its text cells are lit (not the box, not the padding)
                    const int a = std::max(row.code.from, y == ya ? xa : 0);
                    const int b = std::min(row.code.to, y == yb ? xb + 1 : row.code.to);
                    ftxui::Elements parts;
                    int column = 0;
                    for (const cluster::Cell& c: row.cells) {
                        const ftxui::Decorator style = (c.hasFg ? ftxui::color(ftxui::Color::RGB((c.fg >> 16) & 255, (c.fg >> 8) & 255, c.fg & 255)) : ftxui::nothing)
                            | (c.hasBg ? ftxui::bgcolor(ftxui::Color::RGB((c.bg >> 16) & 255, (c.bg >> 8) & 255, c.bg & 255)) : ftxui::nothing);
                        const int size = static_cast<int>(width_(c.text));
                        const std::size_t p0 = byte_at_(c.text, std::clamp(a - column, 0, size));
                        const std::size_t p1 = std::max(p0, byte_at_(c.text, std::clamp(b - column, 0, size)));
                        if (p0 > 0) parts.push_back(ftxui::text(c.text.substr(0, p0)) | style);
                        if (p1 > p0) parts.push_back(ftxui::text(c.text.substr(p0, p1 - p0)) | style | ftxui::inverted);
                        if (p1 < c.text.size()) parts.push_back(ftxui::text(c.text.substr(p1)) | style);
                        column += size;
                    }
                    shown.push_back(ftxui::hbox(std::move(parts)));
                    continue;
                }
                if (lit && !selecting) {
                    // its runs (colours of the cells, or the style of the line) cut at the cells of the part
                    std::vector<std::pair<std::string, ftxui::Decorator>> runs;
                    if (row.cells.empty()) runs.emplace_back(row.text, row.style);
                    for (const cluster::Cell& c: row.cells)
                        runs.emplace_back(c.text, (c.hasFg ? ftxui::color(ftxui::Color::RGB((c.fg >> 16) & 255, (c.fg >> 8) & 255, c.fg & 255)) : ftxui::nothing)
                            | (c.hasBg ? ftxui::bgcolor(ftxui::Color::RGB((c.bg >> 16) & 255, (c.bg >> 8) & 255, c.bg & 255)) : ftxui::nothing));
                    const int rowWidth = static_cast<int>(width_(row.text));
                    const int a = std::get<0>(*lit);
                    const int b = std::min(std::get<1>(*lit), rowWidth - 1);
                    const ftxui::Decorator mark = pressed ? ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent) | ftxui::bold
                        : ftxui::bgcolor(this->_theme.border) | ftxui::bold;
                    ftxui::Elements parts;
                    int column = 0;
                    for (const auto &[part, style]: runs) {
                        const int size = static_cast<int>(width_(part));
                        const int from = std::clamp(a - column, 0, size);
                        const int to = std::clamp(b + 1 - column, 0, size);
                        const std::size_t x0 = byte_at_(part, from);
                        const std::size_t x1 = byte_at_(part, to);
                        if (x0 > 0) parts.push_back(ftxui::text(part.substr(0, x0)) | style);
                        if (x1 > x0) parts.push_back(ftxui::text(part.substr(x0, x1 - x0)) | style | mark);
                        if (x1 < part.size()) parts.push_back(ftxui::text(part.substr(x1)) | style);
                        column += size;
                    }
                    shown.push_back(ftxui::hbox(std::move(parts)));
                    continue;
                }
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
            if (scroll > 0) {
                // the bottom edge: what is below, and new output that came while scrolled up; a click (or Shift+PageDown) jumps to the end
                const std::string endKey = config.keys.contains("transcript_bottom") ? config.keys.at("transcript_bottom") : std::string("Shift+PageDown");
                const std::string cue = anchor.fresh > 0 ? "↓ " + std::to_string(anchor.fresh) + " new line(s) · " + std::to_string(scroll) + " below (click / " + endKey + ": end)"
                    : "↓ " + std::to_string(scroll) + " more line(s) (click / " + endKey + ": end)";
                const bool lit = anchor.fresh > 0;
                shown.push_back(ftxui::text(cue) | ftxui::color(this->_theme.accent) | (lit ? ftxui::bold : ftxui::dim));
                view.lines.push_back(cue);
                view.joins.push_back("\n");
                view.code.emplace_back();
                view.hits.push_back({Hit(0, 100000, [this, id = s.spec.id]() {this->_scroll[id] = 0;})});
                view.entries.push_back(std::string::npos);
            }
            return ftxui::vbox(std::move(shown)) | ftxui::yflex | ftxui::reflect(view.box);
        }

        _nodiscard ftxui::Element panels_(const cluster::Snapshot& s, const int height)
        {
            static cluster::Git git; // lives as long as the program (its refresh threads)
            const cluster::Metrics& m = s.metrics;
            ftxui::Elements blocks;  // one line each: scrolled when there are more than the room
            const std::function<void(const std::string&, ftxui::Elements)> section = [&](const std::string& name, ftxui::Elements body) {
                blocks.push_back(ftxui::text(name) | ftxui::bold | ftxui::color(this->_theme.accent));
                for (ftxui::Element& element: body)
                    blocks.push_back(std::move(element));
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
                ftxui::Elements tokens = {line("last turn", split(m.last))};
                const std::function<void(const cluster::Usage&)> detail = [&](const cluster::Usage& u) {
                    const std::pair<std::string, std::string> two = cluster::usage_detail2(u);
                    tokens.push_back(ftxui::text(" " + two.first) | ftxui::color(this->_theme.muted) | ftxui::dim);
                    tokens.push_back(ftxui::text(" " + two.second) | ftxui::color(this->_theme.muted) | ftxui::dim);
                };
                if (m.last.total() > 0) detail(m.last);
                tokens.push_back(line("current", split(m.current)));
                tokens.push_back(line("total", split(m.total)));
                if (m.total.total() > 0) detail(m.total);
                tokens.push_back(line("turns", std::to_string(m.turns)));
                section("Tokens", std::move(tokens));
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
            // the lines of the room, from the scroll: "▲ n" above, "▼ n more" below (the wheel over them, Ctrl+↑ / ↓)
            const int room = std::max(3, height);
            const int total = static_cast<int>(blocks.size());
            this->_panelScroll = std::clamp(this->_panelScroll, 0, std::max(0, total - room));
            ftxui::Elements shown;
            if (this->_panelScroll > 0) shown.push_back(ftxui::text("▲ " + std::to_string(this->_panelScroll) + " above") | ftxui::color(this->_theme.muted) | ftxui::dim);
            const int from = this->_panelScroll + (this->_panelScroll > 0 ? 1 : 0);
            const int fit = room - (this->_panelScroll > 0 ? 1 : 0);
            const bool more = total - from > fit;
            for (int i = from; i < total && static_cast<int>(shown.size()) < (more ? room - 1 : room); ++i)
                shown.push_back(std::move(blocks[static_cast<std::size_t>(i)]));
            this->_panelHidden = more ? total - from - (room - 1 - (this->_panelScroll > 0 ? 1 : 0)) : 0;
            if (more) shown.push_back(ftxui::text("▼ " + std::to_string(this->_panelHidden) + " more (wheel, Ctrl+↓)") | ftxui::color(this->_theme.muted) | ftxui::dim);
            return ftxui::vbox(std::move(shown)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 36) | ftxui::reflect(this->_panelBox);
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
            const std::string model = cluster::model_label(s.spec.backend, s.modelUsed);
            ftxui::Elements left = {
                ftxui::text(" " + s.spec.name + " ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent),
                ftxui::text(" " + std::string(cluster::state_name(s.state)) + " ") | ftxui::color(s.state == cluster::State::Error ? this->_theme.error : this->_theme.text),
                this->tap_(ftxui::text(model) | ftxui::color(this->_theme.muted), 0, [this]() {this->pick_("model:");}),
                // how it is billed: api (an API key, per token) | auth (a login, a subscription) | local; its thinking and mode; each one a click away
                this->tap_(ftxui::text(std::string("  ") + (this->_manager.authKind(s.spec.backend) == "api" ? "api" : this->_manager.authKind(s.spec.backend) == "auth" ? "auth" : "local"))
                    | ftxui::bold | ftxui::color(this->_manager.authKind(s.spec.backend) == "api" ? this->_theme.warn : this->_theme.ok), 0, [this]() {this->pick_("backend:");}),
                this->tap_(ftxui::text("  " + cluster::effort_label(s.spec.effort, this->_manager.config().effort, s.spec.backend, s.modelUsed)) | ftxui::color(this->_theme.muted), 0, [this]() {this->pick_("effort:");}),
                this->tap_(ftxui::text("  mode " + s.spec.mode) | ftxui::color(s.spec.mode == "bypassPermissions" || s.spec.mode == "dontAsk" ? this->_theme.warn : this->_theme.muted), 0,
                    [this]() {this->pick_("mode:");}),
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
            ftxui::Element panels = compact ? ftxui::emptyElement() : this->panels_(s, height - 2);
            const int panelWidth = compact || s.spec.panels.empty() ? 0 : 37;
            int bodyHeight = std::max(3, height - (s.permissions.empty() ? 2 : 3) - (s.lastError.empty() ? 0 : 1));
            // the pinned item: at most a third of the room, the transcript keeps the rest
            std::pair<ftxui::Element, int> pinned = compact ? std::pair<ftxui::Element, int>{ftxui::emptyElement(), 0} : this->pin_(s, width, std::min(bodyHeight / 3 + 3, bodyHeight - 4));
            bodyHeight = std::max(3, bodyHeight - pinned.second);
            ftxui::Element transcriptView = this->transcript_(s, width - panelWidth, bodyHeight, compact);
            if (s.spec.global && (s.state == cluster::State::Starting || s.state == cluster::State::Error) && s.warning.starts_with("global session ")) {
                // the global session is not running yet (loading, started again after a crash, waiting for the answer to the previous-run question): a popup in the
                // middle of its transcript (not of the panels beside it), never a small line in a corner
                this->_spinning = true;
                static const std::array<const char*, 10> turn = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
                const bool failing = s.state == cluster::State::Error;
                const std::string what = s.warning.substr(15);
                ftxui::Element popup = ftxui::vbox({ftxui::text(std::string(" ") + turn[static_cast<std::size_t>(now_ms_() / 100 % 10)] + " Global session") | ftxui::bold
                        | ftxui::color(failing ? this->_theme.warn : this->_theme.accent), ftxui::text(""),
                    ftxui::text(" " + what + " ") | ftxui::color(this->_theme.text),
                    ftxui::text(""), ftxui::text(failing ? " it is started again by itself (every few seconds) " : " the other sessions are available meanwhile ") | ftxui::color(this->_theme.muted) | ftxui::dim})
                    | ftxui::borderRounded | ftxui::color(failing ? this->_theme.warn : this->_theme.accent) | ftxui::bgcolor(this->_theme.surface) | ftxui::center;
                transcriptView = ftxui::dbox({transcriptView, popup});
            }
            const ftxui::Element view = ftxui::vbox({
                this->header_(s, compact),
                ftxui::separatorLight() | ftxui::color(this->_theme.border),
                ftxui::hbox({transcriptView | ftxui::flex, panelWidth ? ftxui::separatorLight() | ftxui::color(this->_theme.border) : ftxui::emptyElement(), panels}) | ftxui::flex,
                std::move(pinned.first),
                this->permission_(s),
            });
            return view;
        }

        _nodiscard std::pair<ftxui::Element, int> pin_(const cluster::Snapshot& s, const int width, const int room)
        {
            // the pinned item of the session, above the prompt: its title (a click folds it) and Unpin, then the item as the transcript draws it (live:
            // a checklist ticks, a form fills), scrolled inside when it is taller than the room; the height taken
            const cluster::Json block = cluster::display_pinned(s);
            if (!block.is_object() || room < 5 || width < 20) return {ftxui::emptyElement(), 0};
            const std::string sid = s.spec.id;
            const bool folded = this->_pinFolded.contains(sid);
            const std::string title = s.spec.pin.value("title", std::string("pinned"));
            ftxui::Element head = ftxui::hbox({
                this->tap_(ftxui::text(std::string(folded ? "▸ " : "▾ ") + "📌 " + cut_(title, static_cast<std::size_t>(std::max(10, width - 24)))) | ftxui::bold | ftxui::color(this->_theme.accent), 0,
                    [this, sid]() {if (!this->_pinFolded.erase(sid)) this->_pinFolded.insert(sid);}),
                ftxui::filler(),
                this->tap_(ftxui::text(" Unpin ") | ftxui::color(this->_theme.muted), 0, [this, sid]() {
                    try {
                        this->_manager.unpin(sid);
                        this->status_("unpinned");
                    } catch (const utils::exception::IException& e) {
                        this->status_(e.info(), true);
                    }
                })});
            if (folded) return {ftxui::vbox({head}) | ftxui::borderRounded | ftxui::color(this->_theme.border), 3};
            cluster::Snapshot item;
            item.spec.id = "pin:" + sid;
            item.state = cluster::State::Idle;
            if (block.contains("role")) item.entries.push_back({block.value("role", std::string()) == "user" ? cluster::EntryKind::User : block.value("role", std::string()) == "system"
                ? cluster::EntryKind::System : cluster::EntryKind::Assistant, block.value("text", std::string()), "", 0});
            else item.entries.push_back({cluster::EntryKind::Display, block.dump(-1, ' ', false, cluster::JSON_LENIENT), "", s.spec.pin.value("at", std::int64_t{0})});
            const int inner = std::min(width - 2, 200);
            const int most = room - 3;   // the border and the title
            ftxui::Element body = this->transcript_(item, inner, most, false, sid);
            int rows = std::min(static_cast<int>(this->_transcriptLines), most);
            if (static_cast<int>(this->_transcriptLines) > most) {
                body = this->transcript_(item, inner, most - 1, false, sid);   // its "more lines" mark below
                rows = most;
            }
            rows = std::max(1, rows);
            return {ftxui::vbox({head, std::move(body) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, rows)}) | ftxui::borderRounded | ftxui::color(this->_theme.border), rows + 3};
        }
        std::pair<std::string, std::string> _voiceWords;   // the words said to send / drop (shown in the voice bar)
        std::int64_t _voiceWordsAt = 0;

        _nodiscard const std::pair<std::string, std::string>& voiceWords_(void)
        {
            // the words file and the config: read again every 5 s (edited: shown soon)
            if (cluster::now_ms() - this->_voiceWordsAt < 5000) return this->_voiceWords;
            this->_voiceWordsAt = cluster::now_ms();
            const cluster::VoiceConf& conf = this->_manager.config().voice;
            cluster::VoiceWords words = cluster::voice_words(cluster::voice_words_path(conf.words));
            words.send.insert(words.send.end(), conf.sendWords.begin(), conf.sendWords.end());
            words.cancel.insert(words.cancel.end(), conf.cancelWords.begin(), conf.cancelWords.end());
            this->_voiceWords = {words.send.empty() ? "" : words.send.front(), words.cancel.empty() ? "" : words.cancel.front()};
            return this->_voiceWords;
        }

        _nodiscard int voiceLines_(void) const
        {
            // the bar (listening, the live text, the reading), then the buttons of the sentences waiting
            const cluster::Voice& voice = *this->_app.voice;
            const std::vector<cluster::Pending> pending = voice.pending();
            if (!voice.listening() && pending.empty() && !voice.speaking()) return 0;
            return std::any_of(pending.begin(), pending.end(), [](const cluster::Pending& p) {return !p.target.empty();}) ? 2 : 1;
        }

        _nodiscard ftxui::Element voice_(void)
        {
            const cluster::Voice& voice = *this->_app.voice;
            if (this->voiceLines_() == 0) return ftxui::emptyElement();
            const cluster::VoiceConf& conf = this->_manager.config().voice;
            std::string label = voice.listening() ? " ◉ listening " : " ○ voice ";
            const std::vector<std::string> wake = cluster::split(conf.wakeWord, ',');
            if (voice.listening() && conf.mode == "wake")
                label = voice.armed() ? " ◉ armed: say the command " : " ○ waiting for \"" + (wake.empty() ? std::string("?") : wake.front()) + "\" ";
            if (voice.listening() && !voice.ready()) label = " … loading the speech model (~10 s) ";
            ftxui::Elements row = {ftxui::text(label) | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(voice.armed() || (voice.listening() && conf.mode != "wake") ? this->_theme.ok : this->_theme.muted)};
            std::string last;       // the last sentence waiting (to correct)
            int waiting = 0;
            for (const cluster::Pending& p: voice.pending()) {
                const std::int64_t left = p.deadline == 0 ? -1 : std::max<std::int64_t>(0, (p.deadline - std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() + 999) / 1000);
                if (p.target.empty()) { // live: what is being said, its end
                    row.push_back(ftxui::text(" 🎙 " + cluster::tail_utf8(p.text, 110)) | ftxui::italic | ftxui::color(this->_theme.muted));
                    continue;
                }
                ++waiting;
                last = p.text;
                const std::string target = p.target == GLOBAL_ID ? "global" : p.target;
                const std::string when = p.paused ? " (paused while you speak)" : left >= 0 ? " (sent in " + std::to_string(left) + "s)" : " (F5 to end)";
                row.push_back(ftxui::text(" → " + target + ": ") | ftxui::color(this->_theme.accent));
                row.push_back(ftxui::text("\"" + cluster::one_line(p.text, 90) + "\"") | ftxui::bold | ftxui::color(this->_theme.text));
                row.push_back(ftxui::text(when) | ftxui::color(p.paused ? this->_theme.warn : this->_theme.muted));
            }
            if (voice.speaking()) row.push_back(ftxui::text("  speaking (" + this->_manager.config().keys.at("mute") + " mute)") | ftxui::color(this->_theme.muted));
            ftxui::Element bar = ftxui::hbox(std::move(row)) | ftxui::bgcolor(this->_theme.surface);
            if (waiting == 0) return bar;
            // its validation: clicked, or by the keys (Escape: the last one, Ctrl+C: all of them)
            ftxui::Elements buttons = {ftxui::text(" ")};
            buttons.push_back(this->tap_(ftxui::text(" ✓ Send now ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.ok), 0, [this]() {
                this->run_({"voice_confirm", "", "", "", false}, "");
                this->status_("voice prompt sent");
            }));
            buttons.push_back(ftxui::text("  "));
            buttons.push_back(this->tap_(ftxui::text(" ✗ Drop ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.error), 0, [this]() {
                this->_app.voice->cancelLast();
                this->status_("voice prompt dropped");
            }));
            buttons.push_back(ftxui::text("  "));
            buttons.push_back(this->tap_(ftxui::text(" ✎ Correct ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent), 0, [this, last]() {
                this->_pending = {"voice_edit", "Voice: correct the transcription", "text (Enter: sent after the countdown)", last, false};
                this->_prompt = last;
                this->_password = false;
                this->_mode = Mode::Prompt;
            }));
            const std::pair<std::string, std::string>& said = this->voiceWords_();
            buttons.push_back(ftxui::text("   Enter (empty prompt) send · Escape drop" + std::string(waiting > 1 ? " the last (Ctrl+C: all)" : "")
                + (said.first.empty() || said.second.empty() ? std::string() : " · or say \"" + said.first + "\" / \"" + said.second + "\"")) | ftxui::color(this->_theme.muted));
            return ftxui::vbox({bar, ftxui::hbox(std::move(buttons)) | ftxui::bgcolor(this->_theme.surface)});
        }

        _nodiscard ftxui::Element body_(const int width, const int height)
        {
            this->_listBox = ftxui::Box{0, -1, 0, -1};   // drawn again below when the layout shows them
            this->_gridBox = ftxui::Box{0, -1, 0, -1};
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

            if (layout == "grid" && !list.empty()) {
                // as many columns as the cells keep their minimum size, the rows that do not fit scroll
                const int count = static_cast<int>(list.size()) + 1; // the sessions, then the Tasks as a cell like the others
                const int maxColumns = std::max(1, width / GRID_MIN_WIDTH);
                const int maxRows = std::max(1, height / GRID_MIN_HEIGHT);
                int columns = std::min(maxColumns, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
                while ((count + columns - 1) / columns > maxRows && columns < maxColumns)
                    ++columns;
                const int total = (count + columns - 1) / columns;
                const bool scrolls = total > maxRows;
                const int visible = scrolls ? std::max(1, (height - 1) / GRID_MIN_HEIGHT) : total;   // a line kept for the rows indicator
                const int cellWidth = width / columns - 2;
                const int cellHeight = (height - (scrolls ? 1 : 0)) / visible - 2;
                const int outerWidth = width / columns;     // every cell exactly its share, whatever its content
                const int outerHeight = cellHeight + 2;
                this->_gridColumns = columns;
                this->_gridVisible = visible;
                this->_gridRows = total;
                const auto at = std::find_if(list.begin(), list.end(), [&](const cluster::Snapshot& s) {return s.spec.id == this->_active;});
                const int activeRow = tasksShown ? (count - 1) / columns : at == list.end() ? 0 : static_cast<int>(at - list.begin()) / columns;
                if (this->_gridSeen != this->_active) { // the focus moved: its row in view
                    if (activeRow < this->_gridFirst) this->_gridFirst = activeRow;
                    else if (activeRow >= this->_gridFirst + visible) this->_gridFirst = activeRow + 1 - visible;
                    this->_gridSeen = this->_active;
                }
                this->_gridFirst = std::clamp(this->_gridFirst, 0, total - visible);
                std::vector<ftxui::Elements> grid;
                for (std::size_t i = static_cast<std::size_t>(this->_gridFirst * columns); i < static_cast<std::size_t>(count) && i < static_cast<std::size_t>((this->_gridFirst + visible) * columns); ++i) {
                    if (i % static_cast<std::size_t>(columns) == 0) grid.emplace_back();
                    if (i == list.size()) { // the Tasks session: a cell like the sessions
                        grid.back().push_back(this->click_(this->tasksView_(cellWidth, cellHeight) | ftxui::borderStyled(tasksShown ? ftxui::HEAVY : ftxui::ROUNDED)
                            | ftxui::color(tasksShown ? this->_theme.accent : this->_theme.border) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, outerWidth)
                            | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, outerHeight), 0, [this]() {
                                if (this->_active == TASKS_ID) return false;
                                this->_active = TASKS_ID;
                                return true;
                            }) | ftxui::flex);
                        continue;
                    }
                    const bool active = list[i].spec.id == this->_active;
                    const std::string id = list[i].spec.id;
                    // clicked: shown (the one shown: its text selected as usual)
                    grid.back().push_back(this->click_(this->session_(list[i], cellWidth, cellHeight, true) | ftxui::borderStyled(active ? ftxui::HEAVY : ftxui::ROUNDED)
                        | ftxui::color(active ? this->_theme.accent : this->_theme.border) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, outerWidth)
                        | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, outerHeight), 0, [this, id]() {
                            if (this->_active == id) return false;
                            this->_active = id;
                            return true;
                        }) | ftxui::flex);
                }
                while (grid.back().size() < static_cast<std::size_t>(columns)) grid.back().push_back(ftxui::filler());
                ftxui::Element cells = ftxui::gridbox(grid) | ftxui::flex;
                if (!scrolls) return cells;
                const int first = this->_gridFirst;
                ftxui::Elements bar = {
                    this->tap_(ftxui::text(" ▲ ") | ftxui::bold | ftxui::color(first > 0 ? this->_theme.accent : this->_theme.muted), 0, [this]() {--this->_gridFirst;}),
                    this->tap_(ftxui::text(" ▼ ") | ftxui::bold | ftxui::color(first + visible < total ? this->_theme.accent : this->_theme.muted), 0, [this]() {++this->_gridFirst;}),
                    ftxui::text(" rows " + std::to_string(first + 1) + "-" + std::to_string(first + visible) + " of " + std::to_string(total) + " · " + std::to_string(count)
                        + " sessions" + (first > 0 ? " · ↑ more" : "") + (first + visible < total ? " · ↓ more" : "")
                        + " · wheel / Alt+PageUp / Alt+PageDown scroll (Ctrl+wheel: the transcript)") | ftxui::color(this->_theme.muted) | ftxui::dim,
                };
                return ftxui::vbox({cells, ftxui::hbox(std::move(bar)) | ftxui::bgcolor(this->_theme.surface)}) | ftxui::flex | ftxui::reflect(this->_gridBox);
            }
            // list + active panel
            ftxui::Elements rows = {ftxui::text(" Sessions") | ftxui::bold | ftxui::color(this->_theme.accent), ftxui::text("")};
            const int clip = this->clip_();
            rows.push_back(this->tasksRow_(tasksShown, clip)); // the Tasks session first
            // more sessions than the room: the ones around the one shown (2 lines each)
            const bool grouped = this->grouped_();
            std::set<std::string> categories;
            std::size_t labelled = 0;   // the sessions with a third line (their chips)
            for (const cluster::Snapshot& s: list) {
                categories.insert(s.spec.global ? std::string("\x01") : s.spec.category);
                labelled += s.spec.category.empty() && s.spec.tags.empty() ? 0 : 1;
            }
            const std::size_t spare = (grouped ? categories.size() : 0) + labelled / 2;   // the lines taken by the headers and the chips, on average
            // the room really there: the height the list had in the last frame (the chrome around it taken off), else the estimate
            const int measured = this->_listBox.y_max - this->_listBox.y_min + 1;
            const int usable = measured > 6 && measured <= height ? measured - 4 : height - 6; // the title, the Tasks row and the two "more" lines
            const std::size_t room = static_cast<std::size_t>(std::max<int>(1, (usable - static_cast<int>(spare)) / 2));
            const std::size_t at = static_cast<std::size_t>(it - list.begin());
            this->_listRoom = room;
            if (this->_listSeen != this->_active) { // the selection moved: its row in view (the wheel scrolls freely until then)
                if (at < this->_listFirst) this->_listFirst = at;
                else if (at >= this->_listFirst + room) this->_listFirst = at + 1 - room;
                this->_listSeen = this->_active;
            }
            this->_listFirst = list.size() <= room ? 0 : std::min(this->_listFirst, list.size() - room);
            const std::size_t first = this->_listFirst;
            if (first > 0) rows.push_back(ftxui::text(" ↑ " + std::to_string(first) + " more") | ftxui::color(this->_theme.muted) | ftxui::dim);
            const cluster::Taxonomy taxonomy = this->_manager.taxonomy();
            for (std::size_t i = first; i < list.size() && i < first + room; ++i) {
                if (grouped && !list[i].spec.global) {
                    // the heads of the taxonomy: the levels of its path the previous session did not have, one line each, nested (none: "no category")
                    const std::vector<std::string> path = taxonomy.group(list[i].spec);
                    const std::vector<std::string> before = i == first || list[i - 1].spec.global ? std::vector<std::string>{"\x01"} : taxonomy.group(list[i - 1].spec);
                    if (path != before)
                        for (std::size_t depth = path.empty() ? 0: cluster::Taxonomy::common_prefix(before, path); depth < std::max<std::size_t>(path.size(), 1); ++depth) {
                            const std::string node = depth < path.size() ? path[depth] : std::string();
                            rows.push_back(ftxui::text(std::string(1 + 2 * depth, ' ') + "▾ " + (node.empty() ? std::string("no category") : cluster::one_line(node, 20))) | ftxui::bold
                                | ftxui::color(node.empty() ? this->_theme.muted : this->chip_color_(node)));
                        }
                }
                rows.push_back(this->sessionRow_(list[i], list[i].spec.id == this->_active, clip));
            }
            if (first + room < list.size()) rows.push_back(ftxui::text(" ↓ " + std::to_string(list.size() - first - room) + " more") | ftxui::color(this->_theme.muted) | ftxui::dim);
            return ftxui::hbox({
                ftxui::vbox(std::move(rows)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 30) | ftxui::yframe | this->clipBox_(clip) | ftxui::reflect(this->_listBox),
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
            if (this->upgradeShown_()) {
                const cluster::UpgradeOffer offer = this->_manager.upgradeOffer();
                // a choice: under the cursor, filled in its colour with arrows around; the others a quiet button with their key (a click = the key)
                const std::function<ftxui::Element(int, const std::string&, const std::string&, const ftxui::Color&, const ftxui::Event&)> choice = [&](const int index, const std::string& label, const std::string& letter, const ftxui::Color& color, const ftxui::Event& key) {
                    const bool focused = this->_upgradeFocus == index;
                    return press(focused ? ftxui::text("❯" + label + "[" + letter + "] ❮") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(color)
                        : ftxui::text(" " + label + "[" + letter + "]  ") | ftxui::color(color) | ftxui::bgcolor(this->_theme.surface), key);
                };
                return box(ftxui::vbox({ftxui::text(" A newer claude-cluster is installed") | ftxui::bold | ftxui::color(this->_theme.text), ftxui::text(""),
                    ftxui::text(std::string(" ") + CLUSTER_VERSION + "  →  " + offer.version) | ftxui::bold | ftxui::color(this->_theme.accent), ftxui::text(""),
                    ftxui::paragraph(" Update: the turns running end first, the sessions are saved, the core and every terminal / window switch to the new "
                        "version by themselves, the sessions resumed - nothing to start again by hand.") | ftxui::color(this->_theme.text),
                    ftxui::text(""),
                    ftxui::hbox({choice(0, " Update ", "u", this->_theme.accent, ftxui::Event::Character('u')), ftxui::text("  "),
                        choice(1, " Ignore this version ", "i", this->_theme.text, ftxui::Event::Character('i')), ftxui::text("  "),
                        choice(2, " Quit claude-cluster ", "q", this->_theme.error, ftxui::Event::Character('q'))}),
                    ftxui::text(" ←/→ / Tab: choose · Enter: do it · Esc: later (the palette or a new start ask again)") | ftxui::color(this->_theme.muted) | ftxui::dim}), 92);
            }
            if (const cluster::Link link = this->_manager.link(); link.lost && link.upgrading) {
                // the core being updated: this terminal follows it (the new binary run in place of this one)
                this->_spinning = true;
                static const std::array<const char*, 10> turn = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
                return box(ftxui::vbox({ftxui::text(std::string(" ") + turn[static_cast<std::size_t>(cluster::now_ms() / 100 % 10)] + " Updating claude-cluster…")
                    | ftxui::bold | ftxui::color(this->_theme.accent), ftxui::text(""),
                    ftxui::text(" the core runs the new version; this terminal switches to it as soon as it is up") | ftxui::color(this->_theme.text)}), 84);
            }
            if (const cluster::Link link = this->_manager.link(); link.lost) {
                // the core lost: over everything, until it is back (Retry now / Quit)
                this->_spinning = true;
                static const std::array<const char*, 10> spin = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
                const std::int64_t now = cluster::now_ms();
                const std::int64_t next = std::max<std::int64_t>(0, link.next - now);
                return box(ftxui::vbox({ftxui::hbox({ftxui::text(std::string(" ") + spin[static_cast<std::size_t>(now / 100 % 10)] + " Reconnecting to claude-cluster…")
                        | ftxui::bold | ftxui::color(this->_theme.warn)}), ftxui::text(""),
                    ftxui::text(" its core is not reached since " + cluster::took((now - link.since))) | ftxui::color(this->_theme.text),
                    ftxui::text(" try " + std::to_string(link.tries) + " · the next one in " + std::to_string((next + 999) / 1000) + " s (the core started again "
                        "when it is gone: the sessions resumed)") | ftxui::color(this->_theme.muted),
                    ftxui::text(""),
                    ftxui::hbox({press(ftxui::text(" [r] / Enter  Retry now ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.accent),
                            ftxui::Event::Character('r')), ftxui::text("   "),
                        press(ftxui::text(" [q]  Quit ") | ftxui::color(this->_theme.error), ftxui::Event::Character('q'))})}), 84);
            }
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
                return box(ftxui::vbox({ftxui::hbox({ftxui::text(" > ") | ftxui::color(this->_theme.accent), this->fieldView_(this->_filter, 72)}),
                    ftxui::separatorLight(), ftxui::vbox(std::move(items))}), 80);
            }
            if (this->_mode == Mode::Rewind || this->_mode == Mode::Stash) {
                const cluster::PopupColors colors{this->_theme.text, this->_theme.muted, this->_theme.accent, this->_theme.bg, this->_theme.warn, this->_theme.error};
                const bool rewind = this->_mode == Mode::Rewind;
                const cluster::PopupPress popupPress = [this, rewind](ftxui::Element element, const ftxui::Event& pressed, const int row) {
                    return this->tap_(std::move(element), 1, [this, rewind, pressed, row]() {
                        if (row >= 0 && rewind) this->_rewind.select(row);
                        else if (row >= 0) this->_stash.select(row);
                        (void)this->event_(pressed);
                    });
                };
                return box(rewind ? this->_rewind.render(colors, popupPress) : this->_stash.render(colors, popupPress), 110);
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
                return box(ftxui::vbox({ftxui::hbox({ftxui::text(scope) | ftxui::color(this->_theme.accent), this->fieldView_(this->_filter, std::max(10, 112 - static_cast<int>(width_(scope))))}),
                    ftxui::separatorLight(), ftxui::vbox(std::move(items)), ftxui::text(std::string(" Enter: put in the prompt · Tab: ")
                    + (this->_historyAll ? "this session only" : "every session") + " · Esc: close") | ftxui::dim}), 120);
            }
            if (this->_mode == Mode::Prompt) {
                // a password: stars, no cursor moves, no clipboard (typed and erased at the end only)
                const ftxui::Element input = this->_password ? ftxui::hbox({ftxui::text(std::string(static_cast<std::size_t>(width_(this->_prompt)), '*')), ftxui::text("▏") | ftxui::blink})
                    : this->fieldView_(this->_prompt, 72);
                return box(ftxui::vbox({ftxui::text(" " + this->_pending.title) | ftxui::bold, ftxui::text(" " + this->_pending.input + ":") | ftxui::color(this->_theme.muted),
                    ftxui::hbox({ftxui::text(" > ") | ftxui::color(this->_theme.accent), input}),
                    ftxui::hbox({press(ftxui::text(" Enter: run"), ftxui::Event::Return), ftxui::text(" · "), press(ftxui::text("Escape: cancel"), ftxui::Event::Escape)}) | ftxui::dim}), 80);
            }
            if (this->_mode == Mode::Plugins) {
                if (now_ms_() - this->_plugRead > 1000) {
                    this->_plug = cluster::plugins_index();
                    this->_plugRead = now_ms_();
                }
                this->_spinning = true; // the core works (clone, install): its result shown as it comes
                const ftxui::Dimensions size = ftxui::Terminal::Size();
                const int width = std::max(80, size.dimx - 6);
                const int height = std::max(16, size.dimy - 6);
                const std::vector<std::pair<std::size_t, int>> rows = this->plugRows_();
                if (!rows.empty()) this->_plugSel = std::min(this->_plugSel, rows.size() - 1);
                // the list: the sources and their items, their state
                ftxui::Elements list;
                const int listHeight = height - 4;
                const int first = std::max(0, static_cast<int>(this->_plugSel) - listHeight / 2);
                for (int k = first; k < static_cast<int>(rows.size()) && k < first + listHeight; ++k) {
                    const auto &[r, i] = rows[static_cast<std::size_t>(k)];
                    const cluster::PluginRepo& repo = this->_plug[r];
                    const bool on = static_cast<std::size_t>(k) == this->_plugSel;
                    ftxui::Element element;
                    if (i < 0) {
                        const std::string icon = repo.kind == "zip" ? "▤ " : repo.kind == "folder" ? "▣ " : "⎇ ";
                        element = ftxui::hbox({ftxui::text(icon + repo.slug) | ftxui::bold | ftxui::color(repo.error.empty() ? this->_theme.accent : this->_theme.warn),
                            ftxui::text(repo.marketplace.empty() ? "" : "  marketplace") | ftxui::color(this->_theme.muted)});
                    } else {
                        const cluster::PluginItem& item = repo.items[static_cast<std::size_t>(i)];
                        const std::string mark = !item.installed ? "○" : item.enabled ? "●" : "◐";
                        element = ftxui::hbox({ftxui::text("  " + mark + " ") | ftxui::color(!item.installed ? this->_theme.muted : item.enabled ? this->_theme.ok : this->_theme.warn),
                            ftxui::text(item.name) | ftxui::color(this->_theme.text), ftxui::text(" " + item.kind) | ftxui::color(this->_theme.muted)});
                    }
                    list.push_back(this->tap_(on && !this->_plugTree ? element | ftxui::bgcolor(this->_theme.border) : element, 1, [this, k]() {
                        if (this->_plugSel == static_cast<std::size_t>(k)) (void)this->event_(ftxui::Event::Return); // clicked again: its files
                        this->_plugSel = static_cast<std::size_t>(k);
                        this->_plugTree = false;
                        this->_plugFile.clear();
                    }));
                }
                if (rows.empty()) list.push_back(ftxui::paragraph(" no source yet: a to add one (a repository link, a folder, a .zip; drop a file here)")
                    | ftxui::color(this->_theme.muted));
                // the item: its information, the tree of its files, the file previewed
                ftxui::Elements info;
                ftxui::Element right = ftxui::text("");
                if (!rows.empty()) {
                    const auto &[r, i] = rows[this->_plugSel];
                    const cluster::PluginRepo& repo = this->_plug[r];
                    const cluster::PluginItem* item = i >= 0 ? &repo.items[static_cast<std::size_t>(i)] : nullptr;
                    const std::filesystem::path root = item ? std::filesystem::path(item->path.empty() ? repo.dir : item->path) : std::filesystem::path(repo.dir);
                    if (item) {
                        info.push_back(ftxui::hbox({ftxui::text(" " + item->name) | ftxui::bold | ftxui::color(this->_theme.text), ftxui::text("  " + item->kind + " · "
                            + (!item->installed ? "not installed" : item->enabled ? "installed, on" : "installed, off")) | ftxui::color(this->_theme.muted)}));
                        info.push_back(ftxui::hbox({ftxui::text(" " + (item->kind == "skill" ? "/" + repo.slug + ":" + item->name : "/" + item->name + ":<skill>") + " ") | ftxui::color(this->_theme.accent),
                            ftxui::text(" " + (item->kind == "skill" ? repo.slug : item->name) + " ") | ftxui::bold | ftxui::color(ftxui::Color::RGB(255, 255, 255))
                            | ftxui::bgcolor(hex_(cluster::source_color(item->kind == "skill" ? repo.slug : item->name)))}));
                        info.push_back(ftxui::paragraph(" " + (item->description.empty() ? std::string("no description") : item->description)) | ftxui::color(this->_theme.text));
                    } else {
                        info.push_back(ftxui::text(" " + repo.url) | ftxui::bold | ftxui::color(this->_theme.text));
                        info.push_back(ftxui::text(" " + repo.kind + (repo.marketplace.empty() ? "" : " · marketplace " + repo.marketplace) + " · "
                            + std::to_string(repo.items.size()) + " item(s)") | ftxui::color(this->_theme.muted));
                        if (!repo.error.empty()) info.push_back(ftxui::paragraph(" " + repo.error) | ftxui::color(this->_theme.warn));
                    }
                    info.push_back(ftxui::text(" " + cluster::short_path(root.string())) | ftxui::color(this->_theme.border));
                    ftxui::Elements tree;
                    const std::vector<std::tuple<int, std::filesystem::path, bool>> nodes = this->plugNodes_(root);
                    const int treeHeight = height - 9;
                    if (!nodes.empty()) this->_plugNode = std::min(this->_plugNode, nodes.size() - 1);
                    const int top = std::max(0, static_cast<int>(this->_plugNode) - treeHeight / 2);
                    for (int k = top; k < static_cast<int>(nodes.size()) && k < top + treeHeight; ++k) {
                        const auto &[depth, path, dir] = nodes[static_cast<std::size_t>(k)];
                        const bool open = this->_plugOpen.contains(path.string());
                        const std::string name = std::string(static_cast<std::size_t>(depth) * 2, ' ') + (dir ? (open ? "▾ " : "▸ ") : "  ") + path.filename().string();
                        ftxui::Element node = ftxui::text(" " + name) | ftxui::color(dir ? this->_theme.accent : path.string() == this->_plugFile ? this->_theme.ok : this->_theme.text);
                        const bool here = this->_plugTree && static_cast<std::size_t>(k) == this->_plugNode;
                        tree.push_back(this->tap_(here ? node | ftxui::bgcolor(this->_theme.border) : node, 1, [this, k, path, dir]() {
                            this->_plugTree = true;
                            this->_plugNode = static_cast<std::size_t>(k);
                            if (dir) {
                                if (this->_plugOpen.contains(path.string())) this->_plugOpen.erase(path.string());
                                else this->_plugOpen.insert(path.string());
                            } else {
                                this->_plugFile = path.string();
                                this->_plugScroll = 0;
                            }
                        }));
                    }
                    const int previewWidth = width - 44 - 34 - 4;
                    ftxui::Elements preview = this->_plugFile.empty() ? ftxui::Elements{ftxui::text(" Enter: its files, a file: previewed here") | ftxui::color(this->_theme.muted)}
                        : this->plugPreview_(this->_plugFile, treeHeight, previewWidth);
                    if (!this->_plugFile.empty()) preview.insert(preview.begin(), ftxui::text(" " + std::filesystem::path(this->_plugFile).filename().string() + "  (PageUp / PageDown)")
                        | ftxui::bold | ftxui::color(this->_theme.accent));
                    right = ftxui::vbox({ftxui::vbox(std::move(info)), ftxui::separatorLight() | ftxui::color(this->_theme.border),
                        ftxui::hbox({ftxui::vbox(std::move(tree)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 34) | ftxui::frame, ftxui::separatorLight() | ftxui::color(this->_theme.border),
                            ftxui::vbox(std::move(preview)) | ftxui::flex})}) | ftxui::flex;
                }
                ftxui::Elements bottom;
                if (this->_plugAdd) {
                    bottom.push_back(ftxui::hbox({ftxui::text(" source › ") | ftxui::color(this->_theme.accent) | ftxui::bold,
                        this->lineView_(this->_plugInput, std::max(20, width - 60), 1)}));
                    std::string hints;
                    for (const std::string& h: this->_plugHints)
                        hints += h + "  ";
                    bottom.push_back(ftxui::text(hints.empty() ? " a repository (owner/repo, https / git URL), a folder, a .zip · Tab: complete the path · Enter: add · Esc"
                        : " " + hints) | ftxui::color(this->_theme.muted));
                } else {
                    bottom.push_back(ftxui::text(" ↑↓ · Enter / →: its files · i install · r remove · Space toggle · e on / off · a add a source (or drop a file / folder) · "
                        "d drop the source · u update · Esc close") | ftxui::color(this->_theme.muted) | ftxui::dim);
                }
                bottom.push_back(ftxui::text(" ● on  ◐ off  ○ not installed · applied by the core, for the sessions started after it") | ftxui::color(this->_theme.muted) | ftxui::dim);
                return box(ftxui::vbox({ftxui::text(" Plugins and skills") | ftxui::bold | ftxui::color(this->_theme.text), ftxui::separatorLight() | ftxui::color(this->_theme.border),
                    ftxui::hbox({ftxui::vbox(std::move(list)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 44), ftxui::separatorLight() | ftxui::color(this->_theme.border), right})
                        | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, height - 4),
                    ftxui::separatorLight() | ftxui::color(this->_theme.border), ftxui::vbox(std::move(bottom))}), width);
            }
            if (this->_mode == Mode::Form) {
                // a slot per field (wide), the one focused framed; its choices / yes-no clicked; Submit / Cancel
                const int width = std::clamp(ftxui::Terminal::Size().dimx - 8, 50, 110);
                const int slot = width - 6;
                ftxui::Elements rows = {ftxui::text(" " + this->_formTitle) | ftxui::bold | ftxui::color(this->_theme.text), ftxui::text("")};
                for (std::size_t i = 0; i < this->_form.size(); ++i) {
                    const FormField& f = this->_form[i];
                    const bool on = i == this->_formFocus;
                    const ftxui::Color edge = on ? this->_theme.accent : this->_theme.border;
                    rows.push_back(this->tap_(ftxui::text(" " + f.label + (f.type == "text" || f.label == f.name ? "" : "  (" + f.type + ")"))
                        | (on ? ftxui::bold | ftxui::color(this->_theme.accent) : ftxui::color(this->_theme.text)), 1, [this, i]() {this->_formFocus = i;}));
                    ftxui::Element body;
                    if (f.type == "choice") {
                        ftxui::Elements chips = {ftxui::text(on ? " ◀ " : "   ") | ftxui::color(this->_theme.muted)};
                        for (const std::string& option: f.options)
                            chips.push_back(this->tap_(ftxui::text(" " + option + " ") | (option == f.value ? ftxui::color(this->_theme.bg) | ftxui::bgcolor(on ? this->_theme.accent
                                : this->_theme.muted) : ftxui::color(this->_theme.text)), 1, [this, i, option]() {
                                    this->_formFocus = i;
                                    this->_form[i].value = option;
                                }));
                        chips.push_back(ftxui::text(on ? " ▶" : "") | ftxui::color(this->_theme.muted));
                        body = ftxui::hbox(std::move(chips));
                    } else if (f.type == "bool") {
                        const bool yes = f.value == "yes";
                        body = ftxui::hbox({ftxui::text(" "), this->tap_(ftxui::text(yes ? " ☑ yes " : " ☐ no ") | (on ? ftxui::bold | ftxui::color(this->_theme.accent)
                            : ftxui::color(this->_theme.text)), 1, [this, i]() {
                                this->_formFocus = i;
                                this->_form[i].value = this->_form[i].value == "yes" ? "no" : "yes";
                            })});
                    } else {
                        const std::string shown = cluster::one_line(f.value, static_cast<std::size_t>(slot - 3));
                        const std::string hint = f.value.empty() && !on ? (f.type == "date" ? "YYYY-MM-DD" : f.type == "number" ? "a number" : "") : "";
                        ftxui::Element content = on ? this->fieldView_(f.value, slot - 3)
                            : ftxui::hbox({ftxui::text(hint.empty() ? shown : hint) | ftxui::color(hint.empty() ? this->_theme.text : this->_theme.muted), ftxui::filler()});
                        body = this->tap_(content | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, slot) | ftxui::bgcolor(this->_theme.bg), 1, [this, i]() {this->_formFocus = i;});
                    }
                    rows.push_back(ftxui::hbox({ftxui::text(" "), (f.type == "text" || f.type == "number" || f.type == "date" ? body | ftxui::borderRounded : body)
                        // the field focused is edited like the prompt (cursor, selection, words, undo, mouse), a long value scrolls sideways
                        | ftxui::color(edge)}));
                }
                const std::size_t count = this->_form.size();
                rows.push_back(ftxui::text(""));
                rows.push_back(ftxui::hbox({ftxui::text(" "),
                    this->tap_(ftxui::text(" Submit ") | ftxui::bold | (this->_formFocus == count ? ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.ok)
                        : ftxui::color(this->_theme.ok)), 1, [this]() {this->formSubmit_();}), ftxui::text("   "),
                    this->tap_(ftxui::text(" Cancel ") | (this->_formFocus == count + 1 ? ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.error)
                        : ftxui::color(this->_theme.error)), 1, [this]() {this->_mode = Mode::Normal;})}));
                rows.push_back(ftxui::text(" ↑↓ / Tab: field · ←→: choice · Space: yes / no · Enter: next, Submit · Ctrl+S: submit · Esc: cancel") | ftxui::color(this->_theme.muted)
                    | ftxui::dim);
                return box(ftxui::vbox(std::move(rows)), width);
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
                    ftxui::hbox({ftxui::text(" › ") | ftxui::color(this->_theme.accent), this->fieldView_(this->_answer, 82)})};
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
            this->_askChatBox = ftxui::Box{};
            if (this->_mode == Mode::Question && asked) {
                // like the questions of Claude Code: one at a time, its options, other (typed); the keys, the mouse and the voice
                const cluster::Ask& a = *asked;
                const cluster::Json& q = a.questions[a.index];
                const cluster::Json options = q.value("options", cluster::Json::array());
                const bool multi = q.value("multiSelect", false);
                const std::string askKey = a.requestId + ":" + std::to_string(a.index);
                if (askKey != this->_askOtherKey || (a.other != this->_askOther.text() && a.cursor != static_cast<int>(options.size()))) { // another question, or typed elsewhere
                    this->_askOther.set(a.other);
                    this->_askOtherKey = askKey;
                }
                if (a.review) {
                    // like Claude Code: every answer, then Submit / Cancel (back: the last question to edit)
                    const std::size_t width = static_cast<std::size_t>(std::max(30, std::min(92, this->_screen.dimx() - 8)));
                    ftxui::Elements lines = {ftxui::hbox({ftxui::text(" " + a.name + " asks") | ftxui::bold, ftxui::text("  · review your answers") | ftxui::color(this->_theme.accent),
                        ftxui::text(a.session != this->_active ? "  (it waits for your answer)" : "") | ftxui::color(this->_theme.warn)}), ftxui::text("")};
                    for (const cluster::Json& each: a.questions) {
                        const std::string header = each.value("header", std::string());
                        for (const std::string& line: wrap_((header.empty() ? "" : header + " · ") + each.value("question", std::string()), width - 2))
                            lines.push_back(ftxui::text(" " + line + " ") | ftxui::color(this->_theme.muted));
                        const std::string answer = a.answers.value(each.value("question", std::string()), std::string());
                        const std::vector<std::string> said = wrap_(answer.empty() ? std::string("(no answer)") : answer, width - 7);
                        for (std::size_t l = 0; l < said.size(); ++l)
                            lines.push_back(ftxui::text(std::string(l == 0 ? "   → " : "     ") + said[l] + " ") | ftxui::bold
                                | ftxui::color(answer.empty() ? this->_theme.warn : this->_theme.ok));
                        lines.push_back(ftxui::text(""));
                    }
                    lines.push_back(ftxui::text(" Ready to submit your answers?") | ftxui::bold);
                    lines.push_back(ftxui::text(""));
                    lines.push_back(ftxui::hbox({this->tap_(ftxui::text(" 1. Submit (Enter) ") | ftxui::color(this->_theme.ok), 0,
                            [this]() {this->askNext_();}), ftxui::text("   "),
                        this->tap_(ftxui::text(" 2. Cancel (Esc) ") | ftxui::color(this->_theme.error), 0, [this]() {
                            this->_manager.asking().decline();
                            this->status_("question declined");
                            this->_mode = Mode::Normal;
                        }), ftxui::text("   "),
                        this->tap_(ftxui::text(" ← back to edit ") | ftxui::color(this->_theme.muted), 0, [this]() {this->_manager.asking().back();})}));
                    return box(ftxui::vbox(std::move(lines)), 96);
                }
                ftxui::Elements lines = {ftxui::hbox({ftxui::text(" " + a.name + " asks") | ftxui::bold,
                    ftxui::text(a.questions.size() > 1 ? "  (" + std::to_string(a.index + 1) + "/" + std::to_string(a.questions.size()) + ")" : "") | ftxui::color(this->_theme.muted),
                    ftxui::text([&a]() { // the questions: ✔ answered, ◉ this one, ○ not answered
                        std::string marks = a.questions.size() > 1 ? "  " : "";
                        for (std::size_t i = 0; a.questions.size() > 1 && i < a.questions.size(); ++i)
                            marks += i == a.index ? "◉ " : a.answers.contains(a.questions[i].value("question", std::string())) ? "✔ " : "○ ";
                        return marks;
                    }()) | ftxui::color(this->_theme.accent),
                    ftxui::text(a.session != this->_active ? "  (it waits for your answer)" : "") | ftxui::color(this->_theme.warn),
                    ftxui::filler(), ftxui::text(q.value("header", std::string()) + " ") | ftxui::color(this->_theme.accent)}), ftxui::text(""),
                    ftxui::paragraph(" " + q.value("question", std::string())) | ftxui::color(this->_theme.text), ftxui::text("")};
                // the options: the label, then its description, each wrapped on the width of the popup (never cut); Other: what
                // is typed, wrapped too, the cursor at its end
                const bool anyPreview = std::any_of(options.begin(), options.end(), [](const cluster::Json& o) {return o.is_object() && !o.value("preview", std::string()).empty();});
                const bool sideBySide = anyPreview && this->_screen.dimx() >= 120;
                const int boxWidth = anyPreview ? std::clamp(this->_screen.dimx() - 4, 96, 150) : 96;
                const int leftWidth = 54; // the list of the options when the preview is beside it
                const std::size_t inner = sideBySide ? static_cast<std::size_t>(leftWidth - 4) : static_cast<std::size_t>(std::max(30, std::min(92, this->_screen.dimx() - 8)));
                ftxui::Elements optionRows;
                for (int i = 0; i <= static_cast<int>(options.size()); ++i) {
                    const bool other = i == static_cast<int>(options.size());
                    const bool recorded = !multi && !other && a.answers.value(q.value("question", std::string()), std::string()) == options[static_cast<std::size_t>(i)].value("label", std::string());
                    const std::string mark = multi ? (a.picked.contains(i) || (other && !a.other.empty()) ? "[x] " : "[ ] ") : (i == a.cursor ? "› " : "  ");
                    const std::string head = " " + mark + std::to_string(i + 1) + ". ";
                    const std::string indent(static_cast<std::size_t>(width_(head)), ' ');
                    const bool typing = other && i == a.cursor && !this->_askChat;  // its answer edited here: a field with its cursor
                    const std::string label = other ? "Other: " + a.other : options[static_cast<std::size_t>(i)].value("label", std::string()) + (recorded ? "  ✔" : "");
                    const std::string description = other ? "" : options[static_cast<std::size_t>(i)].value("description", std::string());
                    std::vector<std::string> labelLines = wrap_(label, inner - static_cast<std::size_t>(width_(head)));
                    if (labelLines.empty()) labelLines.push_back("");
                    ftxui::Elements rows;
                    if (typing) rows.push_back(ftxui::hbox({ftxui::text(head + "Other: "), this->lineView_(this->_askOther, std::max(8, static_cast<int>(inner) - static_cast<int>(width_(head)) - 8), 1)}));
                    else for (std::size_t l = 0; l < labelLines.size(); ++l)
                        rows.push_back(ftxui::text((l == 0 ? head : indent) + labelLines[l]));
                    const std::vector<std::string> descriptionLines = description.empty() ? std::vector<std::string>() : wrap_(description, inner - static_cast<std::size_t>(width_(head)) - 2);
                    for (std::size_t l = 0; l < descriptionLines.size(); ++l)
                        rows.push_back(ftxui::text(indent + (l == 0 ? "- " : "  ") + descriptionLines[l]) | ftxui::color(this->_theme.muted));
                    ftxui::Element row = ftxui::vbox(std::move(rows));
                    const int count = static_cast<int>(options.size());
                    // clicked: chosen (one choice: answered; several: ticked; Other: typed on)
                    optionRows.push_back(this->tap_(i == a.cursor ? row | ftxui::bold | ftxui::bgcolor(this->_theme.border) : row | ftxui::color(this->_theme.text), 1,
                        [this, i, count, multi]() {
                            this->_manager.asking().choose(i);
                            if (!multi && i < count) this->askNext_();
                        }));
                }
                if (anyPreview) {
                    // the preview of the option under the cursor (Other: none): beside the list, or under it on a narrow screen
                    const std::string previewText = a.cursor >= 0 && a.cursor < static_cast<int>(options.size()) ? options[static_cast<std::size_t>(a.cursor)].value("preview", std::string()) : std::string();
                    const int rowsRoom = std::clamp(this->_screen.dimy() - 14, 6, 26);
                    const int paneWidth = sideBySide ? boxWidth - leftWidth - 6 : boxWidth - 6;
                    ftxui::Element pane = previewText.empty() ? ftxui::text(" (no preview for this option)") | ftxui::color(this->_theme.muted) | ftxui::dim
                        : this->askPreview_(previewText, paneWidth, rowsRoom);
                    if (sideBySide) {
                        lines.push_back(ftxui::hbox({ftxui::vbox(std::move(optionRows)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, leftWidth), ftxui::separatorLight() | ftxui::color(this->_theme.border),
                            ftxui::hbox({ftxui::text(" "), pane}) | ftxui::flex}));
                    } else {
                        for (ftxui::Element& row: optionRows)
                            lines.push_back(std::move(row));
                        lines.push_back(ftxui::separatorLight() | ftxui::color(this->_theme.border));
                        lines.push_back(ftxui::hbox({ftxui::text(" "), pane}));
                    }
                } else {
                    for (ftxui::Element& row: optionRows)
                        lines.push_back(std::move(row));
                }
                lines.push_back(ftxui::text(""));
                const bool lastOne = a.index + 1 >= a.questions.size();
                lines.push_back(ftxui::hbox({ftxui::text(" ↑↓ move · 1-9 "), ftxui::text(multi ? "tick · " : "choose · "),
                    multi ? ftxui::hbox({press(ftxui::text("Enter / Space tick"), ftxui::Event::Return), ftxui::text(" · ")}) : press(ftxui::text("Enter validates · "), ftxui::Event::Return),
                    a.index > 0 ? press(ftxui::text("← / Shift+Tab back · "), ftxui::Event::ArrowLeft) : ftxui::emptyElement(),
                    a.questions.size() > 1 || multi ? press(ftxui::text(lastOne ? (a.questions.size() > 1 ? "→ / Tab review · " : "→ / Tab send · ") : "→ / Tab next (the answer kept) · "), ftxui::Event::ArrowRight)
                        : ftxui::emptyElement(), ftxui::text("type on Other · "),
                    press(ftxui::text("Esc decline"), ftxui::Event::Escape)}) | ftxui::color(this->_theme.muted) | ftxui::dim);
                // "Chat about this": a side chat about the questions (the session untouched), in its own area: wrapped, within a
                // height that keeps the popup inside the screen, scrolled (the newest at the bottom)
                if (!a.chat.empty() || a.chatting) {
                    if (a.chat.size() != this->_askChatSeen) {
                        this->_askChatSeen = a.chat.size();
                        this->_askChatScroll = 0;
                    }
                    struct ChatLine {
                        std::string head;
                        std::string text;
                        bool user;
                    };
                    std::vector<ChatLine> chat;
                    for (const auto &[role, said]: a.chat) {
                        const bool user = role == "user";
                        if (!chat.empty()) chat.push_back({"", "", user});
                        std::string clean = said;
                        for (std::size_t at = clean.find("**"); at != std::string::npos; at = clean.find("**", at))
                            clean.erase(at, 2); // the markdown stars: the text alone
                        const std::string head = user ? " you › " : " ✻ ";
                        const std::string indent(static_cast<std::size_t>(width_(head)), ' ');
                        bool first = true;
                        for (const std::string& paragraph: cluster::split(clean, '\n')) {
                            std::vector<std::string> wrapped = wrap_(paragraph, inner - static_cast<std::size_t>(width_(head)) - 2);
                            if (wrapped.empty()) wrapped.push_back("");
                            for (const std::string& line: wrapped) {
                                chat.push_back({first ? head : indent, line, user});
                                first = false;
                            }
                        }
                    }
                    if (a.chatting) chat.push_back({" ✻ ", "…", false});
                    // the room left: the screen minus the rest of the popup (its frame, the question, the options, the hints)
                    int used = 6;
                    for (const ftxui::Element& line: lines) {
                        line->ComputeRequirement();
                        used += std::max(1, line->requirement().min_y);
                    }
                    used += (this->_askChat ? 2 : 1) + (a.session != GLOBAL_ID ? 1 : 0) + (this->_manager.config().voice.enabled ? 1 : 0);
                    const int room = std::clamp(this->_screen.dimy() - used, 3, 16);
                    const int total = static_cast<int>(chat.size());
                    const int shown = std::min(total, room);
                    this->_askChatScroll = std::clamp(this->_askChatScroll, 0, std::max(0, total - shown));
                    const int last = total - this->_askChatScroll;
                    const int first = std::max(0, last - shown);
                    ftxui::Elements area;
                    if (first > 0)
                        area.push_back(ftxui::text(" ▲ " + std::to_string(first) + " line(s) above (PgUp, the wheel)") | ftxui::color(this->_theme.muted) | ftxui::dim);
                    for (int k = first; k < last; ++k) {
                        const ChatLine& c = chat[static_cast<std::size_t>(k)];
                        area.push_back(ftxui::hbox({ftxui::text(c.head) | ftxui::color(c.user ? this->_theme.text : this->_theme.accent) | ftxui::bold,
                            ftxui::text(c.text) | ftxui::color(c.user ? this->_theme.text : this->_theme.accent)}));
                    }
                    if (this->_askChatScroll > 0)
                        area.push_back(ftxui::text(" ▼ " + std::to_string(this->_askChatScroll) + " newer line(s) (PgDn)") | ftxui::color(this->_theme.muted) | ftxui::dim);
                    lines.push_back(ftxui::text(""));
                    lines.push_back(ftxui::window(ftxui::text(" chat about this ") | ftxui::color(this->_theme.muted), ftxui::vbox(std::move(area)))
                        | ftxui::color(this->_theme.border) | ftxui::reflect(this->_askChatBox));
                }
                if (this->_askChat) {
                    // several lines (Alt+Enter / Shift+Enter), wrapped, scrolled around the cursor; the keys of an editor, the mouse
                    lines.push_back(ftxui::hbox({ftxui::text(" chat › ") | ftxui::color(this->_theme.accent) | ftxui::bold,
                        this->lineView_(this->_askDraft, std::max(10, static_cast<int>(inner) - 32), 6),
                        ftxui::text("Enter send · Esc back ") | ftxui::color(this->_theme.muted) | ftxui::dim}));
                } else
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
                return box(ftxui::vbox(std::move(lines)), boxWidth);
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
            if (this->_mode == Mode::QuitAsk) {
                ftxui::Elements lines = {ftxui::text(" Sessions still running") | ftxui::bold, ftxui::text("")};
                for (const cluster::Snapshot& s: this->_manager.list(true))
                    if (s.state == cluster::State::Working || s.queued > 0)
                        lines.push_back(ftxui::text("   " + s.spec.name + "  " + (s.state == cluster::State::Working ? "working" : "idle")
                            + (s.queued > 0 ? " · " + std::to_string(s.queued) + " prompt(s) queued" : std::string())) | ftxui::color(this->_theme.muted));
                lines.push_back(ftxui::text(""));
                lines.push_back(press(ftxui::text(" [b] / Enter  keep them running in background: the core goes on without interface, its web access closed, and stops")
                    | ftxui::color(this->_theme.accent), ftxui::Event::Character('b')));
                lines.push_back(ftxui::text("              by itself once every session is idle (claude-cluster opened again meanwhile shows them)") | ftxui::color(this->_theme.muted));
                lines.push_back(press(ftxui::text(" [s] / " + this->_manager.config().keys.at("quit") + "  stop everything: the turns interrupted, the sessions kept for the next start")
                    | ftxui::color(this->_theme.warn), ftxui::Event::Character('s')));
                lines.push_back(press(ftxui::text(" [Esc]  back") | ftxui::color(this->_theme.muted), ftxui::Event::Escape));
                return box(ftxui::vbox(std::move(lines)), 110);
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
            this->_fieldsDrawn.clear();
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
            if (status.empty() && layout == "grid")
                status = "grid: ←↑↓→ between the cells (empty prompt; Alt+arrows always) · " + c.keys.at("grid_open") + " or Enter (empty prompt) opens the session in the list";
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
                cluster::demo_enabled() ? ftxui::text(" DEMO ") | ftxui::bold | ftxui::color(this->_theme.bg) | ftxui::bgcolor(this->_theme.warn) : ftxui::emptyElement(),
                key(" " + (this->_mode == Mode::Graph ? std::string(this->_graphHub ? "hub" : "graph") : this->_mode == Mode::Tree ? std::string("tree")
                    : layout == "grid" ? std::string("grid") : std::string("list")), "layout"),
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
            if (this->_manager.raiseRequest() != this->_raiseSeen) { // a notification clicked: this terminal to the front (X11), else marked
                this->_raiseSeen = this->_manager.raiseRequest();
                static const char* windowId = std::getenv("WINDOWID");
                if (windowId != nullptr && *windowId != '\0' && cluster::has_command("wmctrl")) cluster::detach({"wmctrl", "-i", "-a", windowId});
                std::cout << "\a" << std::flush; // the bell: the terminal asks for the attention (its urgency hint)
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
            this->_spinning = false;
            this->_reveal.frameStart();
            const int bodyHeight = size.dimy - 4 - this->voiceLines_();
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
            ftxui::Element screen = this->_mode == Mode::Settings ? this->settings_(size) : this->_mode == Mode::Graph ? ftxui::vbox({top, this->graph_(ftxui::Dimensions{size.dimx, std::max(5, size.dimy - 1)}) | ftxui::flex})
                | ftxui::color(this->_theme.text) | ftxui::bgcolor(this->_theme.bg) : this->_mode == Mode::Tree ? ftxui::vbox({top, this->tree_(ftxui::Dimensions{size.dimx, std::max(5, size.dimy - 1)}) | ftxui::flex})
                | ftxui::color(this->_theme.text) | ftxui::bgcolor(this->_theme.bg) : ftxui::dbox({document, this->modal_()});
            // a long operation: its popup over the screen (its spinner turning); one running: redrawn (its popup at 0.4 s)
            if (this->_manager.progressActive()) this->_spinning = true;
            const std::optional<cluster::Progress> running = this->_manager.progress();
            const bool busy = running && running->id != this->_progressHidden;
            if (busy) screen = ftxui::dbox({screen, this->progressBox_(*running)});
            // what failed of what the user did: over everything (OK / Retry), once its progress popup is gone (one after the other)
            if (const std::vector<cluster::Failure> failed = this->_manager.failures(); !failed.empty() && !busy) screen = ftxui::dbox({screen, this->failureBox_(failed)});
            if (!scan) return screen;
            this->_spinning = true;
            return ftxui::dbox({screen, this->scanBox_(*scan)});
        }

        _nodiscard bool upgradeShown_(void) const
        {
            const cluster::UpgradeOffer offer = this->_manager.upgradeOffer();
            return !offer.version.empty() && !offer.started && offer.version != this->_upgradeLater && this->_mode == Mode::Normal && !this->_manager.link().lost;
        }

        void upgrade_(const std::string& action)
        {
            try {
                this->status_(this->_manager.upgradeAct(action));
            } catch (const utils::exception::IException& e) {
                this->status_(e.info(), true);
            }
        }

        /* events */
        bool event_(ftxui::Event event)
        {
            if (event != ftxui::Event::Custom && !event.is_mouse() && this->upgradeShown_()) {
                // a newer claude-cluster installed: u / Enter update, i ignore it, q quit everything, Esc later
                static const std::array<const char*, 3> choices = {"update", "ignore", "quit"};
                if (event == ftxui::Event::ArrowRight || event == ftxui::Event::Tab) this->_upgradeFocus = (this->_upgradeFocus + 1) % 3;
                else if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::TabReverse) this->_upgradeFocus = (this->_upgradeFocus + 2) % 3;
                else if (event == ftxui::Event::Return) this->upgrade_(choices[static_cast<std::size_t>(this->_upgradeFocus)]);
                else if (event == ftxui::Event::Character('u')) this->upgrade_("update");
                else if (event == ftxui::Event::Character('i')) this->upgrade_("ignore");
                else if (event == ftxui::Event::Character('q')) this->upgrade_("quit");
                else if (event == ftxui::Event::Escape) this->_upgradeLater = this->_manager.upgradeOffer().version;
                return true;
            }
            if (event != ftxui::Event::Custom && !event.is_mouse() && this->_manager.link().lost) {
                // the core lost: its popup only (r / Enter: retry now, q / the quit key: quit)
                if (event == ftxui::Event::Character('r') || event == ftxui::Event::Character('R') || event == ftxui::Event::Return) this->_manager.retryNow();
                else if (event == ftxui::Event::Character('q') || event == ftxui::Event::Character('Q') || event.input() == std::string(1, '\x11')) this->_screen.Exit();
                return true;
            }
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
                if (const std::vector<cluster::Failure> failed = this->_manager.failures();
                    !failed.empty() && !(this->_manager.progress() && this->_manager.progress()->id != this->_progressHidden)) { // shown: after the progress one
                    if (cluster::key_action(this->_manager.config(), key_name_(event)) == "quit") { // the quit key never stuck behind it
                        this->quit_();
                        return true;
                    }
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
            }
            if (event.is_mouse()) {
                const ftxui::Mouse& mouse = event.mouse();
                if (this->_mode == Mode::Graph) { // its own: drag, wheel, click
                    (void)this->_graph.mouse(mouse);
                    return true;
                }
                this->_hover = this->hoverAt_(mouse);
                this->hitHover_(mouse);
                this->_hoverChip = -1;
                for (std::size_t i = 0; i < this->_chipBoxes.size(); ++i)
                    if (this->_chipBoxes[i].Contain(mouse.x, mouse.y)) this->_hoverChip = static_cast<int>(i);
                const bool list = this->_mode == Mode::Settings || this->_mode == Mode::Palette || this->_mode == Mode::History || this->_mode == Mode::Question || this->_mode == Mode::Rewind || this->_mode == Mode::Stash;
                const bool onPanels = this->_mode == Mode::Normal && this->_panelBox.Contain(mouse.x, mouse.y);
                for (cluster::LineEditor* drawn: this->_fieldsDrawn) // a text field of a popup: its cursor placed, a drag selects, a double click takes a word
                    if (drawn->mouse(mouse)) return true;
                if (this->_mode == Mode::Question && this->_askChatBox.Contain(mouse.x, mouse.y)
                    && (mouse.button == ftxui::Mouse::WheelUp || mouse.button == ftxui::Mouse::WheelDown)) { // the chat of the popup
                    this->_askChatScroll = mouse.button == ftxui::Mouse::WheelUp ? this->_askChatScroll + 3 : std::max(0, this->_askChatScroll - 3);
                    return true;
                }
                const bool wheel = mouse.button == ftxui::Mouse::WheelUp || mouse.button == ftxui::Mouse::WheelDown;
                const int step = mouse.button == ftxui::Mouse::WheelUp ? -1 : 1;
                if (wheel && this->_mode == Mode::Normal && this->_listBox.Contain(mouse.x, mouse.y)) { // the list of sessions: scrolled, the selection kept
                    this->_listFirst = static_cast<std::size_t>(std::max<int>(0, static_cast<int>(this->_listFirst) + step)); // clamped at the drawing
                    return true;
                std::string onPin;   // the pinned item under the mouse (its own scroll)
                if (this->_mode == Mode::Normal)
                    for (const auto &[id, view]: this->_views)
                        if (id.starts_with("pin:") && view.box.Contain(mouse.x, mouse.y)) onPin = id;
                if (!onPin.empty() && (mouse.button == ftxui::Mouse::WheelUp || mouse.button == ftxui::Mouse::WheelDown)) {
                    this->_scroll[onPin] = std::max(0, this->_scroll[onPin] + (mouse.button == ftxui::Mouse::WheelUp ? 3 : -3));
                    return true;
                }
                }
                if (wheel && this->_mode == Mode::Normal && this->_gridRows > this->_gridVisible && this->_gridBox.Contain(mouse.x, mouse.y) && !mouse.control && !mouse.shift) {
                    this->_gridFirst += step; // the rows of the grid (Ctrl / Shift + wheel: the transcript as before)
                    return true;
                }
                if (mouse.button == ftxui::Mouse::WheelUp) {
                    if (list) return this->event_(ftxui::Event::ArrowUp);   // the item above
                    if (onPanels) this->_panelScroll = std::max(0, this->_panelScroll - 3);
                    else this->_scroll[this->_active] += 3;
                    return true;
                }
                if (mouse.button == ftxui::Mouse::WheelDown) {
                    if (list) return this->event_(ftxui::Event::ArrowDown);
                    if (onPanels) this->_panelScroll += 3; // clamped at the drawing
                    else this->_scroll[this->_active] = std::max(0, this->_scroll[this->_active] - 3);
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
                if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Right && this->_mode == Mode::Normal) {
                    // a message or a block of a transcript right-clicked: Pin / Unpin (the pinned item itself: Unpin)
                    for (const auto &[id, view]: this->_views) {
                        if (!view.box.Contain(mouse.x, mouse.y)) continue;
                        const std::size_t y = static_cast<std::size_t>(mouse.y - view.box.y_min);
                        const bool pinView = id.starts_with("pin:");
                        if (!pinView && (y >= view.entries.size() || view.entries[y] == std::string::npos)) break;
                        this->_active = pinView ? id.substr(4) : id;
                        this->pick_("pin_menu:" + this->_active + ":" + (pinView ? std::string("pin") : std::to_string(view.entries[y])));
                        return true;
                    }
                }
                if (this->_mode != Mode::Normal) return true;
                if (mouse.motion == ftxui::Mouse::Pressed && this->_hoverChip >= 0 && this->_hoverChip < static_cast<int>(this->_chips.size())) {
                    // a chip: click puts it back in the prompt, right click opens the image (viewer of the desktop)
                    const std::size_t index = this->_chips[static_cast<std::size_t>(this->_hoverChip)];
                    const cluster::Atom& atom = this->_editor.atom(index);
                    if (mouse.button == ftxui::Mouse::Right && !atom.image.empty()) {
                        this->openPicture_(atom.image);
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
                if (this->_mode == Mode::QuitAsk || startup) this->_screen.Exit(); // asked already (the quit key again: everything stopped)
                else this->quit_();
                return true;
            }
            switch (this->_mode) {
                case Mode::QuitAsk:
                    if (event == ftxui::Event::Character('b') || event == ftxui::Event::Character('B') || event == ftxui::Event::Return) {
                        this->_manager.requestBackground();
                        this->_screen.Exit();
                    } else if (event == ftxui::Event::Character('s') || event == ftxui::Event::Character('S')) {
                        this->_screen.Exit();
                    } else if (event == ftxui::Event::Escape) {
                        this->_mode = Mode::Normal;
                    }
                    return true;
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
                    } else {
                        (void)this->fieldKey_(this->_answer, event);
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
                    if (event == ftxui::Event::PageUp || event == ftxui::Event::ArrowUpCtrl) { // the chat about it: older lines
                        this->_askChatScroll += event == ftxui::Event::PageUp ? 8 : 2; // clamped at the drawing
                        return true;
                    }
                    if (event == ftxui::Event::PageDown || event == ftxui::Event::ArrowDownCtrl) {
                        this->_askChatScroll = std::max(0, this->_askChatScroll - (event == ftxui::Event::PageDown ? 8 : 2));
                        return true;
                    }
                    if (ask->review) { // the review: Submit / Cancel / back to edit
                        this->_askChat = false;
                        if (event == ftxui::Event::Return || event == ftxui::Event::Character('1')) {
                            this->askNext_();
                        } else if (event == ftxui::Event::Escape || event == ftxui::Event::Character('2')) {
                            asking.decline();
                            this->status_("question declined");
                            this->_mode = Mode::Normal;
                        } else if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowUp || event == ftxui::Event::Backspace || event == ftxui::Event::TabReverse) {
                            asking.back();
                        }
                        return true;
                    }
                    if (this->_askChat) { // its chat typed (a cursor: arrows, Home / End, Delete...): Enter sent, Esc back to the options
                        if (event == ftxui::Event::Escape) {
                            this->_askChat = false;
                        } else if (event == ftxui::Event::Return) {
                            const std::string done = asking.chat(this->_askDraft.text());
                            if (done.empty()) this->_askDraft.clear();
                            else this->status_(done, true);
                        } else {
                            (void)this->_askDraft.key(event);
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
                    } else if (event == ftxui::Event::ArrowDown) {
                        asking.move(1);
                    } else if (event == ftxui::Event::Tab) { // like Claude Code: Tab / Shift+Tab between the questions
                        if (!asking.forward()) this->status_("choose an option first (Enter)", true);
                    } else if (event == ftxui::Event::TabReverse) {
                        asking.back();
                    } else if (event == ftxui::Event::ArrowLeft && (ask->cursor != count || (this->_askOther.text().empty() && ask->index > 0))) {
                        asking.back(); // ← on an option, or on an empty Other: the question before
                    } else if (event == ftxui::Event::ArrowRight && (ask->cursor != count || this->_askOther.cursor() >= this->_askOther.text().size())) {
                        if (!asking.forward()) this->status_("choose an option first (Enter)", true); // the next question, answered or not (the review flags the ones left)
                    } else if (event == ftxui::Event::Return) {
                        if (multi) asking.toggle(); // several choices: Enter ticks / unticks (Tab / → goes on)
                        else this->askNext_();      // one choice: the option under the cursor is the answer
                    } else if (ask->cursor == count && this->_askOther.key(event)) {
                        // on Other: the answer edited with its cursor (digits and spaces too), the question told at each change
                        if (this->_askOther.text() != ask->other) asking.setOther(this->_askOther.text());
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
                    else {
                        const std::string before = this->_filter;
                        if (this->fieldKey_(this->_filter, event) && this->_filter != before) {
                            this->_selected = 0;
                            this->filter_();
                        }
                    }
                    return true;
                case Mode::Prompt:
                    if (event == ftxui::Event::Escape) this->_mode = Mode::Normal;
                    else if (event == ftxui::Event::Return) this->run_(this->_pending, this->_prompt);
                    else if (this->_password) { // blind: typed and erased at the end only (no cursor moves, no clipboard)
                        if (event == ftxui::Event::Backspace) {
                            while (!this->_prompt.empty() && (static_cast<unsigned char>(this->_prompt.back()) & 0xC0) == 0x80) this->_prompt.pop_back();
                            if (!this->_prompt.empty()) this->_prompt.pop_back();
                        } else if (event.is_character()) {
                            this->_prompt += event.character();
                        }
                    } else {
                        (void)this->fieldKey_(this->_prompt, event);
                    }
                    return true;
                case Mode::Settings:
                    return this->settingsEvent_(event);
                case Mode::Form:
                    return this->formEvent_(event);
                case Mode::Plugins:
                    return this->plugEvent_(event);
                case Mode::Graph:
                    if (cluster::key_action(this->_manager.config(), key) == "layout") { // F2: graph -> hub -> tree
                        if (this->_graphHub) this->openTree_();
                        else this->openGraph_(true);
                        return true;
                    }
                    if (cluster::key_action(this->_manager.config(), key) == "graph" || this->_graph.key(event, key, this->_manager.config().keys) == cluster::GraphView::Result::Close)
                        this->_mode = Mode::Normal;
                    return true;
                case Mode::Tree: {
                    // the sessions by folder: ↑ ↓ choose, Enter opens it (the list), F2 the list again, Esc closes
                    const int count = static_cast<int>(this->_treeIds.size());
                    if (cluster::key_action(this->_manager.config(), key) == "layout" || event == ftxui::Event::Escape) {
                        this->_manager.setUi("layout", "list");
                        this->_mode = Mode::Normal;
                    } else if (count > 0 && (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j'))) {
                        this->_treeSel = (this->_treeSel + 1) % count;
                    } else if (count > 0 && (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k'))) {
                        this->_treeSel = (this->_treeSel + count - 1) % count;
                    } else if (count > 0 && event == ftxui::Event::Return) {
                        this->_active = this->_treeIds[static_cast<std::size_t>(std::clamp(this->_treeSel, 0, count - 1))];
                        this->_manager.setUi("layout", "list");
                        this->_mode = Mode::Normal;
                    } else {
                        (void)this->shortcut_(cluster::key_action(this->_manager.config(), key));
                    }
                    return true;
                }
                case Mode::Search:
                    if (event == ftxui::Event::Escape) {
                        this->_mode = Mode::Normal;
                        this->_scroll[this->_active] = 0;
                    } else if (event == ftxui::Event::Return || event == ftxui::Event::ArrowUp || cluster::key_action(this->_manager.config(), key) == "search") {
                        if (!this->_matches.empty()) this->_hit = (this->_hit + static_cast<int>(this->_matches.size()) - 1) % static_cast<int>(this->_matches.size());
                    } else if (event == ftxui::Event::ArrowDown) {
                        if (!this->_matches.empty()) this->_hit = (this->_hit + 1) % static_cast<int>(this->_matches.size());
                    } else {
                        const std::string before = this->_search;
                        if (this->fieldKey_(this->_search, event) && this->_search != before) this->searchUpdate_();
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
                    } else {
                        const std::string before = this->_filter;
                        if (this->fieldKey_(this->_filter, event) && this->_filter != before) {
                            this->_selected = 0;
                            this->historyFilter_();
                        }
                    }
                    return true;
                case Mode::Rewind:
                    return this->rewindKey_(event);
                case Mode::Stash:
                    return this->stashKey_(event);
                case Mode::Normal:
                    break;
            }

            // prefix (Ctrl+B) then an arrow: change of session
            if (this->_prefixUntil > 0) {
                const bool armed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() <= this->_prefixUntil;
                this->_prefixUntil = 0;
                this->_status.clear();
                if (armed && (event == ftxui::Event::PageUp || event == ftxui::Event::PageDown || event == ftxui::Event::Home || event == ftxui::Event::End)) {
                    this->sessionMove_(event == ftxui::Event::PageUp ? "session_page_up" : event == ftxui::Event::PageDown ? "session_page_down"
                        : event == ftxui::Event::Home ? "session_first" : "session_last");
                    return true;
                }
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
                    // its choice taken; nothing changed (the word already the one listed): Enter sends the prompt
                    const std::string before = this->_editor.text();
                    this->_editor.accept();
                    if (cluster::trim(this->_editor.text()) != cluster::trim(before)) return true;
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
            if (this->_mode == Mode::Normal && (event == ftxui::Event::ArrowUpCtrl || event == ftxui::Event::ArrowDownCtrl)) { // the side panels scrolled
                this->_panelScroll = std::max(0, this->_panelScroll + (event == ftxui::Event::ArrowDownCtrl ? 3 : -3));
                return true;
            }
            if (event == ftxui::Event::Escape && !this->_app.voice->pending().empty()) {
                this->_app.voice->cancelLast();
                this->status_("voice prompt dropped (Ctrl+C: all of them)");
                return true;
            }
            if (this->_active == TASKS_ID) { // the Tasks session: no prompt; its items by the keys, its shortcuts still (next / previous, Ctrl+B...)
                // the grid: Shift+Enter / Alt+O (and o, a terminal sending Shift+Enter as a plain Enter) open the Tasks in the list
                if (event == ftxui::Event::Character('o') && this->shortcut_("grid_open")) return true;
                if (this->tasksKey_(event)) return true;
                (void)this->shortcut_(cluster::key_action(this->_manager.config(), key));
                return true;
            }
            if (const std::optional<Arrow> nav = arrow_(event); nav && this->gridArrow_(*nav)) return true;
            // grid, empty prompt: Enter opens the focused session in the list too (a terminal that sends Shift+Enter as a plain Enter)
            if (event == ftxui::Event::Return && this->_editor.text().empty() && !this->_editor.completing() && this->shortcut_("grid_open")) return true;
            this->vimSync_();
            if (this->vimKey_(event, key)) return true;
            if (cluster::key_action(this->_manager.config(), key) == "grid_open" && this->shortcut_("grid_open")) return true; // grid: Shift+Enter opens, not a new line
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
            if (cluster::key_action(this->_manager.config(), key) == "take_back") {
                this->takeBack_();
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
                // Up / Down: a visual row of the prompt (as wrapped on the screen); on its first / last row, the history
                if (this->visualStep_(arrow->dy, arrow->shift)) return true;
                if (arrow->dy < 0 && !arrow->shift && this->_editor.onFirstLine()) (void)this->historyStep_(1);
                else if (arrow->dy > 0 && !arrow->shift && this->_editor.onLastLine()) (void)this->historyStep_(-1);
                else if (arrow->dy < 0) this->_editor.lineUp(arrow->shift);
                else this->_editor.lineDown(arrow->shift);
            } else if (arrow && arrow->dx != 0) {
                if (arrow->ctrl) arrow->dx < 0 ? this->_editor.wordLeft(arrow->shift) : this->_editor.wordRight(arrow->shift);
                else arrow->dx < 0 ? this->_editor.left(arrow->shift) : this->_editor.right(arrow->shift);
            } else if (event == ftxui::Event::Character(' ')) this->_editor.space();
            else if (event.is_character()) this->_editor.insert(event.character());
            else if (!cluster::editing_key(this->_editor, event, true, false)) return false; // the other editing keys (words, kill ring, Ctrl+Backspace...), the bound ones went before
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
            if (vim == cluster::VimMode::Normal && (up || down) && this->visualStep_(up ? -1 : 1, false)) return true;
            if (vim == cluster::VimMode::Normal && ((up && this->onFirstRow_() && this->historyStep_(1)) || (down && this->onLastRow_() && this->historyStep_(-1))))
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
            if (this->_manager.ui("layout", this->_manager.config().layout) == "grid") this->_manager.setUi("layout", "list"); // every start on the list (F2: the grid, the graph)
            if (const std::string start = this->_app.options.layout; start == "graph" || start == "hub" || start == "tree") { // asked on the command line: that view
                this->_manager.setUi("layout", "list");
                if (start == "tree") this->openTree_();
                else this->openGraph_(start == "hub");
            }
            this->_machineStartup = this->_app.askMachine != 0 && !this->_webTty; // the terminal page never maps the computer
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
                int spins = 0;
                while (alive) {
                    // a change, the typing (25 images / s), the spinners (8 images / s: they turn every 120 ms), else once a second
                    // (timers); a quit signal: out
                    std::this_thread::sleep_for(std::chrono::milliseconds(40));
                    if (cluster::quit_requested()) {
                        this->_screen.Exit();
                        break;
                    }
                    const std::uint64_t version = this->_manager.version() + this->_app.voice->version();
                    const bool spin = this->_spinning && ++spins >= 3;
                    if (version != last || this->_animating || this->_sendPending || spin || ++ticks >= 25) {
                        last = version;
                        ticks = 0;
                        spins = 0;
                        this->_screen.PostEvent(ftxui::Event::Custom);
                    }
                }
            });
            ftxui::Component root = ftxui::Renderer([this]() {return this->render_();});
            // Ctrl+Z: its action (undo) or nothing, never the interface stopped in the background
            root = ftxui::CatchEvent(root, [this](ftxui::Event event) {return this->event_(event) || event.input() == "\x1a";});
            cluster::debug("render", "terminal " + std::to_string(this->_screen.dimx()) + "x" + std::to_string(this->_screen.dimy()) + " (" + cluster::platform_summary() + ")");
            this->_screen.Loop(root);
            this->_worker.stop(); // the running job ends before the screen goes
            // the terminal as it was: the frames buffer out first (it hides the cursor), then the modes off, the cursor
            // shown, the synchronized update closed
            std::cout << std::flush;
            std::cout.rdbuf(previous);
            std::cout << "\x1b[?2026l\x1b[?1004l\x1b[?2004l\x1b[>4;0m\x1b[?25h" << std::flush; // xstyle: ignore LU-ANSI (modes of the terminal, no color)
            if (tty) ::tcsetattr(STDIN_FILENO, TCSANOW, &original);
            alive = false;
            refresher.join();
            (void)std::signal(SIGTSTP, stop);
            return 0;
        }

        TtyUi(cluster::App& app): _app(app), _manager(*app.manager) { /* Nothing */ };
};

} // namespace

/* entry */
namespace {
    // what the terminal gets back when the interface ends abnormally (a crash, an abort): no mouse reports, the main screen,
    // the cursor, no bracketed paste / focus events (async signal safe: one write)
    // xstyle: ignore-next LU-ANSI (one write, async signal safe: no libutils call in a signal handler)
    constexpr const char TERMINAL_RESET[] = "\x1b[>4;0m\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1006l\x1b[?1015l\x1b[?2004l\x1b[?1004l\x1b[?1049l\x1b[?25h\x1b[0m";

    void terminal_reset_(void)
    {
        (void)!::write(STDOUT_FILENO, TERMINAL_RESET, sizeof(TERMINAL_RESET) - 1);
    }

    void terminal_fatal_(const int signal)
    {
        terminal_reset_();
        (void)std::signal(signal, SIG_DFL);
        (void)::raise(signal);
    }
}

_cold int cluster::run_tty(cluster::App& app)
{
    // a crash or an abort never leaves the terminal of the user reporting the mouse (its shell full of escape codes)
    for (const int signal: {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL})
        (void)std::signal(signal, terminal_fatal_);
    std::set_terminate([]() {
        terminal_reset_();
        std::abort();
    });
    TtyUi ui(app);
    return ui.run();
}
