/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Editor.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Editor.hpp"
#include "cluster/Tools.hpp"
#include <filesystem>
#include <algorithm>

/* paths */
_cold std::string cluster::escape_path(const std::string& path)
{
    std::string out;
    for (const char c: path) {
        if (c == ' ') out += '\\';
        out += c;
    }
    return out;
}

_cold std::string cluster::unescape_path(const std::string& path)
{
    std::string out;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '\\' && i + 1 < path.size() && path[i + 1] == ' ') continue;
        out += path[i];
    }
    return out;
}

_cold std::vector<cluster::Candidate> cluster::complete_paths(const std::string& cwd, const std::string& query, const bool foldersOnly)
{
    // query = "src/clu" -> entries of <cwd>/src starting with (then containing) "clu", folders first
    const std::size_t slash = query.rfind('/');
    const std::string dirPart = slash == std::string::npos ? "" : query.substr(0, slash + 1);
    const std::string prefix = cluster::lower(slash == std::string::npos ? query : query.substr(slash + 1));
    std::filesystem::path dir = dirPart.starts_with("~") || dirPart.starts_with("/") ? cluster::expand_home(dirPart) : std::filesystem::path(cwd) / dirPart;
    if (dirPart.empty() && (query == "~" || query.starts_with("~"))) dir = cluster::expand_home("~");
    std::vector<std::pair<int, cluster::Candidate>> found; // <rank, candidate>
    std::error_code error;

    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(dir, error)) {
        const std::string name = entry.path().filename().string();
        const std::string lowered = cluster::lower(name);
        if (name == ".git" || (name.starts_with(".") && !prefix.starts_with("."))) continue;
        const bool isDir = entry.is_directory(error);
        if (foldersOnly && !isDir) continue;
        const int rank = lowered.starts_with(prefix) ? 0 : lowered.find(prefix) != std::string::npos ? 1 : -1;
        if (rank < 0) continue;
        cluster::Candidate candidate;
        candidate.value = cluster::escape_path(dirPart + name + (isDir ? "/" : ""));
        candidate.label = name + (isDir ? "/" : "");
        candidate.directory = isDir;
        candidate.description = isDir ? "folder" : cluster::human_tokens(static_cast<std::int64_t>(entry.file_size(error))) + "B";
        found.emplace_back(rank * 2 + (isDir ? 0 : 1), candidate);
    }
    std::sort(found.begin(), found.end(), [](const std::pair<int, cluster::Candidate>& a, const std::pair<int, cluster::Candidate>& b) {
        return a.first != b.first ? a.first < b.first : a.second.label < b.second.label;
    });
    std::vector<cluster::Candidate> list;
    for (std::size_t i = 0; i < found.size() && i < 60; ++i)
        list.push_back(found[i].second);
    return list;
}

/* tools */
_cold std::size_t cluster::Editor::prev_(const std::size_t at) const
{
    std::size_t i = at;
    if (i == 0) return 0;
    --i;
    while (i > 0 && (static_cast<unsigned char>(this->_text[i]) & 0xC0) == 0x80) --i;
    return i;
}

_cold std::size_t cluster::Editor::next_(const std::size_t at) const
{
    std::size_t i = at;
    if (i >= this->_text.size()) return this->_text.size();
    ++i;
    while (i < this->_text.size() && (static_cast<unsigned char>(this->_text[i]) & 0xC0) == 0x80) ++i;
    return i;
}

_cold std::size_t cluster::Editor::wordStart_(const std::size_t at) const
{
    // start of the word ending at `at` (an escaped space "\ " is inside the word)
    std::size_t p = at;
    while (p > 0 && this->_text[p - 1] != '\n' && !(this->_text[p - 1] == ' ' && !(p >= 2 && this->_text[p - 2] == '\\')))
        --p;
    return p;
}

_cold void cluster::Editor::save_(const bool typing)
{
    if (!(typing && this->_typing)) {
        this->_undo.emplace_back(this->_text, this->_cursor);
        if (this->_undo.size() > 200) this->_undo.erase(this->_undo.begin());
    }
    this->_redo.clear();
    this->_typing = typing;
}

_cold void cluster::Editor::erase_(const std::size_t from, const std::size_t to)
{
    this->_text.erase(from, to - from);
    this->_cursor = from;
    this->_anchor.reset();
}

_cold void cluster::Editor::move_(const std::size_t to, const bool select)
{
    if (select && !this->_anchor) this->_anchor = this->_cursor;
    if (!select) this->_anchor.reset();
    this->_cursor = std::min(to, this->_text.size());
    this->_typing = false;
    this->refresh_();
}

/* editing */
_cold void cluster::Editor::insert(const std::string& text)
{
    this->save_(text.size() == 1 && text != " ");
    const std::pair<std::size_t, std::size_t> sel = this->selection();
    if (sel.first != sel.second) this->erase_(sel.first, sel.second);
    this->_text.insert(this->_cursor, text);
    this->_cursor += text.size();
    this->refresh_();
}

_cold void cluster::Editor::backspace(void)
{
    const std::pair<std::size_t, std::size_t> sel = this->selection();
    if (sel.first == sel.second && this->_cursor == 0) return;
    this->save_();
    if (sel.first != sel.second) this->erase_(sel.first, sel.second);
    else this->erase_(this->prev_(this->_cursor), this->_cursor);
    this->refresh_();
}

_cold void cluster::Editor::erase(void)
{
    const std::pair<std::size_t, std::size_t> sel = this->selection();
    if (sel.first == sel.second && this->_cursor >= this->_text.size()) return;
    this->save_();
    if (sel.first != sel.second) this->erase_(sel.first, sel.second);
    else this->erase_(this->_cursor, this->next_(this->_cursor));
    this->refresh_();
}

_cold void cluster::Editor::wordLeft(const bool select)
{
    std::size_t p = this->_cursor;
    while (p > 0 && this->_text[p - 1] == ' ') --p;
    while (p > 0 && this->_text[p - 1] != ' ') --p;
    this->move_(p, select);
}

_cold void cluster::Editor::wordRight(const bool select)
{
    std::size_t p = this->_cursor;
    while (p < this->_text.size() && this->_text[p] == ' ') ++p;
    while (p < this->_text.size() && this->_text[p] != ' ') ++p;
    this->move_(p, select);
}

_cold void cluster::Editor::selectAll(void)
{
    this->_anchor = 0;
    this->_cursor = this->_text.size();
    this->_items.clear();
}

_cold void cluster::Editor::undo(void)
{
    if (this->_undo.empty()) return;
    this->_redo.emplace_back(this->_text, this->_cursor);
    std::tie(this->_text, this->_cursor) = this->_undo.back();
    this->_undo.pop_back();
    this->_anchor.reset();
    this->_typing = false;
    this->refresh_();
}

_cold void cluster::Editor::redo(void)
{
    if (this->_redo.empty()) return;
    this->_undo.emplace_back(this->_text, this->_cursor);
    std::tie(this->_text, this->_cursor) = this->_redo.back();
    this->_redo.pop_back();
    this->_anchor.reset();
    this->_typing = false;
    this->refresh_();
}

_cold std::string cluster::Editor::cut(void)
{
    const std::pair<std::size_t, std::size_t> sel = this->selection();
    if (sel.first == sel.second) return "";
    const std::string text = this->_text.substr(sel.first, sel.second - sel.first);
    this->save_();
    this->erase_(sel.first, sel.second);
    this->refresh_();
    return text;
}

_cold void cluster::Editor::setText(const std::string& text)
{
    this->save_();
    this->_text = text;
    this->_cursor = text.size();
    this->_anchor.reset();
    this->_dismissed.clear();
    this->refresh_();
}

_cold void cluster::Editor::clear(void)
{
    if (!this->_text.empty()) this->save_();
    this->_text.clear();
    this->_cursor = 0;
    this->_anchor.reset();
    this->_images.clear();
    this->_items.clear();
    this->_dismissed.clear();
}

_cold void cluster::Editor::addImage(const std::string& path)
{
    this->_images.push_back(path);
    this->insert("[Image #" + std::to_string(this->_images.size()) + "] ");
}

/* completion */
_cold void cluster::Editor::refresh_(void)
{
    // Context of the word ending at the cursor: @file, /command (first word), argument of /cd /new /mode /restore
    const std::size_t start = this->wordStart_(this->_cursor);
    const std::string word = this->_text.substr(start, this->_cursor - start);
    const std::string before = this->_text.substr(0, start);
    const std::vector<std::string> words = cluster::split(before, ' ');
    cluster::CompletionKind kind = cluster::CompletionKind::None;
    std::string query;

    if (word.starts_with("@")) {
        kind = cluster::CompletionKind::File;
        this->_start = start + 1;
        query = cluster::unescape_path(word.substr(1));
    } else if (word.starts_with("/") && cluster::trim(before).empty()) {
        kind = cluster::CompletionKind::Command;
        this->_start = start + 1;
        query = word.substr(1);
    } else if (words.size() == 1 && !this->_anchor) {
        this->_start = start;
        query = cluster::unescape_path(word);
        if (words[0] == "/cd" || words[0] == "/new") kind = cluster::CompletionKind::Folder;
        else if (words[0] == "/mode") kind = cluster::CompletionKind::Mode;
        else if (words[0] == "/restore") kind = cluster::CompletionKind::Trash;
    }
    this->_kind = kind;
    const int previous = this->_selected;
    this->_items.clear();
    if (kind == cluster::CompletionKind::None || !this->_provider || this->_anchor || (start + word.size() == this->_cursor && word == this->_dismissed && !word.empty()))
        return;
    this->_dismissed.clear();
    this->_items = this->_provider(kind, query);
    this->_selected = std::clamp(previous, 0, std::max(0, static_cast<int>(this->_items.size()) - 1));
}

_cold void cluster::Editor::replaceWord_(const cluster::Candidate& candidate, const bool final)
{
    this->save_();
    const std::string tail = this->_text.substr(this->_cursor);
    const std::string add = candidate.value + (final && !candidate.directory ? " " : "");
    this->_text = this->_text.substr(0, this->_start) + add + tail;
    this->_cursor = this->_start + add.size();
    this->_anchor.reset();
}

_cold void cluster::Editor::tab(void)
{
    if (this->_items.empty()) return;
    this->replaceWord_(this->_items[static_cast<std::size_t>(this->_selected)], false);
    this->_selected = 0;
    this->refresh_();
}

_cold void cluster::Editor::accept(void)
{
    if (this->_items.empty()) return;
    this->replaceWord_(this->_items[static_cast<std::size_t>(this->_selected)], true);
    this->_selected = 0;
    this->dismiss();
}

_cold void cluster::Editor::space(void)
{
    if (!this->_items.empty() && this->_cursor > 0 && this->_text[this->_cursor - 1] == '\\') {
        this->insert(" "); // escaped space: part of the word, the list stays
        return;
    }
    this->dismiss();
    this->insert(" ");
}

_cold void cluster::Editor::dismiss(void)
{
    const std::size_t start = this->wordStart_(this->_cursor);
    this->_dismissed = this->_text.substr(start, this->_cursor - start);
    this->_items.clear();
}

_cold void cluster::Editor::select(const int delta)
{
    if (this->_items.empty()) return;
    const int count = static_cast<int>(this->_items.size());
    this->_selected = (this->_selected + delta + count) % count;
}

_cold void cluster::Editor::sync(const std::string& text, const std::size_t cursor)
{
    this->_text = text;
    this->_cursor = std::min(cursor, text.size());
    this->_anchor.reset();
    this->refresh_();
}

/* getter */
std::pair<std::size_t, std::size_t> cluster::Editor::selection(void) const
{
    if (!this->_anchor) return {this->_cursor, this->_cursor};
    return {std::min(*this->_anchor, this->_cursor), std::max(*this->_anchor, this->_cursor)};
}

std::string cluster::Editor::selected(void) const
{
    const std::pair<std::size_t, std::size_t> sel = this->selection();
    return this->_text.substr(sel.first, sel.second - sel.first);
}
