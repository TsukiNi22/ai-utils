/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Checker.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/check/Checks.hpp"
#include "xstyle/Checker.hpp"
#include "xstyle/Rules.hpp"
#include "xstyle/Tools.hpp"
#include <algorithm>
#include <regex>

/* selection */
_hot bool xstyle::Checker::selected_(std::string_view code, const bool fix) const
{
    const std::vector<std::string>& codes = fix && !this->_options.fixCodes.empty() ? this->_options.fixCodes : this->_options.codes;

    for (const std::string& pattern: this->_options.ignored)
        if (xstyle::match_code(code, pattern)) return false;
    if (fix && !this->_options.fixCodes.empty() && !this->_options.codes.empty()
        && std::none_of(this->_options.codes.begin(), this->_options.codes.end(), [&](const std::string& p) {return xstyle::match_code(code, p);})) return false;
    if (codes.empty()) return true;
    return std::any_of(codes.begin(), codes.end(), [&](const std::string& p) {return xstyle::match_code(code, p);});
}

_hot bool xstyle::Checker::suppressed_(const xstyle::SourceFile& file, const xstyle::Issue& issue) const
{
    // xstyle: ignore [CODE, ...] on the line, xstyle: ignore-next [CODE, ...] on the line above, xstyle: ignore-file [CODE, ...] anywhere
    static const std::regex directive(R"(xstyle:\s*(ignore-file|ignore-next|ignore)\b([A-Z0-9*,\s-]*))");
    static const std::regex spaces(R"(\s+)");
    const xstyle::Language language = file.getLanguage();
    const bool masked = language != xstyle::Language::Json && language != xstyle::Language::Markdown && language != xstyle::Language::Other;
    const std::vector<std::string>& text = masked ? file.getComments() : file.getLines();

    for (std::size_t i = 0; i < text.size(); ++i) {
        std::smatch match;
        if (text[i].find("xstyle:") == std::string::npos || !std::regex_search(text[i], match, directive)) continue;
        const std::string kind = match[1].str();
        const bool applies = kind == "ignore-file" || (kind == "ignore" && i + 1 == issue.line) || (kind == "ignore-next" && i + 2 == issue.line);
        if (!applies) continue;
        const std::vector<std::string> codes = xstyle::split(std::regex_replace(match[2].str(), spaces, ","), ',');
        if (codes.empty() || std::any_of(codes.begin(), codes.end(), [&](const std::string& p) {return xstyle::match_code(issue.code, p);})) return true;
    }
    return false;
}

_hot std::vector<xstyle::Issue> xstyle::Checker::run_(const xstyle::SourceFile& file, const bool fix) const
{
    xstyle::check::Issues issues;
    const xstyle::Language language = file.getLanguage();

    // A template ({{NAME}} placeholders) is not valid code yet: only the generic rules
    static const std::regex placeholder(R"(\{\{[A-Z][A-Z0-9_]*\}\})");
    const bool templateFile = std::any_of(file.getLines().begin(), file.getLines().end(), [](const std::string& line) {
        return line.find("{{") != std::string::npos && std::regex_search(line, placeholder);
    });

    xstyle::check::generic(file, issues);
    if (!templateFile) {
        if (file.isCLike()) xstyle::check::cpp(file, this->_project, issues);
        if (language == xstyle::Language::Cpp && this->_libutils) xstyle::check::libutils(file, this->_project, issues);
        if (language == xstyle::Language::Python) xstyle::check::python(file, issues);
        if (language == xstyle::Language::Shell) xstyle::check::shell(file, issues);
        if (language == xstyle::Language::Rust) xstyle::check::rust(file, issues);
    }

    // Selection: language of the rule, codes, severity, suppression comments
    std::erase_if(issues, [&](const xstyle::Issue& issue) {
        const xstyle::Rule* rule = xstyle::find_rule(issue.code);
        return !rule || !xstyle::rule_applies(*rule, language) || !this->selected_(issue.code, fix) || issue.severity < this->_options.minSeverity
            || (!this->_options.modes.empty() && std::find(this->_options.modes.begin(), this->_options.modes.end(), issue.mode) == this->_options.modes.end())
            || this->suppressed_(file, issue);
    });
    std::stable_sort(issues.begin(), issues.end(), [](const xstyle::Issue& a, const xstyle::Issue& b) {return a.line != b.line ? a.line < b.line : a.column < b.column;});
    return issues;
}

/* fix */
_hot std::vector<xstyle::AppliedFix> xstyle::Checker::fix(xstyle::SourceFile& file) const
{
    std::vector<xstyle::AppliedFix> applied;

    for (std::size_t pass = 0; pass < FIX_PASSES; ++pass) {
        // Non-overlapping fixes of this pass (the others come back on the next pass)
        std::vector<xstyle::Fix> fixes;
        std::vector<std::pair<std::size_t, std::size_t>> ranges;
        for (const xstyle::Issue& issue: this->run_(file, true)) {
            if (!issue.fix || (issue.fix->unsafe && !this->_options.force) || (issue.fix->dangerous && !this->_options.dangerous)) continue;
            const xstyle::Fix& fix = *issue.fix;
            xstyle::AppliedFix record{issue.code, issue.line, {}, fix.lines};
            if (fix.kind != xstyle::FixKind::Replace) {
                record.before = {fix.kind == xstyle::FixKind::Crlf ? "(CRLF line endings)" : "(no final newline)"};
                record.after = {fix.kind == xstyle::FixKind::Crlf ? "(LF line endings)" : "(final newline)"};
                fixes.push_back(fix);
                applied.push_back(record);
                continue;
            }
            const std::size_t first = fix.line;
            const std::size_t last = fix.line + std::max<std::size_t>(fix.count, 1);
            if (std::any_of(ranges.begin(), ranges.end(), [&](const std::pair<std::size_t, std::size_t>& r) {return first < r.second + 1 && r.first < last + 1;})) continue;
            const std::vector<std::string>& lines = file.getLines();
            record.before.assign(lines.begin() + static_cast<std::ptrdiff_t>(std::min(first, lines.size())), lines.begin() + static_cast<std::ptrdiff_t>(std::min(first + fix.count, lines.size())));
            if (record.before == record.after) continue; // no-op: would loop forever
            ranges.push_back({first, last});
            fixes.push_back(fix);
            applied.push_back(record);
        }
        if (fixes.empty()) break;
        file.apply(fixes);
    }
    return applied;
}

/* constructor */
xstyle::Checker::Checker(const xstyle::Options& options, const xstyle::ProjectInfo& project)
: _options(options), _project(project)
{
    if (this->_options.libutils == "on") this->_libutils = true;
    else if (this->_options.libutils == "off") this->_libutils = false;
    else this->_libutils = this->_project.libutilsInstalled && this->_project.libutilsUsed;
}
