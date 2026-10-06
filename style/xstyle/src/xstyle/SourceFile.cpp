/**************************************************************\
Edition:
##  @date 06/10/2026 by @author Tsukini

File Name:
##  @file SourceFile.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "xstyle/SourceFile.hpp"
#include "xstyle/Tools.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cctype>
#include <regex>

/* fix */
_hot void xstyle::SourceFile::apply(std::vector<xstyle::Fix> fixes)
{
    // Bottom-up: the line numbers of the fixes above stay valid
    std::sort(fixes.begin(), fixes.end(), [](const xstyle::Fix& a, const xstyle::Fix& b) {return a.line > b.line;});
    for (const xstyle::Fix& fix: fixes) {
        switch (fix.kind) {
            case xstyle::FixKind::FinalNewline: this->_finalNewline = true; break;
            case xstyle::FixKind::Crlf:         this->_crlf = false;        break;
            default: {
                std::size_t first = std::min(fix.line, this->_lines.size());
                std::size_t last = std::min(first + fix.count, this->_lines.size());
                this->_lines.erase(this->_lines.begin() + static_cast<std::ptrdiff_t>(first), this->_lines.begin() + static_cast<std::ptrdiff_t>(last));
                this->_lines.insert(this->_lines.begin() + static_cast<std::ptrdiff_t>(first), fix.lines.begin(), fix.lines.end());
                break;
            }
        }
    }
    this->analyze_();
}

_cold void xstyle::SourceFile::save(void) const
{
    std::ofstream file(this->_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, "Can't write " + this->_path.string());

    const std::string eol = this->_crlf ? "\r\n" : "\n";
    for (std::size_t i = 0; i < this->_lines.size(); ++i) {
        file << this->_lines[i];
        if (i + 1 < this->_lines.size() || this->_finalNewline) file << eol;
    }
    if (!file.good()) throw utils::exception::ErrorException(utils::exception::InternalCode::Write, "Can't write " + this->_path.string());
}

/* tools */
_hot xstyle::Scope xstyle::SourceFile::scopeAt(const std::size_t line) const
{
    if (line >= this->_info.size() || this->_info[line].brace == NO_INDEX) return xstyle::Scope::Global;
    return this->_braces[this->_info[line].brace].kind;
}

_hot bool xstyle::SourceFile::isHeader(void) const
{
    static const std::vector<std::string> extensions = {".hpp", ".h", ".hh", ".hxx", ".ipp", ".tpp", ".inl"};
    return std::find(extensions.begin(), extensions.end(), this->_path.extension().string()) != extensions.end();
}

/* analysis */
_hot void xstyle::SourceFile::analyze_(void)
{
    this->_code = this->_lines;
    this->_comments.assign(this->_lines.size(), "");
    for (std::size_t i = 0; i < this->_lines.size(); ++i)
        this->_comments[i] = std::string(this->_lines[i].size(), ' ');
    this->_info.assign(this->_lines.size(), xstyle::LineInfo{});
    this->_braces.clear();

    switch (this->_language) {
        case xstyle::Language::Cpp:
        case xstyle::Language::C:
        case xstyle::Language::Rust:
        case xstyle::Language::JavaScript: this->maskCLike_(); break;
        case xstyle::Language::Python:
        case xstyle::Language::Shell:
        case xstyle::Language::CMake:
        case xstyle::Language::Makefile:
        case xstyle::Language::Yaml:       this->maskHash_();  break;
        case xstyle::Language::Lua:        this->maskLua_();   break;
        default:                                               break;
    }
    if (this->isCLike()) this->scopes_();
}

_hot void xstyle::SourceFile::maskCLike_(void)
{
    enum class State {Normal, Block, String, Raw, Template};
    State state = State::Normal;
    char quote = '"';
    std::string rawEnd; // )delim" | "## (Rust)
    const bool rust = this->_language == xstyle::Language::Rust;
    std::size_t depth = 0; // nested block comments (Rust)

    for (std::size_t i = 0; i < this->_lines.size(); ++i) {
        const std::string& line = this->_lines[i];
        std::string& code = this->_code[i];
        std::string& comment = this->_comments[i];
        const std::size_t n = line.size();
        this->_info[i].inComment = state == State::Block;

        std::size_t j = 0;
        while (j < n) {
            const char c = line[j];
            const char next = j + 1 < n ? line[j + 1] : '\0';
            switch (state) {
                case State::Block:
                    comment[j] = c;
                    code[j] = ' ';
                    if (rust && c == '/' && next == '*') {
                        comment[j + 1] = '*';
                        code[j + 1] = ' ';
                        ++depth;
                        ++j;
                    } else if (c == '*' && next == '/') {
                        comment[j + 1] = '/';
                        code[j + 1] = ' ';
                        if (depth == 0) state = State::Normal;
                        else --depth;
                        ++j;
                    }
                    ++j;
                    break;
                case State::String:
                case State::Template:
                    if (c == '\\') {
                        code[j] = ' ';
                        if (j + 1 < n) code[j + 1] = ' ';
                        j += 2;
                        break;
                    }
                    if (c == quote) state = State::Normal;
                    else code[j] = ' ';
                    ++j;
                    break;
                case State::Raw:
                    if (line.compare(j, rawEnd.size(), rawEnd) == 0) {
                        j += rawEnd.size();
                        state = State::Normal;
                        break;
                    }
                    code[j++] = ' ';
                    break;
                default:
                    if (c == '/' && next == '/') {
                        for (std::size_t k = j; k < n; ++k) {
                            comment[k] = line[k];
                            code[k] = ' ';
                        }
                        j = n;
                    } else if (c == '/' && next == '*') {
                        comment[j] = '/';
                        comment[j + 1] = '*';
                        code[j] = ' ';
                        code[j + 1] = ' ';
                        state = State::Block;
                        j += 2;
                    } else if (rust && c == 'r' && (next == '"' || next == '#') && (j == 0 || !xstyle::is_word(line[j - 1]) || line[j - 1] == 'b')) {
                        // r"..." / r#"..."# / br"..."
                        std::size_t k = j + 1;
                        while (k < n && line[k] == '#') ++k;
                        if (k >= n || line[k] != '"') {
                            ++j;
                            break;
                        }
                        rawEnd = "\"" + std::string(k - j - 1, '#');
                        state = State::Raw;
                        j = k + 1;
                    } else if (rust && c == '\'') {
                        // 'x' / '\n' is a char, 'a (lifetime / label) is not
                        if (next == '\\' || (j + 2 < n && line[j + 2] == '\'')) {
                            quote = c;
                            state = State::String;
                        }
                        ++j;
                    } else if (c == 'R' && next == '"' && (j == 0 || !xstyle::is_word(line[j - 1]) || line[j - 1] == '8' || line[j - 1] == 'u' || line[j - 1] == 'L' || line[j - 1] == 'U')) {
                        std::size_t open = line.find('(', j + 2);
                        if (open == std::string::npos) {
                            j += 2;
                            break;
                        }
                        rawEnd = ")" + line.substr(j + 2, open - j - 2) + "\"";
                        state = State::Raw;
                        j = open + 1;
                    } else if (c == '"' || (c == '`' && this->_language == xstyle::Language::JavaScript)) {
                        quote = c;
                        state = c == '`' ? State::Template : State::String;
                        ++j;
                    } else if (c == '\'') {
                        // Digit separator (1'000'000) is not a char literal
                        if (j > 0 && std::isxdigit(static_cast<unsigned char>(line[j - 1])) && std::isxdigit(static_cast<unsigned char>(next))
                            && std::isdigit(static_cast<unsigned char>(line[j - 1]))) {
                            ++j;
                            break;
                        }
                        quote = c;
                        state = State::String;
                        ++j;
                    } else {
                        ++j;
                    }
                    break;
            }
        }

        // A plain string / char never continues on the next line (unless escaped): recover
        if (state == State::String && !(rust && quote == '"') && (n == 0 || line[n - 1] != '\\')) state = State::Normal;
    }
}

_hot void xstyle::SourceFile::maskHash_(void)
{
    const bool python = this->_language == xstyle::Language::Python;
    const bool shell = this->_language == xstyle::Language::Shell;
    const bool multiLine = shell || this->_language == xstyle::Language::CMake;
    bool inString = false;
    bool triple = false;
    char quote = '"';

    for (std::size_t i = 0; i < this->_lines.size(); ++i) {
        const std::string& line = this->_lines[i];
        std::string& code = this->_code[i];
        std::string& comment = this->_comments[i];
        const std::size_t n = line.size();
        this->_info[i].inComment = false;

        std::size_t j = 0;
        while (j < n) {
            const char c = line[j];
            if (inString) {
                if (c == '\\' && !(shell && quote == '\'')) {
                    code[j] = ' ';
                    if (j + 1 < n) code[j + 1] = ' ';
                    j += 2;
                    continue;
                }
                if (c == quote && (!triple || line.compare(j, 3, std::string(3, quote)) == 0)) {
                    inString = false;
                    j += triple ? 3 : 1;
                    continue;
                }
                code[j++] = ' ';
                continue;
            }
            if (c == '#' && (!shell || j == 0 || std::isspace(static_cast<unsigned char>(line[j - 1])) || line[j - 1] == ';')) {
                for (std::size_t k = j; k < n; ++k) {
                    comment[k] = line[k];
                    code[k] = ' ';
                }
                break;
            }
            if (c == '"' || c == '\'') {
                quote = c;
                inString = true;
                triple = python && line.compare(j, 3, std::string(3, c)) == 0;
                j += triple ? 3 : 1;
                continue;
            }
            ++j;
        }

        // Only the shell / cmake strings and the python triple quotes continue on the next line
        if (inString && !triple && !multiLine) inString = false;
    }
}

_hot void xstyle::SourceFile::maskLua_(void)
{
    bool block = false;

    for (std::size_t i = 0; i < this->_lines.size(); ++i) {
        const std::string& line = this->_lines[i];
        std::string& code = this->_code[i];
        std::string& comment = this->_comments[i];
        const std::size_t n = line.size();
        this->_info[i].inComment = block;

        std::size_t j = 0;
        while (j < n) {
            if (block) {
                comment[j] = line[j];
                code[j] = ' ';
                if (line.compare(j, 2, "]]") == 0) {
                    comment[j + 1] = line[j + 1];
                    code[j + 1] = ' ';
                    block = false;
                    ++j;
                }
                ++j;
                continue;
            }
            if (line.compare(j, 4, "--[[") == 0) {
                block = true;
                continue;
            }
            if (line.compare(j, 2, "--") == 0) {
                for (std::size_t k = j; k < n; ++k) {
                    comment[k] = line[k];
                    code[k] = ' ';
                }
                break;
            }
            if (line[j] == '"' || line[j] == '\'') {
                const char quote = line[j++];
                while (j < n && line[j] != quote) {
                    if (line[j] == '\\' && j + 1 < n) code[j++] = ' ';
                    code[j++] = ' ';
                }
            }
            ++j;
        }
    }
}

_hot void xstyle::SourceFile::scopes_(void)
{
    static const std::regex control(R"(^(?:else\s+)?(?:if|for|while|switch|catch)\b|^(?:else|do|try)$)");
    static const std::regex lambda(R"(\[[^\[\]]*\]\s*(?:\([^{}]*\))?\s*(?:mutable|constexpr|noexcept|->[^{}]*|\s)*$)");
    static const std::regex function(R"(\)\s*(?:const|noexcept(?:\([^()]*\))?|override|final|mutable|volatile|&&|&|try|->[^{};]*|requires[^{};]*|\s)*$)");
    static const std::regex classHead(R"(\b(class|struct|union)\s+(?:\[\[[^\]]*\]\]\s*|_\w+(?:\([^)]*\))?\s+)*(\w+))");
    static const std::regex namespaceHead(R"(\bnamespace\b\s*([\w:]*))");
    static const std::regex staticWord(R"(\bstatic\b)");
    static const std::regex enumHead(R"(\benum\b)");
    std::vector<std::size_t> stack; // open braces
    std::vector<std::size_t> savedParen; // paren depth of the parent of each open brace
    std::string header;
    std::size_t headerLine = NO_INDEX;
    std::size_t paren = 0;
    xstyle::Scope lastClosed = xstyle::Scope::Global;
    bool continuation = false; // preprocessor line continued by a '\'

    for (std::size_t i = 0; i < this->_code.size(); ++i) {
        const std::string& code = this->_code[i];
        xstyle::LineInfo& info = this->_info[i];
        info.brace = stack.empty() ? NO_INDEX : stack.back();
        info.parenDepth = paren;
        for (const std::size_t b: stack)
            if (this->_braces[b].kind == xstyle::Scope::Function || this->_braces[b].kind == xstyle::Scope::Lambda) info.inFunction = true;

        // Preprocessor directives are not part of the code structure
        std::string trimmed = xstyle::trim(code);
        if (continuation || (!trimmed.empty() && trimmed[0] == '#')) {
            info.preprocessor = true;
            continuation = !this->_lines[i].empty() && this->_lines[i].back() == '\\';
            continue;
        }

        for (std::size_t j = 0; j < code.size(); ++j) {
            const char c = code[j];
            if (c == '(') {
                ++paren;
            } else if (c == ')') {
                if (paren > 0) --paren;
            } else if (c == ';' && paren == 0) {
                header.clear();
                headerLine = NO_INDEX;
                continue;
            } else if (c == ':' && paren == 0 && (xstyle::trim(header) == "public" || xstyle::trim(header) == "private" || xstyle::trim(header) == "protected")) {
                // Access specifier: not part of the next declaration
                header.clear();
                headerLine = NO_INDEX;
                continue;
            } else if (c == '{') {
                xstyle::Brace brace;
                brace.openLine = i;
                brace.openColumn = j;
                brace.headerLine = headerLine == NO_INDEX ? i : headerLine;
                brace.header = xstyle::trim(header);
                brace.parent = stack.empty() ? NO_INDEX : stack.back();
                brace.isStatic = std::regex_search(brace.header, staticWord);

                // What is opened, from the text before the brace
                const xstyle::Scope parent = stack.empty() ? xstyle::Scope::Global : this->_braces[stack.back()].kind;
                bool inBody = false;
                for (const std::size_t b: stack)
                    if (this->_braces[b].kind == xstyle::Scope::Function || this->_braces[b].kind == xstyle::Scope::Lambda) inBody = true;
                std::smatch match;
                if (brace.header.find("concept ") != std::string::npos || brace.header.starts_with("requires")) {
                    brace.kind = xstyle::Scope::Init; // requires-expression
                } else if (std::regex_search(brace.header, match, namespaceHead) && !inBody) {
                    brace.kind = xstyle::Scope::Namespace;
                    brace.name = match[1].str();
                } else if (brace.header.starts_with("extern \"") && !inBody) {
                    brace.kind = xstyle::Scope::Extern;
                } else if (std::regex_search(brace.header, enumHead) && brace.header.find('(') == std::string::npos) {
                    brace.kind = xstyle::Scope::Enum;
                } else if (std::regex_search(brace.header, classHead) && brace.header.find('(') == std::string::npos) {
                    // Last match: template<class T> class Name
                    for (std::sregex_iterator it(brace.header.begin(), brace.header.end(), classHead); it != std::sregex_iterator(); ++it) {
                        brace.kind = (*it)[1].str() == "class" ? xstyle::Scope::Class : xstyle::Scope::Struct;
                        brace.name = (*it)[2].str();
                    }
                } else if (std::regex_search(brace.header, lambda) && (inBody || brace.header.find('=') != std::string::npos)) {
                    brace.kind = xstyle::Scope::Lambda;
                } else if (inBody || parent == xstyle::Scope::Control || parent == xstyle::Scope::Block) {
                    if (std::regex_search(brace.header, control)) brace.kind = xstyle::Scope::Control;
                    else if (brace.header.empty() || brace.header.ends_with(")")) brace.kind = xstyle::Scope::Block;
                    else brace.kind = xstyle::Scope::Init;
                } else if (parent == xstyle::Scope::Init || parent == xstyle::Scope::Enum) {
                    brace.kind = xstyle::Scope::Init;
                } else if (brace.header.empty()) {
                    // Body after a constructor init list ending with a brace init (: _a{a}, _b{b})
                    brace.kind = lastClosed == xstyle::Scope::Init ? xstyle::Scope::Function : xstyle::Scope::Block;
                } else if (brace.header.find('(') != std::string::npos && std::regex_search(brace.header, function)) {
                    brace.kind = xstyle::Scope::Function;
                } else {
                    brace.kind = xstyle::Scope::Init;
                }

                this->_braces.push_back(brace);
                stack.push_back(this->_braces.size() - 1);
                savedParen.push_back(paren);
                paren = 0;
                header.clear();
                headerLine = NO_INDEX;
                continue;
            } else if (c == '}') {
                if (!stack.empty()) {
                    xstyle::Brace& brace = this->_braces[stack.back()];
                    brace.closeLine = i;
                    brace.closeColumn = j;
                    lastClosed = brace.kind;
                    stack.pop_back();
                    paren = savedParen.back();
                    savedParen.pop_back();
                }
                header.clear();
                headerLine = NO_INDEX;
                continue;
            }
            if (!std::isspace(static_cast<unsigned char>(c)) && headerLine == NO_INDEX) headerLine = i;
            if (headerLine != NO_INDEX) header += c;
        }
        if (headerLine != NO_INDEX) header += ' ';
    }
}

/* constructor */
xstyle::SourceFile::SourceFile(const std::filesystem::path& path, const std::string& display, const xstyle::Language language)
: _path(path), _display(display), _language(language)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) throw utils::exception::ErrorException(utils::exception::InternalCode::Read, "Can't read " + path.string());

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();
    this->_crlf = content.find("\r\n") != std::string::npos;
    this->_finalNewline = content.empty() || content.back() == '\n';

    std::string line;
    std::istringstream stream(content);
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        this->_lines.push_back(line);
    }
    this->analyze_();
}
