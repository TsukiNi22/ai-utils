/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file Display.hpp

File Description:
##  Rich displays put in a transcript without a prompt (the global
##  session, MCP session_display): markdown, tables, charts, images,
##  diagrams, drawings (SVG), callouts, progress, code; each front-end
##  draws them its own way (terminal: text and colored blocks)
\**************************************************************/

#ifndef DISPLAY_H
    #define DISPLAY_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Json
    #include <cstdint>              // std::uint32_t
    #include <string>               // std::string
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// a run of text of one color (an image of the terminal: chafa)
struct Cell {
    std::string text;
    std::uint32_t fg = 0;           // 0xRRGGBB
    std::uint32_t bg = 0;
    bool hasFg = false;
    bool hasBg = false;
};

//----------------------------------------------------------------//
/* FUNCTION */

// a display checked and made ready: an image file read (data, its type), a diagram (DOT) drawn (SVG and PNG), sizes bounded;
// throws on an unknown type or a bad field
_cold _nodiscard cluster::Json display_prepare(const cluster::Json& display);

// its text: search, export, what a model reads of it
_cold _nodiscard std::string display_text(const cluster::Json& display);

// an image (PNG / JPEG / SVG bytes) as lines of colored runs for the terminal (chafa), at most columns x rows
_cold _nodiscard std::vector<std::vector<cluster::Cell>> display_cells(const std::string& bytes, const int columns, const int rows);

// the bytes of its picture (image, diagram: its PNG, svg: its SVG), empty: none
_cold _nodiscard std::string display_bytes(const cluster::Json& display);

// a text with its ANSI colors (16, 256, 24-bit) as lines of colored runs (a terminal output, chafa)
_cold _nodiscard std::vector<std::vector<cluster::Cell>> ansi_cells(const std::string& text);

// LaTeX as Unicode (Greek letters, operators, superscripts / subscripts, \frac, \sqrt)
_cold _nodiscard std::string display_math(const std::string& latex);

// a sparkline of numbers (▁ to █), at most width characters
_cold _nodiscard std::string display_spark(const cluster::Json& values, const std::size_t width);

// a JSON value as indented lines "key: value" (depth levels, max lines), and the kind of each line (key / string / number / other)
_cold _nodiscard std::vector<std::pair<std::string, char>> display_json_lines(const cluster::Json& data, const int depth, const std::size_t max);

// a tree of files as lines "├── name" (folders end with /, its git state after), and its kind (d: folder, f: file, m: changed, n: new)
_cold _nodiscard std::vector<std::pair<std::string, char>> display_tree_lines(const cluster::Json& root);

// a turn running: its verb (Thinking, Running 3 tools, Waiting for the model...), how long its step and the turn, the
// tokens as they come, the tools running (name, input, ms), the steps done; "took" of milliseconds: 12s, 4m 41s
_cold _nodiscard cluster::Json turn_state(const cluster::Snapshot& snap);
_cold _nodiscard std::string took(const std::int64_t ms);

// the types and their fields, for the MCP tool and the help
_cold _nodiscard std::string display_help(void);

} // namespace end

#endif /* DISPLAY_H */
