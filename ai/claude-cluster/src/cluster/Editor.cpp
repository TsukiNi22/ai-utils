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
#include "cluster/Match.hpp"
#include "cluster/Tools.hpp"
#include <filesystem>
#include <functional>
#include <algorithm>
#include <cctype>
#include <regex>

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
    std::vector<std::pair<float, cluster::Candidate>> found; // <score (lower: closer, a file after a folder), candidate>
    std::error_code error;

    for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator(dir, error)) {
        const std::string name = entry.path().filename().string();
        const std::string lowered = cluster::lower(name);
        if (name == ".git" || (name.starts_with(".") && !prefix.starts_with("."))) continue;
        const bool isDir = entry.is_directory(error);
        if (foldersOnly && !isDir) continue;
        const std::optional<float> score = cluster::match_score(prefix, name); // [ui] matching
        if (!score) continue;
        cluster::Candidate candidate;
        candidate.value = cluster::escape_path(dirPart + name + (isDir ? "/" : ""));
        candidate.label = name + (isDir ? "/" : "");
        candidate.directory = isDir;
        candidate.description = isDir ? "folder" : cluster::human_tokens(static_cast<std::int64_t>(entry.file_size(error))) + "B";
        found.emplace_back(*score + (isDir ? 0.0F : 0.01F), candidate);
    }
    std::sort(found.begin(), found.end(), [](const std::pair<float, cluster::Candidate>& a, const std::pair<float, cluster::Candidate>& b) {
        return a.first != b.first ? a.first < b.first : a.second.label < b.second.label;
    });
    std::vector<cluster::Candidate> list;
    for (std::size_t i = 0; i < found.size() && i < 60; ++i)
        list.push_back(found[i].second);
    return list;
}

/* tools */
namespace {

_cold std::size_t char_size_(const std::string& text, const std::size_t at)
{
    // bytes of the UTF-8 character at `at` (1 on a broken byte)
    const unsigned char c = static_cast<unsigned char>(text[at]);
    const std::size_t size = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
    return std::min(size, text.size() - at);
}

_cold char32_t code_at_(const std::string& text, const std::size_t at)
{
    const std::size_t size = char_size_(text, at);
    const unsigned char c = static_cast<unsigned char>(text[at]);
    if (size == 1) return c;
    char32_t code = static_cast<char32_t>(c & (0xFF >> (size + 1)));
    for (std::size_t i = 1; i < size; ++i)
        code = (code << 6) | static_cast<char32_t>(static_cast<unsigned char>(text[at + i]) & 0x3F);
    return code;
}

_cold bool extends_(const char32_t c)
{
    // a character that stays in the cell of the one before: combining marks, variation selectors, skin tones, tags, joiner
    return (c >= 0x0300 && c <= 0x036F) || (c >= 0x0483 && c <= 0x0489) || (c >= 0x0591 && c <= 0x05BD) || (c >= 0x064B && c <= 0x065F)
        || (c >= 0x1AB0 && c <= 0x1AFF) || (c >= 0x1DC0 && c <= 0x1DFF) || (c >= 0x200C && c <= 0x200D) || (c >= 0x20D0 && c <= 0x20FF)
        || (c >= 0x3099 && c <= 0x309A) || (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xFE20 && c <= 0xFE2F) || (c >= 0x1F3FB && c <= 0x1F3FF)
        || (c >= 0xE0020 && c <= 0xE007F) || (c >= 0xE0100 && c <= 0xE01EF);
}

_cold std::size_t start_before_(const std::string& text, std::size_t at)
{
    // start of the UTF-8 character that ends at `at`
    if (at == 0) return 0;
    --at;
    while (at > 0 && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) --at;
    return at;
}

} // namespace

_cold std::size_t cluster::cell_prev(const std::string& text, const std::size_t at)
{
    // the previous cell: a character with the marks / the emoji sequence joined to it
    if (at == 0) return 0;
    std::size_t i = start_before_(text, std::min(at, text.size()));
    while (i > 0) {
        const std::size_t before = start_before_(text, i);
        if (!extends_(code_at_(text, i)) && code_at_(text, before) != 0x200D) break;
        i = before;
    }
    return i;
}

_cold std::size_t cluster::cell_next(const std::string& text, const std::size_t at)
{
    if (at >= text.size()) return text.size();
    std::size_t i = at + char_size_(text, at);
    char32_t last = code_at_(text, at);
    while (i < text.size() && (extends_(code_at_(text, i)) || last == 0x200D)) {
        last = code_at_(text, i);
        i += char_size_(text, i);
    }
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

_cold void cluster::Editor::killWord(const bool forward)
{
    // the selection when there is one, else the word before (Ctrl+W, Ctrl+Backspace) / after (Alt+d, Ctrl+Delete) the cursor
    if (this->selection().first == this->selection().second) {
        if (forward) this->wordRight(true);
        else this->wordLeft(true);
    }
    const std::string text = this->cut();
    if (!text.empty()) this->_killed = text;
}

_cold void cluster::Editor::killLine(const bool toEnd)
{
    // to the end (Ctrl+K) / the start (Ctrl+U) of the line; Ctrl+K at the end of a line takes its line break
    if (this->selection().first == this->selection().second) {
        if (toEnd) this->move_(this->_cursor == this->lineEnd_(this->_cursor) && this->_cursor < this->_text.size() ? this->_cursor + 1 : this->lineEnd_(this->_cursor), true);
        else this->move_(this->lineStart_(this->_cursor), true);
    }
    const std::string text = this->cut();
    if (!text.empty()) this->_killed = text;
}

_cold void cluster::Editor::selectWord(const std::size_t at)
{
    // the run of letters / digits / marks around `at` (else the run of spaces, else the character)
    const std::string& t = this->_text;
    const std::size_t pos = std::min(at, t.size());
    const std::function<int(const std::size_t)> klass = [&](const std::size_t i) {
        const char c = t[i];
        if (c == ' ' || c == '\t' || c == '\n') return 0;
        return static_cast<unsigned char>(c) >= 0x80 || std::isalnum(static_cast<unsigned char>(c)) || c == '_' ? 1 : 2;
    };
    if (t.empty()) return;
    std::size_t from = pos >= t.size() ? start_before_(t, t.size()) : pos;
    const int k = klass(from);
    std::size_t to = from;
    while (from > 0 && klass(start_before_(t, from)) == k) from = start_before_(t, from);
    while (to < t.size() && klass(to) == k) to += char_size_(t, to);
    this->_anchor = from;
    this->_cursor = to;
    this->_typing = false;
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
    // a text put here (history, stash, a queued prompt...): no completion opened by it (the arrows go on browsing),
    // it opens again when the word is changed
    this->save_();
    this->_text = text;
    this->_cursor = text.size();
    this->_anchor.reset();
    this->dismiss();
}

_cold void cluster::Editor::clear(void)
{
    if (!this->_text.empty()) this->save_();
    this->_text.clear();
    this->_cursor = 0;
    this->_anchor.reset();
    this->_firstAtom = this->_atoms.size();
    this->_imageCount = 0;
    this->_pasteCount = 0;
    this->_items.clear();
    this->_dismissed.clear();
}

/* atoms */
_cold std::string cluster::atom_char(const std::size_t index)
{
    const char32_t c = cluster::ATOM_BASE + static_cast<char32_t>(index);
    return {static_cast<char>(0xF0 | (c >> 18)), static_cast<char>(0x80 | ((c >> 12) & 0x3F)),
        static_cast<char>(0x80 | ((c >> 6) & 0x3F)), static_cast<char>(0x80 | (c & 0x3F))};
}

_cold std::string cluster::Editor::newImage(const std::string& path)
{
    // the same image (same file: named by its content) pasted again in the prompt: the same atom
    for (std::size_t i = this->_firstAtom; i < this->_atoms.size(); ++i)
        if (this->_atoms[i].image == path) return cluster::atom_char(i);
    this->_atoms.push_back({"[Image #" + std::to_string(++this->_imageCount) + "]", "", path});
    return cluster::atom_char(this->_atoms.size() - 1);
}

_cold std::string cluster::Editor::newPaste(std::string text)
{
    text = std::regex_replace(text, std::regex("\r\n?"), "\n");
    const int lines = static_cast<int>(std::count(text.begin(), text.end(), '\n')) + 1;
    const bool lineLimit = this->_pasteLines > 0 && lines > this->_pasteLines;
    const bool charLimit = this->_pasteChars > 0 && static_cast<int>(text.size()) > this->_pasteChars;
    if (!lineLimit && !charLimit) return text;
    this->_atoms.push_back({"[Pasted text #" + std::to_string(++this->_pasteCount) + (lines > 1 ? " +" + std::to_string(lines) + " lines]"
        : " " + std::to_string(text.size()) + " chars]"), text, ""});
    return cluster::atom_char(this->_atoms.size() - 1);
}

_cold bool cluster::Editor::addImage(const std::string& path)
{
    // the same image again: one more reference to it ("here, here and here"), one image in memory and in the message
    const std::string atom = this->newImage(path);
    const bool known = this->_text.find(atom) != std::string::npos;
    this->insert(atom + " ");
    return !known;
}

_cold void cluster::Editor::insertAtom(const std::size_t index)
{
    if (index >= this->_atoms.size()) return;
    this->insert(cluster::atom_char(index) + (this->_atoms[index].image.empty() ? "" : " "));
}

std::vector<std::size_t> cluster::Editor::promptAtoms(void) const
{
    std::vector<std::size_t> list;
    for (std::size_t i = 0; i < this->_atoms.size(); ++i)
        if (i >= this->_firstAtom || this->contains(i)) list.push_back(i);
    return list;
}

std::string cluster::Editor::collapse(const std::string& text) const
{
    // after an edit outside (Ctrl+E): a pasted text found unchanged is its block again
    std::string out = text;
    for (std::size_t index = this->_atoms.size(); index-- > 0;) {
        const std::string& content = this->_atoms[index].content;
        if (content.empty()) continue;
        for (std::size_t at = out.find(content); at != std::string::npos; at = out.find(content, at + 4))
            out.replace(at, content.size(), cluster::atom_char(index));
    }
    return this->atomize(out);
}

_cold std::string cluster::Editor::pasteAtom(const std::string& text)
{
    const int lines = static_cast<int>(std::count(text.begin(), text.end(), '\n')) + 1;
    this->_atoms.push_back({"[Pasted text #" + std::to_string(++this->_pasteCount) + (lines > 1 ? " +" + std::to_string(lines) + " lines]"
        : " " + std::to_string(text.size()) + " chars]"), text, ""});
    return cluster::atom_char(this->_atoms.size() - 1);
}

cluster::Draft cluster::Editor::draftOf(const std::string& text) const
{
    cluster::Draft draft{this->expand(text), {}, {}};
    for (const auto &[label, path]: this->imagesIn(text))
        draft.images.push_back(path);
    for (std::size_t i = 0; i < text.size(); ++i) {
        const std::optional<std::size_t> atom = this->atomAt(text, i);
        if (!atom || this->_atoms[*atom].image.size() || this->_atoms[*atom].content.empty()) continue;
        if (std::find(draft.pastes.begin(), draft.pastes.end(), this->_atoms[*atom].content) == draft.pastes.end()) draft.pastes.push_back(this->_atoms[*atom].content);
    }
    return draft;
}

_cold void cluster::Editor::load(const cluster::Draft& draft)
{
    // its [Image #N] and its pasted texts are blocks again (images: one per path, in the order of the text)
    this->setText(draft.text);
    this->setImages(draft.images);
    for (const std::string& paste: draft.pastes) {
        const std::size_t at = this->_text.find(paste);
        if (at != std::string::npos) this->_text.replace(at, paste.size(), this->pasteAtom(paste));
    }
    this->_cursor = this->_text.size();
    this->dismiss();
}

_cold void cluster::Editor::setImages(const std::vector<std::string>& images)
{
    // a queued prompt back: "[Image #N]" of its text -> atoms again
    this->_imageCount = 0;
    for (const std::string& path: images)
        (void)this->newImage(path);
    this->_text = this->atomize(this->_text);
    this->_cursor = this->_text.size();
}

std::optional<std::size_t> cluster::Editor::atomAt(const std::string& text, const std::size_t at) const
{
    if (at + 4 > text.size() || static_cast<unsigned char>(text[at]) != 0xF3) return std::nullopt;
    const char32_t c = (static_cast<char32_t>(static_cast<unsigned char>(text[at]) & 0x07) << 18) | (static_cast<char32_t>(static_cast<unsigned char>(text[at + 1]) & 0x3F) << 12)
        | (static_cast<char32_t>(static_cast<unsigned char>(text[at + 2]) & 0x3F) << 6) | static_cast<char32_t>(static_cast<unsigned char>(text[at + 3]) & 0x3F);
    if (c < cluster::ATOM_BASE || c - cluster::ATOM_BASE >= this->_atoms.size()) return std::nullopt;
    return static_cast<std::size_t>(c - cluster::ATOM_BASE);
}

std::string cluster::Editor::shown(const std::string& text) const
{
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        const std::optional<std::size_t> atom = this->atomAt(text, i);
        if (atom) {
            out += this->_atoms[*atom].label;
            i += 4;
        } else {
            out += text[i++];
        }
    }
    return out;
}

std::string cluster::Editor::atomize(const std::string& text) const
{
    std::string out = text;
    for (std::size_t index = this->_atoms.size(); index-- > 0;) {
        const std::string& label = this->_atoms[index].label;
        for (std::size_t at = out.find(label); at != std::string::npos; at = out.find(label, at + 4))
            out.replace(at, label.size(), cluster::atom_char(index));
    }
    return out;
}

std::string cluster::Editor::expand(const std::string& text) const
{
    // the images numbered in the order of the text, as imagesIn() lists them: [Image #1] is the first image sent
    std::vector<std::size_t> order;
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        const std::optional<std::size_t> atom = this->atomAt(text, i);
        if (atom) {
            const cluster::Atom& a = this->_atoms[*atom];
            if (a.image.empty()) {
                out += a.content;
            } else {
                auto it = std::find(order.begin(), order.end(), *atom);
                if (it == order.end()) it = order.insert(order.end(), *atom);
                out += "[Image #" + std::to_string(it - order.begin() + 1) + "]";
            }
            i += 4;
        } else {
            out += text[i++];
        }
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> cluster::Editor::imagesIn(const std::string& text) const
{
    // each image once, in the order of the text
    std::vector<std::pair<std::string, std::string>> list;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const std::optional<std::size_t> atom = this->atomAt(text, i);
        if (!atom || this->_atoms[*atom].image.empty()) continue;
        const std::pair<std::string, std::string> item = {this->_atoms[*atom].label, this->_atoms[*atom].image};
        if (std::find(list.begin(), list.end(), item) == list.end()) list.push_back(item);
    }
    return list;
}

std::vector<std::string> cluster::Editor::images(void) const
{
    std::vector<std::string> paths;
    for (const auto &[label, path]: this->imagesIn(this->_text))
        paths.push_back(path);
    return paths;
}

/* completion */
_cold void cluster::Editor::refresh_(void)
{
    // Context of the word ending at the cursor: @file, /command (first word), argument of /cd /new /mode /restore
    if (this->_vim != cluster::VimMode::Off && this->_vim != cluster::VimMode::Insert) {
        this->_items.clear(); // vim Normal / Visual: no completion
        return;
    }
    const std::size_t start = this->wordStart_(this->_cursor);
    const std::string word = this->_text.substr(start, this->_cursor - start);
    const std::string before = this->_text.substr(0, start);
    const std::vector<std::string> words = cluster::split(before, ' ');
    cluster::CompletionKind kind = cluster::CompletionKind::None;
    std::string query;

    if (word.starts_with("::")) {
        kind = cluster::CompletionKind::Session;
        this->_start = start + 2;
        query = word.substr(2);
    } else if (word.starts_with("@")) {
        kind = cluster::CompletionKind::File;
        this->_start = start + 1;
        query = cluster::unescape_path(word.substr(1));
    } else if (word.starts_with("/")) {
        // first: every command; later in the prompt: the skills and the commands of the agent
        kind = cluster::trim(before).empty() ? cluster::CompletionKind::Command : cluster::CompletionKind::Skill;
        this->_start = start + 1;
        query = word.substr(1);
    } else if ((this->_text.starts_with("!") || (this->_text.starts_with(":") && !this->_text.starts_with("::"))) && !this->_anchor) {
        // the modes of the prompt: !<command of the shell> <files>; :<command> <its argument>
        const bool shell = this->_text.starts_with("!");
        const std::string head = before.empty() ? "" : cluster::trim(before.substr(1));
        if (start == 0) {
            kind = shell ? cluster::CompletionKind::Shell : cluster::CompletionKind::Ex;
            this->_start = 1;
            query = word.substr(1);
        } else if (shell && head.empty()) {
            kind = cluster::CompletionKind::Shell;
            this->_start = start;
            query = word;
        } else {
            const std::string command = cluster::split(head, ' ')[0];
            this->_start = start;
            query = cluster::unescape_path(word);
            if (shell || command == "r" || command == "w" || command == "read" || command == "write") kind = cluster::CompletionKind::File;
            else if (command == "e" || command == "cd" || command == "new") kind = cluster::CompletionKind::Folder;
            else if (command == "set" || command == "se") kind = cluster::CompletionKind::Setting;
            else if (command == "mode") kind = cluster::CompletionKind::Mode;
            else if (command == "model") kind = cluster::CompletionKind::Model;
            else if (command == "effort") kind = cluster::CompletionKind::Effort;
        }
    } else if (words.size() == 1 && !this->_anchor) {
        this->_start = start;
        query = cluster::unescape_path(word);
        if (words[0] == "/cd" || words[0] == "/new") kind = cluster::CompletionKind::Folder;
        else if (words[0] == "/mode") kind = cluster::CompletionKind::Mode;
        else if (words[0] == "/model") kind = cluster::CompletionKind::Model;
        else if (words[0] == "/effort") kind = cluster::CompletionKind::Effort;
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
    if (this->_vim == cluster::VimMode::Visual || this->_vim == cluster::VimMode::VisualLine) {
        if (this->_anchor) this->_anchor = std::min(*this->_anchor, text.size());
    } else {
        this->_anchor.reset();
    }
    this->refresh_();
}

/* vim */
_cold std::size_t cluster::Editor::lineStart_(const std::size_t at) const
{
    const std::size_t nl = at == 0 ? std::string::npos : this->_text.rfind('\n', at - 1);
    return nl == std::string::npos ? 0 : nl + 1;
}

_cold std::size_t cluster::Editor::lineEnd_(const std::size_t at) const
{
    const std::size_t nl = this->_text.find('\n', at);
    return nl == std::string::npos ? this->_text.size() : nl;
}

_cold void cluster::Editor::setVim(const bool on)
{
    if (on == (this->_vim != cluster::VimMode::Off)) return;
    this->_vim = on ? cluster::VimMode::Insert : cluster::VimMode::Off;
    this->_pending.clear();
    this->_count.clear();
    this->_anchor.reset();
}

_cold void cluster::Editor::enterInsert_(void)
{
    this->_vim = cluster::VimMode::Insert;
    this->_anchor.reset();
    this->_typing = false;
}

_cold std::optional<std::size_t> cluster::Editor::motion_(const std::string& key, const std::size_t count) const
{
    // target of a motion from the cursor (count times); nullopt: not a motion
    const std::function<bool(char)> word = [](char c) {return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || (static_cast<unsigned char>(c) & 0x80);};
    const std::string& t = this->_text;
    std::size_t p = this->_cursor;
    for (std::size_t n = 0; n < std::max<std::size_t>(count, 1); ++n) {
        if (key == "h") p = p > this->lineStart_(p) ? this->prev_(p) : p;
        else if (key == "l") p = p < this->lineEnd_(p) ? this->next_(p) : p;
        else if (key == "0") p = this->lineStart_(p);
        else if (key == "$") p = this->lineEnd_(p);
        else if (key == "^") {
            p = this->lineStart_(p);
            while (p < this->lineEnd_(p) && (t[p] == ' ' || t[p] == '\t')) ++p;
        } else if (key == "j" || key == "k") {
            const std::size_t column = p - this->lineStart_(p);
            if (key == "j") {
                const std::size_t end = this->lineEnd_(p);
                if (end >= t.size()) return p;
                p = std::min(end + 1 + column, this->lineEnd_(end + 1));
            } else {
                const std::size_t start = this->lineStart_(p);
                if (start == 0) return p;
                const std::size_t previous = this->lineStart_(start - 1);
                p = std::min(previous + column, start - 1);
            }
        } else if (key == "w") {
            if (p < t.size() && word(t[p])) while (p < t.size() && word(t[p])) ++p;
            else if (p < t.size() && t[p] != ' ' && t[p] != '\n') while (p < t.size() && !word(t[p]) && t[p] != ' ' && t[p] != '\n') ++p;
            while (p < t.size() && (t[p] == ' ' || t[p] == '\n')) ++p;
        } else if (key == "b") {
            while (p > 0 && (t[p - 1] == ' ' || t[p - 1] == '\n')) --p;
            if (p > 0 && word(t[p - 1])) while (p > 0 && word(t[p - 1])) --p;
            else while (p > 0 && !word(t[p - 1]) && t[p - 1] != ' ' && t[p - 1] != '\n') --p;
        } else if (key == "e") {
            if (p < t.size()) p = this->next_(p);
            while (p < t.size() && (t[p] == ' ' || t[p] == '\n')) ++p;
            if (p < t.size() && word(t[p])) while (p + 1 < t.size() && word(t[p + 1])) ++p;
            else while (p + 1 < t.size() && !word(t[p + 1]) && t[p + 1] != ' ' && t[p + 1] != '\n') ++p;
        } else if (key == "G") p = this->lineStart_(t.size());
        else if (key == "gg") p = 0;
        else if (key.size() == 2 && (key[0] == 'f' || key[0] == 't')) {
            const std::size_t found = t.find(key[1], p + 1);
            if (found == std::string::npos || found > this->lineEnd_(p)) return p;
            p = key[0] == 'f' ? found : found - 1;
        } else if (key.size() == 2 && (key[0] == 'F' || key[0] == 'T')) {
            if (p == 0) return p;
            const std::size_t found = t.rfind(key[1], p - 1);
            if (found == std::string::npos || found < this->lineStart_(p)) return p;
            p = key[0] == 'F' ? found : found + 1;
        } else return std::nullopt;
    }
    return p;
}

_cold void cluster::Editor::operate_(const char op, std::size_t from, std::size_t to, const bool linewise)
{
    // d (delete), c (change: delete + Insert), y (yank) on [from, to)
    if (from > to) std::swap(from, to);
    to = std::min(to, this->_text.size());
    this->_register = this->_text.substr(from, to - from);
    this->_linewise = linewise;
    if (op == 'y') {
        this->_cursor = from;
        return;
    }
    this->save_();
    this->_text.erase(from, to - from);
    this->_cursor = std::min(from, this->_text.size());
    if (op == 'c') this->enterInsert_();
}

_cold bool cluster::Editor::vimKey(const std::string& key)
{
    const bool used = this->vimCommand_(key);
    // Normal / Visual: the cursor stays on a character (not after the end of the line)
    if (this->_vim != cluster::VimMode::Off && this->_vim != cluster::VimMode::Insert && this->_cursor > this->lineStart_(this->_cursor)
        && this->_cursor >= this->lineEnd_(this->_cursor))
        this->_cursor = this->prev_(this->lineEnd_(this->_cursor));
    return used;
}

_cold bool cluster::Editor::vimCommand_(const std::string& key)
{
    if (this->_vim == cluster::VimMode::Off) return false;
    if (this->_vim == cluster::VimMode::Insert) {
        if (key != "Escape") return false;
        this->_vim = cluster::VimMode::Normal; // like vim: the cursor goes back on the last character typed
        if (this->_cursor > this->lineStart_(this->_cursor)) this->_cursor = this->prev_(this->_cursor);
        this->_items.clear();
        return true;
    }
    const bool visual = this->_vim == cluster::VimMode::Visual || this->_vim == cluster::VimMode::VisualLine;
    if (key == "Escape") {
        const bool had = !this->_pending.empty() || !this->_count.empty() || visual;
        this->_pending.clear();
        this->_count.clear();
        if (visual) {
            this->_vim = cluster::VimMode::Normal;
            this->_anchor.reset();
        }
        return had; // nothing waiting: the front-end handles Esc (queued prompt, Esc Esc)
    }
    // count: 3w, 2dd (0 alone is the start of the line)
    if (key.size() == 1 && std::isdigit(static_cast<unsigned char>(key[0])) && (key != "0" || !this->_count.empty()) && this->_pending.empty()) {
        this->_count += key;
        return true;
    }
    const std::size_t count = this->_count.empty() ? 1 : static_cast<std::size_t>(std::stoul(this->_count));

    // prefixes waiting for one more key: r<c>, f<c> / t<c> (also after an operator), g<g>
    if (!this->_pending.empty()) {
        const std::string pending = this->_pending;
        const char op = pending[0] == 'd' || pending[0] == 'c' || pending[0] == 'y' ? pending[0] : 0;
        const std::string sub = op ? pending.substr(1) : pending;
        this->_pending.clear();
        if (sub == "r") {
            if (this->_cursor < this->lineEnd_(this->_cursor) && key.size() >= 1) {
                this->save_();
                this->_text.replace(this->_cursor, this->next_(this->_cursor) - this->_cursor, key);
            }
            this->_count.clear();
            return true;
        }
        std::string motion = sub + key;
        if (op && sub.empty() && key.size() == 1 && key[0] == op) { // dd cc yy: whole lines
            std::size_t from = this->lineStart_(this->_cursor);
            std::size_t to = this->lineEnd_(this->_cursor);
            for (std::size_t n = 1; n < count && to < this->_text.size(); ++n)
                to = this->lineEnd_(to + 1);
            if (op != 'c') {
                if (to < this->_text.size()) ++to;
                else if (from > 0) --from;
            }
            this->operate_(op, from, to, true);
            this->_count.clear();
            return true;
        }
        if (op && sub.empty() && (key == "f" || key == "t" || key == "F" || key == "T" || key == "g")) {
            this->_pending = pending + key;
            return true;
        }
        if (sub == "g" && key != "g") {
            this->_count.clear();
            return true;
        }
        if (op && motion == "w" && op == 'c') motion = "e"; // cw changes to the end of the word
        const std::optional<std::size_t> target = this->motion_(motion, count);
        this->_count.clear();
        if (!target) return true;
        if (!op) {
            this->_cursor = *target;
            return true;
        }
        const bool inclusive = motion == "e" || motion == "$" || motion[0] == 'f' || motion[0] == 't';
        const std::size_t to = inclusive && *target >= this->_cursor ? this->next_(*target) : *target;
        this->operate_(op, this->_cursor, motion == "$" ? this->lineEnd_(this->_cursor) : to, false);
        return true;
    }

    // visual: the selection follows the cursor, d / x / c / y / ~ act on it
    if (visual) {
        const std::size_t anchor = this->_anchor.value_or(this->_cursor);
        std::size_t from = std::min(anchor, this->_cursor);
        std::size_t to = this->next_(std::max(anchor, this->_cursor));
        if (this->_vim == cluster::VimMode::VisualLine) {
            from = this->lineStart_(from);
            to = std::min(this->lineEnd_(std::max(anchor, this->_cursor)) + 1, this->_text.size());
        }
        if (key == "d" || key == "x" || key == "c" || key == "y") {
            this->_vim = cluster::VimMode::Normal;
            this->_anchor.reset();
            this->operate_(key == "x" ? 'd' : key[0], from, to, false);
            return true;
        }
        if (key == "~") {
            this->save_();
            for (std::size_t i = from; i < to; ++i)
                this->_text[i] = std::isupper(static_cast<unsigned char>(this->_text[i])) ? static_cast<char>(std::tolower(static_cast<unsigned char>(this->_text[i])))
                    : static_cast<char>(std::toupper(static_cast<unsigned char>(this->_text[i])));
            this->_vim = cluster::VimMode::Normal;
            this->_anchor.reset();
            return true;
        }
    }

    // Normal mode
    if (key == "i" || key == "a" || key == "I" || key == "A" || key == "o" || key == "O") {
        if (key == "a" && this->_cursor < this->lineEnd_(this->_cursor)) this->_cursor = this->next_(this->_cursor);
        else if (key == "I") this->_cursor = *this->motion_("^", 1);
        else if (key == "A") this->_cursor = this->lineEnd_(this->_cursor);
        else if (key == "o" || key == "O") {
            this->save_();
            const std::size_t at = key == "o" ? this->lineEnd_(this->_cursor) : this->lineStart_(this->_cursor);
            this->_text.insert(at, "\n");
            this->_cursor = key == "o" ? at + 1 : at;
        }
        this->enterInsert_();
    } else if (key == "v" || key == "V") {
        this->_vim = key == "v" ? cluster::VimMode::Visual : cluster::VimMode::VisualLine;
        this->_anchor = this->_cursor;
    } else if (key == "x" || key == "X") {
        if (key == "x" && this->_cursor < this->lineEnd_(this->_cursor)) {
            std::size_t to = this->_cursor;
            for (std::size_t n = 0; n < count && to < this->lineEnd_(this->_cursor); ++n)
                to = this->next_(to);
            this->operate_('d', this->_cursor, to, false);
        } else if (key == "X" && this->_cursor > this->lineStart_(this->_cursor)) {
            this->operate_('d', this->prev_(this->_cursor), this->_cursor, false);
        }
    } else if (key == "s") {
        this->operate_('c', this->_cursor, this->next_(this->_cursor), false);
    } else if (key == "S") {
        this->operate_('c', this->lineStart_(this->_cursor), this->lineEnd_(this->_cursor), false);
    } else if (key == "D" || key == "C") {
        this->operate_(key == "D" ? 'd' : 'c', this->_cursor, this->lineEnd_(this->_cursor), false);
    } else if (key == "d" || key == "c" || key == "y" || key == "r" || key == "f" || key == "t" || key == "F" || key == "T" || key == "g") {
        this->_pending = key;
        return true;
    } else if (key == "p" || key == "P") {
        if (this->_register.empty()) return true;
        this->save_();
        std::size_t at = this->_cursor;
        if (this->_linewise) at = key == "p" ? std::min(this->lineEnd_(this->_cursor) + 1, this->_text.size()) : this->lineStart_(this->_cursor);
        else if (key == "p" && at < this->_text.size()) at = this->next_(at);
        std::string text = this->_register;
        if (this->_linewise && at == this->_text.size() && !this->_text.empty() && this->_text.back() != '\n') text = "\n" + text;
        this->_text.insert(at, text);
        this->_cursor = at;
    } else if (key == "u") {
        this->undo();
    } else if (key == "Ctrl+R") {
        this->redo();
    } else if (key == "~") {
        if (this->_cursor < this->_text.size()) {
            this->save_();
            char& c = this->_text[this->_cursor];
            c = std::isupper(static_cast<unsigned char>(c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            this->_cursor = std::min(this->next_(this->_cursor), this->lineEnd_(this->_cursor));
        }
    } else if (key == "J") {
        const std::size_t end = this->lineEnd_(this->_cursor);
        if (end < this->_text.size()) {
            this->save_();
            this->_text.replace(end, 1, " ");
            this->_cursor = end;
        }
    } else if (const std::optional<std::size_t> target = this->motion_(key, count)) {
        this->_cursor = *target;
    }
    this->_count.clear();
    this->_items.clear();
    return true; // Normal / Visual: every key is a command
}

/* getter */
std::pair<std::size_t, std::size_t> cluster::Editor::selection(void) const
{
    if (!this->_anchor) return {this->_cursor, this->_cursor};
    if (this->_vim == cluster::VimMode::Visual) // vim: the character under the cursor is in
        return {std::min(*this->_anchor, this->_cursor), std::min(this->next_(std::max(*this->_anchor, this->_cursor)), this->_text.size())};
    if (this->_vim == cluster::VimMode::VisualLine)
        return {this->lineStart_(std::min(*this->_anchor, this->_cursor)), std::min(this->lineEnd_(std::max(*this->_anchor, this->_cursor)) + 1, this->_text.size())};
    return {std::min(*this->_anchor, this->_cursor), std::max(*this->_anchor, this->_cursor)};
}

std::string cluster::Editor::selected(void) const
{
    const std::pair<std::size_t, std::size_t> sel = this->selection();
    return this->_text.substr(sel.first, sel.second - sel.first);
}
