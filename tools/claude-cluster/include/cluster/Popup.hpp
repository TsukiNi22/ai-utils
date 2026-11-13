/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Popup.hpp

File Description:
##  What the small list popups of the terminal share (the
##  chooser of the points of the conversation, the list of the
##  stash): colors, a clicked element, rows, time, sizes
\**************************************************************/

#ifndef CLUSTER_POPUP_H
    #define CLUSTER_POPUP_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard
    #include <ftxui/component/event.hpp>    // ftxui::Event
    #include <ftxui/screen/color.hpp>       // ftxui::Color
    #include <ftxui/dom/elements.hpp>       // ftxui::Element
    #include <functional>                   // std::function
    #include <cstdint>                      // std::int64_t
    #include <cstddef>                      // std::size_t
    #include <string>                       // std::string

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct PopupColors {
    ftxui::Color text;
    ftxui::Color muted;
    ftxui::Color accent;
    ftxui::Color bg;
    ftxui::Color warn;
    ftxui::Color error;
};

// an element of a popup clicked = the key typed (the row it is on is chosen first, -1: none)
using PopupPress = std::function<ftxui::Element(ftxui::Element, const ftxui::Event&, const int)>;

//----------------------------------------------------------------//
/* FUNCTION */

_cold _nodiscard std::string popup_clock(const std::int64_t time);                  // HH:MM today, else dd/mm HH:MM
_cold _nodiscard std::string popup_size(const std::size_t chars);                   // 812 chars, 1.2k chars
_cold _nodiscard ftxui::Element popup_row(const std::string& text, const bool on, const cluster::PopupColors& colors);

} // namespace end

#endif /* CLUSTER_POPUP_H */
