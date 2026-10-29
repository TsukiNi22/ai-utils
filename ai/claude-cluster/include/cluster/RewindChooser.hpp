/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file RewindChooser.hpp

File Description:
##  The chooser of the point of the conversation to go back to
##  (Esc when nothing runs and the prompt is empty): the prompts
##  of the conversation, newest first, each with its time and
##  the size of its answer; Enter goes back to just before it
\**************************************************************/

#ifndef CLUSTER_REWINDCHOOSER_H
    #define CLUSTER_REWINDCHOOSER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Rewind.hpp"           // cluster::RewindPoint
    #include "Popup.hpp"            // cluster::PopupColors, cluster::PopupPress
    #include "Types.hpp"            // cluster::Entry
    #include <vector>               // std::vector
    #include <string>               // std::string

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct RewindResult {
    bool close = false;
    std::string status;                         // told in the status line
    int back = 0;                               // go back this many prompts (Manager::rewind); 0: no
};

//----------------------------------------------------------------//
/* CLASS */

class RewindChooser {
    private:
        bool _open = false;
        int _selected = 0;                      // a prompt, or the row under them: Cancel
        std::vector<cluster::RewindPoint> _points;  // newest first

    public:
        // ---------- Pre-Function -------- //
        _cold bool open(const std::vector<cluster::Entry>& entries);      // false: no prompt to go back to
        _cold _nodiscard cluster::RewindResult key(const ftxui::Event& event);
        _cold _nodiscard ftxui::Element render(const cluster::PopupColors& colors, const cluster::PopupPress& press) const;

        // ---------- Function -------- //
        _nodiscard inline bool active(void) const {return this->_open;};
        _cold inline void select(const int row)   {this->_selected = row;};
        _cold inline void close(void)             {this->_open = false;};

        // ---------- Constructor -------- //
        RewindChooser(void) = default;

        // ----------- Destructor --------- //
        ~RewindChooser() = default;
};

} // namespace end

#endif /* CLUSTER_REWINDCHOOSER_H */
