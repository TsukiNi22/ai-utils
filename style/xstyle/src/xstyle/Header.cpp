/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Header.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/Header.hpp"
#include "xstyle/Tools.hpp"
#include "xstyle/Font.hpp"
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <ctime>

/* tools */
_cold static std::size_t width_(const std::string& s) // characters, not bytes (the font is UTF-8)
{
    return static_cast<std::size_t>(std::count_if(s.begin(), s.end(), [](const char c) {return (static_cast<unsigned char>(c) & 0xC0) != 0x80;}));
}

_cold static const std::unordered_map<char, std::vector<std::string>>& glyphs_(std::size_t& height)
{
    // FIGlet font: <signature><hardblank> <height> ..., <comment lines>, then 'height' rows per character from ' ' (32)
    static std::size_t fontHeight = 0;
    static std::unordered_map<char, std::vector<std::string>> glyphs;
    if (glyphs.empty() && !xstyle::font::ANSI_SHADOW.empty()) {
        std::vector<std::string> lines;
        std::istringstream stream{std::string(xstyle::font::ANSI_SHADOW)};
        for (std::string line; std::getline(stream, line);)
            lines.push_back(line);
        std::istringstream params(lines[0]);
        std::string signature;
        std::size_t comments = 0;
        std::size_t ignored = 0;
        params >> signature >> fontHeight >> ignored >> ignored >> ignored >> comments;
        const char hardblank = signature.back();
        std::size_t i = 1 + comments;
        for (int code = 32; code < 127 && i + fontHeight <= lines.size(); ++code) {
            std::vector<std::string> rows;
            for (std::size_t r = 0; r < fontHeight; ++r, ++i) {
                std::string row = lines[i];
                const char end = row.empty() ? '\0' : row.back();
                while (!row.empty() && row.back() == end) row.pop_back(); // end markers (@ / @@)
                std::replace(row.begin(), row.end(), hardblank, ' ');
                rows.push_back(row);
            }
            glyphs[static_cast<char>(code)] = rows;
        }
    }
    height = fontHeight;
    return glyphs;
}

/* header */
_cold bool xstyle::banner_available(void)
{
    std::size_t height = 0;
    return !glyphs_(height).empty();
}

_cold std::vector<std::string> xstyle::render_banner(const std::string& text)
{
    std::size_t height = 0;
    const std::unordered_map<char, std::vector<std::string>>& glyphs = glyphs_(height);
    std::vector<std::string> out(height);
    if (glyphs.empty()) return {};

    for (const char c: text) {
        const char key = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        const std::vector<std::string>& glyph = glyphs.contains(key) ? glyphs.at(key) : glyphs.at(' ');
        std::size_t width = 0;
        for (const std::string& row: glyph)
            width = std::max(width, width_(row));
        for (std::size_t r = 0; r < height; ++r)
            out[r] += glyph[r] + std::string(width - width_(glyph[r]), ' ');
    }
    // No trailing space, no empty row at the end
    for (std::string& row: out)
        row.erase(row.find_last_not_of(' ') == std::string::npos ? 0 : row.find_last_not_of(' ') + 1);
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

_cold std::vector<std::string> xstyle::make_header(const std::string& file, const std::string& banner, const std::string& description)
{
    const char* author = std::getenv("XSTYLE_AUTHOR");
    const std::time_t now = std::time(nullptr);
    char date[16] = {};
    (void)std::strftime(date, sizeof(date), "%d/%m/%Y", std::localtime(&now));
    std::vector<std::string> header = {"/**************************************************************\\"};

    // Banner: none, default (XARTANIA) or a custom text
    const std::string name = xstyle::lower(banner) == "default" || xstyle::lower(banner) == "xartania" ? "XARTANIA" : banner;
    if (xstyle::lower(banner) != "none") {
        header.push_back("");
        for (const std::string& row: xstyle::render_banner(name))
            header.push_back(" " + row);
        header.push_back("");
    }
    header.insert(header.end(), {"Edition:", "##  @date " + std::string(date) + " by @author " + (author ? author : "Tsukini"), "", "File Name:", "##  @file " + file, "",
        "File Description:"});
    if (xstyle::trim(description).empty()) {
        header.push_back("##  You know, I don t think there are good or bad descriptions,");
        header.push_back("##  for me, life is all about functions...");
    } else {
        std::string text = description;
        for (std::size_t pos = text.find("\\n"); pos != std::string::npos; pos = text.find("\\n", pos)) // "\n" typed as two characters (like header.py)
            text.replace(pos, 2, "\n");
        for (const std::string& line: xstyle::split(text, '\n'))
            header.push_back("##  " + line);
    }
    header.push_back("\\**************************************************************/");
    return header;
}
