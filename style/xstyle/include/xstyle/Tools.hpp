/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Tools.hpp

File Description:
##  String tools shared by the checks
\**************************************************************/

#ifndef TOOLS_H
    #define TOOLS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <string_view>      // std::string_view
    #include <algorithm>        // std::min
    #include <cctype>           // std::isalnum, std::isspace
    #include <string>           // std::string
    #include <vector>           // std::vector

namespace xstyle { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

/* string */
_hot _nodiscard std::string trim(const std::string& s);
_hot _nodiscard std::vector<std::string> split(const std::string& s, const char separator);
_hot _nodiscard std::string lower(std::string s);
_hot _nodiscard bool glob_match(std::string_view pattern, std::string_view text); // * and ? jokers
_hot _nodiscard inline std::string indentation(const std::string& line) {return line.substr(0, std::min(line.find_first_not_of(" \t"), line.size()));}; // leading whitespace
_hot _nodiscard inline bool is_blank(const std::string& s)              {return s.find_first_not_of(" \t") == std::string::npos;};
_hot _nodiscard inline bool is_word(const char c)                        {return std::isalnum(static_cast<unsigned char>(c)) || c == '_';};
_hot _nodiscard inline bool is_space(const char c)                       {return std::isspace(static_cast<unsigned char>(c));};

} // namespace end
#endif /* TOOLS_H */
