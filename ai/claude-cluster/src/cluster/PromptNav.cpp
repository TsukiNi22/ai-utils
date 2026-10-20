/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file PromptNav.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/PromptNav.hpp"
#include <ftxui/screen/string.hpp>
#include <algorithm>

/* the characters of a row: an atom (4 bytes) is one block, a UTF-8 character its bytes */
_cold static std::size_t step_(const cluster::Editor& editor, const std::string& text, const std::size_t at, const std::size_t end)
{
    std::size_t next = editor.atomAt(text, at) ? at + 4 : at + 1;
    while (next < end && (static_cast<unsigned char>(text[next]) & 0xC0) == 0x80)
        ++next;
    return std::min(next, end);
}

_cold static int cells_(const cluster::Editor& editor, const std::string& text, const std::size_t at, const std::size_t next)
{
    const std::optional<std::size_t> atom = editor.atomAt(text, at);
    return atom ? static_cast<int>(editor.atom(*atom).label.size()) : std::max(0, ftxui::string_width(editor.shown(text.substr(at, next - at))));
}

_cold std::size_t cluster::visual_row_of(const std::vector<cluster::VisualRow>& rows, const std::size_t cursor)
{
    std::size_t current = 0;
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (cursor >= rows[i].start && (cursor < rows[i].end || (cursor == rows[i].end && rows[i].last))) current = i;
    return current;
}

_cold int cluster::visual_cells(const cluster::Editor& editor, const std::size_t from, const std::size_t to)
{
    const std::string& text = editor.text();
    int total = 0;
    for (std::size_t at = from; at < to && at < text.size();) {
        const std::size_t next = step_(editor, text, at, text.size());
        total += cells_(editor, text, at, next);
        at = next;
    }
    return total;
}

_cold std::optional<std::size_t> cluster::visual_move(const cluster::Editor& editor, const std::vector<cluster::VisualRow>& rows, const int dy, cluster::VisualGoal& goal)
{
    const std::string& text = editor.text();
    const std::size_t cursor = std::min(editor.cursor(), text.size());
    const std::size_t current = cluster::visual_row_of(rows, cursor);
    if ((dy < 0 && current == 0) || (dy > 0 && current + 1 >= rows.size())) return std::nullopt;
    if (goal.cursor != cursor) goal = {cursor, cluster::visual_cells(editor, rows[current].start, cursor)}; // a new target column
    const cluster::VisualRow& row = rows[dy < 0 ? current - 1 : current + 1];
    std::size_t at = row.start;
    int used = 0;
    while (at < row.end) {
        const std::size_t next = step_(editor, text, at, row.end);
        const int width = cells_(editor, text, at, next);
        if (used + width > goal.column) {
            if ((goal.column - used) * 2 >= width && (next < row.end || row.last)) at = next; // the half after: after the character
            break;
        }
        used += width;
        at = next;
    }
    if (!row.last && at >= row.end && row.end > row.start) { // the end of a wrapped row is the start of the next one: stay on this row
        std::size_t back = row.start;
        for (std::size_t k = row.start; k < row.end; k = step_(editor, text, k, row.end))
            back = k;
        at = back;
    }
    goal.cursor = at;
    return at;
}
