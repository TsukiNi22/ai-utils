/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file typing.cpp

File Description:
##  Type inference of the Python checks: certain types (literals, builtins, displays) and guesses (usage, names)
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/check/Checks.hpp"
#include "xstyle/Tools.hpp"
#include <unordered_map>
#include <unordered_set>
#include <regex>

/* tools */
_hot static std::size_t top_level_(const std::string& e, const std::string& token) // first position of token outside of any bracket (npos: none)
{
    int depth = 0;
    for (std::size_t i = 0; i + token.size() <= e.size(); ++i) {
        const char c = e[i];
        if (c == '(' || c == '[' || c == '{') ++depth;
        if (c == ')' || c == ']' || c == '}') --depth;
        if (depth == 0 && e.compare(i, token.size(), token) == 0) return i;
    }
    return std::string::npos;
}

_hot static bool wrapped_(const std::string& e, const char open, const char close) // e is one bracket group: (...) / [...] / {...}
{
    if (e.size() < 2 || e.front() != open || e.back() != close) return false;
    int depth = 0;
    for (std::size_t i = 0; i < e.size(); ++i) {
        if (e[i] == '(' || e[i] == '[' || e[i] == '{') ++depth;
        if (e[i] == ')' || e[i] == ']' || e[i] == '}') --depth;
        if (depth == 0 && i + 1 < e.size()) return false;
    }
    return true;
}

_hot static std::vector<std::string> body_(const std::vector<std::string>& code, const std::size_t def, const std::string& inlineBody)
{
    // Statements of the function (nested functions / classes replaced by "def": their returns are not ours)
    static const std::regex nested(R"(^\s*(?:async\s+)?(?:def|class)\b)");
    std::vector<std::string> body;
    if (!xstyle::is_blank(inlineBody)) return {xstyle::trim(inlineBody)};
    const std::size_t indent = xstyle::indentation(code[def]).size();
    std::size_t skipBelow = std::string::npos;
    for (std::size_t j = def + 1; j < code.size(); ++j) {
        if (xstyle::is_blank(code[j])) continue;
        const std::size_t level = xstyle::indentation(code[j]).size();
        if (level <= indent) break;
        if (skipBelow != std::string::npos && level > skipBelow) continue;
        skipBelow = std::regex_search(code[j], nested) ? level : std::string::npos;
        body.push_back(skipBelow == level ? "def" : xstyle::trim(code[j]));
    }
    return body;
}

/* typing */
_hot xstyle::check::TypeGuess xstyle::check::python_expression_type(const std::string& expression)
{
    static const std::regex integer(R"(^[-+]?(?:\d[\d_]*|0[xX][0-9a-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+)$)");
    static const std::regex real(R"(^[-+]?(?:\d[\d_]*\.\d*|\.\d+)(?:[eE][-+]?\d+)?$|^[-+]?\d+[eE][-+]?\d+$)");
    static const std::regex text(R"(^([rRuUfFbB]{0,2})("|')\s*\2$)"); // string contents are masked
    static const std::regex call(R"(^([A-Za-z_][\w.]*)\((.*)\)$)");
    static const std::regex method(R"(\.(\w+)\((.*)\)$)");
    static const std::unordered_map<std::string, std::string> builtins = {
        {"int", "int"}, {"float", "float"}, {"str", "str"}, {"bool", "bool"}, {"bytes", "bytes"}, {"list", "list"}, {"dict", "dict"}, {"set", "set"},
        {"tuple", "tuple"}, {"frozenset", "frozenset"}, {"len", "int"}, {"ord", "int"}, {"hash", "int"}, {"id", "int"}, {"repr", "str"}, {"ascii", "str"},
        {"chr", "str"}, {"hex", "str"}, {"oct", "str"}, {"bin", "str"}, {"format", "str"}, {"isinstance", "bool"}, {"issubclass", "bool"}, {"callable", "bool"},
        {"hasattr", "bool"}, {"all", "bool"}, {"any", "bool"}, {"sorted", "list"}, {"os.path.join", "str"}, {"os.path.basename", "str"},
        {"os.path.dirname", "str"}, {"os.path.abspath", "str"}, {"os.path.realpath", "str"}, {"os.path.normpath", "str"}, {"os.path.expanduser", "str"},
        {"os.path.relpath", "str"}, {"os.path.exists", "bool"}, {"os.path.isfile", "bool"}, {"os.path.isdir", "bool"}, {"os.path.islink", "bool"},
        {"os.path.isabs", "bool"}, {"os.getcwd", "str"}, {"os.listdir", "list"}, {"re.findall", "list"}, {"re.sub", "str"}, {"re.escape", "str"},
        {"json.dumps", "str"}, {"shutil.which", "str | None"},
    };
    static const std::unordered_map<std::string, std::string> methods = { // on a value of unknown type: guessed (certain on a string literal)
        {"strip", "str"}, {"lstrip", "str"}, {"rstrip", "str"}, {"lower", "str"}, {"upper", "str"}, {"title", "str"}, {"capitalize", "str"}, {"casefold", "str"},
        {"replace", "str"}, {"format", "str"}, {"join", "str"}, {"zfill", "str"}, {"center", "str"}, {"ljust", "str"}, {"rjust", "str"}, {"decode", "str"},
        {"split", "list"}, {"rsplit", "list"}, {"splitlines", "list"}, {"startswith", "bool"}, {"endswith", "bool"}, {"isdigit", "bool"}, {"isalpha", "bool"},
        {"isspace", "bool"}, {"isalnum", "bool"}, {"encode", "bytes"}, {"read", "str"}, {"readlines", "list"}, {"readline", "str"},
    };
    const std::string e = xstyle::trim(expression);
    std::smatch match;
    if (e.empty()) return {};

    // Literals
    if (e == "True" || e == "False") return {"bool", true};
    if (std::regex_match(e, integer)) return {"int", true};
    if (std::regex_match(e, real)) return {"float", true};
    if (std::regex_match(e, match, text)) return {xstyle::lower(match[1].str()).find('b') != std::string::npos ? "bytes" : "str", true};

    // Operators (precedence: conditional, boolean, comparison, arithmetic)
    const std::size_t ternary = top_level_(e, " if ");
    if (ternary != std::string::npos && top_level_(e, " else ") != std::string::npos) {
        const xstyle::check::TypeGuess a = xstyle::check::python_expression_type(e.substr(0, ternary));
        const xstyle::check::TypeGuess b = xstyle::check::python_expression_type(e.substr(top_level_(e, " else ") + 6));
        if (a.type.empty() || a.type != b.type) return {};
        return {a.type, a.certain && b.certain};
    }
    if (top_level_(e, " and ") != std::string::npos || top_level_(e, " or ") != std::string::npos) return {}; // gives one of its operands
    if (e.starts_with("not ")) return {"bool", true};
    for (const std::string op: {" is ", " in ", " not in ", " is not "})
        if (top_level_(e, op) != std::string::npos) return {"bool", true};
    for (const std::string op: {" == ", " != ", " <= ", " >= ", " < ", " > "})
        if (top_level_(e, op) != std::string::npos) return {"bool", false}; // operators can be overloaded (numpy...)
    for (const std::string op: {" + ", " % "}) {
        const std::size_t pos = top_level_(e, op);
        if (pos == std::string::npos) continue;
        const xstyle::check::TypeGuess left = xstyle::check::python_expression_type(e.substr(0, pos));
        if (left.type == "str" && left.certain) return {"str", true}; // "text" + x / "text %s" % x: str or TypeError
        return {};
    }

    // Displays and comprehensions
    if (wrapped_(e, '[', ']')) return {"list", true};
    if (wrapped_(e, '{', '}')) return {e == "{}" || top_level_(e.substr(1, e.size() - 2), ":") != std::string::npos ? "dict" : "set", true};
    if (wrapped_(e, '(', ')')) {
        const std::string inner = e.substr(1, e.size() - 2);
        if (xstyle::is_blank(inner) || top_level_(inner, ",") != std::string::npos) return {"tuple", true};
        return xstyle::check::python_expression_type(inner);
    }

    // Calls: builtins / stdlib, then the methods
    if (std::regex_match(e, match, call) && wrapped_(e.substr(match[1].length()), '(', ')') && builtins.contains(match[1].str()))
        return {builtins.at(match[1].str()), true};
    if (std::regex_search(e, match, method) && wrapped_(e.substr(static_cast<std::size_t>(match.position(0)) + 1 + match[1].length()), '(', ')') && methods.contains(match[1].str())) {
        const std::string receiver = e.substr(0, static_cast<std::size_t>(match.position(0)));
        const xstyle::check::TypeGuess owner = xstyle::check::python_expression_type(receiver);
        return {methods.at(match[1].str()), owner.type == "str" && owner.certain && match[1].str() != "read" && match[1].str() != "readlines"};
    }
    return {};
}

_hot xstyle::check::TypeGuess xstyle::check::python_return_type(const std::vector<std::string>& code, const std::size_t def, const std::string& inlineBody, const bool future)
{
    static const std::regex returnValue(R"((?:^|[:;]\s*)return\b(.*)$)");
    static const std::regex yield(R"(\byield\b)");
    static const std::regex identifier(R"([A-Za-z_]\w*)");
    const std::vector<std::string> body = body_(code, def, inlineBody);
    std::vector<xstyle::check::TypeGuess> types;
    bool none = false;

    // Stub / abstract body: the type is the one of the implementations
    if (!body.empty() && (body[0] == "..." || body[0].starts_with("raise NotImplementedError"))) return {};
    for (const std::string& line: body) {
        if (line == "def") continue;
        if (std::regex_search(line, yield)) return {}; // generator
        std::smatch match;
        if (!std::regex_search(line, match, returnValue)) continue;
        std::string value = xstyle::trim(match[1].str());
        if (value.ends_with(";")) value.pop_back();
        if (value.empty() || value == "None") none = true;
        else types.push_back(xstyle::check::python_expression_type(value));
        if (!types.empty() && types.back().type.empty() && std::regex_match(value, identifier)) {
            // Local variable assigned once in the function: the type of its value (guessed: it can be changed in place)
            const std::regex assignment("^" + value + R"(\s*(?::[^=]+)?=(?!=)\s*(.+)$)");
            std::vector<std::string> values;
            for (const std::string& statement: body) {
                std::smatch assigned;
                if (statement.starts_with(value) && std::regex_match(statement, assigned, assignment)) values.push_back(assigned[1].str());
            }
            if (values.size() == 1) types.back() = {xstyle::check::python_expression_type(values[0]).type, false};
        }
    }
    if (types.empty()) return {"None", true};
    bool certain = true;
    for (const xstyle::check::TypeGuess& type: types) {
        if (type.type.empty() || type.type != types[0].type) return {};
        certain = certain && type.certain;
    }
    // T | None: Python 3.10+ (or from __future__ import annotations)
    if (none) return {types[0].type.ends_with("| None") ? types[0].type : types[0].type + " | None", certain && future};
    return {types[0].type, certain};
}

_hot xstyle::check::TypeGuess xstyle::check::python_parameter_type(const std::vector<std::string>& code, const std::size_t def, const std::string& name,
    const std::string& value)
{
    static const std::unordered_set<std::string> boolNames = {"verbose", "debug", "force", "quiet", "recursive", "dry_run", "enabled", "strict", "check", "append"};
    static const std::unordered_set<std::string> intNames = {"count", "n", "size", "index", "idx", "num", "number", "limit", "depth", "level", "port", "width",
        "height", "retries", "line", "column", "offset", "start", "end", "step"};
    static const std::unordered_set<std::string> strNames = {"path", "file", "filename", "dir", "directory", "name", "text", "msg", "message", "url", "prefix",
        "suffix", "pattern", "label", "title", "root", "encoding", "mode", "key", "content", "string", "word", "sep", "separator", "out", "output", "src", "dst"};

    // A default value: its type (certain), or the type of the usage when it is None
    const std::string defaultValue = xstyle::trim(value);
    if (!defaultValue.empty() && defaultValue != "None") return xstyle::check::python_expression_type(defaultValue);

    // Usage in the body (guessed)
    const std::string n = name;
    const std::vector<std::pair<std::regex, std::string>> usages = {
        {std::regex("\\b" + n + R"(\.(?:strip|lstrip|rstrip|lower|upper|startswith|endswith|split|splitlines|replace|encode|format|isdigit|isalpha)\()"), "str"},
        {std::regex(R"((?:\bopen|os\.path\.\w+|Path|os\.listdir|os\.makedirs|shutil\.\w+)\(\s*)" + n + R"(\b)"), "str"},
        {std::regex("\\b" + n + R"(\.(?:append|extend|insert|pop|sort|remove)\()"), "list"},
        {std::regex("\\b" + n + R"(\.(?:items|keys|values|setdefault|update)\()"), "dict"},
        {std::regex(R"(\brange\(\s*)" + n + R"(\s*\)|\b)" + n + R"(\s*[-*/%]\s*\d|\b)" + n + R"(\s*\+\s*\d)"), "int"},
    };
    std::string guess;
    for (const std::string& line: body_(code, def, "")) {
        for (const auto &[pattern, type]: usages) {
            if (line.find(n) == std::string::npos || !std::regex_search(line, pattern)) continue;
            if (!guess.empty() && guess != type) return {}; // used as two types: by hand
            guess = type;
        }
    }

    // Name (guessed): is_x / has_x, names of paths / counts...
    if (guess.empty()) {
        const std::string lower = xstyle::lower(n);
        const std::string last = lower.substr(lower.rfind('_') == std::string::npos ? 0 : lower.rfind('_') + 1);
        if (lower.starts_with("is_") || lower.starts_with("has_") || lower.starts_with("use_") || lower.starts_with("should_") || boolNames.contains(lower)) guess = "bool";
        else if (intNames.contains(lower) || lower.starts_with("nb_") || lower.starts_with("num_") || last == "count" || last == "size" || last == "index") guess = "int";
        else if (strNames.contains(lower) || strNames.contains(last)) guess = "str";
    }
    if (guess.empty()) return {};
    return {defaultValue == "None" ? guess + " | None" : guess, false};
}
