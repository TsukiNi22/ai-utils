/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file RewindChooser.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/RewindChooser.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>

_cold bool cluster::RewindChooser::open(const std::vector<cluster::Entry>& entries)
{
    this->_points = cluster::rewind_points(entries);
    this->_open = !this->_points.empty();
    this->_selected = 0;
    return this->_open;
}

_cold cluster::RewindResult cluster::RewindChooser::key(const ftxui::Event& event)
{
    cluster::RewindResult result;
    const int cancel = static_cast<int>(this->_points.size());   // the row under the prompts
    if (event == ftxui::Event::Escape) {
        this->close();
        result.close = true;
    } else if (event == ftxui::Event::ArrowUp) {
        this->_selected = std::max(0, this->_selected - 1);
    } else if (event == ftxui::Event::ArrowDown) {
        this->_selected = std::min(cancel, this->_selected + 1);
    } else if (event == ftxui::Event::Return) {
        if (this->_selected < cancel) result.back = this->_points[static_cast<std::size_t>(this->_selected)].back;
        else result.status = "rewind cancelled";
        this->close();
        result.close = true;
    }
    return result;
}

_cold ftxui::Element cluster::RewindChooser::render(const cluster::PopupColors& colors, const cluster::PopupPress& press) const
{
    ftxui::Elements items;
    const int cancel = static_cast<int>(this->_points.size());
    const int start = std::max(0, this->_selected - 8);
    for (int i = start; i <= cancel && i < start + 16; ++i) {
        if (i == cancel) {
            items.push_back(press(cluster::popup_row("Cancel", i == this->_selected, colors), ftxui::Event::Return, i));
            continue;
        }
        const cluster::RewindPoint& point = this->_points[static_cast<std::size_t>(i)];
        const std::string hint = point.answerChars == 0 && point.tools == 0 ? "no answer"
            : cluster::popup_size(point.answerChars) + (point.tools != 0 ? ", " + std::to_string(point.tools) + " tools" : "");
        items.push_back(press(ftxui::hbox({cluster::popup_row(cluster::popup_clock(point.time) + "  " + cluster::one_line(point.label, 70), i == this->_selected, colors),
            ftxui::text(" " + hint) | ftxui::color(colors.muted) | ftxui::dim}), ftxui::Event::Return, i));
    }
    std::string drop = " nothing changes";
    if (this->_selected < cancel) {
        const cluster::RewindPoint& point = this->_points[static_cast<std::size_t>(this->_selected)];
        drop = " going back to this prompt drops " + std::to_string(point.messages) + " messages (" + std::to_string(point.prompts) + " prompts, "
            + std::to_string(point.tools) + " tool calls) from the conversation; the prompt returns to the editor, the conversation goes on from just before it. "
            "What was dropped is kept (rewind/ of the data folder)";
    }
    return ftxui::vbox({ftxui::text(" go back in the conversation: choose the point (newest first)") | ftxui::bold | ftxui::color(colors.accent), ftxui::separatorLight(),
        ftxui::vbox(std::move(items)), ftxui::separatorLight(), ftxui::paragraph(drop) | ftxui::color(this->_selected < cancel ? colors.warn : colors.muted),
        ftxui::text(" ↑ ↓ choose · Enter: go back · Esc: cancel") | ftxui::dim});
}
