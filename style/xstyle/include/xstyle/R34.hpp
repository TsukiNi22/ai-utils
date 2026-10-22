/**************************************************************\
Edition:
##  @date 22/10/2026 by @author Tsukini

File Name:
##  @file R34.hpp

File Description:
##  The text of R34.md, written by xstyle --fix --force to acknowledge the rule R34
\**************************************************************/

#ifndef R34_H
    #define R34_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include <filesystem>           // std::filesystem::path
    #include <string>               // std::string

    //----------------------------------------------------------------//
    /* DEFINE */

    #define R34_FILE "R34.md" // File that acknowledges the rule R34 (written by --fix --force)
    #define R34_LINK "https://en.wikipedia.org/wiki/Rule_34" // Documentation of the rule, the green link of the issue

namespace xstyle { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

/* R34 */
_cold _nodiscard std::string r34_document(void); // The long explanation of the rule (the content of R34.md)
_cold _nodiscard bool r34_file_exists(const std::filesystem::path& directory); // R34.md (any case) in the directory

} // namespace end
#endif /* R34_H */
