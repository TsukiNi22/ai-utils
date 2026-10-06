/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file XStyleType.hpp

File Description:
##  Types shared by every part of xstyle (rules, issues, fixes, options, project)
\**************************************************************/

#ifndef XSTYLETYPE_H
    #define XSTYLETYPE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include <unordered_set>    // std::unordered_set
    #include <string_view>      // std::string_view
    #include <filesystem>       // std::filesystem::path
    #include <optional>         // std::optional
    #include <cstdint>          // std::uint8_t
    #include <string>           // std::string
    #include <vector>           // std::vector

namespace xstyle { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class Severity: std::uint8_t {
    Negligible,
    Minor,
    Major,
    Unforgivable,
};

enum class Language: std::uint8_t {
    Cpp,
    C,
    Python,
    Shell,
    Lua,
    JavaScript,
    CMake,
    Makefile,
    Yaml,
    Json,
    Markdown,
    Other,
};

enum class FixKind: std::uint8_t {
    Replace, // replace [line, line + count) by lines
    FinalNewline, // add the missing final newline
    Crlf, // convert every CRLF to LF
};

enum class LinkMode: std::uint8_t {
    File, // file://<path> (OSC 8)
    Vscode, // vscode://file/<path>:<line>:<column>
    None,
};

enum class Format: std::uint8_t {
    Text,
    Markdown,
    Json,
};

//----------------------------------------------------------------//
/* STRUCT */

struct Rule {
    std::string_view code;
    xstyle::Severity severity = xstyle::Severity::Minor;
    bool fixable = false;
    std::string_view languages; // "*" = every language, else <lang>,<lang> (cpp, c, py, sh...)
    std::string_view description;
};

struct Fix {
    xstyle::FixKind kind = xstyle::FixKind::Replace;
    std::size_t line = 0; // first replaced line (0-based)
    std::size_t count = 1; // number of replaced lines (0 = insertion before line)
    std::vector<std::string> lines; // new content
};

struct Issue {
    std::string file; // path displayed (relative when possible)
    std::filesystem::path path; // absolute path (hyperlinks)
    std::size_t line = 0; // 1-based, 0 = whole file
    std::size_t column = 0; // 1-based, 0 = whole line
    std::string code;
    xstyle::Severity severity = xstyle::Severity::Minor;
    std::string message;
    std::string source; // line content (empty for whole file)
    std::string suggestion; // proposed fix, as text
    std::optional<xstyle::Fix> fix; // automatic fix (only for fixable rules)
};

struct AppliedFix {
    std::string code;
    std::size_t line = 0; // 1-based, before the fix
    std::vector<std::string> before;
    std::vector<std::string> after;
};

struct Migration {
    std::string from; // utils::network::socket::is_ip | isloaded (member)
    std::string to; // utils::network::is_ip | isLoaded (member)
    bool member = false; // called with . or ->
};

struct ProjectInfo {
    std::filesystem::path root; // git root, else the working directory
    bool libutilsInstalled = false;
    std::filesystem::path libutilsInclude; // <prefix>/include (contains utils/utils.hpp)
    std::string libutilsVersion = "[unknown]";
    bool libutilsUsed = false; // find_package(utils) or utils.hpp included by the project
    bool isLibutils = false; // the project is libutils itself
    std::vector<xstyle::Migration> migrations; // parsed from the installed headers
    std::unordered_set<std::string> attributeMacros; // _hot, _cold... (installed headers or fallback)
};

struct Options {
    std::vector<std::filesystem::path> paths;
    bool recursive = false;
    bool fix = false;
    bool dryRun = false;
    std::vector<std::string> fixCodes; // empty = every fixable code
    std::vector<std::string> codes; // only these codes / prefixes (empty = every code)
    std::vector<std::string> ignored; // never these codes / prefixes
    std::vector<std::string> excludes; // path patterns skipped (substring or * glob)
    std::vector<xstyle::Language> languages; // empty = every known language
    bool topOnly = false; // only the most used language
    xstyle::Severity minSeverity = xstyle::Severity::Negligible; // reported from this one
    xstyle::Severity failOn = xstyle::Severity::Negligible; // exit 1 from this one
    std::optional<std::filesystem::path> report; // report file
    std::optional<xstyle::Format> format; // report format (default: from the extension)
    bool summaryOnly = false;
    bool color = true;
    xstyle::LinkMode link = xstyle::LinkMode::File;
    std::string libutils = "auto"; // auto | on | off
};

} // namespace end
#endif /* XSTYLETYPE_H */
