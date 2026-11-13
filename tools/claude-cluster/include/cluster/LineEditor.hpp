/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file LineEditor.hpp

File Description:
##  A text field of the terminal front-end edited like a real
##  line editor: the cluster::Editor of the prompt (cursor,
##  selection, undo, kill ring, cells) with the keys of an
##  editor, the mouse (click, drag, double click) and its
##  drawing (cursor and selection in the frame, long line
##  scrolled sideways, several lines wrapped and scrolled)
\**************************************************************/

#ifndef CLUSTER_LINEEDITOR_H
    #define CLUSTER_LINEEDITOR_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard
    #include "Editor.hpp"                   // cluster::Editor
    #include <ftxui/component/event.hpp>    // ftxui::Event
    #include <ftxui/component/mouse.hpp>    // ftxui::Mouse
    #include <ftxui/dom/elements.hpp>       // ftxui::Element
    #include <ftxui/screen/color.hpp>       // ftxui::Color
    #include <ftxui/screen/box.hpp>         // ftxui::Box
    #include <cstdint>                      // std::int64_t
    #include <cstddef>                      // std::size_t
    #include <string>                       // std::string
    #include <vector>                       // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct LineColors {
    ftxui::Color text = ftxui::Color::Default;      // the text
    ftxui::Color accent = ftxui::Color::Default;    // the cursor cell (its background)
    ftxui::Color background = ftxui::Color::Default;// the text of the cursor cell
    ftxui::Color muted = ftxui::Color::Default;     // the … marks of a long line
};

//----------------------------------------------------------------//
/* FUNCTION */

// The editing keys, on any cluster::Editor (the prompt too): moves, deletes, kill ring, undo / redo, copy / cut / paste, replace-on-type.
// true: the event was one of them (and was applied). multiline: a paste keeps its line breaks, else it is one line.
// typing: the printable characters are inserted too (false: only the other keys, the prompt does its own characters)
_cold bool editing_key(cluster::Editor& editor, const ftxui::Event& event, const bool multiline, const bool typing = true);

//----------------------------------------------------------------//
/* CLASS */

class LineEditor {
    private:
        struct Row {
            std::size_t from = 0;       // first byte shown
            std::size_t end = 0;        // after the last byte shown
            int offset = 0;             // cells before the text (the … of a long line)
            bool last = false;          // the last row of its line (the cursor at its end is on it)
        };
        cluster::Editor _editor;
        bool _multiline = false;
        mutable std::size_t _first = 0;             // single line: first byte shown (scrolled sideways)
        mutable std::size_t _top = 0;               // several lines: first row shown
        mutable std::vector<Row> _rows;             // the rows of the last drawing
        mutable ftxui::Box _box{-1, -1, -1, -1};    // where it was drawn (the mouse)
        int _want = -1;                             // Up / Down: the column wanted
        bool _dragging = false;
        std::int64_t _clickAt = 0;                  // last press (a double click)
        int _clickX = -1;
        int _clickY = -1;

        // ---------- Pre-Function -------- //
        _cold void layout_(const int width) const;
        _cold _nodiscard std::size_t rowOf_(const std::size_t at) const;
        _cold _nodiscard std::size_t byteAt_(const Row& row, const int column) const;
        _cold _nodiscard std::size_t at_(const ftxui::Mouse& mouse) const;
        _cold _nodiscard bool vertical_(const bool down, const bool select);

    public:
        // ---------- Pre-Function -------- //
        _cold bool key(const ftxui::Event& event);          // true: used (an Up / Down on the first / last row is not)
        _cold bool mouse(const ftxui::Mouse& mouse);        // true: used (a press in the field, a drag started in it)
        _cold _nodiscard ftxui::Element render(const int width, const int rows, const cluster::LineColors& colors, const bool focused = true) const;
        _cold void set(const std::string& text);            // a new value, the cursor at its end
        _cold void clear(void);

        // ---------- Function -------- //
        _nodiscard inline const std::string& text(void) const {return this->_editor.text();};
        _nodiscard inline std::size_t cursor(void) const      {return this->_editor.cursor();};
        _nodiscard inline cluster::Editor& editor(void)       {return this->_editor;};
        _nodiscard inline bool multiline(void) const          {return this->_multiline;};
        _cold inline void insert(const std::string& text)     {this->_editor.insert(text);};
        _cold inline void setMultiline(const bool on)         {this->_multiline = on;};

        // ---------- Constructor -------- //
        LineEditor(void) = default;
        explicit LineEditor(const bool multiline): _multiline(multiline) {};

        // ----------- Destructor --------- //
        ~LineEditor() = default;
};

} // namespace end

#endif /* CLUSTER_LINEEDITOR_H */
