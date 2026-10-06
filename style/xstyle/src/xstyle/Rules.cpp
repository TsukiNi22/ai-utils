/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file Rules.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/Rules.hpp"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

/* catalogue */
_cold const std::vector<xstyle::Rule>& xstyle::rules(void)
{
    static const std::vector<xstyle::Rule> catalogue = {
        // Generic (every language)
        {"G-CRLF",             xstyle::Severity::Major,        true,  "*",            "Windows line endings (CRLF), use LF"},
        {"G-TAB",              xstyle::Severity::Major,        true,  "*",            "Tab used for the indentation, use 4 spaces (except Makefile recipes)"},
        {"G-TRAILING",         xstyle::Severity::Minor,        true,  "*",            "Trailing whitespace"},
        {"G-EMPTY-LINES",      xstyle::Severity::Minor,        true,  "*",            "Two or more consecutive empty lines, one is the maximum"},
        {"G-EOF-NEWLINE",      xstyle::Severity::Negligible,   true,  "*",            "No newline at the end of the file"},
        {"G-EOF-EMPTY",        xstyle::Severity::Negligible,   true,  "*",            "Empty lines at the end of the file"},
        {"G-TODO",             xstyle::Severity::Minor,        false, "*",            "TODO / FIXME / XXX tag, describe the limitation in a normal comment instead"},
        {"G-INDENT",           xstyle::Severity::Negligible,   false, "cpp,c,py,sh,lua,js,cmake", "Indentation that is not a multiple of 4 spaces"},

        // C / C++
        {"CPP-USING-NAMESPACE", xstyle::Severity::Unforgivable, false, "cpp",         "using namespace, always write the fully qualified names"},
        {"CPP-GUARD-MISSING",  xstyle::Severity::Major,        true,  "cpp,c",        "Header without include guard (#ifndef NAME_H / #define NAME_H)"},
        {"CPP-PRAGMA-ONCE",    xstyle::Severity::Minor,        true,  "cpp,c",        "#pragma once instead of the include guard NAME_H"},
        {"CPP-GUARD-NAME",     xstyle::Severity::Minor,        true,  "cpp,c",        "Include guard not named after the file (Name.hpp -> NAME_H)"},
        {"CPP-ENDIF-COMMENT",  xstyle::Severity::Negligible,   true,  "cpp,c",        "Guard #endif without its /* NAME_H */ comment"},
        {"CPP-PREPRO-INDENT",  xstyle::Severity::Negligible,   true,  "cpp,c",        "Preprocessor directive nested in a #if / guard not indented by 4 per level"},
        {"CPP-HEADER",         xstyle::Severity::Minor,        false, "cpp,c",        "Missing file header (cpp-class scripts/header.py)"},
        {"CPP-HEADER-FILE",    xstyle::Severity::Negligible,   true,  "cpp,c",        "@file of the header is not the name of the file"},
        {"CPP-NULL",           xstyle::Severity::Major,        true,  "cpp",          "NULL instead of nullptr"},
        {"CPP-C-CAST",         xstyle::Severity::Major,        true,  "cpp",          "C cast, use static_cast (only (void) is allowed)"},
        {"CPP-AUTO",           xstyle::Severity::Major,        false, "cpp",          "auto, write the type (allowed: iterators, structured bindings)"},
        {"CPP-USING-STD",      xstyle::Severity::Major,        false, "cpp",          "using std::x, always write std::x"},
        {"CPP-TYPEDEF",        xstyle::Severity::Minor,        true,  "cpp",          "typedef, use using Name = Type"},
        {"CPP-BRACE-FUNCTION", xstyle::Severity::Major,        true,  "cpp,c",        "Function opening brace on the signature line, it goes on its own line"},
        {"CPP-BRACE-OWN-LINE", xstyle::Severity::Major,        true,  "cpp,c",        "Opening brace of a class / namespace / control block on its own line, it goes on the same line"},
        {"CPP-ELSE-LINE",      xstyle::Severity::Minor,        true,  "cpp,c",        "else / catch not on the closing brace line (} else {)"},
        {"CPP-KEYWORD-SPACE",  xstyle::Severity::Minor,        true,  "cpp,c",        "No space between if / for / while / switch / catch and the parenthesis"},
        {"CPP-PAREN-SPACE",    xstyle::Severity::Negligible,   true,  "cpp,c",        "Space inside the parentheses of a control statement"},
        {"CPP-COLON-SPACE",    xstyle::Severity::Negligible,   true,  "cpp",          "Space before ':' of a range-for, an inheritance or an enum base"},
        {"CPP-PTR-REF",        xstyle::Severity::Minor,        true,  "cpp,c",        "& / * glued to the name instead of the type (const T& name, char* buf)"},
        {"CPP-VOID-PARAM",     xstyle::Severity::Minor,        true,  "cpp,c",        "Empty parameter list, write (void)"},
        {"CPP-THIS",           xstyle::Severity::Minor,        true,  "cpp",          "Member accessed without this->"},
        {"CPP-NAMESPACE-NESTED", xstyle::Severity::Minor,      false, "cpp",          "Nested namespace blocks, use namespace a::b {"},
        {"CPP-NAMESPACE-COMMENT", xstyle::Severity::Negligible, true, "cpp",          "Namespace block without // namespace start / // namespace end"},
        {"CPP-NAMESPACE-INDENT", xstyle::Severity::Minor,      false, "cpp",          "Content of a namespace indented (it is not)"},
        {"CPP-INCLUDE-ORDER",  xstyle::Severity::Minor,        true,  "cpp,c",        "Include order: personal libs (+ defines), project, external; longest name first"},
        {"CPP-CLASS-NAME",     xstyle::Severity::Major,        false, "cpp",          "Class / struct / enum not in PascalCase"},
        {"CPP-MEMBER-NAME",    xstyle::Severity::Minor,        false, "cpp",          "Class data member not named _camelCase"},
        {"CPP-MACRO-NAME",     xstyle::Severity::Minor,        false, "cpp,c",        "Macro not in UPPER_SNAKE (or _lower for attribute / section macros)"},
        {"CPP-ONE-LINER",      xstyle::Severity::Negligible,   true,  "cpp",          "One-liner body not written {statement};"},
        {"CPP-ACCESS-ORDER",   xstyle::Severity::Negligible,   false, "cpp",          "public before private / protected in a class"},
        {"CPP-SINGLE-STATEMENT", xstyle::Severity::Minor,      false, "cpp",          "Single statement function defined in the .cpp, define it inline in the header"},
        {"CPP-DOXYGEN",        xstyle::Severity::Minor,        false, "cpp,c",        "Doxygen comment (@brief, @param, ///, /**), short plain comments only"},

        // libutils (installed and used by the project)
        {"LU-INCLUDE",         xstyle::Severity::Major,        true,  "cpp",          "libutils header included directly, use #define _Section + <utils/utils.hpp>"},
        {"LU-BARE-INCLUDE",    xstyle::Severity::Major,        true,  "cpp",          "<utils/utils.hpp> without any #define _Section before it"},
        {"LU-SECTION",         xstyle::Severity::Minor,        true,  "cpp",          "libutils section used without its #define before <utils/utils.hpp>"},
        {"LU-ATTRIBUTE",       xstyle::Severity::Minor,        true,  "cpp",          "Standard attribute with a libutils macro equivalent ([[nodiscard]] -> _nodiscard)"},
        {"LU-MIGRATION",       xstyle::Severity::Major,        true,  "cpp",          "Deprecated libutils name (migration alias), use the new name"},
        {"LU-EXCEPTION",       xstyle::Severity::Major,        false, "cpp",          "Standard exception thrown, use utils::exception::ErrorException"},
        {"LU-ARGS",            xstyle::Severity::Minor,        false, "cpp",          "Arguments parsed by hand, use utils::arguments::ArgParser"},
        {"LU-ANSI",            xstyle::Severity::Minor,        false, "cpp",          "Raw ANSI escape sequence, use utils::iomanip"},
        {"LU-SOCKET",          xstyle::Severity::Minor,        false, "cpp",          "Raw BSD socket, use utils::network (TCPSocket, Server, Client)"},
        {"LU-SYSCALL",         xstyle::Severity::Minor,        false, "cpp",          "Raw pipe / dup / fork / epoll / shm / dlopen, use utils::encapsulation"},
        {"LU-THREADS",         xstyle::Severity::Minor,        false, "cpp",          "Hand made thread pool, use utils::pool::Cluster or utils::system::Scheduler"},
        {"LU-ANGLE",           xstyle::Severity::Minor,        false, "cpp",          "Degree / radian conversion by hand, use utils::math::trigo"},
        {"LU-BASE64",          xstyle::Severity::Minor,        false, "cpp",          "Hand made base64, use utils::smanip::codec::Base64Codec"},
        {"LU-VERBOSE",         xstyle::Severity::Minor,        false, "cpp",          "Hand made verbose flag, use utils::verbose (set_verbose, onBasicVerbose)"},
        {"LU-DISTANCE",        xstyle::Severity::Minor,        false, "cpp",          "Hand made string distance, use utils::algorithms::c2dmp"},

        // Python
        {"PY-WILDCARD-IMPORT", xstyle::Severity::Major,        false, "py",           "from x import *, import the names explicitly"},
        {"PY-BARE-EXCEPT",     xstyle::Severity::Major,        false, "py",           "Bare except:, catch the exceptions expected"},
        {"PY-TYPE-HINTS",      xstyle::Severity::Minor,        false, "py",           "Function without type hints on its parameters / return"},
        {"PY-PRINT-ERROR",     xstyle::Severity::Minor,        false, "py",           "print(..., file=stderr), use stderr.write"},
        {"PY-OPEN-ENCODING",   xstyle::Severity::Minor,        false, "py",           "open() of a text file without encoding=\"utf-8\""},

        // Shell
        {"SH-SHEBANG",         xstyle::Severity::Minor,        false, "sh",           "Script without shebang (#!/bin/bash)"},
        {"SH-STRICT",          xstyle::Severity::Minor,        false, "sh",           "Bash script without set -euo pipefail"},
        {"SH-TEST",            xstyle::Severity::Negligible,   false, "sh",           "[ ] test in a bash script, use [[ ]]"},
    };
    return catalogue;
}

_cold const xstyle::Rule* xstyle::find_rule(std::string_view code)
{
    for (const xstyle::Rule& rule: xstyle::rules())
        if (rule.code == code) return &rule;
    return nullptr;
}

_cold bool xstyle::rule_applies(const xstyle::Rule& rule, const xstyle::Language language)
{
    if (rule.languages == "*") return true;

    std::string_view id = xstyle::language_id(language);
    std::string_view list = rule.languages;
    while (!list.empty()) {
        std::size_t comma = list.find(',');
        if (list.substr(0, comma) == id) return true;
        if (comma == std::string_view::npos) break;
        list.remove_prefix(comma + 1);
    }
    return false;
}

_cold bool xstyle::match_code(std::string_view code, std::string_view pattern)
{
    if (pattern == "*" || pattern == code) return true;
    if (pattern.ends_with('*')) return code.starts_with(pattern.substr(0, pattern.size() - 1));
    // CPP -> every CPP-*, CPP-NAMESPACE -> every CPP-NAMESPACE-*
    return code.size() > pattern.size() && code.starts_with(pattern) && code[pattern.size()] == '-';
}

/* names */
_cold std::string_view xstyle::severity_name(const xstyle::Severity severity)
{
    switch (severity) {
        case xstyle::Severity::Unforgivable: return "unforgivable";
        case xstyle::Severity::Major:        return "major";
        case xstyle::Severity::Minor:        return "minor";
        default:                             return "negligible";
    }
}

_cold std::optional<xstyle::Severity> xstyle::parse_severity(const std::string& name)
{
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});

    if (lower == "unforgivable" || lower == "u" || lower == "i") return xstyle::Severity::Unforgivable;
    if (lower == "major" || lower == "ma" || lower == "m") return xstyle::Severity::Major;
    if (lower == "minor" || lower == "mi") return xstyle::Severity::Minor;
    if (lower == "negligible" || lower == "n") return xstyle::Severity::Negligible;
    return std::nullopt;
}

_cold std::string_view xstyle::language_name(const xstyle::Language language)
{
    switch (language) {
        case xstyle::Language::Cpp:        return "C++";
        case xstyle::Language::C:          return "C";
        case xstyle::Language::Python:     return "Python";
        case xstyle::Language::Shell:      return "Shell";
        case xstyle::Language::Lua:        return "Lua";
        case xstyle::Language::JavaScript: return "JavaScript / TypeScript";
        case xstyle::Language::CMake:      return "CMake";
        case xstyle::Language::Makefile:   return "Makefile";
        case xstyle::Language::Yaml:       return "YAML";
        case xstyle::Language::Json:       return "JSON";
        case xstyle::Language::Markdown:   return "Markdown";
        default:                           return "Other";
    }
}

_cold std::string_view xstyle::language_id(const xstyle::Language language)
{
    switch (language) {
        case xstyle::Language::Cpp:        return "cpp";
        case xstyle::Language::C:          return "c";
        case xstyle::Language::Python:     return "py";
        case xstyle::Language::Shell:      return "sh";
        case xstyle::Language::Lua:        return "lua";
        case xstyle::Language::JavaScript: return "js";
        case xstyle::Language::CMake:      return "cmake";
        case xstyle::Language::Makefile:   return "make";
        case xstyle::Language::Yaml:       return "yaml";
        case xstyle::Language::Json:       return "json";
        case xstyle::Language::Markdown:   return "md";
        default:                           return "other";
    }
}

_cold std::optional<xstyle::Language> xstyle::parse_language(const std::string& name)
{
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char c) {return static_cast<char>(std::tolower(c));});

    if (lower == "c++" || lower == "cxx") lower = "cpp";
    if (lower == "python") lower = "py";
    if (lower == "shell" || lower == "bash") lower = "sh";
    if (lower == "ts" || lower == "javascript" || lower == "typescript") lower = "js";
    if (lower == "makefile") lower = "make";
    if (lower == "yml") lower = "yaml";
    if (lower == "markdown") lower = "md";
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(xstyle::Language::Other); ++i) {
        xstyle::Language language = static_cast<xstyle::Language>(i);
        if (xstyle::language_id(language) == lower) return language;
    }
    return std::nullopt;
}
