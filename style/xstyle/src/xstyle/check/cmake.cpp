/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file cmake.cpp

File Description:
##  CMake checks (cmake-style): section order and banners, compiler before project(), globbed / unregistered sources
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/check/Checks.hpp"
#include "xstyle/Tools.hpp"
#include <algorithm>
#include <regex>

/* checks */
_hot void xstyle::check::cmake(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex bannerLine(R"(^#\s*=+\s*$)");
    static const std::regex title(R"(^#\s+(.+?)\s*$)");
    static const std::regex glob(R"(\bfile\s*\(\s*GLOB(?:_RECURSE)?\b)");
    static const std::regex sourceGlob(R"(\*\.(?:cpp|cc|cxx|c|hpp|h)\b)");
    static const std::regex setSrc(R"(^\s*set\s*\(\s*SRC\b)");
    static const std::regex compiler(R"(^\s*set\s*\(\s*CMAKE_CXX_COMPILER\b)");
    static const std::regex project(R"(^\s*project\s*\()");
    static const std::vector<std::vector<std::string>> order = { // cmake-style section order (several names for one place)
        {"options"}, {"compilateur & standard", "compiler & standard"}, {"requirement", "requirements"}, {"warnings"}, {"sources"},
        {"plugins", "compilation parameters"}, {"special targets"}, {"release"}, {"dependencies"}, {"includes", "headers configuration"}, {"modes"},
        {"installation"}, {"utils"}, {"package building"},
    };
    const std::string banner = "# " + std::string(25, '=');
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    if (file.getPath().filename() != "CMakeLists.txt") return;

    // Banners (# + 25 =) and section order
    std::size_t rank = 0;
    std::string previous;
    for (std::size_t i = 0; i + 2 < lines.size(); ++i) {
        std::smatch match;
        if (!std::regex_match(lines[i], bannerLine) || !std::regex_match(lines[i + 1], match, title) || !std::regex_match(lines[i + 2], bannerLine)) continue;
        for (const std::size_t b: {i, i + 2})
            if (lines[b] != banner)
                issues.push_back(xstyle::check::make_issue(file, b, 0, "CMAKE-BANNER", "Section banner not '# ' + 25 '='", "", xstyle::check::replace_line(b, banner)));
        const std::string name = xstyle::lower(match[1].str());
        for (std::size_t r = 0; r < order.size(); ++r) {
            if (std::none_of(order[r].begin(), order[r].end(), [&](const std::string& n) {return name.starts_with(n);})) continue;
            if (r < rank)
                issues.push_back(xstyle::check::make_issue(file, i + 1, 2, "CMAKE-SECTION-ORDER", "Section '" + match[1].str() + "' after '" + previous + "'",
                    "cmake-style order: Options, Compilateur & Standard, Requirement, Warnings, Sources, Special Targets, Release, Dependencies, Modes, Installation, Utils, Package Building"));
            else {
                rank = r;
                previous = match[1].str();
            }
            break;
        }
        i += 2;
    }

    // The compiler is detected by project(): set after it, it is ignored
    std::size_t projectLine = NO_INDEX;
    for (std::size_t i = 0; i < code.size(); ++i) {
        if (projectLine == NO_INDEX && std::regex_search(code[i], project)) projectLine = i;
        if (projectLine == NO_INDEX || i <= projectLine || !std::regex_search(code[i], compiler)) continue;
        std::vector<std::string> fixed(lines.begin() + static_cast<std::ptrdiff_t>(projectLine), lines.begin() + static_cast<std::ptrdiff_t>(i));
        fixed.insert(fixed.begin(), lines[i]);
        xstyle::Fix fix = xstyle::check::replace_lines(projectLine, i - projectLine + 1, fixed);
        fix.unsafe = true; // the compiler really changes (it was ignored)
        issues.push_back(xstyle::check::make_issue(file, i, 0, "CMAKE-COMPILER-ORDER", "CMAKE_CXX_COMPILER set after project(): ignored", "", fix));
    }

    // Sources: explicit list, every source of src/ registered
    bool globbed = false;
    std::size_t srcLine = NO_INDEX;
    std::size_t srcEnd = NO_INDEX;
    for (std::size_t i = 0; i < code.size(); ++i) {
        if (std::regex_search(code[i], glob) && std::regex_search(lines[i], sourceGlob)) {
            globbed = true;
            issues.push_back(xstyle::check::make_issue(file, i, 0, "CMAKE-GLOB-SOURCES", "Sources globbed (a new file is only built after a re-configure)",
                "explicit set(SRC ...) with group comments (cmake-style)"));
        }
        if (srcLine == NO_INDEX && std::regex_search(code[i], setSrc)) {
            srcLine = i;
            for (std::size_t j = i; j < code.size() && srcEnd == NO_INDEX; ++j)
                if (code[j].find(')') != std::string::npos) srcEnd = j;
        }
    }
    const std::filesystem::path dir = file.getPath().parent_path();
    std::error_code error;
    if (globbed || srcLine == NO_INDEX || srcEnd == NO_INDEX || !std::filesystem::is_directory(dir / "src", error)) return;
    std::string content;
    for (const std::string& line: lines)
        content += line + "\n";
    std::vector<std::string> unregistered;
    for (std::filesystem::recursive_directory_iterator it(dir / "src", error), end; it != end && !error; it.increment(error)) {
        const std::string extension = it->path().extension().string();
        if (!it->is_regular_file() || (extension != ".cpp" && extension != ".cc" && extension != ".cxx" && extension != ".c")) continue;
        const std::string relative = std::filesystem::relative(it->path(), dir, error).generic_string();
        if (content.find(relative) == std::string::npos) unregistered.push_back(relative);
    }
    std::sort(unregistered.begin(), unregistered.end());
    for (const std::string& source: unregistered) {
        // Added at the end of the list (the group comment is for the user): it changes the build, --force
        const std::string closing = lines[srcEnd];
        const std::size_t paren = code[srcEnd].find(')');
        std::vector<std::string> fixed;
        if (xstyle::is_blank(code[srcEnd].substr(0, paren))) {
            fixed = {"    " + source, closing};
        } else {
            fixed = {closing.substr(0, paren), "    " + source, closing.substr(paren)};
        }
        xstyle::Fix fix = xstyle::check::replace_lines(srcEnd, 1, fixed);
        fix.unsafe = true;
        issues.push_back(xstyle::check::make_issue(file, srcLine, 0, "CMAKE-UNREGISTERED", source + " is not in set(SRC ...)", "", fix));
    }
}
