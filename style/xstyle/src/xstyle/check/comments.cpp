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
#include <cstdint>
#include <cctype>
#include <regex>
#include <map>
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

/* class blocks */
enum class Member: std::uint8_t {
    Skip, // friend, using, nested type, static_assert...
    Data,
    Declared, // a function declared only (defined in the .cpp), = 0 included
    Defined, // a function defined in the header ({...}, = default, = delete)
    Constructor,
    Destructor,
    Operator,
};

_hot static std::string block_of_(const std::string& line) // name of the class block marker of the line ("" none)
{
    static const std::regex marker(R"(^\s*// -+ (Pre-Function|Function|Operator|Constructor|Destructor) -+ //\s*$)");
    std::smatch match;
    return std::regex_match(line, match, marker) ? match[1].str() : "";
}

_hot static Member member_kind_(std::string text, const std::string& name, const bool body)
{
    // a statement of a class body (masked, joined, up to its ; or its body): what it declares
    static const std::regex skip(R"(^(?:friend|using|typedef|static_assert|enum|struct|class|union|template)\b)");
    static const std::regex operatorWord(R"(\boperator\b)");
    static const std::regex special(R"(\)[^()]*=\s*(?:default|delete)\s*$)");
    text = xstyle::trim(text);
    if (text.starts_with("template")) { // template<...> before a member function: the function
        int depth = 0;
        std::size_t at = text.find('<');
        for (; at != std::string::npos && at < text.size(); ++at) {
            if (text[at] == '<') ++depth;
            else if (text[at] == '>' && --depth == 0) break;
        }
        text = at == std::string::npos || at >= text.size() ? "" : xstyle::trim(text.substr(at + 1));
        if (text.starts_with("requires")) { // its constraint: (expression) or one concept<...>
            std::size_t end = text.find_first_not_of(" \t", 8);
            int nesting = 0;
            for (; end != std::string::npos && end < text.size(); ++end) {
                const char c = text[end];
                if (c == '(' || c == '<') ++nesting;
                else if ((c == ')' || c == '>') && --nesting == 0) break;
                else if (nesting == 0 && (c == ' ' || c == '\t')) break;
            }
            text = end == std::string::npos || end >= text.size() ? "" : xstyle::trim(text.substr(end + 1));
        }
    }
    if (text.empty() || std::regex_search(text, skip)) return Member::Skip;
    // the first ( outside <>, before any = / {: a function, else a data member (with its initializer)
    int angle = 0;
    std::size_t paren = std::string::npos;
    for (std::size_t i = 0; i < text.size() && paren == std::string::npos; ++i) {
        if (text[i] == '<') ++angle;
        else if (text[i] == '>' && angle > 0) --angle;
        else if (angle == 0 && (text[i] == '=' || text[i] == '{')) break;
        else if (angle == 0 && text[i] == '(') paren = i;
    }
    if (std::regex_search(text.substr(0, paren == std::string::npos ? text.size() : paren), operatorWord)) return Member::Operator;
    static const std::regex pointer(R"(^\(\s*(?:[A-Za-z_][\w:]*::)?\s*[*&]\s*\w*\s*\)\s*\()"); // (*name)( / (X::*name)(
    if (paren == std::string::npos || std::regex_search(text.substr(paren), pointer)) return Member::Data; // void (*handler)(int): a member
    const std::size_t last = text.find_last_not_of(" \t", paren == 0 ? 0 : paren - 1);
    std::size_t first = last;
    while (first != std::string::npos && first > 0 && (std::isalnum(static_cast<unsigned char>(text[first - 1])) || text[first - 1] == '_' || text[first - 1] == '~'))
        --first;
    const std::string identifier = last == std::string::npos ? "" : text.substr(first, last - first + 1);
    if (identifier.starts_with("~")) return Member::Destructor;
    if (identifier == name) return Member::Constructor;
    return body || std::regex_search(text, special) ? Member::Defined : Member::Declared;
}

_hot static void class_blocks_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    // in a class using the blocks: Pre-Function = declared only, Function = defined in the header, Constructor /
    // Destructor = theirs, members before the blocks; the Operator block and the operators are free (hint of the skill)
    static const std::regex visibility(R"(^(?:public|private|protected)\s*:.*$)");
    static const std::map<Member, std::string> expected = {{Member::Declared, "Pre-Function"}, {Member::Defined, "Function"},
        {Member::Constructor, "Constructor"}, {Member::Destructor, "Destructor"}};
    static const std::map<Member, std::string> what = {{Member::Data, "Data member"}, {Member::Declared, "Function declared without body"},
        {Member::Defined, "Function defined in the header"}, {Member::Constructor, "Constructor"}, {Member::Destructor, "Destructor"}};
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    const std::vector<xstyle::LineInfo>& info = file.getInfo();
    const std::vector<xstyle::Brace>& braces = file.getBraces();
    std::map<std::pair<std::size_t, std::size_t>, std::size_t> braceAt;
    for (std::size_t b = 0; b < braces.size(); ++b)
        braceAt[{braces[b].openLine, braces[b].openColumn}] = b;

    for (std::size_t b = 0; b < braces.size(); ++b) {
        const xstyle::Brace& owner = braces[b];
        if ((owner.kind != xstyle::Scope::Class && owner.kind != xstyle::Scope::Struct) || owner.closeLine == NO_INDEX) continue;
        bool marked = false;
        for (std::size_t l = owner.openLine + 1; l < owner.closeLine && !marked; ++l)
            marked = info[l].brace == b && !block_of_(lines[l]).empty();
        if (!marked) continue; // a class without blocks (a plain struct...): free
        std::string block;
        for (std::size_t l = owner.openLine + 1; l < owner.closeLine;) {
            if (info[l].brace != b || info[l].preprocessor || info[l].parenDepth > 0) {
                ++l;
                continue;
            }
            const std::string marker = block_of_(lines[l]);
            const std::string start = xstyle::trim(code[l]);
            if (!marker.empty() || start.empty() || std::regex_match(start, visibility)) {
                if (!marker.empty()) block = marker;
                else if (!start.empty()) block.clear(); // a new visibility: its own blocks
                ++l;
                continue;
            }
            // the statement: up to its ; at the class level, or its body (a brace init / lambda inside it skipped)
            std::string text;
            bool body = false;
            bool nested = false;
            int paren = 0;
            std::size_t k = l;
            std::size_t column = 0;
            while (k < owner.closeLine) {
                if (column >= code[k].size()) {
                    text += ' ';
                    ++k;
                    column = 0;
                    continue;
                }
                const char c = code[k][column];
                if (c == '(') ++paren;
                else if (c == ')') --paren;
                else if (paren == 0 && c == ';') break;
                else if (paren == 0 && c == '{' && braceAt.contains({k, column}) && braces[braceAt.at({k, column})].closeLine != NO_INDEX) {
                    const xstyle::Brace& inner = braces[braceAt.at({k, column})];
                    if (inner.kind == xstyle::Scope::Function || inner.kind == xstyle::Scope::Class || inner.kind == xstyle::Scope::Struct
                        || inner.kind == xstyle::Scope::Enum) {
                        body = inner.kind == xstyle::Scope::Function;
                        nested = !body;
                        k = inner.closeLine;
                        break;
                    }
                    text += "{}";
                    k = inner.closeLine;
                    column = inner.closeColumn + 1;
                    continue;
                }
                text += c;
                ++column;
            }
            const Member kind = nested ? Member::Skip : member_kind_(text, owner.name, body);
            const std::size_t line = l;
            l = std::max(k, l) + 1;
            if (kind == Member::Skip || kind == Member::Operator || block == "Operator") continue;
            const std::size_t at = lines[line].find_first_not_of(" \t");
            if (kind == Member::Data) {
                if (!block.empty())
                    issues.push_back(xstyle::check::make_issue(file, line, at, "CPP-CLASS-BLOCK", "Data member under the " + block + " block of " + owner.name,
                        "members first, before the blocks of the visibility"));
                continue;
            }
            const std::string& place = expected.at(kind);
            if (block == place) continue;
            std::string hint = "it goes in the " + place + " block";
            if (kind == Member::Defined && block == "Pre-Function") hint = "move it to the Function block (inline one-liner), or keep the declaration here and define it in the .cpp";
            if (kind == Member::Declared && block == "Function") hint = "move it to the Pre-Function block (defined in the .cpp), or define it here {...};";
            issues.push_back(xstyle::check::make_issue(file, line, at, "CPP-CLASS-BLOCK", what.at(kind) + (block.empty() ? " outside the class blocks" : " in the " + block + " block")
                + " of " + owner.name, hint));
        }
    }
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
        static const std::regex namespaceOpen(R"(^\s*namespace\b[^;]*\{\s*(?://.*)?$)");
        std::size_t next = i + std::max<std::size_t>(size, 1);
        while (next < lines.size() && xstyle::is_blank(lines[next])) ++next;
        // A namespace opened right after: the section describes its content (empty only when the namespace is)
        while (next < lines.size() && std::regex_match(lines[next], namespaceOpen))
            for (++next; next < lines.size() && xstyle::is_blank(lines[next]); ++next);
        // A section (separator + title) holds the class / sub blocks that follow it
        static const std::regex subBlock(R"(^\s*// -+ [A-Za-z][A-Za-z -]* -+ //\s*$)");
        const bool child = size == 2 && next < lines.size() && std::regex_match(lines[next], subBlock);
        const bool empty = next >= lines.size() || (marker_(lines, next) > 0 && !child) || closing_(lines[next]) || (isLabel && std::regex_match(lines[next], label));
        if (!empty) continue;
        if (isLabel)
            issues.push_back(xstyle::check::make_issue(file, i, lines[i].find("/*"), "CPP-ORPHAN-GROUP", "Group label with nothing under it", "", xstyle::check::replace_lines(i, 1, {})));
        else
            issues.push_back(xstyle::check::make_issue(file, i, lines[i].find("//"), "CPP-EMPTY-SECTION", "Section / class block with nothing linked to it", "",
                xstyle::check::replace_lines(i, size, {})));
        i += std::max<std::size_t>(size, 1) - 1;
    }

    // Includes of a header: a trailing comment each, aligned on one column per block; the class blocks of its classes
    if (!file.isHeader()) return;
    if (file.getLanguage() == xstyle::Language::Cpp) class_blocks_(file, issues);
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
