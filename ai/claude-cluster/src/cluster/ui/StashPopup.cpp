/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file StashPopup.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/StashPopup.hpp"
#include "cluster/Tools.hpp"
#include <algorithm>

_cold static std::string more_(const cluster::Draft& draft)
{
    const std::size_t lines = static_cast<std::size_t>(std::count(draft.text.begin(), draft.text.end(), '\n'));
    return lines == 0 ? "" : " (+" + std::to_string(lines) + " lines)";
}

/* open */
_cold void cluster::StashPopup::open(const std::string& session, const int max)
{
    this->_open = true;
    this->_ask = cluster::StashAsk::None;
    this->_session = session;
    this->_max = max;
    this->_selected = 0;
    this->_note.clear();
    this->_undo.reset();
    this->reload_();
}

_cold void cluster::StashPopup::reload_(void)
{
    this->_stash = cluster::stash_read();
    std::reverse(this->_stash.begin(), this->_stash.end());
    this->_selected = std::clamp(this->_selected, 0, std::max(0, static_cast<int>(this->_stash.size()) - 1));
}

/* keys */
_cold cluster::StashResult cluster::StashPopup::keyAsk_(const ftxui::Event& event)
{
    cluster::StashResult result;
    const bool clear = this->_ask == cluster::StashAsk::Clear;
    const int last = clear ? 1 : 2;     // clear: No, Yes - load: Replace, Append, Cancel
    if (event == ftxui::Event::Escape || (clear && event == ftxui::Event::Character('n')) || (!clear && event == ftxui::Event::Character('c'))) {
        this->_ask = cluster::StashAsk::None;
    } else if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowUp) {
        this->_choice = std::max(0, this->_choice - 1);
    } else if (event == ftxui::Event::ArrowRight || event == ftxui::Event::ArrowDown || event == ftxui::Event::Tab) {
        this->_choice = std::min(last, this->_choice + 1);
    } else if (clear && event == ftxui::Event::Character('y')) {
        this->_choice = 1;      // chosen, not done: Enter (or the second click) confirms
    } else if (!clear && event == ftxui::Event::Character('r')) {
        return this->load_(false, true);
    } else if (!clear && event == ftxui::Event::Character('a')) {
        return this->load_(true, false);
    } else if (event == ftxui::Event::Return) {
        if (clear) {
            const std::size_t count = this->_stash.size();
            this->_ask = cluster::StashAsk::None;
            if (this->_choice == 1) {
                cluster::stash_clear();
                this->_undo.reset();
                this->reload_();
                this->_note = "the stash was cleared";
                result.status = "stash cleared (" + std::to_string(count) + " prompts)";
            }
        } else if (this->_choice == 0) {
            return this->load_(false, true);
        } else if (this->_choice == 1) {
            return this->load_(true, false);
        } else {
            this->_ask = cluster::StashAsk::None;
        }
    }
    return result;
}

_cold cluster::StashResult cluster::StashPopup::load_(const bool append, const bool replace)
{
    cluster::StashResult result;
    this->_ask = cluster::StashAsk::None;
    if (this->_stash.empty()) return result;
    const std::optional<cluster::StashEntry> entry = cluster::stash_remove(this->stored_(this->_selected));
    if (!entry) {
        result.status = "that stashed prompt is gone";
        result.error = true;
        this->reload_();
        return result;
    }
    result.load = entry->draft;
    result.append = append;
    result.stashCurrent = replace;
    result.status = std::string("stashed prompt loaded") + (append ? " after the prompt" : "") + " (no longer in the stash)";
    result.close = true;
    this->close();
    return result;
}

_cold cluster::StashResult cluster::StashPopup::key(const ftxui::Event& event, const bool editorEmpty)
{
    if (this->_ask != cluster::StashAsk::None) return this->keyAsk_(event);
    cluster::StashResult result;
    const int count = static_cast<int>(this->_stash.size());
    if (event == ftxui::Event::Escape) {
        this->close();
        result.close = true;
    } else if (event == ftxui::Event::ArrowUp) {
        this->_selected = std::max(0, this->_selected - 1);
    } else if (event == ftxui::Event::ArrowDown) {
        this->_selected = std::min(std::max(0, count - 1), this->_selected + 1);
    } else if (event == ftxui::Event::Return) {
        if (count == 0) return result;
        if (editorEmpty) return this->load_(false, false);
        this->_ask = cluster::StashAsk::Load;   // the prompt being written: replace (stashed), append or cancel
        this->_choice = 0;
    } else if (event == ftxui::Event::Delete || event == ftxui::Event::Character('x')) {
        if (count == 0) return result;
        const std::size_t index = this->stored_(this->_selected);
        const std::optional<cluster::StashEntry> entry = cluster::stash_remove(index);
        if (entry) {
            this->_undo = std::make_pair(index, *entry);
            this->_note = "removed: " + cluster::stash_label(entry->draft) + "  (u: undo)";
            result.status = "stashed prompt removed (u in the list: undo)";
        }
        this->reload_();
    } else if (event == ftxui::Event::Character('u')) {
        if (!this->_undo) return result;
        cluster::stash_insert(this->_undo->first, this->_undo->second);
        this->_note = "put back: " + cluster::stash_label(this->_undo->second.draft);
        this->_undo.reset();
        this->reload_();
    } else if (event == ftxui::Event::Character('X') || event.input() == "\x1b[3;2~") { // xstyle: ignore LU-ANSI (read)
        if (count == 0) return result;
        this->_ask = cluster::StashAsk::Clear;  // two steps: the question (No by default), then Yes and Enter
        this->_choice = 0;
    }
    return result;
}

/* render */
_cold ftxui::Element cluster::StashPopup::render(const cluster::PopupColors& colors, const cluster::PopupPress& press) const
{
    ftxui::Elements items;
    const int count = static_cast<int>(this->_stash.size());
    const int start = std::max(0, this->_selected - 8);
    for (int i = start; i < count && i < start + 16; ++i) {
        const cluster::StashEntry& entry = this->_stash[static_cast<std::size_t>(i)];
        items.push_back(press(ftxui::hbox({cluster::popup_row(cluster::popup_clock(entry.time) + "  " + cluster::one_line(cluster::stash_label(entry.draft), 80), i == this->_selected, colors),
            ftxui::text(more_(entry.draft) + (entry.session.empty() ? "" : "  from " + entry.session)) | ftxui::color(colors.muted) | ftxui::dim}), ftxui::Event::Return, i));
    }
    if (items.empty()) items.push_back(ftxui::text(" the stash is empty (Ctrl+S puts the prompt aside)") | ftxui::color(colors.muted));
    ftxui::Element foot;
    if (this->_ask == cluster::StashAsk::Clear) {
        foot = ftxui::vbox({ftxui::text(" Clear " + std::to_string(count) + " stashed prompts for good?") | ftxui::bold | ftxui::color(colors.error),
            ftxui::hbox({press(cluster::popup_row("No", this->_choice == 0, colors), ftxui::Event::Character('n'), -1), ftxui::text(" "),
                press(cluster::popup_row("Yes", this->_choice == 1, colors), this->_choice == 1 ? ftxui::Event::Return : ftxui::Event::Character('y'), -1)}),
            ftxui::text(" No is the default · y (or a click) chooses Yes, Enter (or a second click) confirms · Esc: cancel") | ftxui::dim});
    } else if (this->_ask == cluster::StashAsk::Load) {
        foot = ftxui::vbox({ftxui::text(" The prompt being written is not empty:") | ftxui::bold | ftxui::color(colors.warn),
            ftxui::hbox({press(cluster::popup_row("Replace (it goes to the stash)", this->_choice == 0, colors), ftxui::Event::Character('r'), -1), ftxui::text(" "),
                press(cluster::popup_row("Append", this->_choice == 1, colors), ftxui::Event::Character('a'), -1), ftxui::text(" "),
                press(cluster::popup_row("Cancel", this->_choice == 2, colors), ftxui::Event::Character('c'), -1)}),
            ftxui::text(" ← → choose · Enter · r / a / c · Esc: cancel") | ftxui::dim});
    } else {
        foot = ftxui::vbox({this->_note.empty() ? ftxui::text("") : ftxui::text(" " + this->_note) | ftxui::color(colors.warn),
            ftxui::text(" Enter: load (removed from the stash) · Del / x: remove · u: undo · X: clear all · Esc: close") | ftxui::dim});
    }
    return ftxui::vbox({ftxui::text(" stashed prompts (" + std::to_string(count) + "/" + std::to_string(this->_max) + ") · loaded into " + this->_session) | ftxui::bold | ftxui::color(colors.accent),
        ftxui::separatorLight(), ftxui::vbox(std::move(items)), ftxui::separatorLight(), foot});
}

/* append */
_cold void cluster::append_draft(cluster::Editor& editor, const cluster::Draft& draft)
{
    std::string text = draft.text;
    std::size_t from = 0;
    for (const std::string& image: draft.images) { // the labels [Image #N] of its text, in order: new blocks of this prompt
        const std::size_t open = text.find("[Image #", from);
        const std::size_t close = open == std::string::npos ? open : text.find(']', open);
        if (close == std::string::npos) break;
        const std::string atom = editor.newImage(image);
        text.replace(open, close + 1 - open, atom);
        from = open + atom.size();
    }
    for (const std::string& paste: draft.pastes) {
        const std::size_t at = text.find(paste);
        if (at != std::string::npos) text.replace(at, paste.size(), editor.pasteAtom(paste));
    }
    editor.end(false);
    if (!editor.text().empty() && !editor.text().ends_with('\n')) editor.insert("\n");
    editor.insert(text);
}
