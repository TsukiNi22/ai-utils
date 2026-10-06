/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Tools.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/Tools.hpp"
#include <algorithm>
#include <sstream>

/* string */
_hot std::string xstyle::trim(const std::string& s)
{
    std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

_hot std::vector<std::string> xstyle::split(const std::string& s, const char separator)
{
    std::vector<std::string> parts;
    std::string part;
    std::istringstream stream(s);

    while (std::getline(stream, part, separator))
        if (!xstyle::trim(part).empty()) parts.push_back(xstyle::trim(part));
    return parts;
}

_hot std::string xstyle::lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});
    return s;
}

_hot bool xstyle::glob_match(std::string_view pattern, std::string_view text)
{
    // Iterative matching with backtracking on the last '*'
    std::size_t p = 0;
    std::size_t t = 0;
    std::size_t star = std::string_view::npos;
    std::size_t mark = 0;

    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            mark = t;
        } else if (star != std::string_view::npos) {
            p = star + 1;
            t = ++mark;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}
