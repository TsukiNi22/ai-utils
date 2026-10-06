/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Git.hpp

File Description:
##  Git helpers of xstyle (libutils Process): changed lines, dirty files, commit of the fixes
\**************************************************************/

#ifndef GIT_H
    #define GIT_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include <filesystem>       // std::filesystem::path
    #include <optional>         // std::optional
    #include <utility>          // std::pair
    #include <string>           // std::string
    #include <vector>           // std::vector
    #include <set>              // std::set
    #include <map>              // std::map

namespace xstyle::git { // namespace start
//----------------------------------------------------------------//
/* TYPEDEF */

using Ranges = std::vector<std::pair<std::size_t, std::size_t>>; // <first, last> lines (1-based, inclusive)
using ChangedLines = std::map<std::filesystem::path, xstyle::git::Ranges>; // absolute path -> changed lines

//----------------------------------------------------------------//
/* STRUCT */

struct Result {
    int code = -1; // exit code (-1: not run / killed)
    std::string out; // standard output
};

//----------------------------------------------------------------//
/* PROTOTYPE */

/* git */
_cold _nodiscard xstyle::git::Result run(const std::filesystem::path& dir, const std::vector<std::string>& args); // git -C dir args...
_cold _nodiscard std::optional<std::filesystem::path> root(const std::filesystem::path& dir); // nullopt: not a repository
_cold _nodiscard xstyle::git::ChangedLines changed_lines(const std::filesystem::path& root, const bool staged, const std::string& ref); // + the untracked files (not staged)
_cold _nodiscard std::set<std::filesystem::path> dirty_files(const std::filesystem::path& root); // modified, staged or untracked
_cold _nodiscard bool commit(const std::filesystem::path& root, const std::vector<std::filesystem::path>& files, const std::string& message, const bool all);

} // namespace end
#endif /* GIT_H */
