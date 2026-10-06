/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Project.hpp

File Description:
##  Detection of the project: root, libutils installed / used, migration aliases, attribute macros
\**************************************************************/

#ifndef PROJECT_H
    #define PROJECT_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "XStyleType.hpp"       // xstyle::ProjectInfo, xstyle::Language
    #include <filesystem>           // std::filesystem::path
    #include <vector>               // std::vector

namespace xstyle { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

/* project */
_cold _nodiscard xstyle::ProjectInfo detect_project(const std::filesystem::path& start);
_cold _nodiscard xstyle::Language language_of(const std::filesystem::path& path);

} // namespace end
#endif /* PROJECT_H */
