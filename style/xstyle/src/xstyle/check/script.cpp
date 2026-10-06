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
        if (std::regex_search(code[i], match, printError))
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), "PY-PRINT-ERROR", "Error printed with print(file=stderr)",
                "stderr.write(f\"...\\n\") then exit(<code>)"));

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
            issues.push_back(xstyle::check::make_issue(file, i, start, "PY-OPEN-ENCODING", "Text file opened without encoding", "open(..., encoding=\"utf-8\")"));
        }
    }
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
    if (bash && !isStrict)
        issues.push_back(xstyle::check::make_issue(file, 0, 0, "SH-STRICT", "Bash script without strict mode", "set -euo pipefail (after the shebang)"));

    // [ ] tests in bash
    if (!bash) return;
    for (std::size_t i = 1; i < code.size(); ++i) {
        std::smatch match;
        if (std::regex_search(code[i], match, singleTest))
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0) + (code[i][static_cast<std::size_t>(match.position(0))] == '[' || code[i][static_cast<std::size_t>(match.position(0))] == 't' ? 0 : 1)),
                "SH-TEST", "[ ] / test in a bash script", "[[ ... ]] (and (( a > b )) for the numbers)"));
    }
}
