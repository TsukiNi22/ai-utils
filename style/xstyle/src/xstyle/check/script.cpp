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
_hot static std::vector<std::string> parameters_(const std::string& list)
{
    // Split on the commas outside of brackets
    std::vector<std::string> parameters;
    std::string current;
    int depth = 0;

    for (const char c: list) {
        if (c == '(' || c == '[' || c == '{') ++depth;
        if (c == ')' || c == ']' || c == '}') --depth;
        if (c == ',' && depth == 0) {
            parameters.push_back(xstyle::trim(current));
            current.clear();
            continue;
        }
        current += c;
    }
    if (!xstyle::trim(current).empty()) parameters.push_back(xstyle::trim(current));
    return parameters;
}

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

        // Type hints on every parameter and on the return
        if (std::regex_search(code[i], match, definition)) {
            const std::string name = match[1].str();
            std::vector<std::string> missing;
            for (const std::string& parameter: parameters_(match[2].str())) {
                if (parameter == "self" || parameter == "cls" || parameter == "*" || parameter == "/") continue;
                const std::string declaration = parameter.substr(0, parameter.find('='));
                if (declaration.find(':') == std::string::npos) missing.push_back(xstyle::trim(declaration));
            }
            if (!match[3].matched && name != "__init__") missing.push_back("return");
            if (!missing.empty()) {
                std::string list;
                for (const std::string& m: missing)
                    list += (list.empty() ? "" : ", ") + m;
                issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(1)), "PY-TYPE-HINTS", "No type hint for: " + list + " (" + name + ")",
                    "def " + name + "(param: type) -> type:"));
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
