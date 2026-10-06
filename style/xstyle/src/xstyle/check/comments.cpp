/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file comments.cpp

File Description:
##  cpp-comments structure checks: include trailing comments, empty sections / class blocks, orphan group labels
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/check/Checks.hpp"
#include "xstyle/Tools.hpp"
#include <algorithm>
#include <regex>
#include <set>

/* tools */
_hot static std::size_t marker_(const std::vector<std::string>& lines, const std::size_t i) // lines of the marker starting at i (0: not a marker)
{
    static const std::regex separator(R"(^\s*//-{20,}//\s*$)");
    static const std::regex section(R"(^\s*/\* [A-Z][A-Z &]* \*/\s*$)");
    static const std::regex block(R"(^\s*// -+ [A-Za-z][A-Za-z -]* -+ //\s*$)");
    if (std::regex_match(lines[i], block)) return 1;
    if (std::regex_match(lines[i], separator)) return i + 1 < lines.size() && std::regex_match(lines[i + 1], section) ? 2 : 1;
    return 0;
}

_hot static bool closing_(const std::string& line) // end of what a marker / label can describe
{
    static const std::regex end(R"(^\s*(?:\}\s*;?|\}\s*//.*|#\s*endif\b.*|(?:public|private|protected)\s*:.*|namespace\b[^;]*\{.*)$)");
    return std::regex_match(line, end);
}

/* checks */
_hot void xstyle::check::comments(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex label(R"(^\s*/\* [a-z][a-z0-9 &/_-]* \*/\s*$)");
    static const std::regex include(R"(^(\s*#\s*include\s*[<"][^>"]+[>"])(\s*)(//.*)?$)");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<xstyle::LineInfo>& info = file.getInfo();

    // Empty sections / class blocks and orphan /* group */ labels (removed: only comments)
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::size_t size = marker_(lines, i);
        const bool isLabel = size == 0 && std::regex_match(lines[i], label);
        if (size == 0 && !isLabel) continue;
        std::size_t next = i + std::max<std::size_t>(size, 1);
        while (next < lines.size() && xstyle::is_blank(lines[next])) ++next;
        const bool empty = next >= lines.size() || marker_(lines, next) > 0 || closing_(lines[next]) || (isLabel && std::regex_match(lines[next], label));
        if (!empty) continue;
        if (isLabel)
            issues.push_back(xstyle::check::make_issue(file, i, lines[i].find("/*"), "CPP-ORPHAN-GROUP", "Group label with nothing under it", "", xstyle::check::replace_lines(i, 1, {})));
        else
            issues.push_back(xstyle::check::make_issue(file, i, lines[i].find("//"), "CPP-EMPTY-SECTION", "Section / class block with nothing linked to it", "",
                xstyle::check::replace_lines(i, size, {})));
        i += std::max<std::size_t>(size, 1) - 1;
    }

    // Includes of a header: a trailing comment each, aligned on one column per block
    if (!file.isHeader()) return;
    for (std::size_t i = 0; i < lines.size();) {
        std::smatch match;
        if (!info[i].preprocessor || !std::regex_match(lines[i], match, include) || info[i].brace != NO_INDEX) {
            ++i;
            continue;
        }
        std::size_t end = i;
        std::vector<std::smatch> block;
        while (end < lines.size() && std::regex_match(lines[end], match, include)) {
            block.push_back(match);
            ++end;
        }
        std::size_t longest = 0;
        std::size_t gap = 1;
        std::set<std::size_t> columns;
        for (std::size_t k = 0; k < block.size(); ++k) {
            const std::size_t width = static_cast<std::size_t>(block[k].length(1));
            if (!block[k][3].matched) {
                issues.push_back(xstyle::check::make_issue(file, i + k, static_cast<std::size_t>(block[k].length(1)), "CPP-INCLUDE-COMMENT", "Include without trailing comment",
                    xstyle::trim(lines[i + k]) + " // what is used (std::x, utils::y...)"));
                continue;
            }
            columns.insert(static_cast<std::size_t>(block[k].position(3)));
            if (width >= longest) {
                longest = width;
                gap = std::max<std::size_t>(static_cast<std::size_t>(block[k].length(2)), 1);
            }
        }
        if (columns.size() > 1) {
            // The column of the longest include (with its own gap) for every comment of the block
            std::vector<std::string> fixed;
            for (std::size_t k = 0; k < block.size(); ++k) {
                if (!block[k][3].matched) {
                    fixed.push_back(lines[i + k]);
                    continue;
                }
                const std::string head = block[k][1].str();
                fixed.push_back(head + std::string(longest + gap - head.size(), ' ') + block[k][3].str());
            }
            issues.push_back(xstyle::check::make_issue(file, i, 0, "CPP-COMMENT-ALIGN", "Include comments not aligned", "", xstyle::check::replace_lines(i, end - i, fixed)));
        }
        i = end;
    }
}
