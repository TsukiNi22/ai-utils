/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Header.hpp

File Description:
##  File header generation (Xartania box, optional ANSI Shadow banner), port of cpp-class scripts/header.py
\**************************************************************/

#ifndef HEADER_H
    #define HEADER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include <string>           // std::string
    #include <vector>           // std::vector

namespace xstyle { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

/* header */
_cold _nodiscard bool banner_available(void); // false when built without the font
_cold _nodiscard std::vector<std::string> render_banner(const std::string& text); // ANSI Shadow, no trailing space
_cold _nodiscard std::vector<std::string> make_header(const std::string& file, const std::string& banner, const std::string& description); // banner: none | default | <text>

} // namespace end
#endif /* HEADER_H */
