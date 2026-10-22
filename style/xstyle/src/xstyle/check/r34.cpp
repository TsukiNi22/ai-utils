/**************************************************************\
Edition:
##  @date 22/10/2026 by @author Tsukini

File Name:
##  @file r34.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/check/Checks.hpp"
#include "xstyle/Tools.hpp"
#include <algorithm>

/* tools */
_hot static bool has_r34_(const std::string& text)
{
    // r34 in any case, inside an identifier, a string or a comment (is_r34, "R34", // r34...)
    return xstyle::lower(text).find("r34") != std::string::npos;
}

_hot static bool has_integer_34_(const std::string& code)
{
    // The integer 34 (34, -34, 34u, 34UL...): not inside a name or a bigger number (134, 0x34, a34), not a float (34.5, 34.)
    for (std::size_t pos = code.find("34"); pos != std::string::npos; pos = code.find("34", pos + 1)) {
        const bool before = pos > 0 && (xstyle::is_word(code[pos - 1]) || code[pos - 1] == '.');
        std::size_t end = pos + 2;
        while (end < code.size() && std::string("uUlLzZ").find(code[end]) != std::string::npos) ++end; // integer suffix
        const bool after = end < code.size() && (xstyle::is_word(code[end]) || code[end] == '.');
        if (!before && !after) return true;
    }
    return false;
}

/* check */
_hot bool xstyle::check::r34_proof(const xstyle::SourceFile& file)
{
    // The masked languages know their code, strings and comments; the others (json, markdown...) only know r34 anywhere
    const xstyle::Language language = file.getLanguage();
    const bool masked = language != xstyle::Language::Json && language != xstyle::Language::Markdown && language != xstyle::Language::Other;
    const std::vector<std::string>& lines = file.getLines();

    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (has_r34_(lines[i])) return true;
        if (masked && file.getCode()[i].find_first_not_of(' ') != std::string::npos && has_integer_34_(file.getCode()[i])) return true;
    }
    return false;
}
