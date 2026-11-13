/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file PromptNav.hpp

File Description:
##  Up / Down of the prompt editor by VISUAL row: the rows the
##  prompt is wrapped in on the screen, the column kept from one
##  row to the other (wide characters, atoms: cells, not bytes)
\**************************************************************/

#ifndef CLUSTER_PROMPTNAV_H
    #define CLUSTER_PROMPTNAV_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Editor.hpp"           // cluster::Editor
    #include <optional>             // std::optional
    #include <cstddef>              // std::size_t
    #include <vector>               // std::vector

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct VisualRow {
    std::size_t start = 0;      // first byte of the row
    std::size_t end = 0;        // end of the row (its \n, the start of the next row, or the end of the text)
    bool last = false;          // the last row of its line: the cursor at its end is on it
};

struct VisualGoal {
    std::size_t cursor = std::string::npos;     // where the last vertical move ended: another cursor = a new target column
    int column = 0;                             // the column (cells) wanted
};

//----------------------------------------------------------------//
/* FUNCTION */

// rows cover the whole text (checked): false = the rows are of an older frame
_cold _nodiscard inline bool visual_rows_valid(const std::vector<cluster::VisualRow>& rows, const std::string& text) {return !rows.empty() && rows.front().start == 0 && rows.back().end == text.size();};
_cold _nodiscard std::size_t visual_row_of(const std::vector<cluster::VisualRow>& rows, const std::size_t cursor);
_cold _nodiscard int visual_cells(const cluster::Editor& editor, const std::size_t from, const std::size_t to);     // width of text[from, to)
// the cursor after Up (dy < 0) / Down (dy > 0); nullopt: already on the first / last row
_cold _nodiscard std::optional<std::size_t> visual_move(const cluster::Editor& editor, const std::vector<cluster::VisualRow>& rows, const int dy, cluster::VisualGoal& goal);

} // namespace end

#endif /* CLUSTER_PROMPTNAV_H */
