/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Reporter.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#define _IOManip
#include <utils/utils.hpp>
#include "xstyle/Reporter.hpp"
#include "xstyle/Rules.hpp"
#include "xstyle/Tools.hpp"
#include <functional>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <set>

/* tools */
_cold static std::string json_escape_(const std::string& s)
{
    std::ostringstream out;
    for (const char c: s) {
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n";  break;
            case '\t': out << "\\t";  break;
            case '\r': out << "\\r";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
                else out << c;
                break;
        }
    }
    return out.str();
}

_cold static std::string markdown_escape_(const std::string& s)
{
    std::string out;
    for (const char c: s)
        out += c == '|' ? std::string("\\|") : c == '`' ? std::string("'") : std::string(1, c);
    return out;
}

_cold static std::string pad_(const std::string& s, const std::size_t width, const bool right = false)
{
    if (s.size() >= width) return s;
    return right ? std::string(width - s.size(), ' ') + s : s + std::string(width - s.size(), ' ');
}

_cold static std::string truncate_(const std::string& s, const std::size_t max)
{
    // Cut on a UTF-8 character boundary (never in the middle of a multi-byte character)
    std::size_t characters = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) continue;
        if (++characters > max) return s.substr(0, i) + "...";
    }
    return s;
}

/* setup */
_cold void xstyle::Reporter::addFile(const xstyle::SourceFile& file)
{
    ++this->_files;
    ++this->_languages[file.getLanguage()];
}

_cold void xstyle::Reporter::addFixes(const std::string& file, const std::vector<xstyle::AppliedFix>& fixes)
{
    for (const xstyle::AppliedFix& fix: fixes)
        this->_fixes.push_back({file, fix});
}

/* formatting */
_cold std::string xstyle::Reporter::paint_(const std::string& text, const utils::iomanip::Color color, const bool tty, const bool bold) const
{
    if (!tty) return text;
    return (bold ? utils::iomanip::strong() : "") + utils::iomanip::color(color) + text + utils::iomanip::reset();
}

_cold std::string xstyle::Reporter::severity_(const xstyle::Severity severity, const bool tty) const
{
    const std::string name = std::string(xstyle::severity_name(severity));
    switch (severity) {
        case xstyle::Severity::Unforgivable: return this->paint_(name, utils::iomanip::Color::BrightMagenta, tty, true);
        case xstyle::Severity::Major:        return this->paint_(name, utils::iomanip::Color::BrightRed, tty, true);
        case xstyle::Severity::Minor:        return this->paint_(name, utils::iomanip::Color::Yellow, tty);
        default:                             return this->paint_(name, utils::iomanip::Color::BrightBlack, tty);
    }
}

_cold std::string xstyle::Reporter::location_(const xstyle::Issue& issue, const bool tty) const
{
    std::string text = issue.file;
    if (issue.line > 0) text += ":" + std::to_string(issue.line) + ":" + std::to_string(issue.column);
    if (!tty || this->_options.link == xstyle::LinkMode::None) return text;

    const std::string absolute = issue.path.string();
    if (this->_options.link == xstyle::LinkMode::Vscode)
        return utils::iomanip::hyperlink(utils::iomanip::strong() + text + utils::iomanip::reset(), "vscode://file" + absolute + ":" + std::to_string(issue.line) + ":" + std::to_string(issue.column));
    return utils::iomanip::file_hyperlink(utils::iomanip::strong() + text + utils::iomanip::reset(), absolute);
}

_cold std::string xstyle::Reporter::issues_(const bool tty) const
{
    std::ostringstream out;
    std::string file;

    for (const xstyle::Issue& issue: this->_issues) {
        if (issue.file != file && !file.empty()) out << "\n";
        file = issue.file;
        std::string fixable = this->paint_("[manual]", utils::iomanip::Color::BrightBlack, tty);
        if (issue.mode == xstyle::FixMode::Auto) fixable = this->paint_("[auto-fix]", utils::iomanip::Color::Green, tty);
        if (issue.mode == xstyle::FixMode::Force) fixable = this->paint_("[force-fix]", utils::iomanip::Color::Yellow, tty);
        if (issue.mode == xstyle::FixMode::Dangerous) fixable = this->paint_("[danger-fix]", utils::iomanip::Color::BrightMagenta, tty);
        if (issue.mode == xstyle::FixMode::Ask) fixable = this->paint_("[ask-fix]", utils::iomanip::Color::Cyan, tty);
        out << this->location_(issue, tty) << " " << this->severity_(issue.severity, tty) << " " << this->paint_(issue.code, utils::iomanip::Color::Cyan, tty) << " " << fixable
            << " " << issue.message << "\n";
        if (issue.line > 0) {
            const std::string source = truncate_(issue.source, 157);
            out << this->paint_(pad_(std::to_string(issue.line), 7, true) + " | ", utils::iomanip::Color::BrightBlack, tty) << source << "\n";
        }
        if (issue.suggestion.empty()) continue;
        const std::vector<std::string> lines = [&]() {
            std::vector<std::string> l;
            std::istringstream stream(issue.suggestion);
            for (std::string s; std::getline(stream, s);)
                l.push_back(s);
            return l;
        }();
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const std::string label = i > 0 ? "" : issue.mode == xstyle::FixMode::Auto ? "fix" : issue.mode == xstyle::FixMode::Force ? "force"
                : issue.mode == xstyle::FixMode::Dangerous ? "danger" : "hint";
            out << this->paint_(pad_(label, 7, true) + " | ", utils::iomanip::Color::BrightBlack, tty)
                << this->paint_(lines[i], issue.fix ? utils::iomanip::Color::Green : utils::iomanip::Color::BrightBlue, tty) << "\n";
        }
    }
    return out.str();
}

_cold std::string xstyle::Reporter::fixes_(const bool tty) const
{
    std::ostringstream out;

    for (const auto &[file, fix]: this->_fixes) {
        out << this->paint_(file + ":" + std::to_string(fix.line), utils::iomanip::Color::Default, tty, true) << " " << this->paint_(fix.code, utils::iomanip::Color::Cyan, tty) << "\n";
        if (!this->_options.dryRun) continue;
        std::vector<std::string> before = fix.before;
        std::vector<std::string> after = fix.after;
        while (!before.empty() && !after.empty() && before.front() == after.front()) {
            before.erase(before.begin());
            after.erase(after.begin());
        }
        while (!before.empty() && !after.empty() && before.back() == after.back()) {
            before.pop_back();
            after.pop_back();
        }
        for (std::size_t i = 0; i < before.size() && i < 8; ++i)
            out << this->paint_("    - " + before[i], utils::iomanip::Color::Red, tty) << "\n";
        for (std::size_t i = 0; i < after.size() && i < 8; ++i)
            out << this->paint_("    + " + after[i], utils::iomanip::Color::Green, tty) << "\n";
        if (before.size() > 8 || after.size() > 8) out << "    ...\n";
    }
    return out.str();
}

_cold std::string xstyle::Reporter::summary_(const bool tty) const
{
    std::ostringstream out;
    using Counts = std::array<std::size_t, 5>; // <issues, auto, force, danger, ask>
    std::map<xstyle::Severity, Counts> severities;
    std::map<std::string, Counts> codes;
    Counts total = {};

    for (const xstyle::Issue& issue: this->_issues) {
        const std::size_t mode = issue.mode == xstyle::FixMode::Auto ? 1 : issue.mode == xstyle::FixMode::Force ? 2 : issue.mode == xstyle::FixMode::Dangerous ? 3
            : issue.mode == xstyle::FixMode::Ask ? 4 : 0;
        for (Counts* counts: {&severities[issue.severity], &codes[issue.code], &total}) {
            ++(*counts)[0];
            if (mode != 0) ++(*counts)[mode];
        }
    }
    const std::function<std::string(const Counts&)> columns = [](const Counts& c) {
        std::string text = pad_(std::to_string(c[0]), 8, true);
        for (std::size_t i = 1; i < c.size(); ++i)
            text += pad_(c[i] ? std::to_string(c[i]) : "-", 7, true);
        return text;
    };
    const std::string titles = pad_("Issues", 8, true) + pad_("Auto", 7, true) + pad_("Force", 7, true) + pad_("Danger", 7, true) + pad_("Ask", 7, true);

    // Files & context
    std::string languages;
    for (const auto &[language, count]: this->_languages)
        languages += (languages.empty() ? "" : ", ") + std::string(xstyle::language_name(language)) + " " + std::to_string(count);
    out << this->paint_("──────────────────────────── xstyle ────────────────────────────", utils::iomanip::Color::BrightBlack, tty) << "\n";
    out << this->paint_("Files     ", utils::iomanip::Color::Default, tty, true) << this->_files << " checked" << (languages.empty() ? "" : " (" + languages + ")")
        << (this->topLanguage().empty() ? "" : " - top language: " + this->topLanguage()) << "\n";
    out << this->paint_("libutils  ", utils::iomanip::Color::Default, tty, true);
    if (!this->_project.libutilsInstalled) out << "not installed";
    else if (this->_project.isLibutils) out << this->_project.libutilsVersion << " installed, the project is libutils itself";
    else out << this->_project.libutilsVersion << " installed, " << (this->_project.libutilsUsed ? "used" : "not used") << " by the project";
    out << " - libutils rules " << (this->_libutils ? "on" : "off") << "\n";
    if (this->_options.fix) {
        std::map<std::string, std::size_t> files;
        for (const auto &[file, fix]: this->_fixes)
            ++files[file];
        out << this->paint_("Fixed     ", utils::iomanip::Color::Default, tty, true) << this->_fixes.size() << " issue(s) in " << files.size() << " file(s)"
            << (this->_options.dryRun ? " (dry run: nothing written, the counters below are the issues left after the fixes)" : "") << "\n";
    }
    out << "\n";

    // Counters by severity
    out << this->paint_(pad_("Severity", 14) + titles, utils::iomanip::Color::Default, tty, true) << "\n";
    for (const xstyle::Severity severity: {xstyle::Severity::Unforgivable, xstyle::Severity::Major, xstyle::Severity::Minor, xstyle::Severity::Negligible}) {
        const std::string name = std::string(xstyle::severity_name(severity));
        out << this->severity_(severity, tty) << std::string(14 - name.size(), ' ') << columns(severities[severity]) << "\n";
    }
    out << this->paint_(pad_("total", 14) + columns(total), utils::iomanip::Color::Default, tty, true) << "\n";

    // Counters by code
    if (!codes.empty()) {
        out << "\n" << this->paint_(pad_("Code", 24) + pad_("Severity", 14) + titles, utils::iomanip::Color::Default, tty, true) << "\n";
        std::vector<std::pair<std::string, Counts>> sorted(codes.begin(), codes.end());
        std::stable_sort(sorted.begin(), sorted.end(), [](const std::pair<std::string, Counts>& a, const std::pair<std::string, Counts>& b) {
            const xstyle::Rule* ra = xstyle::find_rule(a.first);
            const xstyle::Rule* rb = xstyle::find_rule(b.first);
            return ra->severity != rb->severity ? ra->severity > rb->severity : a.second[0] > b.second[0];
        });
        for (const auto &[code, counts]: sorted) {
            const xstyle::Severity severity = xstyle::find_rule(code)->severity;
            const std::string name = std::string(xstyle::severity_name(severity));
            out << this->paint_(pad_(code, 24), utils::iomanip::Color::Cyan, tty) << this->severity_(severity, tty) << std::string(14 - name.size(), ' ')
                << columns(counts) << "\n";
        }
    }
    if (total[1] + total[2] + total[3] + total[4] > 0) out << "\n";
    if (total[1] > 0 && !this->_options.fix)
        out << this->paint_("Auto   xstyle --fix [CODE,...] [paths] (-n to preview)", utils::iomanip::Color::Green, tty) << "\n";
    if (total[2] > 0 && !(this->_options.fix && this->_options.force))
        out << this->paint_("Force  xstyle --fix --force: also the fixes that can change the behavior (check them with -n)", utils::iomanip::Color::Yellow, tty) << "\n";
    if (total[3] > 0 && !(this->_options.fix && this->_options.dangerous))
        out << this->paint_("Danger xstyle --fix --dangerous-force: also the guessed fixes, possibly wrong (check them with -n)", utils::iomanip::Color::BrightMagenta, tty) << "\n";
    if (total[4] > 0)
        out << this->paint_("Ask    xstyle --fix asks the choice per file (or --header none|default|<text>)", utils::iomanip::Color::Cyan, tty) << "\n";
    if (this->_issues.empty()) out << "\n" << this->paint_("Clean: no issue found", utils::iomanip::Color::Green, tty, true) << "\n";
    return out.str();
}

_cold std::string xstyle::Reporter::markdown_(void) const
{
    std::ostringstream out;
    std::map<xstyle::Severity, std::size_t> severities;
    for (const xstyle::Issue& issue: this->_issues)
        ++severities[issue.severity];

    out << "# xstyle report\n\n";
    out << "Project: `" << this->_project.root.string() << "` - " << this->_files << " files checked";
    if (!this->topLanguage().empty()) out << " - top language: " << this->topLanguage();
    out << "\n\n";
    out << "| Severity | Issues |\n|---|---|\n";
    for (const xstyle::Severity severity: {xstyle::Severity::Unforgivable, xstyle::Severity::Major, xstyle::Severity::Minor, xstyle::Severity::Negligible})
        out << "| " << xstyle::severity_name(severity) << " | " << severities[severity] << " |\n";
    out << "| **total** | **" << this->_issues.size() << "** |\n\n";
    if (this->_options.fix) out << "Fixed: " << this->_fixes.size() << " issue(s)" << (this->_options.dryRun ? " (dry run)" : "") << "\n\n";

    std::string file;
    for (const xstyle::Issue& issue: this->_issues) {
        if (issue.file != file) {
            file = issue.file;
            out << "\n## `" << file << "`\n\n| Line | Severity | Code | Fix | Issue | Fix / hint |\n|---|---|---|---|---|---|\n";
        }
        std::string suggestion = issue.suggestion;
        std::replace(suggestion.begin(), suggestion.end(), '\n', ' ');
        std::error_code error;
        const std::filesystem::path base = std::filesystem::absolute(*this->_options.report, error).parent_path();
        const std::string link = std::filesystem::proximate(issue.path, base, error).generic_string();
        out << "| [" << issue.line << "](" << link << "#L" << issue.line << ") | " << xstyle::severity_name(issue.severity) << " | `" << issue.code << "` | " << xstyle::fix_mode_name(issue.mode) << " | "
            << markdown_escape_(issue.message) << " | " << (suggestion.empty() ? "" : "`" + markdown_escape_(suggestion) + "`") << " |\n";
    }
    return out.str();
}

_cold std::string xstyle::Reporter::json_(void) const
{
    std::ostringstream out;
    std::map<xstyle::Severity, std::size_t> severities;
    for (const xstyle::Issue& issue: this->_issues)
        ++severities[issue.severity];

    out << "{\n    \"root\": \"" << json_escape_(this->_project.root.string()) << "\",\n";
    out << "    \"files\": " << this->_files << ",\n";
    out << "    \"top_language\": \"" << json_escape_(this->topLanguage()) << "\",\n";
    out << "    \"libutils\": {\"installed\": " << (this->_project.libutilsInstalled ? "true" : "false") << ", \"version\": \"" << json_escape_(this->_project.libutilsVersion)
        << "\", \"used\": " << (this->_project.libutilsUsed ? "true" : "false") << ", \"rules\": " << (this->_libutils ? "true" : "false") << "},\n";
    out << "    \"summary\": {";
    for (const xstyle::Severity severity: {xstyle::Severity::Unforgivable, xstyle::Severity::Major, xstyle::Severity::Minor, xstyle::Severity::Negligible})
        out << "\"" << xstyle::severity_name(severity) << "\": " << severities[severity] << ", ";
    out << "\"total\": " << this->_issues.size() << ", \"fixed\": " << this->_fixes.size() << "},\n";
    out << "    \"issues\": [";
    for (std::size_t i = 0; i < this->_issues.size(); ++i) {
        const xstyle::Issue& issue = this->_issues[i];
        out << (i == 0 ? "\n" : ",\n") << "        {\"file\": \"" << json_escape_(issue.file) << "\", \"line\": " << issue.line << ", \"column\": " << issue.column
            << ", \"code\": \"" << issue.code << "\", \"severity\": \"" << xstyle::severity_name(issue.severity) << "\", \"message\": \"" << json_escape_(issue.message)
            << "\", \"source\": \"" << json_escape_(issue.source) << "\", \"suggestion\": \"" << json_escape_(issue.suggestion) << "\", \"fixable\": " << (issue.mode == xstyle::FixMode::Auto ? "true" : "false")
            << ", \"fix\": \"" << (issue.mode == xstyle::FixMode::Manual ? "manual" : std::string(xstyle::fix_mode_name(issue.mode))) << "\"}";
    }
    out << (this->_issues.empty() ? "]\n}\n" : "\n    ]\n}\n");
    return out.str();
}

/* output */
_cold std::string xstyle::Reporter::rtk_(void) const
{
    // The strict minimum for an AI: the file once, one short line per issue (no source: the file can be read), what is the
    // same for every issue of a rule written once at the end, one summary line
    static const std::string severities = "nmMU"; // negligible, minor, Major, Unforgivable
    static const std::string modes = "-afdq"; // manual, auto, force, dangerous, ask
    const std::function<std::string(const xstyle::Issue&)> shortHint = [](const xstyle::Issue& issue) {
        std::string hint = xstyle::trim(issue.suggestion.substr(0, issue.suggestion.find('\n')));
        const std::size_t extra = static_cast<std::size_t>(std::count(issue.suggestion.begin(), issue.suggestion.end(), '\n'));
        return truncate_(hint, 110) + (extra > 0 ? " (+" + std::to_string(extra) + " lines)" : "");
    };
    std::map<std::string, std::pair<std::set<std::string>, std::set<std::string>>> variants; // code -> <messages, hints>
    for (const xstyle::Issue& issue: this->_issues) {
        variants[issue.code].first.insert(issue.message);
        variants[issue.code].second.insert(shortHint(issue));
    }
    std::ostringstream out;
    std::array<std::size_t, 4> bySeverity = {};
    std::array<std::size_t, 5> byMode = {};
    std::string file;

    if (!this->_options.summaryOnly && !this->_issues.empty())
        out << "# >file then line:col CODE severity(U/M/m/n) fix(a=auto f=--force d=--dangerous-force q=asked -=manual) [message] [=> fix|hint]; "
            << "message / hint shared by a rule: '* CODE' lines at the end\n";
    for (const xstyle::Issue& issue: this->_issues) {
        ++bySeverity[static_cast<std::size_t>(issue.severity)];
        ++byMode[static_cast<std::size_t>(issue.mode)];
        if (this->_options.summaryOnly) continue;
        if (issue.file != file) {
            file = issue.file;
            out << ">" << file << "\n";
        }
        const std::string hint = shortHint(issue);
        out << (issue.line > 0 ? std::to_string(issue.line) + ":" + std::to_string(issue.column) : "0") << " " << issue.code << " "
            << severities[static_cast<std::size_t>(issue.severity)] << " " << modes[static_cast<std::size_t>(issue.mode)]
            << (variants[issue.code].first.size() > 1 ? " " + issue.message : "")
            << (variants[issue.code].second.size() > 1 && !hint.empty() ? " => " + hint : "") << "\n";
    }
    if (!this->_options.summaryOnly)
        for (const auto &[code, texts]: variants) {
            const std::string message = texts.first.size() == 1 ? *texts.first.begin() : "";
            const std::string hint = texts.second.size() == 1 ? *texts.second.begin() : "";
            if (!message.empty() || !hint.empty()) out << "* " << code << (message.empty() ? "" : " " + message) << (hint.empty() ? "" : " => " + hint) << "\n";
        }
    if (!this->_fixes.empty()) {
        std::map<std::string, std::size_t> codes;
        for (const auto &[fixedFile, fix]: this->_fixes)
            ++codes[fix.code];
        out << (this->_options.dryRun ? "would fix " : "fixed ") << this->_fixes.size() << ":";
        for (const auto &[code, count]: codes)
            out << " " << code << "x" << count;
        out << "\n";
    }
    out << "= " << this->_issues.size() << " issues in " << this->_files << " files: U" << bySeverity[3] << " M" << bySeverity[2] << " m" << bySeverity[1]
        << " n" << bySeverity[0] << " | fix a" << byMode[1] << " f" << byMode[2] << " d" << byMode[3] << " q" << byMode[4] << " -" << byMode[0];
    if (byMode[1] > 0 && !this->_options.fix) out << " | xstyle --fix";
    return out.str() + "\n";
}

_cold void xstyle::Reporter::print(std::ostream& out) const
{
    if (this->_options.rtk) {
        out << this->rtk_();
        return;
    }
    const bool tty = this->_options.color && ::isatty(::fileno(stdout)) && !std::getenv("NO_COLOR");

    if (!this->_fixes.empty()) out << this->paint_(this->_options.dryRun ? "Would fix:" : "Fixed:", utils::iomanip::Color::Green, tty, true) << "\n" << this->fixes_(tty) << "\n";
    if (!this->_options.summaryOnly && !this->_issues.empty()) out << this->issues_(tty) << "\n";
    out << this->summary_(tty);
}

_cold void xstyle::Reporter::write(void) const
{
    if (!this->_options.report) return;
    const std::filesystem::path& path = *this->_options.report;
    xstyle::Format format = xstyle::Format::Text;
    if (this->_options.format) format = *this->_options.format;
    else if (path.extension() == ".md") format = xstyle::Format::Markdown;
    else if (path.extension() == ".json") format = xstyle::Format::Json;

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, "Can't write the report " + path.string());
    switch (format) {
        case xstyle::Format::Markdown: file << this->markdown_(); break;
        case xstyle::Format::Json:     file << this->json_();     break;
        default:
            if (!this->_fixes.empty()) file << (this->_options.dryRun ? "Would fix:\n" : "Fixed:\n") << this->fixes_(false) << "\n";
            file << this->issues_(false) << "\n" << this->summary_(false);
            break;
    }
}

_cold int xstyle::Reporter::exitCode(void) const
{
    for (const xstyle::Issue& issue: this->_issues)
        if (issue.severity >= this->_options.failOn) return 1;
    return OK;
}

_cold std::string xstyle::Reporter::topLanguage(void) const
{
    // Code languages only (documentation / configuration files never lead)
    xstyle::Language top = xstyle::Language::Other;
    std::size_t best = 0;
    for (const auto &[language, count]: this->_languages) {
        if (language == xstyle::Language::Markdown || language == xstyle::Language::Json || language == xstyle::Language::Yaml
            || language == xstyle::Language::CMake || language == xstyle::Language::Makefile || count <= best) continue;
        top = language;
        best = count;
    }
    return best == 0 ? "" : std::string(xstyle::language_name(top));
}

/* constructor */
xstyle::Reporter::Reporter(const xstyle::Options& options, const xstyle::ProjectInfo& project, const bool libutils)
: _options(options), _project(project), _libutils(libutils)
{
    /* Nothing */
}
