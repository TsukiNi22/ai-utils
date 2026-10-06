/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file rust.cpp

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
_hot static bool test_code_(const xstyle::SourceFile& file, std::size_t& testStart)
{
    // Test files (tests/, benches/, examples/) or the #[cfg(test)] module (at the end of the file by convention)
    const std::string path = file.getPath().generic_string();
    testStart = file.getLines().size();
    for (std::size_t i = 0; i < file.getCode().size(); ++i)
        if (file.getCode()[i].find("#[cfg(test)]") != std::string::npos) {
            testStart = i;
            break;
        }
    return path.find("/tests/") != std::string::npos || path.find("/benches/") != std::string::npos || path.find("/examples/") != std::string::npos;
}

/* checks */
_hot void xstyle::check::rust(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex unwrap(R"(\.unwrap\(\s*\))");
    static const std::regex panic(R"(\b(panic|todo|unimplemented)!\s*[\(\[\{]|\b(?:std::)?process::exit\s*\()");
    static const std::regex glob(R"(^\s*(?:pub(?:\([^)]*\))?\s+)?use\s+([\w:]+)::\*\s*;)");
    static const std::regex function(R"(\bfn\s+([A-Za-z_]\w*))");
    static const std::regex type(R"(\b(?:struct|enum|trait|union|type)\s+([A-Za-z_]\w*))");
    static const std::regex constant(R"(\b(?:const|static)\s+(?:mut\s+)?([A-Za-z_]\w*)\s*:)");
    static const std::regex binding(R"(\blet\s+(?:mut\s+)?([A-Za-z_]\w*)\s*[:=;])");
    static const std::regex snake(R"(^_?[a-z][a-z0-9_]*$|^_$)");
    static const std::regex pascal(R"(^[A-Z][A-Za-z0-9]*$)");
    static const std::regex upper(R"(^[A-Z][A-Z0-9_]*$)");
    static const std::regex unsafeBlock(R"(\bunsafe\s*\{)");
    static const std::regex println(R"(\bprintln!\s*\(\s*")");
    static const std::regex errorText(R"(^println!\s*\(\s*"(?:error|err|fatal|warning|failed)\b)", std::regex::icase);
    static const std::regex dbg(R"(\bdbg!\s*\()");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    const std::vector<std::string>& comments = file.getComments();
    const std::string name = file.getPath().filename().string();
    const bool binary = name == "main.rs" || name == "build.rs" || file.getPath().parent_path().filename() == "bin";
    std::size_t testStart = 0;
    const bool testFile = test_code_(file, testStart);

    for (std::size_t i = 0; i < code.size(); ++i) {
        const std::string& line = code[i];
        const bool test = testFile || i >= testStart;
        if (xstyle::is_blank(line)) continue;
        std::smatch match;

        // Errors: no unwrap / panic in the code that can fail (tests excepted)
        if (!test && line.find("unwrap") != std::string::npos && std::regex_search(line, match, unwrap))
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), "RS-UNWRAP", ".unwrap() can panic",
                "propagate with ? and a context (.map_err(|e| format!(\"...: {e}\"))?, anyhow .context(\"...\")?) or handle the error"));
        if (line.find('!') != std::string::npos || line.find("exit") != std::string::npos) {
            if (std::regex_search(line, match, panic)) {
                const std::string macro = match[1].str();
                const bool unfinished = macro == "todo" || macro == "unimplemented";
                if (unfinished || (!binary && !test))
                    issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), "RS-PANIC",
                        unfinished ? macro + "! left in the code" : (macro.empty() ? "process::exit" : macro + "!") + std::string(" in library code"),
                        unfinished ? "implement it, or return an error (Err(...)) describing the limitation" : "return a Result with an error message, the binary decides how to exit"));
            }
        }

        // Imports
        if (line.find("::*") != std::string::npos && std::regex_search(line, match, glob)) {
            const std::string path = match[1].str();
            if (path != "super" && !path.ends_with("prelude"))
                issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), "RS-GLOB-IMPORT", "Glob import of " + path,
                    "use " + path + "::{Name, Other};"));
        }

        // Naming (rustfmt / rustc conventions)
        static const std::vector<std::tuple<const std::regex*, const std::regex*, std::string>> namings = {
            {&function, &snake, "function"}, {&type, &pascal, "type"}, {&constant, &upper, "constant"}, {&binding, &snake, "variable"},
        };
        for (const auto &[pattern, expected, kind]: namings) {
            if (!std::regex_search(line, match, *pattern) || std::regex_match(match[1].str(), *expected)) continue;
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(1)), "RS-NAMING", kind + " " + match[1].str() + " not in "
                + (expected == &snake ? "snake_case" : expected == &pascal ? "PascalCase" : "UPPER_SNAKE"), "rename it (rustfmt / clippy conventions)"));
        }

        // unsafe needs its justification
        if (line.find("unsafe") != std::string::npos && std::regex_search(line, match, unsafeBlock)) {
            bool justified = comments[i].find("SAFETY") != std::string::npos;
            for (std::size_t k = i; k > 0 && k + 3 > i && !justified; --k)
                justified = comments[k - 1].find("SAFETY") != std::string::npos;
            if (!justified)
                issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), "RS-UNSAFE", "unsafe block without // SAFETY: comment",
                    "// SAFETY: why the invariants hold (on the line above)"));
        }

        // Errors on stderr (the message is read on the real line, the code is masked)
        if (line.find("println!") != std::string::npos && std::regex_search(line, match, println)) {
            const std::size_t pos = static_cast<std::size_t>(match.position(0));
            if (std::regex_search(lines[i].substr(pos), errorText)) {
                std::string fixed = lines[i];
                fixed.replace(pos, 8, "eprintln!");
                issues.push_back(xstyle::check::make_issue(file, i, pos, "RS-PRINT-ERROR", "Error printed on stdout", "", xstyle::check::replace_line(i, fixed)));
            }
        }

        // Debug leftovers
        if (line.find("dbg!") != std::string::npos && std::regex_search(line, match, dbg))
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), "RS-DBG", "dbg! left in the code", "remove it (or a log / eprintln! kept on purpose)"));
    }
}
