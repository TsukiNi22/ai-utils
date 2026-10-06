/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file generic.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/check/Checks.hpp"
#include "xstyle/Rules.hpp"
#include "xstyle/Tools.hpp"
#include <algorithm>
#include <regex>

/* issue */
_hot xstyle::Issue xstyle::check::make_issue(const xstyle::SourceFile& file, const std::size_t line, const std::size_t column, std::string_view code,
    const std::string& message, const std::string& suggestion, const std::optional<xstyle::Fix>& fix)
{
    xstyle::Issue issue;
    const xstyle::Rule* rule = xstyle::find_rule(code);

    issue.file = file.getDisplay();
    issue.path = file.getPath();
    issue.line = line == NO_INDEX ? 0 : line + 1;
    issue.column = line == NO_INDEX ? 0 : column + 1;
    issue.code = std::string(code);
    issue.severity = rule ? rule->severity : xstyle::Severity::Minor;
    issue.message = message;
    issue.source = line < file.getLines().size() ? file.getLines()[line] : "";
    issue.suggestion = suggestion;
    issue.fix = fix;
    if (!fix || !suggestion.empty() || fix->kind != xstyle::FixKind::Replace) return issue;

    // Preview of the fix: only the lines that change
    const std::vector<std::string>& lines = file.getLines();
    std::vector<std::string> before(lines.begin() + static_cast<std::ptrdiff_t>(std::min(fix->line, lines.size())),
        lines.begin() + static_cast<std::ptrdiff_t>(std::min(fix->line + fix->count, lines.size())));
    std::vector<std::string> after = fix->lines;
    while (!before.empty() && !after.empty() && before.front() == after.front()) {
        before.erase(before.begin());
        after.erase(after.begin());
    }
    while (!before.empty() && !after.empty() && before.back() == after.back()) {
        before.pop_back();
        after.pop_back();
    }
    if (after.empty()) {
        issue.suggestion = "(" + std::to_string(before.size()) + " line" + (before.size() > 1 ? "s" : "") + " removed)";
        return issue;
    }
    for (std::size_t i = 0; i < after.size() && i < PREVIEW_LINES; ++i)
        issue.suggestion += (i == 0 ? "" : "\n") + after[i];
    if (after.size() > PREVIEW_LINES) issue.suggestion += "\n... (+" + std::to_string(after.size() - PREVIEW_LINES) + " lines)";
    return issue;
}

/* tools */
_hot static bool continued_(const std::string& previous, const std::string& current)
{
    // The line continues an expression of the previous one (alignment is free there)
    static const std::regex endOperator(R"((\\|,|\(|\[|&&|\|\||[+\-*/%?:=<>|&^.]|<<|>>)\s*$)");
    static const std::regex startOperator(R"(^\s*(\.|->|<<|>>|&&|\|\||\?|:|\+|-|\*|/|%|\||&|\)|\]|\}))");
    return std::regex_search(previous, endOperator) || std::regex_search(current, startOperator);
}

/* checks */
_hot void xstyle::check::generic(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex todo(R"(\b(TODO|FIXME|XXX)\b)");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    const std::vector<std::string>& comments = file.getComments();
    const std::vector<xstyle::LineInfo>& info = file.getInfo();
    const xstyle::Language language = file.getLanguage();
    const bool makefile = language == xstyle::Language::Makefile;
    const bool markdown = language == xstyle::Language::Markdown;

    // Whole file
    if (file.hasCrlf())
        issues.push_back(xstyle::check::make_issue(file, NO_INDEX, 0, "G-CRLF", "Windows line endings (CRLF)", "convert every line ending to LF", xstyle::Fix{xstyle::FixKind::Crlf, 0, 0, {}}));
    if (!file.hasFinalNewline() && !lines.empty())
        issues.push_back(xstyle::check::make_issue(file, lines.size() - 1, lines.back().size(), "G-EOF-NEWLINE", "No newline at the end of the file", "add the final newline", xstyle::Fix{xstyle::FixKind::FinalNewline, 0, 0, {}}));

    // Empty lines at the end of the file
    std::size_t end = lines.size();
    while (end > 0 && xstyle::is_blank(lines[end - 1])) --end;
    if (end < lines.size() && end > 0)
        issues.push_back(xstyle::check::make_issue(file, end, 0, "G-EOF-EMPTY", "Empty lines at the end of the file", "(empty lines removed)", xstyle::check::replace_lines(end, lines.size() - end, {})));

    std::size_t previous = NO_INDEX; // previous non blank line
    long brackets = 0; // open ( [ { at the start of the line (the C / C++ analysis gives its own parentheses)
    for (std::size_t i = 0; i < end; ++i) {
        const std::string& line = lines[i];
        const std::string indent = xstyle::indentation(line);

        // Tabs in the indentation (Makefile recipes need them)
        if (indent.find('\t') != std::string::npos && !(makefile && line[0] == '\t')) {
            std::string fixed;
            for (const char c: indent)
                fixed += c == '\t' ? "    " : std::string(1, c);
            fixed += line.substr(indent.size());
            issues.push_back(xstyle::check::make_issue(file, i, indent.find('\t'), "G-TAB", "Tab in the indentation", "", xstyle::check::replace_line(i, fixed)));
        }

        // Trailing whitespace (a Markdown line break is two trailing spaces)
        std::size_t last = line.find_last_not_of(" \t");
        if (!markdown && !line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            const std::string fixed = last == std::string::npos ? "" : line.substr(0, last + 1);
            issues.push_back(xstyle::check::make_issue(file, i, last == std::string::npos ? 0 : last + 1, "G-TRAILING", "Trailing whitespace", "", xstyle::check::replace_line(i, fixed)));
        }

        // Consecutive empty lines
        if (xstyle::is_blank(line) && i > 0 && xstyle::is_blank(lines[i - 1]) && (i < 2 || !xstyle::is_blank(lines[i - 2]))) {
            std::size_t run = i - 1;
            std::size_t stop = i;
            while (stop < end && xstyle::is_blank(lines[stop])) ++stop;
            issues.push_back(xstyle::check::make_issue(file, i, 0, "G-EMPTY-LINES", std::to_string(stop - run) + " consecutive empty lines", "(one empty line kept)",
                xstyle::check::replace_lines(run, stop - run, {""})));
        }

        // Tags of unfinished work in the comments
        std::smatch match;
        if ((comments[i].find("TODO") != std::string::npos || comments[i].find("FIXME") != std::string::npos || comments[i].find("XXX") != std::string::npos)
            && std::regex_search(comments[i], match, todo))
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), "G-TODO", match[1].str() + " tag in a comment",
                "describe the limitation in a normal comment (or open an issue)"));

        // Indentation multiple of 4 (code lines only, continuations are free)
        const std::string& masked = code[i];
        const bool open = file.isCLike() ? info[i].parenDepth > 0 : brackets > 0;
        for (const char c: masked)
            if (!file.isCLike()) brackets = std::max(0L, brackets + (c == '(' || c == '[' || c == '{' ? 1 : c == ')' || c == ']' || c == '}' ? -1 : 0));
        if (!xstyle::is_blank(masked) && !info[i].inComment && !info[i].preprocessor && !open && indent.find('\t') == std::string::npos
            && indent.size() % 4 != 0 && xstyle::rule_applies(*xstyle::find_rule("G-INDENT"), language)
            && (previous == NO_INDEX || !continued_(code[previous], masked))
            && file.scopeAt(i) != xstyle::Scope::Init && masked.find_first_not_of(' ') == indent.size()) {
            issues.push_back(xstyle::check::make_issue(file, i, 0, "G-INDENT", "Indentation of " + std::to_string(indent.size()) + " spaces (not a multiple of 4)",
                "indent with " + std::to_string(indent.size() / 4 * 4) + " or " + std::to_string(indent.size() / 4 * 4 + 4) + " spaces"));
        }
        if (!xstyle::is_blank(masked)) previous = i;
    }
}
