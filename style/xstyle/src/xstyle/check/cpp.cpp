/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file cpp.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/check/Checks.hpp"
#include "xstyle/Tools.hpp"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <regex>
#include <set>

/* tools */
_hot static std::string rstrip_(const std::string& s)
{
    std::size_t last = s.find_last_not_of(" \t");
    return last == std::string::npos ? "" : s.substr(0, last + 1);
}

_hot static std::size_t matching_(const std::string& code, const std::size_t open) // index of the matching ')' / ']' (npos if not on the line)
{
    const char opening = code[open];
    const char closing = opening == '(' ? ')' : ']';
    int depth = 0;

    for (std::size_t j = open; j < code.size(); ++j) {
        if (code[j] == opening) ++depth;
        if (code[j] == closing && --depth == 0) return j;
    }
    return std::string::npos;
}

_hot static std::string previous_word_(const std::string& code, std::size_t pos) // identifier ending right before pos (spaces skipped)
{
    while (pos > 0 && xstyle::is_space(code[pos - 1])) --pos;
    std::size_t end = pos;
    while (pos > 0 && xstyle::is_word(code[pos - 1])) --pos;
    return code.substr(pos, end - pos);
}

_hot static std::size_t previous_code_(const xstyle::SourceFile& file, std::size_t line) // previous line with code (NO_INDEX if none)
{
    while (line > 0) {
        --line;
        if (!xstyle::is_blank(file.getCode()[line])) return line;
    }
    return NO_INDEX;
}

_hot static std::string guard_name_(const xstyle::SourceFile& file)
{
    std::string guard;
    for (const char c: file.getPath().stem().string())
        if (std::isalnum(static_cast<unsigned char>(c))) guard += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        else if (c == '+') guard += 'P'; // c++20.hpp -> CPP20_H
    return guard + "_H";
}

_hot static bool in_function_(const xstyle::SourceFile& file, const std::size_t line, bool& isStatic, std::size_t& function)
{
    // Walk the enclosing braces up to the function (static member function: no this)
    std::size_t b = file.getInfo()[line].brace;
    while (b != NO_INDEX) {
        const xstyle::Brace& brace = file.getBraces()[b];
        if (brace.kind == xstyle::Scope::Function) {
            isStatic = brace.isStatic;
            function = b;
            return true;
        }
        b = brace.parent;
    }
    return false;
}

_hot static bool member_function_(const xstyle::SourceFile& file, const std::size_t function)
{
    // Defined in a class, or out of line with a qualified name (Class::method)
    const xstyle::Brace& brace = file.getBraces()[function];
    if (brace.parent != NO_INDEX) {
        const xstyle::Scope parent = file.getBraces()[brace.parent].kind;
        if (parent == xstyle::Scope::Class || parent == xstyle::Scope::Struct) return true;
    }
    const std::size_t open = brace.header.find('(');
    return open != std::string::npos && brace.header.substr(0, open).find("::") != std::string::npos;
}

/* checks: names & usage */
_hot static void usage_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex usingNamespace(R"(\busing\s+namespace\s+([\w:]+))");
    static const std::regex usingStd(R"(^\s*using\s+(std::[\w:]+)\s*;)");
    static const std::regex null(R"(\bNULL\b)");
    static const std::regex typedefSimple(R"(^(\s*)typedef\s+([^;(){}]+?)\s+(\w+)\s*;(.*)$)");
    static const std::regex typedefWord(R"(\btypedef\b)");
    static const std::regex autoWord(R"(\bauto\b)");
    static const std::regex autoBinding(R"(^\s*[&*]*\s*\[)");
    static const std::regex autoName(R"(^\s*[&*]*\s*(\w+)\s*(=|\(|\{|:))");
    static const std::regex iteratorName(R"(^(it|iter|iterator|\w*It|\w*Iter|\w*_it|\w*_iter)$)");
    static const std::regex iteratorInit(R"(\.c?r?begin\(|\.c?r?end\(|\.find\(|std::(ranges::)?find(_if)?\(|lower_bound\(|upper_bound\(|\.insert\(|\.emplace\(|std::prev\(|std::next\()");
    const bool cpp = file.getLanguage() == xstyle::Language::Cpp;
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    const std::vector<xstyle::LineInfo>& info = file.getInfo();

    for (std::size_t i = 0; i < code.size(); ++i) {
        if (info[i].preprocessor) continue;
        const std::string& line = code[i];
        std::smatch match;

        const bool usingWord = line.find("using") != std::string::npos;
        if (cpp && usingWord && std::regex_search(line, match, usingNamespace) && !match[1].str().ends_with("literals"))
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), "CPP-USING-NAMESPACE", "using namespace " + match[1].str(),
                "remove it and write the fully qualified names (" + match[1].str() + "::...)"));
        if (cpp && usingWord && std::regex_search(line, match, usingStd))
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(1)), "CPP-USING-STD", "using " + match[1].str(), "remove it and write " + match[1].str()));

        // NULL -> nullptr (every occurrence of the line)
        if (cpp && line.find("NULL") != std::string::npos && std::regex_search(line, match, null)) {
            std::string fixed = lines[i];
            for (std::size_t pos = line.rfind("NULL"); pos != std::string::npos; pos = pos == 0 ? std::string::npos : line.rfind("NULL", pos - 1))
                if ((pos == 0 || !xstyle::is_word(line[pos - 1])) && (pos + 4 >= line.size() || !xstyle::is_word(line[pos + 4]))) fixed.replace(pos, 4, "nullptr");
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), "CPP-NULL", "NULL instead of nullptr", "", xstyle::check::replace_line(i, fixed)));
        }

        // typedef A B; -> using B = A;
        if (cpp && line.find("typedef") != std::string::npos && std::regex_search(line, match, typedefWord)) {
            std::smatch simple;
            const std::size_t column = static_cast<std::size_t>(match.position(0));
            if (std::regex_match(lines[i], simple, typedefSimple) && simple[2].str().find_first_of("{}") == std::string::npos)
                issues.push_back(xstyle::check::make_issue(file, i, column, "CPP-TYPEDEF", "typedef", "",
                    xstyle::check::replace_line(i, simple[1].str() + "using " + simple[3].str() + " = " + simple[2].str() + ";" + simple[4].str())));
            else
                issues.push_back(xstyle::check::make_issue(file, i, column, "CPP-TYPEDEF", "typedef", "using Name = Type;"));
        }

        // auto (iterators and structured bindings excepted)
        if (!cpp || line.find("auto") == std::string::npos) continue;
        for (std::sregex_iterator it(line.begin(), line.end(), autoWord); it != std::sregex_iterator(); ++it) {
            const std::size_t pos = static_cast<std::size_t>(it->position(0));
            const std::string after = line.substr(pos + 4);
            std::smatch name;
            if (std::regex_search(after, autoBinding)) continue;
            if (std::regex_search(after, name, autoName) && std::regex_match(name[1].str(), iteratorName)) continue;
            if (std::regex_search(after, iteratorInit)) continue;
            issues.push_back(xstyle::check::make_issue(file, i, pos, "CPP-AUTO", "auto instead of the explicit type", "write the type, even when it is long"));
        }
    }
}

_hot static void names_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex typeHead(R"(\b(class|struct|union|enum(?:\s+class|\s+struct)?)\s+(?:\[\[[^\]]*\]\]\s*|_\w+(?:\([^)]*\))?\s+)*([A-Za-z_]\w*)\s*(?:final\b\s*)?(?::(?!:)|\{|$))");
    static const std::regex pascal(R"(^[A-Z][A-Za-z0-9]*$)");
    static const std::regex define(R"(^\s*#\s*define\s+(\w+)(?!\w|\())");
    static const std::regex macro(R"(^([A-Z][A-Z0-9_]*|_\w+)$)");
    static const std::regex memberStart(R"(^\s*(using|typedef|friend|enum|class|struct|union|template|static_assert|public|private|protected|return)\b)");
    static const std::regex member(R"(^_[a-z][A-Za-z0-9]*$)");
    static const std::regex constant(R"(^[A-Z][A-Z0-9_]*$)");
    static const std::regex access(R"(^(public|private|protected)\s*:)");
    std::unordered_map<std::size_t, std::string> visibility; // class brace -> current access (private by default)
    const bool cpp = file.getLanguage() == xstyle::Language::Cpp;
    const std::vector<std::string>& code = file.getCode();
    const std::vector<xstyle::LineInfo>& info = file.getInfo();
    const std::vector<xstyle::Brace>& braces = file.getBraces();
    const std::string guard = guard_name_(file);

    for (std::size_t i = 0; i < code.size(); ++i) {
        const std::string& line = code[i];
        std::smatch match;

        // Macros: UPPER_SNAKE or _lower / _Section
        if (info[i].preprocessor) {
            if (std::regex_search(line, match, define) && !std::regex_match(match[1].str(), macro) && match[1].str() != guard)
                issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(1)), "CPP-MACRO-NAME", "Macro " + match[1].str() + " not in UPPER_SNAKE",
                    "#define " + xstyle::lower(match[1].str()) + " -> UPPER_SNAKE (or _lower for an attribute macro)"));
            continue;
        }
        if (!cpp) continue;

        // Types in PascalCase (definitions only, template parameters excepted)
        const bool typeWord = line.find("class") != std::string::npos || line.find("struct") != std::string::npos || line.find("enum") != std::string::npos
            || line.find("union") != std::string::npos;
        for (std::sregex_iterator it(line.begin(), typeWord ? line.end() : line.begin(), typeHead); it != std::sregex_iterator(); ++it) {
            const std::size_t pos = static_cast<std::size_t>(it->position(0));
            const std::string before = xstyle::trim(line.substr(0, pos));
            if (!before.empty() && (before.back() == '<' || before.back() == ',')) continue;
            const std::string name = (*it)[2].str();
            if (!std::regex_match(name, pascal))
                issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(it->position(2)), "CPP-CLASS-NAME", "Type " + name + " not in PascalCase", "rename it in PascalCase"));
        }

        // Data members of a class (private / protected): _camelCase (static constants in UPPER_SNAKE)
        if (info[i].brace == NO_INDEX || braces[info[i].brace].kind != xstyle::Scope::Class) continue;
        const std::string trimmed = xstyle::trim(line);
        if (std::regex_search(trimmed, match, access)) {
            visibility[info[i].brace] = match[1].str();
            continue;
        }
        if (visibility.contains(info[i].brace) && visibility[info[i].brace] == "public") continue;
        if (!trimmed.ends_with(";") || trimmed.find('(') != std::string::npos || trimmed.find("operator") != std::string::npos || std::regex_search(trimmed, memberStart)) continue;
        std::string declaration = trimmed.substr(0, trimmed.size() - 1);
        int depth = 0;
        std::size_t cut = std::string::npos;
        for (std::size_t j = 0; j < declaration.size() && cut == std::string::npos; ++j) {
            if (declaration[j] == '<') ++depth;
            if (declaration[j] == '>') --depth;
            if (depth == 0 && (declaration[j] == '=' || declaration[j] == '{' || declaration[j] == ',')) cut = j;
        }
        if (cut != std::string::npos && declaration[cut] == ',') continue;
        declaration = rstrip_(declaration.substr(0, cut));
        if (!declaration.empty() && declaration.back() == ']') declaration = rstrip_(declaration.substr(0, declaration.rfind('[')));
        std::size_t start = declaration.size();
        while (start > 0 && xstyle::is_word(declaration[start - 1])) --start;
        const std::string name = declaration.substr(start);
        if (name.empty() || start == 0 || xstyle::is_blank(declaration.substr(0, start))) continue;
        const bool isConstant = trimmed.find("const") != std::string::npos && std::regex_match(name, constant);
        if (!std::regex_match(name, member) && !isConstant)
            issues.push_back(xstyle::check::make_issue(file, i, line.find(name, line.find_first_not_of(' ')), "CPP-MEMBER-NAME", "Member " + name + " not named _camelCase",
                "_" + name.substr(name.find_first_not_of('_') == std::string::npos ? 0 : name.find_first_not_of('_'))));
    }
}

/* checks: layout */
_hot static void braces_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex testMacro(R"(^[A-Z_][A-Z0-9_]*\s*\()");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    const std::vector<std::string>& comments = file.getComments();

    for (const xstyle::Brace& brace: file.getBraces()) {
        const std::size_t open = brace.openLine;
        const std::string before = code[open].substr(0, brace.openColumn);
        const std::string after = code[open].substr(brace.openColumn + 1);

        // Function: brace on its own line (multi-line bodies only, test macros excepted)
        if (brace.kind == xstyle::Scope::Function && brace.closeLine != open && !xstyle::is_blank(before) && xstyle::is_blank(after)
            && !std::regex_search(brace.header, testMacro)) {
            const std::string signature = rstrip_(lines[open].substr(0, brace.openColumn));
            const std::string indent = xstyle::indentation(lines[brace.headerLine == NO_INDEX ? open : brace.headerLine]);
            issues.push_back(xstyle::check::make_issue(file, open, brace.openColumn, "CPP-BRACE-FUNCTION", "Function brace on the signature line", "",
                xstyle::check::replace_lines(open, 1, {signature, indent + "{" + lines[open].substr(brace.openColumn + 1)})));
        }

        // Class / namespace / control / lambda: brace on the same line
        const bool sameLineKind = brace.kind == xstyle::Scope::Class || brace.kind == xstyle::Scope::Struct || brace.kind == xstyle::Scope::Enum
            || brace.kind == xstyle::Scope::Namespace || brace.kind == xstyle::Scope::Extern || brace.kind == xstyle::Scope::Control || brace.kind == xstyle::Scope::Lambda;
        if (sameLineKind && xstyle::is_blank(before) && !brace.header.empty()) {
            const std::size_t previous = previous_code_(file, open);
            if (previous == NO_INDEX) continue;
            if (!xstyle::is_blank(comments[previous])) {
                issues.push_back(xstyle::check::make_issue(file, open, brace.openColumn, "CPP-BRACE-OWN-LINE", "Opening brace alone on its line", "move the { at the end of the previous line"));
                continue;
            }
            issues.push_back(xstyle::check::make_issue(file, open, brace.openColumn, "CPP-BRACE-OWN-LINE", "Opening brace alone on its line", "",
                xstyle::check::replace_lines(previous, open - previous + 1, {rstrip_(lines[previous]) + " {" + rstrip_(lines[open].substr(brace.openColumn + 1))})));
        }
    }

    // } newline else / catch
    static const std::regex elseStart(R"(^\s*(else|catch)\b)");
    for (std::size_t i = 1; i < code.size(); ++i) {
        std::smatch match;
        if ((code[i].find("else") == std::string::npos && code[i].find("catch") == std::string::npos) || !std::regex_search(code[i], match, elseStart)) continue;
        const std::size_t previous = previous_code_(file, i);
        if (previous == NO_INDEX || xstyle::trim(code[previous]) != "}" || !xstyle::is_blank(comments[previous])) continue;
        issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(1)), "CPP-ELSE-LINE", match[1].str() + " not on the closing brace line", "",
            xstyle::check::replace_lines(previous, i - previous + 1, {rstrip_(lines[previous]) + " " + xstyle::trim(lines[i])})));
    }
}

_hot static void spacing_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex keyword(R"(\b(if|for|while|switch|catch)\()");
    static const std::regex keywordParen(R"(\b(if|for|while|switch|catch)\s*\()");
    static const std::regex forHead(R"(\bfor\s*\()");
    static const std::regex inheritance(R"(\b(?:class|struct|enum\s+class|enum\s+struct|enum)\s+(?:_\w+(?:\([^)]*\))?\s+)*\w+(?:\s+final)?(\s+):(?!:))");
    static const std::unordered_set<std::string> types = {"int", "char", "bool", "void", "float", "double", "long", "short", "unsigned", "signed", "auto", "size_t",
        "wchar_t", "char8_t", "char16_t", "char32_t", "const", "ssize_t"};
    static const std::unordered_set<std::string> qualifiers = {"const", "static", "inline", "constexpr", "volatile", "mutable", "extern", "unsigned", "signed",
        "typename", "struct", "class", "virtual", "explicit", "long", "short", "friend", "register", "thread_local"};
    static const std::unordered_set<std::string> keywords = {"return", "case", "throw", "delete", "else", "co_return", "co_yield", "sizeof", "new", "and", "or", "not",
        "operator", "goto", "typeid", "decltype"};
    const bool cpp = file.getLanguage() == xstyle::Language::Cpp;
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    const std::vector<xstyle::LineInfo>& info = file.getInfo();

    for (std::size_t i = 0; i < code.size(); ++i) {
        if (info[i].preprocessor) continue;
        const std::string& line = code[i];
        std::smatch match;

        const bool parenthesis = line.find('(') != std::string::npos;

        // if( -> if (
        if (parenthesis && std::regex_search(line, match, keyword)) {
            std::string fixed = lines[i];
            std::vector<std::size_t> positions;
            for (std::sregex_iterator it(line.begin(), line.end(), keyword); it != std::sregex_iterator(); ++it)
                positions.push_back(static_cast<std::size_t>(it->position(1) + it->length(1)));
            for (std::size_t p = positions.size(); p > 0; --p)
                fixed.insert(positions[p - 1], " ");
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(0)), "CPP-KEYWORD-SPACE", "No space after " + match[1].str(), "",
                xstyle::check::replace_line(i, fixed)));
        }

        // if ( x ) -> if (x)
        for (std::sregex_iterator it(line.begin(), parenthesis ? line.end() : line.begin(), keywordParen); it != std::sregex_iterator(); ++it) {
            const std::size_t open = static_cast<std::size_t>(it->position(0) + it->length(0) - 1);
            const std::size_t close = matching_(line, open);
            if (close == std::string::npos || close == open + 1) continue;
            const bool spaceOpen = line[open + 1] == ' ';
            const bool spaceClose = line[close - 1] == ' ' && line[close - 2] != ';';
            if ((!spaceOpen && !spaceClose) || xstyle::is_blank(line.substr(open + 1, close - open - 1))) continue;
            std::string inner = lines[i].substr(open + 1, close - open - 1);
            inner = xstyle::trim(inner);
            issues.push_back(xstyle::check::make_issue(file, i, open, "CPP-PAREN-SPACE", "Space inside the parentheses of " + (*it)[1].str(), "",
                xstyle::check::replace_line(i, lines[i].substr(0, open + 1) + inner + lines[i].substr(close))));
            break;
        }
        // for (T x : list) -> for (T x: list)
        if (cpp && line.find("for") != std::string::npos && std::regex_search(line, match, forHead)) {
            const std::size_t open = static_cast<std::size_t>(match.position(0) + match.length(0) - 1);
            const std::size_t close = matching_(line, open);
            int depth = 0;
            for (std::size_t j = open + 1; j < close && close != std::string::npos; ++j) {
                if (line[j] == '(' || line[j] == '[' || line[j] == '{' || line[j] == '<') ++depth;
                if (line[j] == ')' || line[j] == ']' || line[j] == '}' || line[j] == '>') --depth;
                if (line[j] == ';') break;
                if (depth != 0 || line[j] != ':' || line[j + 1] == ':' || line[j - 1] == ':' || line[j - 1] != ' ') continue;
                std::size_t start = j;
                while (start > open + 1 && line[start - 1] == ' ') --start;
                issues.push_back(xstyle::check::make_issue(file, i, start, "CPP-COLON-SPACE", "Space before the ':' of the range-for", "",
                    xstyle::check::replace_line(i, lines[i].substr(0, start) + lines[i].substr(j))));
                break;
            }
        }

        // class A : public B -> class A: public B
        if (cpp && line.find(':') != std::string::npos && std::regex_search(line, match, inheritance)) {
            const std::size_t start = static_cast<std::size_t>(match.position(1));
            issues.push_back(xstyle::check::make_issue(file, i, start, "CPP-COLON-SPACE", "Space before the ':' of the base", "",
                xstyle::check::replace_line(i, lines[i].substr(0, start) + lines[i].substr(start + static_cast<std::size_t>(match.length(1))))));
        }

        // Type &name -> Type& name (declarations only): every & / * after a space and glued to an identifier
        struct Glued {
            std::size_t spaces; // first space before the symbol
            std::size_t symbol;
            std::size_t length; // length of the symbol (& / && / * / **)
        };
        std::vector<Glued> found;
        for (std::size_t j = line.find_first_of("&*"); j != std::string::npos; j = line.find_first_of("&*", j + 1)) {
            std::size_t length = 1;
            while (j + length < line.size() && line[j + length] == line[j] && length < 2) ++length;
            const std::size_t next = j + length;
            if (j == 0 || !xstyle::is_space(line[j - 1]) || next >= line.size() || !(std::isalpha(static_cast<unsigned char>(line[next])) || line[next] == '_')) {
                j = next - 1;
                continue;
            }
            std::size_t typeEnd = j;
            while (typeEnd > 0 && xstyle::is_space(line[typeEnd - 1])) --typeEnd;
            if (typeEnd == 0) continue;
            std::size_t typeStart = typeEnd;
            std::string last;
            if (line[typeEnd - 1] == '>') {
                // Template type: back to the matching '<' then its name
                int depth = 0;
                bool opened = false;
                while (typeStart > 0 && !opened) {
                    --typeStart;
                    if (line[typeStart] == '>') ++depth;
                    if (line[typeStart] == '<' && --depth == 0) opened = true;
                }
                if (!opened) continue;
                while (typeStart > 0 && (xstyle::is_word(line[typeStart - 1]) || line[typeStart - 1] == ':')) --typeStart;
                last = ">";
            } else {
                while (typeStart > 0 && (xstyle::is_word(line[typeStart - 1]) || line[typeStart - 1] == ':')) --typeStart;
                const std::string type = line.substr(typeStart, typeEnd - typeStart);
                last = type.substr(type.rfind(':') == std::string::npos ? 0 : type.rfind(':') + 1);
                if (last.empty() || keywords.contains(last)) continue;
                const bool typeLike = types.contains(last) || type.find("::") != std::string::npos || std::isupper(static_cast<unsigned char>(last[0])) || last.ends_with("_t");
                if (!typeLike) continue;
            }
            // What is before the type: start of a declaration only
            std::size_t k = typeStart;
            while (k > 0 && xstyle::is_space(line[k - 1])) --k;
            const char previous = k == 0 ? '\0' : line[k - 1];
            const std::string word = previous_word_(line, k);
            const bool declarationStart = previous == '\0' || previous == '(' || previous == ',' || previous == '{' || previous == ';' || previous == '}'
                || previous == '<' || (xstyle::is_word(previous) && (qualifiers.contains(word) || word.starts_with("_")));
            if (!declarationStart) continue;
            found.push_back({typeEnd, j, length});
            j = next - 1;
        }
        std::string fixed = lines[i];
        for (std::size_t f = found.size(); f > 0; --f)
            fixed.replace(found[f - 1].spaces, found[f - 1].symbol + found[f - 1].length - found[f - 1].spaces, line.substr(found[f - 1].symbol, found[f - 1].length) + " ");
        if (!found.empty())
            issues.push_back(xstyle::check::make_issue(file, i, found[0].symbol, "CPP-PTR-REF", "& / * glued to the name instead of the type", "", xstyle::check::replace_line(i, fixed)));
    }
}

_hot static void functions_(const xstyle::SourceFile& file, const xstyle::ProjectInfo& project, xstyle::check::Issues& issues)
{
    static const std::regex emptyParams(R"(\(\s*\)\s*(?:const|noexcept|override|final|volatile|&|\s)*(?:=\s*(?:0|default|delete)\s*)?(?:;|\{|:|->|$))");
    static const std::regex prefixNoise(R"(\[\[[^\]]*\]\]|\b_\w+(?:\([^()]*\))?|\b(?:inline|static|virtual|explicit|constexpr|consteval|constinit|friend|extern)\b|template\s*<[^>]*>)");
    static const std::regex member(R"((^|[^\w.>:])(_[a-z][A-Za-z0-9_]*)\b)");
    static const std::regex emptyParens(R"(\(\s*\))");
    static const std::regex privateCall(R"((^|[^\w.>:~])([a-z]\w*_)\s*\()");
    static const std::regex functionName(R"((\w+)\s*\()");
    const bool cpp = file.getLanguage() == xstyle::Language::Cpp;
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    const std::vector<xstyle::LineInfo>& info = file.getInfo();
    const std::vector<xstyle::Brace>& braces = file.getBraces();

    // Free functions defined in the file (static helpers name_ are called without this->), and local lambdas named name_
    static const std::regex lambdaName(R"(\b([a-z]\w*_)\s*=\s*\[)");
    std::set<std::string> freeFunctions;
    for (const std::string& line: code)
        for (std::sregex_iterator it(line.begin(), line.end(), lambdaName); it != std::sregex_iterator(); ++it)
            freeFunctions.insert((*it)[1].str());
    for (const xstyle::Brace& brace: braces) {
        if (brace.kind != xstyle::Scope::Function || (brace.parent != NO_INDEX && braces[brace.parent].kind != xstyle::Scope::Namespace)) continue;
        for (std::sregex_iterator it(brace.header.begin(), brace.header.end(), functionName); it != std::sregex_iterator(); ++it) {
            if (project.attributeMacros.contains((*it)[1].str())) continue;
            freeFunctions.insert((*it)[1].str());
            break;
        }
    }

    for (std::size_t i = 0; i < code.size(); ++i) {
        if (info[i].preprocessor) continue;
        const std::string& line = code[i];
        std::smatch match;

        // f() -> f(void) for declarations / definitions (constructors, destructors, operators and calls excepted)
        if (!info[i].inFunction && info[i].parenDepth == 0 && std::regex_search(line, emptyParens) && std::regex_search(line, match, emptyParams)) {
            // Name: the qualified identifier right before the parentheses
            const std::size_t open = static_cast<std::size_t>(match.position(0));
            std::size_t start = open;
            while (start > 0 && xstyle::is_space(line[start - 1])) --start;
            const std::size_t nameEnd = start;
            while (start > 0 && (xstyle::is_word(line[start - 1]) || line[start - 1] == ':' || line[start - 1] == '~')) --start;
            const std::string prefix = xstyle::trim(std::regex_replace(line.substr(0, start), prefixNoise, " "));
            const std::string name = line.substr(start, nameEnd - start);
            const std::size_t scope = name.rfind("::");
            const std::string last = scope == std::string::npos ? name : name.substr(scope + 2);
            const std::string owner = scope == std::string::npos ? "" : name.substr(0, scope).substr(name.substr(0, scope).rfind(':') == std::string::npos ? 0 : name.substr(0, scope).rfind(':') + 1);
            const bool typeLike = !prefix.empty() && (xstyle::is_word(prefix.back()) || prefix.back() == '>' || prefix.back() == '*' || prefix.back() == '&')
                && !prefix.ends_with("->") && !prefix.ends_with(".") && prefix.find('{') == std::string::npos
                && prefix.find('=') == std::string::npos && prefix.find('(') == std::string::npos && !prefix.ends_with("return") && !prefix.ends_with("new")
                && !prefix.ends_with("else") && !prefix.ends_with("throw") && !prefix.ends_with("delete") && !prefix.ends_with("case");
            if (!name.empty() && typeLike && !last.starts_with("~") && last != owner && name.find("operator") == std::string::npos) {
                issues.push_back(xstyle::check::make_issue(file, i, open, "CPP-VOID-PARAM", "Empty parameter list of " + name, "",
                    xstyle::check::replace_line(i, lines[i].substr(0, open + 1) + "void" + lines[i].substr(line.find(')', open)))));
            }
        }

        // this-> on every member access: the bodies started on a previous line, then the bodies opened on this line
        if (!cpp) continue;
        std::vector<std::pair<std::size_t, std::size_t>> segments; // <start, end> of the code in a member function body
        std::vector<bool> members; // the body is a member function (calls of name_() need this-> too)
        bool isStatic = false;
        std::size_t function = NO_INDEX;
        if (info[i].inFunction && in_function_(file, i, isStatic, function) && !isStatic) {
            segments.push_back({0, line.size()});
            members.push_back(member_function_(file, function));
        }
        for (std::size_t b = 0; b < braces.size() && !info[i].inFunction; ++b) {
            if (braces[b].openLine != i || braces[b].kind != xstyle::Scope::Function || braces[b].isStatic) continue;
            segments.push_back({braces[b].openColumn + 1, braces[b].closeLine == i ? braces[b].closeColumn : line.size()});
            members.push_back(member_function_(file, b));
        }
        std::set<std::size_t> positions;
        for (std::size_t s = 0; s < segments.size(); ++s) {
            const std::string segment = std::string(segments[s].first, ' ') + line.substr(segments[s].first, segments[s].second - segments[s].first);
            if (segment.find('_') == std::string::npos) continue;
            for (std::sregex_iterator it(segment.begin(), segment.end(), member); it != std::sregex_iterator(); ++it) {
                const std::size_t pos = static_cast<std::size_t>(it->position(2));
                std::size_t next = pos + static_cast<std::size_t>(it->length(2));
                while (next < line.size() && xstyle::is_space(line[next])) ++next;
                if ((next < line.size() && line[next] == '(') || project.attributeMacros.contains((*it)[2].str())) continue; // attribute macro
                positions.insert(pos);
            }
            if (!members[s]) continue;
            for (std::sregex_iterator it(segment.begin(), segment.end(), privateCall); it != std::sregex_iterator(); ++it)
                if (!freeFunctions.contains((*it)[2].str()) && !project.attributeMacros.contains((*it)[2].str())) positions.insert(static_cast<std::size_t>(it->position(2)));
        }
        std::string fixed = lines[i];
        for (std::set<std::size_t>::const_reverse_iterator it = positions.rbegin(); it != positions.rend(); ++it)
            fixed.insert(*it, "this->");
        if (!positions.empty())
            issues.push_back(xstyle::check::make_issue(file, i, *positions.begin(), "CPP-THIS", "Member accessed without this->", "", xstyle::check::replace_line(i, fixed)));
    }
}

_hot static void casts_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex cast(R"(\(\s*((?:const\s+)?(?:unsigned\s+|signed\s+)?(?:long\s+long|long\s+double|char|short|int|long|float|double|bool|(?:std::)?(?:size_t|ptrdiff_t|u?int(?:8|16|32|64)_t|uintptr_t|intptr_t)|ssize_t|off_t|[A-Z]\w*(?:::\w+)*\s*\*+|(?:unsigned\s+)?(?:char|void|int)\s*\*+))\s*\)\s*(?=[\w(\-*&!~"']))");
    static const std::regex pointerSpace(R"(\s+\*)");
    static const std::unordered_set<std::string> operators = {"sizeof", "alignof", "decltype", "typeid", "alignas", "_alignas", "noexcept", "static_assert", "requires"};
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    const std::vector<xstyle::LineInfo>& info = file.getInfo();

    for (std::size_t i = 0; i < code.size(); ++i) {
        if (info[i].preprocessor) continue;
        const std::string& line = code[i];
        if (line.find(')') == std::string::npos) continue;
        for (std::sregex_iterator it(line.begin(), line.end(), cast); it != std::sregex_iterator(); ++it) {
            const std::size_t open = static_cast<std::size_t>(it->position(0));
            if (open > 0 && (xstyle::is_word(line[open - 1]) || line[open - 1] == '>' || line[open - 1] == ']' || line[open - 1] == ')')) continue;
            if (operators.contains(previous_word_(line, open))) continue;
            std::string type = std::regex_replace((*it)[1].str(), pointerSpace, "*");
            const std::size_t close = open + static_cast<std::size_t>(it->length(0));

            // Operand: identifier chain with calls / subscripts, or a parenthesized expression
            std::size_t start = close;
            while (start < line.size() && xstyle::is_space(line[start])) ++start;
            std::size_t end = start;
            bool parenthesized = false;
            if (end < line.size() && line[end] == '(') {
                end = matching_(line, end);
                parenthesized = true;
                if (end != std::string::npos) ++end;
            } else {
                while (end != std::string::npos && end < line.size()) {
                    if (xstyle::is_word(line[end]) || (line[end] == ':' && end + 1 < line.size() && line[end + 1] == ':')) {
                        end += line[end] == ':' ? 2 : 1;
                    } else if (line[end] == '.' && end + 1 < line.size() && xstyle::is_word(line[end + 1])) {
                        ++end;
                    } else if (line.compare(end, 2, "->") == 0 && end + 2 < line.size() && xstyle::is_word(line[end + 2])) {
                        end += 2;
                    } else if (line[end] == '(' || line[end] == '[') {
                        end = matching_(line, end);
                        if (end != std::string::npos) ++end;
                    } else {
                        break;
                    }
                }
            }
            const bool pointerCast = type.find('*') != std::string::npos;
            if (end == std::string::npos || end == start) {
                issues.push_back(xstyle::check::make_issue(file, i, open, "CPP-C-CAST", "C cast (" + type + ")",
                    pointerCast ? "static_cast<" + type + "> (from void*) or reinterpret_cast<" + type + ">" : "static_cast<" + type + ">(...)"));
                continue;
            }
            std::string operand = lines[i].substr(start, end - start);
            if (parenthesized) operand = operand.substr(1, operand.size() - 2);
            // A pointer cast may need reinterpret_cast: static_cast is a guess (a wrong one doesn't compile, it never runs wrong)
            xstyle::Fix fix = xstyle::check::replace_line(i, lines[i].substr(0, open) + "static_cast<" + type + ">(" + operand + ")" + lines[i].substr(end));
            fix.dangerous = pointerCast;
            issues.push_back(xstyle::check::make_issue(file, i, open, "CPP-C-CAST", "C cast (" + type + ")" + (pointerCast ? ", static_cast guessed (reinterpret_cast if it doesn't compile)" : ""), "", fix));
        }
    }
}

/* checks: file structure */
_hot static void header_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex fileTag(R"(@file\s+(\S+))");
    const std::vector<std::string>& lines = file.getLines();
    const std::string name = file.getPath().filename().string();

    std::size_t first = 0;
    while (first < lines.size() && xstyle::is_blank(lines[first])) ++first;
    if (first >= lines.size()) return;
    bool hasFile = false;
    for (std::size_t i = first; i < std::min(lines.size(), first + 40); ++i) {
        std::smatch match;
        if (!std::regex_search(lines[i], match, fileTag)) continue;
        hasFile = true;
        if (match[1].str() != name)
            issues.push_back(xstyle::check::make_issue(file, i, static_cast<std::size_t>(match.position(1)), "CPP-HEADER-FILE", "@file " + match[1].str() + " in " + name, "",
                xstyle::check::replace_line(i, lines[i].substr(0, static_cast<std::size_t>(match.position(1))) + name + lines[i].substr(static_cast<std::size_t>(match.position(1) + match.length(1))))));
        break;
    }
    if (xstyle::trim(lines[first]).starts_with("/*") && hasFile) return;

    // Fixed by the Core: --fix asks the banner / description (or --header)
    xstyle::Issue issue = xstyle::check::make_issue(file, first, 0, "CPP-HEADER", "No file header",
        "--fix asks the banner (none / default / text) and the description, or --header none|default|<text> [--header-desc \"...\"]");
    issue.mode = xstyle::FixMode::Ask;
    issues.push_back(issue);
}

_hot static void guard_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex ifndef(R"(^\s*#\s*ifndef\s+(\w+))");
    static const std::regex define(R"(^\s*#\s*define\s+(\w+))");
    static const std::regex endif(R"(^\s*#\s*endif\b)");
    static const std::regex pragma(R"(^\s*#\s*pragma\s+once\b)");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<xstyle::LineInfo>& info = file.getInfo();
    const std::string expected = guard_name_(file);

    std::vector<std::size_t> directives;
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (info[i].preprocessor && xstyle::trim(file.getCode()[i]).starts_with("#")) directives.push_back(i);

    // Content wrapped by a new guard: after the file header
    std::size_t start = 0;
    while (start < lines.size() && xstyle::is_blank(file.getCode()[start]) && !info[start].preprocessor) ++start;
    std::smatch match;
    if (!directives.empty() && std::regex_search(lines[directives[0]], pragma)) {
        std::vector<std::string> fixed = {"#ifndef " + expected, "    #define " + expected};
        fixed.insert(fixed.end(), lines.begin() + static_cast<std::ptrdiff_t>(directives[0] + 1), lines.end());
        while (!fixed.empty() && xstyle::is_blank(fixed.back())) fixed.pop_back();
        fixed.push_back("#endif /* " + expected + " */");
        issues.push_back(xstyle::check::make_issue(file, directives[0], 0, "CPP-PRAGMA-ONCE", "#pragma once", "#ifndef " + expected + " / #define " + expected + " ... #endif /* " + expected + " */",
            xstyle::check::replace_lines(directives[0], lines.size() - directives[0], fixed)));
        return;
    }
    const bool guarded = directives.size() >= 2 && std::regex_search(lines[directives[0]], match, ifndef);
    std::smatch defined;
    if (!guarded || !std::regex_search(lines[directives[1]], defined, define) || defined[1].str() != match[1].str()) {
        std::vector<std::string> fixed = {"#ifndef " + expected, "    #define " + expected, ""};
        fixed.insert(fixed.end(), lines.begin() + static_cast<std::ptrdiff_t>(start), lines.end());
        while (!fixed.empty() && xstyle::is_blank(fixed.back())) fixed.pop_back();
        fixed.push_back("");
        fixed.push_back("#endif /* " + expected + " */");
        issues.push_back(xstyle::check::make_issue(file, start < lines.size() ? start : NO_INDEX, 0, "CPP-GUARD-MISSING", "Header without include guard", "",
            xstyle::check::replace_lines(start, lines.size() - start, fixed)));
        return;
    }

    // Name of the guard & comment of the #endif
    const std::string current = match[1].str();
    const std::size_t last = directives.back();
    const bool closes = std::regex_search(lines[last], endif);
    std::string separated; // fixed_string.hpp -> FIXED_STRING_H is accepted too
    for (const char c: file.getPath().stem().string())
        separated += std::isalnum(static_cast<unsigned char>(c)) ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : '_';
    separated += "_H";
    if (current != expected && current != separated && !current.ends_with("_" + expected) && !current.ends_with("_" + separated)) {
        const std::regex word("\\b" + current + "\\b");
        std::vector<std::string> fixed(lines.begin() + static_cast<std::ptrdiff_t>(directives[0]), lines.end());
        for (const std::size_t line: {directives[0], directives[1], last}) // only the guard lines, never the code
            if (line != last || closes) fixed[line - directives[0]] = std::regex_replace(fixed[line - directives[0]], word, expected);
        issues.push_back(xstyle::check::make_issue(file, directives[0], static_cast<std::size_t>(match.position(1)), "CPP-GUARD-NAME", "Guard " + current + " instead of " + expected, "rename " + current + " -> " + expected,
            xstyle::check::replace_lines(directives[0], lines.size() - directives[0], fixed)));
    }
    if (closes && file.getComments()[last].find(current) == std::string::npos && file.getComments()[last].find(expected) == std::string::npos) {
        issues.push_back(xstyle::check::make_issue(file, last, 0, "CPP-ENDIF-COMMENT", "Guard #endif without its comment", "",
            xstyle::check::replace_line(last, "#endif /* " + current + " */")));
    }
}

_hot static void preprocessor_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex directive(R"(^(\s*)#\s*(\w+))");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<xstyle::LineInfo>& info = file.getInfo();
    std::size_t depth = 0;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::smatch match;
        if (!info[i].preprocessor || !std::regex_search(file.getCode()[i], match, directive)) continue;
        const std::string word = match[2].str();
        const bool closing = word == "endif" || word == "else" || word == "elif" || word == "elifdef" || word == "elifndef";
        if (word == "endif" && depth > 0) --depth;
        const std::size_t expected = (closing && word != "endif" && depth > 0 ? depth - 1 : depth) * 4;
        if (word == "if" || word == "ifdef" || word == "ifndef") ++depth;

        // Only outside of any brace (the code indentation wins inside a function)
        if (info[i].brace != NO_INDEX || match[1].length() == static_cast<std::ptrdiff_t>(expected)) continue;
        issues.push_back(xstyle::check::make_issue(file, i, 0, "CPP-PREPRO-INDENT", "#" + word + " indented by " + std::to_string(match[1].length()) + " instead of " + std::to_string(expected), "",
            xstyle::check::replace_line(i, std::string(expected, ' ') + xstyle::trim(lines[i]))));
    }
}

_hot static void includes_(const xstyle::SourceFile& file, const xstyle::ProjectInfo& project, xstyle::check::Issues& issues)
{
    static const std::regex include(R"(^\s*#\s*include\s*([<"])([^>"]+)[>"])");
    static const std::regex define(R"(^\s*#\s*define\s+(\w+))");
    const std::vector<std::string>& lines = file.getLines();
    const std::string guard = guard_name_(file);

    std::size_t i = 0;
    while (i < lines.size()) {
        // Block: consecutive #include / #define lines
        std::size_t end = i;
        std::smatch match;
        while (end < lines.size() && (std::regex_search(lines[end], include) || (std::regex_search(lines[end], match, define) && match[1].str() != guard))) ++end;
        if (end == i) {
            ++i;
            continue;
        }

        // Units: the defines glued to the include below them
        struct Unit {
            int group;
            std::size_t length;
            std::vector<std::string> lines;
        };
        std::vector<Unit> units;
        std::vector<std::string> pending;
        std::size_t includes = 0;
        for (std::size_t k = i; k < end; ++k) {
            pending.push_back(lines[k]);
            if (!std::regex_search(lines[k], match, include)) continue;
            const std::string target = match[2].str();
            const bool personal = !project.isLibutils && (target.starts_with("utils/") || target == "utils.hpp");
            units.push_back(Unit{personal ? 0 : match[1].str() == "\"" ? 1 : 2, target.size(), pending});
            pending.clear();
            ++includes;
        }
        if (pending.empty() && includes >= 2) {
            std::vector<Unit> sorted = units;
            std::stable_sort(sorted.begin(), sorted.end(), [](const Unit& a, const Unit& b) {return a.group != b.group ? a.group < b.group : a.length > b.length;});
            std::vector<std::string> fixed;
            for (const Unit& unit: sorted)
                fixed.insert(fixed.end(), unit.lines.begin(), unit.lines.end());
            if (fixed != std::vector<std::string>(lines.begin() + static_cast<std::ptrdiff_t>(i), lines.begin() + static_cast<std::ptrdiff_t>(end)))
                issues.push_back(xstyle::check::make_issue(file, i, 0, "CPP-INCLUDE-ORDER", "Includes not in order (personal libs, project, external; longest first)", "",
                    xstyle::check::replace_lines(i, end - i, fixed)));
        }
        i = end;
    }
}

_hot static void namespaces_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    const std::vector<std::string>& comments = file.getComments();
    const std::vector<xstyle::Brace>& braces = file.getBraces();

    // The migration blocks (end of the header) have their own layout
    std::size_t migration = lines.size();
    for (std::size_t i = 0; i < lines.size() && migration == lines.size(); ++i)
        if (comments[i].find("MIGRATION") != std::string::npos) migration = i;

    for (std::size_t b = 0; b < braces.size(); ++b) {
        const xstyle::Brace& brace = braces[b];
        if (brace.kind != xstyle::Scope::Namespace || brace.name.empty()) continue;

        // namespace a { namespace b {
        if (brace.parent != NO_INDEX && braces[brace.parent].kind == xstyle::Scope::Namespace && brace.headerLine <= braces[brace.parent].openLine + 1) {
            issues.push_back(xstyle::check::make_issue(file, brace.openLine, 0, "CPP-NAMESPACE-NESTED", "Nested namespace blocks", "namespace " + braces[brace.parent].name + "::" + brace.name + " {"));
            continue;
        }
        if (brace.parent != NO_INDEX || brace.openLine >= migration) continue;

        // // namespace start & // namespace end (a bare "// namespace" is completed, another comment is replaced only with --force)
        for (const bool opening: {true, false}) {
            const std::size_t line = opening ? brace.openLine : brace.closeLine;
            const std::string expected = opening ? "namespace start" : "namespace end";
            if (line == NO_INDEX || comments[line].find(expected) != std::string::npos) continue;
            const std::string message = std::string("Namespace without // ") + expected;
            const std::size_t column = opening ? brace.openColumn : brace.closeColumn;
            const std::string rest = xstyle::trim(code[line].substr(column + 1));
            if (!rest.empty() && rest != ";") {
                issues.push_back(xstyle::check::make_issue(file, line, column, "CPP-NAMESPACE-COMMENT", message, opening ? "namespace " + brace.name + " { // namespace start" : "} // namespace end"));
                continue;
            }
            const std::size_t comment = comments[line].find("//");
            const std::string current = comment == std::string::npos ? "" : xstyle::trim(comments[line].substr(comment + 2));
            const std::string base = comment == std::string::npos ? rstrip_(lines[line]) : rstrip_(lines[line].substr(0, comment));
            xstyle::Fix fix = xstyle::check::replace_line(line, base + " // " + expected);
            fix.unsafe = !xstyle::is_blank(comments[line]) && current != "namespace"; // the comment written there is lost
            issues.push_back(xstyle::check::make_issue(file, line, column, "CPP-NAMESPACE-COMMENT", message, "", fix));
        }

        // Content not indented (relative to the namespace line, + 4 per #if level opened inside)
        const std::size_t close = brace.closeLine == NO_INDEX ? lines.size() : brace.closeLine;
        const std::size_t base = xstyle::indentation(lines[brace.openLine]).size();
        std::size_t conditional = 0;
        std::size_t previous = brace.openLine;
        for (std::size_t i = brace.openLine + 1; i < close; ++i) {
            const xstyle::LineInfo& info = file.getInfo()[i];
            const std::string directive = xstyle::trim(code[i]);
            if (info.preprocessor && (directive.starts_with("#if"))) ++conditional;
            if (info.preprocessor && directive.starts_with("#endif") && conditional > 0) --conditional;
            if (info.preprocessor || xstyle::is_blank(code[i])) continue;
            const std::string last = xstyle::trim(code[previous]);
            previous = i;
            const bool continued = !last.empty() && std::string("=,<([{+-*/&|?:\\").find(last.back()) != std::string::npos && info.brace == b;
            const std::size_t indent = xstyle::indentation(lines[i]).size();
            if (info.brace != b || info.parenDepth > 0 || continued || indent == base || indent == base + conditional * 4) continue;
            issues.push_back(xstyle::check::make_issue(file, i, 0, "CPP-NAMESPACE-INDENT", "Namespace content indented", "the content of a namespace starts at column 0"));
            break;
        }
    }
}

_hot static void classes_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex access(R"(^\s*(public|private|protected)\s*:)");
    static const std::regex ctor(R"((\w+)::~?(\w+)\s*\()");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& code = file.getCode();
    const std::vector<xstyle::Brace>& braces = file.getBraces();
    const bool source = !file.isHeader();

    for (std::size_t b = 0; b < braces.size(); ++b) {
        const xstyle::Brace& brace = braces[b];

        // public before private / protected
        if (brace.kind == xstyle::Scope::Class && brace.closeLine != NO_INDEX) {
            std::size_t publicLine = NO_INDEX;
            for (std::size_t i = brace.openLine + 1; i < brace.closeLine; ++i) {
                std::smatch match;
                if (file.getInfo()[i].brace != b || !std::regex_search(code[i], match, access)) continue;
                if (match[1].str() == "public" && publicLine == NO_INDEX) {
                    publicLine = i;
                } else if (match[1].str() != "public" && publicLine != NO_INDEX) {
                    issues.push_back(xstyle::check::make_issue(file, publicLine, 0, "CPP-ACCESS-ORDER", "public before " + match[1].str() + " in " + brace.name,
                        "private / protected first, then public"));
                    break;
                }
            }
        }

        // One-liner: {statement};
        if (brace.kind == xstyle::Scope::Function && brace.closeLine == brace.openLine) {
            const xstyle::Scope parent = brace.parent == NO_INDEX ? xstyle::Scope::Global : braces[brace.parent].kind;
            if (parent != xstyle::Scope::Class && parent != xstyle::Scope::Struct && !file.isHeader()) continue;
            const std::string& line = lines[brace.openLine];
            if (xstyle::is_blank(code[brace.openLine].substr(0, brace.openColumn))) continue; // {body} under the signature: free layout
            const std::string body = line.substr(brace.openColumn + 1, brace.closeColumn - brace.openColumn - 1);
            if (xstyle::is_blank(body) || code[brace.openLine].substr(brace.openColumn + 1, brace.closeColumn - brace.openColumn - 1).find('{') != std::string::npos) continue;
            const bool semicolon = brace.closeColumn + 1 < line.size() && line[brace.closeColumn + 1] == ';';
            if (body == xstyle::trim(body) && semicolon) continue;
            issues.push_back(xstyle::check::make_issue(file, brace.openLine, brace.openColumn, "CPP-ONE-LINER", "One-liner not written {statement};", "",
                xstyle::check::replace_line(brace.openLine, line.substr(0, brace.openColumn + 1) + xstyle::trim(body) + "}" + (semicolon ? "" : ";") + line.substr(brace.closeColumn + 1))));
        }

        // Single statement function in a .cpp -> inline in the header
        if (!source || brace.kind != xstyle::Scope::Function || brace.closeLine == NO_INDEX || brace.closeLine <= brace.openLine) continue;
        if (brace.parent != NO_INDEX && braces[brace.parent].kind != xstyle::Scope::Namespace) continue;
        if (brace.isStatic || brace.header.starts_with("template") || brace.header.find("main(") != std::string::npos) continue;
        std::smatch match;
        if (std::regex_search(brace.header, match, ctor) && match[1].str() == match[2].str()) continue;
        if (brace.header.find("::") == std::string::npos) continue; // free function local to the .cpp
        std::vector<std::size_t> body;
        for (std::size_t i = brace.openLine + 1; i < brace.closeLine; ++i)
            if (!xstyle::is_blank(code[i])) body.push_back(i);
        if (body.size() != 1) continue;
        const std::string statement = xstyle::trim(code[body[0]]);
        if (std::count(statement.begin(), statement.end(), ';') != 1 || !statement.ends_with(";") || statement.find_first_of("{}") != std::string::npos) continue;
        issues.push_back(xstyle::check::make_issue(file, brace.headerLine == NO_INDEX ? brace.openLine : brace.headerLine, 0, "CPP-SINGLE-STATEMENT",
            "Single statement body defined in the .cpp", "define it as a one-liner in the Function block of the header: _attr inline T f(...) {" + xstyle::trim(lines[body[0]]) + "};"));
    }
}

_hot static void doxygen_(const xstyle::SourceFile& file, xstyle::check::Issues& issues)
{
    static const std::regex tags(R"([@\\](brief|param|return|returns|tparam|retval|throws|details)\b)");
    const std::vector<std::string>& lines = file.getLines();
    const std::vector<std::string>& comments = file.getComments();

    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::smatch match;
        if (xstyle::is_blank(comments[i])) continue;
        const std::string comment = xstyle::trim(comments[i]);
        const bool doxygenStart = comment.starts_with("///") || comment.starts_with("//!") || comment.starts_with("/*!")
            || (comment.starts_with("/**") && !comment.starts_with("/***") && !comment.starts_with("/**/"));
        if (doxygenStart || std::regex_search(comments[i], match, tags))
            issues.push_back(xstyle::check::make_issue(file, i, comments[i].find_first_not_of(' '), "CPP-DOXYGEN", "Doxygen comment", "short plain comment (// What it does), trailing comment on the declaration"));
    }
}

/* checks */
_hot void xstyle::check::cpp(const xstyle::SourceFile& file, const xstyle::ProjectInfo& project, xstyle::check::Issues& issues)
{
    usage_(file, issues);
    names_(file, issues);
    braces_(file, issues);
    spacing_(file, issues);
    functions_(file, project, issues);
    if (file.getLanguage() == xstyle::Language::Cpp) casts_(file, issues);
    header_(file, issues);
    if (file.isHeader()) guard_(file, issues);
    preprocessor_(file, issues);
    includes_(file, project, issues);
    namespaces_(file, issues);
    classes_(file, issues);
    doxygen_(file, issues);
    xstyle::check::comments(file, issues);
}
