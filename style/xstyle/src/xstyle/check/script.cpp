/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file script.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/check/Checks.hpp"
#include "xstyle/Tools.hpp"
#include <regex>

/* tools */
_hot static std::string stderr_write_(const std::string& code, const std::string& line, const std::size_t start) // print(x, file=stderr) -> stderr.write(x + newline)
{
    static const std::regex literal(R"(^[rRfFuU]{0,2}("|')(.*)("|')$)");
    const std::size_t open = code.find('(', start);
    std::vector<std::pair<std::size_t, std::size_t>> arguments; // <start, end> on the line
    std::size_t from = open + 1;
    std::size_t end = open + 1;
    int depth = 1;
    for (; end < code.size() && depth > 0; ++end) {
        const char c = code[end];
        if (c == '(' || c == '[' || c == '{') ++depth;
        if (c == ')' || c == ']' || c == '}') --depth;
        if ((c == ',' && depth == 1) || depth == 0) {
            arguments.push_back({from, end});
            from = end + 1;
        }
    }
    if (depth != 0 || arguments.size() != 2) return ""; // several values (joined by spaces) or multi-line: by hand

    // One value and file=<stream>, in any order
    std::string value;
    std::string stream;
    for (const auto &[a, b]: arguments) {
        const std::string argument = xstyle::trim(line.substr(a, b - a));
        if (argument.starts_with("file") && argument.find('=') != std::string::npos) stream = xstyle::trim(argument.substr(argument.find('=') + 1));
        else value = argument;
    }
    if (value.empty() || stream.empty() || value.find('=') != std::string::npos) return "";
    std::smatch match;
    const bool simpleLiteral = std::regex_match(value, match, literal) && match[1].str() == match[3].str() && value.find(match[1].str() + match[1].str() + match[1].str()) == std::string::npos;
    const std::string text = simpleLiteral ? value.substr(0, value.size() - 1) + "\\n" + value.back() : "str(" + value + ") + \"\\n\"";
    return line.substr(0, start) + stream + ".write(" + text + ")" + line.substr(end);
}

_hot static std::string literal_type_(const std::string& masked) // type of a simple literal (empty: anything else), on the masked code
{
    static const std::regex integer(R"(^[-+]?(?:\d[\d_]*|0[xX][0-9a-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+)$)");
    static const std::regex real(R"(^[-+]?(?:\d[\d_]*\.\d*|\.\d+)(?:[eE][-+]?\d+)?$|^[-+]?\d+[eE][-+]?\d+$)");
    static const std::regex text(R"(^([rRuUfFbB]{0,2})("|')\s*\2$)"); // contents masked: one literal only
    const std::string value = xstyle::trim(masked);
    std::smatch match;

    if (value == "True" || value == "False") return "bool";
    if (std::regex_match(value, integer)) return "int";
    if (std::regex_match(value, real)) return "float";
    if (std::regex_match(value, match, text)) return xstyle::lower(match[1].str()).find('b') != std::string::npos ? "bytes" : "str";
    return "";
}

_hot static std::string return_type_(const std::vector<std::string>& code, const std::size_t def, const std::string& inlineBody) // return type when it is certain (empty: unknown)
{
    static const std::regex returnValue(R"((?:^|[:;]\s*)return\b(.*)$)");
    static const std::regex yield(R"(\byield\b)");
    static const std::regex nested(R"(^\s*(?:async\s+)?(?:def|class)\b)");
    const std::size_t indent = xstyle::indentation(code[def]).size();
    std::size_t skipBelow = std::string::npos; // body of a nested function / class
    std::vector<std::string> types;
    bool valued = false;
    bool first = true;

    // Body on the def line (def f(x): ...), else the indented lines below it
    std::vector<std::string> body;
    if (!xstyle::is_blank(inlineBody)) body.push_back(inlineBody);
    for (std::size_t j = def + 1; j < code.size() && xstyle::is_blank(inlineBody); ++j) {
        if (xstyle::is_blank(code[j])) continue;
        const std::size_t level = xstyle::indentation(code[j]).size();
        if (level <= indent) break;
        if (skipBelow != std::string::npos && level > skipBelow) continue;
        skipBelow = std::string::npos;
        if (std::regex_search(code[j], nested)) skipBelow = level;
        body.push_back(skipBelow == level ? "def" : code[j]);
    }

    for (const std::string& statement: body) {
        const std::string line = xstyle::trim(statement);
        // Stub / abstract body: the return type is the one of the implementations
        if (first && (line == "..." || line.starts_with("raise NotImplementedError"))) return "";
        first = false;
        if (line == "def") continue; // nested function / class: its returns are not ours
        if (std::regex_search(line, yield)) return ""; // generator
        std::smatch match;
        if (!std::regex_search(line, match, returnValue)) continue;
        std::string value = xstyle::trim(match[1].str());
        if (value.ends_with(";")) value.pop_back();
        if (value.empty() || value == "None") continue;
        valued = true;
        types.push_back(literal_type_(value));
    }
    if (!valued) return "None";
    for (const std::string& type: types)
        if (type.empty() || type != types[0]) return "";
    return types[0];
}

/* checks */
_hot void xstyle::check::python(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex wildcard(R"(^\s*from\s+\S+\s+import\s+\*)");
    static const std::regex bareExcept(R"(^\s*except\s*:)");
    static const std::regex definition(R"(^\s*(?:async\s+)?def\s+(\w+)\s*\((.*)\)\s*(->\s*[^:]+)?:)");
    static const std::regex printError(R"(\bprint\s*\(.*\bfile\s*=\s*(?:sys\.)?stderr)");
    static const std::regex openCall(R"((?:^|[^\w.])open\s*\()");
    static const std::regex binaryMode(R"(['"][rwax+]*b[rwax+]*['"])");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();

    for (std::size_t i = 0; i < code.size(); ++i) {
        std::smatch match;
        if (std::regex_search(code[i], match, wildcard))
            issues.push_back(xstyle::check::make_issue(file, i, 0, "PY-WILDCARD-IMPORT", "Wildcard import", "from module import name # Used for ..."));
        if (std::regex_search(code[i], match, bareExcept))
            issues.push_back(xstyle::check::make_issue(file, i, 0, "PY-BARE-EXCEPT", "Bare except: catches everything (KeyboardInterrupt, SystemExit...)",
                "except <ExpectedError> as e: (with an explicit message on stderr)"));
        if (std::regex_search(code[i], match, printError)) {
            const std::size_t column = static_cast<std::size_t>(match.position(0));
            const std::string fixed = stderr_write_(code[i], lines[i], column);
            if (fixed.empty())
                issues.push_back(xstyle::check::make_issue(file, i, column, "PY-PRINT-ERROR", "Error printed with print(file=stderr)", "stderr.write(f\"...\\n\") then exit(<code>)"));
            else
                issues.push_back(xstyle::check::make_issue(file, i, column, "PY-PRINT-ERROR", "Error printed with print(file=stderr)", "", xstyle::check::replace_line(i, fixed)));
        }

        // Type hints on every parameter and on the return (added when certain: literal defaults, None / literal returns)
        if (std::regex_search(code[i], match, definition)) {
            const std::string name = match[1].str();
            const std::size_t open = static_cast<std::size_t>(match.position(2));
            const std::size_t close = open + static_cast<std::size_t>(match.length(2)); // index of ')'
            std::vector<std::pair<std::size_t, std::size_t>> spans; // parameters <start, end> on the line
            int depth = 0;
            std::size_t from = open;
            for (std::size_t k = open; k <= close; ++k) {
                const char c = k < close ? code[i][k] : ',';
                if (c == '(' || c == '[' || c == '{') ++depth;
                if (c == ')' || c == ']' || c == '}') --depth;
                if (c == ',' && depth == 0) {
                    if (!xstyle::is_blank(code[i].substr(from, k - from))) spans.push_back({from, k});
                    from = k + 1;
                }
            }
            std::vector<std::string> missing;
            std::vector<std::string> unknown;
            std::string fixed = lines[i];
            const std::string inlineBody = code[i].substr(static_cast<std::size_t>(match.position(0) + match.length(0)));
            const std::string returnType = !match[3].matched && name != "__init__" ? return_type_(code, i, inlineBody) : "";
            if (!match[3].matched && name != "__init__") {
                missing.push_back("return");
                if (returnType.empty()) unknown.push_back("return");
                else fixed.insert(close + 1, " -> " + returnType);
            }
            for (std::size_t p = spans.size(); p > 0; --p) {
                const auto &[a, b] = spans[p - 1];
                const std::string masked = code[i].substr(a, b - a);
                const std::string parameter = xstyle::trim(masked);
                const std::size_t equal = masked.find('=');
                const std::string declaration = xstyle::trim(masked.substr(0, equal));
                if (parameter == "self" || parameter == "cls" || parameter == "*" || parameter == "/" || declaration.find(':') != std::string::npos) continue;
                missing.insert(missing.begin(), declaration);
                const std::string type = equal == std::string::npos || declaration.starts_with("*") ? "" : literal_type_(masked.substr(equal + 1));
                if (type.empty()) {
                    unknown.insert(unknown.begin(), declaration);
                    continue;
                }
                const std::string value = xstyle::trim(lines[i].substr(a + equal + 1, b - a - equal - 1));
                const std::string spacing = lines[i].substr(a, masked.find_first_not_of(" \t"));
                fixed.replace(a, b - a, spacing + declaration + ": " + type + " = " + value);
            }
            if (!missing.empty()) {
                std::string list;
                for (const std::string& m: missing)
                    list += (list.empty() ? "" : ", ") + m;
                std::string left;
                for (const std::string& u: unknown)
                    left += (left.empty() ? "" : ", ") + u;
                const std::string message = "No type hint for: " + list + " (" + name + ")" + (unknown.size() < missing.size() && !unknown.empty() ? ", by hand: " + left : "");
                if (fixed == lines[i])
                    issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(1)), "PY-TYPE-HINTS", message, "def " + name + "(param: type) -> type:"));
                else
                    issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(1)), "PY-TYPE-HINTS", message, "", xstyle::check::replace_line(i, fixed)));
            }
        }

        // open() of a text file without encoding (the mode is read on the real line, the code is masked)
        for (std::sregex_iterator it(code[i].begin(), code[i].end(), openCall); it != std::sregex_iterator(); ++it) {
            // Arguments up to the matching parenthesis (open(os.path.join(a, b), "w", encoding="utf-8"))
            const std::size_t start = static_cast<std::size_t>(it->position(0) + it->length(0));
            std::size_t end = start;
            for (int depth = 1; end < code[i].size() && depth > 0; ++end)
                depth += code[i][end] == '(' ? 1 : code[i][end] == ')' ? -1 : 0;
            const std::string arguments = lines[i].substr(start, end - start);
            if (arguments.find("encoding") != std::string::npos || std::regex_search(arguments, binaryMode)) continue;
            // Behavior change for the files that are not UTF-8 (the locale encoding was used): --force
            if (end == start + 1 || code[i][end - 1] != ')') {
                issues.push_back(xstyle::check::make_issue(file, i, start, "PY-OPEN-ENCODING", "Text file opened without encoding", "open(..., encoding=\"utf-8\")"));
                continue;
            }
            xstyle::Fix fix = xstyle::check::replace_line(i, lines[i].substr(0, end - 1) + ", encoding=\"utf-8\"" + lines[i].substr(end - 1));
            fix.unsafe = true;
            issues.push_back(xstyle::check::make_issue(file, i, start, "PY-OPEN-ENCODING", "Text file opened without encoding", "", fix));
        }
    }
}

_hot static std::string bracket_tests_(const std::string& code, const std::string& line, bool& unsafe) // [ ] -> [[ ]] (empty: nothing converted)
{
    static const std::regex comparison(R"((?:^|\s)(?:==?|!=)\s+(\S+))");
    static const std::string before = " \t;&|(!";
    static const std::string after = " \t;&|)";
    std::vector<std::pair<std::size_t, std::size_t>> tests; // <[, ]>

    for (std::size_t j = 0; j + 1 < code.size(); ++j) {
        if (code[j] != '[' || !xstyle::is_space(code[j + 1]) || (j > 0 && before.find(code[j - 1]) == std::string::npos)) continue;
        std::size_t k = j + 2;
        while (k < code.size() && !(code[k] == ']' && xstyle::is_space(code[k - 1]) && (k + 1 == code.size() || after.find(code[k + 1]) != std::string::npos))) ++k;
        if (k >= code.size()) return ""; // continued on the next line: by hand
        const std::string inner = code.substr(j + 1, k - j - 1);
        // -a / -o and unquoted glob patterns don't mean the same thing in [[ ]]
        if (inner.find(" -a ") != std::string::npos || inner.find(" -o ") != std::string::npos) unsafe = true;
        for (std::sregex_iterator it(inner.begin(), inner.end(), comparison); it != std::sregex_iterator(); ++it) {
            const std::size_t pos = j + 1 + static_cast<std::size_t>(it->position(1));
            const std::string right = line.substr(pos, static_cast<std::size_t>(it->length(1)));
            if (right[0] != '"' && right[0] != '\'' && right.find_first_of("*?[") != std::string::npos) unsafe = true;
        }
        tests.push_back({j, k});
        j = k;
    }
    if (tests.empty()) return "";
    std::string fixed = line;
    for (std::size_t t = tests.size(); t > 0; --t) {
        const auto &[open, close] = tests[t - 1];
        std::string inner = fixed.substr(open + 1, close - open - 1);
        for (std::size_t pos = inner.find("\\<"); pos != std::string::npos; pos = inner.find("\\<", pos)) inner.erase(pos, 1);
        for (std::size_t pos = inner.find("\\>"); pos != std::string::npos; pos = inner.find("\\>", pos)) inner.erase(pos, 1);
        fixed.replace(open, close - open + 1, "[[" + inner + "]]");
    }
    return fixed;
}

_hot void xstyle::check::shell(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex strict(R"(\bset\s+-[a-z]*e[a-z]*u?[a-z]*o\s+pipefail|\bset\s+-euo\s+pipefail|\bset\s+-o\s+pipefail)");
    static const std::regex singleTest(R"((?:^|[\s;&|(!])\[\s|(?:^|[;&|!]\s*|\b(?:if|while|until|then|do)\s+)test\s+-?\w)");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    if (lines.empty()) return;

    // Shebang and strict mode
    const bool shebang = lines[0].starts_with("#!");
    const bool bash = shebang && lines[0].find("bash") != std::string::npos;
    if (!shebang)
        issues.push_back(xstyle::check::make_issue(file, 0, 0, "SH-SHEBANG", "Script without shebang", "#!/bin/bash"));
    bool isStrict = false;
    for (const std::string& line: code)
        if (std::regex_search(line, strict)) isStrict = true;
    if (bash && !isStrict) {
        // After the shebang and the comments right under it (can make the script stop where it went on: --force)
        std::size_t at = 1;
        while (at < lines.size() && lines[at].starts_with("#")) ++at;
        std::vector<std::string> inserted = {"set -euo pipefail"};
        if (at > 1) inserted.insert(inserted.begin(), "");
        if (at < lines.size() && !xstyle::is_blank(lines[at])) inserted.push_back("");
        xstyle::Fix fix = xstyle::check::replace_lines(at, 0, inserted);
        fix.unsafe = true;
        issues.push_back(xstyle::check::make_issue(file, 0, 0, "SH-STRICT", "Bash script without strict mode",
            "set -euo pipefail after the shebang (--force: a failing command / unset variable will then stop the script)", fix));
    }

    // [ ] tests in bash
    if (!bash) return;
    for (std::size_t i = 1; i < code.size(); ++i) {
        std::smatch match;
        if (!std::regex_search(code[i], match, singleTest)) continue;
        const std::size_t column = static_cast<std::size_t>(match.position(0)) + (code[i][static_cast<std::size_t>(match.position(0))] == '[' || code[i][static_cast<std::size_t>(match.position(0))] == 't' ? 0 : 1);
        bool unsafe = false;
        const std::string fixed = bracket_tests_(code[i], lines[i], unsafe);
        if (fixed.empty()) {
            issues.push_back(xstyle::check::make_issue(file, i, column, "SH-TEST", "[ ] / test in a bash script", "[[ ... ]] (and (( a > b )) for the numbers)"));
            continue;
        }
        xstyle::Fix fix = xstyle::check::replace_line(i, fixed);
        fix.unsafe = unsafe; // -a / -o or an unquoted pattern: the meaning can change
        issues.push_back(xstyle::check::make_issue(file, i, column, "SH-TEST", "[ ] / test in a bash script", "", fix));
    }
}
