/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file LineEditor.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/LineEditor.hpp"
#include "cluster/Tools.hpp"
#include <ftxui/screen/string.hpp>
#include <functional>
#include <algorithm>
#include <chrono>
#include <map>

namespace {

enum class Act {
    None,
    Left, Right, WordLeft, WordRight,
    SelLeft, SelRight, SelWordLeft, SelWordRight,
    Home, End, SelHome, SelEnd, TextStart, TextEnd,
    Backspace, Delete, KillWordLeft, KillWordRight, KillStart, KillEnd, Yank,
    Undo, Redo, Copy, Cut, Paste,
};

_cold int width_(const std::string& text)
{
    return std::max(0, ftxui::string_width(text));
}

_cold std::int64_t now_(void)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

_cold Act action_(const ftxui::Event& event)
{
    // an event -> what an editor does with it (the sequences of the terminals that tell the modifiers apart too)
    if (event == ftxui::Event::ArrowLeft) return Act::Left;
    if (event == ftxui::Event::ArrowRight) return Act::Right;
    if (event == ftxui::Event::Home) return Act::Home;
    if (event == ftxui::Event::End) return Act::End;
    if (event == ftxui::Event::Backspace) return Act::Backspace;
    if (event == ftxui::Event::Delete) return Act::Delete;
    static const std::map<std::string, Act> table = {
        {"\x1b[1;2D", Act::SelLeft}, {"\x1b[1;2C", Act::SelRight}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x1b[1;5D", Act::WordLeft}, {"\x1b[1;5C", Act::WordRight}, {"\x1b[1;3D", Act::WordLeft}, {"\x1b[1;3C", Act::WordRight}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x1b[1;6D", Act::SelWordLeft}, {"\x1b[1;6C", Act::SelWordRight}, {"\x1b[1;4D", Act::SelWordLeft}, {"\x1b[1;4C", Act::SelWordRight}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x1b[1;2H", Act::SelHome}, {"\x1b[1;2F", Act::SelEnd}, {"\x1b[1;5H", Act::TextStart}, {"\x1b[1;5F", Act::TextEnd}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x1b[7;5~", Act::TextStart}, {"\x1b[8;5~", Act::TextEnd}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x1b[1~", Act::Home}, {"\x1b[4~", Act::End}, {"\x1b[7~", Act::Home}, {"\x1b[8~", Act::End}, {"\x1bOH", Act::Home}, {"\x1bOF", Act::End}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x1b[3;5~", Act::KillWordRight}, {"\x1b[3;3~", Act::KillWordRight}, {"\x1b[3;2~", Act::Cut}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x1b\x7f", Act::KillWordLeft}, {"\x1b[127;5u", Act::KillWordLeft}, {"\x1b[127;3u", Act::KillWordLeft}, {"\x1b[27;5;127~", Act::KillWordLeft}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x01", Act::Home}, {"\x05", Act::End}, {"\x15", Act::KillStart}, {"\x0b", Act::KillEnd}, {"\x17", Act::KillWordLeft},
        {"\x19", Act::Yank}, {"\x1a", Act::Undo}, {"\x18", Act::Cut}, {"\x16", Act::Paste},
        {"\x1b" "b", Act::WordLeft}, {"\x1b" "f", Act::WordRight}, {"\x1b" "d", Act::KillWordRight},
        {"\x1b[122;5u", Act::Undo}, {"\x1b[27;5;122~", Act::Undo}, {"\x1b[121;5u", Act::Yank}, {"\x1b[27;5;121~", Act::Yank}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x1b[122;6u", Act::Redo}, {"\x1b[90;6u", Act::Redo}, {"\x1b[27;6;122~", Act::Redo}, {"\x1b[27;6;90~", Act::Redo}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x1b[99;6u", Act::Copy}, {"\x1b[67;6u", Act::Copy}, {"\x1b[27;6;99~", Act::Copy}, {"\x1b[27;6;67~", Act::Copy}, // xstyle: ignore LU-ANSI (read, not written)
        {"\x1b[118;6u", Act::Paste}, {"\x1b[86;6u", Act::Paste}, {"\x1b[27;6;118~", Act::Paste}, {"\x1b[27;6;86~", Act::Paste}, // xstyle: ignore LU-ANSI (read, not written)
    };
    const std::map<std::string, Act>::const_iterator found = table.find(event.input());
    return found == table.end() ? Act::None : found->second;
}

_cold std::string clipboard_(const bool multiline)
{
    // the text of the clipboard (a field of one line: its line breaks are spaces); only a text type is asked (an image in it: nothing, never
    // its bytes - megabytes of binary in a field froze the interface and broke the JSON of the core), valid UTF-8, 8000 bytes at most
    cluster::Captured read = cluster::capture({"wl-paste", "--no-newline", "--type", "text"}, "", 5);
    if (read.code != 0) read = cluster::capture({"xclip", "-o", "-selection", "clipboard", "-t", "UTF8_STRING"}, "", 5);
    if (read.code != 0) return "";
    read.out = cluster::head_utf8(cluster::clean_utf8(read.out), 8000);
    std::string text;
    for (std::size_t i = 0; i < read.out.size(); ++i) {
        if (read.out[i] == '\r') {
            if (i + 1 < read.out.size() && read.out[i + 1] == '\n') ++i;
            text += multiline ? '\n' : ' ';
        } else if (read.out[i] == '\n') {
            text += multiline ? '\n' : ' ';
        } else {
            text += read.out[i];
        }
    }
    return text;
}

_cold void apply_(cluster::Editor& editor, const Act act, const bool multiline)
{
    const std::pair<std::size_t, std::size_t> sel = editor.selection();
    const bool selected = sel.first != sel.second;
    switch (act) {
        case Act::Left:
            if (selected) editor.place(sel.first, false);
            else editor.left(false);
            break;
        case Act::Right:
            if (selected) editor.place(sel.second, false);
            else editor.right(false);
            break;
        case Act::WordLeft: editor.wordLeft(false); break;
        case Act::WordRight: editor.wordRight(false); break;
        case Act::SelLeft: editor.left(true); break;
        case Act::SelRight: editor.right(true); break;
        case Act::SelWordLeft: editor.wordLeft(true); break;
        case Act::SelWordRight: editor.wordRight(true); break;
        case Act::Home: case Act::TextStart: editor.home(false); break;
        case Act::End: case Act::TextEnd: editor.end(false); break;
        case Act::SelHome: editor.home(true); break;
        case Act::SelEnd: editor.end(true); break;
        case Act::Backspace: editor.backspace(); break;
        case Act::Delete: editor.erase(); break;
        case Act::KillWordLeft: editor.killWord(false); break;
        case Act::KillWordRight: editor.killWord(true); break;
        case Act::KillStart: editor.killLine(false); break;
        case Act::KillEnd: editor.killLine(true); break;
        case Act::Yank: editor.yank(); break;
        case Act::Undo: editor.undo(); break;
        case Act::Redo: editor.redo(); break;
        case Act::Copy:
            if (selected) (void)cluster::copy_text(editor.selected());
            break;
        case Act::Cut: {
            const std::string text = editor.cut();
            if (!text.empty()) (void)cluster::copy_text(text);
            break;
        }
        case Act::Paste: {
            const std::string text = clipboard_(multiline);
            if (!text.empty()) editor.insert(text);
            break;
        }
        case Act::None:
            break;
    }
}

} // namespace

/* keys */
_cold bool cluster::editing_key(cluster::Editor& editor, const ftxui::Event& event, const bool multiline, const bool typing)
{
    const Act act = action_(event);
    if (act == Act::None) {
        if (!typing || !event.is_character()) return false;
        editor.insert(event.character()); // replaces the selection
        return true;
    }
    apply_(editor, act, multiline);
    return true;
}

_cold bool cluster::LineEditor::key(const ftxui::Event& event)
{
    const std::string& in = event.input();
    const int want = this->_want;
    this->_want = -1;
    const bool up = event == ftxui::Event::ArrowUp || in == "\x1b[1;2A"; // xstyle: ignore LU-ANSI (read, not written)
    const bool down = event == ftxui::Event::ArrowDown || in == "\x1b[1;2B"; // xstyle: ignore LU-ANSI (read, not written)
    if (up || down) {
        // several lines: a visual line; a single line (or the first / last row) keeps Up / Down for the caller
        if (!this->_multiline) return false;
        this->_want = want;
        const bool moved = this->vertical_(down, in.size() > 3);
        if (!moved) this->_want = -1;
        return moved;
    }
    if (this->_multiline && (in == "\x1b\r" || in == "\x1b\n" || in == "\x1b[13;2u" || in == "\x1b[27;2;13~")) { // Alt+Enter / Shift+Enter: a new line // xstyle: ignore LU-ANSI (read, not written)
        this->_editor.insert("\n");
        return true;
    }
    const Act act = action_(event);
    const bool rowsKnown = !this->_rows.empty() && this->_rows.back().end <= this->_editor.text().size();
    if (this->_multiline && rowsKnown && (act == Act::Home || act == Act::End || act == Act::SelHome || act == Act::SelEnd)) {
        // Home / End: the start / the end of the row
        const std::string& text = this->_editor.text();
        const Row& row = this->_rows[this->rowOf_(this->_editor.cursor())];
        const bool toEnd = act == Act::End || act == Act::SelEnd;
        const std::size_t to = !toEnd ? row.from : row.last ? row.end : std::max(row.from, cluster::cell_prev(text, row.end));
        this->_editor.place(to, act == Act::SelHome || act == Act::SelEnd);
        return true;
    }
    return cluster::editing_key(this->_editor, event, this->_multiline);
}

_cold bool cluster::LineEditor::vertical_(const bool down, const bool select)
{
    const std::string& text = this->_editor.text();
    if (this->_rows.empty() || this->_rows.back().end > text.size()) { // not drawn yet: the lines of the text
        if (down ? this->_editor.onLastLine() : this->_editor.onFirstLine()) return false;
        if (down) this->_editor.lineDown(select);
        else this->_editor.lineUp(select);
        return true;
    }
    const std::size_t current = this->rowOf_(this->_editor.cursor());
    if (down ? current + 1 >= this->_rows.size() : current == 0) return false;
    const Row& from = this->_rows[current];
    const std::size_t cursor = std::min(this->_editor.cursor(), text.size());
    const std::size_t begin = std::min(from.from, cursor);
    if (this->_want < 0) this->_want = width_(text.substr(begin, cursor - begin));
    const Row& to = this->_rows[down ? current + 1 : current - 1];
    this->_editor.place(this->byteAt_(to, this->_want), select);
    return true;
}

/* mouse */
_cold bool cluster::LineEditor::mouse(const ftxui::Mouse& mouse)
{
    if (mouse.button == ftxui::Mouse::Left && mouse.motion == ftxui::Mouse::Pressed) {
        if (this->_rows.empty() || !this->_box.Contain(mouse.x, mouse.y)) {
            this->_dragging = false;
            return false;
        }
        const std::int64_t now = now_();
        const std::size_t at = this->at_(mouse);
        const bool twice = now - this->_clickAt < 450 && mouse.x == this->_clickX && mouse.y == this->_clickY;
        this->_clickAt = twice ? 0 : now;
        this->_clickX = mouse.x;
        this->_clickY = mouse.y;
        this->_want = -1;
        if (twice) {
            this->_editor.selectWord(at); // double click: the word
            this->_dragging = false;
        } else {
            this->_editor.place(at, false);
            this->_dragging = true;
        }
        return true;
    }
    if (!this->_dragging) return false;
    if (mouse.motion == ftxui::Mouse::Moved) {
        this->_editor.place(this->at_(mouse), true); // drag: the selection
        return true;
    }
    if (mouse.motion == ftxui::Mouse::Released) {
        this->_dragging = false;
        return true;
    }
    return false;
}

_cold std::size_t cluster::LineEditor::byteAt_(const Row& row, const int column) const
{
    // the byte of the cell at a column of a row (past its end: its end)
    const std::string& text = this->_editor.text();
    std::size_t at = std::min(row.from, text.size());
    int used = 0;
    while (at < row.end && at < text.size()) {
        const std::size_t next = cluster::cell_next(text, at);
        const int cells = width_(text.substr(at, next - at));
        if (column < used + std::max(1, cells)) return at;
        used += cells;
        at = next;
    }
    if (row.last || row.end <= row.from) return std::min(row.end, text.size());
    return std::max(row.from, cluster::cell_prev(text, row.end));
}

_cold std::size_t cluster::LineEditor::at_(const ftxui::Mouse& mouse) const
{
    const std::string& text = this->_editor.text();
    if (this->_rows.empty() || this->_rows.back().end > text.size()) return this->_editor.cursor();
    const int line = std::max(0, mouse.y - this->_box.y_min);
    const std::size_t index = std::min(this->_rows.size() - 1, this->_top + static_cast<std::size_t>(line));
    const Row& row = this->_rows[index];
    return this->byteAt_(row, std::max(0, mouse.x - this->_box.x_min - row.offset));
}

/* layout */
_cold std::size_t cluster::LineEditor::rowOf_(const std::size_t at) const
{
    std::size_t found = 0;
    for (std::size_t i = 0; i < this->_rows.size(); ++i)
        if (at >= this->_rows[i].from && (at < this->_rows[i].end || (at == this->_rows[i].end && this->_rows[i].last))) found = i;
    return found;
}

_cold void cluster::LineEditor::layout_(const int width) const
{
    const std::string& text = this->_editor.text();
    this->_rows.clear();
    if (!this->_multiline) {
        // one row scrolled sideways around the cursor: a … at the side where the text goes on
        const std::size_t cursor = this->_editor.cursor();
        const int room = std::max(4, width);
        const std::function<int(const std::size_t, const std::size_t)> cells = [&](const std::size_t from, const std::size_t to) {return width_(text.substr(from, to - from));};
        if (this->_first > text.size()) this->_first = 0;
        if (cursor < this->_first) this->_first = cursor;
        const std::size_t after = cluster::cell_next(text, cursor);
        const int cursorCells = cursor < text.size() ? std::max(1, cells(cursor, after)) : 1;
        while (this->_first < cursor && (this->_first > 0 ? 1 : 0) + cells(this->_first, cursor) + cursorCells + (after < text.size() ? 1 : 0) > room)
            this->_first = cluster::cell_next(text, this->_first);
        if (cells(0, text.size()) + 1 <= room) this->_first = 0; // all of it fits (the cursor cell at its end too)
        const int left = this->_first > 0 ? 1 : 0;
        const std::function<std::size_t(const int)> fit = [&](const int available) {
            std::size_t end = this->_first;
            int used = 0;
            while (end < text.size()) {
                const std::size_t next = cluster::cell_next(text, end);
                const int size = cells(end, next);
                if (used + size > available) break;
                used += size;
                end = next;
            }
            return end;
        };
        std::size_t end = fit(room - left);
        if (end < text.size()) end = fit(room - left - 1);
        end = std::max(end, std::min(cursor, text.size()));
        this->_rows.push_back({this->_first, end, left, true});
        return;
    }
    // several lines: wrapped (at a space when one is near, never a cell cut)
    const int room = std::max(4, width);
    for (std::size_t start = 0;;) {
        const std::size_t newline = text.find('\n', start);
        const std::size_t lineEnd = newline == std::string::npos ? text.size() : newline;
        std::size_t from = start;
        do {
            std::size_t at = from;
            int used = 0;
            std::size_t space = std::string::npos;
            while (at < lineEnd) {
                const std::size_t next = cluster::cell_next(text, at);
                const int size = width_(text.substr(at, next - at));
                if (used + size > room && at > from) break;
                if (text[at] == ' ') space = next;
                used += size;
                at = next;
            }
            if (at < lineEnd && space != std::string::npos && space > from + static_cast<std::size_t>(room) / 2) at = space;
            this->_rows.push_back({from, at, 0, at >= lineEnd});
            from = at;
        } while (from < lineEnd);
        if (newline == std::string::npos) break;
        start = newline + 1;
    }
}

/* drawing */
_cold ftxui::Element cluster::LineEditor::render(const int width, const int rows, const cluster::LineColors& colors, const bool focused) const
{
    // the cursor and the selection are cells of the frame (reverse video), never the cursor of the terminal: nothing to hide, nothing flickers
    this->layout_(width);
    const std::string& text = this->_editor.text();
    const std::size_t cursor = std::min(this->_editor.cursor(), text.size());
    std::pair<std::size_t, std::size_t> sel = {cursor, cursor};
    if (focused) sel = this->_editor.selection();
    const std::size_t room = static_cast<std::size_t>(std::max(1, rows));
    const std::size_t current = this->rowOf_(cursor);
    if (this->_top > current) this->_top = current;
    else if (current >= this->_top + room) this->_top = current + 1 - room;
    if (this->_top + room > this->_rows.size()) this->_top = std::min(current, this->_rows.size() > room ? this->_rows.size() - room : 0);
    const std::size_t cursorEnd = cluster::cell_next(text, cursor);
    const std::function<ftxui::Element(const std::string&)> cursorCell = [&](const std::string& glyph) {return ftxui::text(glyph) | ftxui::color(colors.background) | ftxui::bgcolor(colors.accent) | ftxui::bold;};
    ftxui::Elements lines;
    for (std::size_t i = this->_top; i < this->_rows.size() && i < this->_top + room; ++i) {
        const Row& row = this->_rows[i];
        ftxui::Elements parts;
        if (row.offset > 0) parts.push_back(ftxui::text("…") | ftxui::color(colors.muted));
        std::vector<std::size_t> cuts = {row.from, row.end};
        for (const std::size_t cut: {sel.first, sel.second, cursor, cursorEnd})
            if (cut > row.from && cut < row.end) cuts.push_back(cut);
        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
        for (std::size_t k = 0; k + 1 < cuts.size(); ++k) {
            const std::size_t a = cuts[k];
            const std::size_t b = cuts[k + 1];
            const std::string piece = text.substr(a, b - a);
            if (focused && a == cursor && b == std::min(cursorEnd, row.end)) parts.push_back(cursorCell(piece));
            else if (sel.first != sel.second && a >= sel.first && b <= sel.second) parts.push_back(ftxui::text(piece) | ftxui::color(colors.text) | ftxui::inverted);
            else parts.push_back(ftxui::text(piece) | ftxui::color(colors.text));
        }
        if (focused && cursor == row.end && row.last) parts.push_back(cursorCell(" ")); // the end of the text: one cell
        if (!this->_multiline && row.end < text.size()) parts.push_back(ftxui::text("…") | ftxui::color(colors.muted));
        if (parts.empty()) parts.push_back(ftxui::text(""));
        lines.push_back(ftxui::hbox(std::move(parts)));
    }
    if (lines.empty()) lines.push_back(ftxui::text(""));
    return ftxui::vbox(std::move(lines)) | ftxui::xflex | ftxui::reflect(this->_box);
}

/* value */
_cold void cluster::LineEditor::set(const std::string& text)
{
    this->_editor.setText(text); // the cursor at its end
    this->_first = 0;
    this->_top = 0;
    this->_rows.clear();
    this->_want = -1;
    this->_dragging = false;
}

_cold void cluster::LineEditor::clear(void)
{
    this->_editor.clear();
    this->_first = 0;
    this->_top = 0;
    this->_rows.clear();
    this->_want = -1;
}
