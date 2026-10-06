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
#include <functional>
#include <algorithm>
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
    const bool future = std::any_of(code.begin(), code.end(), [](const std::string& line) {return line.find("from __future__ import annotations") != std::string::npos;});

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

        // Type hints: the certain ones are [auto-fix], the guessed ones [danger-fix], the others by hand
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
            std::vector<std::string> guessed;
            std::vector<std::string> unknown;
            std::string certainLine = lines[i]; // only the certain hints
            std::string guessedLine = lines[i]; // certain + guessed hints
            if (!match[3].matched && name != "__init__") {
                const std::string inlineBody = code[i].substr(static_cast<std::size_t>(match.position(0) + match.length(0)));
                const xstyle::check::TypeGuess type = xstyle::check::python_return_type(code, i, inlineBody, future);
                missing.push_back("return");
                if (type.type.empty()) unknown.push_back("return");
                else if (!type.certain) guessed.push_back("return (" + type.type + ")");
                if (type.certain) certainLine.insert(close + 1, " -> " + type.type);
                if (!type.type.empty()) guessedLine.insert(close + 1, " -> " + type.type);
            }
            for (std::size_t p = spans.size(); p > 0; --p) {
                const auto &[a, b] = spans[p - 1];
                const std::string masked = code[i].substr(a, b - a);
                const std::string parameter = xstyle::trim(masked);
                const std::size_t equal = masked.find('=');
                const std::string declaration = xstyle::trim(masked.substr(0, equal));
                if (parameter == "self" || parameter == "cls" || parameter == "*" || parameter == "/" || declaration.find(':') != std::string::npos) continue;
                missing.insert(missing.begin(), declaration);
                const xstyle::check::TypeGuess type = declaration.starts_with("*") ? xstyle::check::TypeGuess{}
                    : xstyle::check::python_parameter_type(code, i, declaration, equal == std::string::npos ? "" : masked.substr(equal + 1));
                if (type.type.empty()) {
                    unknown.insert(unknown.begin(), declaration);
                    continue;
                }
                if (!type.certain) guessed.insert(guessed.begin(), declaration + " (" + type.type + ")");
                const std::string spacing = lines[i].substr(a, masked.find_first_not_of(" \t"));
                const std::string value = equal == std::string::npos ? "" : " = " + xstyle::trim(lines[i].substr(a + equal + 1, b - a - equal - 1));
                const std::string annotated = spacing + declaration + ": " + type.type + value;
                if (type.certain) certainLine.replace(a, b - a, annotated);
                guessedLine.replace(a, b - a, annotated);
            }
            if (!missing.empty()) {
                const std::function<std::string(const std::vector<std::string>&)> join = [](const std::vector<std::string>& items) {
                    std::string text;
                    for (const std::string& item: items)
                        text += (text.empty() ? "" : ", ") + item;
                    return text;
                };
                std::string message = "No type hint for: " + join(missing) + " (" + name + ")";
                if (!guessed.empty()) message += ", guessed: " + join(guessed);
                if (!unknown.empty() && unknown.size() < missing.size()) message += ", by hand: " + join(unknown);
                const std::size_t column = static_cast<std::size_t>(match.position(1));
                if (certainLine != lines[i]) {
                    issues.push_back(xstyle::check::make_issue(file, i, column, "PY-TYPE-HINTS", message, "", xstyle::check::replace_line(i, certainLine)));
                } else if (guessedLine != lines[i]) {
                    xstyle::Fix fix = xstyle::check::replace_line(i, guessedLine);
                    fix.dangerous = true;
                    issues.push_back(xstyle::check::make_issue(file, i, column, "PY-TYPE-HINTS", message, "", fix));
                } else {
                    issues.push_back(xstyle::check::make_issue(file, i, column, "PY-TYPE-HINTS", message, "def " + name + "(param: type) -> type:"));
                }
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
