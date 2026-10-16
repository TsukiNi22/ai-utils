/**************************************************************\
Edition:
##  @date 16/10/2026 by @author Tsukini

File Name:
##  @file Vt.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Core.hpp"
#include "cluster/Vt.hpp"
#include <algorithm>
#include <iostream>
#include <iterator>
#include <cstdlib>

/* tools */
_cold static std::uint32_t basic_color_(const int index)
{
    // the 16 colors (the ones of the terminals of today)
    static const std::uint32_t colors[16] = {0x000000, 0xcd3131, 0x0dbc79, 0xe5e510, 0x2472c8, 0xbc3fbc, 0x11a8cd, 0xe5e5e5,
        0x666666, 0xf14c4c, 0x23d18b, 0xf5f543, 0x3b8eea, 0xd670d6, 0x29b8db, 0xffffff};
    return colors[std::clamp(index, 0, 15)];
}

_cold static std::uint32_t decode_utf8_(const std::string& ch)
{
    const unsigned char c0 = static_cast<unsigned char>(ch[0]);
    if (ch.size() == 1) return c0;
    if (ch.size() == 2) return ((c0 & 0x1Fu) << 6) | (static_cast<unsigned char>(ch[1]) & 0x3Fu);
    if (ch.size() == 3) return ((c0 & 0x0Fu) << 12) | ((static_cast<unsigned char>(ch[1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(ch[2]) & 0x3Fu);
    return ((c0 & 0x07u) << 18) | ((static_cast<unsigned char>(ch[1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(ch[2]) & 0x3Fu) << 6)
        | (static_cast<unsigned char>(ch[3]) & 0x3Fu);
}

/* constructor */
_cold cluster::Vt::Vt(const int cols, const int rows)
    : _cols(std::max(1, cols)), _rows(std::max(1, rows)), _grid(static_cast<std::size_t>(std::max(1, rows)), std::vector<cluster::VtCell>(static_cast<std::size_t>(std::max(1, cols))))
{
}

/* public */
_cold void cluster::Vt::resize(const int cols, const int rows)
{
    this->_cols = std::max(1, cols);
    this->_rows = std::max(1, rows);
    this->_grid.resize(static_cast<std::size_t>(this->_rows));
    for (std::vector<cluster::VtCell>& row: this->_grid)
        row.resize(static_cast<std::size_t>(this->_cols));
    this->_x = std::min(this->_x, this->_cols - 1);
    this->_y = std::min(this->_y, this->_rows - 1);
    ++this->_changes;
}

_cold std::uint32_t cluster::Vt::palette(const int index)
{
    // 0-15 the basic ones, 16-231 the 6x6x6 cube, 232-255 the grays
    if (index < 16) return basic_color_(index);
    if (index < 232) {
        const int i = index - 16;
        static const std::uint32_t levels[6] = {0, 95, 135, 175, 215, 255};
        return (levels[i / 36] << 16) | (levels[(i / 6) % 6] << 8) | levels[i % 6];
    }
    const std::uint32_t gray = static_cast<std::uint32_t>(8 + (std::clamp(index, 232, 255) - 232) * 10);
    return (gray << 16) | (gray << 8) | gray;
}

_cold int cluster::Vt::width(const std::string& ch)
{
    // combining marks: 0; the wide scripts (CJK, hangul, the emoji): 2; else 1
    if (ch.empty()) return 0;
    const std::uint32_t c = decode_utf8_(ch);
    if ((c >= 0x0300 && c <= 0x036F) || (c >= 0x200B && c <= 0x200F) || c == 0xFE0F || (c >= 0xFE00 && c <= 0xFE0F)) return 0;
    if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF)
        || (c >= 0xFE30 && c <= 0xFE4F) || (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6) || (c >= 0x1F300 && c <= 0x1FAFF)
        || (c >= 0x20000 && c <= 0x3FFFD))
        return 2;
    return 1;
}

_cold void cluster::Vt::feed(const std::string& bytes)
{
    for (const char raw: bytes) {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (!this->_escape.empty()) {
            this->_escape += raw;
            const char kind = this->_escape[1];
            if (this->_escape.size() == 2) {
                if (kind == '[' || kind == ']' || kind == 'P' || kind == '(' || kind == ')' || kind == '_' || kind == '^') continue; // more to come
                // a single one: 7 save, 8 restore, M reverse index, D index, E next line, c reset, = > keypad
                if (kind == '7') {
                    this->_savedX = this->_x;
                    this->_savedY = this->_y;
                } else if (kind == '8') {
                    this->_x = std::min(this->_savedX, this->_cols - 1);
                    this->_y = std::min(this->_savedY, this->_rows - 1);
                } else if (kind == 'M') {
                    if (this->_y > 0) --this->_y;
                    else {
                        this->_grid.pop_back();
                        this->_grid.insert(this->_grid.begin(), std::vector<cluster::VtCell>(static_cast<std::size_t>(this->_cols)));
                    }
                } else if (kind == 'D') {
                    this->newline_();
                } else if (kind == 'E') {
                    this->_x = 0;
                    this->newline_();
                } else if (kind == 'c') {
                    this->erase_(0, 0, this->_cols - 1, this->_rows - 1);
                    this->_x = this->_y = 0;
                    this->_attr = cluster::VtCell();
                }
                this->_escape.clear();
                ++this->_changes;
                continue;
            }
            if (kind == '[') { // CSI: until a final byte (0x40-0x7E)
                if (c >= 0x40 && c <= 0x7E) {
                    this->csi_(this->_escape.substr(2));
                    this->_escape.clear();
                }
            } else if (kind == ']' || kind == 'P' || kind == '_' || kind == '^') { // OSC / DCS / APC / PM: until BEL or ESC \ (ignored)
                if (c == 0x07 || (c == '\\' && this->_escape.size() >= 2 && this->_escape[this->_escape.size() - 2] == 0x1b)) this->_escape.clear();
            } else { // a charset ( or ): one more byte
                this->_escape.clear();
            }
            continue;
        }
        if (c == 0x1b) {
            this->_utf8.clear();
            this->_escape = "\x1b";
            continue;
        }
        if (c < 0x20 || c == 0x7f) {
            this->_utf8.clear();
            if (c == '\r') this->_x = 0;
            else if (c == '\n' || c == 0x0b || c == 0x0c) this->newline_();
            else if (c == '\b') this->_x = std::max(0, this->_x - 1);
            else if (c == '\t') this->_x = std::min(this->_cols - 1, (this->_x / 8 + 1) * 8);
            ++this->_changes;
            continue;
        }
        // UTF-8: the bytes of a character gathered
        this->_utf8 += raw;
        const unsigned char first = static_cast<unsigned char>(this->_utf8[0]);
        const std::size_t need = first < 0x80 ? 1 : (first >> 5) == 0x6 ? 2 : (first >> 4) == 0xE ? 3 : (first >> 3) == 0x1E ? 4 : 1;
        if (this->_utf8.size() < need) continue;
        this->put_(this->_utf8);
        this->_utf8.clear();
    }
}

_cold std::string cluster::Vt::text(void) const
{
    std::string out;
    for (const std::vector<cluster::VtCell>& row: this->_grid) {
        std::string line;
        for (const cluster::VtCell& cell: row)
            line += cell.ch.empty() ? "" : cell.ch;
        while (!line.empty() && line.back() == ' ')
            line.pop_back();
        out += line + "\n";
    }
    return out;
}

/* private */
_cold void cluster::Vt::put_(const std::string& ch)
{
    const int w = cluster::Vt::width(ch);
    if (w == 0) { // a combining mark: joined to the cell before
        if (this->_x > 0) this->_grid[static_cast<std::size_t>(this->_y)][static_cast<std::size_t>(this->_x - 1)].ch += ch;
        return;
    }
    if (this->_x + w > this->_cols) { // the end of the line: the next one (autowrap off: the last cells overwritten)
        if (this->_wrap) {
            this->_x = 0;
            this->newline_();
        } else {
            this->_x = std::max(0, this->_cols - w);
        }
    }
    cluster::VtCell cell = this->_attr;
    cell.ch = ch;
    this->_grid[static_cast<std::size_t>(this->_y)][static_cast<std::size_t>(this->_x)] = cell;
    if (w == 2 && this->_x + 1 < this->_cols) {
        cluster::VtCell half = this->_attr;
        half.ch.clear();
        this->_grid[static_cast<std::size_t>(this->_y)][static_cast<std::size_t>(this->_x + 1)] = half;
    }
    this->_x = std::min(this->_cols - 1, this->_x + w);
    ++this->_changes;
}

_cold void cluster::Vt::newline_(void)
{
    if (this->_y + 1 < this->_rows) {
        ++this->_y;
        return;
    }
    this->_grid.erase(this->_grid.begin());
    this->_grid.emplace_back(static_cast<std::size_t>(this->_cols));
    ++this->_changes;
}

_cold void cluster::Vt::clearRow_(const int y, const int fromX, const int toX)
{
    for (int x = std::max(0, fromX); x <= std::min(toX, this->_cols - 1); ++x) {
        cluster::VtCell blank;
        blank.bg = this->_attr.bg;
        blank.bgSet = this->_attr.bgSet;
        this->_grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = blank;
    }
}

_cold void cluster::Vt::erase_(const int fromX, const int fromY, const int toX, const int toY)
{
    for (int y = std::max(0, fromY); y <= std::min(toY, this->_rows - 1); ++y)
        this->clearRow_(y, y == fromY ? fromX : 0, y == toY ? toX : this->_cols - 1);
    ++this->_changes;
}

_cold void cluster::Vt::csi_(const std::string& sequence)
{
    // the parameters (numbers; a private ?), the final byte
    const char final = sequence.back();
    std::string body = sequence.substr(0, sequence.size() - 1);
    const bool priv = !body.empty() && body[0] == '?';
    if (priv) body.erase(0, 1);
    std::vector<int> params;
    std::string number;
    for (const char c: body + ";") {
        if (c >= '0' && c <= '9') number += c;
        else {
            params.push_back(number.empty() ? -1 : std::atoi(number.c_str()));
            number.clear();
        }
    }
    if (params.size() == 1 && params[0] == -1) params.clear();
    const int n = params.empty() || params[0] <= 0 ? 1 : params[0];
    if (priv) { // the modes
        for (const int p: params) {
            if (p == 25) this->_cursor = final == 'h';
            else if (p == 7) this->_wrap = final == 'h';
            else if (p == 1000 || p == 1002 || p == 1003 || p == 1006) this->_mouse = final == 'h';
            else if (p == 2004) this->_paste = final == 'h';
            else if (p == 1049 || p == 47 || p == 1047) {
                if (final == 'h') this->erase_(0, 0, this->_cols - 1, this->_rows - 1); // the alternate screen: blank
                this->_x = this->_y = 0;
            }
        }
        ++this->_changes;
        return;
    }
    switch (final) {
        case 'A': this->_y = std::max(0, this->_y - n); break;
        case 'B': this->_y = std::min(this->_rows - 1, this->_y + n); break;
        case 'C': this->_x = std::min(this->_cols - 1, this->_x + n); break;
        case 'D': this->_x = std::max(0, this->_x - n); break;
        case 'E': this->_x = 0; this->_y = std::min(this->_rows - 1, this->_y + n); break;
        case 'F': this->_x = 0; this->_y = std::max(0, this->_y - n); break;
        case 'G': this->_x = std::clamp(n - 1, 0, this->_cols - 1); break;
        case 'd': this->_y = std::clamp(n - 1, 0, this->_rows - 1); break;
        case 'H':
        case 'f':
            this->_y = std::clamp((params.empty() || params[0] <= 0 ? 1 : params[0]) - 1, 0, this->_rows - 1);
            this->_x = std::clamp((params.size() < 2 || params[1] <= 0 ? 1 : params[1]) - 1, 0, this->_cols - 1);
            break;
        case 'J': {
            const int mode = params.empty() || params[0] < 0 ? 0 : params[0];
            if (mode == 0) this->erase_(this->_x, this->_y, this->_cols - 1, this->_rows - 1);
            else if (mode == 1) this->erase_(0, 0, this->_x, this->_y);
            else this->erase_(0, 0, this->_cols - 1, this->_rows - 1);
            break;
        }
        case 'K': {
            const int mode = params.empty() || params[0] < 0 ? 0 : params[0];
            if (mode == 0) this->clearRow_(this->_y, this->_x, this->_cols - 1);
            else if (mode == 1) this->clearRow_(this->_y, 0, this->_x);
            else this->clearRow_(this->_y, 0, this->_cols - 1);
            break;
        }
        case 'X': this->clearRow_(this->_y, this->_x, this->_x + n - 1); break;
        case 'P': { // characters deleted, the rest of the line shifted left
            std::vector<cluster::VtCell>& row = this->_grid[static_cast<std::size_t>(this->_y)];
            for (int i = 0; i < n && this->_x < this->_cols; ++i) {
                row.erase(row.begin() + this->_x);
                row.emplace_back();
            }
            break;
        }
        case '@': { // blanks inserted
            std::vector<cluster::VtCell>& row = this->_grid[static_cast<std::size_t>(this->_y)];
            for (int i = 0; i < n && this->_x < this->_cols; ++i) {
                row.insert(row.begin() + this->_x, cluster::VtCell());
                row.pop_back();
            }
            break;
        }
        case 'L': // lines inserted at the cursor
            for (int i = 0; i < n; ++i) {
                this->_grid.pop_back();
                this->_grid.insert(this->_grid.begin() + this->_y, std::vector<cluster::VtCell>(static_cast<std::size_t>(this->_cols)));
            }
            break;
        case 'M': // lines deleted at the cursor
            for (int i = 0; i < n; ++i) {
                this->_grid.erase(this->_grid.begin() + this->_y);
                this->_grid.emplace_back(static_cast<std::size_t>(this->_cols));
            }
            break;
        case 'S': // scrolled up
            for (int i = 0; i < n; ++i) {
                this->_grid.erase(this->_grid.begin());
                this->_grid.emplace_back(static_cast<std::size_t>(this->_cols));
            }
            break;
        case 'T': // scrolled down
            for (int i = 0; i < n; ++i) {
                this->_grid.pop_back();
                this->_grid.insert(this->_grid.begin(), std::vector<cluster::VtCell>(static_cast<std::size_t>(this->_cols)));
            }
            break;
        case 's': this->_savedX = this->_x; this->_savedY = this->_y; break;
        case 'u': this->_x = std::min(this->_savedX, this->_cols - 1); this->_y = std::min(this->_savedY, this->_rows - 1); break;
        case 'm': this->sgr_(params); break;
        default: break; // r (scroll region), n (reports), t (window), h / l (modes of the ANSI set)...: ignored
    }
    ++this->_changes;
}

_cold void cluster::Vt::sgr_(const std::vector<int>& params)
{
    if (params.empty()) {
        this->_attr = cluster::VtCell();
        return;
    }
    for (std::size_t i = 0; i < params.size(); ++i) {
        const int p = params[i] < 0 ? 0 : params[i];
        if (p == 0) this->_attr = cluster::VtCell();
        else if (p == 1) this->_attr.bold = true;
        else if (p == 2) this->_attr.dim = true;
        else if (p == 3) this->_attr.italic = true;
        else if (p == 4) this->_attr.underline = true;
        else if (p == 7) this->_attr.inverse = true;
        else if (p == 9) this->_attr.strike = true;
        else if (p == 22) this->_attr.bold = this->_attr.dim = false;
        else if (p == 23) this->_attr.italic = false;
        else if (p == 24) this->_attr.underline = false;
        else if (p == 27) this->_attr.inverse = false;
        else if (p == 29) this->_attr.strike = false;
        else if (p >= 30 && p <= 37) { this->_attr.fg = basic_color_(p - 30); this->_attr.fgSet = true; }
        else if (p == 39) this->_attr.fgSet = false;
        else if (p >= 40 && p <= 47) { this->_attr.bg = basic_color_(p - 40); this->_attr.bgSet = true; }
        else if (p == 49) this->_attr.bgSet = false;
        else if (p >= 90 && p <= 97) { this->_attr.fg = basic_color_(p - 90 + 8); this->_attr.fgSet = true; }
        else if (p >= 100 && p <= 107) { this->_attr.bg = basic_color_(p - 100 + 8); this->_attr.bgSet = true; }
        else if ((p == 38 || p == 48) && i + 1 < params.size()) {
            std::uint32_t color = 0;
            bool got = false;
            if (params[i + 1] == 5 && i + 2 < params.size()) {
                color = cluster::Vt::palette(std::clamp(params[i + 2], 0, 255));
                got = true;
                i += 2;
            } else if (params[i + 1] == 2 && i + 4 < params.size()) {
                color = (static_cast<std::uint32_t>(std::clamp(params[i + 2], 0, 255)) << 16) | (static_cast<std::uint32_t>(std::clamp(params[i + 3], 0, 255)) << 8)
                    | static_cast<std::uint32_t>(std::clamp(params[i + 4], 0, 255));
                got = true;
                i += 4;
            }
            if (got && p == 38) { this->_attr.fg = color; this->_attr.fgSet = true; }
            if (got && p == 48) { this->_attr.bg = color; this->_attr.bgSet = true; }
        }
    }
}

/* tests */
_cold int cluster::vt_test(const std::vector<std::string>& words)
{
    // vt-test <cols> <rows>: stdin (the output of a terminal) fed, the screen printed line by line
    const int cols = words.size() > 1 ? std::atoi(words[1].c_str()) : 80;
    const int rows = words.size() > 2 ? std::atoi(words[2].c_str()) : 24;
    cluster::Vt vt(cols, rows);
    const std::string bytes((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    vt.feed(bytes);
    std::cout << vt.text();
    return 0;
}
