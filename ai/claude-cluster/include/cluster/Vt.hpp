/**************************************************************\
Edition:
##  @date 16/10/2026 by @author Tsukini

File Name:
##  @file Vt.hpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef VT_H
    #define VT_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    #define _Attribute
    #include <utils/utils.hpp> // _cold, _nodiscard
    #include <cstdint>         // std::uint32_t
    #include <string>          // std::string
    #include <vector>          // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// A cell of the terminal: its character (UTF-8, empty: the right half of a wide one), its colors (0xRRGGBB, set or the
// default), its attributes
struct VtCell {
    std::string ch = " ";
    std::uint32_t fg = 0;
    std::uint32_t bg = 0;
    bool fgSet = false;
    bool bgSet = false;
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    bool inverse = false;
};

//----------------------------------------------------------------//
/* CLASS */

// A small terminal (what the terminal interface of claude-cluster emits: SGR with 24-bit colors, the cursor moved,
// lines erased, its modes): the bytes of a pty in, a grid of cells out - the window and the web page draw it
class Vt {
    private:
        int _cols;
        int _rows;
        std::vector<std::vector<cluster::VtCell>> _grid;
        int _x = 0;
        int _y = 0;
        int _savedX = 0;
        int _savedY = 0;
        bool _cursor = true;
        bool _wrap = true;                  // autowrap (?7): a character past the last column goes to the next line
        bool _mouse = false;                // the application wants the mouse (?1000 / ?1002 / ?1006)
        bool _paste = false;                // bracketed paste (?2004)
        bool _sync = false;                 // a synchronized update being written (?2026h ... ?2026l: a whole frame)
        cluster::VtCell _attr;              // the attributes of what comes next
        std::string _escape;                // an escape sequence being read
        std::string _utf8;                  // a character being read
        std::uint64_t _changes = 0;         // grows at every change (a drawer knows when to draw again)

        // ---------- Pre-Function -------- //
        _cold void put_(const std::string& ch);             // a character at the cursor (wide: two cells)
        _cold void newline_(void);                          // down, the screen scrolled at the bottom
        _cold void csi_(const std::string& sequence);       // ESC [ ... final
        _cold void sgr_(const std::vector<int>& params);    // ESC [ ... m
        _cold void erase_(const int fromX, const int fromY, const int toX, const int toY); // cells blanked (inclusive)
        _cold void clearRow_(const int y, const int fromX, const int toX);

    public:
        // ---------- Constructor -------- //
        _cold Vt(const int cols, const int rows);

        // ---------- Pre-Function -------- //
        _cold void resize(const int cols, const int rows);  // the grid kept (cut or extended)
        _cold void feed(const std::string& bytes);          // what the pty wrote
        _cold _nodiscard std::string text(void) const;      // the screen as plain lines (tests)
        _cold _nodiscard static std::uint32_t palette(const int index); // the 256 colors (0-255) as 0xRRGGBB
        _cold _nodiscard static int width(const std::string& ch);     // 0, 1 or 2 cells (combining, plain, wide)

        // ---------- Function -------- //
        _nodiscard inline int cols(void) const                                              {return this->_cols;};
        _nodiscard inline int rows(void) const                                              {return this->_rows;};
        _nodiscard inline int cursorX(void) const                                           {return this->_x;};
        _nodiscard inline int cursorY(void) const                                           {return this->_y;};
        _nodiscard inline bool cursorShown(void) const                                      {return this->_cursor;};
        _nodiscard inline bool mouse(void) const                                            {return this->_mouse;};
        _nodiscard inline bool bracketedPaste(void) const                                   {return this->_paste;};
        _nodiscard inline bool synchronizing(void) const                                    {return this->_sync;};
        _nodiscard inline std::uint64_t changes(void) const                                 {return this->_changes;};
        _nodiscard inline const std::vector<std::vector<cluster::VtCell>>& grid(void) const {return this->_grid;};
};

} // namespace end
#endif /* VT_H */
