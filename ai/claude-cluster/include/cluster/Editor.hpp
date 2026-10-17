/**************************************************************\
Edition:
##  @date 09/10/2026 by @author Tsukini

File Name:
##  @file Editor.hpp

File Description:
##  Prompt editor of the front-ends: cursor, selection, undo /
##  redo, atoms (pasted images and long texts: one character),
##  vim mode, and the completion of @files, /commands (skills,
##  local commands) and their arguments
\**************************************************************/

#ifndef CLUSTER_EDITOR_H
    #define CLUSTER_EDITOR_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Draft
    #include <functional>           // std::function
    #include <optional>             // std::optional
    #include <cstddef>              // std::size_t
    #include <utility>              // std::pair
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class CompletionKind {
    None,
    File,       // @path: files and folders of the project of the session
    Folder,     // argument of /cd, /new
    Command,    // /command: local commands, skills, commands of the agent
    Skill,      // a /word later in the prompt: the skills and the commands of the agent (the local ones run first only)
    Mode,       // argument of /mode
    Model,      // argument of /model
    Effort,     // argument of /effort
    Trash,      // argument of /restore
    Shell,      // !command: the commands of the user's shell (its aliases, functions, builtins, programs)
    Ex,         // :command
    Setting,    // argument of :set (key=)
    Session,    // ::name: a session of claude-cluster (open or closed)
};

enum class VimMode {
    Off,        // plain editor
    Insert,
    Normal,
    Visual,     // characters
    VisualLine, // lines
};

//----------------------------------------------------------------//
/* STRUCT */

struct Candidate {
    std::string value;          // text put in place of the word (after @ or /), escaped
    std::string label;          // shown in the list
    std::string description;
    bool directory = false;     // Tab / Enter go on inside it
};

// [Image #N] / [Pasted text #N +L lines]: one character of the text (U+F0000 + index), shown as its label
struct Atom {
    std::string label;
    std::string content;        // the pasted text (empty: an image)
    std::string image;          // path of the image
};

using CompletionProvider = std::function<std::vector<cluster::Candidate>(const cluster::CompletionKind, const std::string&)>;

constexpr char32_t ATOM_BASE = 0xF0000;    // an atom: one character of the Supplementary Private Use Area-A (never typed)

_cold _nodiscard std::string atom_char(const std::size_t index);    // UTF-8 of U+F0000 + index

//----------------------------------------------------------------//
/* CLASS */

class Editor {
    private:
        std::string _text;
        std::size_t _cursor = 0;                    // byte offset
        std::optional<std::size_t> _anchor;         // selection: from the anchor to the cursor
        std::vector<std::pair<std::string, std::size_t>> _undo;
        std::vector<std::pair<std::string, std::size_t>> _redo;
        bool _typing = false;                       // consecutive characters: one undo step
        std::vector<cluster::Atom> _atoms;          // index i -> the character U+F0000 + i (kept: undo)
        std::size_t _firstAtom = 0;                 // atoms of the current prompt: from this index (clear)
        int _imageCount = 0;                        // numbers of the labels since the last clear
        int _pasteCount = 0;
        int _pasteLines = 3;                        // a paste of more lines / characters becomes an atom (0: never)
        int _pasteChars = 800;
        cluster::CompletionProvider _provider;
        cluster::CompletionKind _kind = cluster::CompletionKind::None;
        std::size_t _start = 0;                     // first byte of the completed word (after @ or /)
        std::vector<cluster::Candidate> _items;
        int _selected = 0;
        std::string _dismissed;                     // word for which the list was closed
        cluster::VimMode _vim = cluster::VimMode::Off;
        std::string _pending;                       // vim: operator / prefix waiting (d, c, y, g, r, f, t...)
        std::string _count;                         // vim: count typed before the command
        std::string _register;                      // vim: last yank / delete
        bool _linewise = false;                     // the register holds whole lines

        // ---------- Pre-Function -------- //
        _cold void save_(const bool typing = false);
        _cold void erase_(const std::size_t from, const std::size_t to);
        _cold void refresh_(void);
        _cold void replaceWord_(const cluster::Candidate& candidate, const bool final);
        _cold _nodiscard std::size_t prev_(const std::size_t at) const;    // previous UTF-8 character
        _cold _nodiscard std::size_t next_(const std::size_t at) const;
        _cold _nodiscard std::size_t wordStart_(const std::size_t at) const;
        _cold void move_(const std::size_t to, const bool select);
        _cold _nodiscard std::size_t lineStart_(const std::size_t at) const;
        _cold _nodiscard std::size_t lineEnd_(const std::size_t at) const;     // position of its \n (or the end)
        _cold _nodiscard std::optional<std::size_t> motion_(const std::string& key, const std::size_t count) const;
        _cold void operate_(const char op, std::size_t from, std::size_t to, const bool linewise);
        _cold void enterInsert_(void);
        _cold _nodiscard bool vimCommand_(const std::string& key);

    public:
        // ---------- Pre-Function -------- //
        _cold void insert(const std::string& text);
        _cold void backspace(void);
        _cold void erase(void);                     // Delete key
        _cold void wordLeft(const bool select);
        _cold void wordRight(const bool select);
        _cold void selectAll(void);
        _cold void undo(void);
        _cold void redo(void);
        _cold _nodiscard std::string cut(void);     // selected text, removed
        _cold void setText(const std::string& text);
        _cold void clear(void);
        _cold bool addImage(const std::string& path);    // [Image #N] at the cursor; false: this image is already in the prompt
        _cold void insertAtom(const std::size_t index);   // an atom of the prompt back at the cursor (chip clicked)
        _cold void setImages(const std::vector<std::string>& images);   // with setText: a queued prompt back, its labels atoms again
        _cold _nodiscard std::string newImage(const std::string& path); // the character of a new image atom
        _cold _nodiscard std::string newPaste(std::string text);        // the character of a new paste atom, or the text if short
        _cold _nodiscard std::string pasteAtom(const std::string& text); // a paste atom whatever its size (a draft back)
        _cold void load(const cluster::Draft& draft);                   // a prompt back: its images and pasted texts as blocks

        /* vim (Off: plain editor) */
        _cold void setVim(const bool on);           // on: Insert mode first (like Claude Code)
        _cold _nodiscard bool vimKey(const std::string& key);  // a key of the Normal / Visual modes, Escape of Insert; true: used

        /* completion */
        _cold void tab(void);                       // complete with the selected item, the list stays
        _cold void accept(void);                    // complete and close the list
        _cold void space(void);                     // closes the list unless the character before is a backslash
        _cold void dismiss(void);
        _cold void select(const int delta);
        _cold void sync(const std::string& text, const std::size_t cursor);  // text edited elsewhere (window): completion only

        /* text */
        _nodiscard std::pair<std::size_t, std::size_t> selection(void) const;   // empty: from == to
        _nodiscard std::string selected(void) const;
        _nodiscard std::optional<std::size_t> atomAt(const std::string& text, const std::size_t at) const; // index of the atom there
        _nodiscard std::string shown(const std::string& text) const;    // atoms -> labels (display, window)
        _nodiscard std::string atomize(const std::string& text) const;  // labels of the known atoms -> atoms
        _nodiscard std::string collapse(const std::string& text) const; // + the unchanged pasted texts -> their atom again
        _nodiscard std::vector<std::size_t> promptAtoms(void) const;    // atoms of the current prompt (chips)
        _nodiscard std::string expand(const std::string& text) const;   // to send: images -> labels, pastes -> their text
        _nodiscard std::vector<std::pair<std::string, std::string>> imagesIn(const std::string& text) const; // <label, path>
        _nodiscard std::vector<std::string> images(void) const;         // paths of the images still in the text
        _nodiscard cluster::Draft draftOf(const std::string& text) const;   // text (with atoms) -> its draft

        // ---------- Function -------- //
        _nodiscard inline const std::string& text(void) const {return this->_text;};
        _nodiscard inline std::size_t cursor(void) const {return this->_cursor;};
        _nodiscard inline bool onFirstLine(void) const {return this->lineStart_(this->_cursor) == 0;};
        _nodiscard inline bool onLastLine(void) const {return this->lineEnd_(this->_cursor) >= this->_text.size();};
        _nodiscard inline const cluster::Atom& atom(const std::size_t index) const {return this->_atoms[index];};
        _nodiscard inline std::size_t atomCount(void) const {return this->_atoms.size();};
        _nodiscard inline bool contains(const std::size_t index) const {return index < this->_atoms.size() && this->_text.find(cluster::atom_char(index)) != std::string::npos;};
        _nodiscard inline std::string plain(void) const {return this->expand(this->_text);};
        _nodiscard inline cluster::Draft draft(void) const {return this->draftOf(this->_text);};
        _nodiscard inline bool completing(void) const {return !this->_items.empty();};
        _nodiscard inline const std::vector<cluster::Candidate>& items(void) const {return this->_items;};
        _nodiscard inline int selectedItem(void) const {return this->_selected;};
        _nodiscard inline cluster::CompletionKind kind(void) const {return this->_kind;};
        _nodiscard inline std::size_t completionStart(void) const {return this->_start;};
        _nodiscard inline cluster::VimMode vim(void) const {return this->_vim;};
        _nodiscard inline const std::string& vimPending(void) const {return this->_pending;};
        _cold inline void left(const bool select) {this->move_(this->prev_(this->_cursor), select);};
        _cold inline void right(const bool select) {this->move_(this->next_(this->_cursor), select);};
        _cold inline void home(const bool select) {this->move_(0, select);};
        _cold inline void end(const bool select) {this->move_(this->_text.size(), select);};
        _cold inline void lineUp(const bool select) {this->move_(this->motion_("k", 1).value_or(this->_cursor), select);};
        _cold inline void lineDown(const bool select) {this->move_(this->motion_("j", 1).value_or(this->_cursor), select);};
        _cold inline void place(const std::size_t to, const bool select) {this->move_(std::min(to, this->_text.size()), select);}; // mouse
        _cold inline void paste(const std::string& text) {this->insert(this->newPaste(text));};          // long: [Pasted text #N +L lines]
        _cold inline void setPasteLimits(const int lines, const int chars) {this->_pasteLines = lines; this->_pasteChars = chars;};
        _cold inline void setProvider(cluster::CompletionProvider provider) {this->_provider = std::move(provider);};

        // ---------- Constructor -------- //
        Editor(void) = default;

        // ----------- Destructor --------- //
        ~Editor() = default;
};

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard std::string escape_path(const std::string& path);         // spaces -> "\ "
_cold _nodiscard std::string unescape_path(const std::string& path);
_cold _nodiscard std::vector<cluster::Candidate> complete_paths(const std::string& cwd, const std::string& query, const bool foldersOnly);

} // namespace end

#endif /* CLUSTER_EDITOR_H */
